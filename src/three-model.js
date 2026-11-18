import * as T from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { validateGLB } from '../core/glb.mjs';
function load(data) {
  validateGLB(data);
  const manager = new T.LoadingManager();
  manager.setURLModifier((u) => {
    if (!u.startsWith('blob:')) throw Error('GLB 不允许外部纹理。');
    return u;
  });
  return new GLTFLoader(manager).parseAsync(data, '');
}
function dispose(scene) {
  const mats = new Set(),
    maps = new Set();
  scene.traverse((o) => {
    o.geometry?.dispose();
    for (const m of [o.material].flat()) if (m) mats.add(m);
  });
  for (const m of mats) {
    for (const x of Object.values(m)) if (x?.isTexture) maps.add(x);
    m.dispose();
  }
  for (const t of maps) {
    t.source?.data?.close?.();
    t.dispose();
  }
}
export async function checkRenderable(data) {
  const g = await load(data);
  try {
    const size = new T.Box3().setFromObject(g.scene).getSize(new T.Vector3());
    if (!Number.isFinite(size.length()) || !size.length())
      throw Error('模型中没有可显示的网格。');
  } finally {
    dispose(g.scene);
  }
}
export async function mountModel(canvas, data, signal) {
  const gltf = await load(data);
  if (signal.aborted) {
    dispose(gltf.scene);
    return;
  }
  let renderer;
  try {
    renderer = new T.WebGLRenderer({
      canvas,
      alpha: true,
      preserveDrawingBuffer: !!window.petdesk,
      antialias: true,
      powerPreference: 'low-power',
    });
  } catch (e) {
    dispose(gltf.scene);
    throw e;
  }
  const scene = new T.Scene();
  scene.add(gltf.scene, new T.HemisphereLight(0xfff6ff, 0xada9da, 2));
  const light = new T.DirectionalLight(0xfff5de, 2.5);
  light.position.set(2, 4, 3);
  scene.add(light);
  const box = new T.Box3().setFromObject(gltf.scene),
    center = box.getCenter(new T.Vector3()),
    size = box.getSize(new T.Vector3());
  const camera = new T.PerspectiveCamera(32, 1, 0.001, 10000);
  const mixer = new T.AnimationMixer(gltf.scene);
  const clip =
    gltf.animations.find((a) => /idle|stand|待机/i.test(a.name)) ??
    gltf.animations[0];
  if (clip) mixer.clipAction(clip).play();
  renderer.setClearColor(0x000000, 0);
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 1.75));
  function resize() {
    const w = Math.max(1, canvas.clientWidth),
      h = Math.max(1, canvas.clientHeight);
    renderer.setSize(w, h, false);
    camera.aspect = w / h;
    const dist =
      (Math.max(size.y, size.x / camera.aspect, 0.001) * 0.57) /
        Math.tan((16 * Math.PI) / 180) +
      size.z * 0.5;
    camera.near = Math.max(0.0001, dist / 100);
    camera.far = dist * 20;
    camera.position.set(center.x, center.y + 0.03 * size.y, center.z + dist);
    camera.lookAt(center);
    camera.updateProjectionMatrix();
  }
  resize();
  const observer = new ResizeObserver(resize);
  observer.observe(canvas);
  let raf,
    prev = 0;
  function frame(t) {
    raf = requestAnimationFrame(frame);
    if (document.hidden || t - prev < 1000 / 30) return;
    mixer.update(Math.min(0.1, (t - prev) / 1000));
    prev = t;
    renderer.render(scene, camera);
  }
  raf = requestAnimationFrame(frame);
  signal.addEventListener(
    'abort',
    () => {
      cancelAnimationFrame(raf);
      observer.disconnect();
      mixer.stopAllAction();
      mixer.uncacheRoot(gltf.scene);
      dispose(gltf.scene);
      renderer.dispose();
      renderer.forceContextLoss();
    },
    { once: true },
  );
}
