# Phase 3A：默认关闭的减和弦经过插入评分例外

## 范围与结论

核验日期：2026-10-11。对象：`library/v4`，HEAD `42a915bda447457bd1bdc8b2f79a90e5ddc69c00`，dev.3 源码基线，Release 独立测试程序。没有构建或发布新开发版本，没有安装插件。

本阶段只改变满足严格条件的单个查询插入事件的评分。内部开关 `RecommendationWeights::diminishedPassingInsertionException` 默认 `false`；现有插件调用没有启用它。匹配器、60 分门槛、节奏、Color、Factory、Schema、Session、Snapshot 与宿主接口均未修改。

路径核验通过；R01 开启后只有 V4_A08 获得 8 分返还。此结果证明受控召回行为，不证明听觉质量；人工音乐评审及真实 Cubase 验收仍待进行。

## 1. 开发前的路径核验

先在未修改生产代码时构建、运行现有 RecommendationReliabilityTests，新增路径断言通过后才修改评分。使用真实的 Loop 意图偏好让原有候选跨过原有门槛，未调低匹配或显示门槛。

Factory V4_A08 为 `i IV i VII IV i`，A 小调语境对应：

| Factory 原始索引 | 0 | 1 | 2 | 3 | 4 | 5 |
| --- | --- | --- | --- | --- | --- | --- |
| 和弦 | Am | D | Am | G | D | Am |

| 输入索引及和弦 | Factory 索引 | 操作 | 解释 |
| --- | --- | --- | --- |
| 0 Am | 2 Am | Match | 实际根音、性质相同 |
| 1 G#dim | 无 | QueryInsertion | 单个输入插入，未匹配为模板和弦 |
| 2 G | 3 G | Match | 实际根音、性质相同 |
| 3 D/F# | 4 D | Substitute | 实际都是 D 大三和弦；BorrowedVariant 来自功能解释，并非 D→Dm 替换 |

插入原始对齐成本约 0.6096，末项功能替代成本约 0.4060。匹配窗口是连续的 Factory `[2,4]`；完整输入还包括插入项，因此不是四项对三项的直接连续截取。没有模板删除、模板重排或匹配阶段循环。

`ProgressionMatcher.cpp` 按 `full.end + 1` 给出 continuationStartIndex=5。`recommendContinuations` 先追加 `[5,end)`，再因 `loopable=true` 追加 `[0,templateMatchStart)`，所以后续索引严格为 **5→0→1，即 Am→Am→D**。两个 Am 分别来自模板末尾及开头，是已存在的循环边界，未错误复制同一索引。重复主和弦是否适合推荐，仍是人工音乐判断。

每项为 4QN。通过原有 `continuationProgression` 和 `midi::candidatePayload` 导出，后续位于 16、20、24QN；两个 Am 的音级集合 A/C/E，D 为 D/F#/A，每项保留 4QN。实际 VoiceLed MIDI 后续为：

- 16QN：45、57、60、64；20QN：45、57、60、64；24QN：38、57、62、66。
- R01 输入低音为 A→G#→G→F#；Factory 是抽象和弦，不强制复现输入转位，续写 D 使用原有配音规则。
- 测试断言核对逐项索引、操作、输出标签、音级、时长；开关两种设置下同一候选的最终 MIDI 字节完全相同。

Factory 记录没有作者写定的音符级 MIDI，只有抽象和弦及节奏。这里的文件是原有引擎根据 Factory 抽象路径配音的模拟导出，**不是 Cubase 真实推荐，也不是作者录制的演奏**。

证据：`build-v4-dev2/reliability/phase3a-path-verification.txt`、`phase3a-A08-engine-output.mid`、`phase3a-A08-verified-path.mid`。前者保存原始 MIDI 音符与时间，后两个分别是产品导出入口与简化试听配音。

## 2. 修改位置与规则

| 文件 | 函数或结构 | 修改 |
| --- | --- | --- |
| src/core/ContinuationEngine.h | RecommendationWeights | 末尾增加默认关闭的独立内部开关；不持久化、不加 UI |
| 同上 | DiminishedInsertionDiagnostic、RecommendationSet | 增加仅在启用时填充的诊断；不改变 Snapshot 结构 |
| src/core/ContinuationEngine.cpp | evaluateDiminishedPassingInsertion | 只读核验现有轨迹、输入及模板，不修改对齐结果 |
| 同上 | recommendContinuations | 原扣分循环保持；启用后一次性返还符合条件的那个实际插入罚分 |
| tests/RecommendationReliabilityTests.cpp | 现有测试入口 | 增加路径、开关、变形、负例、MIDI、性能及内存分配量检查；保留既有阶段代码 |

必须同时满足：

1. 对齐轨迹恰好一个 QueryInsertion，位置在输入内部。
2. 完整输入逐项被轨迹解释，索引单调、无重复；Factory 窗口连续，无删除或跳过。两侧都有确定的模板对应。
3. 插入和弦为减三和弦或全减七和弦；本切片保守排除半减七。
4. 两侧根音均为同方向半音，例如 A→G#→G。
5. 插入和弦与至少一侧和弦共享两个音级，例如 G#dim 与 G 共享 B、D。
6. 其他所有输入和弦实际根音及完整核心性质与 Factory 相同。功能标签 Substitute 可以通过，但 D/Dm 等性质冲突不能通过。未知身份、次属目标以及字段冲突不获得例外。
7. 续写索引、hasContinuation 与模板长度相符，且有尾部或合法循环。原有实际路径、重复当前和弦、约束等检查仍生效。
8. 原有 ResolvesToMatchedTarget 未解释这次插入，否则原本没有该罚分，返还为零。

先运行原扣分，再返还这一个事件实际扣除的非负罚分，未增加额外奖励。默认罚分 8，故 R01 返还 8；不是无条件固定加 8。测试罚分 0、1、8、12 时均只返还对应罚分，最终该评分小计相同。多个插入、无可靠轨迹、任何核心冲突均不返还。规则不读取 Factory ID，重命名模板的资格检查保持相同。

诊断包含各项条件、总插入扣分、可返还事件原扣分、实际返还、评分小计前后值。最终支持分、去重、多样性及截断结果仍由 MatchResult 的最终分数与 disposition 解释。诊断不写正常运行日志，开关关闭时不计算条件、不填充诊断数组。

## 3. R01 全部候选对照

固定 A 小调、4/4、120BPM、每项 4QN、Color 意图偏好、默认候选设置，MIDI 导入后识别为 Am–Abdim–G–D/Gb。拍号必须与历史基线一致；新增测试初稿遗漏拍号曾得到不同基线，已修正并加入固定基线断言，未为此修改生产代码。

| ID | 完整路径 | 匹配窗口 | 续写 | OFF | ON | 原因 |
| --- | --- | --- | --- | ---: | ---: | --- |
| V4_A08 | Am–D–Am–G–D–Am | 索引2–4 Am–G–D | Am–Am–D（5,0,1） | 57.403870 | 65.403870 | 所有条件成立，返还单项8分 |
| V3_DUSK_010 | Am–G–Dm–E–Am | 索引0–2 | E–Am | 53.613865 | 53.613865 | 明确 D/Dm 核心性质冲突 |
| V3_DUSK_043 | Am–G–Dm–Am | 索引0–2 | Am | 52.413868 | 52.413868 | 同上 |
| V3_NIGHT_004 | Am–G–Dm–F | 索引0–2 | F–Am（循环） | 53.613865 | 53.613865 | 同上 |
| V3_NIGHT_043 | Am–G–Dm–Am–C–F | 索引0–2 | Am–C–F–Am（循环） | 56.013863 | 56.013863 | 同上 |

匹配、有效路径、去重数量均维持 5；通过评分门槛和最终可见数量由 0→1。原始排序不变，A08 仍第一；其余四条没有被抬分。全局 minimumScore=60、minimumMatch=0.60 不变。

A08 原始评分组成约为：匹配22.920、骨架7.584、风格4、意图3.5、终止7、续写9、先验4.4、节奏7、支持0、插入扣分−8。新行为只免去最后一项；原有对齐成本仍影响匹配相似度，功能替代成本没有被删除。

音乐依据是 A–G#–G 的半音经过连接及共同音，随后 D 为多利亚色彩 IV。它是可供人工评审的结构依据，不是人工听觉证据。

## 4. 测试设计与结果入口

必要检查全部复用现有测试入口，没有新增独立工具、脚本或开发版本。

- 核心：原始五条分数、0→1 可见候选、60 分门槛不变、逐项轨迹、循环5→0→1、原 MIDI 音级/时长、同一候选开关前后 MIDI 字节相同。
- 增量：原始 R01、B 小调移调、12个半音移调、转位加八度、单独转位、单独上/下八度、减七经过、短减和弦。
- 负例：非经过、不连续、只有单侧半音、方向反转、核心性质冲突、无上下文、多个插入、半减七、非减和弦半音运动、没有足够共同音、普通无减和弦。
- 对齐防护：损坏 Factory 索引、重复查询索引、模板删除、第二个插入、已经解释、无尾部；另测 Factory ID 重命名与非循环耗尽路径。
- 已有 F–F#dim7–G–C 解释机制：不再返还；其候选和分数维持。
- 原有 1440 组：开启/关闭逐个核对显示候选 ID、排序、分数完全一致，并断言新增返还均为零；覆盖 B08、B11、OPEN、节奏、导入方式与调性设置。不是只检查数量。
- FULL/SKELETON 为视图状态：ProductizationTests 验证切换不触发重新分析；Matcher/Analysis 测试验证双表示；没有改造输入模式或声称已执行 Cubase 点击测试。
- Snapshot：MidiExportTests 的推荐 Snapshot 往返、版本兼容，以及可靠性矩阵内 Enrichment Snapshot 往返；未修改结构。

持久回归代码保留在 RecommendationReliabilityTests。诊断文本与 MIDI 留在忽略的构建目录，不进入生产数据库。

## 5. 性能、风险与宿主边界

性能固定为同一进程、Release、同一 Factory 索引、固定 R01/4QN/4/4 查询。先预热 OFF/ON 各10次，再交替次序各80次；计时包含完整推荐。独立重复10000次条件判断，并重复2000次五候选检查和诊断构造。最终数值见下方执行结果。

内存测量在测试进程中对当前线程普通 C++ new/new[] 的申请次数与累计请求字节计数，范围是推荐调用。它衡量分配流量，不是进程驻留峰值、泄漏证明或音频线程测量；本路径的普通容器和字符串由此覆盖。测试测量代码不链接到插件。

风险：仍可能把音乐上不自然、但满足半音与共同音结构的插入解释为经过音；它不证明节拍重音、声部倾向、风格或听感合适。保守的完整核心性质、连续轨迹及共同音限制会漏掉部分合理扩展和弦或更复杂插入，这是本切片的边界。本阶段没有把扩展和弦视为完全相同，没有改变 Color 召回。

性能样本受系统调度影响，不用单次中位数的下降声称提速，也不将复杂度估计当作无开销证据。没有经决策端批准的正式性能预算，测量结果只供审批。

真实 Cubase 待验收：成功/失败原 MIDI 与时间、识别和弦、实际调性及拍号、FULL/SKELETON、intent、OPEN、UI候选及来源、本地诊断导出；人工试听 A08 的循环边界。尚未构建可启用该内部开关的宿主验收包，现有插件仍走默认关闭路径，不能以独立测试冒充宿主验收。

## 6. 回退与数据保护

- 运行行为关闭：内部开关保持/设为 false 即走原扣分分支；无数据迁移、无快照重写。
- 生产源码独立回退：`build-v4-dev2/reliability/phase3a-production.patch` 只包含 ContinuationEngine.cpp/.h；本轮已执行 `git apply --reverse --check` 验证可反向应用，**没有实际执行回退**。
- 回退源码后需同步移除本阶段依赖新接口的测试片段；不得整份恢复 RecommendationReliabilityTests，因为其中有进入本阶段前就存在的测试及诊断。已有308行阶段改动必须保留。
- 不提交、不推送、不合并、不创建 Tag；没有触碰正式发布或安装器。Factory 数据与数据结构不变。测试使用构建目录及既有临时测试库，没有访问真实用户库。

新增持久产物为本报告；NOW 只增加简短当前状态及链接。新增构建产物为路径与评分诊断、基线文本副本、测试 MIDI、生产差异补丁、构建与回归日志。先前 Phase 1/2 的三份未跟踪报告和测试代码均保留。

## 7. 最终执行结果

### 实际运行

最终源码构建成功。RecommendationReliabilityTests strict 返回0，`matrix failures=0`；33组专项通过，其中20组正例获得单个事件返还，13组负例/普通/已解释对照均未获返还。20组包含12个移调，不能视作20条独立音乐质量证据。

| 对照 | 可见结果 OFF→ON | 检查结论 |
| --- | --- | --- |
| R01原始、B小调、转位加八度、单独转位、单独升降八度 | 各0→1 | 仅A08获补偿 |
| 12个半音移调 | 各0→1 | 根音平移不影响资格 |
| 下降经过减七 | 0→1 | 单事件返还 |
| 短经过减和弦 | 1→1 | 原本已有显示；资格成立但数量不是成功标准 |
| F–F#dim7–G–C | 4→4 | 已解释，不重复补偿 |
| 非经过 Am–C#dim–D–E | 1→1 | 原有普通结果保留，没有新补偿 |
| F–F#dim7–Gm–C 核心冲突 | 3→3 | 未获得新补偿；不是宣称原算法完全没有模糊结果 |
| 无减和弦 Am–G–D | 5→5 | 无补偿 |
| 其余负例 | 各0→0 | 不新增显示结果 |

7项轨迹/ID检查通过（日志汇总字段名为 malformedTraceChecks，其中包含1项ID重命名正向检查）；另有真实推荐入口的非循环耗尽检查、四种原罚分检查、输出MIDI字节相同检查。原有1440组的开关对照完全一致。

修改前后矩阵SHA-256均为 `3B3494E862DAA0F26538CC550BF3740A0F3AC0BF140E650C0FBB8318F4858909`；Phase3A-0诊断文本前后均为 `B5E325FA91E50C5C335E6BE127149C492C3A5C20905859A79796E9960C7554F2`。这支持默认关闭行为保持，不以源码看起来相同替代运行证据。

最终12项定向CTest全部通过，0失败，总15.62秒：FactoryV4Focused、LibraryV3_snapshot、EnrichmentTests、UILayoutTests、MidiExportTests、MidiImportTests、PreviewTests、ProductizationTests、HarmonyAnalysisTests、ProgressionMatcherTests、ContinuationEngineTests、CubaseReliabilityFocused。可靠性矩阵另行直接运行通过，故不是这12项中的重复计数。CubaseReliabilityFocused 是模拟输入测试，**不是实际Cubase运行**。未运行全量正式候选回归，正式构建前仍需执行。

### 实测性能

| 指标 | OFF | ON |
| --- | ---: | ---: |
| 完整推荐中位耗时（80次，微秒） | 1075.6 | 1075.4 |
| P95耗时（微秒） | 1863.9 | 1830.9 |
| 有效路径数 | 5 | 5 |
| 显示候选数 | 0 | 1 |
| 每次累计普通C++分配请求字节 | 1,456,669 | 1,458,333 |
| 每次普通C++分配次数 | 7,546 | 7,555 |

新增分配流量为1,664字节、9次/推荐调用，包括诊断及新增显示结果的容器开销；不能全部归因于条件判断，也不能称为驻留内存增加。关闭时诊断容器为空，不新增其堆分配；RecommendationSet 本身多了一个空容器字段。

条件判断独立均值0.91298微秒/次（10000次全通过）；五候选核验及诊断构造均值10.4087微秒/批（2000批、10000条记录），该测试包括线性查找模板的额外开销，不等于生产评分阶段的精确增量。完整推荐中位差异约−0.02%，在噪声范围，不据此宣称提速或零开销。

### Git与产物

结束时分支仍为 `library/v4`，HEAD仍为 `42a915bda447457bd1bdc8b2f79a90e5ddc69c00`，没有提交或推送。Factory仍为657条，SHA-256仍为 `74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534`。

工作区：

- 修改：ContinuationEngine.cpp（新增87行）、ContinuationEngine.h（新增11行）、RecommendationReliabilityTests.cpp（相对HEAD共新增476行，进入本阶段前已有308行，本阶段新增168行），以及NOW状态链接。
- 新增未跟踪：本报告。
- 保留此前未跟踪：PHASE1_FACTORY_V4_ABSTRACT_MATCH_AUDIT_20261011.md、PHASE2_FACTORY_V4_ABSTRACT_MATCH_DESIGN_20261011.md、PHASE2_DECISION_APPROVAL_SUMMARY_20261011.md。
- 没有Factory源数据、Schema、User Library、Session/Snapshot生产文件、安装器或版本文件差异。没有修改main、正式Tag或发布产物。
- 构建目录新增：phase3a-path-verification.txt、phase3a-implementation-diagnostics.txt、phase3a-baseline-matrix.tsv、phase3a-baseline-diagnostics.txt、phase3a-production.patch、phase3a-A08-verified-path.mid、phase3a-A08-engine-output.mid、phase3a-R01_INVERSION_ONLY.mid、phase3a-R01_OCTAVE_DOWN.mid、phase3a-R01_OCTAVE_UP.mid，以及phase3a-build*.log、phase3a-regression*.log。既有phase3a0 MIDI和既有测试输出保留/按现有入口重新生成。

`git diff --check`通过（仅提示既有测试文件将在Git操作时规范换行）；生产补丁反向检查通过，未执行回退。完整证据位于忽略的构建目录，审批时应随报告保留，清理build会删除这些诊断。

PHASE 3A IMPLEMENTATION READY：YES

DEFAULT ENABLED：NO

READY FOR CUBASE ACCEPTANCE：NO

READY FOR FORMAL RELEASE：NO

Cubase标为NO的原因：只完成源码与独立测试，没有创建可启用内部开关的宿主验收构建；既有插件继续默认关闭。是否制作受控验收构建、是否启用及是否接受音乐质量，留待决策端批准。到此停止，不进入Phase3B。
