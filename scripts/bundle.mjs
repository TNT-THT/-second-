// Stage a runnable app (zero runtime npm dependencies) plus optional runtime bootstraps.
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { zipSync } from 'fflate';
const root = fileURLToPath(new URL('../', import.meta.url));
process.chdir(root);
const pkg = JSON.parse(await fs.readFile('package.json')),
  ver = pkg.devDependencies.electron,
  tag = 'PetDesk-' + pkg.version;
const archive = path.join('out', tag + '-Starter.zip'),
  dir = path.join('out', tag),
  appDir = path.join(dir, 'app');
await fs.access('dist/index.html');
await fs.rm(dir, { recursive: true, force: true });
await fs.mkdir(appDir, { recursive: true });
for (const d of ['core', 'desktop', 'dist'])
  await fs.cp(d, path.join(appDir, d), { recursive: true });
await fs.writeFile(
  path.join(appDir, 'package.json'),
  JSON.stringify(
    {
      name: pkg.name,
      productName: pkg.productName,
      version: pkg.version,
      description: pkg.description,
      type: 'module',
      main: pkg.main,
    },
    null,
    2,
  ),
);
await fs.copyFile('docs/LOCAL-TEST.md', path.join(dir, '使用说明.txt'));
await fs.copyFile('docs/ART.md', path.join(dir, 'ART.md'));
const licenseDir = path.join(appDir, 'licenses');
await fs.mkdir(licenseDir, { recursive: true });
for (const lib of [
  'react',
  'react-dom',
  'scheduler',
  'three',
  'lucide-react',
  '@fontsource-variable/noto-sans-sc',
  'electron',
])
  await fs.copyFile(
    path.join('node_modules', lib, 'LICENSE'),
    path.join(licenseDir, lib.replaceAll('/', '-') + '.txt'),
  );
const sums = JSON.parse(
  await fs.readFile('node_modules/electron/checksums.json'),
);
const substitutions = { '@VERSION@': ver };
for (const [key, plat] of Object.entries({
  WIN: 'win32',
  MAC: 'darwin',
  LINUX: 'linux',
}))
  for (const arch of ['arm64', 'x64'])
    substitutions[`@${key}_${arch.toUpperCase()}@`] =
      sums[`electron-v${ver}-${plat}-${arch}.zip`];
for (const [src, name] of [
  ['Start-Windows.cmd', 'Start-Windows.cmd'],
  ['start-unix.sh', 'Start-macOS.command'],
  ['start-unix.sh', 'Start-Linux.sh'],
]) {
  let text = await fs.readFile('scripts/launchers/' + src, 'utf8');
  for (const [k, v] of Object.entries(substitutions)) {
    if (!v) throw Error('Missing runtime checksum ' + k);
    text = text.replaceAll(k, v);
  }
  if (name.endsWith('.cmd')) text = text.replace(/\r?\n/g, '\r\n');
  await fs.writeFile(path.join(dir, name), text, { mode: 0o755 });
}
const files = {};
async function add(folder) {
  for (const ent of await fs.readdir(folder, { withFileTypes: true })) {
    const f = path.join(folder, ent.name);
    if (ent.isDirectory()) await add(f);
    else {
      const rel = path.relative('out', f).split(path.sep).join('/');
      files[rel] = [
        new Uint8Array(await fs.readFile(f)),
        {
          os: 3,
          attrs:
            ((/\.(sh|command)$/.test(f) ? 0o100755 : 0o100644) << 16) >>> 0,
        },
      ];
    }
  }
}
await add(dir);
const bytes = zipSync(files, { level: 6 });
await fs.writeFile(archive, bytes);
await fs.writeFile(
  archive + '.sha256',
  createHash('sha256').update(bytes).digest('hex') +
    '  ' +
    path.basename(archive) +
    '\n',
);
console.log(
  'Created ' + archive + ' (' + (bytes.length / 1048576).toFixed(1) + ' MiB)',
);
