# 桌边 · Haru

Windows 原生透明桌宠，另有可随时收起的本地工作台。**不是 Electron 界面，也不是贴图播放。** 普通用户启动，Haru 直接站在桌面上；右键才打开工作台。真实投喂只查询白名单文件的属性，绝不因为“吃过了”移走原件。

- **[下载 Windows x64 测试版](https://github.com/TNT-THT/-second-/releases/tag/petdesk-v2.0.0-haru-preview.1)** · [本地操作与测试](docs/LOCAL-TEST.md) · [已实现的边界和未验收项](docs/BOUNDARIES.md)
- 工作台：概览、意图实验室、感知与隐私、64×64 黑白画布、回忆徽章。第 15 / 16 项分别开关，默认关闭，Esc 回到纯规则。无需网页账号或在线对话。
- 官方 [Cubism Native Samples 4-r.7](https://github.com/Live2D/CubismNativeSamples/tree/bd14cff46881660b582694da915568b638c3ec9e/Samples/Resources/Haru) Haru 模型保持原样；[LICENSE](assets/models/haru/LICENSE) 和 [版权说明](assets/models/haru/readme.txt) 随模型和下载包分发。使用 [Purism Core](vendor/purism/ORIGIN.txt) 的兼容解算器直接驱动真正的 `.moc3`，不是 Live2D 官方专有 Core。官方 Core 也是可选的本地构建后端。
- 分开安装的[洛谷扩展](docs/LUOGU.md)与全局**次数**感知均须分别授权。无运行时下载，无剪贴板、文本或候选词读取。
- `engine/` 为可在 Linux/Windows 测试的规则、状态、意图 LUT、模型解算/栅格化；`native/` 是 Win32 窗口、工作台、私有追加日志、可选适配器。`tests/`、`tools/` 仅用于开发和 CI，不属于桌宠运行权限。

旧标签 1.x 仅作历史记录。**请勿下载旧版以测试当前的非破坏性投喂**；也不要混合两个版本的目录。
