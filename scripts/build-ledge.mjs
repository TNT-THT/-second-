// Compile with the .NET Framework compiler included on supported Windows desktops. The
// official Starter (including Windows ARM64) and portable are built by the Windows job.
import { execFileSync } from 'node:child_process';
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
  execFileSync(
    csc,
    [
      '/nologo',
      '/optimize+',
      '/platform:anycpu',
      '/target:exe',
      '/out:dist/native/WindowLedge.exe',
      'desktop/window-ledge.cs',
    ],
    { stdio: 'inherit' },
  );
}
