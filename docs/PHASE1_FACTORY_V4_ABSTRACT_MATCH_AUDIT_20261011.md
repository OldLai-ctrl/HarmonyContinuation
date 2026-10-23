# HarmonyContinuation 0.10.0-dev.3 — Factory V4 抽象匹配审计

日期：2026-10-11
阶段：Phase 0 基线确认 + Phase 1 只读审计
范围：只读检查代码、Factory 数据库、现有测试与测试产物；未修改插件行为，未新增测试或诊断工具。按要求，临时审计文件和临时构建目录已清理。本报告是本次唯一新增项目文件。

补充：本文件末尾追加了 Phase 1B 的受控复现证据；下文前半部分记录的是 Phase 0–1 当时的结论。

## 结论摘要

- 当前工作分支和远端一致，工作区在审计前干净；Factory V4 数据库包含 657 条，内容校验正常。当前包的源码构建来自 `99017f5`，而当前 HEAD `42a915b` 是验证记录提交。
- 本机重新运行的 12 项相关测试全部通过；测试矩阵仍是合成案例，不包含 R01 的真实输入，也不包含 R02 的 `F → G → Am` 与 `4+4 / 3+5+OPEN` 配对。因此，两个新问题在本次环境都**没有复现**。
- Factory 的静态路径检查显示：R01 的完整根音与和弦性质组合没有精确连续路径；用户提出的 `F → G → Am → C` 也没有精确四和弦路径。这个结果只说明精确结构未命中，不能证明宽容匹配没有近似候选。
- R02 的 `F → G → Am` 三和弦根音与核心性质组合，在曲库中有 7 个连续窗口。Phase 1 当时把 Color 条目 `COMMON_MINOR_015` 推断为可循环；Phase 1B 用同一 Factory 文件重新核对后纠正了这个判断：该条目并不可循环，实际原因见下文补充。
- 代码中节奏不决定对齐路径或相似度，但会进入候选总分；总分随后有 60 分门槛。因此，节奏可能间接影响候选是否留下。R02 同时改变了节奏和最后和弦的 OPEN 状态，现有证据无法分离这两个因素。
- 根因分类、候选阶段数量和 UI 流失位置对 R01、R02 均为 **UNKNOWN**。不建议据此进入 Phase 2 实施；先需要两个案例的真实本地诊断导出。

## 1. 基线确认

| 项目 | 本次核验 |
|---|---|
| 仓库 / 分支 | `HarmonyContinuation-v4` / `library/v4` |
| 当前 HEAD | `42a915bda447457bd1bdc8b2f79a90e5ddc69c00` |
| 远端分支 | `origin/library/v4`，远端 HEAD 与本地相同 |
| 工作区 | 审计前干净；审计后除本报告外无源码改动 |
| 产品构建 | `0.10.0-dev.3`，Release；源码构建提交 `99017f596cef895f2024677120b7005fa7c4a98e` |
| Factory | 元数据 `library_version=4`、`schema_version=2`；657 条；`PRAGMA quick_check=ok` |
| Factory SHA-256 | 开发库与 dev.3 包内库相同：`74c529828f8c4b934bbaf4ada719aa95491ab498b58885522124ff6505671534` |
| 其他 Schema | User Library 2；Session 5；Recommendation Snapshot 3；Enrichment Snapshot 2 |

这里的“Factory V4”是曲库版本 4，不是 Schema 4。对应常量见 `src/library/ProgressionLibrary.h:9-10`、`src/session/PluginSessionState.h:15`、`src/snapshot/RecommendationSnapshot.h:9`、`src/snapshot/EnrichmentSnapshot.h:8`。

### 当前测试

在本机重新运行了以下 12 项相关 CTest：`CubaseReliabilityFocused`、`RecommendationReliabilityFocused`、`FactoryV4Focused`、`EnrichmentTests`、`UILayoutTests`、`MidiExportTests`、`MidiImportTests`、`PreviewTests`、`HarmonyAnalysisTests`、`ProgressionMatcherTests`、`ContinuationEngineTests`、`VstXmlChordParserTests`。结果 **12/12 通过**，总耗时约 54 秒。

现有矩阵产物分别有 1440 行旧推荐对照和 800 行 Cubase 合成对照（含表头的文件分别为 1441、801 行）。没有运行完整 CTest、VST 验证器或真实 DAW 人工验收。包内关于隔离安装 413 项通过的记录是既有报告结果，本次没有重跑。

## 2. 推荐链路定位与已知门槛

1. **宿主导入**：Cubase 拖放/剪贴板由 `src/plugin/HostAdapters.cpp:60-85, 90-128` 送入 `VstXmlChordParser`。解析器保留和弦名、根音、低音、音集合、掩码和原始时间字段（`src/plugin/VstXmlChordParser.cpp:214-260`），只接受 quarterNotes 时间域。`sortAndInferDurations` 按相邻起点推时长，并把最后一个事件标为 OPEN（`src/core/Progression.h:10-25`）。标准 MIDI 导入是另一条路径：`src/midi/MidiImportWorkflow.cpp` 与 `src/midi/MidiHarmonyExtractor.cpp`。
2. **和弦与调性分析**：`src/core/ChordNormalizer.cpp` 读取和弦名、音集合及显式低音；`src/core/HarmonyAnalysis.cpp:244-333` 生成和弦事件、候选调性、功能和结构节点。没有真实 R01/R02 输入，不能确认其解析出的根音、性质、转位或调性。
3. **Factory 检索与匹配**：`RecommendationWorker` 读取 Factory 和用户库，建立索引（`src/plugin/RecommendationWorker.cpp:80-105`）。默认短名单上限为 800；Factory 657 条时全部进入匹配（`src/core/ProgressionMatcher.cpp:402-423`）。匹配会检查不同模板位置，并通过插入、删除和替代代价对齐；不是只比 Factory 第一和弦（`src/core/ProgressionMatcher.cpp:180-224, 426-479`）。不同大/小调模式的解释不会互相直接对齐（`440` 行）。
4. **过滤与排序**：默认相似度下限为 0.60，候选总分下限为 60；之后还会检查输入尾部是否对齐、V4 显式低音/定义音是否冲突、是否有可输出续写、旋律硬约束、去重和每组保留数（`src/core/ContinuationEngine.h:47-56`、`src/core/ContinuationEngine.cpp:165-212, 243-331`）。Intent 进入候选分组；用户偏好的 Intent 参与评分，没有单独的 Color 匹配门槛（`ContinuationEngine.cpp:44-56, 218-256`）。
5. **节奏与 OPEN**：对齐注释明确写明先按和声选路径、时间用于后续评分（`ProgressionMatcher.cpp:185-186`）；已知时值用于估计统一缩放与节奏相似度（`282-304`）。但节奏子分会进入候选总分，并可能间接跨过 60 分硬门槛（`ContinuationEngine.cpp:251-256, 303-307`）。无时值的 OPEN 和弦不进入已知时值比率；它还可能使用匹配模板时长给出建议时长（`ContinuationEngine.cpp:235-241`）。
6. **显示与缓存**：结果按 Resolve、Develop、Loop、Color 分组，UI 按当前意图调整分组顺序并应用显示去重/可见规则（`ProductServices.cpp:119-151`、`MainView.cpp:1076-1133`）。因此“已匹配”不等同于“默认可见”。Worker 只在 `rankingOnly`、短语版本号相同等条件下复用分析（`RecommendationWorker.cpp:63-73`）；当前没有 R01/R02 的 Session 或缓存状态可检查。
7. **输出**：候选和 Factory 模板绑定；现有诊断会记录输入摘要、各阶段数量、过滤 disposition、匹配 ID、候选组、排名和续写路径（`src/session/ProductServices.cpp:12-50`）。现有测试样例中的这份诊断是合成 B08 输入，不是 R01/R02 的追踪。

## 3. Factory V4 静态覆盖

以下统计来自当前只读打开的 `build-v4-dev2/factory.db`。Style 是可重叠标签；它们的计数不能相加为 657。复杂度字段只有 basic / rich / advanced，没有独立的“experimental”类别。

| 维度 | 数量 |
|---|---|
| Intent | loop 127；resolve 350；develop 91；color 89 |
| Mode | major 473；minor 184 |
| Complexity | basic 323；rich 242；advanced 92 |
| Style（重叠） | pop 419；functional 286；jazz 256；rock 212；citypop 180；rnb 157 |
| 路径长度 | 3 和弦 7；4 和弦 161；5 和弦 217；6 和弦 194；7 和弦 69；8 和弦 9 |
| 全路径事件 | 3469；Major 1477、Minor 875、Dominant7 310、Major7 354、Minor7 365、Diminished 22、Diminished7 39、HalfDiminished7 27 |
| 明确低音 | 15 / 3469 个事件；非零颜色掩码或显示后缀 12 / 3469 个事件 |

各条目的 `full` 路径可形成不同起点和长度的连续窗口；理论窗口数为：长度 2 有 2812 个、长度 3 有 2155 个、长度 4 有 1498 个、长度 5 有 848 个、长度 6 有 359 个、长度 7 有 87 个、长度 8 有 9 个。它们只是曲库切片数，不是匹配成功数。模板按相对调式级数储存并按查询调性解释；具体输入的有效候选仍须由运行追踪确认。

### 与两个案例有关的静态路径检查

检查方式：读取全部 657 条 `typedData.full`，把调式级数转换为相对半音根音序列，逐条检查连续窗口，并同时核对核心和弦性质；该检查不运行推荐引擎，也不放宽和弦性质。

- **R01**：输入核心性质为 Am / G#dim / G / D，归一化根音间距为 `[0, 11, 10, 5]`，性质为 Minor / Diminished / Major / Major。精确连续窗口 **0**。两段下行半音对（Am→G#dim；G#dim→G）按相同核心性质检查也各为 **0**。用户提出的 F→G→Am→C 精确四和弦窗口 **0**。这不能排除 matcher 的宽容替代、插入/删除或另一调性解释；它们仍需真实 query 才能评估。
- **R02**：F→G→Am（根音间距 `[0,2,4]`，Major / Major / Minor）找到 **7** 个曲库连续窗口。例如 `FUNCTIONAL_011` 后接 ii–V–I，`POP_011` 后接 IV–V–I，`V3_DUSK_051` 后接 iv–V–i。它们证明该三和弦核心并非曲库完全没有覆盖。
- Phase 1 对 `COMMON_MINOR_015` 的循环属性是误判。Phase 1B 在同一 SHA-256 的 Factory 文件中确认其 `loopable=false`；A 小调查询对齐到该条目末尾后，运行时以 `no_continuation` 淘汰。其余 Color 条目还需结合 Phase 1B 的匹配结果解释，不能仅由相似根音走势推断可用续写。

## 4. CASE-R01 诊断状态

| 检查项 | 结果 |
|---|---|
| 原始 Cubase/MIDI 输入与事件时值 | **UNKNOWN**：仓库没有这次输入的原始载荷、音符事件或诊断导出 |
| Am、G#dim、G、D/F# 识别及斜杠低音保留 | **UNKNOWN**：只能确认代码有相应解析字段；没有该输入的实际解析结果 |
| 调性解释、匹配模板、候选阶段数 | **UNKNOWN**：不能从静态路径统计推算实际候选数 |
| 数据库精确完整路径 | **未发现**：上述精确窗口扫描为 0 |
| 是否属于 A 输入识别、B 数据缺口、C 匹配、D 召回、E 过滤、F UI | **UNKNOWN**：没有实际链路记录，不能给根因分类 |
| 用户建议续写 F→G→Am→C | 曲库没有精确四和弦窗口；不能因此把它说成用户输入的必然期望或引入新 Factory 条目 |

现有测试明确把 CubaseReliability 的 XML 标为 adversarial synthetic（`tests/CubaseReliabilityTests.cpp:13`）。其 8 种输入包括 C–Cmaj7/B、C–Cmaj7/B–C7/Bb、C–F–F#dim7–G 等，没有 Am–G#dim–G–D/F#。此前 dev.3 关于缺五度的 B08/B11 结果不能替代本案例证据。

## 5. CASE-R02 诊断状态

| 检查项 | 结果 |
|---|---|
| Case A/B 输入事件和原始 MIDI | **UNKNOWN**：无 MIDI、Cubase 导出或每和弦原始起止点 |
| BPM、拍号、FULL/SKELETON、Intent/筛选、插件/宿主版本 | **UNKNOWN**：本案例的记录不存在 |
| 唯一差异是否为节奏 | **不能成立**：B 同时把 F/G 时值改为 3/5 QN，并把末尾 Am 设为 OPEN；A 被描述为每个和弦一小节 |
| 对齐路径是否因节奏变了 | 按代码，节奏不参与 DP 路径选择或相似度计算；但可参与最终分数并导致 60 分门槛变化。实际阶段计数仍 **UNKNOWN** |
| OPEN 影响 | 最后和弦缺少时长时不计入节奏比例估计；建议时值可由模板产生。没有两组输入的诊断，不能判断它是否改变模板对齐或排名 |
| 缓存 / UI 隐藏 | **UNKNOWN**：没有短语版本号、Worker 输出或 `ACTUAL_UI` 记录 |
| 是否有能给出 Color 续写的 Factory 路径 | 有 Color 条目与相同根音走势，但精确三和弦 Color 条目在 Am 处没有新和弦可接；近似 Color 条目需要实际分数与过滤记录 |

现有 `RecommendationReliabilityTests.cpp:29-38` 使用 15 个别的和弦序列，包括 C–Am–F–G、C–F–G–C 和 Cmaj9–Fmaj9–G13–Cmaj9，并改变一组固定时值；没有 F–G–Am 的 3+5 QN / OPEN 与闭合配对。`CubaseReliabilityTests.cpp:21-37` 的时值变化也作用于其合成 B08/B11 等输入，不能视为 R02 复现。

## 6. 需要保留的边界与 Phase 2 优先问题

本次能确认的是静态结构、现有过滤位置和合成回归范围；不能确认两个用户案例各自在哪个阶段失去候选。数据库精确窗口计数不是宽容匹配召回率，已有测试通过也不是 Cubase 手工验收通过。

进入 Phase 2 设计前，建议先补齐：

1. 在 Cubase 对 R01、R02 各导出一次现有的“Export local input and recommendation diagnostics”结果，并保留和弦符号截图。记录原始 MIDI/音符事件、起止位置、拍号、BPM、最后和弦 OPEN、FULL/SKELETON、调性、Intent、风格、插件 Build ID 与宿主版本。
2. R02 用同一组 F–G–Am 和相同设置做四组对照：4+4/闭合、4+4/OPEN、3+5/闭合、3+5/OPEN。比较 `shortlist → aligned → similarity → tail → realization → path → score → workerOutput → UI_DEFAULT/UI_MORE`，这样才能分开节奏和 OPEN 的影响。
3. R01 先依据真实输入确认每个和弦的解析根音、性质、显式低音与调性解释，再检查实际匹配模板和各过滤 disposition；之后才判断优先处理输入、候选匹配/过滤、UI，还是将精确路线缺失列为曲库边界。

以上是取证优先顺序，不是 Phase 2 方案或实现。本报告提交后停止在 Phase 1，未进入 Phase 2，也未修改 Factory、Schema、插件或安装器。

## 7. Phase 1B：受控复现与候选消失位置（2026-10-11）

### 7.1 输入边界与复现设置

没有收到本次 Cubase 工程的 MIDI、剪贴板 XML 或诊断导出。以下均是 `RecommendationReliabilityTests` 生成并重新导入的**模拟 MIDI**，不是用户真实 Cubase 输入，也不代表 Cubase 中出现过同样结果。固定条件为：标准 MIDI 文件导入、A 小调、4/4、120 BPM、单轨/通道 0、`preferredIntent=Color`（排序偏好，不是硬筛选）、Balanced tendency、默认推荐权重及阈值（最低匹配 0.60、最低总分 60、每组最多 3 项）。

测试在现有 `tests/RecommendationReliabilityTests.cpp` 中扩展；没有新建独立脚本或工具，没有改推荐实现，也没有调整阈值。原始音符事件、识别结果和完整阶段追踪保存在构建目录的 `build-v4-dev2/reliability/phase1b-inputs.tsv` 与 `phase1b-diagnostics.txt`。

音符使用固定配器：低音为 `36 + 根/转位低音音级`，上方和弦音为 `60 + 根音 + 和弦音程`；低音力度 90，上方和弦音力度 80。R01 四和弦均为 4 QN。R02 T1/T2 为 4+4+4 QN，T3/T4 为 3+5+4 QN。T2/T4 通过现有 MIDI 导入器的 `openEnded=true` 选项产生末和弦无时值、`OPEN=true`；MIDI 文件中的末和弦音符仍在 QN 12 结束，这是音符释放点，不是关闭和弦事件的时值。

### 7.2 R01

音符号均为 MIDI 音高号，区间按 QN 表示：

| 和弦位置 | 低音 MIDI 音符 | 上方和弦音 MIDI 音符 | 起止 QN | 导入器识别 |
|---|---:|---|---:|---|
| Am | 45 | 69, 72, 76 | 0–4 | Am，根音 A、低音 A、小三和弦 |
| G#dim | 44 | 68, 71, 74 | 4–8 | Abdim，根音音级 8、低音音级 8、减三和弦 |
| G | 43 | 67, 71, 74 | 8–12 | G，根音 G、低音 G、大三和弦 |
| D/F# | 42 | 62, 66, 69 | 12–16 | D/Gb，根音 D、低音音级 6、大三和弦 |
| D 对照 | 38 | 62, 66, 69 | 12–16 | D，根音 D、低音 D、大三和弦 |

`G#dim` 被 MIDI 音高识别器拼写为等音 `Abdim`，`D/F#` 被拼写为 `D/Gb`；根音、和弦性质和低音音级均正确。此结果只说明模拟音符的识别，不证明 Cubase 原输入也被这样识别。

两条完整 R01 输入的推荐阶段完全相同：`shortlist=657 → aligned=184 → similarity=5 → tail=5 → realization=5 → path=5 → distinct=5 → scorePassed=0 → output=0`。也就是说，候选并非全都没召回：5 条近似路径通过匹配、尾部、实现和路径检查，最后都因低于 60 分而被 `minimum_score` 淘汰。

| Factory ID | 相似度 | 候选分 | 意图 | 可续写抽象路径 | 淘汰原因 |
|---|---:|---:|---|---|---|
| `V4_A08` | 0.6741 | 57.4039 | Loop | i → i → IV | `minimum_score` |
| `V3_DUSK_010` | 0.6258 | 53.6139 | Resolve | V → i | `minimum_score` |
| `V3_DUSK_043` | 0.6258 | 52.4139 | Resolve | i | `minimum_score` |
| `V3_NIGHT_004` | 0.6258 | 53.6139 | Loop | VI → i | `minimum_score` |
| `V3_NIGHT_043` | 0.6258 | 56.0139 | Loop | i → III → VI → i | `minimum_score` |

因此，R01 的模拟输入有 5 条可以继续的近似曲库路线，但没有任何一条跨过总分门槛；不能把它归因成“曲库里没有合理相似续写”。精确完整四和弦路径仍为 0，这是曲库精确覆盖的边界，不是本次无输出的直接原因。

把末和弦改为普通 D 后，上述 5 个 ID、相似度、分数和各阶段计数均不变；本次输入没有观察到转位带来的额外限制或推荐变化。短前缀 `Am → G#dim` 与连续局部片段 `G#dim → G → D/F#` 各自都有 184 个对齐尝试，但都没有候选达到 0.60（`similarityPassed=0`）。所以代码支持局部对齐，不代表这两个特定短片段能成为可用候选。

### 7.3 R02 四组对照

四组识别结果相同，均为 `F → G → Am`。每个和弦的音符号如下：F 为低音 41、和弦音 65/69/72；G 为低音 43、和弦音 67/71/74；Am 为低音 45、和弦音 69/72/76。T1/T2 的起止分别为 F 0–4、G 4–8、Am 8–12；T3/T4 为 F 0–3、G 3–8、Am 8–12。T1/T3 最后一和弦闭合；T2/T4 最后一和弦 OPEN。

| 组 | 输入时值 | OPEN | Factory 阶段计数：shortlist / aligned / similarity / tail / realization / path / distinct / scorePassed / output | Color 候选 / 默认可见 | `V3_DUSK_051` 节奏子分 / 置信度 / 匹配节奏分 | OPEN 建议时值 |
|---|---|---|---|---:|---|---:|
| T1 | 4+4+4 | 否 | 657 / 184 / 32 / 32 / 32 / 15 / 13 / 8 / 5 | 0 / 0 | 0.875 / 0.75 / 1.000 | 无 |
| T2 | 4+4+4 | 是 | 657 / 184 / 32 / 32 / 32 / 15 / 13 / 7 / 5 | 0 / 0 | 0.750 / 0.50 / 1.000 | 4 QN |
| T3 | 3+5+4 | 否 | 657 / 184 / 32 / 32 / 32 / 15 / 14 / 7 / 5 | 0 / 0 | 0.719836 / 0.5625 / 0.877172 | 无 |
| T4 | 3+5+4 | 是 | 657 / 184 / 32 / 32 / 32 / 15 / 13 / 7 / 5 | 0 / 0 | 0.407879 / 0 / 0.815759 | 4 QN |

代码里没有独立的 OPEN 评分字段；候选分数会用节奏子分，末和弦 OPEN 时则可从模板推导建议时值。表中的 OPEN 建议时值是在 T2/T4 对齐到模板后得到的 4 QN。OPEN 与时值形状改变了候选分数和 `scorePassed` 数量，但四组最后都输出 5 个相同 ID，没有出现 Color。

五个默认展示候选按组内次序为：Resolve：`V3_DUSK_051`（Dm → E → Am）、`V3_DUSK_049`（Dm → Am）；Loop：`V3_NIGHT_047`（Dm → C → Am）、`V3_NIGHT_048`（F → Dm → Am）、`V3_NIGHT_035`（Dm）。其分数如下；五项均通过默认展示索引，没有在显示阶段丢失：

| Factory ID | 续写 | T1 | T2 | T3 | T4 |
|---|---|---:|---:|---:|---:|
| `V3_DUSK_051` | Dm → E → Am | 80.4041 | 79.3461 | 77.9138 | 76.5878 |
| `V3_DUSK_049` | Dm → Am | 69.2942 | 68.2362 | 68.0539 | 65.4779 |
| `V3_NIGHT_047` | Dm → C → Am | 78.1338 | 77.0758 | 76.8935 | 74.3175 |
| `V3_NIGHT_048` | F → Dm → Am | 70.0249 | 68.9670 | 68.7846 | 66.2087 |
| `V3_NIGHT_035` | Dm | 67.6249 | 66.5670 | 66.3846 | 63.8087 |

没有任何一组产生可供跨组追踪的成功 Color 候选，所以本模拟未复现“3QN/5QN/OPEN 时 Color 出现、其他条件消失”的现象。已有 Color 模板追踪显示：A 小调 `COMMON_MINOR_015` 精确命中且相似度约 0.96，但它非循环、模板没有后续和弦，四组均以 `no_continuation` 淘汰；`COMMON_MINOR_016`、`017`、`018` 的匹配分数低于 0.60，均以 `minimum_match` 淘汰；其余 Color 条目属于大调模式，在固定 A 小调查询中没有返回匹配。`V3_SHADE_027` 也属于大调模式，当前 A 小调查询没有返回它。由此可将本模拟的 Color 缺失定位在**可续写生成之前**（模式排除、最低匹配门槛或无后续路径），不是后续去重、总分、每组截断或显示隐藏。

### 7.4 Phase 1 的七个精确窗口逐项复核

下表按七个静态精确窗口逐项核对。在固定 A 小调的 R02 T1 运行中，大调模板受模式约束没有返回；三条小调模板则获得实际结果。其他三组的窗口结论相同：`COMMON_MINOR_015` 仍无后续和弦；`V3_DUSK_051` 与 `V3_NIGHT_047` 仍进入输出。

| ID | 精确窗口 | 模式 | 窗口后的曲库片段 | 意图 | T1 实际结果 |
|---|---:|---|---|---|---|
| `COMMON_MAJOR_010` | 1–3 | Major | 无 | Develop | A 小调模式下未返回；无排序结果 |
| `COMMON_MINOR_015` | 1–3 | Minor | 无；且 `loopable=false` | Color | 精确命中，`no_continuation`；无候选/排序 |
| `FUNCTIONAL_011` | 1–3 | Major | ii → V → I | Develop | A 小调模式下未返回；无排序结果 |
| `POP_011` | 1–3 | Major | IV → V → I | Resolve | A 小调模式下未返回；无排序结果 |
| `V3_DUSK_051` | 0–2 | Minor | Dm → E → Am | Resolve | 续写候选，Resolve 组第 1，默认可见 |
| `V3_NIGHT_047` | 1–3 | Minor | Dm → C → Am（循环段） | Loop | 续写候选，Loop 组第 1，默认可见 |
| `V3_OPEN_031` | 1–3 | Major | iii | Develop | A 小调模式下未返回；无排序结果 |

### 7.5 根因分类、回归与交付状态

- **R01 模拟输入**：识别通过；完整精确路径缺失，但近似续写已召回并走到 `path`；五个候选都在 `minimum_score=60` 前被淘汰。转位没有造成新增损失。短前缀/局部片段的候选在 `minimum_match=0.60` 处消失。分类为**评分门槛（完整句）**及**匹配门槛（短片段）**，不是显示问题，也不是“没有任何合理相似续写”。
- **R02 模拟输入**：识别通过；四组都召回大量匹配并输出相同五个非 Color 候选。Color 的精确同调式窗口在无后续和弦处被丢弃，其余同调式 Color 模板未达到匹配下限，大调 Color 模板受当前 A 小调模式限制。分类为**匹配模式/最低匹配门槛/曲库续写末端**；没有证据指向显示隐藏。四组中没有 Color 成功案例，因此实际 Cubase 条件下的差异根因仍未知。
- **曲库边界**：R01 精确四和弦窗口仍为 0。R02 有七个精确三和弦窗口，但在固定 A 小调下只有三条实际匹配，其中 Color 那一条没有尾随续写；这说明存在精确 Color 覆盖但缺少从该命中位置出发的后续和弦，并不等于整个曲库没有任何 R02 续写。
- **对旧结论的更正**：Factory SHA-256 为 `74c529828f8c4b934bbaf4ada719aa95491ab498b58885522124ff6505671534`，与 Phase 0–1 报告记录一致。对该文件重新加载后，`COMMON_MINOR_015.loopable=false`，实际 disposition 是 `no_continuation`。旧报告把它说成 loopable 并归因于 `repeated_current_chord` 是错误的，已在本文中更正。
- **测试**：扩展后的 `RecommendationReliabilityFocused` 通过；此前 12 项相关 CTest 随后复跑为 **12/12 通过**（包括本测试、Cubase 可靠性、Factory、MIDI 导入、匹配和续写引擎等）。没有运行完整全量 CTest、真实 Cubase 手工验收或 VST 宿主测试。
- **Git/文件**：只有既有 `tests/RecommendationReliabilityTests.cpp` 增加了测试夹具与诊断导出；本报告原本就是 Phase 0–1 的未跟踪交付，本次在原文件追加补充，没有另建诊断程序。`phase1b-inputs.tsv`、`phase1b-diagnostics.txt` 和八个模拟 MIDI 在被忽略的构建输出目录；没有 Factory、Schema、User Library、Snapshot、安装器、插件推荐行为或发布状态改动。

### 7.6 真实 Cubase 输入仍需的最少资料

本模拟没有复现用户的 Cubase 观察，不能用它认定真实输入的根因。最少需要：

1. R01、R02 各自从出问题的插件实例导出的 **local input and recommendation diagnostics**，以及插件 Build ID 和 Cubase 版本。
2. 对应片段的原始 MIDI 音符号、音符起止位置（QN 或 tick）、轨道/通道；若来源是和弦轨道，则附本地输入 VST-XML 和和弦符号截图，以区分音符识别与宿主和弦符号导入。
3. 实际调性（AUTO 或强制）、FULL/SKELETON、输入模式、Intent/风格/筛选和每组候选设置；R02 还需四组各自的 OPEN 状态及最后事件时值。
4. R02 每组实际的默认结果区和 More 结果区截图或 `ACTUAL_UI` 诊断片段，以区分推荐引擎输出与界面展示差异。

Phase 1B 到此结束。未进入 Phase 2，等待决策端批准下一步。
