# HarmonyContinuation — Phase 2 决策审查补充：Phase 3A 审批摘要

日期：2026-10-11
依据：docs/PHASE2_FACTORY_V4_ABSTRACT_MATCH_DESIGN_20261011.md
审查结论：Phase 2 有条件接受；本摘要不构成 Phase 3 执行授权

## 一、R01 五条候选

以下和弦按 A 小调具体化。每条均对齐输入中的 3/4 个和弦，G#dim 被当作一个额外查询项，没有并入 Factory 路径。D/F# 与 D 对照得到相同候选及分数。

| Factory ID | 完整 Factory 路径 | 匹配部分 | 可续写部分 | 得分 | 主要问题 |
|---|---|---|---|---:|---|
| V4_A08 | Am–D–Am–G–D–Am | 模板位置 2–4：Am–G–D；三和弦根音及核心性质相合 | Am–Am–D，含循环回绕 | 57.4039 | 对一个 G#dim 插入项扣 8 分；因此低于 60 分门槛 |
| V3_DUSK_010 | Am–G–Dm–E–Am | 模板位置 0–2；Am、G、D 根音相合，但输入 D 大三和弦对到 Dm | E–Am | 53.6139 | 插入项 −8 分；D/Dm 性质和功能不一致 |
| V3_DUSK_043 | Am–G–Dm–Am | 模板位置 0–2；同上，D 对到 Dm | Am | 52.4139 | 插入项 −8 分；D/Dm 性质不一致；尾部只有一个和弦 |
| V3_NIGHT_004 | Am–G–Dm–F | 模板位置 0–2；同上，D 对到 Dm | F–Am，含循环回绕 | 53.6139 | 插入项 −8 分；D/Dm 性质不一致 |
| V3_NIGHT_043 | Am–G–Dm–Am–C–F | 模板位置 0–2；同上，D 对到 Dm | Am–C–F–Am，含循环回绕 | 56.0139 | 插入项 −8 分；D/Dm 性质不一致 |

**结构判断：** V4_A08 是最值得进入人工评审的一条：它保留 D 大三和弦，并且命中段的三个根音与核心性质一致。但它实际给出的 Am–Am–D 尾部有重复主和弦，不能仅凭匹配得分认定合适。

V3_DUSK_043 是当前最弱的直接匹配候选：它把输入 D 大三和弦对到 Dm，且只提供 Am 一个后续和弦。可以说它不适合作为此输入的高置信度直接匹配，不能据此断言这条曲子本身音乐上错误。

Phase 2 没有人工听觉标注。因此，**没有哪一条可以被判定为已验证的好推荐或音乐上明显错误**；以上仅是结构匹配强弱判断。五条的 52.4139–57.4039 分均可由现有加权项重建，固定插入惩罚均为 −8 分。

## 二、R02 Color 问题

### 已证明的离线条件

| 语境 | 实际条件 | Color 消失位置 |
|---|---|---|
| A 小调 T1–T4 | 85 条大调 Color 在匹配器的严格模式条件处被排除；只剩 4 条小调 Color | COMMON_MINOR_015 对齐 Am–F–G–Am 的末尾，loopable=false，之后以 no_continuation 淘汰。COMMON_MINOR_016/017/018 的最佳对齐均低于 0.60，以 minimum_match 淘汰 |
| C 大调 T1 补充对照 | 85 条大调 Color 中 84 条有返回对齐；14 条有非空尾部，但最高相似度仅 0.581427 | 84 条全部在最低匹配门槛处以 minimum_match 淘汰；没有进入续写评分或 UI |

对应代码条件：

- src/core/ProgressionMatcher.cpp 中查询模式与模板模式不一致时跳过该模板（Phase 2 报告定位于第 440 行附近）。
- src/core/ContinuationEngine.cpp 的最低匹配检查（第 166 行附近）会在尾部、候选分数和展示前淘汰低于 0.60 的结果。
- 通过匹配后，若没有尾部且模板不可循环，候选以 no_continuation 退出（第 204 行附近）。
- 当前没有把 Color 意图当作硬筛选的逻辑；Color 偏好只影响排序。离线 Color 缺失不是去重或界面隐藏造成的。
- 在这些模拟中，Color 没有走到 60 分总分门槛；不能把原因归为评分不足。

### 尚未证明的 Cubase 真机问题

四组结果都是模拟 MIDI。真实 Cubase 的调性状态、原始音符时间、OPEN 状态、FULL/SKELETON、候选来源和 UI 结果均未取得。因此，尚不能判断用户所见 Color 差异属于识别、模式选择、真实输入过滤、候选来源或展示差异，也不能将模拟的退出点当作真机根因。

## 三、唯一优先的 Phase 3A 切片

### 建议：为严格限定的减和弦经过音增加可关闭的评分例外

**要解决的问题：** R01 模拟中，G#dim 位于 A 与 G 两个相邻半音根音之间，被对齐器作为查询插入项。它使五条已具备可续写路径的候选各扣 8 分。最贴合的 V4_A08 若不扣该罚分，现有其他分项合计为 65.4039；这只是按 Phase 2 分数推算，尚未运行新规则，也未证明该候选音乐上合适。

**现有逻辑：** src/core/ProgressionMatcher.cpp 已能给特定插入项标注“解析到匹配目标”的原因；src/core/ContinuationEngine.cpp::recommendContinuations 在约第 257–263 行对其他 QueryInsertion 扣除 RecommendationWeights.unexplainedInsertionPenalty，默认值为 8。匹配/评分门槛本身分别为 0.60 和 60。

**计划改成：** 只增加一个单独的、默认关闭的规则：当一个内部查询插入项是减三和弦、其根音恰好处在前后两个相差全音的已匹配根音之间（如 A–Ab–G），且本次查询所有已对齐和弦的根音与核心性质均与 Factory 对应位置一致时，记录为“半音经过减和弦”。仅在测试显式开启 Phase 3A 选项时，该插入项不再按“未解释插入”扣 8 分。不得豁免其他插入项，不得忽略任何对齐和弦的核心性质冲突，不改 0.60/60 门槛。

按现有 R01 数据，这个规则预期只会让 V4_A08 从 57.4039 回到 65.4039；V3 路径因 D 大三和弦对 Dm 的性质冲突不符合规则。这个预期必须由测试验证，不是已实现结果。

### 文件和函数范围

- src/core/ProgressionMatcher.h：新增独立的对齐原因标记。
- src/core/ProgressionMatcher.cpp：在 align() 的 QueryInsertion 回溯标注处（约第 263–279 行）识别上述严格条件；现有 secondary-target 解释规则保持不变。
- src/core/ContinuationEngine.h：在 RecommendationWeights 中加入默认关闭的 Phase 3A 选项。
- src/core/ContinuationEngine.cpp：仅在该选项开启且对齐原因标记成立时跳过这一项 8 分扣分；其他候选评分不变。
- tests/ProgressionMatcherTests.cpp、tests/ContinuationEngineTests.cpp、tests/RecommendationReliabilityTests.cpp：分别验证标记规则、评分开关和 R01 输出基线/启用效果。

### 预期收益、风险、边界与回退

- **收益：** 针对已定位的 R01 评分损失；不放宽全局门槛，不把减和弦与大/小三和弦视为同一和弦。
- **新增风险：** 有些半音减和弦并非经过和弦；即使规则匹配，候选也可能不适合具体曲风。默认关闭及“所有匹配核心性质必须一致”的条件用于限制误判。
- **不受影响：** Factory 内容、Schema、用户库、和弦识别、Color 搜索、模式过滤、UI、Enrichment、推荐 MIDI 的 Factory 路径忠实度及其他意图。该切片不修复 R02。
- **关闭/回退：** 选项默认 false；关闭时输出必须与当前基线相同。若启用结果或质量不合格，可保持关闭或回退一个独立提交，无需数据库迁移。

## 四、3B、3C、3D 边界及产品原则

- **3A：** 仅为 R01 这种严格的半音经过减和弦插入建立可关闭的评分例外。
- **3B：** 研究分级和弦兼容性，包括省略五度、C 到部分 Cmaj7/Cmaj9 的方向性软匹配、11/#11 表示，以及 C/Cm、Cmaj7/C7、sus/减类硬负例。不得混淆核心性质。
- **3C：** 研究多长度前缀、中间连续片段、有限的多窗口/有尾部对齐、调性假设和 Color 召回层级；低层候选不能挤掉高置信匹配。
- **3D：** 取得成功/失败两组真实 Cubase 输入，核对插件解析、推荐来源、过滤原因和 UI；之后才讨论诊断字段、产品集成和分阶段启用。

所有后续阶段继续遵循：

1. Factory V4 的 657 条是唯一完整进行来源；不扩库、不改 Schema。
2. 用户输入以抽象和弦身份匹配；默认不强制转位、低音、音域、八度或配器一致。
3. 保留核心和弦性质区别；扩展音可在有依据时软匹配，不能把扩展和弦与三和弦视为完全相同。
4. 推荐 MIDI 必须忠实于所引用的 Factory 原始路径；Enrichment 与 Factory 路径检索分开。
5. 节奏主要辅助排序；不启动即时和声路径生成器。

## 五、Phase 3A 测试与验收

### 核心必测

1. Matcher 正例：仅 A–Abdim–G 这类严格内部半音经过结构获得新标记；根音方向、间隔、核心性质或位置不符时不标记。
2. Scorer 开关：默认关闭时，R01 的五个 ID、分数和阶段结果必须与当前基线一致；开启时，只有符合规则且无其他核心性质冲突的 V4_A08 可免去该单项罚分。
3. 端到端：D/F# 与 D 对照继续得到相同结果；最低匹配与最低分数仍为 0.60/60。
4. R02 不回归：T1–T4 仍无 Color；不得延长 COMMON_MINOR_015，不得由此改动模式过滤或 Color 评分。核心自动化入口为 ProgressionMatcherTests、ContinuationEngineTests、RecommendationReliabilityFocused。

### 增量必测

- 正例：R01 Am–G#dim–G–D；同一输入中，V4_A08 的对齐核心性质一致，应可测试豁免，而对到 Dm 的 V3 候选仍有性质冲突，不得豁免。
- 负例：插入根音不构成连续半音、插入项不是减三和弦、插入位置在首尾、存在多个插入项；R01 的 V3 对齐用来验证 D 大三和弦对 Dm 的冲突不会被经过音规则掩盖。
- 兼容回归：C/Cm、Cmaj7/C7、转位、缺五度、显式低音、OPEN、FULL/SKELETON、不同意图。
- B08/B11 现有用例继续通过；已有 secondary-target 插入解释行为不变。

### 回归抽检

Phase 2 报告记录过的相关 12 项基线为 12/12；Phase 2 本身只重跑了 RecommendationReliabilityFocused 1/1。Phase 3A 应重跑核心匹配/续写/推荐测试，并抽检 FactoryV4Focused、CubaseReliabilityFocused、HarmonyAnalysisTests、MidiImportTests、MidiExportTests、VstXmlChordParserTests、UILayoutTests、EnrichmentTests 和 PreviewTests。不能把 Phase 2 的历史结果冒充 Phase 3A 结果。

### 宿主人工验证

启用默认行为前，需用真实 Cubase R01 成功/失败样本核对原始 MIDI、逐和弦识别、调性、过滤记录和 UI。当前无这份真实输入，故宿主验收尚不可执行。启用选项前至少需要盲评者将 V4_A08 标为“可用”或“特定语境可用”；若不接受，应保持关闭。

### 通过标准

- 关闭选项：目标及回归集与当前 ID、排序、分数和过滤基线完全一致。
- 开启选项：R01 的预期召回改善为 0 条跨过 60 分到 1 条 V4_A08；此数量变化只有在盲评接受时才算质量改善。
- 错误风险：锁定硬负例的错误豁免数为 0；所有核心性质冲突仍被拒绝豁免。
- R02 Color、Factory 来源路径及 UI 无变化。
- 性能记录新旧 p50/p95；当前没有可信毫秒基线，先记录数据，再按决策端批准的预算验收。

## 六、Git 与数据安全

本次核对时状态：

- 分支：library/v4。
- HEAD：42a915bda447457bd1bdc8b2f79a90e5ddc69c00。
- 工作区已有变化：tests/RecommendationReliabilityTests.cpp 为测试专用改动（220 行新增）；Phase 1/1B 报告、Phase 2 设计报告和本审批摘要为未跟踪文件。
- 已有忽略产物：build-v4-dev2/reliability 下的 Phase 1B 输入/诊断文件及 8 个模拟 MIDI。
- Factory 数据库 SHA-256：74c529828f8c4b934bbaf4ada719aa95491ab498b58885522124ff6505671534，与 Phase 2 记录一致。
- 本次仅新增本审批摘要，没有改动任何生产代码；没有提交、创建版本或运行 Phase 3。

Factory 657 条、Schema、User Library、Snapshot、安装器、正式 Tag 和 main 均保持不变。

## 停止点

本文件仅供决策端审查 Phase 3A 的单一开发切片。当前没有实施该切片，也没有授权 3B、3C 或 3D。等待决策端批准。
