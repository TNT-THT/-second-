import * as T from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { matchAnimation } from '../core/performance.mjs';
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
    // Render the imported model once for the card/profile, never screen contents.
    let renderer;
    try {
      const canvas = document.createElement('canvas');
      renderer = new T.WebGLRenderer({ canvas, alpha: true, antialias: true });
      renderer.setSize(208, 208, false);
      const scene = new T.Scene(),
        box = new T.Box3().setFromObject(g.scene),
        c = box.getCenter(new T.Vector3()),
        d = Math.max(size.x, size.y) * 2.05 + size.z * 0.5;
      const cam = new T.PerspectiveCamera(
        32,
        1,
        Math.max(0.0001, d / 200),
        d * 10,
      );
      cam.position.set(c.x, c.y, c.z + d);
      cam.lookAt(c);
      scene.add(g.scene, new T.HemisphereLight(0xfff7ff, 0xd2c8e3, 2.5));
      const l = new T.DirectionalLight(0xfff5df, 2);
      l.position.set(2, 4, 3);
      scene.add(l);
      renderer.render(scene, cam);
      return await new Promise((resolve) =>
        canvas.toBlob(resolve, 'image/webp', 0.9),
      );
    } catch {
      return null;
    } finally {
      renderer?.dispose();
      renderer?.forceContextLoss();
    }
  } finally {
    dispose(g.scene);
  }
}
export async function mountModel(
  canvas,
  data,
  signal,
  getPose = () => ({ kind: 'idle', face: 1 }),
) {
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
      preserveDrawingBuffer: false,
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
  let prevPose = '',
    previousAction;
  function syncClip() {
    const { kind, face, disabled = [] } = getPose(),
      key = `${kind}/${face}/${disabled.join(',')}`;
    if (prevPose === key) return;
    prevPose = key;
    const { index, mirror } = matchAnimation(
      gltf.animations,
      kind,
      face,
      disabled,
    );
    canvas.style.transform = mirror ? 'scaleX(-1)' : '';
    const clip = gltf.animations[index];
    if (!clip) {
      previousAction?.stop();
      previousAction = null;
      return;
    }
    const next = mixer.clipAction(clip);
    if (previousAction === next) return;
    next
      .reset()
      .setEffectiveTimeScale(1)
      .setEffectiveWeight(1)
      .fadeIn(0.14)
      .play();
    previousAction?.fadeOut(0.14);
    previousAction = next;
  }
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
    syncClip();
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
