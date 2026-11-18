## Haru 桌边 · 2.0 本地测试版（preview.2）

真实 **Haru**（Live2D Cubism Native Samples `4-r.7`，`bd14cff`）在 Windows 桌面上透明站立，模型原始 `LICENSE`、`readme.txt` 与全部资源逐字节保留。自带 MIT 授权的 **Purism Core** 直接在进程中解算 `.moc3` 网格，因此不用另行下载 Live2D 专有 Core；也可用获授权的官方 Core 本地重建。

### 这个 ZIP 里是什么
- `PetDesk.exe`：Win32 原生桌宠，无网页外壳、无运行时下载。
- `资产`/`模型`：官方 Haru 纹理、动作、表情、物理、动作采样表与原版权声明。
- 工作台：**概览 / 表达实验室 / 感知与隐私 / 画给我看 / 我的回忆**。第 15、16 项**分别开关**，默认关闭；启用前逐一列出调用的 Windows 函数并请求同意，拒绝后再次点选仍会询问。
- 感知默认全关：音量峰值、输入法语言、gamma 色温、洛谷事件、全局输入次数均需单独授权。天气只读本机 `config/weather.json`，不发送位置。
- 64×64 黑白画布；每张 512 字节位图转 base64，只追加到私有 `data/doodles.jsonl`，物理上限 20 张；徽章为本地矢量。

### 已在本次发布的 Windows 运行机上自动验证
1. Release 构建（MSVC x64）与 `rules_test` 34 组、`vault_test` 全部通过；额外 Windows 交叉编译检查。
2. 用真实 MOC3 + 官方图集渲染三帧不同的 Haru 网格；篡改 MOC 字节会被拒绝加载。
3. 解压 ZIP 后启动真实进程：命中测试只在角色椭圆内接住鼠标、透明留白不拦截桌面点击。
4. 右键菜单真实打开；系统文件选择器**打开并取消**（不写入任何食物记录）。
5. 通过真实 OLE `CF_HDROP` 拖放同一文件两次：写入两条元数据记录、原件字节不变、24 小时内不重复增加饱食度。
6. 右键长按拖动窗口后 `Esc` 回到桌边纯规则状态。
7. 打开工作台 → 表达实验室 → 点第 15 项出现精确数据源对话框；取消后再次点选仍会询问（未被静默启用）。画布连画 21 张：`data/doodles.jsonl` 物理停在 20 条。
8. 运行观测：空闲 CPU **0.97%**，活跃采样峰值 **1.87%**，峰值工作集 **68.3 MiB**（阈值 ≤3% / ≤8% / ≤120 MiB，4 逻辑 CPU 的 CI 机器）。正常退出后 `state.jsonl` 保留 70 饱食度。
9. 两个真实官方模型（Haru、Hiyori）在同一 CPU 内核上分别解算三帧；断言 Haru 只调制 `sidelock`、Hiyori 只调制 `skirt`，未绑定部件从不被写。Hiyori 仅用于测试，**不在 ZIP 内**。
10. PE 审计：x64、`asInvoker`、无额外特权、无被禁止的删除/改名/进程注入/剪贴板/ShellExecute 导入；Winsock 仅接受文档化的序号导入（socket/select/WSAStartup 等）。

### 明确未验证 / 不在包内
- Whisper 语音备忘：源码可选接入，但**本包不含权重或识别后端**，不会打开麦克风。
- 天气为**本机提供方适配器**（城市 + WMO 代码），不是联网气象 API。
- Windows Update 重启前的挥手依赖系统结束会话通知；强关无法保证时间。
- Windows Update 重启的问候只在 `ENDSESSION_CLOSEAPP`（更新/安装器重启）时设置，注销不再误触发；仍未在真实更新日验证。
- 未做官方 ONNX 量化校准 / 目标机 1000 样本漂移、十种来源模型指纹盲测、非技术用户盲测、VirusTotal 多引擎扫描；CPU/内存数字仅代表该 CI 机器。
- Haru 原包没有裙摆物理目标、中性表情文件或节日服装；程序不会伪造这些资源，界面会如实提示。

MIT 依赖与来源记录在 `LICENSE-Purism.txt`、`Runtime-origin.txt`、`LICENSE-json.txt`。模型授权请遵守 `assets/models/haru/LICENSE` 中 Live2D 的单独条款。
