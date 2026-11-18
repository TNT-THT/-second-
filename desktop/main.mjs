import { windowLedges } from './ledges.mjs';
import { motionOptions, canFly, allowed } from '../core/repertoire.mjs';
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
import {
  Wander,
  STILL,
  actorSize,
  resizeAnchored,
  confine,
  contains,
} from '../core/motion.mjs';
import { feedOne, within } from './food.mjs';

const BASE = fileURLToPath(new URL('../', import.meta.url)),
  DIST = path.join(BASE, 'dist');
const HOME = 'petdesk://app/index.html',
  REPO = 'https://github.com/TNT-THT/-second-/releases/tag/petdesk-v1.2.0';
const ledges = windowLedges(path.join(DIST, 'native', 'WindowLedge.exe'), (p) =>
  screen.screenToDipPoint(p),
);
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
let wander = null,
  drag = null,
  lastTick = 0,
  loop,
  tray = null,
  motion = { ...STILL },
  motionKey = '';
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
const snap = () => ({ state, petOpen: !!pet && !pet.isDestroyed(), motion });
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
function move(p) {
  pet?.setPosition(Math.round(p.x), Math.round(p.y), false);
}
function sendPose(next) {
  motion = next;
  const key = [
    next.kind,
    next.face,
    next.moving,
    next.surface,
    Math.floor(next.phase * 48),
  ].join(':');
  if (key !== motionKey && pet && !pet.isDestroyed()) {
    motionKey = key;
    pet.webContents.send('petdesk:motion', next);
  }
}
function placeBack() {
  if (!pet) return;
  const b = pet.getBounds(),
    area = screen.getDisplayMatching(b).workArea,
    size = actorSize(state.prefs.scale, area);
  const wanted = resizeAnchored(b, size, area, wander?.surface);
  const pos = wander.place(wanted, area, size, {
    wait: 0.75,
    surface: wander.surface,
    ledge: ledges.get(state.prefs.ledges),
  });
  pet.setBounds({ ...pos, ...size }, false);
  sendPose(wander.track());
}
function resetMotion(delay = 1200) {
  wander?.stop(delay / 1000);
  if (wander) sendPose(wander.track());
}
function openPet() {
  if (pet) {
    pet.showInactive();
    return;
  }
  const a = screen.getDisplayMatching(
      studio?.getBounds() ?? screen.getPrimaryDisplay().workArea,
    ).workArea,
    { width, height } = actorSize(state.prefs.scale, a);
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
  // No cursor polling to turn hit-testing on/off. The compact actor window ALWAYS receives
  // mouse and file drops; outside this rectangle the underlying desktop remains untouched.
  pet.setIgnoreMouseEvents(false);
  wander = new Wander(p, a, { width, height }, { wait: 3, surface: 'floor' });
  motionKey = '';
  motion = wander.track();
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
    ledges.stop();
    wander = null;
    motion = { ...STILL };
    motionKey = '';
    emit();
  });
  void pet.loadURL(HOME + '#pet');
  resetMotion(4000);
  lastTick = Date.now();
  loop = setInterval(tick, 33);
  emit();
}
function settleDrag() {
  if (!drag || !pet || !wander) return;
  const d = drag;
  drag = null;
  const cursor = screen.getCursorScreenPoint(),
    b = pet.getBounds();
  const moved = d.moved || Math.hypot(cursor.x - d.sx, cursor.y - d.sy) >= 4;
  // Sample the release too: a short flick can begin and end between two motion ticks.
  const a = (
    moved ? screen.getDisplayNearestPoint(cursor) : screen.getDisplayMatching(b)
  ).workArea;
  const size = actorSize(state.prefs.scale, a),
    target = moved ? { x: cursor.x - d.dx, y: cursor.y - d.dy } : b;
  if (!moved && Date.now() - d.t < 180) {
    sendPose(wander.track());
    return;
  }
  const opts = motionOptions(state);
  const pos = wander.place(target, a, size, {
    ...opts,
    wait: 1.1,
    snap: moved,
    release: true,
    ledge: ledges.get(state.prefs.ledges),
  });
  pet.setBounds({ ...pos, ...size }, false);
  sendPose(wander.track());
}
function tick() {
  if (!pet || pet.isDestroyed() || !wander) return;
  const now = Date.now(),
    dt = (now - lastTick) / 1000;
  lastTick = now;
  const b = pet.getBounds(),
    cursor = screen.getCursorScreenPoint();
  hover = contains(b, cursor);
  if (drag) {
    if (now - drag.t > 60000) {
      settleDrag();
      return;
    }
    if (!drag.moved && Math.hypot(cursor.x - drag.sx, cursor.y - drag.sy) < 4)
      return;
    drag.moved = true;
    const area = screen.getDisplayNearestPoint(cursor).workArea;
    move(
      confine(cursor.x - drag.dx, cursor.y - drag.dy, area, b.width, b.height),
    );
    return;
  }
  const pose = wander.step(dt, {
    ...motionOptions(state, now),
    ledge: ledges.get(state.prefs.ledges, now),
    blocked:
      hover ||
      feedBusy ||
      menuOpen ||
      state.sleeping ||
      state.reaction?.until > now,
  });
  if (Math.round(wander.pos.x) !== b.x || Math.round(wander.pos.y) !== b.y)
    move(wander.pos);
  sendPose(pose);
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
  tray.setToolTip('PetDesk · ' + state.model.name + '（可拖动 / 可投喂）');
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
    { label: '说说话', click: () => dispatch({ type: 'chat' }) },
    ...[
      ['play', '跳扇舞'],
      ['read', '翻绘本'],
      ['tea', '喝花茶'],
    ].map(([type, label]) => ({
      label,
      enabled: allowed(state.prefs, type),
      click: () => dispatch({ type }),
    })),
    {
      label: '自由飞行',
      type: 'checkbox',
      checked: state.prefs.flight,
      enabled: canFly(state.model) && allowed(state.prefs, 'fly'),
      click: () =>
        dispatch({ type: 'pref', key: 'flight', value: !state.prefs.flight }),
    },
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
        'chat',
        'tea',
        'read',
        'animation',
        'model-caps',
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
  handle('drag', (e, on) => {
    if (e.sender !== pet?.webContents) return;
    if (on === true && !feedBusy && !drag) {
      const p = screen.getCursorScreenPoint(),
        b = pet.getBounds();
      resetMotion();
      drag = {
        dx: p.x - b.x,
        dy: p.y - b.y,
        sx: p.x,
        sy: p.y,
        t: Date.now(),
        moved: false,
      };
    } else if (on === false) settleDrag();
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
    ledges.stop();
    persist();
  });
}
