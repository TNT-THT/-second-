// Test-only observer imported from an isolated COPY of main.mjs, never from the shipped app.
import { app, BrowserWindow, dialog, screen } from 'electron';
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import { setTimeout as delay } from 'node:timers/promises';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { fileURLToPath } from 'node:url';
const runFile = promisify(execFile);
async function hitProbe(w, a, b, gesture) {
  const p = screen.dipToScreenPoint(a),
    q = screen.dipToScreenPoint(b);
  const raw = w.getNativeWindowHandle(),
    h = (
      raw.length >= 8 ? raw.readBigUInt64LE(0) : raw.readUInt32LE(0)
    ).toString();
  await runFile(
    path.join(
      process.env.SystemRoot,
      'System32',
      'WindowsPowerShell',
      'v1.0',
      'powershell.exe',
    ),
    [
      '-NoProfile',
      '-NonInteractive',
      '-File',
      fileURLToPath(new URL('./win-hit-probe.ps1', import.meta.url)),
      '-Handle',
      h,
      '-X',
      String(p.x),
      '-Y',
      String(p.y),
      '-X2',
      String(q.x),
      '-Y2',
      String(q.y),
      '-Gesture',
      gesture,
    ],
    { timeout: 16000, windowsHide: true },
  );
}
const dir = process.env.PETDESK_SMOKE_DIR;
if (!dir) throw Error('Run via npm run smoke:desktop');
const log = (m) =>
  fs.appendFileSync(path.join(dir, 'trace.log'), String(m) + '\n');
let ended = false;
function finish(ok, msg) {
  if (ended) return;
  ended = true;
  log(msg);
  fs.writeFileSync(
    path.join(dir, 'result.json'),
    JSON.stringify({ ok, message: String(msg) }),
  );
  app.exit(ok ? 0 : 1);
}
process.on('uncaughtException', (e) => finish(false, e.stack));
process.on('unhandledRejection', (e) => finish(false, e?.stack || e));
log('Probe loaded before app ready: ' + !app.isReady());
app.on('browser-window-created', (_e, w) => {
  log('Window created: ' + w.id);
  w.webContents.on('did-fail-load', (_e, c, d) => log(`Load failed ${c} ${d}`));
  w.webContents.on('render-process-gone', (_e, d) =>
    log('Renderer exit: ' + d.reason),
  );
  w.webContents.on('console-message', (_e, details) => {
    if (details?.level === 'error') log('Renderer: ' + details.message);
  });
});
async function until(query, label, ms = 20000) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    const v = await query();
    if (v) return v;
    await delay(80);
  }
  throw Error('Timed out: ' + label);
}
const evalIn = (w, code) => w.webContents.executeJavaScript(code, true);
async function run() {
  const pet = await until(
    () =>
      BrowserWindow.getAllWindows().find((w) =>
        w.webContents.getURL().endsWith('#pet'),
      ),
    'pet URL',
  );
  await until(
    () =>
      evalIn(pet, "!!document.querySelector('.native .qwen-actor[data-clip]')"),
    'visible companion',
  );
  assert.equal(BrowserWindow.getAllWindows().length, 1);
  assert.equal(pet.isAlwaysOnTop(), true);
  assert.equal(pet.isFocusable(), false);
  assert.equal(pet.isFocused(), false);
  assert.equal(
    await evalIn(pet, 'getComputedStyle(document.body).backgroundColor'),
    'rgba(0, 0, 0, 0)',
  );
  log(
    'PASS: direct pet-only launch, alpha background, topmost/nonfocusable, no keyboard focus',
  );
  await evalIn(pet, "window.petdesk.act({type:'pref',key:'roam',value:false})");
  await until(() => pet.isVisible(), 'pet window visible');
  if (process.env.GITHUB_ACTIONS === 'true' && process.platform === 'win32') {
    const b = pet.getBounds(),
      center = {
        x: b.x + Math.floor(b.width / 2),
        y: b.y + Math.floor(b.height * 0.5),
      },
      corner = { x: b.x + 5, y: b.y + 5 };
    await hitProbe(pet, center, corner, 'tap');
    await until(
      () =>
        evalIn(
          pet,
          'window.petdesk.snapshot().then(r=>r.state.stats.bond===36)',
        ),
      'two OS clicks on actor and transparent corner',
    );
    assert.equal(pet.isFocused(), false);
    log(
      'PASS: Windows WindowFromPoint and OS-routed clicks, including the transparent corner, and no hit outside the window; no focus theft',
    );
    const area = screen.getDisplayMatching(b).workArea;
    // Move the actual mouse, entering a short climb 44 DIP below the top-right corner.
    const end = {
      x: area.x + area.width - b.width + Math.floor(b.width / 2) - 1,
      y: area.y + 44 + Math.floor(b.height * 0.5),
    };
    await hitProbe(pet, center, end, 'drag');
    assert.ok(Math.abs(pet.getBounds().x + b.width - area.x - area.width) < 4);
    await evalIn(
      pet,
      "window.petdesk.act({type:'pref',key:'roam',value:true})",
    );
    await until(
      () =>
        evalIn(
          pet,
          "document.querySelector('.qwen-actor')?.dataset.clip==='climb'",
        ),
      'actual wall pose after dragging to edge',
    );
    await until(
      () =>
        evalIn(
          pet,
          "document.querySelector('.qwen-actor')?.dataset.clip==='glide'",
        ),
      'ceiling fan traverse',
    );
    assert.equal(pet.getBounds().y, area.y);
    log('PASS: real pickup/drag, right-wall climb and top-edge fan traverse');
    await evalIn(
      pet,
      "window.petdesk.act({type:'pref',key:'roam',value:false})",
    );
  }
  await evalIn(pet, 'window.petdesk.studio()');
  const studio = await until(
    () => BrowserWindow.getAllWindows().find((w) => w !== pet),
    'studio window',
  );
  await until(
    () => evalIn(studio, "!!document.querySelector('.behavior')"),
    'studio DOM',
  );
  const before = pet.getBounds();
  await evalIn(
    studio,
    `(()=>{const r=document.querySelector('.behavior input[type=range]');Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(r,'1.4');r.dispatchEvent(new Event('input',{bubbles:true}));})()`,
  );
  await until(
    () => pet.getBounds().height > before.height * 1.38,
    'workspace scale updates native bounds',
  );
  const bigger = pet.getBounds();
  assert.ok(Math.abs(bigger.width / before.width - 1.4) < 0.015);
  assert.ok(Math.abs(bigger.height / before.height - 1.4) < 0.015);
  const box = await evalIn(
    pet,
    "(()=>{const b=document.querySelector('.pet-node').getBoundingClientRect();return [b.width,b.height,innerWidth,innerHeight]})()",
  );
  assert.ok(Math.abs(box[0] - box[2]) <= 1 && Math.abs(box[1] - box[3]) <= 1);
  log(
    'PASS: uniform scaling of art, hit target and native window via workstation slider',
  );
  if (process.env.GITHUB_ACTIONS === 'true' && process.platform === 'win32') {
    const dl = path.join(dir, 'Downloads');
    fs.mkdirSync(dl);
    app.setPath('downloads', dl);
    const snack = path.join(dl, 'disposable-petdesk-qa.txt');
    fs.writeFileSync(snack, 'Disposable CI test fixture');
    await evalIn(
      pet,
      `(()=>{const i=document.createElement('input');i.type='file';i.id='qa-fixture';i.hidden=true;document.body.append(i);})()`,
    );
    const dev = pet.webContents.debugger;
    dev.attach();
    const { root } = await dev.sendCommand('DOM.getDocument');
    const { nodeId } = await dev.sendCommand('DOM.querySelector', {
      nodeId: root.nodeId,
      selector: '#qa-fixture',
    });
    const dropFixture = async () => {
      await dev.sendCommand('DOM.setFileInputFiles', {
        nodeId,
        files: [snack],
      });
      await evalIn(
        pet,
        `(()=>{const b=document.querySelector('.pet-node'),dt=new DataTransfer();dt.items.add(document.querySelector('#qa-fixture').files[0]);for(const name of ['dragover','drop'])b.dispatchEvent(new DragEvent(name,{bubbles:true,cancelable:true,dataTransfer:dt}));})()`,
      );
    };
    let seen = 0;
    const original = dialog.showMessageBox;
    try {
      dialog.showMessageBox = async (_win, opts) => {
        assert.ok(opts.detail.includes(snack));
        assert.equal(opts.defaultId, 0);
        assert.equal(opts.cancelId, 0);
        return { response: seen++ === 0 ? 0 : 1 };
      };
      await dropFixture();
      await until(() => seen === 1, 'real File path and cancel dialog');
      assert.ok(fs.existsSync(snack));
      assert.equal(
        await evalIn(
          studio,
          'window.petdesk.snapshot().then(r=>r.state.meals)',
        ),
        0,
      );
      await delay(200);
      await dropFixture();
      await until(() => seen === 2, 'confirm dialog');
      await until(
        () =>
          evalIn(
            studio,
            'window.petdesk.snapshot().then(r=>r.state.meals===1)',
          ),
        'actual Windows recycle bin operation',
      );
      assert.equal(fs.existsSync(snack), false);
      await until(
        () =>
          evalIn(
            pet,
            "document.querySelector('.qwen-actor')?.dataset.clip==='eat'",
          ),
        'successful file meal draws the nibbling animation',
      );
      log(
        'PASS: disposable fixture; cancel preserved file; confirm moved it using real shell.trashItem',
      );
    } finally {
      dialog.showMessageBox = original;
      dev.detach();
    }
  }
  await evalIn(
    studio,
    `Array.from(document.querySelectorAll('nav button')).find(b=>b.textContent==='模型库').click()`,
  );
  await until(
    () => evalIn(studio, "!!document.querySelector('.library')"),
    'model library',
  );
  await evalIn(
    studio,
    `(async()=>{const blob=await(await fetch('./art/qwen.webp')).blob();const input=document.querySelector('.library input[type=file]'),dt=new DataTransfer();dt.items.add(new File([blob],'probe.webp',{type:'image/webp'}));input.files=dt.files;input.dispatchEvent(new Event('change',{bubbles:true}));})()`,
  );
  await until(
    () =>
      evalIn(
        pet,
        "document.querySelector('.pet-art img')?.src.startsWith('blob:')",
      ),
    'model sync via local IndexedDB',
  );
  await evalIn(studio, 'window.petdesk.setPet(true)');
  assert.equal(studio.isVisible(), false);
  assert.equal(pet.isVisible(), true);
  log(
    'PASS: model import synced to real pet; returning to desktop hides studio',
  );
  finish(true, 'All desktop smoke checks passed.');
}
app
  .whenReady()
  .then(() => setImmediate(() => run().catch((e) => finish(false, e.stack))));
