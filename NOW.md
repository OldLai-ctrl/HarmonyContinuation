# 当前接续状态

## 功能基线

- 当前正式版本：`0.9.0`；发布提交与 `v0.9.0` Tag 均为 `ddc4f89babd4520daac3eae74824d6f581ce0e75`。源码清理提交不改变该正式包的构建身份。
- 正式版为 Factory Library 3；本轮独立开发版本为 Library 4。Factory Schema 2 / User Schema 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2 保持；生产数据与宿主适配未改。
- Factory V3 为 155 条 canonical 历史记录加 474 条新进行，共 629 条；六条 QUESTIONABLE 留在非生产清单。统计见 [LIBRARY_V3_SUMMARY](LIBRARY_V3_SUMMARY.md)，兼容与降级边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。

## 当前任务

2026-10-10：`library/v4` 最新候选为 **0.10.0-dev.2**，本轮只修复真实旧版升级被 bundled Factory 识别规则误拦截的问题。Factory V4 仍为 657 条（629+28）；音乐算法、Factory 源内容与 Schema 未变。

官方完整内容与可信 RC 哈希可识别；移除旧 bundled 副本前创建永久备份并核对 SHA-256。未知或修改过的文件仍阻止安装，并显示中英文路径、版本、哈希、原因与安全处理建议。V4 不允许搭配旧 0.9.0 引擎进行数据单独升级；组件取消勾选保持原状，管理记录保留实际库版本/哈希。

本机 Release 构建及统一打包通过；最终定向验证 611 项、旧双安装迁移 93 项，共 **704 项 PASS**，覆盖新装、官方 v0.9.0 标签重建夹具升级、实际 RC/手动替换、未知文件保护、备份失败、修改/修复/卸载及个人/历史数据保护。最终包解包 16/16 插件文件匹配；库为 V4 / Schema 2 / 657。未执行全量 CTest、Validator、DAW 矩阵或音乐 Benchmark。原冲突文件当前不在现场，不能宣称已恢复其失败当时哈希；详见 [升级修复证据](docs/INSTALLER_DEV2_UPGRADE_FIX.md)。

新包：`build-installer/v4-0.10.0-dev.2/HarmonyContinuation-0.10.0-dev.2-Setup.exe`，SHA-256 `95984545b637e1e70498d8658bc9cd8e5b1f1596a8d8433758fbe4e30bc31ba5`。Build ID `0.10.0-dev.2 / 533eb46 / Release / Library 4`；后续证据提交不改变编译身份。旧 GitHub 未发布草稿仍为 dev.1，本轮 dev.2 安装包在本机独立目录；不将旧包误认为 dev.2。

**INSTALLER UPGRADE FIX READY**。完成源码提交/推送后停止，等待用户重新安装与 Cubase 人工验收；FL Studio / 风格听审继续 Pending，不自动启动其它功能。V4 原产品化和人工验收范围仍见 [V4 产品化](docs/FACTORY_V4_PRODUCTIZATION.md)、[V4 人工验收](docs/V4_DAW_ACCEPTANCE.md)。
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
