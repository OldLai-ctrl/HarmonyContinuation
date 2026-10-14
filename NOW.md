# 当前接续状态

## 已完成阶段

Phase 3 系统审计、Phase 3A 受控减和弦插入实验、Phase 4A 决策分析及 Phase 4B 节奏/和声检索审计均已形成报告。Phase 3A 补偿保持默认关闭；相关历史范围和证据边界见各阶段报告。

## 功能基线

- 当前正式版本：`0.9.0`；发布提交与 `v0.9.0` Tag 均为 `ddc4f89babd4520daac3eae74824d6f581ce0e75`。源码清理提交不改变该正式包的构建身份。
- 正式版为 Factory Library 3；本轮独立开发版本为 Library 4。Factory Schema 2 / User Schema 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2 保持；生产数据与宿主适配未改。
- Factory V3 为 155 条 canonical 历史记录加 474 条新进行，共 629 条；六条 QUESTIONABLE 留在非生产清单。统计见 [LIBRARY_V3_SUMMARY](LIBRARY_V3_SUMMARY.md)，兼容与降级边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。

## 当前接续状态

2026-10-13：Phase 5 / dev.6 代码构建提交 `3a582e4`。新增默认关闭的 Abstract 方向性扩展和弦匹配，改善真实 Factory V4_B11 的简单输入召回；修复 UserLibrary 并发首次初始化，保持 Factory 确定性编译。默认 OFF、HF-ON、Abstract-ON 三模式各 CTest 33/33、Validator 47/47、V4 专项13,048、隔离候选专项41,105通过；原1440矩阵哈希不变。最终Phase5专项8,361检查，OFF同步首写20/20、隔离安装21/21通过。完整对照、性能、历史核销及边界见 [Phase 5 报告](docs/PHASE5_COMPREHENSIVE_20261013.md)。

2026-10-14：Phase 5.1 离线诊断 713/713 通过（R02 200、音乐/映射 513），UserLibrary 定向回归 283 checks 通过。增补仅涉及测试与文档，production plugin 未改；BuildCommit/BuildID 仍为 `3a582e4` / `0.10.0-dev.6`。范围、证据与限制见 [Phase 5.1 收尾报告](docs/PHASE51_CLOSEOUT_20261014.md) 和 [Cubase 验收清单](docs/PHASE51_CUBASE_ACCEPTANCE.md)。

dev.4 源码交付快照与接手源码 435 个项目文件内容一致；详细核销与来源证据保存在同级 `HarmonyContinuation-validation-20261012/HISTORY_PROVENANCE.md`。dev.4 包保留在 `build-phase4d-delivery/`。新人工清单见 [dev.5 验收](docs/DEV5_CUBASE_ACCEPTANCE.md)。Factory V4 SHA-256 仍为 `74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534`；独立候选及其音乐设计见 [候选说明](data/factory-proposals/20261012/README.md)。

## 下一步：在另一台设备继续

当前安全开发分支为 `development/phase5-20261013`，插件构建提交为 `3a582e4`。Phase 5.1 接续增加测试与文档，未改变 production plugin；本轮测试/文档提交身份与远端读回见增补交付的 Git proof。Phase 5 交付目录为同级 `HarmonyContinuation-phase5-delivery-20261013`，原始日志为 `HarmonyContinuation-phase5-validation-20261013`；Phase 5.1 验证与唯一增补包目录分别为同级 `HarmonyContinuation-phase51-validation-20261014`、`HarmonyContinuation-phase51-delivery-20261014`。独立候选仍为 `HarmonyContinuation-candidates-20261012`，dev.5 冻结包及历史分支保留。路径关系见 [MAP](MAP.md)。

下一步按 [Phase 5.1 Cubase 验收清单](docs/PHASE51_CUBASE_ACCEPTANCE.md) 由用户执行真实宿主与试听验收；R02 的 Cubase 根因仍 UNKNOWN。只有 DEFAULT-OFF、HF-ON、ABSTRACT-ON 三种现有包列入本轮验收，其余 flag 组合仍属 core 实验。HF/P3/Abstract 默认均 OFF；V5 候选继续隔离，不进入 Factory V4。正式发布未授权且未执行。Phase 5 原人工清单与历史结论保留在 [Phase 5 验收](docs/PHASE5_CUBASE_ACCEPTANCE.md) 及 [Phase 5 报告](docs/PHASE5_COMPREHENSIVE_20261013.md)。

本机 ON 安装未完成：便携包91项、旧bundle17项备份均核验通过，但Windows UAC返回“操作已被用户取消”，实际插件及独立Factory未改变。备份、暂存、安装脚本和回退说明保存在同级 `HarmonyContinuation-on-install-20261012/`，不随Git同步。另一设备不能假定ON已安装，应按dev.5清单选择一个模式安装并核实About的Build ID/HF/P3。

dev.5历史交接包已在既有GitHub未发布草稿，Phase5交付状态以 [MAP](MAP.md#同步) 和本轮交付proof为准。换设备步骤见 [RUNBOOK](RUNBOOK.md#phase5-dev6-另一设备接手)，可复制的接续说明见 [交付词](docs/PHASE5_HANDOFF_PROMPT.md)。

## 发布与验证证据

正式包：`build-installer/release-0.9.0/HarmonyContinuation-0.9.0-Setup.exe`。构建版本、提交、VST3/Factory/Setup 哈希、依赖与验证范围保存在同目录 `BUILD_INFO.json`、`SHA256SUMS.txt` 与 `FILE_MANIFEST.sha256`。Setup SHA-256：`1ee1ba17a3b689ad7b8ea8f7ffe245c2fc08ac71226ae82eed441788628136f7`。

正式发布原有一次 Release 构建与一次打包通过；对应配置、构建、打包及依赖日志保留在正式目录 `evidence/`。此次清理没有重做发布验证。

复用的历史结果（本轮未重跑）：

- RC3：28 项隔离安装生命周期通过，覆盖组件增删/修复、旧双安装迁移、占用拒绝与卸载，保留个人哨兵、历史库及未管理文件。原始结果为 `evidence/rc3-unified-lifecycle.log`；可复用入口保留在 `tools/test-unified-installer.ps1`。
- RC4：两种模式已知/Uncertain 色条、文字、中英资源、独立提示开关的定向离屏检查通过，原始结果为 `evidence/rc4-color-visibility-smoke-final.log`。
- Library V3：四组兼容桥 406 检查通过，原始结果为 `evidence/LastTest.log`。内容收口校验记录为 629 条、六个 legacy 合并 6/6、Exact Musical Duplicate 0、Missing Metadata 0；旧过程记录可在 Git 历史查阅。
- dev.5–dev.8：色彩计算、展示排序/恢复、候选身份和双语 Why 的定向冒烟曾通过；一次性工具及旧日志已清理，历史范围可从清理前 Git 版本追溯，不宣称当前重新验证。

## 待验收与边界

Cubase 核心功能与 RC4 色彩显示由用户报告通过，正式构建未重新实机测试；宿主证据见 [HOST_SPIKE](docs/HOST_SPIKE.md)。FL Studio Not Verified / Pending。人工逐条试听未执行；全量回归按既有约定留到 v1.0 正式发布前。现行人工清单见 [RC_DAW_ACCEPTANCE](docs/RC_DAW_ACCEPTANCE.md)。

User Schema 2 需要本兼容桥或后续支持版本；v0.8.0 和旧 v0.9.0-dev.1 无法读写它。安装前按 [RISKS](RISKS.md) 定位降级风险并保留用户数据备份。已安装插件、生产 Factory 历史、真实 user.db 和同级旧 checkout 归档不属于本次仓库工作区清理。
