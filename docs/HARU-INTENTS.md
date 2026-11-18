# 意图映射：声称的支持 vs. 真实支持

`config/intent_mapping.json` 只保存**语义通道 → Cubism 参数别名**。运行时用能力指纹
（小写、去前缀、官方/社区别名归一）找到真实参数，再用 Core 的 Min/Max 钳位；没有写死的
参数 ID，也不需要为换模型改逻辑。`tools/model_compatibility_check.py` 与工作台的导入提示
都基于同一套语义。

## Haru（`assets/models/haru`，4-r.7）

| 语义 | 实际绑定的参数 | 说明 |
|---|---|---|
| `eye_left` / `eye_right` | `ParamEyeLOpen` / `ParamEyeROpen` | 开闭 |
| `smile_left` / `smile_right` | `ParamEyeLSmile` / `ParamEyeRSmile` | 弯眼，COMFORT 用得上 |
| `mouth_open` / `mouth_form` | `ParamMouthOpenY` / `ParamMouthForm` | 张口 / 抿嘴 |
| `breath` | `ParamBreath` | 呼吸相位 |
| `body_roll` / `body_yaw` | `ParamBodyAngleZ` / `ParamBodyAngleX` | 侧倾 / 前后 |
| `head_tilt` / `nod` | `ParamAngleZ` / `ParamAngleY` | 歪头 / 点头 |
| `gaze_x` / `gaze_y` | `ParamEyeBallX` / `ParamEyeBallY` | 注视 |
| `hand_left` / `hand_right` | `ParamHandChangeL`（别名 `ParamHandDhangeL`）/ `ParamHandChangeR` | 官方 ID 有拼写笔误，因此别名表必须含社区写法 |
| `fringe` / `sidelock` | `ParamHairFront` / `ParamHairSide` | **且**必须出现在 `.physics3.json` 的输出里 |
| `skirt` | — | Haru 的物理输出只有 `ParamHairFront/HairSide/HairBack/ParamScarf`，**没有裙摆** |

- 物理振幅只改**确实被物理绑定**的 `fringe` / `sidelock`；`skirt` 永远不写。
- 表情只有 `F01–F08`，**没有** `neutral.exp3.json`；`NEUTRAL` 用参数默认值，不伪造文件。
- 动作只有 idle ×2、TapBody ×4；没有名为 celebrate/comfort 的动作组，意图靠参数与节奏表达，不虚构动作名。

## 第 15 条表格逐项核对

| 原始要求 | 本实现 | 差异 |
|---|---|---|
| CELEBRATE：眼 1.0、嘴 0.8、`BodyAngleZ` 15°、`HairFront`/`Skirt` +30% | 前四项照做；物理只加 `HairFront`（`HairSide` 同组） | **裙摆不存在**，工作台导入提示会列出 `skirt` 缺失 |
| COMFORT：弯眼 0.6、呼吸 0.4（12 次/分）、`BodyAngleX` −5° | 全部支持 | — |
| FOCUS_ACCOMPANY：注视灵敏度 0.4、`HeadAngleY` 2°、呼吸 0.55（16 次/分） | 全部支持（`ParamAngleY` 即“点头”通道） | — |
| IDLE_CURIOSITY：`HeadAngleZ` 10°、手部索引 2 | 支持 | 手部由 `ParamHandChange*` 承载 |
| REST_REMIND：眼 0.7、嘴型 −0.3、亮度图层 0.7 | 眼与嘴型照做；**“亮度图层”不是参数**，改为降低整幅合成亮度 | 模型没有可透明图层参数，不假装有 |
| NEUTRAL：重置默认 + `neutral.exp3.json` | 重置默认 | 该文件在原包中不存在，不伪造 |

## 第二个真实模型（仅测试）：Hiyori

`tests/models/hiyori` 是同一个官方标签里的原始资源，只用于证明引擎与指纹不是 Haru 专用：

| 语义 | Haru | Hiyori |
|---|---|---|
| 绑定通道数 | 17 | 17 |
| 物理可调 | `fringe`、`sidelock`（`ParamHairSide`） | `fringe`、`skirt`（`ParamSkirt`） |
| 缺失 | `skirt` | `sidelock` |
| 指纹 | `345409691644f780` | `7b8229cff8b84bba` |

同一份 `config/policy.json`（`breath/head_tilt/fringe/skirt`）在 Haru 上提示缺 `skirt`，
在 Hiyori 上全部匹配；这就是“策略存语义、导入时重新匹配”，而不是硬编码参数名。
Hiyori **不会**被打进发行包，运行时仍只接受逐文件校验的 Haru。
