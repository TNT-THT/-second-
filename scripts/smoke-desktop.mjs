// Exercise a disposable copy of the real app instead of enabling a Node inspector in the product.
// A Windows runner can point PETDESK_BINARY at the already-packaged exe. Its ZIP is never modified.
import fs from 'node:fs/promises';
import { spawn } from 'node:child_process';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';
const root = fileURLToPath(new URL('../', import.meta.url)),
  tmp = await fs.mkdtemp(
    path.join(process.env.RUNNER_TEMP || os.tmpdir(), 'petdesk-smoke-'),
  );
let child,
  timeout,
  stderr = '';
const resultFile = path.join(tmp, 'result.json'),
  trace = path.join(tmp, 'trace.log');
try {
  let exe,
    appDir,
    args = [];
  if (process.env.PETDESK_BINARY) {
    const original = path.resolve(process.env.PETDESK_BINARY),
      copy = path.join(tmp, 'runtime');
    await fs.cp(path.dirname(original), copy, { recursive: true });
    exe = path.join(copy, path.basename(original));
    appDir = path.join(copy, 'resources', 'app');
  } else {
    exe = createRequire(import.meta.url)('electron');
    appDir = path.join(tmp, 'app');
    await fs.mkdir(appDir);
    for (const d of ['core', 'desktop', 'dist'])
      await fs.cp(path.join(root, d), path.join(appDir, d), {
        recursive: true,
      });
    await fs.copyFile(
      path.join(root, 'package.json'),
      path.join(appDir, 'package.json'),
    );
    args.push(appDir);
  }
  await fs.copyFile(
    new URL('./native-probe.mjs', import.meta.url),
    path.join(appDir, 'desktop/native-probe.mjs'),
  );
  await fs.copyFile(
    new URL('./win-hit-probe.ps1', import.meta.url),
    path.join(appDir, 'desktop/win-hit-probe.ps1'),
  );
  const main = path.join(appDir, 'desktop/main.mjs');
  await fs.writeFile(
    main,
    "import './native-probe.mjs';\n" + (await fs.readFile(main, 'utf8')),
  );
  child = spawn(
    exe,
    [
      `--user-data-dir=${path.join(tmp, 'profile')}`,
      '--enable-logging=stderr',
      ...args,
    ],
    {
      stdio: ['ignore', 'pipe', 'pipe'],
      env: { ...process.env, PETDESK_SMOKE_DIR: tmp },
    },
  );
  for (const p of [child.stdout, child.stderr])
    p.on('data', (d) => {
      stderr = (stderr + d).slice(-5000);
    });
  const code = await new Promise((res, rej) => {
    child.on('error', rej);
    child.on('exit', res);
    timeout = setTimeout(() => {
      child.kill();
      rej(Error('Desktop smoke timed out'));
    }, 120000);
  });
  const result = JSON.parse(await fs.readFile(resultFile, 'utf8'));
  if (!result.ok || code !== 0) throw Error(result.message || `Exit: ${code}`);
  console.log(await fs.readFile(trace, 'utf8'));
} catch (e) {
  const detail = await fs
    .readFile(trace, 'utf8')
    .catch(() => '(probe did not start)');
  const msg = `${e.message}\n${detail}\n${stderr}`;
  if (process.env.GITHUB_ACTIONS)
    console.error(
      '::error title=Desktop smoke::' +
        msg
          .slice(-7000)
          .replaceAll('%', '%25')
          .replaceAll('\r', '%0D')
          .replaceAll('\n', '%0A'),
    );
  throw Error(msg);
} finally {
  clearTimeout(timeout);
  if (child?.exitCode === null) child.kill();
  await fs.rm(tmp, {
    recursive: true,
    force: true,
    maxRetries: 6,
    retryDelay: 300,
  });
}
