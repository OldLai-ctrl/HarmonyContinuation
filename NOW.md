# 当前接续状态

## 本轮工作与下一步

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
- 正式版为 Factory Library 3；本轮独立开发版本为 Library 4。Factory Schema 2 / User Schema 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2 保持；生产数据与宿主适配未改。
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
