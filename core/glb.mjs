// Only self-contained, uncompressed glTF 2.0 binary models. Never resolve sidecars or URLs.
export const MAX_MODEL = 24 * 1024 * 1024;
export function validateGLB(buffer) {
  const bad = (m = '不是有效的二进制 glTF 2.0 模型。') => {
    throw Error(m);
  };
  if (
    !(buffer instanceof ArrayBuffer) ||
    buffer.byteLength < 28 ||
    buffer.byteLength > MAX_MODEL
  )
    bad();
  const d = new DataView(buffer);
  const u = (i) => d.getUint32(i, true);
  if (
    u(0) !== 0x46546c67 ||
    u(4) !== 2 ||
    u(8) !== buffer.byteLength ||
    u(16) !== 0x4e4f534a
  )
    bad();
  const len = u(12),
    end = 20 + len;
  if (
    len % 4 ||
    len > 2 * 1024 * 1024 ||
    end + 8 > buffer.byteLength ||
    u(end + 4) !== 0x004e4942 ||
    end + 8 + u(end) !== buffer.byteLength
  )
    bad();
  let j;
  try {
    j = JSON.parse(new TextDecoder().decode(new Uint8Array(buffer, 20, len)));
  } catch {
    bad();
  }
  if (
    j?.asset?.version !== '2.0' ||
    !Array.isArray(j.meshes) ||
    !j.meshes.length ||
    j.buffers?.length !== 1 ||
    j.buffers[0].byteLength > u(end)
  )
    bad();
  // Reject resource-bearing extensions as well as user-provided data/file/http URIs.
  const permitted = new Set([
    'KHR_materials_unlit',
    'KHR_materials_emissive_strength',
    'KHR_texture_transform',
    'KHR_mesh_quantization',
  ]);
  const stack = [j];
  while (stack.length) {
    const o = stack.pop();
    if (!o || typeof o !== 'object') continue;
    if (Object.hasOwn(o, 'uri') || Object.hasOwn(o, 'url'))
      bad('仅接受完全内嵌的 GLB，不允许外部资源。');
    if (
      o.extensions &&
      Object.keys(o.extensions).some((k) => !permitted.has(k))
    )
      bad('该模型含暂不支持的压缩或材质扩展。请导出为基础 GLB。');
    for (const v of Object.values(o))
      if (v && typeof v === 'object') stack.push(v);
  }
  if ((j.extensionsRequired || []).some((k) => !permitted.has(k)))
    bad('模型依赖未支持的扩展。');
  if (
    (j.images || []).some(
      (i) =>
        !Number.isInteger(i.bufferView) ||
        !['image/png', 'image/jpeg'].includes(i.mimeType),
    )
  )
    bad('纹理必须是内嵌的 PNG 或 JPEG。');
  if (
    (j.nodes || []).length > 2000 ||
    (j.animations || []).length > 64 ||
    (j.accessors || []).reduce((a, b) => a + b.count, 0) > 4e6
  )
    bad('模型过于复杂，请使用轻量桌宠模型。');
  if (
    (j.bufferViews || []).some(
      (v) =>
        v.buffer !== 0 ||
        !Number.isSafeInteger(v.byteLength) ||
        v.byteLength < 0 ||
        (v.byteOffset || 0) < 0 ||
        (v.byteOffset || 0) + v.byteLength > u(end),
    )
  )
    bad();
  return j;
}
