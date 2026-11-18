import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import { feedOne, inspectFood } from '../desktop/food.mjs';
async function fixture(t) {
  const root = await fs.realpath(
    await fs.mkdtemp(path.join(os.tmpdir(), 'petdesk-test-')),
  );
  t.after(() => fs.rm(root, { recursive: true, force: true }));
  const folder = path.join(root, 'Downloads');
  await fs.mkdir(folder);
  const file = path.join(folder, 'snack.txt');
  await fs.writeFile(file, 'a disposable test fixture');
  return {
    root,
    folder,
    file,
    policy: { roots: [folder], blocked: [path.join(folder, 'app')] },
  };
}
test('cancel never calls the trash operation', async (t) => {
  const { file, policy } = await fixture(t);
  const r = await feedOne(file, policy, {
    confirm: async (f) => {
      assert.equal(f.path, file);
      return false;
    },
    trash: () => assert.fail('must not trash'),
  });
  assert.equal(r.cancelled, true);
  assert.equal((await fs.stat(file)).isFile(), true);
});
test('only confirmed, unchanged regular file is handed to OS trash once', async (t) => {
  const { file, policy } = await fixture(t);
  let n = 0;
  const r = await feedOne(file, policy, {
    confirm: () => true,
    trash: async (p) => {
      assert.equal(p, file);
      n++;
    },
  });
  assert.equal(r.ok, true);
  assert.equal(n, 1);
});
test('refuses changed file after confirmation and never falls back to unlink', async (t) => {
  const { file, policy } = await fixture(t);
  await assert.rejects(
    feedOne(file, policy, {
      confirm: async () => {
        await fs.appendFile(file, 'new');
        return true;
      },
      trash: () => assert.fail(),
    }),
    /变化/,
  );
  await assert.rejects(
    feedOne(file, policy, {
      confirm: () => true,
      trash: () => {
        throw Error('no trash');
      },
    }),
    /不会改用永久删除/,
  );
  assert.ok((await fs.readFile(file, 'utf8')).endsWith('new'));
});
test('rejects directories, neighboring prefix, hidden files, executables, app data and hardlinks', async (t) => {
  const { file, root, policy, folder } = await fixture(t);
  for (const n of [
    'Downloads-other/a.txt',
    'Downloads/.hidden/n.txt',
    'Downloads/.key.txt',
    'Downloads/test.exe',
    'Downloads/app/assets/a.png',
  ]) {
    const f = path.join(root, n);
    await fs.mkdir(path.dirname(f), { recursive: true });
    await fs.writeFile(f, 'x');
    await assert.rejects(inspectFood(f, policy));
  }
  await assert.rejects(inspectFood(folder, policy));
  const h = path.join(folder, 'link.txt');
  await fs.link(file, h);
  await assert.rejects(inspectFood(h, policy));
});
test('rejects symbolic links including linked parents', async (t) => {
  const { root, folder, file, policy } = await fixture(t);
  const linked = path.join(folder, 'linked.txt');
  try {
    await fs.symlink(file, linked);
  } catch (e) {
    if (e.code === 'EPERM') {
      t.skip('OS forbids symlink creation');
      return;
    }
    throw e;
  }
  await assert.rejects(inspectFood(linked, policy));
  const outside = path.join(root, 'Private');
  await fs.mkdir(outside);
  await fs.writeFile(path.join(outside, 'x.txt'), 'untouched');
  const d = path.join(folder, 'redirected');
  await fs.symlink(outside, d, 'dir');
  await assert.rejects(inspectFood(path.join(d, 'x.txt'), policy));
});
