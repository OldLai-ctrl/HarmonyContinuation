# 当前接续状态

## 本轮工作与下一步

### 工作包B / Factory V6 B3（2026-10-25）

独立分支 `development/factory-v6-20261024`，接手 `d9d4539`；用户听过首轮10组未发现明显音乐问题、未确认风格标签，并已批准B3。[当前集中裁决与产物](docs/FACTORY_V6_REVIEW.md)。旧56最终45A/2B/9C，原4待审隔离；新增手工110候选107A/1B/2C，采纳87A，另20A预算待决。总170提案，A152/B3/C15；实验790条（V5 658＋132采纳），781个跨移调完整音乐身份、741个低置信核心桶。V5内容与旧60条音乐字段不变，9组V5重复核实为整体移调同形，未删旧ID。

Release定向1/1通过：132源合法前缀实际推荐及Snapshot/Preview/MIDI、517底层前缀、1584移调、49固定C minor；新增四代表12调排名/来源序列稳定。原四推荐组与Enrich/Wish API均消费新内容，非GUI/DAW验收。固定C严格池Dorian24、Lydian20、Phrygian13、Mixolydian24、爵士旋律小调6；Locrian与古典双向仍0。风格仅编辑用途建议，常用程度unrated，仍缺可靠Neo-Soul/R&B/City Pop风格来源。

真实A shadow810触发800输入预算截断门禁；受测full-only/source miss均0，Top20保留3040/3040。保留790实验与20A待决，暂停扩大收录；先集中裁决最小提高预算/规划规模内全扫描方案，不改Unified评分、门槛或权重。核心数据工程通过，V5正式哈希不变、真实user.db未访问；未安装/激活V6、未推送/合并main/Tag/Release。完整插件/DAW及安装生命周期仍属B4。下列G01记录为保留基线，本工作线不更新其验收。

### G01 / G4-H 真实环境验收（2026-10-24，Pending）

唯一受控候选保持 **0.11.0-dev.2 / f99dfa1657b04db0af50d76795141ea24b50ae10**，接手交付记录f2c3ee6。未重打包或修改二进制；本轮只读重核Setup SHA为 `08B0D6516BE1BF6EF5F4FD48D0FC523C9B221D6B09DBE1B928201C8C8D621ABA`，候选模块版本/提交/哈希与冻结身份一致。[当前证据](../HarmonyContinuation-g04h-acceptance-20261024/identity-and-environment.json)、[实机逐项清单](docs/G01_G4_ACCEPTANCE.md#g4-h真实验收进度2026-10-24)。

本机标准路径仍为生产0.10.0-rc.1，Factory哈希保持V5基线；尚未获准替换为dev.2，已请求用户指定测试环境及DAW操作者。本机Cubase可执行文件元数据15.0.30 / 15.0.30.287；未启动或测试，FL Studio版本待提供。标准路径/UAC、新版标准安装后加载、Cubase/FL、双实例与音频所有权、实际抽取听评全部Pending，不将G4-R的58/47历史结果当作本轮实际执行。

202诊断/恢复guard已对照源码审查；[人工处理说明](docs/INSTALLER_AND_LIBRARY.md#dev2恢复失败与人工处理)补齐保留日志与整个恢复目录、journal逆序恢复受管理文件、再恢复精确登记、核验后清标记的维护者流程，并修正旧“直接重跑修复”建议。未故意制造不可恢复状态，无人工恢复演练PASS。主代理负责包身份、恢复文档和RC判断，Luna进行只读恢复契约检查。

当前工程基线可继续验收，但真实环境和音乐质量证据不足，**不建议冻结RC1**。等待获准环境及实际操作者结果；既存文档改动保留，未合并、推送、Tag或Release。

### G01 / G4-R 安装失败恢复（2026-10-24）

恢复专项技术门禁通过，可进入RC候选评审；真实Cubase/FL Studio、标准路径/UAC及人工听评仍待验收。本地分支 `development/g01-chord-wish-20261023`，新开发构建 **0.11.0-dev.2 / Release x64**，冻结BuildCommit `f99dfa1657b04db0af50d76795141ea24b50ae10`；后续测试/文档提交与构建身份分开。唯一新[Setup](../HarmonyContinuation-g04r-delivery-20261024/HarmonyContinuation-0.11.0-dev.2-Setup.exe)、[身份和证据](../HarmonyContinuation-g04r-delivery-20261024/BUILD_INFO.json)、[根因/修复及人工清单](docs/G01_G4_ACCEPTANCE.md)。旧dev.1/fb68a6e包保留为失败诊断资产，禁止正常分发。

- 根因与修复：原关键状态写入晚于Inno原生提交，异常不改变成功状态；旧恢复遗漏支持文件/卸载登记且不核验。关键写入已提前，修改前捕获受管理文件/哈希/卸载器/完整登记；失败按顺序恢复并验证，关键失败返回非零。无法恢复时保留材料并阻止重试，不重写安装器或扩大个人数据删除范围。
- 本轮执行：SDK Validator 47/47；隔离生命周期58项通过。原组件状态写入失败、Factory后中断、manifest后中断均exit3，注册后升级/新装中断均exit201。四份升级故障快照中全应用目录/卸载器、登记键值/类型/子键、插件、Factory树/活动指针、V4及个人哨兵前后一致；[故障证据](../HarmonyContinuation-g04r-delivery-20261024/isolation-lifecycle-1/upgrade/test-evidence/)。恢复失败202分支经代码审查，未人为制造不可恢复状态，不计执行PASS。
- 生命周期：新装失败后正常重试、正式0.10.0原脚本/原载荷升级、组件增删及损坏修复、直接静默降级安全拒绝、卸载开发版后重装正式版回退均通过；旧/新真实模块SDK加载和moduleinfo校验通过（工厂/类信息查询，非DAW）。最终隔离卸载exit0、测试注册/托管组件清理、用户哨兵和V4历史保留，见[清理证据](../HarmonyContinuation-g04r-delivery-20261024/isolation-lifecycle-1/cleanup-result.txt)。最终测试脚本两项审查修正仅做语法/直接检查及局部bundle查询，未重复整个生命周期。
- 身份与保护：Setup SHA-256 `08B0D6516BE1BF6EF5F4FD48D0FC523C9B221D6B09DBE1B928201C8C8D621ABA`；构建、暂存、解包及实装插件一致。Factory源码/正式包/新包/实装均为V5/Schema2/658，SHA `F989A83EA9EF92BA7C272BF0E5E3C7073C225899DC81D60DD46C10EA6B48CB3C`。生产插件仍0.10.0-rc.1，前后hash不变，生产Factory不变，真实user.db未访问。Schema/Session5/Snapshot3和调式数据不变。
- 范围与后续：仅安装机制及版本身份重建；G3音乐/UI/原生图和G4标注/音乐12646项证据按未改源码范围复用，未当作本轮新执行。主代理负责根因、事务修复和最终门禁，Luna负责隔离测试及独立只读检查。下一步由决策端审查进入RC候选及实机验收；未推送、未合并main、未建Tag/Release，既存协作文档改动保留。

### G01 / G3 和弦祈愿 UI（2026-10-23）

本地分支 `development/g01-chord-wish-20261023`，G3接手 `58222d0`（包含G2核心及全库标注；`0f0ff63`为其祖先）。实现第六同级入口及A3详细/简略卡片，技术门禁通过后停在G3，等待决策端审查G4。开发构建版本为 `0.10.0-g03.dev.1`；精确源码提交与二进制BuildCommit分别在Git和产物内核对。

- 界面：六入口共用现有布局，800×780及100/125/150%缩放；快捷预设和进阶多选共用 `WishSettings`。默认自由随机，独立输入关联/清除、12主音、10种调式身份（自然大小调含中古同义身份）、严格筛选、六类现有风格及结构色彩方向。未审核归属不进入调式池，数据不足返回实际数量。
- 对接：`WishWorkflow`在独立非实时worker调用G2 `makeFreePool` / `makeInputPool` / `WishRound`，没有另写抽样或资格算法；保存同批次一抽/十抽/再十抽的无放回状态。generation/revision阻止旧任务覆盖；清除和参数/输入约束变化清理结果及手势。自由模式不消费全局进行、调性分析或推荐历史；清除不写Session或用户库。
- 对象：卡片持有 `shared_ptr<const WishItem>`，来源、可选匹配、调式中心及常用程度与完整路线绑定。复用来源播放带、原输入对齐、Why/色彩说明、PreviewTransport、MIDI和Snapshot3。自由卡完整来源/空续写；输入卡保留Unified资格和匹配边界。Controller验证当前对象后取冻结路线；自由卡不受旧“仅后续”试听范围影响。
- 实际验证：Release插件构建成功；现有 `WishCoreFocused` 12646项、`UILayoutTests` 146480项通过，[首轮四目标日志](../HarmonyContinuation-g03-validation-20261023/tests-first.log)。新增覆盖是异步适配、风格/色彩过滤及第六入口交互缺口。`CompactEditorUI` / `WishEditorUI`在最终界面修复后通过，[UI最终日志](../HarmonyContinuation-g03-validation-20261023/tests-ui-final.log)；含真实V5、连续21抽、无自动播放、空输入/空池、跨页、窗口重开、上下文变更、MIDI/Snapshot音乐身份及离线试听非静音检查。插件构建自动执行SDK Validator，47/47通过；未运行无关全量CTest。首次构建遇到SDK调试符号C1902，本轮验证环境使用编译器 `/Z7` 嵌入式调试信息后通过，未改SDK或跳过编译检测。
- 原生证据：[100%简略](../HarmonyContinuation-g03-validation-20261023/build/wish-ui-evidence/wish-100-compact.png)、[150%详细](../HarmonyContinuation-g03-validation-20261023/build/wish-ui-evidence/wish-150-detailed.png)、[150%进阶](../HarmonyContinuation-g03-validation-20261023/build/wish-ui-evidence/wish-150-advanced.png)、[输入匹配](../HarmonyContinuation-g03-validation-20261023/build/wish-ui-evidence/wish-input.png)。使用真实VSTGUI离屏绘制/PNG；首轮中间截图缺少绘制配对导致空白，修复测试绘制后重新生成并人工核图。
- 来源与边界：版本化标注嵌入开发二进制，并测试与v2文本目录一致；正式Factory内容及Unified/Schema2/Session5/Snapshot3未改。本轮构建DB与实际已安装V5的SHA-256再次一致，均为 `F989A83EA9EF92BA7C272BF0E5E3C7073C225899DC81D60DD46C10EA6B48CB3C`。窗口关闭/重开在控制器内存中保留祈愿回合；祈愿参数、聚合来源标签及调式目录不写入Session5或Snapshot3，Snapshot3冻结实际音乐、主要来源与可选匹配。
- 分工与待办：主代理负责UI/Controller、音乐语义、架构和最终门禁；Luna负责worker适配、定向测试与只读审查。原有协作文档改动保留。G4尚待开发Setup/打包哈希、安装生命周期、真实Cubase/FL Studio操作及人工听评；正式Setup包内DB直接比对的既有缺口保留。未推送、未合并main、未建Tag或Release。

### G01 / G2 和弦祈愿核心基线（2026-10-23）

分支 `development/g01-chord-wish-20261023`，接手HEAD `99310f1`，基于正式源码 `c8d8dc3`。G1由用户结项并批准G2；本轮仅完成独立多调式标注及双路径抽样核心。规格和全库编辑标注在[标注目录](data/mode-annotations/v2/README.md)，已批准决定见[D12](DECISIONS.md#d12-g01-多调式归属与独立抽样g2)。以上为G2交付边界；当前G3技术状态见上节，G4尚未验收。

- 实现：精确ID/源音乐身份绑定、多归属与编辑常用程度、明确的未标注/未审校/无法确认/失配状态；纯随机输入隔离、Unified原资格约束路径、12主音、多选并集/严格筛选、完整音乐身份去重、固定种子及连续无放回。输入路径保留全局shortlist预算并逐源资格判定，避免引擎原后缀聚合丢失不同来源前缀。相关原评分、Factory、Schema2、Session5、Snapshot3未改。
- 数据（本次完善）：658条全部有来源绑定记录，共727个归属；618条全部解释已审校、15条兼有已审校/未确认解释、25条仅有未确认解释。633条至少有一个已审校解释；69条多归属。687个已审校归属覆盖7种调式，40个未确认解释不参与调式筛选；Locrian及两种旋律小调缺乏充分证据而保留空池。无约束实化去重仍为7788条完整音乐。评级与音乐证据见标注目录，用户实听待完成。
- G2核心首次验证（本次标注完善前）：Release / HC_BUILD_PLUGIN=OFF，`WishCoreFocused`报告1001检查通过；`MidiExportTests`598、`PreviewTests`129、`ContinuationEngineTests`323/323通过。覆盖真实V5加载、源失配、多归属、12调、完整前缀身份、单轮抽样与完整/空续写Snapshot3及MIDI字节往返。[首轮日志](../HarmonyContinuation-g02-validation-20261023/tests.log)中Wish曾因合成功能投影缺失而失败；修正夹具后[Wish最终日志](../HarmonyContinuation-g02-validation-20261023/tests-wish-final.log)通过，其他三个目标未重复运行。默认Debug探测曾报C1902，改为Release探测后构建成功；未跳过编译器检测。
- 本次标注完善实际验证：Release / HC_BUILD_PLUGIN=OFF，扩展现有 `WishCoreFocused`，12585项检查通过，涵盖全658条记录/源指纹、完整音符严格池、真实未确认条目的无约束可抽/调式排除，以及既有输入隔离、抽样、Preview/MIDI/Snapshot3往返通过；结果见[最终日志](../HarmonyContinuation-g02-validation-20261023/tests-annotations-v2-final.log)。只重跑这一相关目标，独立历史测试未重跑；实际安装库与构建库哈希再次一致。
- 来源证据：[哈希核对](../HarmonyContinuation-g02-validation-20261023/source-hashes.log)显示本轮源码重建库与已安装V5逐字节SHA-256一致，均为 `F989A83EA9EF92BA7C272BF0E5E3C7073C225899DC81D60DD46C10EA6B48CB3C`；正式Setup本体与其既有哈希一致，包内DB尚未直接解包比对，继续保留此证据缺口。
- G2核心工程验证通过；后续G3技术状态见上节，真实宿主、用户实听及开发Setup留待G4。未推送、未合并main、未建Tag或Release；接手前既存协作文档改动保留。

### 正式基线与既有整理

2026-10-23：当前发布版本 **v0.10.0**，正式源码以 `v0.10.0` Tag 固定，main 为正式发布主线。[正式 Release 与安装包](https://github.com/OldLai-ctrl/HarmonyContinuation/releases/tag/v0.10.0)提供唯一 `HarmonyContinuation-0.10.0-Setup.exe`、SHA-256 和精确 BuildCommit；功能与已验收 RC1 相同，仅调整正式版本元数据、构建配置及文档。

用户已明确确认 RC1 / `435ed85` 在 Cubase 和 FL Studio 实机全部验收通过，含播放手柄、升级高亮、起点曲线；此为用户提供的 RC1 证据，正式二进制未另行 DAW 实测。正式构建的相关 CTest、Validator、安装后加载、Factory V5/658、Session/Snapshot/用户库、RC1升级与数据保护的实际结果随 Release 发布；不把历史通过数字当作新的运行结果。

Factory V5 / 658 / Schema2、Session5 / Snapshot3、推荐质量与音频实现不变。发布与回退说明见[正式更新说明](docs/V0_10_RELEASE_NOTES.md)及[安装文档](docs/INSTALLER_AND_LIBRARY.md)，原RC技术证据见[RC报告](docs/DEV15_RC_REVIEW.md)。完整身份与最终门禁见Release资产。

2026-10-23 仓库精简：按用户确认，本地及远端仅保留 `main` 和正式 Tag `v0.6.0`、`v0.8.0`、`v0.9.0`、`v0.10.0`；撤除过程分支、四个阶段Tag、三个临时工作树及旧未发布交接草稿。精简前全部引用和未提交文档已在同级 `_archive/HarmonyContinuation-repo-cleanup-20261023-180326/` 冷备份，包括唯一未合并的两行状态文档提交。保留主工作区协作文档改动、完整主线提交历史及正式发布资产；本轮仅维护文档，不重建插件或移动正式Tag。正式 BuildCommit 仍为 `c8d8dc364b616e8d75402d977fa0cb9083d760e6`；后续文档提交与构建身份分别核实。以下开发分支及草稿记录均为历史。

### dev.14 历史交付（本轮用户报告Cubase验收已全部通过）

2026-10-19：dev.14 推荐展示与匹配解释升级完成。开发分支 `development/recommendation-browser-20261019`；源码 / BuildCommit `b4e6f65ebefad14e7345d797d941af6d49abbb5d`（`b4e6f65`）已推送 origin 同名分支。唯一测试包：[0.10.0-dev.14 Setup](../HarmonyContinuation-dev14-delivery-20261019/HarmonyContinuation-0.10.0-dev.14-Setup.exe)，Release / Factory V5 / Schema 2 / 658条。包、moduleinfo、二进制版本与提交一致，暂存插件/Factory与最终构建字节一致；dev.13源码和安装包保留。

- 推荐浏览：五类入口保留；所有展示合格候选进入同一滚动列表，每页最多10条。Top3仅为优先标识，探索有独立标签。引擎取消资格之后的每类10条截断；检索预算、硬约束、中性资格门槛60、精确聚合、评分与用途策略不变。不能将新增可见数量解释为音乐质量提升。
- 总览：当前分类全部结果与主卡片共享最终索引顺序；可定位到正确页及卡片，复制含来源、指纹、匹配位置、插入/跳过计数及用途依据的完整文本。升级按已有润色→丰富→进阶顺序合并，不作跨组重评分。翻页、分类、详情、试听和MIDI使用原候选引用，刷新清理失效选中及手势。
- 对齐：上方完整来源播放带保留数据库和弦、QN与级数；下方原输入对照消费Matcher轨迹。R01实际得到 Am→iv、G#dim→输入插入、G→III、D/F#→VII、Em→i；模板跳过保留独立红色缺口，输入插入为褐色，均有文字及OPEN/续写边界。没有原query的Snapshot仅显示冻结路线，对齐标为不可用。Session5 / Snapshot3 / UserLibrary格式及Factory数据未修改。
- 自动化：[首轮相关目标](../HarmonyContinuation-dev14-validation-20261019/tests.log)中7项通过；UILayout发现小窗口候选区被分页条挤压，移到固定试听栏末行后，[最终UILayout与CompactEditorUI](../HarmonyContinuation-dev14-validation-20261019/tests-final-ui.log)通过，共9个不同定向目标。新增覆盖0/1/3/4/10/11/20/21分页、完整列表/总览同序、跨页动作身份、保存意图恢复与失效选中、11条合格路线的有限/无限保留同资格同分、trace/缺口与旧快照降级。未重复全量CTest、Validator或安装生命周期。
- 音乐集成：[8组真实Worker检查](../HarmonyContinuation-dev14-validation-20261019/integration/unified.txt)通过，3493检查；C–G–Am保留15634125、四和弦Loop、R02两种节奏的Color、借用iv回接及R01一条默认/三条探索均保留。308是八组结果累计，包含重复输入，不作为覆盖或质量评分。
- 原生界面：[R01](../HarmonyContinuation-dev14-validation-20261019/R01-ui.log)、[模板跳过](../HarmonyContinuation-dev14-validation-20261019/template-skip-ui.log)、[七和弦来源](../HarmonyContinuation-dev14-validation-20261019/source-route-ui.log)通过；含100/125/150%入口与点击、详情、MIDI分发，七和弦离线音频及2种配音×3范围MIDI/Snapshot一致。[四张交付截图](../HarmonyContinuation-dev14-delivery-20261019/screenshots/)为真实VSTGUI渲染；模板跳过使用既有M05测试数据，不加入Factory。

工程、定向自动化、原生渲染与唯一测试包完成。主代理负责候选保留/来源语义、UI集成和最终复核；Luna完成只读链路审计、纯展示模型与性质测试补丁、状态风险审查。未修改独立Phase7工作树，未合并main、未建Tag/Release。现有未提交协作文档保留。

安装、Cubase实机与人工音乐听评均为Pending。关闭Cubase后安装主程序及Factory V5，About核对 `0.10.0-dev.14 / b4e6f65`；复验分页/分类/总览、试听及完整/续写MIDI与拖放对象、保存重开和旧快照。听评原来源与输入差异、R01探索及借用iv回接。旧Snapshot缺原query时无法恢复原输入对齐；总览过长文本可复制完整清单。可进入RC技术评审，人工验收和正式发布待决定。

### dev.13 已交付基线（历史）

2026-10-19：dev.12 同步审计与新增输出/UI要求完成。当前分支 `development/source-route-ui-20261019`，生产源码 / BuildCommit `0db40a3681a2ed0d7b4ebf4599e4d7902da6cccc`（`0db40a3`）已推送 origin 同名分支；后续状态文档提交不改变构建身份。唯一新增测试包：[0.10.0-dev.13 Setup](../HarmonyContinuation-dev13-delivery-20261019/HarmonyContinuation-0.10.0-dev.13-Setup.exe)，Release / Factory V5 / Schema 2 / 658 条；二进制、moduleinfo、Setup版本一致，暂存插件及Factory与最终构建字节一致。原dev.12包保留。

- 主界面：收束、发展、循环、色彩、升级进行五个同级入口。升级复用A3卡片、逐和弦QN、More、试听/MIDI/详情和固定试听栏；不修改独立Phase7工作树。
- 输出：输入作为检索条件保留在顶部和Session；选中候选使用其真实代表来源的完整路线，从头包括匹配位置前的和弦，采用库中性质、扩展、低音、转位、节奏和拍号。循环保留已有有限回接；显式旋律硬约束仍需通过。评分、门槛、用途证据及精确续写聚合不改；多来源仍由统一引擎选择完整代表解释。
- 实际路线检查：真实V4_B12合法匹配4–6/7，通过隔离该库条目的定向检索验证 Cmaj7–Ebmaj7–Dbmaj7–Cmaj7–Fmaj7–G7–Cmaj7，7和弦/28QN，续写边界24QN；全库Worker保留其来源证据，不要求它固定胜出代表竞争。音频渲染、两种配音×三个MIDI范围与快照读回一致。
- 兼容：新快照用现有Snapshot3字段冻结实际演奏前段/后段；旧快照继续使用内嵌音乐，不从当前曲库重建。Session5、Snapshot3、Factory、UserLibrary格式不改。原始输入、评分和宿主音频线程契约保留；过期代表来源操作被拒绝。
- 必要验证：[8项定向检查](../HarmonyContinuation-source-route-validation-20261019/tests.txt)全部通过（UILayout、MIDI、Preview、Productization与Library snapshot/user/factory）；[真实来源输出及原生界面](../HarmonyContinuation-source-route-validation-20261019/source-ui.txt)、[R01探索入口](../HarmonyContinuation-source-route-validation-20261019/exploration-ui.txt)通过。原生图在同目录source-ui/，升级试听/MIDI/详情及100/125/150%入口检查通过。沙箱临时目录的文件替换限制改用项目临时目录后通过；未改数据代码。只制作一个Setup，未重跑全量CTest、Validator或安装生命周期。
- GitHub审计：unified本地/远端 `013b4c2`，Phase7本地/远端 `c17a795`，两分支仅MAP/NOW文档不同，均含dev.12源码 `e164ccd`；main本地/远端仍为 `bf77019`。统一执行链、旧调用归一、V5继承V4的657条及独立15634125路线完整保留；复用dev.12七例/2522检查等原证据，未重新开发算法。

工程、定向自动化和原生界面检查完成。主代理负责输出语义、实现审查及集中门禁；子代理承担只读基线审计和局部UI实现。尚未安装此包、未完成真实Cubase加载/拖放/MIDI/保存重开验收或人工音乐听评，均为Pending。可提交RC技术评审；RC验收与正式发布仍待决定。关闭Cubase后安装主程序及Factory V5，在About核对 `dev.13 / 0db40a3`，听评完整来源适配、循环回接及探索路线。未合并main、未创建正式Tag/Release；原有未提交协作文档保留。

### dev.12 已交付基线（历史）

2026-10-19：统一推荐引擎、版本化流行变体及已批准的 Phase 7 正式界面已完成本地集成。开发分支 `development/unified-engine-20261019`；真实生产源码 / BuildCommit `e164ccdee38683459e74656d0c1555c3bfa1fb0f`（`e164ccd`）。唯一测试包为 [0.10.0-dev.12 Setup](../HarmonyContinuation-dev12-delivery-20261019/HarmonyContinuation-0.10.0-dev.12-Setup.exe)，Release / Factory Library 5 / Schema 2 / 658 条。暂存插件与 Factory 均和该构建字节一致，moduleinfo 与二进制版本/提交一致。

- 单一引擎：复用原和声检索、HarmonyFirst 结构/节奏资格分离及严格方向扩展兼容；保留 dev.11 精确身份、有限多调性解释、硬约束、用途证据和中性资格。门槛仍为60，不求三模式并集、不填满Top3。调性置信度只解释，不再进入音乐质量；候选和调性并列顺序归一到输入根音，十二调同分性质检查通过。旧实验状态未持久化，旧调用归一到同一 Worker；正式 Controller/UI 无三算法分支。
- 曲库：只追加完整 `15634125`，与既有 `15634145` 保持独立身份。冻结 V4 的657条完整payload/typedData逐条一致；新库独立版本目录、防降级及旧库官方内容识别通过。后续按[有限家族审校与前缀门禁](data/factory-proposals/20261019-pop-variants/family-review.md)收录，不自动组合生成。
- 实际输出：C–G–Am 的 Em–F–G–C、Em–F–C–F–G、Em–F–C–Dm–G 均默认可达。Am–G#dim–G–D/F# 仍为默认 Em 1条，加 G–C–Em、C–G–Em、Em–D–C 三条通过资格的探索路线；后者基础分63.27、排序63.51，单独 More 可访问，不宣称优质 Develop/Loop。F–G–Am Auto 保留色彩后续，固定调性继续约束解释；四和弦及借用iv循环通过。排名和数量不代表听感质量。
- Phase 7：按独立工作线提交 `adb88dd` 合并保留设计历史；正式800×780，详细/简略切换、全组More/探索、独立MIDI手柄及固定试听栏。完整/后续与跟随宿主/自定义BPM按实例工作，后续从完整配音裁切，原始QN及MIDI导出独立；旧无尺寸Session迁移默认保留。没有重写算法/配音或宿主音频契约。
- 必要验证：[七个定向目标](../HarmonyContinuation-unified-validation-20261019/targeted-tests.txt)、[七例真实Worker/Snapshot/Preview/MIDI集成](../HarmonyContinuation-unified-validation-20261019/integration-result.txt)（2522检查，含13个扩展兼容方向门禁）、合并后的[五个直接相关目标](../HarmonyContinuation-unified-validation-20261019/merged-tests.txt)及[真实R01原生探索入口](../HarmonyContinuation-unified-validation-20261019/exploration-ui.txt)均通过。V5新路线快照/MIDI回读通过；Session、Snapshot及个人库格式不变。仅一次最终Release插件构建和一次Setup，未跑全量CTest、Validator、安装生命周期或全库排列。

本地工程和测试包完成；未安装或核验真实Cubase。关闭Cubase后安装主程序与Factory V5，About核对 `dev.12 / e164ccd`。待用户听评新长线变体、D/F#→Em及三条探索路线、借用iv回接；新版UI的真实宿主拖放和试听交互仍需验收。2026-10-19 Fetch确认unified与Phase7两分支已在GitHub完整保存e164ccd，源码相同，仅状态文档不同；未合并main、未建Tag、未正式发布。既有未提交协作文档改动保留。

### dev.11 已交付基线（历史）

2026-10-17：方案 A 核心推荐决策链修复已完成。独立分支 `development/core-decision-chain-20261017`；生产源码提交 `1c671521603949768eae666656a120b9f56f5f18`，已推送 origin。唯一测试包为 [0.10.0-dev.11 Setup](../HarmonyContinuation-dev11-delivery-20261017/HarmonyContinuation-0.10.0-dev.11-Setup.exe)，BuildCommit `1c67152`；一次 Release 插件构建和一次打包通过，暂存插件与构建字节一致，moduleinfo 与二进制版本/提交一致，Factory V4 / Schema 2 / 657 条。

- 决策链：有限 template–key 解释保留至实化及硬约束；精确音乐聚合保留低音、性质、扩展、精确 QN、OPEN 和按输入锚点归一的实现约束。每条音乐只占一个位置，代表解释来自真实匹配，不拼接 subscores。来源数只作证据，支持分为零。
- 基础资格固定使用 style/intent/prior=0.5、support=0，门槛 60；保留原权重与三模式节奏尺度。偏好、来源质量、完成度和软旋律适配只排序；多样性只选默认位置。ProductServices 与色彩重排使用显式资格。用途来自实际路径；缺乏充分证据的路线标为探索性，只进入有界 More。
- 关键对照：C–G–Am 的 Em–F–G–C 在三模式均为默认 Develop 首项；单 F Loop 仍合格，现位于 More。C–G–Am–F 保留完整下一轮；C–Fm–G 的 Dm 在 Auto/固定 C major 仍为默认 Loop。F–G–Am 的原五条 F major Color 均保留，Forced Key 只检索指定调性。Am–G#dim–G–D/F# 的两条近似三和弦路线改为探索性；单 Em 唯一，依据实际 D→Em 作 E minor 模态收束解释，未继承 Loop/Develop。计数与排名变化不构成听感质量结论。
- 兼容：Session 5 / Snapshot 3 / Factory / UserLibrary / 宿主及音频线程格式与职责保持；Session pin 字符串资源上限由 8 扩至 16 KiB，以容纳完整实现引用。旧引用完整恢复音乐后匹配，缺失实现约束的旧 pin 保持不可用，避免错误绑定；旧 Snapshot 保留原音乐，重算后才获得新推荐。Why/Snapshot 按真实 key 与匹配窗口取代表解释。
- 必要证据：[8 个定向测试目标](../HarmonyContinuation-core-chain-validation-20261017/checks-final.txt)全部通过；[13 例 worker 集成](../HarmonyContinuation-core-chain-validation-20261017/integration.txt)通过、重复音乐为零，包括实际 Preview、Snapshot、两种配音×三个 MIDI 导出范围的读回，以及有顶声部/事件切分的时间锚点检查。性质检查覆盖复制/重排来源、细小时值/OPEN/低音/扩展、偏好隔离及首选 key 硬约束失败后次解释保留。dev.10 [有限对照](../HarmonyContinuation-core-chain-validation-20261017/before/consistency.txt)与 dev.11 [实际结果](../HarmonyContinuation-core-chain-validation-20261017/after/consistency.txt)保留。算法职责见 [Continuation Engine](docs/CONTINUATION_ENGINE.md)。未重跑完整 CTest、Validator、安装生命周期或全库穷举；安装契约未改。

工程与单包交付已完成；尚未安装或进行真实 Cubase 加载、保存重开和音乐听评。关闭 Cubase 后安装上述 Setup，About 核对 `0.10.0-dev.11 / 1c67152`，使用顶部 Legacy、HarmonyFirst、Abstract 按钮切换；工程重开恢复 Legacy。重点听评经典完整续写、普通循环及借用 iv 回接、D/F#→Em 收束与探索路线。主代理负责算法判断、实现和集中最终复核；Luna 只读取证并提供性质测试补丁。Phase 7 工作树未修改；未合并 main、未建 Tag、未正式发布。原有未提交协作文档差异保留。

## 已完成阶段

Phase 3 系统审计、Phase 3A 受控减和弦插入实验、Phase 4A 决策分析及 Phase 4B 节奏/和声检索审计均已形成报告。Phase 3A 补偿保持默认关闭；相关历史范围和证据边界见各阶段报告。

## 功能基线

- 当前正式版本：`0.9.0`；发布提交与 `v0.9.0` Tag 均为 `ddc4f89babd4520daac3eae74824d6f581ce0e75`。源码清理提交不改变该正式包的构建身份。
- 正式版为 Factory Library 3；本轮独立开发版本为 Library 5（继承冻结V4并追加一条路线）。Factory Schema 2 / User Schema 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2 保持；生产数据与宿主适配未改。
- Factory V3 为 155 条 canonical 历史记录加 474 条新进行，共 629 条；六条 QUESTIONABLE 留在非生产清单。统计见 [LIBRARY_V3_SUMMARY](LIBRARY_V3_SUMMARY.md)，兼容与降级边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。

## 历史接续记录

2026-10-13：Phase 5 / dev.6 代码构建提交 `3a582e4`。新增默认关闭的 Abstract 方向性扩展和弦匹配，改善真实 Factory V4_B11 的简单输入召回；修复 UserLibrary 并发首次初始化，保持 Factory 确定性编译。默认 OFF、HF-ON、Abstract-ON 三模式各 CTest 33/33、Validator 47/47、V4 专项13,048、隔离候选专项41,105通过；原1440矩阵哈希不变。最终Phase5专项8,361检查，OFF同步首写20/20、隔离安装21/21通过。完整对照、性能、历史核销及边界见 [Phase 5 报告](docs/PHASE5_COMPREHENSIVE_20261013.md)。

2026-10-14：Phase 5.1 离线诊断 713/713 通过（R02 200、音乐/映射 513），UserLibrary 定向回归 283 checks 通过。增补仅涉及测试与文档，production plugin 未改；BuildCommit/BuildID 仍为 `3a582e4` / `0.10.0-dev.6`。范围、证据与限制见 [Phase 5.1 收尾报告](docs/PHASE51_CLOSEOUT_20261014.md) 和 [Cubase 验收清单](docs/PHASE51_CUBASE_ACCEPTANCE.md)。

2026-10-14：Phase 6 `0.10.0-dev.7` 构建身份为 BuildCommit `8c27c27a0c50da7fb0cf991f60ecde66890116f0`。交付形态为一个 Setup、一个 VST3 插件，提供 Legacy、HarmonyFirst、Abstract 三种运行时模式；关闭并重新打开工程后恢复 Legacy。Factory V4 内容未改，Phase 3A 保持 OFF，不提供 Phase6 Candidate 预设、不安装 V5；隔离 V5 的评估 MIDI 和证据另附。工程报告与验收入口见 [Phase 6 综合报告](docs/PHASE6_ENGINEERING_20261014.md) 和 [Cubase 验收清单](docs/PHASE6_CUBASE_ACCEPTANCE.md)。

dev.4 源码交付快照与接手源码 435 个项目文件内容一致；详细核销与来源证据保存在同级 `HarmonyContinuation-validation-20261012/HISTORY_PROVENANCE.md`。dev.4 包保留在 `build-phase4d-delivery/`。新人工清单见 [dev.5 验收](docs/DEV5_CUBASE_ACCEPTANCE.md)。Factory V4 SHA-256 仍为 `74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534`；独立候选及其音乐设计见 [候选说明](data/factory-proposals/20261012/README.md)。

## Phase 6：历史验收与交付对照

Phase 6 交付时分支名为 `development/phase5-20261013`；Phase 6 插件 BuildCommit 为 `8c27c27a0c50da7fb0cf991f60ecde66890116f0`。以 Phase 6 报告记录的构建、测试和最终交付证据核对当前 HEAD 与包身份。路径映射见 [MAP](MAP.md)。

Phase 6 的真实 Cubase 验收尚未完成；R02 Cubase 根因仍 UNKNOWN。请按 [Phase 6 Cubase 验收清单](docs/PHASE6_CUBASE_ACCEPTANCE.md) 集中核对版本/Build ID、Factory、三种模式、R01/R02、拍号、Preview/MIDI、保存重开、User Library 和诊断导出。模拟宿主不代替真实 Cubase。Factory V4 SHA-256 为 `74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534`；V5 不在插件内启用，隔离 MIDI 只作听评。运行时模式重开后为 Legacy，Phase 3A 保持 OFF。最终完整 CTest 34/34、Validator 47/47、隔离安装生命周期 21 checks 通过；Setup SHA 与全部交付证明见综合报告及校验清单。正式发布未授权、未执行。

Phase 5 / 5.1 的人工清单、历史结论、交付与设备接续信息保留在 [Phase 5 报告](docs/PHASE5_COMPREHENSIVE_20261013.md)、[Phase 5 验收](docs/PHASE5_CUBASE_ACCEPTANCE.md)、[Phase 5.1 收尾报告](docs/PHASE51_CLOSEOUT_20261014.md) 和 [Phase 5.1 验收](docs/PHASE51_CUBASE_ACCEPTANCE.md)。Phase 6 换设备与安装步骤见 [RUNBOOK](RUNBOOK.md#phase6-dev7-另一设备接手)。

## 发布与验证证据

正式包：`build-installer/release-0.9.0/HarmonyContinuation-0.9.0-Setup.exe`。构建版本、提交、VST3/Factory/Setup 哈希、依赖与验证范围保存在同目录 `BUILD_INFO.json`、`SHA256SUMS.txt` 与 `FILE_MANIFEST.sha256`。Setup SHA-256：`1ee1ba17a3b689ad7b8ea8f7ffe245c2fc08ac71226ae82eed441788628136f7`。

正式发布原有一次 Release 构建与一次打包通过；对应配置、构建、打包及依赖日志保留在正式目录 `evidence/`。此次清理没有重做发布验证。

复用的历史结果（本轮未重跑）：

- RC3：28 项隔离安装生命周期通过，覆盖组件增删/修复、旧双安装迁移、占用拒绝与卸载，保留个人哨兵、历史库及未管理文件。原始结果为 `evidence/rc3-unified-lifecycle.log`；可复用入口保留在 `tools/test-unified-installer.ps1`。
- RC4：两种模式已知/Uncertain 色条、文字、中英资源、独立提示开关的定向离屏检查通过，原始结果为 `evidence/rc4-color-visibility-smoke-final.log`。
- Library V3：四组兼容桥 406 检查通过，原始结果为 `evidence/LastTest.log`。内容收口校验记录为 629 条、六个 legacy 合并 6/6、Exact Musical Duplicate 0、Missing Metadata 0；旧过程记录可在 Git 历史查阅。
- dev.5–dev.8：色彩计算、展示排序/恢复、候选身份和双语 Why 的定向冒烟曾通过；一次性工具及旧日志已清理，历史范围可从清理前 Git 版本追溯，不宣称当前重新验证。

## 待验收与边界

Phase 6 dev.7 尚无真实 Cubase 验收或人工盲听结果；请以 [Phase 6 Cubase 清单](docs/PHASE6_CUBASE_ACCEPTANCE.md) 记录本机版本、操作和结果。Cubase 核心功能与 RC4 色彩显示的既有用户报告及宿主证据见 [HOST_SPIKE](docs/HOST_SPIKE.md)，不代表 dev.7 已验收。FL Studio Not Verified / Pending；既有 RC 人工清单见 [RC_DAW_ACCEPTANCE](docs/RC_DAW_ACCEPTANCE.md)。

User Schema 2 需要本兼容桥或后续支持版本；v0.8.0 和旧 v0.9.0-dev.1 无法读写它。安装前按 [RISKS](RISKS.md) 定位降级风险并保留用户数据备份。已安装插件、生产 Factory 历史、真实 user.db 和同级旧 checkout 归档不属于本次仓库工作区清理。
