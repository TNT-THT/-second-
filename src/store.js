import { useSyncExternalStore } from 'react';
import { initialState, restoreState, reduceState } from '../core/state.mjs';
export const api = window.petdesk;
export const RELEASE =
  'https://github.com/TNT-THT/-second-/releases/tag/petdesk-v1.1.0';
const key = 'petdesk.studio.v1',
  listeners = new Set();
let saved;
if (!api) {
  try {
    saved = JSON.parse(localStorage.getItem(key));
  } catch {}
}
let data = {
  state: saved ? restoreState(saved) : initialState(),
  petOpen: false,
};
function publish(v) {
  if (!v?.state) return;
  data = v;
  for (const f of listeners) f();
}
if (api) {
  api.onState(publish);
  api.snapshot().then(publish).catch(console.error);
} else
  window.addEventListener('storage', (e) => {
    if (e.key !== key) return;
    try {
      publish({ ...data, state: restoreState(JSON.parse(e.newValue)) });
    } catch {}
  });
export const useStudio = () =>
  useSyncExternalStore(
    (f) => {
      listeners.add(f);
      return () => listeners.delete(f);
    },
    () => data,
  );
export async function act(a) {
  if (api) {
    return api.act(a);
  }
  const state = reduceState(data.state, a);
  if (state !== data.state) {
    publish({ ...data, state });
    try {
      localStorage.setItem(key, JSON.stringify(state));
    } catch {}
  }
}
export async function setPet(on) {
  if (api) return api.setPet(on);
  publish({ ...data, petOpen: on });
}
export const pref = (key, value) => act({ type: 'pref', key, value });
export function openRelease() {
  if (api) return api.utility('release');
  window.open(RELEASE, '_blank', 'noopener,noreferrer');
}
let ctx;
export function chirp(t = 'pat') {
  if (!data.state.prefs.sound) return;
  try {
    ctx ??= new AudioContext();
    if (ctx.state === 'suspended') void ctx.resume();
    const now = ctx.currentTime;
    const notes =
      t === 'nap' ? [523, 392] : t === 'play' ? [523, 659, 784] : [660, 880];
    notes.forEach((f, i) => {
      const o = ctx.createOscillator(),
        g = ctx.createGain();
      o.type = 'sine';
      o.frequency.value = f;
      const t0 = now + i * 0.08;
      g.gain.setValueAtTime(0.0001, t0);
      g.gain.exponentialRampToValueAtTime(0.045, t0 + 0.012);
      g.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.14);
      o.connect(g).connect(ctx.destination);
      o.start(t0);
      o.stop(t0 + 0.15);
    });
  } catch {}
}
