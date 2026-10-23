# HarmonyContinuation Phase 4B 收尾：历史工作包核销与补证记录

日期：2026-10-12
工作区：`D:/VibeCoding/HarmonyContinuation-v4`
本文件先记录历史核销，随后追加 A–H 补证、回归修复与最终状态。此报告不代表人工 Cubase 验收。

## 核销口径

- `DONE` 仅表示有对应代码、数据或测试证据覆盖该阶段授权范围；不会把模拟当成宿主证据。
- `PARTIAL` 表示有有效子项，但仍缺该工作包要求的重要覆盖或当前构建证据。
- `MISSING` 表示未找到足以证明执行的材料。
- `BLOCKED` 表示后续工作依赖的质量门禁尚未通过。
- 下表“补做结果”在开始本轮补证前统一为“待执行”；之后会更新，不追溯性改写成历史通过。

## 历史阶段核销

| 阶段 | 原任务与验收范围 | 实际操作 / 代码与数据证据 | 构建及开关状态 | 状态 | 缺失项与是否补做 | 补做结果 |
|---|---|---|---|---|---|---|
| Phase 0 | 确认工作分支、HEAD、Factory 657 条及哈希、Schema/库版本、测试基线 | `docs/PHASE1_FACTORY_V4_ABSTRACT_MATCH_AUDIT_20261011.md` §1 记录了基线、Factory `quick_check` 与包内/开发库 SHA-256；本轮 Git 与 Factory 文件再核验 | dev.3 / `build-v4-dev2`；审计前工作区干净；当前工作树另有后续授权的未提交更改 | DONE（与 Phase 1 合并记录） | 无需重做历史审计；本轮复核当前 Git、数据库哈希 | 待执行 |
| Phase 1 | 只读审计匹配链、R01/R02 静态覆盖及候选消失未知项；不改生产行为 | 同上 §2–6；代码路径、门槛、Factory 静态窗口、真实输入缺口均有记录；静态扫描指出精确路径不等同于近似召回 | dev.3；只读；静态检查不是运行时根因证据 | DONE（范围内） | 历史静态审计已完成，不重复；保留其证据边界 | 不重复 |
| Phase 1B | 用模拟 MIDI 复现 R01/R02，逐阶段定位，不冒充 Cubase | 同上 §7：R01 MIDI/识别/5 条续写/`minimum_score`；R02 T1–T4及7个窗口；Factory ID、OPEN 与原因；构建目录保留输入、诊断与 MIDI | `build-v4-dev2` Release；模拟 MIDI 导入；生产匹配行为未改 | DONE（受控模拟范围） | 真实 Cubase 两组输入仍缺；其采集属于宿主证据，不以模拟补做 | 不重复模拟；宿主待用户验收 |
| Phase 2 | 用真实代码和候选数据完成抽象匹配、Color、评估及分阶段方案设计 | `docs/PHASE2_FACTORY_V4_ABSTRACT_MATCH_DESIGN_20261011.md` 与 `docs/PHASE2_DECISION_APPROVAL_SUMMARY_20261011.md`，包含 R01 五候选、R02 Color 消失点、兼容边界与评估设计 | 只读设计；未改生产算法 | DONE（设计交付） | 不重做已接受设计；落实与否留待本轮 Phase 4B 的独立审计 | 不重复 |
| Phase 3A-0 | 离线验证减和弦经过音评分例外、正负样本、反事实与可行性 | `docs/PHASE3A_CONTROLLED_IMPLEMENTATION_20261011.md` §1–5、§7；`build-phase4b/reliability/phase3a0-*` 与 `phase3a-*` MIDI/诊断；报告记载33组专项、1440矩阵及音频/MIDI语义核验 | Release 测试；补偿开关 OFF 是主基线，专项另测 ON；不是 Cubase | DONE（离线授权范围） | 人工听觉与真机验收未授权/未完成，不影响离线可行性核销；本轮核查四组合是否在同一构建覆盖 | 待检查组合证据 |
| Phase 3A | 严格条件、默认关闭、单事件减和弦插入评分例外及回归 | `docs/PHASE3A_CONTROLLED_IMPLEMENTATION_20261011.md` §2–7；仅 `RecommendationWeights::diminishedPassingInsertionException` 默认 false；33组、12项相关 CTest、1440组关闭等价；Factory/MIDI 路径检查 | dev.3 / `build-v4-dev2` Release；OFF 为插件默认，ON 仅测试注入 | DONE（受控实现）；Cubase/发布明确未就绪 | 真机验收因此前阶段门禁暂停，不计作遗漏；本轮检查与 HarmonyFirst 的重复补偿 | 待核查四种组合 |
| Phase 3-SYS-AUDIT | 评估 Factory 657 条可达性、评分、曝光/多样性、变形稳定性及性能 | `docs/PHASE3_SYSTEM_AUDIT_20261011.md` 全库受限空间追踪；伴随 657 行可达性、5801次运行、266条消融、220个变形、6个试听 MIDI 等产物 | Release / Factory 657；Phase3A OFF；核心引擎，不是 UI 曝光或 Cubase | DONE（报告明确限定输入空间） | 不把受限输入空间外推到所有用户输入；不重跑大矩阵 | 不重复历史矩阵 |
| Phase 4A | 基于已有审计数据提出决策，不改算法 | `docs/PHASE4A_ALGORITHM_HEALTH_AND_DESIGN_DECISION_20261011.md`；含候选评分、曝光、兼容边界、方案、验收标准 | 只读设计；未运行新矩阵 | DONE | 本轮只核对其证据引用，不扩大既有审计 | 不重复 |
| Phase 4B 初始节奏审计 | BPM/QN/拍号依赖调查、提出隔离实验 | `docs/PHASE4B_RHYTHM_DECOUPLING_AUDIT_20261011.md` §1–11；BPM/QN调用路径已核，实际多拍号推荐差分明确未执行 | 只读源码 + 复用 Phase3 结果；Phase3A OFF | PARTIAL（其阶段门禁为 NO） | 其缺口由综合授权 A–G 补做；不将代码推导写成实测 | 待执行 |

## Phase 4B 综合工作包 A–H：补做前核销

| 包 | 原验收要求 | 现有证据及开关/构建 | 状态 | 缺少的具体证据 / 本轮动作 | 补做结果 |
|---|---|---|---|---|---|
| A 节奏与拍号 | 六拍号、BPM/QN成对对照、缩放/单项时值、OPEN、进入位置、代表样本；核实导入—分析—引擎—输出链路 | `docs/PHASE4B_INTEGRATED_IMPLEMENTATION_20261012.md` §3–6；331 MIDI / 1324调用，报告称拍号进入合成推荐，但未逐列证明授权中每种配对的真实导入和引擎证据 | PARTIAL | 核对既有 TSV、MIDI、测试入口；只补没有证据的组合，明确字段经 SMF 导入还是测试参数注入 | 待执行 |
| B 结构分类与评分耦合 | 横跨普通三和弦、减和弦、次属、借用、扩展、调性与上下文；定位重复奖励/惩罚和分类边界效应 | R01 13点时值扫描与 `phase4b-matches.tsv`；代码模式详细，但历史输入族不足以代表完整类别 | PARTIAL | 复核现有轨迹字段与源码；补正负类别覆盖，不对权重作生产修改 | 待执行 |
| C 方案比较 | 完整比较保守式、连续权重、和声/节奏分层三方案，含召回、误匹配、稳定、Color、多样性、性能、复杂度与回退 | 4种测试模式和对应TSV存在；模式名不能直接等同原方案 | PARTIAL | 以实现代码定义方案对应关系，并用同一现有固定数据比较，记录不具可比处 | 待执行 |
| D 受控实现 | 独立开关、默认OFF、OFF与基线一致、ON验证、核心和弦/Factory/MIDI不退化、可回退 | `RetrievalTimingMode` 默认 `Legacy`，测试注入 `HarmonyFirst`；没有宿主 OFF/ON 参数。1440矩阵哈希与 Legacy 基线一致；实验 MIDI 仅验证部分输出 | PARTIAL | 实际检查全调用链、默认行为、被测合约和可回退；确认是否有必要的最小修复 | 待执行 |
| E Phase 3A 兼容 | 同一构建测 HarmonyFirst×Phase3A 四组合；评分、候选、误例、重复奖励及性能 | Phase3A 既有33组 OFF/ON；Phase4B `phase4b-phase3a.tsv` 存在，报告只明确 R01重叠计算，不足以证明同构建33组四组合 | PARTIAL | 检查TSV范围；若未覆盖，复用现有测试入口做四组合，不删历史机制、两开关默认OFF | 待执行 |
| F 质量与全量回归 | 1440矩阵、33专项、B08/B11、657条契约、分析/匹配/续写/Color/FULL/SKELETON/Enrichment/Preview/MIDI/Snapshot/UI/Productization；最终全量通过 | 旧 Phase3A 12项通过不能代表当前；Phase4B报告记32项25过7败，另1440哈希相同、节奏专项通过 | PARTIAL | 先归因七项；修复真实缺陷；最终同一代码状态全量测试并核查登记数 | 待执行 |
| G 性能与稳定 | 延迟/候选/CPU/内存/高频刷新/长前缀/重复导入/开关与缓存/拍号/OPEN；旧行为与ON对照 | 仅有331查询的推荐时间与累计分配流量；无CPU、RSS、压力/导入/切换/缓存实测 | PARTIAL | 用现有测试入口/系统观测补最小基线与HarmonyFirst对照；资源口径限定，不能冒称VST实时指标 | 待执行 |
| H 宿主包 | 门禁全部通过后构建带开关开发版与安全安装包、ID/hash/验收清单 | 当前回归7项失败；当前VST3默认Legacy且无宿主切换；之前未打包 | BLOCKED | 只有所有适用自动门禁通过、且可提供独立OFF/ON方式才继续；否则输出阻断报告 | 待门禁 |

## 4B 原始失败集合（来自前次综合报告，待本轮日志逐项重验）

| # | 测试 | 既有报告观察 | 初始分类（未独立验证） |
|---:|---|---|---|
| 1 | `LibraryV3_user` | 测试库清理时文件仍被占用 | 未定位：清理/锁定可能 |
| 2 | `LibraryV3_factory` | 测试库清理时文件仍被占用 | 未定位：清理/锁定可能 |
| 3 | `LibraryStoreTests` | “repair damaged active pointer”断言失败，随后进程退出码 `0xc0000409` | 尚未定位，存在真实断言失败 |
| 4 | `ContinuationEngineTests` | 42项断言后删除临时 `user.db` 遇文件占用；旧 `build-v4-dev2`二进制曾通过 | 未定位：当前构建差异/文件生命周期 |
| 5 | `HostContractFLStudio` | 38/40；case27重连父级，case38 Factory数期望161 | case27未定位；case38旧版本预期已疑似过时 |
| 6 | `HostContractGeneric` | 同上 | 同上 |
| 7 | `HostContractCubase` | 同上 | 同上 |

初始核销结束。此处没有把任何待补项目写成通过。后续按 A→G 补证、按失败实际根因分类，最后才判断 H 是否解锁。

## 最终补证：Phase 4B 综合授权执行结果（2026-10-12）

本节是上方“待执行”表格的最终补证；前面的表格保留了补证开始时的真实状态，不作追溯改写。所有实验均在隔离 Release 构建中进行。自动测试与合成 MIDI 不代表 Cubase 真机或人工听觉验收。

### 最终阶段核销

| 阶段/工作包 | 结果 | 最终证据与边界 |
|---|---|---|
| Phase 0 | 完成 | 分支 `library/v4`，HEAD `42a915bda447457bd1bdc8b2f79a90e5ddc69c00`；Factory 版本4、657条；包内两个构建库 SHA-256 均为 `74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534`。Schema、User Library、Snapshot 等均未改。 |
| Phase 1 / 1B | 完成（静态审计/受控模拟范围） | 原报告和 `build-phase4b/reliability/phase1b-*` 保留。R01/R02 的合成样例仍不等于用户真实 Cubase 输入。 |
| Phase 2 | 完成（方案交付） | Phase 2 设计报告与审批摘要保留；本轮没有扩展为抽象兼容生产改造。 |
| Phase 3A-0 / 3A | 完成（离线验证/受控实现） | 减和弦插入补偿仍默认关闭。严格1440组矩阵为1440条数据行，SHA-256 与原基线完全一致：`3B3494E862DAA0F26538CC550BF3740A0F3AC0BF140E650C0FBB8318F4858909`。真实宿主验收未完成。 |
| Phase 3 系统审计 | 完成（原报告定义的受限空间） | 657条可达性、评分消融、稳定性等产物保留；受限样本不外推为任意用户输入的覆盖率。 |
| Phase 4A | 完成（决策分析） | 原报告保留；本轮未修改全局阈值或生产评分权重。 |
| Phase 4B-A 节奏/拍号 | 完成（离线证据） | 331个 MIDI 往返/样本检查、10个 `importFile` 探针、11个 Factory MIDI 输出检查、1324次引擎推荐。六拍号为4/4、3/4、6/8、5/4、7/8、9/8。MIDI 文件导入可读到元数据；推荐测试仍直接注入分析上下文拍号，未证明 Cubase 实时 ProcessContext 的端到端传递。 |
| Phase 4B-B 结构分类 | 完成（离线证据） | R01 G#dim 时值扫描13点；保留 MIDI、结构权重、匹配轨迹和评分诊断。未将离线样本的结构关系外推成普遍音乐质量结论。 |
| Phase 4B-C 方案对照 | 完成（限定实现对照） | 用同一合成集比较 Legacy、BarPositionIndependent、ContinuousStructure、HarmonyFirst。名称与 Phase 4B 方案 A/B/C 不是一一等价；实现边界见 Phase 4B 综合报告。 |
| Phase 4B-D 隔离开关 | 完成 | CMake 插件选项 `HC_EXPERIMENT_HARMONY_FIRST_PLUGIN` 默认 OFF；另建独立 ON 验收构建。未增加宿主运行时开关。Phase 3A 仍 OFF。默认 OFF 与旧1440矩阵一致。 |
| Phase 4B-E 交互矩阵 | 完成 | 33个固定样本 × Legacy/HarmonyFirst × Phase3A OFF/ON，共132种状态；记录候选、分数、可见结果及压力测量。开启两个实验机制时 R01 A08 得到额外的+8分，证实将来不能不经验证叠加。 |
| Phase 4B-F 自动质量门 | 通过 | 默认 OFF 全量 CTest 32/32；ON 相关回归9/9；ON VST3 验证器47/47。原先报告的七项失败均已在最终 OFF 全量回归中通过。 |
| Phase 4B-G 性能 | 完成（引擎/测试进程范围） | 重复推荐、Phase3A切换、导入、CPU与工作集样本记录在 `phase4b-stress.tsv`。不是 VST 实时线程或 Cubase 延迟数据。 |
| Phase 4B-H 宿主验收包 | 包已交付；宿主验收待做 | 便携 ZIP 包含 OFF/ON 两种构建、运行库、报告、离线原始数据、MIDI、CTest 日志和逐文件校验清单。无 Inno Setup 编译器，未生成安装器；未安装到系统。真实 Cubase 验收仍需用户执行。 |

### R01 与 R02 最终证据

**R01（4QN，Am–G#dim–G–D/F#）**：逐和弦轨迹确认是输入 Am 对齐 Factory A08 索引2 Am；G#dim 是一个查询插入；G 对齐索引3 G；D/F# 对齐索引4 D。续写从 A08 索引5开始，A08 为循环路径，故原路径后续按顺序为 Am、Am、D；不是重组片段或人为拼接。输出 MIDI 的音高、低音与续写时值核对通过。

- Legacy：匹配0.674117，FULL/SKELETON均0.689446，Rhythm=1；排名57.403870，原因是结构角色使插入解释/匹配相关分下降，低于60，不显示。
- BarPositionIndependent 与 HarmonyFirst：匹配0.769499，FULL/SKELETON均0.787274，Rhythm仍为1；排名61.722977，组内第一且显示 V4_A08。
- ContinuousStructure：匹配0.710931，FULL/SKELETON均0.727205，Rhythm=1；排名59.070923，仍低于60。
- 4QN Legacy 的657条进入 shortlist、184条匹配结果中只有5条通过续写检查，0条过总分门槛。HarmonyFirst 有8条通过路径检查。统计匹配窗口不能替代有效续写数。
- 4QN 下 G#dim 权重0.59，4/4小节起点因素提高了该结构权重；2QN 权重0.54。2QN Legacy 的 A08 为61.722977并显示。两者 Rhythm 分相同，故这个差异不是直接节奏加分。分类、对齐与骨架相关子分共同变化；是否构成不合理的重复评价，仍需更多独立样本和人工听觉判断。
- 90 BPM/2QN 与180 BPM/4QN总秒数相同，但QN格点/小节位置不同，Legacy结果可不同。固定QN改变BPM未改变候选打分；速度主要进入时长换算和MIDI tempo元数据。

**R02（合成 F–G–Am Color）**：四种检索模式、C大调和A小调语境均无可见Color候选。代表性 `V3_SHADE_004` 有后续和弦；Legacy匹配0.581427低于0.60匹配门槛，BarPositionIndependent/HarmonyFirst为0.588810也未过门槛；ContinuousStructure匹配0.607967后总分仅52.982，仍低于60。此例不是路径尾部耗尽，也不是意图过滤；具体消失点是匹配门槛，或ContinuousStructure下的总分门槛。用户真实Cubase差异尚未定位。

### 七项旧失败的最终修复核销

此前最终回归未通过的七项现已在同一最终 OFF 全量 CTest 中通过，并在 ON 相关构建中再次通过：

| 旧失败 | 最终处理/证据 |
|---|---|
| `LibraryV3_user`、`LibraryV3_factory` | MSVC 项目目标补上 `/EHsc`，使 C++ 异常路径执行 RAII 清理；最终测试完成后临时库可清理。 |
| `LibraryStoreTests` | 加强冲突内容错误断言及后续有效安装重新取得锁的验证；释放/回收异常路径后通过。 |
| `ContinuationEngineTests` | 收窄临时用户库和工作线程生命周期，在删除临时数据库前释放资源；完成清理。 |
| 三项 HostContract | case 27 fixture 改为读取配置的 V4 Factory 版本；case 38 预期改为配置的657条，而非旧的161条。FL Studio、Generic、Cubase三项均通过。 |

旧 `build-v4-dev2` 并不是完整可执行的比较基线：CTest登记31项，但部分旧二进制缺失，因此不报告“旧基线全绿”。此次结论以当前完整 OFF 32/32 和 ON 9/9 的运行记录为准。

### A–H 性能、质量和风险摘要

- **可重复设置**：Release；Factory4；默认OFF；矩阵固定样本；13点结构扫描；推荐压力各60次/多组合；输入用固定测试夹具，没有随机采样。
- **压力样本**：R01四态开关对照80次，P50 3.3619 ms、P95 3.9452 ms、最大4.631 ms；其中184条匹配记录、最终1条可见。B08 Legacy OFF 60次，P50 12.6852 ms、P95 14.1049 ms、最大14.7702 ms；B11 Legacy OFF 60次，P50 11.516 ms、P95 13.5551 ms、最大14.9395 ms。60次导入的P95为0.4454 ms。测试工作集约15.6 MB，观察峰值约16.1 MB。以上为进程内测量，不是Cubase/VST实时指标，也不构成性能保证。
- **候选稳定与多样性**：时间缩放下平均候选Jaccard：Legacy 0.72（110个非空可比对）、BarPositionIndependent 0.85（120）、ContinuousStructure 0.00（15；另有105对两边均空）、HarmonyFirst 1.00（120）。HarmonyFirst这组小样本稳定性较高，但候选曝光集中：合成样本Top10份额52.9%、41个不同ID；Legacy为48.5%、47个ID。它不是真实用户曝光数据，需进一步评审误召回和多样性。
- **Color与R02**：B11离线样本部分模式可得到Color结果，但F–G–Am的R02 Color差异仍未复现。没有扩大Factory或放宽和弦性质边界。
- **宿主限制**：当前插件上下文可传分子/分母，未传复合/不规则拍的分组。6/8无法仅凭该信息区分3拍或2组，7/8的2+2+3与3+2+2也无法区分。OFF/ON均尚未在Cubase实机验证；没有人工试听结论。

### 产物、Git 与安全边界

- Cubase验收包：`build-phase4b/HarmonyContinuation-dev.3-Cubase-Acceptance-20261012.zip`。ZIP含82个文件；其中81个逐文件哈希与包内 `SHA256SUMS.txt` 核对一致。OFF/ON二进制和Factory库哈希在包内复核一致。
- 包内OFF插件SHA-256：`3F6A0D23D7866EEC0CBD62EE3EBD3FDBA39CB237DE89A44ADEF73CEB7F57EAC0`；ON插件SHA-256：`4AFA78EA7FE7907508E573A65E897F3EF60422F244F7E49150E03B80F95C7262`。两个包都含同一Factory V4哈希。
- 安装器未生成：本机未找到Inno Setup编译器；因此提供可人工替换、可从备份恢复的便携包。没有执行安装，没有运行Cubase，没有更改ProgramData/LocalAppData用户数据。
- Git仍为用户既有未提交工作区：分支和HEAD未变，没有提交、重置或丢弃改动。修改文件：`CMakeLists.txt`、`NOW.md`、`src/core/ContinuationEngine.cpp/.h`、`src/core/HarmonyAnalysis.cpp/.h`、`src/core/ProgressionMatcher.cpp/.h`、`src/plugin/Controller.cpp`、`tests/ContinuationEngineTests.cpp`、`tests/HostContractHarness.cpp`、`tests/LibraryStoreTests.cpp`、`tests/RecommendationReliabilityTests.cpp`。未跟踪报告/数据：Phase 1/2/3/3A/4A/4B已有阶段文档及 Phase 3 审计数据目录；本收尾报告为 `docs/PHASE4B_CLOSEOUT_RECONCILIATION_20261012.md`。没有新增独立诊断脚本或工具。
- 构建与离线数据位于忽略的 `build-phase4b/` 和 `build-phase4b-harmonyfirst/`；未纳入Git。Factory数据库、Schema、User Library、Session、Snapshot、安装器、正式Tag、`main` 均未修改；没有新版本发布或安装。
- 生产默认模式仍为Legacy；HarmonyFirst只在独立ON验收构建中启用，Phase3A补偿在两构建仍默认关闭。ON包是供后续人工验收的实验版，不是正式发行版。

### 最终状态

- HISTORICAL WORK PACKAGES RECONCILED：YES（保留限制和历史边界）
- PHASE 4B A–H OFFLINE DELIVERABLES：YES（H包已交付，Cubase人工验收待执行）
- SEVEN FAILURES RESOLVED：YES
- FULL OFF REGRESSION：32 / 32
- HARMONYFIRST DEFAULT ENABLED：NO
- PHASE 3A DEFAULT ENABLED：NO
- FACTORY DATA CHANGED：NO
- PRODUCTION DEFAULT / FORMAL RELEASE CHANGED：NO
- READY FOR CUBASE ACCEPTANCE：YES（仅表示验收包和清单就绪，不表示真机通过）
- READY FOR FORMAL RELEASE：NO
