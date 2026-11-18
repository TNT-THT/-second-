// Run on a graphical desktop or a Windows runner. Uses isolated preferences; does not feed files.
import { _electron as electron } from 'playwright-core';
import { mkdtemp, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
const root = fileURLToPath(new URL('../', import.meta.url)),
  profile = await mkdtemp(path.join(os.tmpdir(), 'petdesk-smoke-'));
let app;
try {
  app = await electron.launch({
    args: [`--user-data-dir=${profile}`, root],
    timeout: 60000,
  });
  const pet = await app.firstWindow({ timeout: 40000 });
  const errors = [];
  pet.on('pageerror', (e) => errors.push(e.message));
  await pet.locator('.native .pet-art img').waitFor();
  await pet.waitForFunction(
    () => document.querySelector('.pet-art img')?.naturalWidth > 0,
  );
  assert.equal(pet.url(), 'petdesk://app/index.html#pet');
  assert.equal(
    await pet.evaluate(() => getComputedStyle(document.body).backgroundColor),
    'rgba(0, 0, 0, 0)',
  );
  const native = await app.evaluate(({ BrowserWindow }) => {
    const all = BrowserWindow.getAllWindows(),
      w = all[0];
    return {
      count: all.length,
      top: w.isAlwaysOnTop(),
      focusable: w.isFocusable(),
      visible: w.isVisible(),
    };
  });
  assert.equal(native.count, 1);
  assert.equal(native.top, true);
  assert.equal(native.focusable, false);
  await pet.evaluate(() =>
    window.petdesk.act({ type: 'pref', key: 'roam', value: false }),
  );
  await pet.evaluate(() => window.petdesk.act({ type: 'pat' }));
  assert.equal(
    await pet.evaluate(
      async () => (await window.petdesk.snapshot()).state.stats.bond,
    ),
    34,
  );
  const next = app.waitForEvent('window');
  await pet.evaluate(() => window.petdesk.studio());
  const studio = await next;
  studio.on('pageerror', (e) => errors.push(e.message));
  await studio.locator('h1').waitFor();
  await studio.getByRole('switch', { name: '免打扰模式', exact: true }).click();
  await pet.waitForFunction(
    async () =>
      (await window.petdesk.snapshot()).state.prefs.passThrough === true,
  );
  await studio.getByRole('button', { name: '模型库', exact: true }).click();
  await studio
    .locator('input[aria-label="选择模型文件"]')
    .setInputFiles(path.join(root, 'public/art/qwen.webp'));
  await pet.waitForFunction(() =>
    document.querySelector('.pet-art img')?.src.startsWith('blob:'),
  );
  await studio.getByRole('button', { name: '回到桌面', exact: true }).click();
  const visible = await app.evaluate(({ BrowserWindow }) =>
    BrowserWindow.getAllWindows()
      .filter((w) => w.isVisible())
      .map((w) => w.webContents.getURL()),
  );
  assert.deepEqual(visible, ['petdesk://app/index.html#pet']);
  assert.deepEqual(errors, []);
  console.log(
    'PASS native: pet-only launch, transparent always-on-top nonfocusable window, real two-window IPC, DND, local model sync, only pet visible on return.',
  );
} finally {
  await app?.close();
  await rm(profile, {
    force: true,
    recursive: true,
    maxRetries: 4,
    retryDelay: 200,
  });
}
