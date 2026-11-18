// Pocket Wisteria · original chibi puppet. Every stroke is drawn in one 232 × 252 design space.
// The portrait and every performance share these exact shapes, including the eyes and hand props.
const W = 232,
  H = 252,
  TAU = Math.PI * 2;
const r = (n) => Math.round(n * 100) / 100,
  tr = (x, y) => `translate(${r(x)} ${r(y)})`,
  rot = (a, x, y) => `rotate(${r(a)} ${x} ${y})`,
  sc = (x, y) => `scale(${r(x)} ${r(y)})`;
const about = (x, y, sx, sy) => `${tr(x, y)} ${sc(sx, sy)} ${tr(-x, -y)}`;
const off = '!';
export function poseAt(k = 'idle', p = 0, face = 1) {
  const a = p * TAU,
    s = Math.sin(a),
    c = Math.cos(a),
    bounce = Math.sin(a * 2),
    trail = Math.sin(a - 0.62),
    blink = p > 0.93 && p < 0.98;
  let x = 0,
    y = 0,
    roll = 0,
    sx = 1,
    sy = 1,
    hx = 0,
    hy = 0,
    head = -2 + 1.5 * s,
    look = 0,
    eyes = 'open',
    mouth = 'smile',
    ls = 7 + 2 * s,
    rs = -4 - 3 * c,
    lf = 5,
    rf = -4,
    ll = 0,
    rl = 0,
    ly = 0,
    ry = 0,
    lsX = 0,
    rsX = 0,
    skirt = 1.3 * trail,
    hair = 2.3 * trail,
    handfan = true,
    item = '',
    itemY = 0,
    mirror = face < 0;
  const leg = (u) => {
    u = (u + 1) % 1;
    return u < 0.5
      ? [13 - 52 * u, 0]
      : [-13 * Math.cos((u - 0.5) * TAU), -9 * Math.sin((u - 0.5) * TAU)];
  };
  switch (k) {
    case 'idle':
      hy = -1.2 * (1 - c);
      break;
    case 'notice':
      head = -8 + 3 * s;
      rs = -91 - 13 * s;
      rf = -16;
      look = 2;
      mouth = 'happy';
      break;
    case 'walk': {
      [lsX, ly] = leg(p);
      [rsX, ry] = leg(p + 0.5);
      y = -1.5 * (1 - Math.cos(2 * a));
      roll = 2 + 1.3 * s;
      look = 4;
      hx = 3;
      ls = 10 + 21 * c;
      rs = -6 - 25 * c;
      ll = 4 * c;
      rl = -4 * c;
      skirt = 4 * trail;
      hair = -4 - 3 * trail;
      break;
    }
    case 'climb':
      x = 29;
      y = 0;
      roll = 0;
      head = -5;
      hx = -3;
      look = 4;
      hair = 22 + 3 * trail;
      ls = -87 - 18 * s;
      rs = -104 + 21 * s;
      lf = -32;
      rf = -18;
      ll = -14 - 20 * c;
      rl = 24 + 23 * c;
      ly = -10 * (1 - c);
      ry = -10 * (1 + c);
      handfan = false;
      break;
    case 'hang':
      y = 0;
      head = 3 * s;
      ls = 142 - 13 * c;
      rs = -142 - 13 * c;
      lf = 16;
      rf = -16;
      handfan = false;
      roll = 0;
      ll = 10 + 9 * trail;
      rl = -8 - 9 * trail;
      item = 'ribbons';
      break;
    case 'fly':
      sx = sy = 0.91;
      y = -13 + 2 * c;
      roll = -6 + 2 * s;
      head = 4 + 2 * s;
      ls = 40 + 5 * trail;
      rs = -46 - 8 * s;
      ly = -7;
      ry = -14;
      ll = 21 + 8 * s;
      rl = -22 + 5 * trail;
      look = 3;
      hair = -12 - 6 * trail;
      skirt = -5 - 3 * s;
      handfan = false;
      item = 'sail';
      break;
    case 'held':
      sx = sy = 0.96;
      y = -6;
      roll = 3 * s;
      ls = 48 + 9 * s;
      rs = -68 + 8 * s;
      ly = -4;
      ry = -7;
      ll = 15 + 12 * trail;
      rl = -14 - 10 * s;
      mouth = 'o';
      handfan = false;
      break;
    case 'fall':
      sx = sy = 0.98;
      x = -3;
      y = -6;
      roll = -3 * s;
      ls = 80 + 7 * s;
      rs = -83 + 8 * c;
      lf = 10;
      rf = -8;
      head = -4;
      hair = -11 + 6 * s;
      ll = 12 + 9 * s;
      rl = -14 - 7 * c;
      mouth = 'o';
      handfan = false;
      break;
    case 'land': {
      const q = Math.sin(Math.PI * p);
      sx = 1 + 0.095 * q;
      sy = 1 - 0.16 * q;
      ls = 27 + 34 * q;
      rs = -24 - 32 * q;
      head = 4 * q;
      eyes = 'closed';
      handfan = false;
      break;
    }
    case 'pat':
      eyes = 'closed';
      mouth = 'happy';
      head = -8 + 4 * c;
      sy = 0.982 + 0.018 * c;
      ls = -18;
      rs = 16;
      break;
    case 'eat':
      eyes = p < 0.52 ? 'closed' : 'open';
      mouth = p < 0.52 ? 'o' : 'happy';
      head = 3 + 4 * c;
      ls = -33;
      rs = 33;
      lf = -30;
      rf = 30;
      handfan = false;
      item = 'cookie';
      itemY = -5 - 11 * c;
      break;
    case 'play':
      roll = 6 * s;
      head = -5 * s;
      ls = 40 + 28 * s;
      lf = 15 + 12 * c;
      rs = -48 + 22 * c;
      ll = 8 * s;
      rl = -14 * (1 - c);
      ry = -8 * (1 - c);
      y = -2 * (1 - c);
      hair = 5 * trail;
      mouth = 'happy';
      break;
    case 'read':
      head = 5 + 2 * c;
      hy = 3;
      look = -1 + s * 2;
      ls = -31;
      rs = 31;
      handfan = false;
      item = 'book';
      itemY = 1.2 * s;
      break;
    case 'tea':
      eyes = p > 0.25 && p < 0.68 ? 'closed' : 'open';
      mouth = 'o';
      head = -4 + 3 * s;
      ls = -32;
      rs = 34;
      handfan = false;
      item = 'cup';
      itemY = -4 - 7 * Math.sin(Math.PI * p) ** 4;
      break;
    case 'sleep':
      eyes = 'closed';
      mouth = 'o';
      head = 10 + 2 * s;
      hy = 3 + 2 * c;
      sy = 0.96 + 0.008 * s;
      ls = -15;
      rs = 18;
      break;
  }
  // Map the shoulder-to-palm axis to the prop without stretching sleeve WIDTH.
  function aim(x, hand, target) {
    const dx = hand - x,
      dy = 37,
      L = Math.hypot(dx, dy),
      nx = target[0] - x,
      ny = target[1] - 158,
      N = Math.hypot(nx, ny),
      k = N / L,
      u = dx / L,
      v = dy / L,
      a = nx / N,
      b = ny / N;
    const A = k * a * u + b * v,
      B = k * b * u - a * v,
      C = k * a * v - b * u,
      D = k * b * v + a * u;
    return `matrix(${[A, B, C, D, x - A * x - C * 158, 158 - B * x - D * 158].map(r).join(' ')})`;
  }
  let leftTarget, rightTarget;
  if (k === 'eat') {
    leftTarget = [105, 171 + itemY];
    rightTarget = [127, 171 + itemY];
  }
  if (k === 'tea') {
    leftTarget = [101, 181 + itemY];
    rightTarget = [133, 179 + itemY];
  }
  if (k === 'read') {
    leftTarget = [88, 184 + itemY];
    rightTarget = [144, 185 + itemY];
  }
  if (k === 'hang') {
    leftTarget = [72, 128 + 7 * s];
    rightTarget = [159, 128 - 7 * s];
  }
  if (k === 'climb') {
    leftTarget = [130, 151 - 9 * s];
    rightTarget = [178, 139 + 11 * s];
  }
  if (leftTarget) {
    lf = 0;
    rf = 0;
  }
  const larm = leftTarget ? aim(92, 81, leftTarget) : rot(ls, 92, 158),
    rarm = rightTarget ? aim(140, 151, rightTarget) : rot(rs, 140, 158);
  return {
    mirror: mirror ? 'translate(232 0) scale(-1 1)' : '',
    body: `${tr(x, y)} ${about(116, 249, sx, sy)} ${rot(roll, 116, 207)}`,
    head: `${tr(hx, hy)} ${rot(head, 116, 143)}`,
    hair: rot(hair, 112, 92),
    braid: rot(-hair * 0.9, 61, 104),
    fringe: rot(hair * 0.14, 108, 46),
    gaze: tr(look, 0.4 * c),
    open: eyes === 'open' && !blink ? '' : off,
    closed: eyes === 'closed' || blink ? '' : off,
    smile: mouth === 'smile' ? '' : off,
    happy: mouth === 'happy' ? '' : off,
    oh: mouth === 'o' ? '' : off,
    skirt: rot(skirt, 116, 181),
    leftArm: larm,
    rightArm: rarm,
    leftGrip: larm,
    rightGrip: rarm,
    gripL: rot(lf, 84, 183),
    gripR: rot(rf, 148, 183),
    strapL: sc(1, (128 + 7 * s) / 128),
    strapR: sc(1, (128 - 7 * s) / 128),
    hook: k === 'climb' ? tr(0, 11 * s) : off,
    leftWrist: rot(lf, 84, 183),
    rightWrist: rot(rf, 148, 183),
    leftLeg: `${tr(lsX, ly)} ${rot(ll, 102, 209)}`,
    rightLeg: `${tr(rsX, ry)} ${rot(rl, 131, 209)}`,
    fan: handfan ? rot(3 * s, 83, 194) : off,
    book: item === 'book' ? tr(0, itemY) : off,
    page: about(115, 175, 0.22 + (0.72 * (1 + c)) / 2, 1),
    cup: item === 'cup' ? tr(0, itemY) : off,
    steam: tr(s, -2 * (1 - c)),
    cookie: item === 'cookie' ? tr(0, itemY) : off,
    sail: item === 'sail' ? rot(3 * s, 117, 221) : off,
    ribbons: item === 'ribbons' ? '' : off,
    stars: item === 'sail' ? tr(1.3 * s, 1.7 * c) : off,
  };
}
export function drawChibi({
  skin = 'qwen',
  kind = 'idle',
  phase = 0,
  face = 1,
  portrait = false,
  uid = 'q',
} = {}) {
  const lilac = skin === 'qwen-lilac',
    P = uid.replace(/[^a-zA-Z0-9_-]/g, '') + '-',
    v = poseAt(kind, phase, face),
    url = (n) => `url(#${P + n})`;
  const ink = '#40354f',
    skinTone = url('skin'),
    hair = url('hair'),
    deep = url('deep'),
    navy = url('silk'),
    pale = url('cream'),
    accent = lilac ? '#a08cc7' : '#86abd9',
    gold = '#e3bd79';
  const attr = (n) =>
    v[n] === off ? 'display="none"' : v[n] ? `transform="${v[n]}"` : '';
  const j = (n, content) => `<g data-j="${n}" ${attr(n)}>${content}</g>`,
    g = (s, t) => `<g transform="${s}">${t}</g>`;
  const path = (d, fill, stroke = ink, w = 1.85) =>
      `<path d="${d}" fill="${fill}" stroke="${stroke}" stroke-width="${w}"/>`,
    flat = (d, f) => path(d, f, 'none', 0),
    line = (d, col, w = 1.6) => path(d, 'none', col, w),
    ell = (cx, cy, rx, ry, f) =>
      `<ellipse cx="${cx}" cy="${cy}" rx="${rx}" ry="${ry}" fill="${f}"/>`;
  const star = (x, y, z, f) =>
    g(
      `translate(${x} ${y}) scale(${z})`,
      flat('M0 -7 Q1 -1 6 0 Q1 1 0 7 Q-1 1 -6 0 Q-1 -1 0 -7Z', f),
    );
  const flower = (x, y, z, petal = '#fff4e0') =>
    g(
      `translate(${x} ${y}) scale(${z})`,
      Array.from({ length: 5 }, (_, i) =>
        g(
          'rotate(' + i * 72 + ')',
          path(
            'M0 1 C-8 -2 -7 -9 -3 -10 Q0 -12 2 -9 C7 -10 9 -4 0 1Z',
            petal,
            '#ad897e',
            0.85,
          ),
        ),
      ).join('') + ell(0, 0, 2.5, 2.5, gold),
    );
  const grad = (id, colors, diag = false) =>
    `<linearGradient id="${P + id}" x1="0" y1="0" x2="${diag ? 1 : 0}" y2="1" gradientUnits="objectBoundingBox">${colors.map(([t, c]) => `<stop offset="${t}" stop-color="${c}"/>`).join('')}</linearGradient>`;
  const defs =
    grad(
      'hair',
      [
        [0, lilac ? '#beb9ec' : '#b9d5f5'],
        [0.37, lilac ? '#999dd9' : '#86abe1'],
        [0.61, lilac ? '#8084bd' : '#728bc7'],
        [1, lilac ? '#6c649e' : '#6266a2'],
      ],
      true,
    ) +
    grad(
      'deep',
      [
        [0, '#586891'],
        [0.6, lilac ? '#726a9f' : '#6c82b5'],
        [1, '#a3a3d3'],
      ],
      true,
    ) +
    grad(
      'silk',
      [
        [0, lilac ? '#9987ba' : '#657aab'],
        [0.55, lilac ? '#6b598c' : '#3c527e'],
        [1, lilac ? '#55486b' : '#303955'],
      ],
      true,
    ) +
    grad('cream', [
      [0, '#fffbee'],
      [0.65, '#f8edda'],
      [1, '#e9d5ba'],
    ]) +
    grad('skin', [
      [0, '#ffe3cc'],
      [0.46, '#fff0df'],
      [1, '#f9d4c1'],
    ]) +
    grad('iris', [
      [0, '#38375e'],
      [0.37, '#68629c'],
      [0.72, lilac ? '#ad8acf' : '#959fdf'],
      [1, '#b8e9f3'],
    ]) +
    grad('paper', [
      [0, '#fff6dc'],
      [1, lilac ? '#cbb9de' : '#b2cde3'],
    ]);
  const bow = (x, y, z, col = accent) =>
    g(
      `translate(${x} ${y}) scale(${z})`,
      path(
        'M-1 0 Q-14 -11 -15 -4 L-13 6 Q-6 6 -1 2 M1 0 Q12 -10 15 -5 L14 6 Q8 7 1 2',
        col,
        ink,
        1.5,
      ) +
        path('M-4 2 L-7 14 L-2 11 L2 14 L4 2', col, ink, 1) +
        ell(0, 0, 3.4, 3.4, gold),
    );
  const fanShape = (large = false) =>
    path(
      'M0 0 L-29 -16 Q-26 -29 -16 -34 Q-10 -41 0 -40 Q9 -42 17 -35 Q28 -32 32 -19Z',
      url('paper'),
      ink,
      1.8,
    ) +
    path(
      'M-29 -16 Q-25 -25 -22 -27 Q2 -48 29 -23 L32 -19 Q6 -31 -18 -14 L-22 -12Z',
      accent,
      'none',
    ) +
    [-24, -15, -5, 6, 16, 25]
      .map((n, i) =>
        line(`M0 0 L${n} ${[-20, -29, -34, -35, -30, -22][i]}`, gold, 1.15),
      )
      .join('') +
    flower(-4, -26, 0.36) +
    ell(0, 0, 2, 2, gold) +
    (large ? star(11, -23, 0.7, 'white') : '');
  const back = j(
    'hair',
    path(
      'M51 78 C42 106 56 131 39 158 Q30 174 19 174 Q26 185 43 182 Q31 195 23 192 Q37 216 61 209 Q52 222 42 219 Q88 243 112 204 Q138 226 161 216 Q191 222 204 200 Q190 207 185 194 Q207 198 211 178 Q192 183 187 159 C179 142 194 106 173 66Z',
      deep,
    ) +
      path(
        'M58 116 Q65 158 47 174 Q46 173 46 171 Q50 190 35 197 Q64 208 78 180 Q80 200 68 208 Q99 207 107 182L112 122Z',
        hair,
        'none',
      ) +
      path(
        'M161 123 C154 153 169 190 191 190 Q171 196 162 178 Q166 199 178 205 Q150 200 146 167Z',
        hair,
        'none',
      ) +
      line(
        'M60 133 Q61 164 50 173 M47 202 Q64 201 72 185 M165 141 Q163 164 177 177 M151 188 Q153 202 166 208',
        '#acb9df',
        1.7,
      ),
  );
  const leg = (right = false) =>
    g(
      `translate(${right ? 130 : 102} 207)`,
      path('M-8 0 Q-10 13 -7 31 Q-4 36 6 32 Q10 14 8 0Z', pale) +
        path('M-7 24 Q0 27 7 24 L6 29 Q0 31 -7 28Z', accent, ink, 0.85) +
        path(
          'M-7 31 Q-1 33 3 30 Q7 24 11 30 Q16 32 15 38 Q15 43 -5 43 Q-12 42 -11 37Z',
          navy,
        ) +
        line('M-9 39 Q1 43 13 38', '#b5adba', 1.4) +
        ell(5, 32, 2.2, 1.5, gold),
    );
  const dress =
    path(
      'M93 145 Q113 140 138 147 Q155 158 147 173 L140 189 L90 189 Q81 160 93 145Z',
      pale,
    ) +
    path('M95 145 Q98 163 111 167 L103 183 L88 184 L85 158Z', navy) +
    path('M137 146 Q133 160 123 168 L131 183 L145 184 L149 159Z', navy) +
    path(
      'M108 143 L116 149 L124 142 L129 150 L117 158 L103 150Z',
      '#fcf4db',
      ink,
      1.25,
    ) +
    line('M115 160 L114 181', '#c6ad90', 1.6) +
    ell(117, 166, 1.7, 1.7, gold) +
    ell(117, 174, 1.7, 1.7, gold);
  const skirt = j(
    'skirt',
    path(
      'M92 177 Q114 185 140 177 Q149 192 166 211 Q167 215 160 216 Q159 223 151 221 Q147 227 139 224 Q133 230 126 226 Q115 232 108 226 Q97 229 91 223 Q82 226 78 219 Q68 220 66 214 Q83 190 92 177Z',
      pale,
    ) +
      path(
        'M91 177 Q104 182 111 183 Q104 204 84 218 Q75 215 68 210 Q84 188 91 177Z',
        navy,
      ) +
      path(
        'M139 177 Q148 190 166 210 Q158 218 147 221 Q127 200 123 183Z',
        navy,
      ) +
      line('M93 187 Q85 204 78 209 M139 188 Q149 204 155 211', gold, 1.7) +
      line('M110 194 L103 214 M124 194 L132 215', '#d9c6ae', 1.6) +
      line('M84 218 Q116 230 149 219', '#cbb89d', 1.25) +
      flower(146, 210, 0.42),
  );
  const belt =
    path('M92 176 Q116 182 140 175 L143 182 Q119 190 89 183Z', navy) +
    bow(116, 182, 0.76) +
    star(116, 183, 0.43, '#fffbe4');
  function sleeve(right) {
    const sx = right ? 1 : -1,
      x = right ? 140 : 92;
    return g(
      `translate(${x} 156) scale(${sx} 1)`,
      path(
        'M-5 -6 Q5 -11 12 -3 Q17 5 13 13 L16 24 Q8 30 -1 28 Q3 12 -5 6Z',
        navy,
      ) +
        path('M0 22 Q8 24 16 19 L19 26 Q13 33 -1 31Z', pale) +
        line('M2 22 Q10 23 15 19', gold, 1.25),
    );
  }
  function hand(right) {
    const x = right ? 151 : 81;
    return g(
      `translate(${x} 189)`,
      path(
        'M-6 -2 Q-10 1 -8 5 Q-6 12 0 12 Q7 11 7 5 Q11 1 7 -1 Q5 -2 4 0 Q0 -5 -6 -2Z',
        skinTone,
        ink,
        1.55,
      ),
    );
  }
  const cheek = (x) =>
    ell(x, 119, 9, 3.5, '#f3b9b3') +
    line(`M${x - 4} 119 l2 -2 m2 4 l2 -3`, '#d79898', 1);
  const eye = (x, flip) =>
    g(
      `translate(${x} 105) scale(${flip ? -1 : 1} 1)`,
      path(
        'M-14 -4 Q-8 -15 3 -13 Q13 -12 14 0 Q14 13 4 15 Q-7 17 -11 10 Q-14 5 -14 -4Z',
        '#fff9fa',
        ink,
        1.1,
      ) +
        ell(1, 1, 10.3, 13.1, url('iris')) +
        ell(0.3, -2, 5.1, 8.4, '#514978') +
        ell(-3, -6.1, 4.6, 4.4, '#fffff9') +
        ell(6, 4.5, 2.05, 2.4, '#fff') +
        line('M-5 10 Q0 14 6 10', '#d5edff', 1.85) +
        flat(
          'M-16 -6 Q-10 -15 0 -15 Q10 -16 14 -6 Q13 -4 12 -4 Q6 -13 -3 -10 Q-11 -8 -12 -1 Q-13 0 -14 1 L-14 -3 Q-20 -4 -20 -10 Q-17 -7 -16 -6Z',
          '#43364e',
        ),
    );
  const open = j('open', j('gaze', eye(91, false) + eye(141, true))),
    closed = j(
      'closed',
      line('M77 110 Q87 101 100 109 M129 109 Q141 100 153 108', ink, 3.7) +
        line('M78 109 l-3 -2 M151 107 l4 -2', ink, 2),
    );
  const faceArt =
    path(
      'M55 85 Q54 51 111 45 Q166 44 177 83 L171 114 Q164 139 139 145 Q119 153 95 145 Q66 139 59 118Z',
      skinTone,
    ) +
    ell(59, 111, 7.2, 9.5, '#f1c4b6') +
    ell(171, 110, 6.8, 9, '#f1c4b6') +
    cheek(77) +
    cheek(154) +
    line('M78 86 Q87 80 96 84 M132 84 Q143 80 151 84', '#806382', 2.3) +
    open +
    closed +
    line('M115 120 q1 1.1 2 .1', '#d4a398', 1.2) +
    j(
      'smile',
      line('M109 128 Q114 132 117 128 Q119 131 124 126', '#9a616a', 1.8),
    ) +
    j(
      'happy',
      path(
        'M109 127 Q117 131 125 126 Q121 137 116 136 Q112 135 109 127Z',
        '#e9949f',
        '#9d6772',
        1.3,
      ),
    ) +
    j('oh', ell(117, 130, 3.5, 4, '#c7848e'));
  const bangs = j(
    'fringe',
    path(
      'M52 112 C34 76 48 41 77 30 Q100 15 128 24 Q158 19 177 48 Q195 78 179 107 Q175 128 158 132 Q168 117 161 96 Q155 84 152 69 Q146 84 133 88 Q134 80 134 78 Q126 88 108 92 Q116 83 116 72 Q100 88 83 91 Q92 82 91 71 Q83 82 74 87 Q74 112 63 123 Q62 113 61 109 Q59 119 56 122Z',
      hair,
    ) +
      path(
        'M54 76 Q55 49 82 38 Q100 30 115 34 Q78 38 65 68 Q75 57 84 54 Q69 69 64 82Z',
        '#d2dff8',
        'none',
      ) +
      path(
        'M112 35 Q143 25 161 49 Q149 42 141 43 Q156 49 159 60 Q137 41 113 42Z',
        '#d6dcf3',
        'none',
      ) +
      line(
        'M91 71 Q100 61 100 52 M117 72 Q121 65 121 52 M152 69 Q150 58 141 52 M172 111 Q179 87 170 71',
        '#646f9e',
        1.5,
      ) +
      line('M77 50 Q83 45 94 42 M104 39 l6 -1', '#eef5ff', 2.1),
  );
  const braid = j(
    'braid',
    g(
      'translate(58 119) rotate(9)',
      [0, 1, 2]
        .map((n) =>
          g(
            `translate(${n % 2 ? 1 : 0} ${n * 12}) scale(${1 - n * 0.12})`,
            path(
              'M-2 -5 Q-14 -2 -8 9 Q-5 16 1 16 Q14 13 9 1 Q6 -4 -2 -5Z',
              hair,
            ) + line('M-6 2 Q-8 7 -2 11', '#c1ceef', 1.5),
          ),
        )
        .join('') +
        path('M-3 42 Q-5 52 -12 53 Q-2 58 4 48Z', deep) +
        bow(0, 42, 0.55),
    ),
  );
  const hat = g(
    'rotate(-16 72 41)',
    path('M44 36 Q43 23 49 16 Q62 5 86 11 Q96 13 96 28 L96 36Z', navy) +
      path('M44 30 Q67 36 96 26 L97 34 Q71 46 44 38Z', pale) +
      path(
        'M35 37 Q44 33 55 38 Q80 40 99 32 Q109 29 111 33 Q108 43 84 48 Q46 51 34 42 Q30 39 35 37Z',
        navy,
      ) +
      line('M46 42 Q75 50 99 38', gold, 1.3) +
      path(
        'M48 31 Q30 19 33 8 Q37 8 42 16 Q42 6 45 3 Q52 10 48 31Z',
        '#fff0dc',
        '#937c8c',
        1.25,
      ) +
      flower(48, 32, 0.83) +
      star(82, 22, 0.46, gold),
  );
  const headGroup = j(
    'head',
    faceArt +
      bangs +
      braid +
      hat +
      g('translate(171 89)', bow(0, 0, 0.57) + ell(0, 10, 1.7, 2.5, gold)),
  );
  const book = j(
    'book',
    path(
      'M84 168 Q101 164 115 174 Q131 164 149 168 L146 193 Q131 191 117 198 Q102 191 86 193Z',
      '#6589a2',
    ) +
      path('M87 165 Q106 163 115 171 L116 194 Q101 186 88 189Z', pale) +
      path('M115 171 Q129 163 146 166 L143 189 Q127 187 116 194Z', pale) +
      line(
        'M94 172 q8 -1 15 4 m-14 2 q6 0 12 3 M123 177 q8 -4 14 -3',
        '#ae9283',
        1.2,
      ) +
      j(
        'page',
        path(
          'M116 172 Q126 161 135 160 L133 184 Q124 188 116 194Z',
          '#fff8e9',
          '#c6b79d',
          1,
        ),
      ),
  );
  const cup = j(
    'cup',
    j(
      'steam',
      line('M107 165 q-5 -5 0 -9 q3 -3 0 -6 M120 164 q4 -4 0 -8', '#ece1cc', 2),
    ) +
      path('M128 173 Q144 166 138 182 Q133 188 126 184Z', pale) +
      path('M97 170 Q112 165 132 169 Q134 188 119 191 Q102 192 97 170Z', pale) +
      ell(114.5, 170, 16, 3.1, '#cb9567') +
      flower(116, 181, 0.47, accent) +
      line('M99 193 q15 4 33 -1', '#a493a6', 2),
  );
  const cookie = j(
    'cookie',
    g(
      'translate(116 169)',
      path(
        'M0 -15 Q2 -17 5 -9 L14 -8 Q17 -7 11 -1 L10 3 L11 12 Q9 15 1 10 L-7 14 Q-11 16 -10 8 L-10 2 L-16 -4 Q-18 -8 -8 -9 L-4 -10Z',
        '#efbe78',
        '#a57d61',
        1.8,
      ) +
        ell(-5, -3, 1.6, 1.6, '#bd8660') +
        ell(4, 4, 1.4, 1.6, '#bd8660') +
        star(2, -5, 0.35, '#ffe7b2'),
    ),
  );
  const sail = j(
    'sail',
    g('translate(114 247) rotate(-3) scale(2.55 1.15)', fanShape(true)) +
      line('M46 228 Q26 229 18 222 M170 226 q16 -1 24 -6', '#b2d8e5', 2),
  );
  const tether = (x, id) =>
    j(
      id,
      path(`M${x + 6} 2 Q${x + 10} 63 ${x} 128`, 'none', '#8e7081', 3.8) +
        line(`M${x + 6} 2 Q${x + 10} 63 ${x} 128`, '#f3d4ad', 1.9),
    );
  const ribbons = j('ribbons', tether(72, 'strapL') + tether(159, 'strapR'));
  const hook = j(
    'hook',
    path('M200 126 Q192 125 183 133 L178 139', 'none', '#84647b', 4) +
      line('M200 126 Q192 125 183 133 L178 139', '#f7d7a3', 2),
  );
  const left = j(
      'leftArm',
      sleeve(false) +
        j(
          'leftWrist',
          j('fan', g('translate(80 192) rotate(-35)', fanShape())),
        ),
    ),
    right = j('rightArm', sleeve(true));
  const hands =
    j('leftGrip', j('gripL', hand(false))) +
    j('rightGrip', j('gripR', hand(true)));
  const art = j(
    'mirror',
    sail +
      j(
        'body',
        back +
          j('rightLeg', leg(true)) +
          j('leftLeg', leg()) +
          dress +
          skirt +
          belt +
          headGroup +
          ribbons +
          hook +
          left +
          right +
          book +
          cup +
          cookie +
          hands,
      ) +
      j(
        'stars',
        star(31, 209, 1.1, gold) +
          star(199, 209, 0.78, '#9dc7e5') +
          star(187, 231, 0.6, gold),
      ),
  );
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${portrait ? '22 -3 180 180' : `0 0 ${W} ${H}`}" stroke-linejoin="round" stroke-linecap="round"><defs>${defs}</defs>${art}</svg>`;
}
