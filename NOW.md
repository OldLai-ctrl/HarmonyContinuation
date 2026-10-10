# 当前接续状态

## 功能基线

- 当前正式版本：`0.9.0`；发布提交与 `v0.9.0` Tag 均为 `ddc4f89babd4520daac3eae74824d6f581ce0e75`。源码清理提交不改变该正式包的构建身份。
- Factory Library 3 / Factory Schema 2 / User Schema 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2。音乐算法、宿主适配、MIDI 和生产数据本轮未改。
- Factory V3 为 155 条 canonical 历史记录加 474 条新进行，共 629 条；六条 QUESTIONABLE 留在非生产清单。统计见 [LIBRARY_V3_SUMMARY](LIBRARY_V3_SUMMARY.md)，兼容与降级边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。

## 当前任务

2026-10-10 按用户要求清理本地与远程仓库：移除四个阶段专用色彩/Why 冒烟工具、三个已被统一安装器测试取代的旧脚本、过期人工清单/交接材料和 Phase 3 生成报告及其脚本；同步删除构建目标和失效引用。保留常规回归测试、测试输入、迁移 fixture、数据生成工具、音乐基准、现行安装器和打包工具。

本地只保留正式版 `build-installer/release-0.9.0/` 与可复用 Inno 编译器；旧开发/RC 包、隔离安装目录、缓存和中间产物清除。正式包依赖的最低历史证据集中至正式目录 `evidence/`。这些本地输出不进入 Git。

清理检查：完整插件配置的 CMake 生成通过，剩余源码入口与文档引用检查通过，正式 Setup 校验和保持不变。本轮不重新编译产品、不运行音乐或宿主全量回归。清理提交同步到 `main` 与 `v0.9/dev`；实际提交/推送状态以 Git HEAD 与远程引用为准。旧版本分支、Tag 与 Git 历史保留。

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
