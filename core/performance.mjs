import { allowed, isFlightName } from './repertoire.mjs';
export function performanceFor({
  track,
  held,
  offer,
  hover,
  sleeping,
  reaction,
  prefs,
}) {
  if (held) return 'held';
  if (['fall', 'land', 'fly', 'climb', 'hang'].includes(track.kind))
    return track.kind;
  if (offer) return allowed(prefs, 'notice') ? 'notice' : 'idle';
  if (sleeping) return 'sleep';
  const action = {
    eat: 'eat',
    pat: 'pat',
    play: 'play',
    read: 'read',
    tea: 'tea',
    chat: 'notice',
    time: 'notice',
    wake: 'notice',
  }[reaction?.kind];
  if (action) return allowed(prefs, action) ? action : 'idle';
  return hover && allowed(prefs, 'notice')
    ? 'notice'
    : allowed(prefs, track.kind)
      ? track.kind
      : 'idle';
}
const names = {
  walk: ['walk', 'run', '散步', '行走', '走路'],
  climb: ['climb', '攀爬'],
  hang: ['hang', 'ceiling', '悬挂'],
  held: ['drag', 'held', 'pickup', 'lift', '提起'],
  fall: ['fall', '下落'],
  land: ['land', '落地'],
  eat: ['eat', 'feed', '进食', '吃'],
  play: ['play', 'dance', '扇舞', '玩耍'],
  read: ['read', '阅读'],
  tea: ['tea', 'drink', '喝茶'],
  pat: ['pat', 'happy', 'bow', '摸头'],
  notice: ['look', 'greet', 'wave', '招手'],
  sleep: ['sleep', 'nap', '小憩'],
  idle: ['idle', 'stand', '待机', '站立'],
};
const norm = (s) => s.toLowerCase().replace(/[\s_.:-]+/gu, '');
export function matchAnimation(clips, kind, face = 1, disabled = []) {
  const c = clips
    .map((v, i) => ({ s: norm(v.name || ''), fly: isFlightName(v.name), i }))
    .filter(
      (x) =>
        (!x.fly || (kind === 'fly' && !disabled.includes('fly'))) &&
        !disabled.some((k) => (names[k] || []).some((t) => x.s.includes(t))),
    );
  const result = (v, mirror = false, directional = false) => ({
    index: v?.i ?? -1,
    directional,
    mirror,
  });
  if (kind === 'fly')
    return result(
      c.find((x) => x.fly),
      face < 0,
    );
  if (kind === 'walk') {
    const dir = (f) =>
      f < 0
        ? ['walkleft', 'leftwalk', '向左走', '左行']
        : ['walkright', 'rightwalk', '向右走', '右行'];
    const d = c.find((x) => dir(face).some((k) => x.s.includes(k)));
    if (d) return result(d, false, true);
    const rev = c.find((x) => dir(-face).some((k) => x.s.includes(k)));
    if (rev && !c.some((x) => x.s === 'walk')) return result(rev, true, true);
  }
  const terms = names[kind] || [];
  return result(
    c.find((x) => terms.includes(x.s)) ||
      c.find((x) => terms.some((k) => x.s.includes(k))) ||
      c.find((x) => names.idle.some((k) => x.s.includes(k))) ||
      c[0],
    face < 0 && ['walk', 'climb', 'hang'].includes(kind),
  );
}
