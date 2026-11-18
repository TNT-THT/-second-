# 本版边界与复现

| 范围 | 已实现的路径 | 不能据此声称的事 |
|---|---|---|
| 角色 | `4-r.7` 的真实 Haru、原纹理、动作曲线离线采样后逐帧插值，实时参数和裁剪网格；Purism Core（MIT）静态链接。也可显式提供获授权官方 Core 头文件/静态库重建。 | 官方示例仓库没有专有 Core 二进制。Haru 不是原先设想的 6.5 头身双马尾定制稿。裙摆 24 骨、湿度专用原画、唐装和万圣节模型不是原包资源；雨天使用服装部件材质变暗，节日有轻量效果/声音/动作，非完整新服装。 |
| 事件意图 | 不输入聊天或语音正文；6 位已授权事件特征，作者定义的 FP32 ONNX 分类器，64 种可能输入**全部**用 ONNX Runtime 离线编译为标签/置信度 LUT；未做有损量化，不需要随程序加载 ORT。1000 条复核比较同一输入的概率。识别关而反馈开时使用规则事件。 | 不是自然语言大模型；合成校验不是用户研究、目标机器量化校准或 ASR 准确率。 |
| 泛用反馈 | 逐模型能力指纹；语义别名、物理白名单、实时 Core Min/Max、插值包络、富表达/面部/单通道/纯节奏四档。工作台限幅。 | `.model3.json` 并不存参数上下限。`neutral.exp3.json`、`ParamHeadAngleY`、`ParamSkirt` 等不能凭表格假设存在。新模型元数据可以离线检查，发行程序默认只接受锁定的 Haru。未做十个独立来源的盲测。 |
| 作息、电源 | 以启动时当地时间锚定 `GetTickCount64`，持续运行时调系统时钟不改变 06–07 的晨眠周期。读取 `GetSystemPowerStatus`；确认非注销系统结束会话才设下次问候，预告不阻止 Windows 关机。 | 新进程会重新锚定，无法预测任意重启，更不能声称能分辨所有 Windows Update 重启原因。系统立即强关可能没有挥手时间。 |
| 天气 | 可选**本机提供方适配器**：后台读取 `config/weather.json`，形如 `{"city":"洛杉矶","code":61}`。成功结果在 `data/weather.jsonl` 缓存 24h，失败沿用旧值。0=晴，51–67/80–82/95–99=雨。 | 没有通用的 Win32 离线城市实况天气 API。未抓取 Windows 小组件、未发定位或城市到第三方；无本机提供方就是“无数据”，不是自动联网气象查询。 |
| 声音/输入法/色温 | WASAPI 活动播放会话、峰值间隔稳定后估拍；IME 只查语言及开关/转换模式；gamma 表在 sRGB 假设下估算白点并在 1.5s 内过渡。 | 峰值不是音乐分类，gamma 不是校色仪。受限 TSF、硬件滤蓝光或没有音频端点时可能无信号。不会截屏取色、访问候选词或循环录音。 |
| 语音备忘 | 源代码可选 whisper.cpp 1.7.6 内存接口、显式许可、单次按住、单声道 16kHz/≤10s、取消/清零、私有哈希去重。 | 常规包**不含** Whisper；该模型的识别、内存和 CPU 尚未通过同一台机器的预算验收，不以空权重或云识别代替。开启其他感知不等于授权麦克风。 |
| 性能/安全 | 自有代码 I/O 边界、独占锁投喂哨兵、硬链接拒绝、真实 PE 导入和清单审计；测试随每个候选包运行。仅 CPU 网格绘制，效果图集≤64×64/12帧。 | 导入扫描不是“绝对无害”证明，也不等价于 VirusTotal 检测。DWM 合成显存受机器影响。不要把一个 Windows CI 机器上的 CPU/RSS 视作所有 GPU/高 DPI 或启用 ASR 的测量。 |

**开发复现**：CMake 3.20+，Windows 10/11 x64 + VS 2022。`cmake -S . -B build -A x64`，`cmake --build build --config Release`，`ctest --test-dir build -C Release --output-on-failure`，`python tools/package.py build/Release/PetDesk.exe`。开发 HUD 需单独 `-DPET_DEV=ON`，发行版不带其热键或状态读取。Linux 可编译规则和同一 CPU 模型内核，`python tests/render_haru.py build/avatar_test`（测试工具需要 Pillow），`python tests/compat.py`，`python tools/audit.py`。

`python tools/model_compatibility_check.py assets/models/haru/Haru.model3.json --policy config/policy.json` 列出匹配/缺失/物理目标。`tools/haru-lock.json` 保留逐文件 SHA-256；打包校验所有原件和原始声明。三类残缺描述符放在 `tests/fixtures/`，不会打包成角色。`tools/bake_tracks.py` 可复现动作表。ONNX 编译器 `tools/compile_intents.py` 需要开发机安装 numpy、onnx、onnxruntime；桌宠不需要。

可选 SDK 构建：向 CMake 传 `CUBISM4_INCLUDE` 和 `CUBISM4_LIBRARY`（同版本 x64 /MT，遵守 Live2D 条款）。不要上传专有 SDK。实验性离线语音构建：`WHISPER_CPP_DIR` 指向已合法取得的 v1.7.6 源码，`WHISPER_TINY_SHA256` 固定已核验的多语言 tiny GGML 权重摘要；仅从 `assets/whisper-tiny.bin` 读该模型。必须重新进行发行包导入审计和目标机资源验证，不属于本次标准 ZIP。
