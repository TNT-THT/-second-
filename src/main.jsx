import { BUILTINS, builtin, allowed } from '../core/repertoire.mjs';
import React, { useCallback, useEffect, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import {
  ArrowUpRight,
  Clapperboard,
  MessageCircle,
  PanelsTopLeft,
  BookOpen,
  Box,
  Boxes,
  Calculator,
  Check,
  ChevronRight,
  Clock3,
  Cookie,
  Download,
  Expand,
  File as FileIcon,
  FolderOpen,
  Footprints,
  Gamepad2,
  Heart,
  History,
  Info,
  Layers3,
  LayoutDashboard,
  LockKeyhole,
  MapPin,
  Monitor,
  MonitorPlay,
  MonitorUp,
  Moon,
  MousePointer2,
  Move,
  Plus,
  Settings2,
  ShieldCheck,
  SlidersHorizontal,
  Smile,
  Sun,
  Trash2,
  Upload,
  UserRound,
  Utensils,
  Volume2,
  VolumeX,
  X,
} from 'lucide-react';
import '@fontsource-variable/noto-sans-sc/wght.css';
import './style.css';
import {
  DEFAULT_MODEL,
  VERSION,
  foodHint,
  formatBytes,
} from '../core/state.mjs';
import {
  api,
  act,
  chirp,
  openRelease,
  pref,
  setPet,
  useStudio,
} from './store.js';
import { MotionRoom, RoutePicker, LeisurePicker } from './MotionRoom.jsx';
import { MIN_SCALE, MAX_SCALE } from '../core/motion.mjs';
import { Companion, Avatar } from './Companion.jsx';
import { importModel, listModels, removeModel } from './models.js';
import { Modal, IconButton, Toggle, SnackArt } from './ui.jsx';

const pages = [
  ['desk', '工作台', LayoutDashboard],
  ['models', '模型库', Layers3],
  ['motion', '动作手册', Clapperboard],
  ['history', '活动记录', History],
];
const titles = {
  desk: [
    'COMPANION STUDIO',
    '桌宠工作台',
    '在这里照顾伙伴，定制属于你的桌面陪伴。',
  ],
  models: ['MODEL LIBRARY', '模型库', '收藏喜欢的形象，随时为桌面换一位伙伴。'],
  motion: [
    'MOTION HANDBOOK',
    '伙伴动作手册',
    '试映当前形象，决定她在哪里散步、怎样度过闲暇。',
  ],
  history: [
    'ACTIVITY JOURNAL',
    '活动记录',
    '把陪伴中的小事，留在这份本地手记里。',
  ],
};
const categories = {
  system: '系统',
  model: '模型',
  interact: '互动',
  feed: '投喂',
};
const nativePet = !!api && location.hash === '#pet';
document.documentElement.dataset.surface = nativePet ? 'pet' : 'studio';
function useNotification() {
  const [message, set] = useState(''),
    tm = useRef(null);
  const tell = useCallback((m) => {
    clearTimeout(tm.current);
    set(m);
    tm.current = setTimeout(() => set(''), 4100);
  }, []);
  useEffect(() => () => clearTimeout(tm.current), []);
  return [message, tell];
}
function useFeeding(notify, preview) {
  const busy = useRef(false);
  return async (files, items) => {
    if (busy.current) return;
    const f = files?.[0];
    if (
      files?.length !== 1 ||
      Array.from(items || []).some((i) => i.webkitGetAsEntry?.()?.isDirectory)
    )
      return notify('请一次投喂一个普通文件，不能投喂文件夹。');
    const issue = foodHint(f.name, f.size);
    if (issue) return notify(issue);
    if (!api) {
      preview(f);
      return;
    }
    busy.current = true;
    try {
      const result = await api.feed(f);
      if (result.ok) {
        chirp('eat');
        notify('投喂成功，文件已移入系统回收站。');
      } else if (!result.cancelled) notify(result.message || '没有进行投喂。');
    } catch {
      notify('未能完成投喂，请查看文件原位置。');
    } finally {
      busy.current = false;
    }
  };
}
function NativePet() {
  const [message, notify] = useNotification();
  const feed = useFeeding(notify);
  return (
    <>
      <Companion mode="native" onFeed={feed} />
      <div
        role="status"
        className={'native-note ' + (message ? 'visible' : '')}
      >
        {message}
      </div>
    </>
  );
}
function PanelTitle({ icon: Icon, title, children }) {
  return (
    <div className="panel-head">
      <div className="panel-title">
        <Icon size={19} strokeWidth={1.8} />
        <h2>{title}</h2>
      </div>
      {children}
    </div>
  );
}
function Stage({
  feed,
  notify,
  state,
  expanded,
  onExpand,
  paused,
  onMotionRoom,
}) {
  const [practice, setPractice] = useState(false);
  const dusk = state.prefs.scene === 'dusk';
  return (
    <section className={'panel stage-panel ' + (expanded ? 'expanded' : '')}>
      <PanelTitle icon={MonitorPlay} title="实时预览">
        <div className="inline-tools">
          <button className="motion-room-btn" onClick={onMotionRoom}>
            <Clapperboard size={14} />
            动作试映
          </button>
          <span className="tool-divider" />
          <IconButton
            label={state.prefs.sound ? '关闭交互音效' : '开启交互音效'}
            onClick={() => pref('sound', !state.prefs.sound)}
          >
            {state.prefs.sound ? <Volume2 /> : <VolumeX />}
          </IconButton>
          <span className="tool-divider" />
          <IconButton
            label={expanded ? '收起预览' : '放大预览'}
            onClick={onExpand}
          >
            {expanded ? <X /> : <Expand />}
          </IconButton>
        </div>
      </PanelTitle>
      <div
        className={'habitat ' + (dusk ? 'dusk' : '')}
        style={{ '--actor-scale': state.prefs.scale }}
      >
        <div className="habitat-art" />
        <div className="habitat-vignette" />
        <span className="scene-live">
          <i /> LIVE
        </span>
        <span className="scene-no">SCENE &nbsp;{dusk ? '02' : '01'} / 02</span>
        <Companion
          practice={practice}
          onFeed={feed}
          paused={paused}
          notify={notify}
        />
        <div className="scene-location">
          <MapPin size={14} />
          {dusk ? '薄暮花庭' : '紫藤花庭'}
        </div>
        <button
          className="scene-switch"
          onClick={() => pref('scene', dusk ? 'meadow' : 'dusk')}
          aria-label="切换场景光照"
        >
          {dusk ? <Moon size={14} /> : <Sun size={14} />}
          <span>切换光照</span>
        </button>
      </div>
      <div className="stage-foot">
        <span>
          <MousePointer2 size={14} />
          点击互动
          <i />
          拖拽移动
        </span>
        <button
          className="shelf-btn"
          aria-pressed={practice}
          disabled={!state.prefs.ledges}
          onClick={() => setPractice(!practice)}
        >
          <PanelsTopLeft size={13} /> 练习窗沿
        </button>
        <span className="quiet-status">
          <b />
          {state.sleeping
            ? '正在小憩'
            : state.prefs.roam
              ? state.prefs.flight
                ? '空中巡游'
                : '步行巡游'
              : '已固定位置'}
        </span>
      </div>
    </section>
  );
}
function Profile({ state, toModels }) {
  const { model, stats, sleeping } = state;
  const actions = [
    ['pat', Heart, '摸摸头'],
    ['chat', MessageCircle, '说说话'],
    ['play', Gamepad2, '玩一会'],
    ['nap', sleeping ? Sun : Moon, sleeping ? '唤醒她' : '小憩'],
  ];
  return (
    <section className="panel profile-panel">
      <PanelTitle icon={UserRound} title="伙伴档案">
        <button className="text-btn small" onClick={toModels}>
          换模型 <ChevronRight size={14} />
        </button>
      </PanelTitle>
      <div className="profile-body">
        <div className="identity">
          <div className="face">
            <Avatar key={model.id} model={model} flat />
          </div>
          <div className="identity-text">
            <div className="serial">
              {builtin(model.id)
                ? 'POCKET QWEN / ' + builtin(model.id).code
                : 'CUSTOM COMPANION'}
            </div>
            <h3>{model.name}</h3>
            <span className="label-lilac">
              {builtin(model.id)
                ? 'Q版 · ' + builtin(model.id).edition
                : model.kind === 'glb'
                  ? '3D 模型'
                  : '2D 模型'}
            </span>
            <span className="identity-state">
              <i />
              {sleeping ? '小憩中' : '陪伴中'}
            </span>
          </div>
        </div>
        <div className="meters">
          {[
            ['fullness', '饱食度', Utensils],
            ['mood', '心情值', Smile],
            ['bond', '亲密度', Heart],
          ].map(([key, text, I]) => (
            <div key={key} className={'meter ' + key}>
              <div>
                <span>
                  <I size={14} />
                  {text}
                </span>
                <strong>
                  {stats[key]}
                  <em> / 100</em>
                </strong>
              </div>
              <div
                className="meter-track"
                role="progressbar"
                aria-label={text}
                aria-valuemin={0}
                aria-valuemax={100}
                aria-valuenow={stats[key]}
              >
                <b style={{ width: stats[key] + '%' }} />
              </div>
            </div>
          ))}
        </div>
        <div className="action-row">
          {actions.map(([k, I, t]) => (
            <button
              key={k}
              disabled={k === 'play' && !allowed(state.prefs, 'play')}
              className={'interact ' + k}
              onClick={() => {
                chirp(k);
                act({ type: k });
              }}
            >
              <I size={19} />
              {t}
            </button>
          ))}
        </div>
      </div>
    </section>
  );
}
function DropStation({ feed }) {
  const [over, setOver] = useState(false),
    input = useRef();
  return (
    <section className="panel feed-panel">
      <PanelTitle icon={Cookie} title="文件投喂">
        <span className="safe-badge">
          <ShieldCheck size={13} /> 安全投喂
        </span>
      </PanelTitle>
      <div className="feed-body">
        <button
          className={'dropzone ' + (over ? 'over' : '')}
          onClick={() => input.current.click()}
          onDragOver={(e) => {
            e.preventDefault();
            setOver(true);
          }}
          onDragLeave={() => setOver(false)}
          onDrop={(e) => {
            e.preventDefault();
            setOver(false);
            feed(e.dataTransfer.files, e.dataTransfer.items);
          }}
        >
          <SnackArt />
          <div>
            <strong>拖入文件，给伙伴一份点心</strong>
            <p>
              或 <span>点击选择文件</span>，支持常见文档与图片
            </p>
          </div>
          <Upload className="drop-upload" size={21} />
        </button>
        <input
          hidden
          ref={input}
          type="file"
          accept=".txt,.md,.csv,.log,.pdf,.png,.jpg,.jpeg,.gif,.webp"
          aria-label="选择要投喂的文件"
          onChange={(e) => {
            if (e.target.files?.length) feed(e.target.files);
            e.target.value = '';
          }}
        />
        <div className="snack-receipt">
          <span>01 · 选择点心</span>
          <i>›</i>
          <span>02 · 核对文件</span>
          <i>›</i>
          <span>{api ? '03 · 确认回收' : '03 · 模拟进食'}</span>
        </div>
        <div className="footnote">
          <Info size={13} />
          {api
            ? '确认后移入系统回收站，不会永久删除。'
            : '网页预览仅模拟投喂，不会读取或删除原文件。'}
        </div>
      </div>
    </section>
  );
}
function SwitchRow({ icon: I, title, desc, value, change }) {
  return (
    <div className="switch-row">
      <div className="setting-key">
        {I && (
          <div className="setting-icon">
            <I size={17} />
          </div>
        )}
        <div>
          <strong>{title}</strong>
          <small>{desc}</small>
        </div>
      </div>
      <Toggle label={title} value={value} onChange={change} />
    </div>
  );
}
function ScaleControl({ value }) {
  const pct = Math.round(value * 100);
  return (
    <label className="size-label">
      <span>
        角色大小 <b>{pct}%</b>
      </span>
      <input
        aria-label="角色大小"
        aria-valuetext={`${pct}% 等比例`}
        type="range"
        min={MIN_SCALE}
        max={MAX_SCALE}
        step="0.05"
        value={value}
        onChange={(e) => pref('scale', +e.target.value)}
      />
    </label>
  );
}
function Behavior({ state, settings, handbook }) {
  return (
    <section className="panel behavior">
      <PanelTitle icon={SlidersHorizontal} title="行为设置">
        <IconButton label="更多行为设置" onClick={settings}>
          <Settings2 size={17} />
        </IconButton>
      </PanelTitle>
      <div className="behavior-body">
        <SwitchRow
          icon={Footprints}
          title="自由漫游"
          desc="在屏幕内散步，沿边缘停留"
          value={state.prefs.roam}
          change={(v) => pref('roam', v)}
        />
        <RoutePicker state={state} />
        <ScaleControl value={state.prefs.scale} />
        <button className="handbook-link" onClick={handbook}>
          <Clapperboard size={14} /> 管理动作与待机 <ChevronRight size={14} />
        </button>
      </div>
    </section>
  );
}
function Library({ state, models, onImport, onRemove, loading, sample }) {
  const inp = useRef();
  const [over, hover] = useState(false);
  const all = [
    ...BUILTINS.map((m) => ({ ...m, ext: 'Q版 · ' + m.edition })),
    ...models,
  ];
  return (
    <div className="library">
      <div className="section-label">
        <span>
          <Boxes size={16} /> 我的伙伴{' '}
          <b>{String(all.length).padStart(2, '0')}</b>
        </span>
        <button
          className="btn secondary compact"
          onClick={sample}
          disabled={loading}
        >
          <Box size={15} /> 试用 3D 示例
        </button>
      </div>
      <div className="library-grid">
        {all.map((m) => (
          <article
            className={
              'model-card ' + (m.id === state.model.id ? 'selected' : '')
            }
            key={m.id}
          >
            <div
              className={
                'model-thumb ' + (builtin(m.id) ? 'garden ' + m.id : '')
              }
            >
              <Avatar model={m} flat={m.kind === 'glb'} />
              <span className="model-type">{m.ext}</span>
              {m.id === state.model.id && (
                <span className="selected-label">
                  <Check size={12} /> 使用中
                </span>
              )}
            </div>
            <div className="model-info">
              <h3>{m.name}</h3>
              <p>
                {builtin(m.id)
                  ? '掌心花庭 · ' + builtin(m.id).edition + ' / 分层 Q 版'
                  : `${m.kind === 'glb' ? (m.flyClip ? '3D · 可飞行' : '3D · 地面') : '2D · 地面'} · ${formatBytes(m.size)}`}
              </p>
              <div>
                <button
                  className="btn secondary compact"
                  disabled={m.id === state.model.id}
                  onClick={() =>
                    act({
                      type: 'select',
                      model: {
                        id: m.id,
                        name: m.name,
                        kind: m.kind,
                        flyClip: m.flyClip,
                      },
                    })
                  }
                >
                  {m.id === state.model.id ? '当前伙伴' : '使用模型'}
                </button>
                {!builtin(m.id) && (
                  <IconButton
                    label={'移除 ' + m.name}
                    onClick={() => onRemove(m)}
                  >
                    <Trash2 size={16} />
                  </IconButton>
                )}
              </div>
            </div>
          </article>
        ))}
        <button
          disabled={loading}
          className={'import-tile ' + (over ? 'over' : '')}
          onClick={() => inp.current.click()}
          onDragOver={(e) => {
            e.preventDefault();
            hover(true);
          }}
          onDragLeave={() => hover(false)}
          onDrop={(e) => {
            e.preventDefault();
            hover(false);
            onImport(e.dataTransfer.files);
          }}
        >
          <div className="import-plus">
            <Plus size={25} />
          </div>
          <strong>{loading ? '正在导入…' : '导入本地模型'}</strong>
          <small>拖入此处或选择文件</small>
          <em>PNG · GIF · WEBP · GLB</em>
        </button>
      </div>
      <input
        type="file"
        ref={inp}
        hidden
        accept=".png,.gif,.webp,.glb"
        aria-label="选择模型文件"
        onChange={(e) => {
          if (e.target.files?.length) onImport(e.target.files);
          e.target.value = '';
        }}
      />
      <div className="library-note">
        <ShieldCheck size={18} />
        <div>
          <b>只保存在这台设备</b>
          <p>
            导入是复制，不会移动原文件。每个模型最大 24 MB；GLB
            需将网格和纹理全部内嵌，支持按动作名称切换动画。
          </p>
        </div>
      </div>
    </div>
  );
}
function Journal({ state, clear }) {
  const [filter, set] = useState('all');
  const entries = state.log.filter(
    (e) => filter === 'all' || e.category === filter,
  );
  return (
    <section className="panel journal">
      <PanelTitle icon={History} title="陪伴手记">
        <button
          className="text-btn"
          disabled={!state.log.length}
          onClick={clear}
        >
          <Trash2 size={14} /> 清空记录
        </button>
      </PanelTitle>
      <div className="filter-row">
        {[['all', '全部'], ...Object.entries(categories)].map(([k, n]) => (
          <button
            key={k}
            className={filter === k ? 'active' : ''}
            onClick={() => set(k)}
          >
            {n}
          </button>
        ))}
      </div>
      {entries.length ? (
        <ul className="entry-list">
          {entries.map((e) => {
            const I = {
              system: Monitor,
              interact: Heart,
              feed: Cookie,
              model: Layers3,
            }[e.category];
            return (
              <li key={e.id}>
                <div className={'entry-icon ' + e.category}>
                  <I size={18} />
                </div>
                <div>
                  <strong>{e.text}</strong>
                  <small>
                    {categories[e.category]} ·{' '}
                    {new Date(e.at).toLocaleDateString('zh-CN')}
                  </small>
                </div>
                <time>
                  {new Date(e.at).toLocaleTimeString('zh-CN', {
                    hour: '2-digit',
                    minute: '2-digit',
                    hour12: false,
                  })}
                </time>
              </li>
            );
          })}
        </ul>
      ) : (
        <div className="empty">
          <BookOpen />
          <h3>这一页还很安静</h3>
          <p>和伙伴互动后，就会在这里留下记录。</p>
        </div>
      )}
      <div className="journal-foot">
        <LockKeyhole size={13} /> 仅保存最近 100 条，不记录文件路径或内容。
      </div>
    </section>
  );
}
function Guide({ download = false, model }) {
  return (
    <>
      <div className="guide-hero">
        <div className="guide-avatar">
          <Avatar model={model} key={model.id} flat />
        </div>
        <div>
          <div className="eyebrow">POCKET COMPANION</div>
          <strong>让她在桌面上陪你工作</strong>
          <p>无需一直打开工作台，也不需要登录。</p>
        </div>
      </div>
      <div className="guide-list">
        {[
          [
            '01',
            '直接出现在真实桌面',
            '启动桌面版后，只显示透明置顶角色。不弹工作台，不抢当前窗口的键盘焦点。',
          ],
          [
            '02',
            '只接住属于她的点击',
            '角色小窗始终接收鼠标：点按回礼、按住提起、拖动再松手。小窗以外不拦点击，也不抢键盘焦点。',
          ],
          [
            '03',
            '喂食与换装是两件事',
            '拖文件到角色身上是投喂，需确认才移入回收站；加新形象请去「模型库」导入。',
          ],
          [
            '04',
            '随时找回工作台',
            '右键系统托盘的 PetDesk 图标，或再次启动程序，即可打开工作台、调整大小或退出。',
          ],
        ].map(([n, t, d]) => (
          <div key={n}>
            <b>{n}</b>
            <div>
              <strong>{t}</strong>
              <p>{d}</p>
            </div>
          </div>
        ))}
      </div>
      {(download || !api) && (
        <div className="dialog-note">
          <Info size={16} />{' '}
          浏览器只能预览。下载测试包、解压，再按内附说明启动，才可在其他应用上方活动。
        </div>
      )}
      <div className="dialog-actions">
        <button className="btn primary" onClick={openRelease}>
          <Download size={16} /> 前往下载桌面版 <ArrowUpRight size={14} />
        </button>
      </div>
    </>
  );
}
function Settings({ state, utility }) {
  const p = state.prefs;
  return (
    <>
      <div className="settings-section">
        <SwitchRow
          icon={Move}
          title="边缘玩耍"
          desc="漫游时优先沿当前显示器的工作区边缘活动"
          value={p.edges}
          change={(v) => pref('edges', v)}
        />
        <SwitchRow
          icon={Volume2}
          title="交互音效"
          desc="仅在主动摸头、玩耍或投喂时发声"
          value={p.sound}
          change={(v) => pref('sound', v)}
        />
        <SwitchRow
          icon={PanelsTopLeft}
          title="窗口落脚点"
          desc="Windows：将当前普通窗口的上沿当作地面；不读取标题或画面。预览用练习窗沿。"
          value={p.ledges}
          change={(v) => pref('ledges', v)}
        />
        <LeisurePicker state={state} />
        <ScaleControl value={p.scale} />
      </div>
      <div className="tools-section">
        <h3>桌面小工具</h3>
        <p>只响应你的点击，不读取剪贴板或代替你操作其他软件。</p>
        <div>
          {[
            [Clock3, '报时', 'time'],
            [FolderOpen, '下载目录', 'downloads'],
            [Calculator, '计算器', 'calculator'],
          ].map(([I, n, k]) => (
            <button
              className="btn secondary compact"
              key={k}
              onClick={() => utility(k)}
            >
              <I size={15} />
              {n}
            </button>
          ))}
        </div>
      </div>
      {api && (
        <div className="dialog-actions">
          <button className="text-btn" onClick={() => api.quit()}>
            退出 PetDesk
          </button>
        </div>
      )}
    </>
  );
}
function Studio() {
  const { state } = useStudio(),
    [page, setPage] = useState('desk'),
    [dialog, setDialog] = useState(null),
    [expanded, expand] = useState(false),
    [models, setModels] = useState([]),
    [importing, setImporting] = useState(false);
  const [note, notify] = useNotification(),
    lock = useRef(false);
  const feed = useFeeding(notify, (f) =>
    setDialog({ type: 'meal', name: f.name, size: f.size }),
  );
  const go = (p) => {
    if (!pages.some(([id]) => id === p)) return;
    notify('');
    setPage(p);
    setDialog(null);
    expand(false);
    window.scrollTo(0, 0);
  };
  useEffect(() => api?.onPage(go), []);
  useEffect(() => {
    const cancel = (e) => {
      if (e.key === 'Escape') expand(false);
    };
    window.addEventListener('keydown', cancel);
    return () => window.removeEventListener('keydown', cancel);
  }, []);
  async function reload() {
    try {
      setModels(await listModels());
    } catch {
      notify('本地模型库不可用，请检查存储权限。');
    }
  }
  useEffect(() => {
    if (page === 'models') void reload();
  }, [page]);
  async function add(files) {
    if (lock.current) return;
    if (files?.length !== 1) return notify('一次请选择一个模型。');
    lock.current = true;
    setImporting(true);
    try {
      const m = await importModel(files[0]);
      await act({
        type: 'select',
        model: { id: m.id, name: m.name, kind: m.kind, flyClip: m.flyClip },
      });
      await reload();
      notify(`已导入并换上「${m.name}」。`);
    } catch (e) {
      notify(e.message || '模型导入失败。');
    } finally {
      lock.current = false;
      setImporting(false);
    }
  }
  async function sample() {
    try {
      const r = await fetch('./models/Mint.glb');
      if (!r.ok) throw Error();
      await add([new File([await r.blob()], '薄荷团子.glb')]);
    } catch {
      notify('内置示例读取失败。');
    }
  }
  async function del(m) {
    try {
      if (state.model.id === m.id)
        await act({ type: 'select', model: DEFAULT_MODEL });
      await removeModel(m.id);
      await act({ type: 'removed', name: m.name });
      await reload();
      notify('已移除模型副本，原文件未改动。');
    } catch (e) {
      notify(e.message);
    }
    setDialog(null);
  }
  async function utility(k) {
    if (k === 'time') {
      await act({ type: 'time' });
      notify(
        '本地时间 ' +
          new Date().toLocaleTimeString('zh-CN', {
            hour: '2-digit',
            minute: '2-digit',
          }),
      );
      return;
    }
    if (!api) return notify('此操作仅在桌面版可用，网页不会启动系统程序。');
    const r = await api.utility(k);
    notify(r.ok ? '已发送打开请求。' : r.message);
  }
  const [kicker, title, sub] = titles[page],
    show = (t) => setDialog({ type: t });
  return (
    <div className="shell">
      <header className="topbar">
        <div className="wrap top-inner">
          <button
            className="brand"
            onClick={() => go('desk')}
            aria-label="PetDesk 工作台"
          >
            <img src="./art/petdesk.svg" alt="" />
            <strong>PetDesk</strong>
          </button>
          <span className="brand-line" />
          <nav aria-label="主导航">
            {pages.map(([id, n, I]) => (
              <button
                key={id}
                className={id === page ? 'active' : ''}
                aria-current={id === page ? 'page' : undefined}
                onClick={() => go(id)}
              >
                <I size={16} />
                {n}
              </button>
            ))}
          </nav>
          <div className="top-right">
            <span className="local-chip">
              <i />
              {api ? '桌面端在线' : '本地预览'}
            </span>
            <span className="tool-divider" />
            <IconButton label="工作台设置" onClick={() => show('settings')}>
              <Settings2 size={18} />
            </IconButton>
          </div>
        </div>
      </header>
      <main className="wrap">
        <div className="intro">
          <div>
            <div className="eyebrow">
              <span />
              {kicker}
            </div>
            <h1>{title}</h1>
            <p>{sub}</p>
          </div>
          <div className="intro-actions">
            <button className="btn secondary" onClick={() => show('guide')}>
              <BookOpen size={16} />
              使用指南
            </button>
            <button
              className="btn primary"
              onClick={() => (api ? setPet(true) : show('download'))}
            >
              {api ? <MonitorUp size={17} /> : <Download size={16} />}
              {api ? '回到桌面' : '下载桌面版'}
            </button>
          </div>
        </div>
        {page === 'desk' && (
          <div className="dashboard">
            <Stage
              state={state}
              feed={feed}
              notify={notify}
              expanded={expanded}
              onExpand={() => expand((v) => !v)}
              paused={!!dialog}
              onMotionRoom={() => show('motion')}
            />
            <Profile state={state} toModels={() => go('models')} />
            <DropStation feed={feed} />
            <Behavior
              handbook={() => go('motion')}
              state={state}
              settings={() => show('settings')}
            />
          </div>
        )}
        {expanded && (
          <div className="expand-shade" onClick={() => expand(false)} />
        )}
        {page === 'motion' && <MotionRoom state={state} />}
        {page === 'models' && (
          <Library
            state={state}
            models={models}
            onImport={add}
            onRemove={(m) => setDialog({ type: 'remove', m })}
            loading={importing}
            sample={sample}
          />
        )}
        {page === 'history' && (
          <Journal state={state} clear={() => show('clear')} />
        )}
        <footer>
          <span>
            <span className="footer-mark">✦</span>
            <b>PetDesk</b>
            <i /> DESKTOP COMPANION <em>v{VERSION}</em>
          </span>
          <span>
            <ShieldCheck size={14} />
            本地存储 · 离线陪伴
          </span>
        </footer>
      </main>
      {dialog && (
        <Modal
          title={
            {
              guide: '桌宠使用指南',
              download: '带伙伴回到桌面',
              settings: '工作台设置',
              motion: '伙伴动作试映',
              meal: '模拟文件投喂',
              remove: '移除这个模型？',
              clear: '清空活动记录？',
            }[dialog.type]
          }
          kicker={
            {
              guide: 'QUICK GUIDE',
              download: 'DESKTOP EDITION',
              settings: 'PREFERENCES',
              motion: 'MOTION ATELIER',
              meal: 'FEEDING PREVIEW',
              remove: 'MODEL LIBRARY',
              clear: 'ACTIVITY JOURNAL',
            }[dialog.type]
          }
          close={() => setDialog(null)}
        >
          {['guide', 'download'].includes(dialog.type) && (
            <Guide model={state.model} download={dialog.type === 'download'} />
          )}
          {dialog.type === 'motion' && <MotionRoom state={state} compact />}
          {dialog.type === 'settings' && (
            <Settings state={state} utility={utility} />
          )}
          {dialog.type === 'meal' && (
            <>
              <div className="file-chip">
                <FileIcon size={23} />
                <div>
                  <strong>{dialog.name}</strong>
                  <small>{formatBytes(dialog.size)}</small>
                </div>
              </div>
              <p className="dialog-copy">
                让{state.model.name}尝尝这份点心吗？本次是浏览器模拟，
                <b>原文件不会被读取或删除</b>。
              </p>
              <div className="dialog-actions">
                <button
                  className="btn secondary"
                  onClick={() => setDialog(null)}
                >
                  取消
                </button>
                <button
                  className="btn primary"
                  onClick={() => {
                    chirp('eat');
                    act({ type: 'demo-meal' });
                    setDialog(null);
                    notify('模拟投喂完成，原文件未改动。');
                  }}
                >
                  模拟喂食
                </button>
              </div>
            </>
          )}
          {dialog.type === 'remove' && (
            <>
              <p className="dialog-copy">
                移除「{dialog.m.name}
                」在本机模型库中的副本。不会修改导入时的原文件。
              </p>
              <div className="dialog-actions">
                <button
                  className="btn secondary"
                  onClick={() => setDialog(null)}
                >
                  取消
                </button>
                <button className="btn danger" onClick={() => del(dialog.m)}>
                  确认移除
                </button>
              </div>
            </>
          )}
          {dialog.type === 'clear' && (
            <>
              <p className="dialog-copy">
                这只会清除此设备上的手记，不影响模型、伙伴状态或任何文件。
              </p>
              <div className="dialog-actions">
                <button
                  className="btn secondary"
                  onClick={() => setDialog(null)}
                >
                  取消
                </button>
                <button
                  className="btn danger"
                  onClick={() => {
                    act({ type: 'clear-log' });
                    setDialog(null);
                  }}
                >
                  确认清空
                </button>
              </div>
            </>
          )}
        </Modal>
      )}
      {note && (
        <div role="status" className="toast">
          <Info size={17} />
          {note}
        </div>
      )}
    </div>
  );
}
for (const ev of ['dragover', 'drop'])
  window.addEventListener(ev, (e) => {
    if (e.dataTransfer?.types.includes('Files')) e.preventDefault();
  });
createRoot(document.getElementById('root')).render(
  nativePet ? <NativePet /> : <Studio />,
);
