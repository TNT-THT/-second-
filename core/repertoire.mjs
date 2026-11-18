// One catalog for the rig, behavior permissions, audition and model picker. Physical states
// (especially falling/landing) cannot be switched off as a way of suspending gravity.
export const BUILTINS = Object.freeze([
  {
    id: 'qwen',
    name: 'Qwen 娘',
    kind: 'illustration',
    edition: '花庭蓝',
    code: 'AZURE',
    tint: '#607bc3',
  },
  {
    id: 'qwen-lilac',
    name: 'Qwen 娘 · 藤铃',
    kind: 'illustration',
    edition: '藤铃紫',
    code: 'WISTERIA',
    tint: '#9172b8',
  },
]);
export const builtin = (id) => BUILTINS.find((m) => m.id === id);
export const clips = Object.freeze(
  Object.fromEntries(
    [
      ['idle', '执扇待机', 3.6, '呼吸、眨眼，发辫随着手中的小扇轻晃。'],
      ['walk', '踏步散步', 0.84, '朝行进方向交替落脚，在地面和窗沿上行走。'],
      ['climb', '贴边攀爬', 1.05, '交替伸手和蹬腿，攀上屏幕侧边。', 1],
      ['hang', '悬挂横移', 1.1, '抓住顶部的小绸环，换手横移；这不是飞行。', 1],
      [
        'fly',
        '乘扇飞行',
        1.65,
        '乘着星纹大扇飞行；只有开启飞行模式才可在空中巡游。',
        1,
      ],
      ['held', '拎起晃晃', 1.3, '提起时张开小手，鞋尖和发辫跟着晃动。'],
      [
        'fall',
        '空中下落',
        0.68,
        '松手后张开手臂找平衡，加速下落直到碰到支撑面。',
      ],
      ['land', '落地缓冲', 0.43, '脚尖触地后屈膝压低，再回弹站稳。'],
      ['notice', '招手问候', 1.3, '看到鼠标靠近，抬眼挥一挥小手。', 1],
      ['pat', '摸头回礼', 1.2, '眯起眼睛，藏在小帽檐下微笑。', 1],
      ['eat', '星星点心', 1.5, '双手捧着饼干，小口咬下；试映不会处理文件。', 1],
      ['play', '花间扇舞', 1.45, '踮脚、展扇，再轻轻换到另一只脚。', 1],
      ['read', '翻翻绘本', 2.4, '打开口袋绘本，低头读一页，再翻到下一页。', 1],
      ['tea', '桂花茶歇', 2.25, '端起小茶盏，吹一吹热气，再抿上一口。', 1],
      ['sleep', '抱扇小憩', 3.1, '合眼打盹，呼吸放慢，停止自主巡游。'],
    ].map(([key, label, seconds, desc, optional]) => [
      key,
      { key, label, seconds, desc, optional: !!optional },
    ]),
  ),
);
export const leisureKeys = ['play', 'read', 'tea'];
export const allowed = (prefs, k) => !prefs?.disabledClips?.includes(k);
export const canFly = (m) =>
  !!builtin(m?.id) || (m?.kind === 'glb' && m.flyClip === true);
export const flightActive = (s) =>
  !!(
    s.prefs.flight &&
    !s.sleeping &&
    canFly(s.model) &&
    allowed(s.prefs, 'fly')
  );
export function motionOptions(s, now = Date.now()) {
  const needsGround =
    s.sleeping ||
    (s.reaction?.until > now &&
      ['eat', 'read', 'tea', 'play'].includes(s.reaction.kind));
  return {
    roam: s.prefs.roam,
    edges: s.prefs.edges && !needsGround,
    flight: flightActive(s) && !needsGround,
    disabled: s.prefs.disabledClips,
    leisure: s.prefs.leisure,
  };
}
export const isFlightName = (s) =>
  typeof s === 'string' &&
  /(?:^|[^a-z])(?:fly|flying|flight|hover|float|glide)(?:ing)?(?:$|[^a-z])|飞行|悬浮|滑翔/i.test(
    s.replace(/([a-z])([A-Z])/g, '$1 $2'),
  );
export const hasFlightClip = (animations) =>
  (animations || []).some((c) => isFlightName(c.name));
