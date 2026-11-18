// Test-only observer imported from an isolated COPY of main.mjs, never from the shipped app.
import { app, BrowserWindow, dialog } from 'electron';
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import { setTimeout as delay } from 'node:timers/promises';
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
      evalIn(
        pet,
        "document.querySelector('.native .pet-art img')?.naturalWidth>0",
      ),
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
  await evalIn(pet, "window.petdesk.act({type:'pat'})");
  assert.equal(
    await evalIn(pet, 'window.petdesk.snapshot().then(r=>r.state.stats.bond)'),
    34,
  );
  await evalIn(pet, 'window.petdesk.studio()');
  const studio = await until(
    () => BrowserWindow.getAllWindows().find((w) => w !== pet),
    'studio window',
  );
  await until(
    () => evalIn(studio, "!!document.querySelector('.behavior')"),
    'studio DOM',
  );
  await evalIn(
    studio,
    `document.querySelector('[role="switch"][aria-label="免打扰模式"]').click()`,
  );
  await until(
    () =>
      evalIn(
        pet,
        'window.petdesk.snapshot().then(r=>r.state.prefs.passThrough)',
      ),
    'DND shared between windows',
  );
  log('PASS: isolated preload bridge, interaction and DND state');
  if (process.env.GITHUB_ACTIONS === 'true' && process.platform === 'win32') {
    const dl = path.join(dir, 'Downloads');
    fs.mkdirSync(dl);
    app.setPath('downloads', dl);
    const snack = path.join(dl, 'disposable-petdesk-qa.txt');
    fs.writeFileSync(snack, 'Disposable CI test fixture');
    const dev = studio.webContents.debugger;
    dev.attach();
    const { root } = await dev.sendCommand('DOM.getDocument');
    const { nodeId } = await dev.sendCommand('DOM.querySelector', {
      nodeId: root.nodeId,
      selector: 'input[aria-label="选择要投喂的文件"]',
    });
    let seen = 0;
    const original = dialog.showMessageBox;
    try {
      dialog.showMessageBox = async (_win, opts) => {
        assert.ok(opts.detail.includes(snack));
        assert.equal(opts.defaultId, 0);
        assert.equal(opts.cancelId, 0);
        return { response: seen++ === 0 ? 0 : 1 };
      };
      await dev.sendCommand('DOM.setFileInputFiles', {
        nodeId,
        files: [snack],
      });
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
      await dev.sendCommand('DOM.setFileInputFiles', {
        nodeId,
        files: [snack],
      });
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
