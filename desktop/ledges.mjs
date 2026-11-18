import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
// Only the main process sees ephemeral geometry. Keep a single worker, bounded protocol,
// no stale platform when a read fails or when a window stops being foreground.
export function windowLedges(exe, toDip) {
  let child = null,
    current = null,
    last = 0,
    asked = 0,
    pending = false,
    retry = 0;
  function stop() {
    const old = child;
    child = null;
    current = null;
    pending = false;
    last = 0;
    asked = 0;
    retry = 0;
    if (old) {
      old.stdin.end();
      old.kill();
    }
  }
  function get(on, now = Date.now()) {
    if (process.platform !== 'win32' || !on) {
      if (child) stop();
      return null;
    }
    if (!child && now >= retry) {
      retry = now + 5000;
      if (!existsSync(exe)) return null;
      const c = spawn(exe, [], {
        stdio: ['pipe', 'pipe', 'ignore'],
        shell: false,
        windowsHide: true,
      });
      child = c;
      let buf = '';
      const lost = () => {
        if (child === c) {
          stop();
          retry = Date.now() + 5000;
        }
      };
      c.on('error', lost);
      c.once('exit', lost);
      c.stdin.on('error', lost);
      c.stdout.setEncoding('utf8');
      c.stdout.on('data', (s) => {
        if (child !== c) return;
        buf += s;
        if (buf.length > 512) {
          lost();
          return;
        }
        let i;
        while ((i = buf.indexOf('\n')) >= 0) {
          const line = buf.slice(0, i).trim();
          buf = buf.slice(i + 1);
          pending = false;
          last = Date.now();
          current = null;
          const m =
            /^([A-F0-9]{1,16}) (-?\d{1,7}) (-?\d{1,7}) (-?\d{1,7})$/.exec(line);
          if (!m) continue;
          const left = toDip({ x: +m[2], y: +m[3] }),
            right = toDip({ x: +m[4] - 1, y: +m[3] });
          current = {
            id: m[1],
            x: left.x,
            y: left.y,
            width: right.x - left.x + 1,
          };
        }
      });
    }
    if (child && pending && now - asked > 2000) {
      stop();
      retry = now + 3000;
      return null;
    }
    if (child && !pending && now - asked >= 200) {
      asked = now;
      pending = true;
      child.stdin.write('?\n');
    }
    return now - last < 750 ? current : null;
  }
  return { get, stop };
}
