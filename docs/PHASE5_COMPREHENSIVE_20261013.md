# Phase 5 综合开发与验收报告

日期：2026-10-13。代码构建提交：`3a582e4d8ea9bca5df4507a3b7abfabb08d2ecd9`，版本 `0.10.0-dev.6`，开发分支 `development/phase5-20261013`。本报告覆盖用户工作包 A–I；二进制冻结于该代码提交，后续 HEAD 只更新交付文档。

## 状态

| 最终状态 | 结论 |
|---|---|
| PHASE 5 AUTOMATED WORK COMPLETE | YES（自动化与交付；实机及试听仍待验收） |
| DEV5 HISTORICAL TASKS RECONCILED | YES（10/10核销，7 DONE / 3 PARTIAL） |
| FACTORY ABSTRACT RETRIEVAL IMPROVED | YES（独立实验，收益范围见 B/F） |
| R01 TARGET CONTINUATION | STAGED |
| R02 COLOR OFFLINE STATUS | UNCHANGED（本轮Abstract及新增内容的Color差异） |
| R02 REAL CUBASE ROOT CAUSE | UNKNOWN |
| ISOLATED LIBRARY VALIDATED | YES（技术验证，音乐采纳待定） |
| FACTORY V4 BASELINE CHANGED | NO |
| HARMONYFIRST DEFAULT ENABLED | NO |
| PHASE 3A DEFAULT ENABLED | NO |
| ABSTRACT DEFAULT ENABLED | NO |
| OFF REGRESSION | 33/33 |
| ON REGRESSION | HF-ON 33/33；ABSTRACT-ON 33/33 |
| USER LIBRARY PROTECTED | YES（隔离验证范围；真实库未操作） |
| GIT STATUS | development/phase5-20261013；代码3a582e4；文档HEAD/远端HEAD/干净状态见git-proof.json |
| READY FOR FUTURE CUBASE ACCEPTANCE | YES |
| REAL CUBASE ACCEPTANCE COMPLETED | NO |
| READY FOR FORMAL RELEASE | NO |

真实 Cubase、人工试听及正式发布未执行。默认 HF、Phase 3A、Abstract 均为 OFF；候选内容不进入默认 Factory。自动化完成不表示所有音乐和宿主问题已解决。

证据根为交接包内 `evidence/`。原始 TSV、CSV、MIDI、日志及各自来源保留；下文的证据文件名均相对该目录或明确注明的子目录。本机隔离验证目录为同级 `HarmonyContinuation-phase5-validation-20261013`，交付目录为同级 `HarmonyContinuation-phase5-delivery-20261013`；不依赖开发机器的绝对路径。

## A. 历史核销与工程身份

接手基线为 `dd744069694425c174aaef5d1a990f925e4afed9`，当时与 dev.5 远端分支一致。dev.5 冻结二进制构建提交为 `730e819`，交接文档为 `c117163`；这些历史身份和测试不计入本轮门禁。

| dev.5 遗留项 | 核销 | 证据边界 |
|---|---|---|
| HF 实际状态与 PENDING | DONE | 代码及历史 OFF/ON 诊断，真实宿主另验 |
| P3 实际状态独立显示 | DONE | 默认 OFF，结果记录实际调用值 |
| A08 ID | PARTIAL | 离线追踪确认 V4_A08；旧截图身份缺原始诊断 |
| 3/4 输出 | DONE | 六拍号离线 QN/SMF 证据 |
| Preview/MIDI 一致 | DONE | 实际音符序列比对 |
| FULL/SKELETON 差异样例 | DONE | 合成 MIDI 与 HF 两态结果 |
| R02 诊断 | PARTIAL | 离线追踪齐备，真实成功/失败输入未采集 |
| 半音低音与侧移回归 | DONE | V4_B08/B11 的低音、性质及 MIDI 检查 |
| 隔离候选库 | PARTIAL | 技术验证与来源齐备，试听/采纳待定 |
| User Library/Git/安装保护 | DONE（隔离范围） | 历史安装证据；本轮另外修复并发初始化并重新验收 |

十项已逐项核销，7 DONE / 3 PARTIAL。详细历史来源见 `phase5/history-audit.md`，其中路径与行号对应核销时的源码；最终状态以本报告及本轮原始日志为准。

## B. Factory 抽象检索增强

现有链路为 Normalize/Analysis → MatchQuery → Factory shortlist → FULL/SKELETON 最佳对齐 → minimumMatch → 尾部/明确性质与低音验证 → 有效续写 → score → 去重与分组展示。657 条库低于 800 shortlist 上限，当前扫描没有因该上限截掉条目。开发 worker 每组保留 10 项，默认展示前三项。

独立开关 `HC_EXPERIMENT_ABSTRACT_RETRIEVAL_PLUGIN` 默认 OFF。开启后，仅对双方均有明确音集、同实际根音、同且唯一核心三度、查询为模板严格音集子集、明确七度与低音不冲突的方向性匹配，降低显式音集差异费用。Major→Dominant7 还要求双方均为 Dominant 功能；缺纯五度而含改变五度、sus/dim/aug 等不获优惠。反向简化、根音或核心性质冲突、相等音集、旧 mask=0 条目保留既有费用。

该修改把满足条件的显式音集差异费由 0.30 改用既有扩展差异费 0.07；原有和弦性质费仍保留。它属于软兼容，不将 C 与 Cmaj9 记为完全等价。追踪新增 `AbstractExtensionCompatibility`，不改变阈值、尾部、评分、去重或 Factory 后缀；Snapshot 未增加此开关或 reason 位。实际调用状态贯穿 worker、结果、About 与本地诊断。

实现提交 `f0d7cd5`；后续 `7283dc0` / `3a582e4` 修复用户库首次写入竞争及现有测试，并保留 Factory 确定性编译。核心入口为 `src/core/ProgressionMatcher.cpp`、`ContinuationEngine.*`、`src/plugin/DevelopmentMode.h`，诊断入口为 Controller/MainView/ProductServices。

最终真实 Factory 定向结果：C→Db 的 V4_B11 相似度 0.596031→0.809685、费用 0.74→0.28，由 minimum_match 淘汰变为默认 Top3；三和弦 C→Db→C 为 0.598990→0.812644，同样进入 Top3。完整扩展音前缀仍精确匹配（两项原始 cost=0 / similarity=0.9762）。模板实际路线为 Cmaj9→Dbmaj9→Cmaj9→Am9→Dm9→G13→Cmaj9；测试中的 5/6 项前缀也来自该实际路线。13 个兼容例包含 4 个方向性收益、9 个不获优惠的负例，均通过；0/9 是此有限硬负例集的错误优惠数，不能外推为全输入误匹配率。代表性 B11 后缀经过 Snapshot、Preview 与实际 SMF 的音符/音集/QN 一致性验证。

## C. R01 与候选路线

目标完整路线：Am→G#dim→G→D/F#→F→G→Am→C。对 V4/657 扫描所有 12 移调形状，完整八和弦、原四和弦前缀及目标四和弦后缀均无命中。A 小调 F–G–Am 有 3 个局部窗口，跨模式形状有 7 个；局部命中不能组成目标完整续写。对应明细在 `content-audits/content-route-*.csv`。

`V4_A08` 是不同路线，其本轮确认基线为 2QN、OPEN、固定 A minor、HF ON/P3 OFF/Abstract OFF，分数 60.904，默认可见。4QN、OPEN、固定 A minor、三个开关 OFF 的既有结果为 score 56.405 / eligibility 55.484，因分数门槛淘汰；两项同时改变时值与 HF，不能作单变量因果比较。

隔离候选 `CANDIDATE_V5_CHROMATIC_MINOR_001` 提供完整目标路线，V5/658、Schema 2、4/4、八项各 4QN、Develop、非循环、cadence none。只有 D/F# 显式存有 bassInterval=4，其余低音未声明；不能将 null 当已确认根位。G#dim 为 Embellishing，D 的功能标记 Unknown；A–G#–G–F#–F 为音乐设计线索，试听 MIDI 是规则化实现，非作者录制。续写保留原输入，只附加原型后缀。目标状态为 **STAGED**。

候选 JSON、音乐说明及来源见 `data/factory-proposals/20261012/`；候选评审 ZIP 同时含 V4 对照、V5 DB、源 JSON、内容审计及六拍号 MIDI。试听、正式采纳和激活 V5 均待决策。

## D. R02 Color 离线诊断

本轮 96 组覆盖 V4/V5、Auto/C major/A minor、4/4/closed 与 3/5/OPEN、HF/P3/Abstract 各两态，保存各阶段数量、淘汰原因及完整 trace。固定 C major 与 A minor 各场景 Color 均为0；Auto等时closed为HF OFF=0 / ON=5，Auto 3/5/OPEN为两态均5。Abstract两态、候选增条及P3不改变这些Color数量；V4/V5各48组Color数量合计均60。因此本轮 Color 离线状态为 **UNCHANGED**，不降低门槛或人为补入无有效尾部的 Color。真实 Cubase 输入、成功/失败诊断及安装身份未补充，根因仍 **UNKNOWN**。

历史 C major 模拟输入的 89 条 Color 中，84 条 major 对齐都低于 minimumMatch=0.60，1 条未形成对齐记录，4 条 minor 被模式过滤；14 条对齐后仍有尾部，也不能越过匹配门槛。CITYPOP_JPOP_020 的存储 mask 均为零，没有显式音集差异费，本轮实验不应据此宣称修复该例。历史逐条审计见 `phase5/match-audit.md`；本轮证据以 `phase5-final/phase5-r02.tsv` 及 trace 为准。

## E. 隔离内容质量与版本提案

V4/657 SHA-256：`74c529828f8c4b934bbaf4ada719aa95491ab498b58885522124ff6505671534`。

候选 V5/658 SHA-256：`db15757038c6c333e386499ef5be5480a5d279215b34020e403a92e0ba607297`。仅新增上述候选；旧 657 条 payload 逐条不变，JSON 与 DB 的 typedData 字段经浮点规范化后相同。

严格指纹保留 mode/meter/相对级数/性质/target/roles/音集/bass 声明/精确 QN，null 与显式 0 分开。658 条严格重复 0 对、移调形状等价 1 对、仅低音变化 0 对、仅节奏变化 18 对、四项以上仅一和弦变化的近似关系 137 对；新增候选相关关系 0。移调等价的 V3_CYCLE_014 与 V3_CYCLE_029 保留不同调内功能，不机械删除。编译器自有 canonical 口径另报告 Exact Musical Duplicate=0 / Missing Metadata=0。

新增仅使 Pop 419→420、Rock 212→213、Minor 184→185、Develop 91→92、ModalMixture 45→46。风格为多标签，不可相加为记录数。建议保留 V4 默认，仅让单条 V5 候选进入音乐评审，不继续扩库、不设风格配额、不按相似关系自动删内容。完整指纹、关系和覆盖在 `content-audits/`。

## F. 召回、多样性与精确匹配保护

最终 A/B/C 同语料对照：A 为 V4/657 Abstract OFF，B 为同 V4 Abstract ON，C 为隔离 V5/658 Abstract ON；HF/P3 均 OFF。语料从 V4 实际可续写路径派生 2/3/5 和弦完整与简化前缀，去重键保留模式、音名与 QN，同一查询保留全部目标 ID。完整输入785个唯一查询、简化582个；每世界/变体均为1,579个目标×查询对，不能把目标行数当唯一查询数。

| 变体/世界 | eligible | 来源支持>0 | 默认可见 | worker Top3 | worker Top10 |
|---|---:|---:|---:|---:|---:|
| exact A/B/C | 1545/1579 | 1448/1579 | 1191/1579 | 1209/1579 | 1448/1579 |
| simplified A | 1452/1579 | 1110/1579 | 874/1579 | 888/1579 | 1110/1579 |
| simplified B/C | 1454/1579 | 1113/1579 | 876/1579 | 890/1579 | 1113/1579 |

简化目标有效召回91.96%→92.08%，增2且无损；默认可见/Top3增2、Top10增3。精确目标上述指标不变。新候选在C有10次默认主卡曝光，均为Top3；B→C在完整/简化语料各增5个query-card结果，Top3/10均无旧卡损失。

目标保护和主卡保护须分开：A→B已经发生两项旧完整输入主卡变化，C延续相同变化。C–E7–Am下 V3_HOME_011 从rank10退出，原后缀Em–Dm–G–C，V4_B14以Ab13–G13–Cmaj9进入rank5；C–Am–A7–Dm–G下 V3_HOME_018 从rank3降至4，原Am–F–G–C及score62.4213不变，V4_B11以Cmaj9后缀升至rank3/score68.7132。这些是实验兼容改变竞争排名的实例，非新增658内容挤压；尚需音乐评审，因此保持默认OFF。明细在 `phase5/phase5-analysis-old-exact-primary-losses.csv`，无候选的查询也计入覆盖分母。

主卡曝光只计默认呈现的 primaryID，不把 supportingID 当主卡。Top10/20/50 指跨语料最常出现 ID 的累计展示份额；worker 每组仍为 10，未扩为 50。有限合成语料曝光不能解释真实用户使用分布，也不能与历史不同语料的份额直接比较。原始 coverage/delta/visible-candidates 和分析 JSON/CSV 保留计数分母、路径及得失原因。

| 世界 | 默认主卡次数 | 曝光ID/总ID | 零曝光/仅一次 | Top10份额 | Top20份额 | Top50份额 |
|---|---:|---:|---:|---:|---:|---:|
| A | 7433 | 628/657 | 29/16 | 9.71% | 16.47% | 30.27% |
| B | 7448 | 628/657 | 29/16 | 9.69% | 16.43% | 30.18% |
| C | 7458 | 629/658 | 29/16 | 9.68% | 16.41% | 30.14% |

最高频均为V3_CYCLE_046（94次），新增候选10次、频率rank231。C默认Intent为Resolve2638/Develop1366/Loop2464/Color990；风格Pop5795/Rock3390/Functional3161/Jazz1834/CityPop1440/Rnb848，风格多标签重叠。各世界Color主卡均990次，完整/简化有Color的查询分别353/785、247/582。按显示音名+QN聚合的重复后缀组为709/710/711，其中各232组含多个primaryID；这类跨语料重复不等于同查询重复卡或应删除的内容。完整高频/低频ID、风格、Intent及路径明细随分析CSV交付，未引入随机重排或配额。

## G. HF 与 Phase 3A 收敛建议

两个机制及 Abstract 默认均 OFF。本轮保存 V4/V5 × HF/P3/Abstract × 明确/未声明低音的 32 组及已确认 A08 基线，结果记录实际调用状态；既有 Phase3A 专项检查结构、插入惩罚、重复补偿、负例及 MIDI 不变。

HF 是全局检索时值/结构策略，P3 是局部插入补偿。后续可优先评审 HF 是否覆盖 P3 的收益，再决定弃用局部补偿；当前自动化证据不能代替音乐评审，因此保留实现与可回退 OFF 行为，不合并默认、不删除历史机制。

## H. 回归、并发修复、性能与兼容

最终代码 `3a582e4` 的三模式全目标构建均通过，实际门禁如下：

| 构建 | HF/P3/Abstract | CTest | VST3 Validator | V4 专项 | 隔离 V5/658 专项 |
|---|---|---:|---:|---:|---:|
| DEFAULT-OFF | 0/0/0 | 33/33 | 47/47 | 13,048 | 41,105 |
| HF-ON | 1/0/0 | 33/33 | 47/47 | 13,048 | 41,105 |
| ABSTRACT-ON | 0/0/1 | 33/33 | 47/47 | 13,048 | 41,105 |

每模式重新执行既有 HarmonyAnalysis、Matcher、Continuation、MIDI、Preview、Enrichment、UI、Factory/User 契约、Phase3A、Recommendation/Cubase reliability 及 SDK 合成宿主生命周期。SDK 中的 Cubase/FL/Generic 模拟不是实机验收。三份原 1440 组矩阵 SHA-256 均为 `8dc3784e84ce4bc6d99ec5b1f70ce02214eb69ec3d81326fdeb26fd0663e54d8`，与 dev.5 相同；每份 1440 数据行加表头。

最终 OFF 同步起跑 20/20 通过；默认 Setup 隔离生命周期 21/21 通过，专用 HKCU 清除、V4/插件卸载、V3/用户库/设置哨兵保留。Setup SHA-256：`dff8e0578761130498357ad7829649754eecc25cbc24b5720aef400df728c21d`。

构建环境：Windows 10.0.26200 x64、i5-13500H / 16 逻辑核、MSVC 19.51.36260.0、CMake 4.3.1-msvc1、离线 VST SDK 3.8.1。Release `/O2 /Ob2 /DNDEBUG`，`/INCREMENTAL:NO`；本机 PDB 工具不匹配以 `_CL_=/Z7` 解决，不改 SDK。测试 TEMP/TMP 指向独立工作区，避免沙箱临时目录的原子替换限制。

最终 Phase5 专项 8,361 检查通过，1,367 个唯一查询、9,474 条目标对照行。B11 24 组、R02 96 组、开关 32+1 组、13 个兼容例和实际 SMF 均已执行。性能在其他构建、重负载测试与压缩结束后独占采样，每组预热 10 次、测量 80 次：

| 世界 | 前缀项数 | p50 ms | p95 ms | max ms | 90 次总 CPU ms |
|---|---:|---:|---:|---:|---:|
| A V4 OFF | 2 | 4.9308 | 5.7371 | 6.5566 | 453.125 |
| B V4 Abstract ON | 2 | 4.7129 | 5.7926 | 7.5474 | 453.125 |
| C V5 Abstract ON | 2 | 4.7103 | 6.4324 | 8.0093 | 437.500 |
| A | 5 | 2.1025 | 3.2100 | 3.4751 | 203.125 |
| B | 5 | 2.0111 | 2.4562 | 3.5932 | 187.500 |
| C | 5 | 1.9847 | 2.3188 | 2.6378 | 187.500 |

A/B/C shortlist 为 657/657/658，采样进程 RSS 从 24,653,824 到 24,805,376 字节，累计峰值 25,157,632。单次有限采样中 C 短前缀 p95 比 A 高约 12%，绝对差 0.6953ms；这不构成真实宿主时延分布的证明。

2x/4x 分别复制原型并更换 ID，形成 1,314/2,628 条内存索引，只测 matchProgression，不与上表端到端推荐直接相除。两者 shortlist 均限制 800；短/长前缀 p95 分别为 2.1018/3.5692 与 3.4115/3.8264ms，4x 累计 RSS 峰值 31,817,728 字节。这是索引容量实验，不代表新增音乐内容质量。高频刷新、重复导入、缓存失效、BPM/QN 缩放和六拍号行为由既有 reliability/Phase4D 专项覆盖；量化性能仅覆盖表列场景。

首轮 f0 OFF CTest 32/33，HostContractGeneric 的八线程首写仅保存七项，原日志缺 writer 错误。定向重复还发现 PID 临时目录复用造成 progressions.id 冲突；现有测试改为原子创建唯一目录，并保留逐 writer 错误。加入八线程 barrier 后，在第五次干净数据库运行复现 2/8 成功，六项 `UNIQUE constraint failed: metadata.key`。

用户库初始化原先分步检查/写 metadata，缺少统一事务。修复在三个 UserLibrary 写入入口使用 `initializeUser`，以 `BEGIN IMMEDIATE` 包围既有初始化主体，提交前释放 Statement，异常回滚；调用均在其他显式事务之外，迁移流程及 2000ms busy timeout 保持。遵循 [SQLite 事务契约](https://www.sqlite.org/lang_transaction.html)，未增加锁框架或重试。Factory 编译器独占未发布的 `.new` 数据库，沿用原 metadata 写序；中间统一事务版虽内容测试通过，但改变了 Factory 字节，被哈希门禁拒绝。最终修正恢复原 V4 哈希。修复后 barrier 定向 20/20、曲库/迁移/产品化 6/6 通过，最终身份的复验另列门禁。原始七项成功的错误文本无法恢复，不能把后来的复现错误直接等同于原日志；首轮及每轮失败证据均保留。

Factory Schema 2 / User Schema 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2 保持。未操作真实 user.db、宿主工程或生产安装；生命周期只在独立 TestRoot/HKCU 进行。音频线程未加入数据库或匹配工作。

## I. Git、包与统一交付

构建提交与后续文档提交分开；后者只更新 docs/ 和 NOW/MAP/RUNBOOK/PROJECT_INDEX，不改变已验证代码。同步仅使用新安全开发分支与本机已核实代理；最终本地/远端HEAD、文档差异及干净状态在 `git-proof.json`。main `bf770192c165ec55a73a80e0c61ffe2ff5696b23`、v0.9.0 `ddc4f89babd4520daac3eae74824d6f581ce0e75`、Factory V4 保持，不强推、不建正式Tag、不正式发布。

交付包含三模式便携 ZIP、默认 OFF Setup、候选V5/658评审ZIP、Git源码ZIP及 `HarmonyContinuation-Phase5-Handoff.zip`。`BUILD_INFO.json`区分BuildCommit与HandoffHead；每个ZIP包含内部文件清单，外层SHA256SUMS记录逐包哈希。候选库明确标为V5/658且不默认加载；SDK、构建缓存、真实用户库、私有音乐和宿主工程不随包分发。

完整交接包及独立 `HANDOFF_PHASE5_SHA256SUMS.txt` 可增补至既有未发布开发草稿；实际上传状态以本轮GitHub读回proof为准，保留旧六个资产及草稿元信息。跨设备流程见RUNBOOK，直接接续词见 [PHASE5_HANDOFF_PROMPT](PHASE5_HANDOFF_PROMPT.md)。

主代理负责计划评估、匹配语义、回归性质判断、SQLite修复方案与最终关键证据复核；子代理分别承担历史/内容审计、规格明确的实现与构建回归、统计解析、安装与交付整理。代码及验收结论由主代理统一审核，子代理报告未直接代替门禁。

| # | 统一交付 | 权威入口 |
|---:|---|---|
| 1 | Phase 5 完整报告 | 本文 |
| 2 | dev.5 核销矩阵 | A 与 phase5/history-audit.md |
| 3 | 抽象检索及差异 | B、f0d7cd5 与最终专项 |
| 4 | A08 确认证据 | C、switch matrix、既有 R01 trace |
| 5 | 目标 F–G–Am–C | C、隔离候选与路径扫描 |
| 6 | R02 离线诊断/可续写性 | D、96 组 TSV 与完整 trace |
| 7 | 内容及重复分析 | E、content-audits/ |
| 8 | 内容版本提案 | E、候选源说明 |
| 9 | 候选 MIDI/音乐说明 | 候选评审包、六拍号测试输出 |
| 10 | 召回/多样性对照 | F、最终分析 CSV/JSON |
| 11 | HF/P3 整合建议 | G、开关矩阵及 Phase3A 专项 |
| 12 | 自动化与全回归 | H、三模式原始日志 |
| 13 | 性能 | H、原始 performance TSV |
| 14 | 用户数据与兼容 | H、曲库及隔离安装证据 |
| 15 | Git 状态与提交 | I、git-proof.json |
| 16 | 开发验收包与 SHA | BUILD_INFO、SHA256SUMS、逐文件清单 |
| 17 | 中文 Cubase 清单 | [集中人工验收](PHASE5_CUBASE_ACCEPTANCE.md) |
| 18 | Release Notes 草案 | [dev.6 说明](PHASE5_RELEASE_NOTES_DRAFT.md) |
| 19 | 未解决问题 | 下节与各原始证据 |

## 后续人工项目

真实 R02 成功/失败诊断、旧 A08 截图身份、候选与扩展音音乐试听、真实宿主安装/拖放/保存重开均待集中验收。FL Studio 仍 Not Verified/Pending。复合分组与历史变拍 map 未获新证据。本机旧 HF-ON 安装被 UAC 取消，不能当作已安装 dev.6。正式发布仍需后续独立决策。
