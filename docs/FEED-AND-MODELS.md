# 投喂、气泡与模型库

这一篇只讲三件新事情：吃下什么、气泡放在哪里、以及换一张脸。

## 1. 投喂（`config/feed_limits.json`）

```json
{
  "schema": 1,
  "max_bytes": 52428800,
  "recycle": true,
  "failed_line": "没力气搬动呢…",
  "heavy_line": "这个太重了吃不下…"
}
```

* `max_bytes`：单份上限，默认 50 MiB。只读文件大小属性，**不会打开文件内容**；超过就摇头并说
  `heavy_line`（默认「这个太重了吃不下…」）。允许范围 1 KiB–8 GiB，越界或类型不对会退回默认值
  （`life.dream` 置位，行为等同配置损坏）。
* `recycle`：吃完后是否把这一份交给系统回收站。第一次真正生效前会弹一次询问：
  同意后才会调用 `IFileOperation::DeleteItem`（`FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NO_UI`），
  并用 `2026-11-18T11-51-09_原文件名` 重命名后在回收站里留痕。
  * 拒绝也完全可用：只记录名字/类型/大小/指纹，一个字节都不碰。
  * 失败（没有回收站、网络盘、权限不足、对象被占用）不会报错弹窗，会在气泡里说 `failed_line`
    （「没力气搬动呢…」），文件原封不动。
  * 右键菜单「投喂后把小点心移到回收站」可以随时改主意，选择会写进 `data/models.jsonl`。
* 其它文件永不改动；`recycle` 只对这一次投喂生效，重复投喂同一指纹不会再次搬运。

## 2. 气泡（`engine/layout.hpp`）

每次显示台词前，用**虚拟屏幕坐标**做一次纯几何计算：

1. 以当前监视器的 work area（排除任务栏）为边界；
2. 优先放在头顶上方，放不下改到下方，上下都不够就选空间更大的一侧；
3. 整块框被夹进 work area（`margin = 6`），箭头尖端再夹到框内 `pad = min(18, 宽/3)`；
4. 换回窗口坐标绘制并做最后一道窗内夹取，箭头始终指向角色。

所以多显示器、负坐标显示器、125%/150% DPI 下都不会半截出屏；代码里没有绝对像素偏移。

## 3. 模型库（`config/builtin_models.json`）

内置六套官方示例（来自 `Live2D/CubismNativeSamples` 4-r.7 的 `Samples/Resources`）：

| id | 名称 | 目录 | 说明 |
| --- | --- | --- | --- |
| haru | Haru | `assets/models/haru` | 默认，湿裙摆材质 |
| hiyori | Hiyori | `assets/models/builtin/hiyori` | |
| mao | Mao | `assets/models/builtin/mao` | |
| mark | Mark | `assets/models/builtin/mark` | 最精简（无表情、只有 Idle） |
| natori | Natori | `assets/models/builtin/natori` | |
| rice | Rice | `assets/models/builtin/rice` | |

每个条目固定 `manifest` / `moc` / `sha256`（MOC 指纹），并带一个
`model_adapter.json`（`cloth_part`、`expressions`、可选 `tracks`）。运行时只按这份清单加载，
清单可热更新；每个模型目录内的 `LICENSE.md`、`NOTICE.md` 都是上游原文。

* 右键菜单「模型」子菜单即可切换，切换耗时写在工作台（`已切换到 X · N ms`）。
* MOC 在加载前校验 SHA-256，之后每 5 分钟在内存里复核；不一致就退回纯规则并说
  「好像做了个奇怪的梦呢」。

### 导入模型

右键「模型 → 导入模型…」，选择任意目录下的 `*.model3.json`：

1. 只读取该目录里的描述文件与它引用的文件（`FileReferences`）；
2. 逐项核对 MOC、贴图、物理、表情是否都在，缺哪份就报哪份，例如
   「缺少 Haru.physics3.json」；
3. 通过后登记进 `data/models.jsonl`（绝对路径 + MOC 指纹 + 指纹域），并立刻切过去；
   **不复制、不移动、不修改**用户的模型文件；
4. 登记信息在下次启动时重新校验，目录被挪走或指纹变化就退回 Haru。

> 导入只是登记路径，因此请保留原目录；想移除某套模型，把 `data/models.jsonl` 里对应
> `kind:"entry"` 的那一行删掉即可。

## 4. 拖拽

桌面角色上只有**一个** OLE 落点（`native/drop.cpp`）：

* 文件：`.txt/.png/.cpp/.json` 视为投喂，`*.model3.json` 视为导入；
* 文本 / URL：不解析，一次困惑摇头；
* 有效悬停 → 主题色高亮环；无效 → 半透明遮罩 + 禁止符（深浅色主题各一套配色）；
* 取消、离开、`RevokeDragDrop`、进程退出都会走 RAII 清理，高亮不会卡住。
