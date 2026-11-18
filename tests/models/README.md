# 第二个官方模型（仅测试）

`hiyori/` 是 Live2D 官方 `CubismNativeSamples` 标签 `4-r.7`
（提交 `bd14cff46881660b582694da915568b638c3ec9e`）里 `Samples/Resources/Hiyori`
的原始资源子集：`.moc3`、`.model3.json`、`.cdi3.json`、`.physics3.json`、`.pose3.json`、
`.userdata3.json` 和两张 2048 图集。**没有**改动任何字节；`LICENSE.md` 原样保留。
动作与音频未收录，因为测试只验证指纹、语义匹配和真实网格渲染。

它**不进入** `PetDesk-*-Windows-x64.zip`：发行版仍然只接受逐文件校验的 Haru。
`tools/package.py` 的允许列表只写 Haru 与固定配置，`tests/` 从不打包。

用途：
- `python tests/compat.py` 用两个真实模型的描述符证明能力指纹不是 Haru 专用，
  并展示同一份“语义策略”在 Haru 上缺 `skirt`、在 Hiyori 上缺 `sidelock`，而不是写死参数 ID。
- `python tests/render_generic.py build/avatar_test <scratch>` 用同一个 CPU 内核分别解算
  两套 `.moc3` 网格，各输出三帧并校验篡改会被拒绝。

授权仍然遵循模型自身的素材条款；这里只作开发/验证用途，不做角色再分发包装。
