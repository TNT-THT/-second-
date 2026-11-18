// Creative pose selection is independent of the file/permission boundary. In particular,
// drag-over can only select 'notice'; a meal animation requires a committed meal reaction.
export function performanceFor({
  track,
  held,
  landing,
  offer,
  hover,
  sleeping,
  reaction,
}) {
  if (held) return 'held';
  if (offer) return 'notice';
  if (reaction?.kind === 'eat') return 'eat';
  if (sleeping) return 'sleep';
  if (landing) return 'land';
  if (reaction) {
    return (
      { pat: 'pat', play: 'play', wake: 'notice', time: 'notice' }[
        reaction.kind
      ] || 'idle'
    );
  }
  if (track.kind === 'climb' || track.kind === 'glide') return track.kind;
  return hover ? 'notice' : track.kind;
}
const names = {
  walk: ['walk', 'walking', 'run', '散步', '行走', '走路'],
  climb: ['climb', 'climbing', '攀爬', '攀边'],
  glide: ['glide', 'hang', 'ceiling', 'float', 'fly', '顶端', '滑翔'],
  held: ['drag', 'held', 'pickup', 'lift', '拖拽', '提起'],
  eat: ['eat', 'eating', 'feed', '进食', '吃'],
  play: ['play', 'dance', 'fan', '扇舞', '玩耍'],
  pat: ['pat', 'happy', 'bow', '摸头', '鞠躬'],
  notice: ['look', 'greet', 'wave', '招手'],
  sleep: ['sleep', 'nap', 'rest', '小憩', '睡觉'],
  land: ['land', 'landing', '落地'],
  idle: ['idle', 'stand', '待机', '站立'],
};
const norm = (s) => s.toLowerCase().replace(/[\s_.:-]+/gu, '');
export function matchAnimation(clips, kind, face = 1) {
  const c = clips.map((v, i) => ({ s: norm(v.name || ''), i }));
  const terms = names[kind] || [];
  if (kind === 'walk') {
    const directed =
      face < 0
        ? ['walkleft', 'leftwalk', '向左走', '左行']
        : ['walkright', 'rightwalk', '向右走', '右行'];
    const d = c.find((x) => directed.some((k) => x.s.includes(k)));
    if (d) return { index: d.i, directional: true };
  }
  const m =
    c.find((x) => terms.includes(x.s)) ||
    c.find((x) => terms.some((k) => x.s.includes(k))) ||
    c.find((x) => names.idle.some((k) => x.s.includes(k))) ||
    c[0];
  return { index: m?.i ?? -1, directional: false };
}
