# 当前接续状态

## 功能基线

- 当前正式版本：`0.9.0`；发布提交与 `v0.9.0` Tag 均为 `ddc4f89babd4520daac3eae74824d6f581ce0e75`。源码清理提交不改变该正式包的构建身份。
- 正式版为 Factory Library 3；本轮独立开发版本为 Library 4。Factory Schema 2 / User Schema 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2 保持；生产数据与宿主适配未改。
- Factory V3 为 155 条 canonical 历史记录加 474 条新进行，共 629 条；六条 QUESTIONABLE 留在非生产清单。统计见 [LIBRARY_V3_SUMMARY](LIBRARY_V3_SUMMARY.md)，兼容与降级边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。

## 当前任务

2026-10-10：`library/v4` 正在完成 **0.10.0-dev.2 核心推荐可靠性修复**。先前独立的安装器修复已提交并推送（提交 `533eb46`），安装器-only 包 `build-installer/v4-0.10.0-dev.2` 保留，不覆盖。本轮只在独立核心修复包目录重建，并继续保护 `main`、正式标签及用户数据。

根因已在定向跨版本矩阵中复现：dev.1 的五组普通输入继续发展数量与 v0.9.0 接近，但 Polish/Rich/Advanced 全为零；部分 Cubase XML 式精确音集合让整组和弦被过度保护，且基础 Polish 不成功会提前挡住 Rich/Advanced。短输入自动调性只看前三个假设，slash 和弦也可能未正确还原指定低音。本轮逐项修复操作级保护、短前缀调性覆盖、slash 解析与 MIDI 音集读取。

定向自动检查已通过：11 个相关测试目标全部通过；新的 1440 组合矩阵（15 组输入、8 种节奏、直接事件/实际 MIDI 文件导入/Cubase VST XML 解析夹具）无失败。FULL/SKELETON 离屏界面专项通过；V4 28 条可发现性为 26 条默认显示、2 条位于 More、0 条受阻，B09 仍可从 More 访问。v0.9.0、dev.1、修复代码对照显示普通输入升级候选恢复，五组 Polish 各 1 条，Rich 各 3 条；Factory 仍为 657 条，数据库及用户相关 Schema 未改。测试不是 Cubase 真机验收，人工验收仍待用户进行。

下一步：补齐独立证据包，重新构建并验证 0.10.0-dev.2，提交并推送 `library/v4`。新安装包路径与 SHA-256、嵌入 Build ID 在构建后补记。全量 CTest、Validator、DAW 矩阵及无关音乐基准均未运行；不合并 `main`、不创建正式 Tag。细节见 [核心可靠性修复报告](docs/DEV2_CORE_RELIABILITY_FIX.md)、[升级修复证据](docs/INSTALLER_DEV2_UPGRADE_FIX.md) 与 [V4 人工验收清单](docs/V4_DAW_ACCEPTANCE.md)。
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
