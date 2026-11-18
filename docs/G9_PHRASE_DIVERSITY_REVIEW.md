# G9 乐句连接与多轮多样性

## 对象与结论边界

从干净的 G8 交付 HEAD `3fedd827b284dc265fc22d33d2b08198a419ac00` 建立独立分支 `development/g9-phrase-diversity-20261102`。产品为 **0.11.0-dev.10 / Factory8 / 1288**；源码及插件 BuildCommit 为 `f105a0b4b4dd50281cc5593bdde05dfc166d3347`。交付文档 HEAD 与产物哈希见同级证据目录的 [identity.json](../../HarmonyContinuation-g9-evidence/identity.json)。

G8 的 46/50 是原版工程连续性证据。用户对三条链的实际听评已完成：有参考价值，乐句组织与后文启发不足，创作质量门禁未通过。G9 的自动结构检查、连续性复核和人工音乐听评分别记录。**本轮不制作 Setup，未进行真实 DAW 验收。**

## 已确认根因

三条旧链的实际 MIDI 均为顺序追加，新增来源完整保留，没有发生演奏事件的时间重叠。问题包括来源内部组织、OPEN 时值推断、局部检索的结构解释及历史盲区，不能只由“每条来源 32 QN”排除。

| 原听评链 | G8 三个接缝 QN | 已确认问题 | G9 处理 |
| --- | --- | --- | --- |
| V3_CYCLE_027 → 002 → 008 → 014 | 16 / 48 / 80 | 时间连续且每段32 QN；三条不同家族仍反复使用接近的平台/相对小调语汇 | 保留完整合法路线；将相近家族归入同一机制簇，在有合格替代时优先不同方向 |
| V6_B3_02_054 → 014 → 003 → 002 | 36 / 68 / 100 | 原句时值为 `8 4 4 4 4 4 4 8`，完整40 QN；OPEN中位数4取代原句尾8，首接缝缩至36 | 依据完整实际音集、低音及已知时值匹配，为未知OPEN提供8 QN建议；结构审查识别两小节准备＋4＋4，首接缝40；不改后续冻结输入中已经明确的旧时值 |
| FUNCTIONAL_015 → 004 → 002 → 008 | 28 / 60 / 92 | 起点本身为 `I–ii–V–I | IV–V–I`，4＋3小节，末OPEN4没有缩短原来源 | 完整保留28 QN；首轮无独立的不对称跨句结构依据，降低优先级，不补拍、不伸缩、不宣称修复为四小节 |

QN 以导入选区起点为相对锚点，拍号4/4；G8每段后续和弦均4 QN。九个旧接缝前的OPEN实播均4 QN。上述“接缝”是前句实际结束与后句首有效事件同一时刻，不表示共享和弦或重叠已经获得编辑审核。旧记录后续轮保留当时真实输入，不能把第一轮的新建议倒写进原始证据。[逐轮实际查询及结构状态](../../HarmonyContinuation-g9-evidence/final-checks/g9-legacy-structure.tsv)。

## 结构资格与历史比较

灵感结果在原音乐资格之外增加结构判断。完整来源关系、拍号、实际起点、源时值、OPEN建议及来源结束边界分别核查。新整句从前句结束后追加；完整循环只追加一遍。完整来源前缀从选区起点连续匹配时可沿原来源继续；仅尾部两三和弦的局部匹配不能证明新乐句已经开始。

反例 `Am–F–C–G` 只与来源 `C–G–Em–F–Am` 共享 `C–G`：普通检索可保留完整后缀 `Em–F–Am`，结构依据不足，排在有依据的路线之后。没有删除相同和弦、裁剪尾部或把共享材料输出两次。当前没有独立审校的重叠、弱起或延迟接缝记录，因此不会凭和弦相似自动授予这些结构资格。

四/八小节组织是有来源与时值支持的优先证据，并非强制句长。V6反例的10小节完整来源以实际音集、低音和时值绑定一条小范围结构审查，采用两小节主和声准备＋两个四小节组。没有这类依据的不对称结构保留为探索路线。来源结构审查不等于独立接缝审核，更不等于用户听感通过。

历史比较使用实际发声音集、低音及节奏识别材料复现，辅以移调归一的和声骨架。MIDI的等音集符号重解释不被冒称为原来源ID恢复。002、008、014合并为相近机制簇；调式持续低音、半音低音线、上行低音、Lydian、隐主音、悬挂序列和共同音中心转向另行区分。选择先寻找合格且尚未使用的机制，再执行原方向/来源软平衡与排序；原始分数不因新奇度被提升，所有合法结果仍可分页访问。

还修复了挂留入口的曝光缺口：原索引主要提供属级挂留解释，导致实际音符连接合理的非属级语境被漏检。现在索引可提供各调内级数的Sus2/Sus4解释，逐条仍须通过原实际低音、公共音、声部运动、调中心、旋律硬约束及新结构资格。针对真实的002句尾，新增的回归断言要求正常推荐出现新机制，且首个合格完整未终止路线优先采用新机制。

**新增完整来源为0。** Factory V8及HCA字节保持；先使用已存在但此前曝光不足的来源，不补相似32 QN模板。

## 必要验证

Release插件及定向工具构建成功；生成版本、moduleinfo与二进制均为dev.10/上述BuildCommit。自动后置SDK Validator关闭，本轮未重跑旧47项。Factory DB SHA-256仍为 `C32257F878EEAD44E0DF6FEBE14340AC08EAB4A66D7C4DB34BF6AF911ED6CE9E`，HCA仍为原V7字节。G7/G8工作树干净且交付HEAD不变。

[最终定向检查](../../HarmonyContinuation-g9-evidence/final-checks/g9-checks.tsv)全部通过：共享1–5反例、完整源正例、挂留后的新机制曝光、三条旧链的逐轮来源及时间身份、84事件正常推荐、两条不同机制的Snapshot3/Preview/MIDI/WAV。另由本次RCR保存的三条三轮材料执行9组Snapshot重开、Preview和生产MIDI字节一致性核查。没有把显示标签重解析当作完整演奏真值。

84事件一次查询为 **393.697 ms**，含CandidateIndex建立、两视图推荐与排序，不含初始makeMatchQuery；单批RCR中20事件的正常流程（含makeMatchQuery、复用已建索引）为 **142.713 ms**。两者测量范围不同，不据此作规模性能曲线或UI/宿主响应承诺。[长输入结果](../../HarmonyContinuation-g9-evidence/final-checks/g9-long-input.tsv)、[真实首页](../../HarmonyContinuation-g9-evidence/final-checks/g9-top10.tsv)。

自动调性下，`F–C–Dm–Am`的实际首页顺序为002平台、004 Dorian踏板、V3_CYCLE_027精确、010上行低音、V7_01_095、POP_014精确、V7_04_055、V7_01_032、V5_POP_VARIANT_001精确、V7_04_034：**精确3、局部0、算法跨段7**。普通精确循环仍占位置，踏板与低音机制有实际展示机会。表内`reviewed-v8-phrase`另为固定C大调的正例，应与这些Auto行分开解读。

### 原冻结50起点的一次复核

直接读取G8的原有50条顺序及完整音乐指纹，不重新随机。输出起点文件与原始文件SHA-256同为 `111B8D5A922A16561FDCBCAFD05A7F13504F5056004321F118C58061C3A8286C`。OPEN公开导入场景、首个正常可见合格候选、实际DAWClip回导、来源/完整音乐/家族不重复等原条件保持，另要求结构资格。不回溯，不换样本，不使用特殊参数。

| 本批指标 | 实际结果 |
| --- | --- |
| 三轮完成且每轮结构合格 | **12/50，24%** |
| 三轮机制互异 | **12/50，24%** |
| 所选轮次 / 第一页可达 | **36 / 36** |
| 精确 / 局部 / 算法跨段 | **0 / 0 / 36** |
| 使用来源 / 机制 | **7 / 5** |
| 002、008、014集中度 | **11/36，30.6%**；G8历史为100/138，72.5% |

[汇总](../../HarmonyContinuation-g9-evidence/rcr-3/g9-summary.tsv)、[逐起点](../../HarmonyContinuation-g9-evidence/rcr-3/g9-start-results.tsv)、[逐轮实际输入与导出指纹](../../HarmonyContinuation-g9-evidence/rcr-3/g9-rounds.tsv)。12条均完成三轮，38条均在首轮停止；失败起点保留。只读核对原来源拍号与时值后，四条为非4/4，另34条为尚无独立结构审查的不对称起点。该归类是与当前结构门禁一致的覆盖限制，未另作逐候选失败审计。

**本批达到20%自动持续续写最低目标；不宣称音乐最终通过。** 单链机制重复已明显减少，跨起点首选仍集中：010使用11次、003使用11次、014使用7次，合计29/36；多数成功链仍共享平台、上行低音、隐主音三类词汇。首页可见候选分布与自动首选分布不同，以上不是全库创作成功率估计。当前首要缺口是结构审查覆盖和更广泛的有效路线曝光，不据此自动扩库或批量批准未知不对称接缝。

初始红/绿循环区分了产品缺陷与夹具错误：第一份负例的G→Dm在原和声门槛就被拒绝，改用有真实连接资格的G→Em；旧display label无法完整表达类型/扩展的真值比较，改用冻结的实际音集与同调完整typed source。没有修改门槛来迁就测试。

初始构建缓存缺少Release优化，秒级测量不作正式性能结果。正式配置显式使用 `/O2 /Ob2 /DNDEBUG`。同时移除了新结构判断中按每个候选重复分析输入的开销。未运行G6十三组、G7全套UI、十二调、压力矩阵、旧安装生命周期或完整SDK Validator。

## 连续路线与试听

下面三条均来自上述唯一批次的正常首选，B/C/D为完整8和弦、32 QN来源。所有接缝是**算法评估的独立整句追加**，没有独立作品直接延续或编辑接缝审核证据。来源均为已标注的原创编辑进行。

媒体为 **120 BPM、4/4**；DAWClip MIDI本身不携带tempo/meter元事件，宿主需按给定速度与拍号试听。时间提示以WAV起点为0秒，包含A与全部已选后续；每个来源的前/后四小节各16 QN，无重叠事件。结尾保留原句，未用裁尾制造未解决感。

### 1. 四小节起点 → 平台展开 → 上行低音 → 隐主音发展

A=`F–C–Dm–Am`，4小节。B/C/D：`V8_01_002 → V8_01_010 → V8_01_003`。接缝 **QN16/48/80，即8/24/40秒**，总112 QN/28小节。

- B：`Bbmaj9–Dm9–C6–F/A | Bbmaj9–Gm9–Am7–Csus4`。
- C：`Cm9–Dm7–Ebmaj9–F6 | Gm9–Am7b5–Bbmaj9/D–Ebmaj9/G`。
- D：`Ebadd9–F/Eb–Dm7–Gm9 | Ebmaj7–Gm7/D–Cm9–Dm7`。

投影键为F大调→Bb大调→Bb大调，用于实化和解释；不凭此宣称目标调已由终止确立。C段低音的递进与D段侧重隐主音的组织应作为听评重点。

| 累计轮次 | WAV | MIDI |
| --- | --- | --- |
| A+B | [第一轮](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-1.wav) | [第一轮](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-1.mid) |
| A+B+C | [第二轮](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-2.wav) | [第二轮](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-2.mid) |
| A+B+C+D | [第三轮完整](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-3.wav) | [第三轮完整](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-3.mid) |

### 2. 原40 QN借用和声乐句的时值修复对照

A=`C(8)–Bb/D–F/A–Eb/G–Fm/Ab–C/G–F–C(8)`，中间各4 QN，保留10小节。B/C/D：`V8_01_014 → V8_01_003 → V8_01_010`。接缝 **QN40/72/104，即20/36/52秒**，总136 QN/34小节。两小节主和声准备后按4+4组织原句，首轮末C不再缩至一小节。

- B：`Dm9–F/C–Bbmaj9–C6 | Dm/A–Gm9–Bbmaj7–Csus4`。
- C：`Bbadd9–C/Bb–Am7–Dm9 | Bbmaj7–Dm7/A–Gm9–Am7`。
- D：`Gm9–Am7–Bbmaj9–C6 | Dm9–Em7b5–Fmaj9/A–Bbmaj9/D`。

投影键D小调→F大调→F大调，借用色彩的原句进入相对大小调展开。它与案例1共享主要词汇，是时值修复与不同发展顺序的对照，**不作为第三套完全独立的和声词汇**。可与[G8原完整WAV](../../HarmonyContinuation-g8-evidence/rcr-3/representative-paths/3-V6_B3_02_054/V6_B3_02_054-round-3.wav)对照首接缝18秒/20秒的差异。

| 累计轮次 | WAV | MIDI |
| --- | --- | --- |
| A+B | [第一轮](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-1.wav) | [第一轮](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-1.mid) |
| A+B+C | [第二轮](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-2.wav) | [第二轮](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-2.mid) |
| A+B+C+D | [第三轮完整](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-3.wav) | [第三轮完整](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-3.mid) |

### 3. Phrygian踏板 → 悬挂序列 → 半音低音线

A=`V8_01_006`，`Am7–Bb/A–C/A–Bb/A | Am9–Gm9–Bbmaj7–Gm9`，完整8小节。B/C/D：`V8_01_001 → V8_01_011 → V8_01_005`。接缝 **QN32/64/96，即16/32/48秒**，总128 QN/32小节。

- B：`Bbmaj7–C6–Dm7–F/A | Bbadd9–Dm7/A–Gm9–Csus4`。
- C：`Cm11–Fsus4–Dm11–Gsus4 | Ebmaj9–Abadd9–Bb/F–Cm9/G`。
- D：`Gm9–GmMaj7/Gb–Gm7/F–C9/E | Ebmaj7–Dm7–Cm9–F6`。

投影键F大调→Bb大调→G小调。C段悬挂和弦保持张力，D段实际低音G–Gb–F–E | Eb–D–C–F逐步演化；末F6按完整来源保留。该路线的调式/低音机制与前两条差异最大，听评重点是Csus4进入Cm11、持续低音G转入Gm9两处的自然性。

| 累计轮次 | WAV | MIDI |
| --- | --- | --- |
| A+B | [第一轮](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-1.wav) | [第一轮](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-1.mid) |
| A+B+C | [第二轮](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-2.wav) | [第二轮](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-2.mid) |
| A+B+C+D | [第三轮完整](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-3.wav) | [第三轮完整](../../HarmonyContinuation-g9-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-3.mid) |

每轮Snapshot3在相同目录同名`.snapshot3.json`，实际演奏重开检查通过。**以上为结构审查和实际可播放材料，未进行新一轮人工听感验收。下一开发Setup音乐质量门禁尚未通过。**

## 工程保护、分工与 Skills

匹配优先的Exact/Unified原始分数、60分门槛及原演奏身份保持；仅灵感视图增加结构资格与曝光排序。四入口、升级、祈愿抽样、Worker异步职责不改。Why仅增加结构依据/探索状态说明。Session5/Snapshot3格式不迁移；快照继续冻结实际演奏，新增结构/历史注记为运行期信息，重开不伪造接缝元数据。

主代理负责根因、结构与和声裁决、核心实现、排序和最终门禁；LUNA负责时间轴只读取证及定向测试/冻结起点工具实现。主代理执行并审核本轮构建与验收。人工音乐听评及真实DAW仍需用户完成。

实际读取并应用：`deja-search`（既有决定检索）、`diagnosing-bugs`（已保存反例和红/绿回归）、`stop-that-shit`（限制实现与复验范围）、`codebase-design`（共享核心判断接口）、`domain-modeling`（分离结构依据、来源与接缝审核）。检查了当前Skills清单、本地技能目录与可发现工具；没有发现专用音乐理论、VST3或SQLite技能入口，使用已有C++/CMake、SQLite只读查询及项目MIDI/Preview工具。未要求安装额外技能。

G7/G8工作树、旧产物及Factory版本保留；不操作生产插件或真实user.db，不自动推送、合并main、建Tag、Release或Setup。
