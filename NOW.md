# 当前接续状态

## 已完成阶段

Phase 3 系统审计、Phase 3A 受控减和弦插入实验、Phase 4A 决策分析及 Phase 4B 节奏/和声检索审计均已形成报告。Phase 3A 补偿保持默认关闭；相关历史范围和证据边界见各阶段报告。

## 功能基线

- 当前正式版本：`0.9.0`；发布提交与 `v0.9.0` Tag 均为 `ddc4f89babd4520daac3eae74824d6f581ce0e75`。源码清理提交不改变该正式包的构建身份。
- 正式版为 Factory Library 3；本轮独立开发版本为 Library 4。Factory Schema 2 / User Schema 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2 保持；生产数据与宿主适配未改。
- Factory V3 为 155 条 canonical 历史记录加 474 条新进行，共 629 条；六条 QUESTIONABLE 留在非生产清单。统计见 [LIBRARY_V3_SUMMARY](LIBRARY_V3_SUMMARY.md)，兼容与降级边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。

## 当前接续状态

2026-10-16：用户已选择 Phase 7 方案A，并指出名称、相对时值和输入匹配位置缺失；本轮完成A2卡片修订。首次源码基线为 `66d7f0713b31f0e5180abf97d14d5a2b9dfc51ac`，首版设计提交 `913b7ac`；工作树和隔离分支见 [MAP](MAP.md)。原开发目录的并行改动保留原样。

- UI 审计、功能迁移、创作流程、视觉交互规范、BPM 可行性和正式工作包：[Phase 7 设计报告](design/phase7/README.md)。
- [直接打开A2原型](design/phase7/index.html)，或查看报告中的19张PNG。新增来源进行名、完整模板定位、输入对应序号、替代/未覆盖标记、QN比例时值与播放头；真实B08和R02中段样例可切换。原型不连接宿主或真实用户曲库。
- 本轮实际验证：18组窗口/缩放检查通过；4:8:16 QN块宽为1:2:4、中段匹配/未覆盖标记正确、来源未覆盖和弦不加入实际片段、仅续写播放头从边界开始；原有交互与MIDI下载检查通过。结果见 [verification.json](design/phase7/verification.json)，仍属浏览器离屏证据。
- 插件仍为既有 `0.10.0-dev.7` / BuildCommit `8c27c27a0c50da7fb0cf991f60ecde66890116f0`；核心算法、Factory V4、User Library、Schema 和正式 UI 未改。本轮没有构建、安装、发布或真实 Cubase 验收。
- 独立试听 BPM 可局部实现；建议先停止改速再重播。真正暂停、无缝变速、多实例并行发声仍需额外设计，详见报告。

布局方向已确认A，决定见 [D09](DECISIONS.md#d09-phase7采用a并保留音乐判断信息)。本轮处理用户指出的卡片设计缺口；正式VSTGUI下一阶段按完善后的音乐结构信息实施，不再要求重选A/B。

本轮设计成果同步至设计分支及 `development/phase5-20261013`；实际结果以最终 Git 远端读回为准。原 `HarmonyContinuation-release` 的本地分支和未提交文件不强制更新，接续使用设计工作树或从远端核对后再合并本地文档。

## 历史工程摘要

Phase 5 的源码构建身份为 `3a582e4` / dev.6；完整验证、交付与历史核销见 [Phase 5 报告](docs/PHASE5_COMPREHENSIVE_20261013.md)。

2026-10-14：Phase 5.1 离线诊断 713/713 通过（R02 200、音乐/映射 513），UserLibrary 定向回归 283 checks 通过。增补仅涉及测试与文档，production plugin 未改；BuildCommit/BuildID 仍为 `3a582e4` / `0.10.0-dev.6`。范围、证据与限制见 [Phase 5.1 收尾报告](docs/PHASE51_CLOSEOUT_20261014.md) 和 [Cubase 验收清单](docs/PHASE51_CUBASE_ACCEPTANCE.md)。

2026-10-14：Phase 6 `0.10.0-dev.7` 构建身份为 BuildCommit `8c27c27a0c50da7fb0cf991f60ecde66890116f0`。交付形态为一个 Setup、一个 VST3 插件，提供 Legacy、HarmonyFirst、Abstract 三种运行时模式；关闭并重新打开工程后恢复 Legacy。Factory V4 内容未改，Phase 3A 保持 OFF，不提供 Phase6 Candidate 预设、不安装 V5；隔离 V5 的评估 MIDI 和证据另附。工程报告与验收入口见 [Phase 6 综合报告](docs/PHASE6_ENGINEERING_20261014.md) 和 [Cubase 验收清单](docs/PHASE6_CUBASE_ACCEPTANCE.md)。

dev.4 源码交付快照与接手源码 435 个项目文件内容一致；详细核销与来源证据保存在同级 `HarmonyContinuation-validation-20261012/HISTORY_PROVENANCE.md`。dev.4 包保留在 `build-phase4d-delivery/`。新人工清单见 [dev.5 验收](docs/DEV5_CUBASE_ACCEPTANCE.md)。Factory V4 SHA-256 仍为 `74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534`；独立候选及其音乐设计见 [候选说明](data/factory-proposals/20261012/README.md)。

## Phase 6：当前验收与交付状态

Phase 6 已交付源码的开发分支名为 `development/phase5-20261013`；插件 BuildCommit 为 `8c27c27a0c50da7fb0cf991f60ecde66890116f0`。当前工作树、分支与构建身份分别核实，路径映射见 [MAP](MAP.md)。

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
