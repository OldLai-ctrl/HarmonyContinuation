# dev.2 Cubase 人工验收 → RC Gate

**dev.2 Cubase Manual Gate：PASS（用户明确回传）**。用户确认拖出无额外 Marker Track / 标记、七组滚动、滚动候选试听与拖出、More Candidate、MIDI Import 及其它 dev.2 人工项目均通过，当前未发现新问题。集中回归已通过，版本推进至 0.8.0-rc.1。RC1 是新构建，尚未重新人工验收。

## 先安装

保存工程，关闭 Cubase，运行 [dev.2 Setup](../build-installer/output/HarmonyContinuation-0.8.0-dev.2-Setup.exe)。重新打开，在“更多 → 关于”确认版本 dev.2、Library 2；浏览 161 条进行，确认个人进行仍在。使用测试工程。

## 验收顺序与回传

文件集中放在 [manual-tests/dev2-midi](../manual-tests/dev2-midi/README.md)。其中三个基础文件来自上一阶段已通过的导出 / 导入用例；多轨、短经过音和失败文件为本轮人工验收准备，不预设 Cubase 结果。

| # | 回传项 | 操作 / 通过条件 | 当前状态 |
|---|---|---|---|
| 1 | More audition | 两种模式选未显示的候选；试听、再次点击停止、换候选替换、Why、保存 MIDI、快照可用；关闭详情不异常停声，内容对应所选候选。 | 用户报告 PASS |
| 2 | Visible Continuation drag | 从继续发展主卡的 MIDI 按钮拖到 Cubase MIDI 轨。 | 用户报告 PASS |
| 3 | More Continuation drag | 从继续发展更多详情的 MIDI 按钮拖到 MIDI 轨。 | 用户报告 PASS |
| 4 | Visible Enrichment drag | 从升级进行主卡的 MIDI 按钮拖到 MIDI 轨。 | 用户报告 PASS |
| 5 | More Enrichment drag | 从升级进行更多详情的 MIDI 按钮拖到 MIDI 轨。 | 用户报告 PASS |
| 6 | Saved MIDI vs Drag MIDI | 同候选保存文件并导入，与拖出的 Part 对照；音乐内容一致，无需比较文件字节。 | 用户报告 PASS |
| 7 | Basic MIDI Import | 更多 → 导入 MIDI → 完整片段，选 `pop_block.mid`；C–Am–F–G，持续 1 / 1.5 / 2 / 2.5 QN。 | 用户报告 PASS |
| 8 | Inversion Import | `inversion_block.mid`；C–G/B–Am，G 根音和 B 低音分开，不能变成无关和弦。 | 用户报告 PASS |
| 9 | 7th chord Import | `sevenths_block.mid`；Cmaj7–Am7–Dm7–G7，七和弦类别正确。 | 用户报告 PASS |
| 10 | Complete vs OPEN | 同一个 pop 文件：完整模式最后 G = 2.5 QN；未完成模式最后 G = OPEN，之后可继续推荐。 | 用户报告 PASS |
| 11 | Explorer Drop | 将 `.mid` 从 Windows Explorer 拖到插件当前进行时间轴；应与文件选择导入一致。 | 用户报告 PASS |
| 12 | Multi-track | `multi_chords_melody_drums.mid`：应选原和弦轨，还原 pop，旋律与鼓不混入；高级信息的选轨合理。 | 用户报告 PASS |
| 13 | Passing-note case | `short_passing_C7.mid`：C7 共 4 QN，中间 D 只有 0.05 QN；不形成大量新和弦。 | 用户报告 PASS |
| 14 | Failure preservation | 先保留有效进行，再导入 `no_chords_melody.mid` / `malformed_truncated.mid` / `unsupported_smpte.mid`；原进行不丢失、不崩溃。 | 用户报告 PASS |
| 15 | Constraint + imported MIDI | 导入后手动开启旋律约束，两种模式仍可工作；导入本身不自动创建约束。 | 用户报告 PASS |
| 16 | Session Restore | 导入 MIDI，设置 Mode、Style、Tendency、旋律约束、Zoom，保存工程，关闭 Cubase 再打开；状态合理恢复。 | 用户报告 PASS |
| 17 | zh-CN / English | 新增导入 / 拖出文案正常，无明显缺失。 | 用户报告 PASS |
| 18 | Zoom | 100% / 125% / 150% 各看一次，新增按钮与入口可点击。 | 用户报告 PASS |
| 19 | 发现的 bug | 报具体入口、输入文件、候选、操作与实际现象；复杂编曲识别保守可记 PRODUCT QUALITY。 | 当前无新问题 |
| 20 | 是否修复 | 有明确 BUG 时只修相关模块并定向验证。 | 两项 UX 修复已通过复测 |
| 21 | 是否允许进入 rc.1 | 关键人工项目无 blocker 后，统一执行最后一次自动回归，再决定推进 RC。 | Gate 通过，推进 rc.1 |

四处拖出共用通过条件：生成 MIDI Part，不弹保存框；音符、时值、长度正确；继续发展包含 Existing + Recommendation，升级进行包含完整变换后的进行；取消 / 失败不崩溃。

回复可合并通过项，例如 `1–10 PASS；11 ISSUE：拖入没有反应；12–18 PASS`。失败附文件、截图或候选信息。上表 PASS 来自用户的 dev.2 人工回传，不代表 RC1 新安装包已经复测。

**Cubase 私有 MIDI Part 直接拖入仍 NOT IMPLEMENTED，不作为成功验收项；Explorer 文件拖入失败单独记录。**

本轮集中回归已完成一次且通过。仅更新版本与 RC 文档，RC1 最终包单独核对资源及 Validator，完成后停止。
