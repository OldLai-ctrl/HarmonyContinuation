# 当前接续状态

## 功能基线

- 当前正式版本：`0.9.0`；发布提交与 `v0.9.0` Tag 均为 `ddc4f89babd4520daac3eae74824d6f581ce0e75`。源码清理提交不改变该正式包的构建身份。
- 正式版为 Factory Library 3；本轮独立开发版本为 Library 4。Factory Schema 2 / User Schema 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2 保持；生产数据与宿主适配未改。
- Factory V3 为 155 条 canonical 历史记录加 474 条新进行，共 629 条；六条 QUESTIONABLE 留在非生产清单。统计见 [LIBRARY_V3_SUMMARY](LIBRARY_V3_SUMMARY.md)，兼容与降级边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。

## 当前任务

2026-10-11：`library/v4` 的 **0.10.0-dev.3 Cubase 输入兼容性开发包已完成，等待人工复验**。代码提交 `99017f596cef895f2024677120b7005fa7c4a98e` 已通过 GitHub 接口同步；本地与远端源码 tree 完全一致，未使用崩溃的 git-remote-https 推送。验证记录另行提交至同分支；不合并 main、不创建正式 Tag、不正式发布。

缺五度造成七和弦身份丢失、节奏参与和声对齐及门槛的问题已在明确标记的模拟冲突输入中复现并修复。新 800 组覆盖中，500 组 B08/B11 目标均匹配、保留、默认可见且 More 可访问，300 组普通输入有推荐；原 1440 组及 12 个相关测试通过。界面专项、Preview/MIDI 和 Enrichment 回归通过；隔离安装 413 项通过，个人数据哨兵保留。Factory 数据库哈希与 dev.2 相同，657 条，Schema 2/2/5/3/2 未改。

开发包位于 `build-installer/v4-0.10.0-dev.3/`，Build ID `0.10.0-dev.3 / 99017f5 / Release / Library 4`；产物、SHA-256、对照、检查范围与局限见 [dev.3 可靠性报告](docs/DEV3_CUBASE_RELIABILITY_FIX.md) 和包内 BUILD_INFO / evidence。旧 dev.1/dev.2 目录未覆盖。安装器脚本逻辑未变，仅复用隔离检查并增加可选版本参数。

**READY FOR CUBASE RE-ACCEPTANCE：YES；Cubase 真实 B08/B11 仍 Pending。** 旧真实 VST-XML 协议证据存在，但用户这次失败的原始载荷尚未捕获，不能用模拟 XML 宣称真机已经修复。现有菜单已增加主动本地导出输入字段、模板过滤原因、后台结果和 UI 排名。下一步只按 [人工清单](docs/V4_DAW_ACCEPTANCE.md) 安装复验，必要时主动导出；完成后停止，不自动开发其它功能。dev.2 历史验证见 [旧报告](docs/DEV2_CORE_RELIABILITY_FIX.md)。

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
