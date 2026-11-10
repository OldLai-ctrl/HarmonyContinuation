# G13-C 产品集成与测试构建

> 后续正式交付：v0.11.0已发布，Tag/BuildCommit `d888e2c`；本篇保留dev.16实现及定向证据。正式身份和一次隔离升级结果见[NOW](../NOW.md)与[发布说明](V0_11_RELEASE_NOTES.md)。

## 当前结果

G13.1三条研究试听已获得用户正面评价，用户批准C阶段。本轮先审查并保存G13.1为 `b1b855d`，再建立独立 `development/g13c-product-integration-20261109`。产品集成代码提交 **`7b4000e364dc3506bd097f07319e7e67c2b67054`**，测试版本 **0.11.0-dev.16**，Factory V8 / 1,288条。dev.15研究版本保留，故使用dev.16。

同级 `HarmonyContinuation-g13c-test-20261110/` 提供可直接打开的Demo、独立VST3、运行依赖、输入MIDI、生成媒体和最小宿主清单。它未覆盖已安装dev.14；没有Setup、推送、合并、Tag或发布。最终文件哈希见该目录 `BUILD_INFO.json`。

工程集成及本轮局部检查通过。用户已确认Cubase与FL Studio最小加载、生成试听及MIDI操作检查通过，具体版本未提供；产品CC0先验音乐已获正面评价并批准直接发布v0.11.0。本记录保留dev.16工程证据，正式构建与发布进度见[NOW](../NOW.md)。

## 调用与来源

- `RecommendationWorker`在原后台线程完成数据库推荐后，调用 `generation::appendProductGeneration`。沿用原查询/分析缓存、代数token取消和过期结果拒收；没有新线程、全局可变状态或第二套生命周期。生成不在音频线程执行。每次使用实际输入、约束与倾向；先验为本进程固定的只读构建数据，未增加可能串用profile的结果缓存。
- 每档最多1条，最多3条。每档沿用规划上界4,096和实际和弦搜索上界16,000，三档默认总上界60,288个检查节点。取消在各档和搜索层检查；无合格路线就不增加该档候选。
- 数据库候选的内容、分数和相对次序保留，生成结果插在原数据库Top3之后。灵感完整列表固定每页10条，Top3仍按前三个可见合格项标记。匹配优先保留独立数据库结果；生成条目只进入灵感视图。
- `ContinuationCandidate::generated`是独立生成来源，含生成器版本、先验身份/许可、常规/丰富/大胆层次、音乐依据与实际低音MIDI。Factory ID为空，不赋予Factory作品来源或人工审校资格。界面显示“算法生成”，不把生成结果的内部占位分数显示成Unified评分。

## 演奏与冻结

通用 `preview::buildSequence`在构建实际E时使用逐事件 `bassMidi`，约束切分保留该音区。MIDI、拖放和离线音频继续共享原序列/配音实现。`PreviewTransport::preparePreview`亦改用共享 `voiceEvent`，使处理器实际音频后端获得同一低音，而不是只在离线导出保留音区；没有显式低音计划的旧路径保持原配音。

实际演奏指纹只为生成候选额外包含低音MIDI；先验ID、解释文字等元数据不改变声音身份。旧数据库候选指纹格式保持。

Snapshot写入升级为schema4，冻结实际音集、低音、时值、约束与生成来源。读取schema1/2/3；生成快照无需Factory引用即可恢复，不重新运行生成器。schema4读取方校验字段长度、事件数、低音范围和音级对应关系。旧dev.14不具备schema4读取能力。Session继续schema5，固定对象按实际演奏指纹恢复，旧Session读取不变。

## 数据许可与音乐范围

产品构建只嵌入 `data/generation-priors/billboard-cc0.tsv`，先验ID为 `billboard2-cc0-dedup740-v1`。源为[McGill Billboard官方CC0标注](https://ddmal.ca/research/The_McGill_Billboard_Project_(Chord_Analysis_Dataset)/)，聚合方法及条件见[语料记录](G131_CORPUS_GRAMMAR.md)。程序算法和统计文件分开维护，产品没有研究先验注入选项。

IdolSongsJp与RWC派生统计保留在独立非商业研究目录，未嵌入VST/Demo或本次可分发先验。已有正面评价对应Idol研究profile，不能自动授予CC0产品profile；本轮通过旧三条冻结JSON/MIDI，在新Preview和Snapshot路径逐音符复核演奏身份，未重做研究生成与大规模音乐验收。

| 实际输入 | CC0产品生成结果 | 当前解释与限制 |
| --- | --- | --- |
| F–G–Em–Am | 常规：Dm(4)–G(4)–C(4)–F(4)；丰富：Dm7/A(4)–G7(4)–Fmaj7(2)–C/E(2)–F(4)；大胆：Dm7/A(4)–G7/B(4)–C(4)–C7(2)–Fmaj7(2) | C大调解释；均为正常首选，音乐待用户评价 |
| Am–G#dim–G–D/F# | 丰富：E7(2)–Amaj7/E(2)–Dmaj7(4)–A/C#(4)–D(4)；大胆：A(4)–A7(2)–D/A(2)–Amaj7(4)–Dmaj7(4) | 选择A大调解释，前句Am与后文A大调色彩的实际接缝需听评；常规档无合格完整路线，未增加兜底结果 |

括号为QN。原输入JSON将F#写作Gb，音高相同。当前规划支持所选大调解释；不把大小调多解或第二输入的色彩重解释宣称为已建立转调。5条完整D→E、5份MIDI和Snapshot及2份输入MIDI在测试包 `examples/`，定位清单在包内README。

## 最低必要证据

同级 `HarmonyContinuation-g13c-evidence/`：

| 检查 | 实际结果与范围 |
| --- | --- |
| `integration-checks.txt` | **414项通过**：两输入真实worker；数据库Exact身份及相对排序、取消/过期拒收、缓存复用、3/2条实际生成、Preview/MIDI/处理器预配音、Snapshot4、Session5固定对象、旧已验收三条冻结音乐逐音符复核 |
| `snapshot-midi-checks.txt` | **606项通过**，复用MidiExportTests，含新增来源/音区及非法metadata拒绝 |
| `legacy-snapshot-checks.txt` | **75项通过**，仅LibraryCompatibilityTests的snapshot分组，保留旧格式读取 |
| `generated-ui-fg-em-am.txt` | 原生离屏**PASS / Exit0**：导入真实输入MIDI后的UI前10项；数据库Top3在1–3，生成三档在4–6；固定分页、来源标签、Why/详情依据、直接演奏路由和音频/MIDI按钮可用 |

G13.1的198项定向检查及152项媒体读回沿用，没有重跑。未运行全量CTest、RCR-3、大规模音乐基准、dev.15 A/B或无关安装生命周期。

### 本轮定位的UI启动缺口

1. Demo离屏绘制曾AccessViolation。WER RVA `0x15a136`经链接map定位到VSTGUI `D2DBitmap(const CPoint&)`；拥有者源码 `vstgui/lib/platform/win32/direct2d/d2dbitmap.cpp`在构造时调用WIC工厂，`win32factory.cpp`通过CoCreateInstance建立WIC。独立Demo此前未初始化UI线程COM，导致空工厂。Demo入口现成对调用OleInitialize/OleUninitialize，插件宿主路径未改。修复后同一路径正常执行；失败阶段日志仍保留，临时map构建选项已移除。
2. Demo接收真实worker结果时，`setWorkerStatus`未传第六个resultReady参数，默认false使界面一直pending。已显式传true，与插件正常结果路径一致。
3. 历史完整Compact smoke要求六入口，与当前四入口不匹配；完整G7导航harness也有旧动作未通过。本轮保留失败记录，使用明确命名的 `--g13c-ui-smoke`聚焦既有Browse检查，不宣称完整旧导航harness通过。Browse的模拟分页fixture现明确在Match模式运行，生成标签断言检查整行而非只搜索ID之后。

主代理负责产品许可、来源身份、演奏与后台接线、实际构建/核验；辅助代理负责Snapshot实现及UI首版。最终问题由主代理接管定位并核验。本轮使用本地历史检索、适用表达规则及范围控制技能；技能不替代实际检查或宿主验收。
