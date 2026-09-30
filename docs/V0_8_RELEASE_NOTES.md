# HarmonyContinuation 0.8.0

按用户授权将已验收功能正式封版为 v0.8.0。dev.2 Cubase 人工验收 **PASS**，RC1 集中回归与最终包 Validator 均通过。正式封版只更新版本与文档；正式版新构建本身没有新增 Cubase 人工测试记录。

## 用户功能

- 旋律音约束：Present / Top Voice，Hard / Soft，帮助候选适配指定音符。
- 和声倾向：稳妥 / 均衡 / 大胆。
- 界面缩放：100% / 125% / 150%。
- 独立本地 Factory Library；个人进行单独保存，安装、更新和卸载保留用户数据。
- Windows 完整安装器及独立曲库安装器。
- “查看更多”支持试听、MIDI 保存 / 拖出、Why 和快照。
- MIDI Drag-Out：拖出为精简音符片段；点击保存仍保留说明信息。
- 标准 MIDI 文件导入：SMF Format 0 / 1、PPQ，多轨选取和弦轨，区分根音与转位低音。
- 完整片段 / OPEN 导入；失败保留原有进行。
- 主界面七个候选分组独立滚动，约两条可见高度；候选数量与质量门槛沿用原有策略。

## 当前边界

- Cubase 私有 MIDI Part 直接拖入未实现；仅支持标准 MIDI 文件或可读取的文件路径。
- 音频和弦检测未实现。
- Preview 沿用既有输出方式。
- 部分推荐组允许为空；单和弦也可能完整实现当前意图。
- 三处既有结构质量提示保留，本轮未调整音乐算法。

## 数据与验收

Library Version **2**，SQLite Schema **1**，Session Schema **5**；Continuation / Enrichment Snapshot Schema **2 / 1**。

功能冻结基线 `58e765e69ef5cbaec3289e53729db40ce4a6d74d`：CTest **20/20**，Validator **47/47**；Continuation **42 例 / 214 候选**、Enrichment **30 例 / 161 候选**，路径、排序、分数及转位相关数据变化 **0**；Constraint **16/16**，MIDI Import **27/27**，Round-trip **12/12**，Demo MIDI / More / Scroll / Why **PASS**，Zoom **9/9**，Resize **96/96**。

证据位于 `build-v08/rc-gate-*.log`、`continuation-rc-gate.json`、`enrichment-rc-gate.json`、`rc-gate-comparison-summary.json`、`Testing/Temporary/LastTest.log`。最终 RC 包校验结果及确定提交记录在随包 `BUILD.txt`、`SHA256SUMS.txt` 中。

RC 后仅处理 blocker / correctness bug；不新增功能，不进入 0.9，不合并 main。
