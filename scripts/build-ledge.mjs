// Compile with the .NET Framework compiler included on supported Windows desktops. The
// official Starter (including Windows ARM64) and portable are built by the Windows job.
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
if (process.platform === 'win32') {
  const base = path.join(
    process.env.SystemRoot || 'C:\\Windows',
    'Microsoft.NET',
  );
  const csc = ['Framework64', 'Framework']
    .map((f) => path.join(base, f, 'v4.0.30319', 'csc.exe'))
    .find((p) => fs.existsSync(p));
  if (!csc) throw Error('找不到 Windows .NET Framework C# 编译器。');
  fs.mkdirSync('dist/native', { recursive: true });
  // The legacy Framework compiler's argument parser does not accept POSIX-style
  // directory separators in all switch values. Feed it native absolute paths.
  const args = [
    '/nologo',
    '/optimize+',
    '/platform:anycpu',
    '/target:exe',
    `/out:${path.resolve('dist', 'native', 'WindowLedge.exe')}`,
    path.resolve('desktop', 'window-ledge.cs'),
  ];
  const r = spawnSync(csc, args, { encoding: 'utf8', windowsHide: true });
  const detail = [r.error?.message, r.stdout, r.stderr]
    .filter(Boolean)
    .join('\n');
  if (detail) console.log(detail);
  if (r.status !== 0) throw Error('Building WindowLedge failed: ' + detail);
}
