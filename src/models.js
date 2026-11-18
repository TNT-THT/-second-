import { hasFlightClip } from '../core/repertoire.mjs';
import { MAX_MODEL, validateGLB } from '../core/glb.mjs';
let dbPromise;
function db() {
  return (dbPromise ??= new Promise((resolve, reject) => {
    const r = indexedDB.open('petdesk-models', 1);
    r.onupgradeneeded = () =>
      r.result.createObjectStore('models', { keyPath: 'id' });
    r.onsuccess = () => resolve(r.result);
    r.onerror = () => reject(Error('本地模型库不可用。'));
  }));
}
async function request(mode, run) {
  const d = await db();
  return new Promise((resolve, reject) => {
    const tx = d.transaction('models', mode);
    const r = run(tx.objectStore('models'));
    tx.oncomplete = () => resolve(r.result);
    tx.onerror = tx.onabort = () =>
      reject(Error('模型库写入失败或存储空间不足。'));
  });
}
async function migrate(r) {
  if (r?.kind === 'glb' && typeof r.flyClip !== 'boolean') {
    try {
      r.flyClip = hasFlightClip(
        validateGLB(await r.blob.arrayBuffer()).animations,
      );
    } catch {
      r.flyClip = false;
    }
    await request('readwrite', (s) => s.put(r));
  }
  return r;
}
export const getModel = async (id) =>
  migrate(await request('readonly', (s) => s.get(id)));
export const listModels = async () =>
  Promise.all((await request('readonly', (s) => s.getAll())).map(migrate));
export const removeModel = (id) => request('readwrite', (s) => s.delete(id));
export async function importModel(file) {
  if (!file || file.size < 1 || file.size > MAX_MODEL)
    throw Error('模型需大于 0 字节且不超过 24 MB。');
  let thumb = null,
    flyClip = false,
    kind = 'image',
    blob,
    ext = file.name.split('.').pop().toLowerCase();
  const a = await file.arrayBuffer(),
    b = new Uint8Array(a),
    str = (i, n) => String.fromCharCode(...b.slice(i, i + n));
  if (ext === 'glb') {
    flyClip = hasFlightClip(validateGLB(a).animations);
    kind = 'glb';
    blob = new Blob([a], { type: 'model/gltf-binary' });
    // Verify that Three can actually parse it before replacing the current companion.
    const { checkRenderable } = await import('./three-model.js');
    thumb = await checkRenderable(a);
  } else {
    const mime =
      ext === 'png' && b[0] === 137 && str(1, 3) === 'PNG'
        ? 'image/png'
        : ext === 'gif' && /^GIF8[79]a$/.test(str(0, 6))
          ? 'image/gif'
          : ext === 'webp' && str(0, 4) === 'RIFF' && str(8, 4) === 'WEBP'
            ? 'image/webp'
            : '';
    if (!mime) throw Error('仅支持 PNG、GIF、WebP 或内嵌资源的 GLB 模型。');
    blob = new Blob([a], { type: mime });
    const src = URL.createObjectURL(blob);
    try {
      const img = new Image();
      img.src = src;
      await img.decode();
      if (img.naturalWidth * img.naturalHeight > 16777216) throw Error();
    } catch {
      throw Error('图片无法解码或像素尺寸过大（最多 1600 万像素）。');
    } finally {
      URL.revokeObjectURL(src);
    }
  }
  if ((await listModels()).length >= 20)
    throw Error('模型库最多保存 20 个导入模型，请先移除不再使用的模型。');
  const record = {
    id: 'local-' + crypto.randomUUID(),
    name:
      file.name
        .replace(/\.[^.]+$/, '')
        .replace(/[\u0000-\u001f\u007f\u202a-\u202e\u2066-\u2069]/g, '')
        .trim()
        .slice(0, 40) || '新伙伴',
    kind,
    flyClip,
    thumb,
    ext: ext.toUpperCase(),
    size: blob.size,
    blob,
    at: Date.now(),
  };
  await request('readwrite', (s) => s.put(record));
  return record;
}
