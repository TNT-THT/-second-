import path from 'node:path';
import fs from 'node:fs/promises';
import { foodHint } from '../core/state.mjs';
export function within(root, candidate) {
  const r = path.relative(root, candidate);
  return (
    r === '' ||
    (r !== '..' && !r.startsWith('..' + path.sep) && !path.isAbsolute(r))
  );
}
export async function inspectFood(input, { roots, blocked = [] }) {
  if (
    typeof input !== 'string' ||
    !path.isAbsolute(input) ||
    input.includes('\0') ||
    /^[\\/]{2}/.test(input) ||
    /[\u0001-\u001f\u007f\u202a-\u202e\u2066-\u2069]/u.test(input)
  )
    throw Error('请拖入一个本机普通文件。');
  const name = path.normalize(input);
  if (
    name
      .slice(path.parse(name).root.length)
      .split(path.sep)
      .some(
        (s) =>
          s.startsWith('.') ||
          s.includes(':') ||
          /\.(app|bundle|framework|pkg)$/i.test(s),
      )
  )
    throw Error('隐藏目录、应用包或特殊路径里的文件不能投喂。');
  if (
    !roots.some((r) => r && within(r, name)) ||
    blocked.some((r) => r && within(r, name))
  )
    throw Error('为保护电脑，只接受桌面、下载或文档目录中的非应用文件。');
  let info, real;
  try {
    info = await fs.lstat(name);
    real = await fs.realpath(name);
  } catch {
    throw Error('文件已移动或无法读取，未进行投喂。');
  }
  if (
    !info.isFile() ||
    info.isSymbolicLink() ||
    info.nlink !== 1 ||
    path.relative(real, name) !== ''
  )
    throw Error('不能投喂文件夹、链接或重定向路径。');
  const issue = foodHint(path.basename(name), info.size);
  if (issue) throw Error(issue);
  return {
    path: name,
    name: path.basename(name),
    size: info.size,
    stamp: [info.dev, info.ino, info.size, info.mtimeMs, info.ctimeMs].join(
      ':',
    ),
  };
}
// This is the only feeding code that changes the filesystem. Never fall back to unlink.
export async function feedOne(input, policy, { confirm, trash }) {
  const before = await inspectFood(input, policy);
  if (!(await confirm(before))) return { ok: false, cancelled: true };
  const again = await inspectFood(input, policy);
  if (again.stamp !== before.stamp)
    throw Error('确认期间文件发生了变化，已取消。');
  try {
    await trash(again.path);
  } catch {
    throw Error('系统回收站不可用或文件被占用；不会改用永久删除。');
  }
  return { ok: true };
}
