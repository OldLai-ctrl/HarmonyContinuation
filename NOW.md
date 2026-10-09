# 当前接续状态

## 功能基线

- 开发主线：`v0.9/dev`；原基线 `1ecef083a2b49d3c31ec5f13503cf79aed6a63ef` 是 `library/v3` 的祖先，两分支无分叉改动或合并冲突。本次使用显式 merge 保留兼容桥 `33732b3` 和内容收口 `dcda0092688291550495cc132d754935188474a3` 的历史；`library/v3` 分支保留。
- 当前源码版本：`0.9.0-dev.5`，Factory Library 3 / Factory Schema 2 / User Schema 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2。冻结边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。FL Studio HostCompatibilityLayer 和宿主连接入口未改，不修改音乐 Core。
- 主目录与旧版本恢复关系只在 [MAP](MAP.md) 维护。跨设备导航文档已合入本轮改动，不再作为独立未完成任务。

## 当前任务

Color Analysis V1 已实现独立只读分析及两类推荐卡片提示，正在完成一次 Release 编译、三个数学代表案例及一个中英 UI / Why? 冒烟。支持范围、未知结构与完整路径接口见 [Color Analysis V1](docs/COLOR_ANALYSIS_V1.md)。显示色彩提示默认开启，复用编辑器状态在当前插件实例内保留；暂不跨工程或插件重载保存，不升级 Schema。推荐排序、曲库、音乐 Core、宿主入口和 MIDI 协议均冻结。本轮不制作安装器，不启动 dev.6。

Factory V3 已收口为 DEVELOPMENT READY：155 条 canonical 历史记录加 474 条新进行，总计 629 条。内容冻结，本次不重新审计 Catalog 或旧 ID 合并；六条 QUESTIONABLE 不进入生产库。来源见 [V3 数据说明](data/factory-v3/README.md)，统计见 [LIBRARY_V3_SUMMARY](LIBRARY_V3_SUMMARY.md)。

兼容桥已有六个旧 ID 的查询/快照/固定候选恢复、canonical 写入、Library 2/3 共存，以及完整和弦保存和 additive User Schema 2 迁移。本次收口补齐旧重复条目的元数据合并、整个生产库统一音乐指纹去重和简短统计；原 `data/factory/` 的 161 条数据未改。六个旧 ID 的 Style、Intent、Technique、Aliases、Search Tags、中英文名称全部保留，`ROCK_001` 的 Rock / Loop 合入 `COMMON_MAJOR_020`，canonical 主意图保持不变。

三个旧 checkout 已整体移入同级归档，保留 Git 历史、未提交文档及构建产物；主开发目录为独立仓库，已安装插件、Factory 历史和生产 user.db 未移动、未覆盖。

`library/v3` 已推送到 `dcda009`，其干净提交的 `0.9.0-dev.3` 开发包及 SHA-256 已核验，记录位于 `build-installer/v3-final-output/BUILD_INFO.json` 和 `SHA256SUMS.txt`。本次仅将它集成到 `v0.9/dev` 并统一开发版本；不修改 main、不移动 v0.8.0 tag、不创建正式 Release Tag、不重新制作安装器。

合并提交为 `32a0d64a71d34cca446b2e05a9cb2b5e32fa971d`，两个父提交分别为原 `v0.9/dev` 基线及 `dcda009`。该干净提交的一次 MSVC Release 编译及两个最小冒烟均通过；可进入 Color Analysis V1。该记录为 dev.4 历史证据；当前进行 dev.5 附加色彩功能。合并与本条证据记录一同推送至 `v0.9/dev`，最终同步状态以 Git 的分支和 tracking ref 为准。

## 证据

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

Factory V3 保持约 629 条及 DEVELOPMENT READY 状态，不继续批量扩库；六条 QUESTIONABLE 继续留在非生产清单。开发主线集成只以本轮指定的最小验证为完成关口。

User Schema 2 需要本桥接构建或后续支持版本；v0.8.0 和旧 v0.9.0-dev.1 无法读写它。安装前按 [RISKS](RISKS.md) 定位降级风险，保留用户数据备份；这是版本边界，不是本轮执行阻塞。

人工逐条试听和真实 Cubase/FL 验收仍未执行。用户本次明确全量测试统一推迟到 v1.0 正式发布前，v0.9 正式版也不执行全量回归；该规则取代此前 v0.9 最终收口安排。本轮不跑全 CTest、Validator、基准、宿主矩阵、MIDI/迁移全套、性能或安装回归。最小验证及推送完成后停止。
