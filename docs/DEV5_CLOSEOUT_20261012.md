# dev.5 收尾与发布准备

核验对象：`0.10.0-dev.5`，源码构建提交 `730e8192620871aed394239155b1dd8044696f16`，Build ID `730e819`，Release/x64。工程自动化收尾完成，可进行新一轮 Cubase 验收。真实 R02 根因及人工音乐质量尚未闭环，正式发布条件未满足。

源码位于安全分支 `development/dev5-closeout-20261012`。二进制构建后只更新交付文档；交接源码 HEAD 与二进制 BuildCommit 分别记录于包内元数据。未改 main、正式 Tag、Schema、生产 User Library 或正式 Factory V4。

## 计划判断与最终实现

保留既有检索、对齐、评分、候选合并与输出管线。有效输入 duration/onset 决定 QN 缩放；完全缺少 QN 样本时，一致的导入 Session 拍号提供小节比例回退；证据冲突或时间信息未知时保留原型。特殊节奏保留相对比例。该方案集中在公共估计器，可供 Preview、保存和拖放共用。

SMF 导入先清除旧 Session 拍号，再判断当前抽取区段的有效拍号是否一致，修复连续导入造成的拍号残留。播放头 ProcessContext 只提供当前参考，不冒充历史选择区段的完整拍号图；依据本地 SDK 及 [Steinberg ProcessContext 契约](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/structSteinberg_1_1Vst_1_1ProcessContext.html)。复合拍分组、未知变拍 map 不推断。

About、推荐结果、高级诊断及导出使用共同的编译模式与权重来源；未产生当前推荐结果时显示 PENDING。交付默认 HarmonyFirst OFF / Phase3A OFF，ON 实验包为 HarmonyFirst ON / Phase3A OFF。

候选负例揭示了既有明确性质检查可被相邻 TemplateDeletion/QueryInsertion 绕过。现将同根相邻 gaps 纳入相同的明确低音、三度、七度冲突检查，两个方向都覆盖。未设置 R01、R02 或候选 ID 特例，未调整全局 60 分门槛、权重、配额或去重排序策略。

## 历史核销与来源

详细历史矩阵及 dev.4 ZIP 清单对照见交付包 `evidence/HISTORY_PROVENANCE.md`。历史工作包整体 PARTIAL：Phase3/3A/4A/4B/4C/4D 的实现或离线结果与真实宿主验收分开核销；若旧 raw 日志已不存在，仅保留历史报告结论。

dev.4 Source ZIP 的外层 SHA-256 为 `8DB0032DEA6D67CA3000454E464579948E2D7A75C18E586D65C9E2C589933F49`，内部 437 个清单条目通过。435 个项目文件与接手工作树语义一致，428 个字节相同，7 个仅行尾差异。原 Build ID `6efacf3-phase4d` 对应基线 `6efacf3` 加已交付工作树补丁；本轮将这些成果保存在新源码提交。

32/33 的登记差异来自增加 `Phase4DCubaseFeedback`。本轮实际 OFF/ON 各注册并执行全部 33 项，没有沿用历史通过数。

## R01 与 A08

合成输入 `Am → G#dim → G → D/F#`，各 2QN、末 OPEN，HF ON/P3 OFF：

| 配置 | 真实 ID 与结果 |
|---|---|
| Auto | V4_A08 经 A Minor 解释保留；similarity 0.769533，eligibility 60.1784，合并后 60.9041，Loop 第6，可通过“查看更多”访问。Auto 选 A Major，置信度约 19%，并非唯一调性断言。 |
| 固定 A Minor | V4_A08 第1，60.9041；FULL 4 / SKELETON 3。 |
| 固定 A Major | 当前输入无通过门槛的输出。 |

A08 已在可复现离线语境恢复，真实 ID 确认为 `V4_A08`，其续接为 `Am → Am → D`。旧截图的安装二进制与原始输入未提供，不能把离线 ID 确认当作截图身份确认。48 个 R01 四态/调性/时值组合的逐匹配信息在 `candidate-traces.txt`，精简表为 `R01_A08_trace.tsv`。无 A08 优先级提升。

HF 与 P3 分别改变检索结构解释和受控插入惩罚退款。两者独立，R01 中 P3 可再退款 8 分，存在同时补偿同一经过事件的可能；当前不合并、不自动叠加。P3 继续作为默认关闭的受控实验，长期去留需人工音乐验证及更广语料，现有数据不足以删除或默认启用。四态矩阵、有效续写、核心性质、Color、候选路径与去重均重新运行。

## 时间、结构及 R02

六拍号为 4/4、3/4、6/8、5/4、7/8、9/8；十条真实原型 × 六拍号 × OPEN/closed 共120组，另有缺时值拍号回退及真实 V4_B11 推荐输出检查。Preview 与保存/拖放文件实际音符起止读回相同，BPM 不改变 QN。3/4 的普通 4QN 原型按可靠输入缩放至 3QN；特殊长短节奏保持原型比例。未知拍号的 annotated SMF 元数据仍使用已说明的 4/4 兼容默认，不作为推断证据。

可复现 MIDI 均为合成测试数据：

| 文件 | 用途 |
|---|---|
| `FULL_SKELETON_F_Fsharpdim7_G_C.mid` | F 4QN → F#dim7 0.5QN → G 3.5QN → C 4QN；固定 C Major，OFF FULL4/SKELETON3，ON可4/4，输出保留原输入。 |
| `FULL_SKELETON_HF_Am_Gsharpdim_G_DoverFsharp_Aminor.mid` | ON、固定 A Minor 的 FULL4/SKELETON3 替代例。 |
| `Descending_bass_FULL_5chords.mid` | C → Cmaj7/B → C7/Bb → F/A → Fm/Ab，明确低音和缺五度对照。 |
| `Side_slipping_FULL_3chords.mid` | Cmaj9 → Dbmaj9 → Cmaj9，保留大七度、九度。 |
| `R02_F_G_Am_4QN.mid` / `R02_F3_G5_AmOPEN.mid` | R02 的等时与不等时/OPEN 输入。SMF 末音有限，导入后按界面设置 OPEN。 |
| `V4_*_<拍号>_output.mid` | 六拍号输出与 Preview/MIDI 回读证据。 |

R02 合成12组合：Auto 等时 OFF Color0/ON Color5，Auto不等时OPEN 两态 Color5；固定 C Major/A Minor 两种输入两态均 Color0。全库 Color 软家族检查仅有 COMMON_MINOR_015 的末尾 F–G–Am，无后缀且非 loopable；没有据此降低门槛或虚构有效续写。真实 Cubase 的成功/失败原始音符、调性、OPEN、安装版本和调用状态仍缺，根因 UNKNOWN。按中文清单导出两组本地诊断后才能进一步定位。

## Factory 定向提案

对 V4/657 的全部连续窗口、十二移调进行根音、明确性质及相对低音比较：目标八和弦、输入四和弦和目标后缀 F–G–Am–C 均无精确命中。审计 JSON 记录方法与全部相关命中，不把近似功能匹配当精确内容覆盖。

隔离候选 `CANDIDATE_V5_CHROMATIC_MINOR_001`：Am → G#dim → G → D/F# → F → G → Am → C，Develop、非循环、无终止承诺，建议 V5/658。使用现有 typed Schema2；经过减和弦及 Dorian IV 的现有功能模型不足处使用 Unknown，并保存音乐解释。设计数据没有伪造录制音符。

现有 db_compiler：658条、Exact Musical Duplicate0、Missing Metadata0、legacy merge6/6、canonicalization与DB读回通过。原657条 payload 语义差异0，仅增加一个ID。288组合覆盖 A/C/D Minor、六拍号、2QN/按小节QN、OPEN、抽象低音与HF两态；候选均正常评分第1，约77.7448。八度、上声部转位及 MIDI 读回通过；A/Am、G#major/G#dim、Dm/F/D/F#、D/A明确性质或低音冲突负例通过。试听 MIDI 和独立DB随候选ZIP提供，人工音乐质量 Pending。

V4基线 SHA-256：`74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534`。
隔离V5 DB SHA-256：`DB15757038C6C333E386499EF5BE5480A5D279215B34020E403A92E0BA607297`。
插件与默认 Setup 均为 V4/657；候选DB不能直接覆盖默认V4，内容升级需单独批准。

## 当前质量门禁

| 门禁 | OFF | ON | 原始证据 |
|---|---:|---:|---|
| 全部 CTest | 33/33 | 33/33 | `build-dev5-*/final-ctest.log`、`LastTest.log` |
| VST3官方Validator | 47/47 | 47/47 | `build-dev5-*/validator.log` |
| V4专项 | 13047检查 | 13047检查 | `build-dev5-*/phase4d/summary.txt` |
| 独立候选专项 | 41104检查 | 41104检查 | `candidate-*/summary.txt`、288行表 |
| 严格推荐矩阵 | 1440组 | 1440组 | `reliability/matrix.tsv` |
| 节奏/四态专项 | 331 trials /1324 runs，0 failures | 同范围，0 failures | `rhythm-decoupling/`、CTest原始输出 |
| 隔离安装生命周期 | 21检查通过 | 无ON Setup，提供便携包 | `installer/result.txt`及逐次安装日志 |

两模式严格矩阵 SHA-256 均为 `8DC3784E84CE4BC6D99EC5B1F70CE02214EB69EC3D81326FDEB26FD0663E54D8`，与现存 dev.4 矩阵一致。覆盖核心三度/七度/低音、Factory、User桥、旧Session/Snapshot、匹配/续写、Enrichment、MIDI、Preview、UI、Productization、模拟宿主和缓存等，逐项结果在日志。

安装复用原 V4 生命周期测试，仅改测试版本、路径和唯一测试注册名：无插件/0.9保护、plugin-only保留历史库、兼容library-only、文件占用拒绝、修复、卸载和新装均通过；个人设置与User哨兵、历史V3保留，测试注册清除。实际生产安装和真实Cubase未执行。历史完整RC3迁移28项只作为历史证据，未计入本轮21项。

构建工具环境：MSVC19.51.36260，Ninja/CMake，离线VST3SDK，Release `/O2 /Ob2 /DNDEBUG`。本机PDB工具冲突用 `_CL_=/Z7` 处理，未修改SDK源码。首轮8项原子替换因沙箱TEMP权限失败，改TEMP/TMP为工作区独立目录后8项复测及OFF/ON全量通过；失败日志保留，不更改产品逻辑来适配沙箱。

## 性能与资源

同机，源码固定，构建结束后运行；每个检索模式10次预热+80次统计。时间单位ms，CPU为80次测量区段累计，内存为进程观测。两个构建中的专项都显式测量两个检索模式。

| 构建/检索模式 | p50 | p95 | max | CPU | 工作集/峰值MiB |
|---|---:|---:|---:|---:|---:|
| dev4 / Legacy | 0.6306 | 0.7602 | 1.0804 | 62.5 | 14.63/18.13 |
| dev4 / HF | 0.6284 | 0.8591 | 0.9976 | 62.5 | 14.63/18.13 |
| dev5 OFF / Legacy | 0.6326 | 0.8012 | 0.9364 | 62.5 | 14.86/18.14 |
| dev5 OFF / HF | 0.6416 | 0.8366 | 1.5417 | 62.5 | 14.86/18.14 |
| dev5 ON / Legacy | 0.6225 | 0.8288 | 0.8544 | 46.875 | 15.41/18.79 |
| dev5 ON / HF | 0.6237 | 0.8396 | 0.9462 | 62.5 | 15.52/18.79 |

657→658的同轮候选比较：OFF进程Legacy p50 0.6496→0.6666、p95 0.8147→0.8533；HF p50 0.6232→0.6383、p95 0.7610→0.9265。ON进程见随包candidate-performance.tsv。该样本未见明显资源退化，尾延迟仍有系统调度波动，不外推为真实宿主实时性能保证。

现有压力测试还测长前缀、扩展和弦、四态80次切换、重复MIDI导入60次和多拍号。OFF长前缀p50约2.50–2.54，p95约2.91–3.31；四态切换p50 0.6771/p95 0.9551；重复导入p50 0.2644/p95 0.4561。缓存复用/稳定性和分配量详见phase4b-stability/performance/stress表。未在实时音频线程增加检索，也未增加无界搜索。

## 交付与24项核销

交付目录为工作区同级 `HarmonyContinuation-dev5-delivery-20261012/`。入口 `README_交付说明.md`，包含默认OFF Setup、OFF/ON便携ZIP、候选V5 ZIP、源码ZIP、Handoff ZIP、BUILD_INFO.json和SHA256SUMS.txt。每个ZIP都有内部逐文件哈希，外层校验覆盖独立分发物。

| 计划交付项 | 当前结果与证据 |
|---|---|
| 1 历史工作包 | PARTIAL，HISTORY_PROVENANCE.md逐阶段核销。 |
| 2 dev4来源 | YES，ZIP437清单及435源码内容对照。 |
| 3 实际开关 | YES，共同编译/调用入口，PENDING及结果状态测试。 |
| 4 R01 ID与评分 | YES，V4_A08与48组合trace；截图身份仍待诊断。 |
| 5 A08恢复 | YES（离线），Auto更多可见，固定小调第1。 |
| 6 3/4根因与修复 | PASS，原型4QN未按输入缩放/拍号残留及无证据回退已处理。 |
| 7 多拍号 | PASS，六拍号120组合及真实推荐输出。 |
| 8 Preview/MIDI | PASS，实际音符起止回读一致。 |
| 9 FULL/SKELETON | YES，两个可复现MIDI及HF差异说明。 |
| 10 低音/侧移 | YES，完整可读MIDI和缺五度/扩展音检查。 |
| 11 R02 | UNKNOWN真实根因；12组合/全库Color审计及缺口已记录。 |
| 12 全库重复/内容 | YES，657窗口审计与658编译完整重复检查。 |
| 13 目标续接候选 | STAGED，独立V5/658、288组合、试听MIDI。 |
| 14 内容版本 | 建议V5，V4原件与默认开发包保持657。 |
| 15 机制整合 | 完成评估；保留独立默认关闭，不自动双重补偿。 |
| 16 多样性/语义 | 严格矩阵不变，明确性质gap绕过修复，无强制配额。 |
| 17 OFF/ON | 各33/33、47/47及专项通过。 |
| 18 性能 | 当前与dev4同机对照、压力/导入/内存/CPU表。 |
| 19 安装升级 | 21项隔离生命周期通过，个人/历史数据保护。 |
| 20 Git | 本地代码已提交，安全分支同步；最终HEAD见包元数据。 |
| 21 Cubase开发包 | YES，双模式同身份只安装一个，带回退与哈希。 |
| 22 中文清单 | DEV5_CUBASE_ACCEPTANCE.md。 |
| 23 发布准备 | 本报告含草案变更、门禁、内容策略及已知限制。 |
| 24 人工项目 | Cubase实机、R01截图身份、R02双诊断、候选试听、窗口/DPI/拖放与工程重开。 |

## Release Notes 草案与发布决定

修复推荐状态展示与导入拍号残留；统一缺少时值样本时的拍号回退；修复对齐 gaps 绕过明确和弦性质检查；完善六拍号、Preview/MIDI和隔离内容测试。新增V5候选提案，默认Factory内容不变。兼容Schema与Session/Snapshot版本不变。

本轮允许再次Cubase验收。正式发布需R02真实成功/失败诊断闭环、ON包身份确认、候选和Color音乐试听、真实工程重开及交互验收，并取得决策端批准。人工步骤见 [中文清单](DEV5_CUBASE_ACCEPTANCE.md)；诊断从“诊断/宿主诊断”中的“导出本地输入与推荐诊断”保存，成功/失败各一份。

## 最终标记

HISTORICAL WORK PACKAGES RECONCILED：PARTIAL  
DEV4 SOURCE PROVENANCE VERIFIED：YES  
HARMONYFIRST STATE VERIFIED：YES  
R01 A08 ID CONFIRMED：YES（离线；截图身份待核实）  
R01 DESIRED CONTINUATION COVERED：STAGED  
R02 HOST ROOT CAUSE：UNKNOWN  
METER-AWARE MIDI OUTPUT：PASS（已知/一致时间证据范围）  
PREVIEW-EXPORT CONSISTENCY：PASS  
FULL-SKELETON VERIFIED：YES  
FACTORY V4 BASELINE CHANGED：NO  
FACTORY CONTENT ADDITION PROPOSAL：YES  
OFF REGRESSION：33/33  
ON REGRESSION：33/33  
USER LIBRARY PROTECTED：YES（隔离测试/未触及生产库）  
GIT PROVENANCE VERIFIED：YES  
READY FOR CUBASE RE-ACCEPTANCE：YES  
READY FOR FORMAL RELEASE：NO  
FORMAL RELEASE PERFORMED：NO
