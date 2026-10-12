# HarmonyContinuation Phase 4B：节奏语义与和声检索解耦审计

日期：2026-10-11
范围：复核推荐、宿主、MIDI 与输出代码；复用 Phase 3/3A 已保存结果；运行现有相关测试。未修改生产逻辑、Factory 数据或测试源码。

## 结论摘要

1. **BPM 不进入当前和弦识别、候选匹配或推荐评分。** BPM 只从 VST3 宿主上下文读取，用于试听换算和 MIDI 输出元数据。MIDI 文件导入时虽然保存了 tempo 事件，但推荐控制器没有把文件 tempo 设为试听/导出速度；该速度来自宿主最近一次有效 BPM，初始值为 120。
2. **QN 时值与拍号会间接影响和声检索。** 输入 MIDI 先把 tick 换算成 QN；分析以相对时值和小节起点修正结构权重。结构权重跨过 0.55 后，事件角色、插入成本、FULL/SKELETON 对齐和最终排名都可能变化。
3. **R01 的既有实测是 QN/小节相位效应，不是 BPM 效应。** Phase 3 的变形函数把 tempo 固定为 120 BPM，只把事件起点与时值乘以 0.5 或 2。R01 的 2QN 情况由 5 条续写路径变成 8 条，V4_A08 从 57.403870 升至 61.722977 并显示；8QN 回到基线。把原数据列名 tempo_half/tempo_double 当成 BPM 试验会误读证据。
4. **拍号支持只覆盖每小节 QN 长度，不覆盖节拍分组或拍号变化地图。** 代码能计算 3/4、6/8、5/4、7/8、9/8 的 bar length，但 7/8 的 2+2+3 与 3+2+2 无法区分。MIDI 导入只将第一个拍号写入会话；VST3 只提供当前一个拍号。
5. **本轮没有多拍号下的实际推荐引擎差分结果。** 现有推荐诊断入口固定 4/4、120 BPM；为遵守“不额外编写工具”，本报告将多拍号表格标为代码推导，未伪装成运行结果。已有 R01 时间缩放证据、源代码链路和 8 个现有测试程序已核实。
6. 证据足以选择下一步离线原型方向，但不足以声称解决 R02 的真实 Cubase 差异或通过多拍号宿主验收。

## 1. 基线与证据标签

| 项目 | 当前核实 |
|---|---|
| 分支 / HEAD | library/v4 / 42a915bda447457bd1bdc8b2f79a90e5ddc69c00 |
| Factory V4 | 657 条 |
| 构建数据库 SHA-256 | 74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534 |
| Phase 3A 减和弦补偿 | 已有生产源码改动，开关默认 false；本轮未启用 |
| 本轮生产行为 | 未改变 |
| 本轮新增文件 | 仅本报告；没有新增测试代码、脚本或诊断工具 |

证据分级：

- **代码事实**：由当前源码路径直接确认。
- **既有实测**：Phase 3 已保存的稳定性表和报告；不代表本轮重新跑过。
- **本轮测试**：本轮直接运行的现有测试程序。
- **代码推导**：依据当前公式计算；没有运行推荐引擎，不能据此报告候选数、最终分数、Color 或 MIDI 字节。
- **待宿主验证**：需要 Cubase 实际 ProcessContext、输入数据和插件结果。

依赖关系简图：

```text
MIDI tick / PPQ → QN 起点与时值 → MIDI 和弦识别 → 结构权重 / skeleton → Factory 对齐与插入成本 → 可续写检查 → 分数门槛 / 去重 / Top-K → UI
宿主拍号或 MIDI 第一条拍号 → AnalysisContext → 小节起点加权 ────────────────────┘
宿主 BPM ─────────────────────────────────────────────→ 试听秒数与 MIDI tempo 元数据（不进入检索）
输入与 Factory 的 QN 时值比例 → rhythmScore / continuation rhythmScale → 排名及输出 MIDI QN 时长
```

## 2. 节奏与和声检索依赖关系

推荐链路可概括为：

1. SMF note ticks 按 PPQ 转成 startQN/durationQN。
2. MIDI 抽取器按 QN 起点容差与最短持续时间合并、分段和识别和弦；VST-XML 输入则直接给出和弦身份与工程 QN。
3. 会话从 MIDI 文件第一条拍号事件或 VST3 当前宿主上下文取得拍号。
4. HarmonyAnalysis 结合和弦功能、相对时值、端点/终止/借用信息和小节起点，生成 structuralWeight 与 skeletonIndices。
5. 0.55 条件改变 Structural/Embellishing 角色。makeMatchQuery 把该角色与时值送入 ProgressionMatcher。
6. 匹配器计算 FULL/SKELETON 对齐、gap cost 和和弦相似度；minimumMatch 为0.60。
7. 续写引擎检查有效 Factory 尾部、V4 显式低音/核心音冲突，再算 rhythmScale 与候选分数。
8. 节奏分占总排名8%；总分60、diversityPenalty、每意图组最多3条及UI展示策略可能淘汰或遮住候选。
9. Preview/MIDI 使用候选 QN 时长、宿主 BPM 和会话拍号；BPM影响秒数及文件元数据，不进入检索查询。

| 阶段 | 实际字段与位置 | 对检索/输出的作用 |
|---|---|---|
| SMF 读取 | src/midi/StandardMidiFileReader.cpp::readFromMemory，notes 的 start/duration 由 tick/PPQ 变成 QN；tempo 与 meter 事件分别保存在列表中 | tempo 不参与 note 的 QN 换算；文件在同样 tick/PPQ 下只改 tempo meta，和弦识别输入保持相同 |
| MIDI 和弦分段 | src/midi/MidiHarmonyExtractor.h 默认 onsetToleranceQN=0.125、minimumPersistenceQN=0.125；MidiHarmonyExtractor.cpp::extractHarmony 用起点差、持续音和 slice 长度分组/抑制 | QN 的绝对数值会影响极短音符的并组、持续音证据和识别；BPM 元数据本身不会改变该计算 |
| OPEN | extractHarmony 最后把末和弦 duration 清空并设 openEnded；HarmonyAnalysis::medianDuration 只统计已知正时值 | OPEN 端点仍被保留为骨架端点；它减少节奏样本，且在较短或不均匀输入中可能改变用于其他和弦的中位时值 |
| 调性证据 | HarmonyAnalysis.cpp::scoreKey 使用 durationFactor=本和弦时值/已知时值中位数，限制在0.5–1.5 | 同比例缩放时不变；不均匀时值可改变自动调性证据，继而改变调内功能和候选匹配。拍号不参与此步 |
| 结构权重 | HarmonyAnalysis.cpp::structuralWeight：相对时值 log2 项、和弦功能、端点/终止/借用角色、短次属/趋近惩罚、barStartEffect | 相对时值项是归一化的；小节项按 startQN 对 barQN 取余，是当前拍号进入和弦检索的直接入口 |
| 骨架分类 | analyzeHarmony：端点、明确配器/低音、终止角色或 structuralWeight≥0.55 即进入 skeleton；否则加 Embellishing | 0.55 是离散边界。结构权重的微小变化可能变成角色变化，而不只改变一个辅助分数 |
| 匹配 | makeMatchQuery 将 structuralWeight、roles、durationQN 写入 MatchEvent；V4 模板保留所有骨架节点时，matchProgression 对结构视图也使用完整 query | 结构分类改变 gap factor 与角色相似度；因此即使模板匹配窗口不变，Match 和 Skeleton 两条对齐分数也会改变 |
| 候选打分 | ContinuationEngine.cpp::recommendContinuations 的分数含 rhythm 8%；Match 34%、Skeleton 11% 等 | Match 的0.60门槛在节奏子分之前；总分60门槛在节奏子分之后。因此节奏不会直接让 minimumMatch 失败，但最多可通过总分贡献的8分使候选未过60 |
| 输出时值 | estimateRhythmScale 用匹配和弦的 QN 比值估比例；续写时值为 Factory QN 时值×比例 | 节奏可改变推荐 MIDI 的 QN 长度；BPM改变这些 QN 的秒数，不改变和弦音高或 Factory 路径 |
| 去重和展示 | suffixFingerprint 的精细签名包含量化后的 QN 时值；最终分数还经过 diversityPenalty 与每组最多3条 | 时值可以影响候选是否合并、排序及可见位；时值不只出现在一个节奏子分里 |
| Color UI 排序 | MainView.cpp 调用 ColorPreferenceReranker；该模块对已有候选按相对时间位置/时值评分 | 只重排已生成的候选，不会召回在匹配、续写或评分门槛前消失的 Color 条目；默认关闭时不做该重排 |

补充代码事实：

- ProgressionMatcher.h::MatchWeights 有 rhythmWeight=0.18 字段，但当前 ProgressionMatcher.cpp 没有读取它。对齐时单独计算 rhythmScore；MatchResult.similarity 由 full/skeleton harmony 与 key prior 组成。
- 当前 Factory 为657条，小于 CandidateIndex::shortlistLimit=800，所以本库运行时 shortlist 会纳入所有条目。rhythmShape 被生成，但 fingerprintScore 不用它；本版本没有因该指纹的节奏特征造成 Top-800 召回漏失。
- Factory 单条的时值仍参与对齐的全局节奏比例、节奏子分和续写时长；Factory 的 meter 字段不参与 ProgressionMatcher 的输入拍号兼容判断。
- MatchQuery 保存 timeSigNumerator/timeSigDenominator，但除赋值外，匹配器不读取这两个字段。拍号通过前置 HarmonyAnalysis 间接影响匹配。

## 3. R01 的等价输入与已有实测

R01 合成音符在4QN基线下识别为 Am–Abdim–G–D/Gb，起点/时值为0+4、4+4、8+4、12+4。2QN 为0+2、2+2、4+2、6+2。历史生成器固定 tempoBPM=120、meter=4/4，再缩放 QN 起点与时值。

| 既有变形 | BPM / 拍号 | 识别 | 通过续写路径检查数 | V4_A08 相似度 | V4_A08 总分 | 通过60分门槛 / 引擎输出数 |
|---|---|---|---:|---:|---:|---|
| 4QN 基线 | 120 / 4/4 | 4/4 和弦 | 5 | 0.674117 | 57.403870 | 0 / 0 |
| 2QN，CSV 标名 tempo_half | 120 / 4/4 | 4/4 和弦 | 8 | 0.769499 | 61.722977 | 1 / 1 |
| 8QN，CSV 标名 tempo_double | 120 / 4/4 | 4/4 和弦 | 5 | 0.674117 | 57.403870 | 0 / 0 |
| 3+5QN | 120 / 4/4 | 4/4 和弦 | 5 | 0.674757 | 57.161598 | 0 / 0 |
| OPEN 末和弦 | 120 / 4/4 | 4/4 和弦 | 5 | 0.674150 | 56.405006 | 0 / 0 |

4QN 时 q1 G#dim 的相对时值项为0；它是 A 小调语境中的 Unknown、非端点、非终止、非借用。其权重为：

0.66 - 0.12 + 0.05 = 0.59

这里的 +0.05 来自 q1 的 startQN=4 恰为4/4小节起点。2QN 时 q1 startQN=2，不在小节起点：

0.66 - 0.12 = 0.54

因此 0.55 分类从 Structural 变为 Embellishing。V4_A08 轨迹和 Factory 窗口未变；QueryInsertion 的 gap factor 从普通结构插入变为 embellishing 插入。现有 gapCost 中插入成本由0.609600变为0.211536。完整与结构对齐在该 V4 条目上都使用完整查询，二者的分数一起升高。候选节奏子分保持0.875，证明 2QN 的过线不是直接节奏加分。

这不是“拍号真的改变和声”的证据，而是输入 startQN、小节相位与离散分类共同造成的检索变化。当前公式假设 QN=0 是小节网格原点，并用当前一个拍号贯穿整段。

### 其他等价条件的区别

| 比较方式 | 能否从当前代码得出 | 对 R01 的解释 |
|---|---|---|
| 固定 QN note events，只把 BPM 从90改180 | 检索输入字段不含 BPM；候选、分数、Color应不变，输出秒数减半 | 代码事实；本轮没有做成对运行 |
| 90 BPM/2QN 与180 BPM/4QN，保持真实秒数相等 | 两者起点/持续秒数可以相同，但 QN 与小节相位不同 | 4/4 下2QN q1在小节内、4QN q1在小节起点；当前结构分类会不同。既有 2QN/4QN 运行并非此 BPM 成对实验 |
| 固定 BPM，所有 QN 起点/时值同比例缩放 | 已有实测，候选集合不稳定 | R01 0.5 缩放导致路径5→8、显示0→1；2倍缩放恢复基线 |
| 只改变拍号，不改 QN | 公式可精确推导；没有推荐引擎运行 | barQN 会改变哪些事件得到0.05结构加分；最终候选结果尚未实测 |
| 转位/八度 | 已有4/4、4QN矩阵 | octave up/down 与 inversion 的 R01 识别均4/4，V4_A08分数和可见数不变；没有跨拍号变形 |

R01 分数来源与耦合：Match 从0.674117到0.769499，Skeleton从0.689446到0.787274；节奏分不变。Match权重34%带来约+3.243分，Skeleton权重11%再带来约+1.076分；总计约+4.319分。对该 V4 模板，FULL 与结构视图实际上走同一查询，两个相似度数值相同；最终 Match 又包含68%结构相似度，之后还单独加11% Skeleton。因此一次 gapCost 变化会在两个最终评分项中重复体现。它是已证实的数学重叠；是否构成不合理重复评价，仍需以正负音乐样本和听觉评审判断。

OPEN 的差异并非拍号变化：OPEN 末项没有持续时值，不进入时间比值样本，历史 R01 节奏子分由0.875降到0.750，约减少1个总分点；该项仍保留为端点骨架。

## 4. 多拍号支持与 R01 代码推导

HarmonyAnalysis 以 barQN = numerator × 4 / denominator 计算每小节四分音符数。对固定4QN R01，G#dim 的起点为4QN；它的无时间基值仍为0.54。下表只计算是否添加 barStartEffect：

| 拍号 | barQN | q1 G#dim startQN=4 是否小节起点 | 推导结构权重 | 0.55分类 |
|---|---:|---|---:|---|
| 4/4 | 4 | 是 | 0.59 | Structural |
| 3/4 | 3 | 否 | 0.54 | Embellishing |
| 6/8 | 3 | 否 | 0.54 | Embellishing |
| 5/4 | 5 | 否 | 0.54 | Embellishing |
| 7/8 | 3.5 | 否 | 0.54 | Embellishing |
| 9/8 | 4.5 | 否 | 0.54 | Embellishing |

同一输入的 G 在 startQN=8，A 小调下有 borrowed 角色的0.04；在上表所有拍号中仍会保持高于0.55（4/4为0.63，其余为0.58）。Am 与末尾D/Gb由端点条件保留。这里仅能推出 G#dim 事件在输入侧的分类可能改变；**没有运行这些拍号下的候选数、Color、排名或 MIDI**。

从当前字段能力判断：

- HostContextAdapter 支持分子1–64、分母为2的幂且不大于64；包含所列拍号。
- HarmonyAnalysis 只使用每小节QN长度，不表示拍组、强拍/弱拍或重音。
- 3/4 与6/8在当前计算中具有相同 barQN=3，无法分别表达两者常见的强拍组织。
- 7/8 的 2+2+3 与3+2+2都只有7/8，不同分组对当前代码完全不可见。
- Cubase ProcessContext 只保存当前拍号一对数字；没有每小节分组或时间变化表。
- SMF reader 保存有序 meter 事件，但 Controller::applyMidiImport 仅采用 meters.front()。它没有按每个和弦时间点选择对应的meter。
- 若 host 没有提供拍号，HarmonyAnalysis 的拍号字段为空且不会默认加4/4小节分；但导出、快照等若无值，会以4/4作为默认元数据。MIDI 导入没有 meter 事件时，next 从已有 session state 复制，代码不会明确清空旧拍号，存在沿用旧值的可能。

## 5. BPM、时值、拍号及输出能力现状

### BPM

- VST3 Processor 请求 tempo 与 time signature；HostContextAdapter 对有效 BPM 做20–400范围验证。
- Controller 更新 lastTempoBPM_，初始120；搜索任务只接收和弦、AnalysisContext 与推荐选项，不接收 BPM。
- PreviewSequence 的 QN→秒换算为 QN × 60 / BPM；PreviewSynth 与 MIDI 导出使用这一速度。
- MIDI writer 写入一个起始 tempo meta event；不保存 tempo map。MIDI reader 能读 tempo event list，但推荐控制器未用导入文件 tempo 更新 lastTempoBPM_。
- 所以同一 QN 输入只改变 BPM，不应影响核心和弦候选；试听秒长及输出 MIDI tempo 元数据会改变。这个核心不变性来自当前调用字段，不是本轮成对运行结果。

### QN 绝对值与相对值

- **绝对 QN 依赖**：MIDI 抽取的0.125 QN onset tolerance / persistence，绝对 startQN 对 barQN 的整除；OPEN 缺时值；有效长度限制。
- **相对 QN 依赖**：key evidence 的 duration/median、structureWeight 的 log2(duration/median)、短趋近音阈值，以及匹配后全局 rhythm scale。
- **全局缩放原则**：若所有时值同比例改变，duration/median 与对齐的全局比例残差通常不变；但 startQN/barQN 的小节相位并不会自动同步缩放，所以 R01 的分类依然可能改变。
- **节奏淘汰位置**：节奏相似度不直接参与0.60 minimumMatch；候选总分仍把8% rhythm 放入60分门槛，故它可间接导致minimumScore淘汰。结构分类变化则可直接改变相似度并在minimumMatch阶段淘汰。

### MIDI 输出

- 推荐 Factory 路径的音高/和弦来自原始条目；输入节奏可通过 rhythmScale 改变续写 QN 时值。
- buildClip 把选定 meter 写入序列；SMF writer 使用 QN 起止位置生成 note tick，tempo 与 meter 作为文件元数据写入。
- 输出不按 3/4、6/8 或不规则拍号重新量化和弦位置，也不会重组7/8节拍；它保留 QN 时间轴并写一个拍号标记。
- 本轮跑了现有 MIDI Import/Export 和 Preview 测试，但没有比较同一候选在不同 BPM/拍号下的导出文件。故不能报告音符字节一致或真实 DAW 秒数验证。

## 6. R02、B08/B11 与 Color

### R02 已有证据

- Phase 3 四组 R02 合成输入均固定4/4、120 BPM；四组有5条 Resolve/Loop 路径，没有 Color。
- 之前的固定语境诊断中，C大调 Color 条目最高相似度为0.581427，低于0.60；A小调有一条 COMMON_MINOR_015 匹配度高但无可续写尾部。Color 没有进入评分/UI阶段。
- 这说明已测样本中的 Color 问题来自和弦匹配不足或没有可继续的 Factory 尾部，不是 Color 排序/拍号过滤。它不能证明改变拍号也不会影响其他Color候选。
- 真实 Cubase 的调性、原始 MIDI/VST XML、OPEN 语义及成功/失败两组数据仍未确认。不得宣称本审计定位或解决了真实 R02。

### B08 / B11

- Phase 3 已有稳定性矩阵含 B08/B11，但变形输入固定4/4、120 BPM，没有多拍号候选对照。
- 本轮的现有 FactoryV4Focused 测试覆盖 V4_B08 与 V4_B11 推荐、完整路径及 MIDI 导出契约；未扩展其时间签名矩阵。
- 因而只能确认现有4/4测试通过，不能外推至6/8、7/8或不规则拍号。

## 7. 三种隔离方案比较

这些是后续实验设计，不是已实施行为。

| 方案 | 数学/代码逻辑 | 对 R01 /普通和弦 | 主要风险与兼容性 | 性能与实现 |
|---|---|---|---|---|
| A 保留现有分类，节奏只排序 | 保留现有 structuralWeight/roles 供诊断展示；另造一个不含时值/小节相位的 retrieval projection；先记录不受节奏影响的 core candidate pool，节奏仅重排。若总分门槛仍可因节奏变化淘汰，便没有真正做到“只排序”，需把 core eligibility 与 display rank 分开 | 4QN 与2QN的 core ID 集合目标为相同；A08 的顺序可以变。普通和弦核心性质规则可原样复用 | 两套结构视图可能分歧；若节奏排序影响Top-3，低位候选仍可能不可见；Color未召回不会凭排序恢复 | 最接近保留现有算法；需要新增内部字段或中间池。额外候选遍历小，但实际耗时未测。对FULL/SKELETON必须分别验证 |
| B 和声结构优先，节奏为弱辅助 | skeleton/gap 依据端点、核心和弦功能、明确终止、次属/借用目标等；时值比例与bar相位只作为连续、受限的辅助分，不单独决定是否属于核心骨架 | R01 G#dim不再仅因位于4/4强拍进入骨架；预计4QN/2QN的插入成本更接近。普通长和弦若功能未知也可能被低估，短V7等关键和弦须保留功能上下文 | 语义规则需明确；长时值 chromatic chord 与短时值功能和弦是关键反例；可能改变Skeleton、Color和已有插入惩罚。不能直接叠加Phase3A | 主要改HarmonyAnalysis与ProgressionMatcher；代码量中等，不需改库或Schema；全Factory对齐计算量级不变，须实测 |
| C 核心检索与节奏评价分层 | 核心和弦匹配/有效续写单独产出 eligibility 与 core score；节奏计算为独立分数/scale，最后用于排序与输出时值。将minimumMatch/minimumScore的可达性依据分开记录 | 可直接保证BPM/QN/拍号改变不会单独删掉harmonically valid core candidate；可保留节奏自然度排序。R01易诊断；R02若是和弦匹配不足或曲库尾部耗尽则仍不会被“修复” | 若核心匹配过宽，会引入和声不合适但时间无关的路径；Color、用户可见Top-K仍受排序和容量影响；需保持核心性质、V4显式低音冲突及Factory原路径 | 可解释性最好，但需要更清楚的MatchResult/诊断字段及门槛策略。若实现为额外完整匹配遍历，最坏接近多一轮对齐；可复用中间对齐降低成本。性能未测 |

### 推荐的最小风险方向

先批准 **B 的离线影子原型**，不直接生产化：

1. 当前的诊断 structuralWeight 保留，便于解释原行为。
2. 为检索单独生成不含 BPM、barStartEffect 与相对时值项的 retrievalWeight/骨架；非和弦节奏信号另行保留。
3. 先只比较 R01、常规和弦、短功能和弦、长非功能色彩和弦、R02、B08/B11；记录 match前池、续写池、最终rank、Color及MIDI。
4. 只有当多拍号/缩放下核心ID集合更稳定且负例不增时，再决定是否扩大到C的显式双层分数。

此选择直接针对已经实测的0.05 barStartEffect越过0.55边界，同时不改和弦核心性质、Factory输出、数据库Schema或60分门槛。它仍是实验建议，不是已验证修复。

## 8. Phase 3A 重叠

Phase 3A 开关在当前生产代码中默认为 false；本轮基线保持关闭。

| 状态 | R01 V4_A08 得分 | 证据性质 |
|---|---:|---|
| 原逻辑 4QN / 4/4，Phase3A OFF | 57.403870 | 已有运行结果 |
| 原逻辑4QN，Phase3A例外ON | 65.403870 | Phase3A报告中的隔离实现结果；本轮未运行 |
| 若B类结构投影产生与2QN相同gap cost、Phase3A OFF | 约61.722977 | 公式推导，不是本轮实验 |
| 上一假设再叠加Phase3A返还 | 约69.722977 | 公式推导，禁止当作验证结果 |

若结构投影让G#dim作为经过插入获得较低 gap cost，再同时返还完整8分，会对同一插入事件双重获益。Phase3A必须继续OFF；若未来结构方案稳定解决R01，优先评估弃用局部例外，而不是叠加。

## 9. 测试结果、缺口与回退

### 本轮运行的现有测试

在 build-v4-dev2 目录直接运行，未运行全套CTest：

| 测试程序 | 结果 |
|---|---|
| HarmonyAnalysisTests | 全部黄金样本通过 |
| ProgressionMatcherTests | 40/40 |
| ContinuationEngineTests | 42/42 |
| MidiImportTests | 354项检查通过 |
| MidiExportTests | 578项检查通过 |
| PreviewTests | 75项检查通过 |
| ProductizationTests | 68项检查通过 |
| FactoryV4Tests | V4 focused contracts通过，涵盖28条定义及目标、元数据、骨架、低音、颜色、移调、推荐、Preview与MIDI |

这些是现有4/4或测试构造场景，并不构成多拍号推荐结果。ProgressionMatcherTests 的J/K记录显示：rhythm subscore从1降至0.292，但Match相似度及Skeleton/Full相似度不变；这验证匹配相似度与独立节奏子分相分离。HarmonyAnalysisTests 的duration_scaled测试未设置timeSig，不能覆盖barStartEffect的全局缩放风险。

未运行 RecommendationReliabilityTests 的全量 system-audit 分支，以免重写已有 build-v4-dev2/reliability 审计产物；Phase 3 已有5,801次运行、220个变形及既有CSV可复用。HostContractHarness 不计入通过项；它需要VST3路径和宿主名称参数，且属于SDK模拟，不是真实Cubase。没有运行Cubase实机。

### 下一轮最小验证矩阵

- R01、R02、B08、B11各自固定同一组抽象和弦和音高。
- BPM仅改90/120/180且QN不变；分别记录相同候选集合、分数、Color、MIDI QN与秒数。
- BPM与QN配成等秒输入：90/2QN、180/4QN；另加相同bar-relative结构对照，避免混淆秒时长、QN比例和小节位置。
- 固定QN输入遍历4/4、3/4、6/8、5/4、7/8、9/8，记录HarmonyAnalysis角色、MatchResult、候选各阶段数、Color、最终UI与导出MIDI元数据。
- 对7/8测试2+2+3和3+2+2只能先记为“当前模型无法表达”；只有新增宿主/会话分组字段后才可测。
- FULL/SKELETON、OPEN/非OPEN、Resolve/Develop/Loop/Color、转位/八度、核心性质正负例必须配对；Phase3A保持OFF。
- 原型通过前，核心候选ID集合对纯BPM变化应完全相同；QN全局缩放与只改拍号时，凡和弦身份相同且无真实节拍分组证据的样本，核心召回集合Jaccard应为1.0。节奏排序和输出时值允许变化。
- 若召回集合不变，至少检查原有全套推荐测试通过、错误核心性质兼容数不增加、Color候选不可因重排被制造、Factory路径与输出音高/核心性质逐项保持。
- 性能必须用同一Release二进制、同一657条数据库、同一输入列表配对报告P50/P95；当前没有批准的性能预算，不虚构百分比门槛。拓扑或推荐候选质量由人工盲听样本复核，不能用算法分数代替听觉判断。

无现成的多拍号推荐诊断入口。完成实际候选/Color/MIDI差分的最小改动，是给现有 RecommendationReliabilityTests 增加测试专用 meter/BPM 输入矩阵和逐阶段导出字段；不需要新增独立工具或碰生产调用。当前用户要求“不额外编写工具”，因此本阶段没有增加此代码。

回退设计：本阶段只有报告文件，无生产改动可回退。获准后若做影子原型，应只放在已有测试入口；验证失败时删除影子分支和测试输出即可，不触碰Factory/Schema/User Library/Snapshot/安装器。未来生产切片必须有独立开关、默认关闭，并通过A/B候选记录可确认开关关闭与当前版本完全一致。

## 10. Cubase 真实证据的最小集合

比较此前“成功”与“失败”的两组输入，至少需要：

1. 输入 MIDI 文件或 VST-XML 原始和弦 payload；逐音 MIDI 的 pitch、velocity、channel、track、start tick/QN、duration tick/QN。
2. SMF PPQ、tempo event list、time-signature event list；若有拍号变化需每条变化的QN；7/8若用户依赖分组需提供2+2+3或3+2+2。
3. 插件识别的和弦、低音/转位、切片起止、置信度、OPEN状态，以及实际 forced/selected key。
4. Host diagnostics 中实际 BPM、拍号、工程QN与输入导入路径；VST-XML场景还需插件收到的逐和弦projectTime。
5. FULL/SKELETON、intent、风格、候选设置；逐个Factory ID的 match score、alignment、继续路径、score子项、淘汰原因和UI可见排序。
6. 成功与失败两次所用的 Factory V4 版本及插件 build。

有了这两组数据，先确认差异是：宿主传值、MIDI识别、拍号上下文、结构分类、和弦匹配、续写/过滤、60分门槛还是 UI排序。当前模拟数据不能替代这些证据。

## 11. Git 与产物

- 开始与结束分支均为 library/v4；HEAD 为 42a915bda447457bd1bdc8b2f79a90e5ddc69c00。
- 构建数据库 factory.db SHA-256与用户基线相同：74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534。
- 本轮没有修改生产源文件、测试源码、Factory JSON/DB、Schema、User Library、Snapshot、安装器、版本号、main或Tag。
- 原有工作区修改均保留：NOW.md、ContinuationEngine.cpp/.h、RecommendationReliabilityTests.cpp，以及此前未跟踪的Phase1/2/3A/3/4A报告与审计目录。
- 本轮唯一新增持久文件为 PHASE4B_RHYTHM_DECOUPLING_AUDIT_20261011.md。

## 阶段结论

- PHASE 4B RHYTHM AUDIT COMPLETE：NO（BPM/代码路径和R01 QN效应已定位；实际6种拍号的推荐、Color、MIDI差分未运行）
- PRODUCTION BEHAVIOR CHANGED：NO
- READY FOR ARCHITECTURE DECISION：YES（可批准离线影子原型；不等于批准生产开发或Cubase验收）

审计到此停止，等待决策端批准下一步。
