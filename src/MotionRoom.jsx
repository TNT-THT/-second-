import React, { useState } from 'react';
import { ArrowLeft, ArrowRight, Film, Info } from 'lucide-react';
import { QwenActor } from './QwenActor.jsx';
import atlas from '../core/qwen-atlas.json';
const reels = [
  ['walk', '收扇漫步', '收起折扇迈步，落脚的节奏跟随实际走过的距离。'],
  ['climb', '贴边攀行', '手掌交替换点，屈膝蹬住侧边；下行时倒放攀行步相。'],
  ['glide', '乘扇横渡', '到达顶部后侧坐扇面，鞋尖轻摆，发梢顺风而动。'],
  [
    'held',
    '提起扶帽',
    '被提起时扶住小礼帽。实际拖动时，她会随鼠标速度轻轻摆动。',
  ],
  ['land', '屈膝落定', '松手时屈膝缓冲，再回到当前边界对应的姿势。'],
  ['eat', '捧碟进食', '托住小碟，把星形点心送到嘴边。真实投喂成功后才会播放。'],
  ['play', '花间扇舞', '转腕展扇、抬腿踏步，裙摆和长发跟随重心回摆。'],
  ['pat', '抚摸回礼', '闭眼微笑，提起裙边，向你行一个小礼。'],
  ['notice', '抬眼迎接', '鼠标移近，或有文件靠近时，抬眼侧首迎接。'],
  ['sleep', '垂首小憩', '合眼低头，动作随着呼吸放慢。小憩时不会漫游。'],
  ['idle', '执扇待机', '手腕轻摇折扇，发束轻摆，偶尔眨眼。'],
];
export function MotionRoom() {
  const [index, select] = useState(0),
    [face, turn] = useState(1);
  const [key, name, desc] = reels[index];
  const mirror = ['walk', 'climb', 'glide'].includes(key);
  return (
    <div className="motion-room">
      <div className="rehearsal">
        <div className="reel-slate">
          <Film size={13} /> SERVICE QWEN
        </div>
        <div className="reel-figure">
          <QwenActor key={key} kind={key} face={mirror ? face : 1} demo />
        </div>
        <small>TAKE {String(index + 1).padStart(2, '0')} / 11</small>
      </div>
      <div className="reel-head">
        <div>
          <div className="eyebrow">{atlas[key].frames} FRAMES</div>
          <h3>{name}</h3>
        </div>
        {mirror && (
          <div className="dir-toggle" aria-label="动作朝向">
            {[
              [-1, '向左', ArrowLeft],
              [1, '向右', ArrowRight],
            ].map(([v, n, I]) => (
              <button key={v} aria-pressed={face === v} onClick={() => turn(v)}>
                <I size={13} />
                {n}
              </button>
            ))}
          </div>
        )}
      </div>
      <p className="reel-copy">{desc}</p>
      <div className="reel-list">
        {reels.map(([k, n], i) => (
          <button key={k} aria-pressed={i === index} onClick={() => select(i)}>
            {n}
          </button>
        ))}
      </div>
      <div className="footnote">
        <Info size={13} />
        试映仅展示内置伙伴的动作，不投喂文件，也不改变桌宠状态。
      </div>
    </div>
  );
}
