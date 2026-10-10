# 当前接续状态

## 功能基线

- 开发主线：`v0.9/dev`；原基线 `1ecef083a2b49d3c31ec5f13503cf79aed6a63ef` 是 `library/v3` 的祖先，两分支无分叉改动或合并冲突。本次使用显式 merge 保留兼容桥 `33732b3` 和内容收口 `dcda0092688291550495cc132d754935188474a3` 的历史；`library/v3` 分支保留。
- 当前源码版本：`0.9.0-rc.2`，Factory Library 3 / Factory Schema 2 / User Schema 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2。冻结边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。FL Studio HostCompatibilityLayer 和宿主连接入口未改，不修改音乐 Core。
- 主目录与旧版本恢复关系只在 [MAP](MAP.md) 维护。跨设备导航文档已合入本轮改动，不再作为独立未完成任务。

## 当前任务

RC2 定向修复：用户确认 Cubase Pro 15 的 RC1 加载、自动推荐、排序切换、Why?、Preview、MIDI 导出和 Continuation 提示通过；Enrichment 是显示灰色「不足以判断」，并非渲染缺失。截图 Dmin → C → Bb → A 的末和弦 OPEN。原因是整段汇总要求完整时值，Enrichment 保留 OPEN，UI 却同时隐藏了可靠静态指标；Continuation 具有候选建议时值。修复仅在 Enrichment 展示层保留可靠逐和弦静态信息，并明确整段信息不足及原因，不补时值、不改变分析与排序资格。最小定向验证和 RC2 打包待记录；真实 Cubase 定向复验 Pending，FL 无新增证据。RC1 产物保留，完成后停止等待用户复验。

Factory V3 已收口为 DEVELOPMENT READY：155 条 canonical 历史记录加 474 条新进行，总计 629 条。内容冻结，本次不重新审计 Catalog 或旧 ID 合并；六条 QUESTIONABLE 不进入生产库。来源见 [V3 数据说明](data/factory-v3/README.md)，统计见 [LIBRARY_V3_SUMMARY](LIBRARY_V3_SUMMARY.md)。

兼容桥已有六个旧 ID 的查询/快照/固定候选恢复、canonical 写入、Library 2/3 共存，以及完整和弦保存和 additive User Schema 2 迁移。本次收口补齐旧重复条目的元数据合并、整个生产库统一音乐指纹去重和简短统计；原 `data/factory/` 的 161 条数据未改。六个旧 ID 的 Style、Intent、Technique、Aliases、Search Tags、中英文名称全部保留，`ROCK_001` 的 Rock / Loop 合入 `COMMON_MAJOR_020`，canonical 主意图保持不变。

三个旧 checkout 已整体移入同级归档，保留 Git 历史、未提交文档及构建产物；主开发目录为独立仓库，已安装插件、Factory 历史和生产 user.db 未移动、未覆盖。

`library/v3` 已推送到 `dcda009`，其干净提交的 `0.9.0-dev.3` 开发包及 SHA-256 已核验，记录位于 `build-installer/v3-final-output/BUILD_INFO.json` 和 `SHA256SUMS.txt`。本次仅将它集成到 `v0.9/dev` 并统一开发版本；不修改 main、不移动 v0.8.0 tag、不创建正式 Release Tag、不重新制作安装器。

合并提交为 `32a0d64a71d34cca446b2e05a9cb2b5e32fa971d`，两个父提交分别为原 `v0.9/dev` 基线及 `dcda009`。该干净提交的一次 MSVC Release 编译及两个最小冒烟均通过；可进入 Color Analysis V1。该记录为 dev.4 历史证据；dev.5 附加色彩功能已完成，当前只增加 dev.6 展示排序。合并与本条证据记录一同推送至 `v0.9/dev`，最终同步状态以 Git 的分支和 tracking ref 为准。

## 证据

2026-10-09 rc.1：从干净提交 `da1fd6d08ab20771f8696944bdaa4b027e4ca7b9` 一次 MSVC Release 构建插件及 library_manager 通过，沿用 `_CL_=/Z7`，Validator 关闭；产物 `0.9.0-rc.1 / Release / da1fd6d`，VST3 moduleinfo 与完整 Setup ProductVersion 一致。后续证据提交仅改本文档，不重编生产代码。

一次调用现有 build-installer.ps1 生成完整 Setup 和其所需的独立 Library 3 更新包。包暂存插件/语言资源/Factory 与构建源哈希一致，库元数据为 `library_version=3 schema=2 progressions=629`，MSVC 导入依赖已随附（其余为 Windows 10+ 系统组件）；暂存仅插件、图标/元数据、项目双语资源、Factory DB、更新工具及 CRT，不含购买的 PDF/Excel/付费转换数据。安装/卸载路径与数据保留逻辑未改，复用既有九项隔离安装证据，不执行新安装生命周期。

完整包 `HarmonyContinuation-0.9.0-rc.1-Setup.exe` SHA-256：`5a8fbe9a1842b297b65420077239fb3ec569c9227a1b6faaa43aec0861b6b751`；独立库 `HarmonyContinuation-Library-3-Setup.exe`：`b558ed64090ef324d710f4ac4d249f20959c6184b272201b243da924b91c9740`。两项均重算匹配 SHA256SUMS.txt。原始证据为 `build-v3-plugin/rc1-release-configure.log`、`rc1-release-build.log`、`build-installer/rc1-package-build.log`、`rc1-dependencies.log`；RC 目录附 `BUILD_INFO.json`、`FILE_MANIFEST.sha256`、README、版本说明及人工清单。构建/包/日志不随 Git 推送，须单独分发。

本轮未发现安装准备、编译或打包阻断；没有实际 DAW 通过证据，不能据此正式发布。偏好实例内保留、详情前八位置、Unknown/Uncertain 和最终听感/输出限制沿用 dev.8。复用 dev.5–dev.8 证据，不运行测试程序、CTest、Validator、42/30 基准、MIDI、宿主矩阵、性能或安装回归。全量回归仍留到 v1.0 正式发布前。

2026-10-09 dev.8：生产提交 `825b8fc9325154b7fd14399664e8338a47bddad1` 的干净源码一次 MSVC Release 编译通过，目标仅 `HarmonyContinuation` 和 `WhyV2Smoke`，Validator 关闭，沿用 `_CL_=/Z7`。产物 `0.9.0-dev.8 / Release / 825b8fc`，moduleinfo 版本一致；后续仅提交本证据，生产源码未改。直接运行一次 WhyV2Smoke，Continuation 与 Enrichment 两个场景均通过：完整卡片/默认三句 Why?/高级详情的离屏绘制，中英资源，Unknown/Uncertain 中性文案与缺失低音原因，提示与排序独立，关闭恢复原索引，实例内偏好恢复；排序后卡片/Why/Preview/MIDI payload 指向同一原始候选，设置未触发重生成回调。日志为 `build-v3-plugin/dev8-why-configure.log`、`dev8-why-build.log`、`dev8-why-smoke.log`。

本轮改动仅 UI、本地化、版本/构建入口和文档；HarmonyColorAnalyzer、ColorPreferenceReranker、候选生成/评分、Matcher、VoiceLeading、旋律约束、Factory/User 库、FULL/SKELETON、档位预算、MIDI 和宿主代码未改，Schema 保持 Factory 2 / User 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2。复用 dev.5–dev.7 算法证据，未重跑旧测试、CTest、Validator、42/30 基准、宿主矩阵、MIDI、安装器或性能回归，未制作安装包或 Tag。验证通过后停止，不进入 v0.9 发布阶段。

限制：偏好仍仅当前实例保存；缺失时值、低音及模型支持范围继续限制色彩结论。短卡片不足以容纳双标签时省略次标签，完整路径及细节可在 Why? 查看。高级色彩数字只列前八位置并明示截断。离屏绘制与模拟 MIDI payload 身份不替代实际导出、缩放矩阵或真实 DAW 验收；真实 DAW 保持 Pending。功能及证据提交一同推送 `v0.9/dev`，同步状态以 tracking ref 为准。

2026-10-09 dev.7：生产提交 `fe6be3783b4246cb1d97ba0ef9af2b8c6f9f29c4` 的干净源码一次 MSVC Release 编译通过，目标仅插件和 `EnrichmentColorSmoke`，Validator 关闭，沿用 `_CL_=/Z7`。插件产物为 `0.9.0-dev.7 / Release / fe6be37`，moduleinfo 版本一致。后续仅调整测试 fixture 和验证记录，未改生产代码、未重编整个项目。

合并冒烟覆盖一个完整 Enrichment 张力弧目标、关闭恢复及重排后卡片/Why/Preview/MIDI payload/比较/快照身份；首次四位置 fixture 未换位，保留生产门槛，调整同一示例后身份与恢复通过。随后修正 fixture 的原进行和真实两次扩展音操作对应关系，其声部评价损失超过 0.02，正确保留原位；扩展该单一示例的共同位置以满足同一声部质量门槛后，只使用 `--target-only` 重验目标，排序及显式转位/缺失低音断言通过。没有放宽生产资格或改变色彩权重，已通过的身份与恢复项不重复运行。证据为 `build-v3-plugin/dev7-color-configure.log`、`dev7-color-build.log`、`dev7-color-smoke.log`、`dev7-color-smoke-fix.log`、`dev7-color-target-final.log`；定向工具增量编译日志为 `dev7-color-smoke-build-fix.log`。身份检查使用模拟 payload 观察候选对象，不代表实际 MIDI 导出或 DAW 听感验收。

复用 dev.5/dev.6 证据，未跑 CTest、Validator、音乐基准、完整 Host Harness、MIDI 套件、安装或性能测试，未制作安装包。所有要求通过后停止，不启动 dev.8。限制：原时值不完整、低音缺失、模型 Unknown/Uncertain 或声部标签与明确低音不一致时不参与重排；质量窗口保守可能不换位，暖收束还需原功能分析支持。偏好仍仅实例内保存；最终 MIDI 配音与真实 DAW 听感未验收。Factory V3、生成/评分/音乐预算、宿主/MIDI 及全部 Schema 未改。功能及后续工具/证据提交一同推送 `v0.9/dev`，同步状态以 tracking ref 为准。

2026-10-09 dev.6：生产功能提交 `0ab87b8d6a50357bfd522705797d78a008cd11dd` 的干净源码一次 MSVC Release 编译通过（沿用 `_CL_=/Z7`），目标仅插件和 `ColorPreferenceSmoke`，Validator 关闭。产物版本 `0.9.0-dev.6 / Release / 0ab87b8`，moduleinfo 版本一致；后续仅提交此验证记录，未更改生产源码。直接运行一次最小冒烟：关闭时原候选身份/顺序保持、一个完整张力弧目标在同质量窗口前移且低质量候选不越级、Unknown/Uncertain 固定位置、短前段自动模式回退、关闭恢复原顺序，全部通过。同一代表场景离屏绘制工具栏/卡片/Why? 中英文字，并验证提示开关独立、实例内偏好恢复、原索引选择不变和未发送重生成回调。证据在 `build-v3-plugin/dev6-color-configure.log`、`dev6-color-build.log`、`dev6-color-smoke.log`。复用 dev.5 分析证据，没有重跑旧检查、CTest、Validator、音乐基准、宿主矩阵、MIDI、安装或性能测试，未制作安装包。通过后停止，不启动 dev.7。

当前限制：只重排原展示策略已接受的集合；三位置/五分质量带/三分分差及原质量子项门槛较保守，可能保持原次序。分析器的 Unknown/Uncertain 和缺失时值继续限制可排序范围；自动模式需要三个可靠前段位置。设置仅当前插件实例保存，真实 DAW 显示未实测。Enrichment、Factory V3、全部 Schema 及音乐生成路径未改。本次指定的代表排序目标为张力弧，没有扩展成五预设测试矩阵。实现和本文档证据提交一同推送 `v0.9/dev`，最终同步状态以 tracking ref 为准。

2026-10-09 dev.5：功能提交 `3a4c3268e0f8386ae3328aa5f19e466dd9f340a1`（以 Git 完整 hash 为准）的只读色彩模块、卡片及 Why? 已完成。一次 MSVC Release 构建首次在新冒烟工具遇到 Windows `near` 宏冲突和缺少 COM 头文件；仅修复该工具并增量完成剩余目标，未重跑整个项目。修复提交 `e0b8a818dba72ff11146208c3ae3c36662c71d77` 只改冒烟工具。插件版本 `0.9.0-dev.5 / Release`，生成 Git 标识 `3a4c326`，对应生产功能提交且不是 dirty/unknown；后续测试修复未改生产源码。插件及直接运行的 `HarmonyColorSmoke` 成功；三个案例覆盖普通和弦、转位/时值/完整路径接缝、角度跨界/多方向，另一个离屏 UI / Why? 冒烟覆盖中英、隐藏提示、推荐身份及功能解释保持、实例内开关恢复，全部通过。日志为 `build-v3-plugin/dev5-color-configure.log`、`dev5-color-build.log`、`dev5-color-build-finish.log`、`dev5-color-smoke.log`。未运行任何历史套件、Validator、音乐基准、宿主矩阵或安装器测试，未打包安装器；通过后停止测试。

限制：V1 数学子集及阈值见专题文档；宽跨度、未覆盖等级/音集返回 Unknown，多方向返回 Uncertain；缺失时值不生成整段结论。开关不跨插件/工程重载保存。真实 DAW 显示未验收。本轮未改推荐排序、权重、Factory V3、用户库、宿主入口、MIDI 或任何 Schema。dev.6 可使用只读完整路径接口，但本轮不开始该阶段。

2026-10-09 主线集成：`0.9.0-dev.4 / 32a0d64 / Release` 一次编译通过，复用本机 `_CL_=/Z7` 构建方式，Validator 关闭。直接运行 `FactoryCatalogTests --smoke-one`，仅 `V3_DUSK_001` 加载→推荐→MIDI 通过；`LibraryCompatibilityTests user-smoke` 仅一个隔离 Schema 1 用户库恢复案例通过，名称、备注、标签、收藏及进行条数保留，未触发迁移、未访问生产 user.db。宿主连接入口未改，Generic Host 检查跳过。未重审 629 条内容或六个旧 ID 合并，未运行历史测试套件、基准或安装检查，未制作 dev.4 安装器。原始结果为 `build-v3-plugin/v09-integration-build.log`、`v09-integration-factory-smoke.log`、`v09-integration-user-smoke.log`。通过后停止测试。

以下为已完成 Library V3 的历史证据，本次集成不重复执行。

2026-10-09 精简收口内容校验执行一次，通过 Catalog 严格解析、合法和弦、DB 读回、全生产库去重及源元数据逐条保留检查：629 条、474 条新增、六个 legacy 合并 6/6、Exact Musical Duplicate 0、Missing Metadata 0。仅运行两条消费者冒烟 `V3_DUSK_001`（新增小调）和 `V3_OPEN_001`（新增 Develop），均通过加载→推荐→MIDI；低音/转位/扩展音复用兼容桥 18 类通过证据，未重跑。原始结果为 `build-v3-plugin/v3-closeout-content.log`、`v3-closeout-smoke.log`。冻结覆盖统计见 [LIBRARY_V3_SUMMARY](LIBRARY_V3_SUMMARY.md)。

2026-10-09 本目录 `0.9.0-dev.3 / 33732b3-dirty` 的集中内容验收：插件、Demo、MIDI CLI 与 FactoryCatalogTests 的 MSVC Release 构建完成。生产编译器执行格式、元数据、音乐重复和 DB 读回校验；唯一运行的 `FactoryV3Catalog` 测试通过，覆盖全部 474 条新增进行的试听结构、音符范围/时值及 MIDI 字节生成，并核对 Library 2 的 161 个引用与音乐内容。原始结果在 `build-v3-plugin/catalog-build.log`、`catalog-test.log` 和 `Testing/Temporary/LastTest.log`。步骤见 [RUNBOOK](RUNBOOK.md)。

首轮构建在插件 `/Zi` 编译处遇到 MSVC C1902，测试未启动；改用本机 `_CL_=/Z7` 保留对象内调试信息后增量完成构建。没有重跑已通过的测试，没有改变 SDK 或安装工具链。这是本机构建方式，不是产品 schema 改动。

兼容桥基线证据仍为 2026-10-08 的四组 406 检查（resolver 30、snapshot 75、user 265、factory 36），保存在 `build-v3/Testing/Temporary/LastTest.log`；本轮未重跑这四组。下面两项迁移结论属于该基线：

- 18 类用户和弦保存、关闭、重开后，完整内容/元数据、试听音高结构和 MIDI 字节一致。旧 Schema 1 fixture 的 raw payload、名称/标签/风格/意图/收藏/备注/时间戳不因迁移改变。
- 注入迁移失败及 typed data 写失败，确认事务回滚且无部分行；一次性备份保留且不重复生成。未操作真实 user.db。
- 当前生产 V3 DB 为 `library_version=3 schema=2 progressions=629`；历史 V2 DB 保持 `library_version=2 schema=1 progressions=161`。兼容桥的 155 条 canonical fixture 不因扩库重写。
- 未跑全 CTest、42 Continuation、30 Enrichment、FL Host Harness 或 Validator，未做 DAW 验收。历史宿主报告不替代本轮真机验收。

2026-10-09 本目录后续打包阶段：编译 `library_manager`，复用上述插件/DB 产物，生成完整 `HarmonyContinuation-0.9.0-dev.3-Setup.exe` 与独立 `HarmonyContinuation-Library-3-Setup.exe`，均附 SHA-256 文件，位于 `build-installer/v3-output/`。本机从官方发布下载并核验 Pyrsys 签名，以 portable 模式准备 Inno Setup 6.7.3；运行库来自本机 MSVC x64 可分发 CRT。编译器与 CRT 不进入 Git。

仅完成一次隔离完整包安装→卸载生命周期，9 项检查通过：实际插件字节与打包暂存一致，独立活动库为 Library 3 / Schema 2 / 629 条；安装和卸载保留旧 Library 2、Library 3、活动指针及个人数据哨兵文件。使用独立 AppId、项目内路径、无安装注册项/快捷方式的测试包；未读取、迁移或修改真实 user.db。独立库 Setup 已编译，本轮未额外运行其安装流程。此前扩库与兼容桥测试不重复运行。日志为 `build-installer/v3-package-build.log`、`v3-smoke/result.txt`、`v3-smoke/install.log`、`v3-smoke/uninstall.log`。

## 开发完成边界

Factory V3 保持约 629 条及 DEVELOPMENT READY 状态，不继续批量扩库；六条 QUESTIONABLE 继续留在非生产清单。rc.1 只以本轮指定的构建/打包文件检查为完成关口。

User Schema 2 需要本桥接构建或后续支持版本；v0.8.0 和旧 v0.9.0-dev.1 无法读写它。安装前按 [RISKS](RISKS.md) 定位降级风险，保留用户数据备份；这是版本边界，不是本轮执行阻塞。

人工逐条试听和真实 Cubase/FL 验收仍未执行。用户本次明确全量测试统一推迟到 v1.0 正式发布前，v0.9 正式版也不执行全量回归；该规则取代此前 v0.9 最终收口安排。本轮不跑全 CTest、Validator、基准、宿主矩阵、MIDI/迁移全套、性能或安装回归。RC 打包及推送完成后停止，等待人工 DAW 结果。
