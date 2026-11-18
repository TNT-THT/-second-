import {
  app,
  BrowserWindow,
  screen,
  dialog,
  ipcMain,
  Menu,
  protocol,
  shell,
  session,
  Tray,
  nativeImage,
} from 'electron';
import path from 'node:path';
import fs from 'node:fs/promises';
import { readFileSync, writeFileSync, renameSync, existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { spawn } from 'node:child_process';
import {
  initialState,
  reduceState,
  restoreState,
  formatBytes,
} from '../core/state.mjs';
import { advance, confine, destination } from '../core/motion.mjs';
import { shouldIgnore, MASK_W, MASK_H } from '../core/hit-test.mjs';
import { feedOne, within } from './food.mjs';

const BASE = fileURLToPath(new URL('../', import.meta.url)),
  DIST = path.join(BASE, 'dist');
const HOME = 'petdesk://app/index.html',
  REPO = 'https://github.com/TNT-THT/-second-/releases/tag/petdesk-v1.0.0';
const CSP =
  "default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' blob: data:; font-src 'self'; connect-src 'self' blob: data:; media-src 'self' blob:; object-src 'none'; frame-src 'none'; base-uri 'none'; form-action 'none'";
const mime = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript',
  '.css': 'text/css',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.webp': 'image/webp',
  '.woff2': 'font/woff2',
  '.glb': 'model/gltf-binary',
};
if (process.platform === 'linux' && process.env.DISPLAY)
  app.commandLine.appendSwitch('ozone-platform', 'x11');
app.setName('PetDesk');
if (process.platform === 'win32') app.setAppUserModelId('studio.petdesk.local');
protocol.registerSchemesAsPrivileged([
  {
    scheme: 'petdesk',
    privileges: {
      standard: true,
      secure: true,
      supportFetchAPI: true,
      corsEnabled: true,
    },
  },
]);
let studio = null,
  pet = null,
  state = initialState(),
  saveTimer,
  feedBusy = false,
  menuOpen = false,
  hover = false;
let target = null,
  precise = null,
  drag = null,
  waitUntil = 0,
  lastTick = 0,
  walking = false,
  loop,
  tray = null,
  mask = null,
  ignoring = true;
const userFile = () => path.join(app.getPath('userData'), 'studio-v1.json');
function persist() {
  clearTimeout(saveTimer);
  try {
    const f = userFile();
    writeFileSync(f + '.tmp', JSON.stringify(state), { mode: 0o600 });
    renameSync(f + '.tmp', f);
  } catch (e) {
    console.error('PetDesk: cannot save preferences:', e.code);
  }
}
const snap = () => ({ state, petOpen: !!pet && !pet.isDestroyed() });
function emit() {
  refreshTray();
  for (const w of [studio, pet])
    if (w && !w.isDestroyed()) w.webContents.send('petdesk:state', snap());
}
function dispatch(a) {
  const next = reduceState(state, a);
  if (next !== state) {
    state = next;
    clearTimeout(saveTimer);
    saveTimer = setTimeout(persist, 300);
    emit();
  }
  return snap();
}
function privileged(e) {
  return (
    [studio, pet].some(
      (w) => w && !w.isDestroyed() && e.sender === w.webContents,
    ) &&
    e.senderFrame === e.sender.mainFrame &&
    e.senderFrame.url.split('#')[0] === HOME
  );
}
function handle(name, fn) {
  ipcMain.handle('petdesk:' + name, async (e, ...args) => {
    if (!privileged(e)) throw Error('不受信任的窗口');
    return fn(e, ...args);
  });
}
function guard(w) {
  w.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  for (const event of ['will-navigate', 'will-redirect', 'will-attach-webview'])
    w.webContents.on(event, (e) => e.preventDefault());
}
const webPreferences = {
  preload: path.join(BASE, 'desktop/preload.cjs'),
  contextIsolation: true,
  nodeIntegration: false,
  sandbox: true,
  webviewTag: false,
  webSecurity: true,
};
function showStudio(page) {
  if (!studio) {
    studio = new BrowserWindow({
      title: 'PetDesk · 桌宠工作台',
      width: 1300,
      height: 880,
      minWidth: 820,
      minHeight: 600,
      backgroundColor: '#f7f6fb',
      show: false,
      autoHideMenuBar: true,
      icon: path.join(DIST, 'art/icon.png'),
      webPreferences,
    });
    guard(studio);
    studio.once('ready-to-show', () => {
      studio?.show();
      if (page) studio?.webContents.send('petdesk:page', page);
    });
    studio.on('closed', () => {
      studio = null;
    });
    void studio.loadURL(HOME);
  } else {
    if (studio.isMinimized()) studio.restore();
    studio.show();
    studio.focus();
    if (page) studio.webContents.send('petdesk:page', page);
  }
}
function petSize() {
  return [
    Math.round(218 * Math.max(1, state.prefs.scale)),
    Math.round(52 + 194 * state.prefs.scale),
  ];
}
function move(p) {
  pet?.setPosition(Math.round(p.x), Math.round(p.y), false);
}
function placeBack() {
  if (!pet) return;
  const b = pet.getBounds(),
    a = screen.getDisplayMatching(b).workArea;
  move(confine(b.x, b.y, a, b.width, b.height));
  resetMotion(1300);
}
function setWalking(on) {
  if (walking === on) return;
  walking = on;
  pet?.webContents.send('petdesk:motion', { walking: on });
}
function resetMotion(delay = 1500) {
  waitUntil = Date.now() + delay;
  target = null;
  precise = null;
  setWalking(false);
}
function openPet() {
  if (pet) {
    pet.showInactive();
    return;
  }
  const a = screen.getDisplayMatching(
      studio?.getBounds() ?? screen.getPrimaryDisplay().workArea,
    ).workArea,
    [width, height] = petSize();
  const p = confine(
    a.x + a.width * 0.7,
    a.y + a.height - height,
    a,
    width,
    height,
  );
  pet = new BrowserWindow({
    ...p,
    width,
    height,
    show: false,
    frame: false,
    focusable: false,
    transparent: true,
    hasShadow: false,
    resizable: false,
    alwaysOnTop: true,
    skipTaskbar: true,
    backgroundColor: '#00000000',
    icon: path.join(DIST, 'art/icon.png'),
    webPreferences: { ...webPreferences, backgroundThrottling: false },
  });
  mask = null;
  ignoring = true;
  pet.setIgnoreMouseEvents(true);
  if (process.platform === 'darwin')
    pet.setVisibleOnAllWorkspaces(true, {
      visibleOnFullScreen: true,
      skipTransformProcessType: true,
    });
  guard(pet);
  pet.setAlwaysOnTop(true, 'floating');
  if (process.platform !== 'darwin') pet.setMenu(null);
  pet.once('ready-to-show', () => pet?.showInactive());
  pet.on('closed', () => {
    pet = null;
    drag = null;
    hover = false;
    menuOpen = false;
    clearInterval(loop);
    resetMotion();
    emit();
  });
  void pet.loadURL(HOME + '#pet');
  resetMotion(4000);
  lastTick = Date.now();
  loop = setInterval(tick, 33);
  emit();
}
function tick() {
  if (!pet || pet.isDestroyed()) return;
  const now = Date.now(),
    dt = (now - lastTick) / 1000;
  lastTick = now;
  const b = pet.getBounds();
  if (state.prefs.passThrough) drag = null;
  const ignore =
    state.prefs.passThrough ||
    shouldIgnore(mask, b, screen.getCursorScreenPoint(), false, !!drag);
  if (ignore !== ignoring) {
    pet.setIgnoreMouseEvents(ignore);
    ignoring = ignore;
  }
  hover = !ignore && !drag;
  if (drag) {
    if (now - drag.t > 45000) {
      drag = null;
      resetMotion();
      return;
    }
    const c = screen.getCursorScreenPoint(),
      a = screen.getDisplayNearestPoint(c).workArea;
    move(confine(c.x - drag.dx, c.y - drag.dy, a, b.width, b.height));
    return;
  }
  if (
    hover ||
    feedBusy ||
    menuOpen ||
    state.sleeping ||
    !state.prefs.roam ||
    state.reaction?.until > now
  ) {
    if (walking) resetMotion(1000);
    return;
  }
  if (now < waitUntil) return;
  const area = screen.getDisplayMatching(b).workArea;
  if (!target) {
    target = destination(b, area, b.width, b.height, state.prefs.edges);
    precise = { x: b.x, y: b.y };
    setWalking(true);
  }
  precise = advance(precise, target, 75, dt);
  move(confine(precise.x, precise.y, area, b.width, b.height));
  if (precise.arrived) {
    resetMotion(2200 + Math.random() * 3500);
    if (state.prefs.edges && Math.random() < 0.6)
      pet.webContents.send('petdesk:motion', { walking: false, play: true });
  }
}
async function runUtility(key) {
  try {
    if (key === 'release') {
      await shell.openExternal(REPO);
      return { ok: true };
    }
    if (key === 'downloads') {
      const dest = app.getPath('downloads');
      if (!(await fs.stat(dest)).isDirectory()) throw Error();
      if (await shell.openPath(dest)) throw Error();
    } else if (key === 'calculator') {
      if (process.platform === 'darwin') {
        if (await shell.openPath('/System/Applications/Calculator.app'))
          throw Error();
      } else {
        const candidates =
          process.platform === 'win32'
            ? [
                path.join(
                  process.env.SystemRoot || 'C:\\Windows',
                  'System32',
                  'calc.exe',
                ),
              ]
            : [
                '/usr/bin/gnome-calculator',
                '/usr/bin/kcalc',
                '/usr/bin/mate-calc',
                '/usr/bin/xcalc',
              ];
        const exe = candidates.find((p) => existsSync(p));
        if (!exe) throw Error();
        await new Promise((resolve, reject) => {
          const child = spawn(exe, [], {
            shell: false,
            stdio: 'ignore',
            detached: true,
          });
          child.once('error', reject);
          child.once('spawn', () => {
            child.unref();
            resolve();
          });
        });
      }
    } else return { ok: false, message: '不在桌宠的操作白名单内。' };
    dispatch({ type: 'utility', key });
    return { ok: true };
  } catch {
    return {
      ok: false,
      message: '未找到对应的系统程序或目录，未执行其他操作。',
    };
  }
}
function refreshTray() {
  if (!tray || tray.isDestroyed()) return;
  tray.setToolTip(
    'PetDesk · ' +
      state.model.name +
      (state.prefs.passThrough ? '（免打扰）' : ''),
  );
  tray.setContextMenu(
    Menu.buildFromTemplate([
      { label: '打开桌宠工作台', click: () => showStudio() },
      {
        label: pet ? '暂时隐藏桌宠' : '显示桌宠',
        click: () => {
          if (pet) pet.close();
          else openPet();
        },
      },
      {
        label: '免打扰（所有点击穿透）',
        type: 'checkbox',
        checked: state.prefs.passThrough,
        click: () => {
          drag = null;
          dispatch({
            type: 'pref',
            key: 'passThrough',
            value: !state.prefs.passThrough,
          });
        },
      },
      {
        label: '自由漫游',
        type: 'checkbox',
        checked: state.prefs.roam,
        click: () =>
          dispatch({ type: 'pref', key: 'roam', value: !state.prefs.roam }),
      },
      { type: 'separator' },
      { label: '退出 PetDesk', click: () => app.quit() },
    ]),
  );
}
function initTray() {
  try {
    tray = new Tray(
      nativeImage
        .createFromPath(path.join(DIST, 'art/tray.png'))
        .resize({ width: 20, height: 20 }),
    );
    tray.on('double-click', () => showStudio());
    refreshTray();
  } catch {
    console.warn(
      'PetDesk: no system tray. Launch the app again to open the studio.',
    );
  }
}
function petMenu() {
  if (!pet || menuOpen) return;
  menuOpen = true;
  resetMotion();
  const util = (k) => async () => {
    const r = await runUtility(k);
    if (!r.ok) dialog.showErrorBox('PetDesk', r.message);
  };
  Menu.buildFromTemplate([
    { label: `${state.model.name} · PetDesk`, enabled: false },
    { type: 'separator' },
    { label: '摸摸头', click: () => dispatch({ type: 'pat' }) },
    { label: '一起玩耍', click: () => dispatch({ type: 'play' }) },
    {
      label: state.sleeping ? '叫醒伙伴' : '小憩一下',
      click: () => dispatch({ type: 'nap' }),
    },
    {
      label: '自由漫游',
      type: 'checkbox',
      checked: state.prefs.roam,
      click: () =>
        dispatch({ type: 'pref', key: 'roam', value: !state.prefs.roam }),
    },
    {
      label: '桌面小工具',
      submenu: [
        { label: '现在几点？', click: () => dispatch({ type: 'time' }) },
        { label: '打开下载文件夹', click: util('downloads') },
        { label: '打开计算器', click: util('calculator') },
      ],
    },
    {
      label: '进入免打扰（从托盘恢复）',
      click: () => dispatch({ type: 'pref', key: 'passThrough', value: true }),
    },
    { type: 'separator' },
    { label: '返回工作台', click: () => showStudio() },
    { label: '导入模型…', click: () => showStudio('models') },
    {
      label: '收回桌宠',
      click: () => {
        showStudio();
        pet?.close();
      },
    },
    { label: '退出 PetDesk', click: () => app.quit() },
  ]).popup({
    window: pet,
    callback: () => {
      menuOpen = false;
      hover = false;
      resetMotion();
    },
  });
}
function wire() {
  handle('snapshot', () => snap());
  handle('act', (e, a) => {
    if (
      !a ||
      ![
        'pat',
        'play',
        'nap',
        'time',
        'pref',
        'select',
        'removed',
        'clear-log',
      ].includes(a.type)
    )
      return snap();
    const prev = state.prefs.scale;
    dispatch(a);
    if (pet && prev !== state.prefs.scale) {
      pet.setSize(...petSize());
      placeBack();
    }
    return snap();
  });
  handle('pet', (_e, on) => {
    if (on === true) {
      openPet();
      studio?.hide();
    }
    if (on === false) {
      showStudio();
      pet?.close();
    }
    return snap();
  });
  handle('quit', (e) => {
    if (e.sender === studio?.webContents) setImmediate(() => app.quit());
  });
  handle('studio', () => showStudio());
  handle('utility', (_e, k) => runUtility(k));
  handle('menu', (e) => {
    if (e.sender === pet?.webContents) petMenu();
  });
  handle('mask', (e, data) => {
    if (
      e.sender !== pet?.webContents ||
      typeof data !== 'string' ||
      data.length !== (((MASK_W * MASK_H) / 8) * 4) / 3
    )
      return;
    mask = Buffer.from(data, 'base64');
  });
  handle('drag', (e, on) => {
    if (e.sender !== pet?.webContents) return;
    if (on === true && !feedBusy && !state.prefs.passThrough) {
      const p = screen.getCursorScreenPoint(),
        b = pet.getBounds();
      resetMotion();
      drag = { dx: p.x - b.x, dy: p.y - b.y, t: Date.now() };
    } else {
      drag = null;
      resetMotion(1800);
    }
  });
  handle('feed', async (e, p) => {
    if (feedBusy) return { ok: false, message: '先等上一份点心处理完哦。' };
    feedBusy = true;
    drag = null;
    resetMotion();
    try {
      const win = BrowserWindow.fromWebContents(e.sender),
        home = app.getPath('home'),
        roots = ['desktop', 'downloads', 'documents']
          .map((k) => app.getPath(k))
          .filter((p) => p !== path.parse(p).root && p !== home);
      const systemDirs =
        process.platform === 'win32'
          ? [
              process.env.SystemRoot,
              process.env.ProgramFiles,
              process.env['ProgramFiles(x86)'],
              process.env.ProgramData,
              process.env.APPDATA,
              process.env.LOCALAPPDATA,
            ]
          : [
              '/etc',
              '/usr',
              '/bin',
              '/sbin',
              '/var',
              '/boot',
              '/opt',
              '/System',
              '/Library',
              '/Applications',
              path.join(home, 'Library'),
            ];
      const result = await feedOne(
        p,
        {
          roots,
          blocked: [
            ...systemDirs,
            app.getAppPath(),
            app.getPath('userData'),
            path.dirname(app.getPath('exe')),
          ],
        },
        {
          confirm: async (f) =>
            (
              await dialog.showMessageBox(win, {
                type: 'warning',
                title: 'PetDesk · 确认文件投喂',
                message: `让${state.model.name}吃掉「${f.name}」？`,
                detail: `${f.path}\n大小：${formatBytes(f.size)}\n\n确认后该文件会离开原位置，进入系统回收站。不想失去的文件请先备份。`,
                buttons: ['取消', '确认并移入回收站'],
                defaultId: 0,
                cancelId: 0,
                noLink: true,
              })
            ).response === 1,
          trash: (p) => shell.trashItem(p),
        },
      );
      if (result.ok) dispatch({ type: 'meal' });
      return result;
    } catch (err) {
      return { ok: false, message: err.message };
    } finally {
      feedBusy = false;
      resetMotion(2000);
    }
  });
}
if (!app.requestSingleInstanceLock()) {
  app.quit();
} else {
  app.on('second-instance', () => {
    if (app.isReady()) showStudio();
  });
  app.whenReady().then(() => {
    try {
      state = restoreState(JSON.parse(readFileSync(userFile(), 'utf8')));
    } catch {
      /* First launch or invalid settings. */
    }
    protocol.handle('petdesk', async (req) => {
      try {
        const u = new URL(req.url);
        if (u.hostname !== 'app' || req.method !== 'GET') throw Error();
        const f = path.join(DIST, decodeURIComponent(u.pathname));
        if (!within(DIST, f) || !mime[path.extname(f)]) throw Error();
        return new Response(await fs.readFile(f), {
          headers: {
            'Content-Type': mime[path.extname(f)],
            'Content-Security-Policy': CSP,
            'X-Content-Type-Options': 'nosniff',
          },
        });
      } catch {
        return new Response('Not found', { status: 404 });
      }
    });
    const sess = session.defaultSession;
    sess.setPermissionRequestHandler((_wc, _p, cb) => cb(false));
    sess.setPermissionCheckHandler(() => false);
    sess.on('will-download', (e) => e.preventDefault());
    sess.webRequest.onBeforeRequest(
      { urls: ['http://*/*', 'https://*/*', 'ws://*/*', 'wss://*/*'] },
      (_d, cb) => cb({ cancel: true }),
    );
    if (process.platform === 'darwin') app.setActivationPolicy('accessory');
    Menu.setApplicationMenu(null);
    wire();
    initTray();
    openPet();
    screen.on('display-removed', placeBack);
    screen.on('display-metrics-changed', placeBack);
    // Only an explicit tray/menu action or a second launch opens the studio.
  });
  app.on('window-all-closed', () => {
    if (!tray) app.quit();
  });
  app.on('before-quit', () => {
    clearInterval(loop);
    persist();
  });
}
