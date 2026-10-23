# HarmonyContinuation Phase 4B 综合实施报告

日期：2026-10-12
基线分支：`library/v4`
基线 HEAD：`42a915bda447457bd1bdc8b2f79a90e5ddc69c00`

## 结论摘要

本阶段完成了节奏与和声检索的隔离原型、六种拍号下的合成 MIDI 导入与推荐对照、时间缩放和性能记录。生产入口仍默认使用 `Legacy`，没有修改 Factory 数据、数据库结构、User Library、正式版本或安装器。

最稳定的隔离模式是 `HarmonyFirst`：本次 120 组有共同非空候选的时间变形对照中，候选集合 Jaccard 为 1.00；R01 的 4QN 和 2QN 都显示 V4_A08。它也让本次小样本的推荐曝光更集中（前 10 条占 52.9%，基线 48.5%），且没有恢复 R02 Color。因此它适合作为下一轮受控验证原型，不足以默认启用或发布。

完整自动化回归为 32 项，其中 25 项通过、7 项失败；失败项均已复跑。当前没有可在 Cubase 中切换 OFF/ON 的测试包。因而本阶段的算法隔离实现与离线验证已完成，但宿主验收包和发布验收未完成。

## 阶段门禁

| 项目 | 结果 |
|---|---|
| PHASE 4B INTEGRATED WORK COMPLETE | **PARTIAL**：隔离实现、离线验证和报告完成；回归仍有失败，宿主包未就绪 |
| MULTI-METER TESTS EXECUTED | **YES**：331 个 MIDI 样本覆盖六种拍号，并进入推荐引擎 |
| RHYTHM-HARMONY DECOUPLING IMPLEMENTED | **YES**：增加内部实验模式；生产入口仍默认 Legacy |
| DEFAULT ENABLED | **NO** |
| PHASE 3A DEFAULT ENABLED | **NO** |
| FACTORY DATA CHANGED | **NO** |
| PRODUCTION RELEASE CHANGED | **NO** |
| READY FOR CUBASE ACCEPTANCE | **NO**：没有宿主可切换开关，且全量回归有失败 |
| READY FOR FORMAL RELEASE | **NO** |

## 1. 基线、范围与证据类别

复核结果：分支和 HEAD 与授权基线相符。Factory V4 数据库为 657 条；构建目录中的 Factory 数据 SHA-256 为 `74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534`，与阶段基线一致。没有改写数据库、Schema、用户库、Snapshot、安装器、main 或 Tag。

本报告区分：

- **代码事实**：由本地源码及调用字段确认。
- **本次离线实测**：331 个合成 MIDI 样本通过现有 MIDI 导入器后，实际调用核心推荐引擎；不是 Cubase 录制。
- **自动化回归**：实际构建并运行的 CTest 项目；失败照实保留。
- **音乐判断**：尚未经过人工试听，不能当作质量结论。
- **宿主待验证**：需要真实 Cubase 工程状态、输入和插件界面结果。

本轮没有另写独立诊断程序。诊断入口复用现有 `RecommendationReliabilityTests`，只增加一个测试场景，并由 CTest 注册。

## 2. 推荐链路和本轮改动

```text
MIDI 音符 / QN / 拍号
        ↓
和弦识别、时值与结构权重
        ↓
FULL / SKELETON 输入投影
        ↓
Factory 匹配与对齐轨迹
        ↓
有效续写检查
        ↓
Match 门槛 → 总分门槛 → 去重 / 多样性 → 每意图组 Top-K
        ↓
预览与 MIDI 导出
```

这条链的主要消失点是：识别结果变化；匹配相似度低于 0.60；Factory 路径没有可续写尾部；总分低于 60；之后的去重、排序及分组容量。本次没有调整 0.60 或 60 门槛。

| 隔离模式 | 代码行为 | 本次解释 |
|---|---|---|
| `Legacy` | 原有默认行为 | 生产上下文默认值；不设置新模式时继续走此路径 |
| `BarPositionIndependent` | 检索结构权重保留相对时值和短角色信息，去掉小节起点奖励 | 专门检查重拍相位是否改变 0.55 分类 |
| `ContinuousStructure` | 不把输入先压成离散骨架；所有输入和弦作为软结构参与对齐，缺口成本连续变化 | 消除一部分硬分类跳变，但本次多组对照未能稳定产出候选 |
| `HarmonyFirst` | 检索权重不采用时值、小节位置和短角色扣减；检索角色不保留 Approach；单独以不含节奏项的资格分过 60，节奏仍进入排序分和输出时值 | 本次在 R01 和时间缩放稳定性上最好，但需要关注曝光集中与 Color 未改善 |

具体位置：

- [HarmonyAnalysis.h](D:/VibeCoding/HarmonyContinuation-v4/src/core/HarmonyAnalysis.h:33)、[HarmonyAnalysis.cpp](D:/VibeCoding/HarmonyContinuation-v4/src/core/HarmonyAnalysis.cpp:193)：增加检索时值模式与独立检索结构表示；`structuralWeight` 的时值、小节位置和短角色项由实验模式选择。
- [ProgressionMatcher.h](D:/VibeCoding/HarmonyContinuation-v4/src/core/ProgressionMatcher.h:95)、[ProgressionMatcher.cpp](D:/VibeCoding/HarmonyContinuation-v4/src/core/ProgressionMatcher.cpp:82)：将模式带入匹配；连续结构模式使用软缺口成本。
- [ContinuationEngine.cpp](D:/VibeCoding/HarmonyContinuation-v4/src/core/ContinuationEngine.cpp:70)：增加节奏中性的资格分。`HarmonyFirst` 仍按正常含节奏的排名分排序，但以资格分判断是否过 60。
- [RecommendationReliabilityTests.cpp](D:/VibeCoding/HarmonyContinuation-v4/tests/RecommendationReliabilityTests.cpp:346)：在现有测试入口增加隔离运行；[CMakeLists.txt](D:/VibeCoding/HarmonyContinuation-v4/CMakeLists.txt:583) 注册独立 CTest 项。

`RetrievalTimingMode` 默认是 `Legacy`。VST3 正常入口没有设置实验模式，也没有 UI 或宿主可用的 OFF/ON 切换；因此这里的 ON 结果来自测试注入，不能用于声称已有可交付的 Cubase A/B 包。

## 3. 实验设计与覆盖

- 331 个确定性构造的 SMF MIDI 样本，12 类代表性进行，含 R01、R02、B08、B11、调内、借用、Color、次属、sus、经过减和弦及非经过减和弦等。
- 4 种模式各跑 331 次，共 1324 次实际推荐引擎调用；MIDI 往返 331 次；失败数 0。引擎调用总耗时 6734.15 ms。
- 六种拍号：4/4、3/4、6/8、5/4、7/8、9/8。测试构造了拍号元数据并读取识别结果；对 7/8 没有伪造 2+2+3 或 3+2+2 分组，因为现有宿主输入没有分组字段。
- 同一 QN 输入的 BPM 对照、相同物理时长但不同 BPM/QN、整段缩放、中间和弦时值变化、OPEN/非 OPEN、不同节拍位置、转位和八度均纳入样本。测试条件为合成文件及参数注入，不是宿主现场捕获。
- 每个模式的 shortlist 均为 657，故在本次这些查询中粗筛没有漏掉 Factory 条目；这不能证明任意输入都没有召回漏失。
- 每条匹配都分别记录 FULL 与 SKELETON 相似度；这覆盖了引擎内部两路评分，但本次没有把产品界面的 FULL / SKELETON 作为独立用户模式逐个切换验收。
- 随机种子：不适用，样本构造为固定输入。

R01 的原始合成 MIDI 音符为：

| 起点 / 时长 | 音符号（MIDI） | 识别和弦 |
|---|---|---|
| 0 / 4 QN | 45, 60, 64, 69 | Am |
| 4 / 4 QN | 44, 62, 68, 71 | Abdim（输入拼写为 G#dim） |
| 8 / 4 QN | 43, 62, 67, 71 | G |
| 12 / 4 QN | 42, 62, 66, 69 | D/Gb（输入为 D/F#） |

识别中的 Ab/G#、Gb/F# 是等音拼写；MIDI 音高类一致。样本的完整 MIDI 事件、所有匹配轨迹和各模式结果在构建目录的 TSV 中。

## 4. R01 逐阶段结果

V4_A08 Factory 原路径为 `Am–D–Am–G–D–Am`（罗马数字 `i | IV | i | VII | IV | i`，A 小调）。本轮所有模式的 A08 对齐轨迹均为：

```text
输入索引 0 Am      → Factory 索引 2 Am：MATCH
输入索引 1 G#dim   → 无 Factory 索引：QUERY_INSERTION
输入索引 2 G       → Factory 索引 3 G：MATCH
输入索引 3 D/F#    → Factory 索引 4 D：SUBSTITUTE
```

续写从 Factory 索引 5 开始。该条目可循环，故续写按原 Factory 次序成为 `Am@4QN → Am@4QN → D@4QN`；缩放样本则输出为 `Am@2QN → Am@2QN → D@2QN`。这是循环路径的续写，不是新拼接的和弦。MIDI 导出核验了续写和弦音高、根音/低音及各续写音符 QN 时长与候选一致。

| R01 条件 / 模式 | Match 相似度 | FULL / Skeleton | Rhythm | 资格分 | 排名分 | 门槛 / 可见 |
|---|---:|---:|---:|---:|---:|---|
| 4QN Legacy | 0.674117 | 0.689446 / 0.689446 | 1.000 | 55.482 | 57.404 | 低于 60，不显示 |
| 4QN BarPositionIndependent | 0.769499 | 0.787274 / 0.787274 | 1.000 | 60.177 | 61.723 | 显示，组内第 1 |
| 4QN ContinuousStructure | 0.710931 | 0.727205 / 0.727205 | 1.000 | 57.294 | 59.071 | 低于 60，不显示 |
| 4QN HarmonyFirst | 0.769499 | 0.787274 / 0.787274 | 1.000 | 60.177 | 61.723 | 显示，组内第 1 |
| 2QN Legacy | 0.769499 | 0.787274 / 0.787274 | 1.000 | 60.177 | 61.723 | 显示，组内第 1 |
| 2QN HarmonyFirst | 0.769499 | 0.787274 / 0.787274 | 1.000 | 60.177 | 61.723 | 显示，组内第 1 |

4QN Legacy 的 Factory 候选阶段计数是 657 条进入 shortlist、184 条匹配结果、5 条通过续写检查、0 条通过 60 分门槛。不能把 184 条匹配结果说成有效续写。HarmonyFirst 的 4QN 查询让 8 条通过路径检查，其中 A08 过门槛并可见。连续结构模式虽让对齐分高于基线，但 A08 总分仍只有 59.071，未显示。

根因证据：4QN 下 G#dim 的结构权重为 0.59，因其起点 4QN 被 4/4 小节起点奖励抬高；2QN 下起点为 2QN，权重为 0.54。0.55 的离散分类由此改变插入缺口成本。两组 Rhythm 都为 1，说明直接节奏子分不是分差原因。该变化进入 Match 与 Skeleton 两个最终子分，因此影响被计入两处；对本条 V4 路径两者数值相等。是否构成不合理重复评价仍需更多音乐样本及人工试听，不据此单独判定。

等时长对照中，90 BPM/2QN 与 180 BPM/4QN 的总持续秒数相等（均为约 5.333 秒），但 QN 网格和小节位置不同；Legacy 会按其 QN/小节结构得出不同可见结果。固定 QN 改变 BPM 不改变推荐打分；BPM 用于试听秒数和 MIDI tempo 元数据。时值缩放可改变输出 QN 长度，但本次验证的和弦音高与 Factory 路径未改变。

边界扫描使用 13 个 G#dim 时值，包括 3.125 QN 与 SMF 量化后约 3.175 QN 的边界邻近值。由于 960 PPQ 的 MIDI tick 网格不能表达理论上的精确 0.55，本次不声称测试了数学上恰好等于边界的 MIDI。Legacy 在邻近点从约 61.72 跌至约 57.59，约 4.13 分；连续模式变化较平滑但本组仍未过 60；HarmonyFirst 在扫描点保持可见。扫描 CSV 在构建产物目录。

## 5. R02 Color 结果

R02 使用固定音符序列 `F–G–Am`；C 大调样本 MIDI 为：

| 起点 / 时长 | 音符号（MIDI） |
|---|---|
| 0 / 4 QN | 41, 60, 65, 69 |
| 4 / 4 QN | 43, 62, 67, 71 |
| 8 / 4 QN | 45, 60, 64, 69 |

C 大调与 A 小调、六种拍号、固定/每小节/3+5+4/OPEN，以及 Color 意图样本中，四种模式均没有可见 Color 候选。这里仅证明合成离线样本，不能定位 Cubase 中的差异。

代表性 Color Factory 条目 `V3_SHADE_004`，路径 `I | V | vi | IV | iv | I`，对输入只把 G→V、Am→vi 匹配到 Factory 索引 1–2；F 是一次查询插入。Factory 索引 3 起仍有三和弦续写，因此不是无尾部条目。消失阶段如下：

| 模式 | Match | 结果 |
|---|---:|---|
| Legacy | 0.581427 | 低于 0.60 `minimumMatch`，未评分 |
| BarPositionIndependent | 0.588810 | 低于 0.60 `minimumMatch`，未评分 |
| HarmonyFirst | 0.588810 | 低于 0.60 `minimumMatch`，未评分 |
| ContinuousStructure | 0.607967 | 通过匹配，但 52.982 分低于 60，`minimum_score` |

所以在此具体条目中，主因是匹配门槛；软结构模式下则转为总分不足。它不是意图过滤或没有可续写尾部。A 小调样本也没有 Color，但不能据此推断所有 Factory Color 条目的根因相同。此前已知的真实 Cubase 调性、MIDI、OPEN 状态和 UI 来源差异仍未取得。

## 6. 多拍号、稳定性与曝光

专项样本中，B08（`C–Cmaj7/B–C7/Bb–F/A`）在每种模式各 12 个时值/拍号变体中均有可见候选；Legacy、BarPositionIndependent、HarmonyFirst 各覆盖 3 个不同 ID，ContinuousStructure 覆盖 5 个，且没有 Color。B11（`Cmaj9–Dbmaj9–Cmaj9`）也在四种模式各 12 个变体中均有可见候选；前三种模式各覆盖 2 个 ID，ContinuousStructure 覆盖 3 个，并在 12 个变体中出现 Color。该 Color 改善只证明这一组 B11 离线输入，不代表 R02 或一般 Color 召回已修复。

### 支持边界

代码使用 `barQN = numerator × 4 / denominator` 计算小节长度，并以 `startQN % barQN == 0` 加小节起点权重。BPM 不进入 HarmonyAnalysis 或 ProgressionMatcher 的打分字段。QN 时值仍会影响时值归一化、结构角色、对齐及输出长度。宿主适配层只传分子和分母，不提供复合/不规则拍子的节拍分组；因此 6/8 与 3/4 的重音组织不能从现有字段区分，7/8 的 2+2+3 和 3+2+2 也不可区分。

本次六种拍号的确进入了合成推荐测试，但没有 Cubase ProcessContext 实测。不能把测试中的拍号覆盖表述为宿主节拍分组已完整支持。

### 时间缩放集合稳定性

下表只统计候选并集非空的对照，避免把“两边都空”误算为完全稳定：

| 模式 | 可比对数 | 平均 Jaccard | 最低 | Jaccard=0 |
|---|---:|---:|---:|---:|
| Legacy | 110 | 0.72 | 0.00 | 31 |
| BarPositionIndependent | 120 | 0.85 | 0.00 | 18 |
| ContinuousStructure | 15 | 0.00 | 0.00 | 15 |
| HarmonyFirst | 120 | 1.00 | 1.00 | 0 |

连续结构模式另有 105 组候选并集为空，主要问题是没有足够候选通过下游；因此 1.00 的时间稳定性并不适用于它。HarmonyFirst 在该测试集最稳，但不能据此断言对所有用户输入都稳定。

### 小样本推荐曝光

这里统计每个合成样本返回的可见候选 ID，不是用户实际曝光量、使用量或 UI 行为。样本经过重复构造，不代表生产使用分布。

| 模式 | 可见候选槽位 | 不同 Factory ID | 前 10 ID 份额 | 前 20 ID 份额 |
|---|---:|---:|---:|---:|
| Legacy | 1059 | 47 | 48.5% | 73.0% |
| BarPositionIndependent | 1099 | 45 | 49.8% | 72.7% |
| ContinuousStructure | 956 | 47 | 47.7% | 68.1% |
| HarmonyFirst | 1045 | 41 | 52.9% | 75.9% |

HarmonyFirst 的低排名稳定不代表多样性无损；在这组样本里可见条目数减少、曝光集中度上升，V4_A08 高频出现。正式评价需要更大且非 Factory 派生的独立输入集，并按意图、风格和适用情境分层。

## 7. 评分、性能与 Phase 3A

所有模式的 0.60 Match 门槛和 60 总分门槛保持不变。新模式只调整检索投影或 HarmonyFirst 的资格门槛分离；排名分仍含节奏分。R02 的 Color 例子说明增加路径匹配数不等于增加可见 Color：ContinuousStructure 的 `V3_SHADE_004` 通过 0.60 匹配门槛，最后仍以 52.982 分被淘汰。

| 模式 | 平均推荐耗时 | 中位数 | P95 | 最大值 | 平均分配流量 |
|---|---:|---:|---:|---:|---:|
| Legacy | 5.13 ms | 3.52 ms | 10.04 ms | 11.76 ms | 约 1865 KiB |
| BarPositionIndependent | 5.10 ms | 3.42 ms | 10.15 ms | 11.90 ms | 约 1849 KiB |
| ContinuousStructure | 5.06 ms | 3.51 ms | 10.05 ms | 12.29 ms | 约 1812 KiB |
| HarmonyFirst | 5.05 ms | 3.40 ms | 9.79 ms | 12.11 ms | 约 1839 KiB |

这些是测试进程中的核心推荐耗时和分配字节累计值，不是 VST 实时线程延迟或驻留内存测量。没有观察到本组样本中的明显耗时退化；仍需在宿主候选构建和长时间压力下复测。

Phase 3A 经过减和弦插入补偿保持默认关闭。单独隔离启用时，A08 从 57.404 提升 8 分到 65.404；HarmonyFirst 单独得到 61.723；两者同时启用会到 69.723，体现可能重复补偿。若后续确认 HarmonyFirst 是合适方向，应先重新评估并可能弃用局部补偿，不应叠加。当前未删除该机制。

## 8. 自动化回归与构建

- 完整构建：通过。
- 本次新增 CTest `RecommendationRhythmDecoupling`：通过，331 次 MIDI 往返、1324 次引擎调用、0 失败；最近一次耗时约 14.50 秒。
- 现有 1440 组严格可靠性矩阵：通过；与既有基线 TSV 的 SHA-256 完全一致：`3B3494E862DAA0F26538CC550BF3740A0F3AC0BF140E650C0FBB8318F4858909`。这验证默认 Legacy 对该矩阵没有行为差异。
- 全 CTest：32 项中 25 项通过，7 项失败；失败项已逐项重跑，未将其标成通过：

| 失败测试 | 观察到的失败 |
|---|---|
| `LibraryV3_user`、`LibraryV3_factory` | 临时库清理时遇到文件仍被占用 |
| `LibraryStoreTests` | “repair damaged active pointer” 检查失败，之后清理阶段进程异常退出 |
| `ContinuationEngineTests` | 42 项断言执行到结束，删除临时 user.db 时因文件占用失败；旧基线构建的相同测试曾通过，根因尚未确定 |
| `HostContractFLStudio`、`HostContractGeneric`、`HostContractCubase` | 每项 38/40；case 27 “remove reattach recreated parent” 和 case 38 隔离作业 Factory/User DB 检查失败。case 38 断言 Factory 数量为 161，而当前 V4 为 657；这是测试预期与当前库版本不一致，不能据此判断运行库少载入数据。case 27 尚未定位 |

回归失败使本阶段不能达到 Cubase 验收包条件。不能把失败简单归为环境噪声；需要在下一步处理或获得决策端对已知失败的明确处置。

## 9. 产物、Git 与回退

本轮新增正式报告：本文件。实验输出放在忽略的 `build-phase4b/rhythm-decoupling-final/` 下；主要文件包括：

- `phase4b-cases.tsv`：样本、候选计数、Color 和可见 ID。
- `phase4b-matches.tsv`：Factory 匹配轨迹、分项分数和淘汰原因。
- `phase4b-midi-inputs.tsv`：合成 MIDI 原始音符及 QN。
- `phase4b-performance.tsv`、`phase4b-time-scaling.tsv`、`phase4b-stability.tsv`、`phase4b-phase3a.tsv`。
- `R01_input_4-4_120bpm_4qn.mid`。
- `R01_recommendation_BarPositionIndependent.mid`、`R01_recommendation_HarmonyFirst.mid`。
- 隔离构建产物 `build-phase4b/VST3/Release/HarmonyContinuation.vst3`，默认仍是 Legacy；未制作安装包或 Cubase A/B 包。

当前工作区还保留着开始本阶段前已有的未提交改动和阶段报告。Git 状态含 `CMakeLists.txt`、`NOW.md`、四个 core 源文件、`tests/RecommendationReliabilityTests.cpp` 的修改，以及 Phase 1/2/3/3A/4A/前置 4B 报告等未跟踪文件；未提交、未丢弃、未强制重置。`NOW.md` 与这些既有报告予以保留。本报告是本阶段唯一新增的 docs 文件。构建目录在忽略区，不纳入源代码新增文件清单。

可回退方式：将本轮新增的 `RetrievalTimingMode`、检索投影字段/分支、连续结构对齐分支及 HarmonyFirst 资格门槛按本次差异逐块撤回；从现有测试程序移除 `rhythm-decoupling` 场景及 CTest 注册。由于这些源文件包含本阶段之前的 Phase 3A 和用户未提交改动，不应对文件整体执行还原或覆盖。没有提交，因此可以逐块审查并回退，不影响 Factory 数据。

## 10. 尚待人工与 Cubase 验收

以下事项没有通过本轮模拟结果证明：

1. 用 Cubase 真实捕获的 R01/R02 MIDI、实际调性、OPEN 状态和候选来源；R02 差异仍未定位。
2. 在宿主内切换 Legacy 与 HarmonyFirst，并检查 FULL/SKELETON、Color、Resolve/Develop/Loop、预览和拖放 MIDI。
3. 4/4、3/4、6/8 和至少一种不规则拍号在 Cubase 的真实工程上下文；7/8 分组需先确认宿主/API 能否提供。
4. 人工试听 R01 的 V4_A08 延续和其他高曝光/低曝光样例；自动分数不代表音乐质量。
5. 确认去重、Top-K 与多样性变化可接受，尤其是 HarmonyFirst 的 41 个唯一 ID和 52.9% 前 10 占比。
6. 查明七项回归失败并让候选包构建通过。

真实宿主最少需要：两组成功/失败工程的原始 MIDI 音符和起止时间、Cubase BPM/拍号、插件解析出的和弦与调性、FULL/SKELETON、Intent、OPEN、可见候选及 Factory 来源；另需分别使用 OFF/ON 导出的诊断追踪。只采集这两组工程即可先复现 R02，无需启用大量持久日志。

**本阶段到此停止；不进入 Phase 3B/3C，不修改生产默认值，不发布版本，等待决策端验收。**
