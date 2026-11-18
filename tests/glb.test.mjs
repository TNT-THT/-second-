import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { validateGLB } from '../core/glb.mjs';
const original = fs.readFileSync(
    new URL('../public/models/Mint.glb', import.meta.url),
  ),
  ab = (b) => Uint8Array.from(b).buffer;
function modified(edit) {
  const start = 20 + original.readUInt32LE(12),
    j = JSON.parse(original.subarray(20, start).toString());
  edit(j);
  const json = Buffer.from(JSON.stringify(j));
  const l = (json.length + 3) & ~3,
    head = Buffer.from(original.subarray(0, 20));
  head.writeUInt32LE(20 + l + original.length - start, 8);
  head.writeUInt32LE(l, 12);
  return ab(
    Buffer.concat([
      head,
      json,
      Buffer.alloc(l - json.length, 32),
      original.subarray(start),
    ]),
  );
}
test('bundled sample is a real animated GLB', () => {
  const j = validateGLB(ab(original));
  assert.ok(j.meshes.length);
  assert.equal(j.animations[0].name, 'Idle');
});
test('refuses external textures, buffers, compression and out-of-bounds views', () => {
  for (const edit of [
    (j) => (j.images = [{ uri: 'https://example.invalid/a.png' }]),
    (j) => (j.buffers[0].uri = 'file:///outside.bin'),
    (j) => (j.meshes[0].extensions = { KHR_draco_mesh_compression: {} }),
    (j) => (j.bufferViews[0].byteLength = 2 ** 30),
  ])
    assert.throws(() => validateGLB(modified(edit)));
});
test('rejects truncation, bad magic, malformed length', () => {
  assert.throws(() => validateGLB(ab(original.subarray(0, 60))));
  assert.throws(() => validateGLB(new ArrayBuffer(0)));
  const b = Buffer.from(original);
  b.writeUInt32LE(1, 0);
  assert.throws(() => validateGLB(ab(b)));
});
