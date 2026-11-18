import { MIN_SCALE, MAX_SCALE } from './motion.mjs';
export const VERSION = '1.1.0';
export const DEFAULT_MODEL = Object.freeze({
  id: 'qwen',
  name: 'Qwen 娘',
  kind: 'illustration',
});
export const DEFAULT_PREFS = Object.freeze({
  roam: true,
  edges: true,
  sound: false,
  scale: 1,
  scene: 'meadow',
});
export const FOODS = Object.freeze([
  'txt',
  'md',
  'csv',
  'log',
  'pdf',
  'png',
  'jpg',
  'jpeg',
  'gif',
  'webp',
]);
export const MAX_FOOD_BYTES = 100 * 1024 * 1024;
const clamp = (n, lo, hi) => Math.max(lo, Math.min(hi, n));
const whole = (n, d = 0) => (Number.isFinite(n) ? Math.round(n) : d);
const label = (v, n = 40) =>
  typeof v === 'string'
    ? v
        .replace(/[\u0000-\u001f\u007f-\u009f\u202a-\u202e\u2066-\u2069]/g, '')
        .trim()
        .slice(0, n)
    : '';
export function initialState(now = Date.now()) {
  return {
    version: 1,
    stats: { fullness: 68, mood: 86, bond: 32 },
    meals: 0,
    sleeping: false,
    prefs: { ...DEFAULT_PREFS },
    model: { ...DEFAULT_MODEL },
    serial: 1,
    born: now,
    log: [
      {
        id: 1,
        at: now,
        category: 'system',
        text: 'Qwen 娘来到紫藤花庭，准备好陪伴你了。',
      },
    ],
    reaction: null,
  };
}
export function restoreState(input, now = Date.now()) {
  const s = initialState(now);
  if (!input || input.version !== 1) return s;
  for (const k of Object.keys(s.stats))
    s.stats[k] = clamp(whole(input.stats?.[k], s.stats[k]), 0, 100);
  for (const k of ['roam', 'edges', 'sound'])
    if (typeof input.prefs?.[k] === 'boolean') s.prefs[k] = input.prefs[k];
  if (Number.isFinite(input.prefs?.scale))
    s.prefs.scale = clamp(input.prefs.scale, MIN_SCALE, MAX_SCALE);
  if (input.prefs?.scene === 'dusk') s.prefs.scene = 'dusk';
  if (validModel(input.model)) s.model = cleanModel(input.model);
  s.sleeping = input.sleeping === true;
  s.meals = Math.max(0, whole(input.meals));
  s.serial = Math.max(1, whole(input.serial, 1));
  s.born = Number.isFinite(input.born) ? input.born : now;
  if (Array.isArray(input.log))
    s.log = input.log
      .filter(
        (e) =>
          e &&
          Number.isFinite(e.at) &&
          typeof e.text === 'string' &&
          ['system', 'model', 'interact', 'feed'].includes(e.category),
      )
      .slice(0, 100)
      .map((e, i) => ({
        id: whole(e.id, i),
        at: e.at,
        category: e.category,
        text: label(e.text, 120),
      }));
  return s;
}
export const validModel = (m) =>
  m &&
  typeof m.id === 'string' &&
  (/^local-[a-zA-Z0-9-]{1,60}$/.test(m.id) || m.id === 'qwen') &&
  ['illustration', 'image', 'glb'].includes(m.kind) &&
  !!label(m.name);
const cleanModel = (m) =>
  m.id === 'qwen'
    ? { ...DEFAULT_MODEL }
    : { id: m.id, name: label(m.name), kind: m.kind };
export function reduceState(state, a, now = Date.now()) {
  if (!a || typeof a.type !== 'string') return state;
  const s = { ...state, serial: state.serial + 1, stats: { ...state.stats } };
  const inc = (k, n) => (s.stats[k] = clamp(s.stats[k] + n, 0, 100));
  const log = (category, text) =>
    (s.log = [{ id: s.serial, at: now, category, text }, ...s.log].slice(
      0,
      100,
    ));
  const react = (kind, text, ms = 4300) =>
    (s.reaction = { id: s.serial, kind, text, at: now, until: now + ms });
  switch (a.type) {
    case 'pat':
      s.sleeping = false;
      inc('mood', 3);
      inc('bond', 2);
      react(
        'pat',
        ['嘿嘿，最喜欢你啦！', '再摸一下嘛～', '今天也一起加油呀！'][
          s.serial % 3
        ],
      );
      log('interact', `轻轻摸了摸${s.model.name}的头。`);
      break;
    case 'play':
      s.sleeping = false;
      inc('mood', 6);
      inc('bond', 1);
      inc('fullness', -3);
      react('play', '看好啦，这一式叫花间摇扇。');
      log('interact', `和${s.model.name}玩了一会儿。`);
      break;
    case 'nap':
      s.sleeping = !s.sleeping;
      react(
        s.sleeping ? 'sleep' : 'wake',
        s.sleeping ? '午安，待会见… z Z' : '我醒啦！',
      );
      log(
        'interact',
        s.sleeping ? '伙伴合眼垂首，小憩片刻。' : '伙伴睡醒了，伸了个懒腰。',
      );
      break;
    case 'meal':
    case 'demo-meal':
      s.sleeping = false;
      s.meals++;
      inc('fullness', 16);
      inc('mood', 4);
      inc('bond', 2);
      react('eat', '啊呜，好吃！再来一口～');
      log(
        'feed',
        a.type === 'meal'
          ? '完成文件投喂，文件已移入系统回收站。'
          : '完成一次模拟投喂，原文件未改动。',
      );
      break;
    case 'time': {
      const t = new Date(now).toLocaleTimeString('zh-CN', {
        hour: '2-digit',
        minute: '2-digit',
        hour12: false,
      });
      react('time', `现在是 ${t} 哦！`);
      log('interact', '伙伴报出了本地时间。');
      break;
    }
    case 'pref': {
      const { key, value } = a;
      if (
        ['roam', 'edges', 'sound'].includes(key)
          ? typeof value !== 'boolean'
          : key === 'scale'
            ? !(
                Number.isFinite(value) &&
                value >= MIN_SCALE &&
                value <= MAX_SCALE
              )
            : key === 'scene'
              ? !['meadow', 'dusk'].includes(value)
              : true
      )
        return state;
      if (s.prefs[key] === value) return state;
      s.prefs = { ...s.prefs, [key]: value };
      break;
    }
    case 'select':
      if (!validModel(a.model)) return state;
      s.model = cleanModel(a.model);
      s.sleeping = false;
      react('wake', '嗨，很高兴见到你！');
      log('model', `换上了「${s.model.name}」模型。`);
      break;
    case 'removed':
      log('model', `从模型库移除了「${label(a.name)}」。`);
      break;
    case 'utility':
      if (!['downloads', 'calculator'].includes(a.key)) return state;
      log(
        'system',
        a.key === 'downloads'
          ? '已请求文件管理器打开下载目录。'
          : '已请求打开系统计算器。',
      );
      break;
    case 'clear-log':
      s.log = [];
      break;
    default:
      return state;
  }
  return s;
}
export function foodHint(name, size) {
  const ext = String(name).split('.').pop().toLowerCase();
  if (
    !name ||
    name.startsWith('.') ||
    !name.includes('.') ||
    !FOODS.includes(ext)
  )
    return '只接受常见文档或图片：' + FOODS.join('、') + '。';
  if (!Number.isFinite(size) || size < 0 || size > MAX_FOOD_BYTES)
    return '一份点心不能超过 100 MB。';
  return '';
}
export function formatBytes(n) {
  return n < 1024
    ? `${n} B`
    : n < 1048576
      ? `${+(n / 1024).toFixed(1)} KB`
      : `${+(n / 1048576).toFixed(1)} MB`;
}
