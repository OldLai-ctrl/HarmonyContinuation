# dev.2 Cubase 人工验收 → RC Gate

当前版本保持 **0.8.0-dev.2**，代码 / 安装包提交 `eedbf78`；`d7f34c9` 仅补充开发回传文档。当前人工结果 **待回传**，不能据自动验证标记人工 PASS。

## 先安装

保存工程，关闭 Cubase，运行 [dev.2 Setup](../build-installer/output/HarmonyContinuation-0.8.0-dev.2-Setup.exe)。重新打开，在“更多 → 关于”确认版本 dev.2、Library 2；浏览 161 条进行，确认个人进行仍在。使用测试工程。

## 验收顺序与回传

文件集中放在 [manual-tests/dev2-midi](../manual-tests/dev2-midi/README.md)。其中三个基础文件来自上一阶段已通过的导出 / 导入用例；多轨、短经过音和失败文件为本轮人工验收准备，不预设 Cubase 结果。

| # | 回传项 | 操作 / 通过条件 | 当前状态 |
|---|---|---|---|
| 1 | More audition | 两种模式选未显示的候选；试听、再次点击停止、换候选替换、Why、保存 MIDI、快照可用；关闭详情不异常停声，内容对应所选候选。 | 待测 |
| 2 | Visible Continuation drag | 从继续发展主卡的 MIDI 按钮拖到 Cubase MIDI 轨。 | 待测 |
| 3 | More Continuation drag | 从继续发展更多详情的 MIDI 按钮拖到 MIDI 轨。 | 待测 |
| 4 | Visible Enrichment drag | 从升级进行主卡的 MIDI 按钮拖到 MIDI 轨。 | 待测 |
| 5 | More Enrichment drag | 从升级进行更多详情的 MIDI 按钮拖到 MIDI 轨。 | 待测 |
| 6 | Saved MIDI vs Drag MIDI | 同候选保存文件并导入，与拖出的 Part 对照；音乐内容一致，无需比较文件字节。 | 待测 |
| 7 | Basic MIDI Import | 更多 → 导入 MIDI → 完整片段，选 `pop_block.mid`；C–Am–F–G，持续 1 / 1.5 / 2 / 2.5 QN。 | 待测 |
| 8 | Inversion Import | `inversion_block.mid`；C–G/B–Am，G 根音和 B 低音分开，不能变成无关和弦。 | 待测 |
| 9 | 7th chord Import | `sevenths_block.mid`；Cmaj7–Am7–Dm7–G7，七和弦类别正确。 | 待测 |
| 10 | Complete vs OPEN | 同一个 pop 文件：完整模式最后 G = 2.5 QN；未完成模式最后 G = OPEN，之后可继续推荐。 | 待测 |
| 11 | Explorer Drop | 将 `.mid` 从 Windows Explorer 拖到插件当前进行时间轴；应与文件选择导入一致。 | 待测 |
| 12 | Multi-track | `multi_chords_melody_drums.mid`：应选原和弦轨，还原 pop，旋律与鼓不混入；高级信息的选轨合理。 | 待测 |
| 13 | Passing-note case | `short_passing_C7.mid`：C7 共 4 QN，中间 D 只有 0.05 QN；不形成大量新和弦。 | 待测 |
| 14 | Failure preservation | 先保留有效进行，再导入 `no_chords_melody.mid` / `malformed_truncated.mid` / `unsupported_smpte.mid`；原进行不丢失、不崩溃。 | 待测 |
| 15 | Constraint + imported MIDI | 导入后手动开启旋律约束，两种模式仍可工作；导入本身不自动创建约束。 | 待测 |
| 16 | Session Restore | 导入 MIDI，设置 Mode、Style、Tendency、旋律约束、Zoom，保存工程，关闭 Cubase 再打开；状态合理恢复。 | 待测 |
| 17 | zh-CN / English | 新增导入 / 拖出文案正常，无明显缺失。 | 待测 |
| 18 | Zoom | 100% / 125% / 150% 各看一次，新增按钮与入口可点击。 | 待测 |
| 19 | 发现的 bug | 报具体入口、输入文件、候选、操作与实际现象；复杂编曲识别保守可记 PRODUCT QUALITY。 | 未收到反馈 |
| 20 | 是否修复 | 有明确 BUG 时只修相关模块并定向验证。 | 暂无修复 |
| 21 | 是否允许进入 rc.1 | 关键人工项目无 blocker 后，统一执行最后一次自动回归，再决定推进 RC。 | 尚不允许，等待人工结果 |

四处拖出共用通过条件：生成 MIDI Part，不弹保存框；音符、时值、长度正确；继续发展包含 Existing + Recommendation，升级进行包含完整变换后的进行；取消 / 失败不崩溃。

回复可合并通过项，例如 `1–10 PASS；11 ISSUE：拖入没有反应；12–18 PASS`。失败附文件、截图或候选信息。当前表内“待测”只表示尚未收到结果，不表示已失败。

**Cubase 私有 MIDI Part 直接拖入仍 NOT IMPLEMENTED，不作为成功验收项；Explorer 文件拖入失败单独记录。**

本轮不增加功能、不调音乐算法。人工 Gate 通过前不重跑 CTest、Validator、基线、约束、导入 / 往返或 Demo 全套。基础 More、四处拖出、基础导入、转位 / 七和弦、OPEN、失败保留和工程恢复通过后，才集中回归一次并判断 rc.1。复杂 MIDI 的产品质量问题不阻塞 RC，除非影响基础和弦块工作流。
