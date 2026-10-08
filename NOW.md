# 当前接续状态

## 功能基线

- 开发分支：`library/v3`；实际来源：GitHub `v0.9/dev` 的 `1ecef083a2b49d3c31ec5f13503cf79aed6a63ef`。用户授权以该基线补齐能力；未获得 `b7c9ef9…`，未声称推送过该 capability commit。
- 当前源码版本：`0.9.0-dev.2`。冻结格式和兼容边界以 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md) 及其链接的代码常量为准。
- 主目录与旧版本恢复关系只在 [MAP](MAP.md) 维护。跨设备导航文档已合入本轮改动，不再作为独立未完成任务。

## 当前任务

Factory V3 兼容桥代码已实现：六个旧 ID 的查询/快照/固定候选恢复、新写入 canonical ID、Library 2/3 共存，完整和弦保存与 additive User Schema 2 迁移。本轮不批量扩库，不修改音乐算法或 Host/FL 适配。Core 仅增加结构化数据及 realization/进行构造的字段传递。

三个旧 checkout 已整体移入同级归档，保留 Git 历史、未提交文档及构建产物；主开发目录为独立仓库，已安装插件、Factory 历史和生产 user.db 未移动、未覆盖。

提交与推送对象为 `library/v3`；实时状态以 `git status -sb`、本分支最后的 compatibility bridge 提交及远端同名分支为准，不在文档反复追加 HEAD 快照。未授权 merge、tag 或正式发布。

## 证据

2026-10-08 在本工作目录运行的限定验收：CMake/MSVC Release 核心构建，`ctest -R '^LibraryV3_'` 四组通过，总计 406 检查：resolver 30、snapshot 75、user 265、factory 36。包括保存已结束末和弦时不误用 OPEN 默认时值。输出在忽略的 `build-v3/Testing/Temporary/LastTest.log`；可复现步骤见 [RUNBOOK](RUNBOOK.md)。这些是当前任务实际执行的结果，不是交接 PASS 的转述。

- 18 类用户和弦保存、关闭、重开后，完整内容/元数据、试听音高结构和 MIDI 字节一致。旧 Schema 1 fixture 的 raw payload、名称/标签/风格/意图/收藏/备注/时间戳不因迁移改变。
- 注入迁移失败及 typed data 写失败，确认事务回滚且无部分行；一次性备份保留且不重复生成。未操作真实 user.db。
- 插件、Demo 和 MIDI CLI 的 Release 在本轮编译成功；提交后重新配置交付产物，使生成元数据携带本轮提交。实际二进制提交由 `build-v3-plugin/generated/ProductVersionGenerated.h` 与包内 moduleinfo 核对，不能把早期工作树编译的 `1ecef08` 标成最终提交。
- 生成 V3 DB 的检查结果：`library_version=3 schema=2 progressions=155`。Library 2 的 161 条历史 fixture 同时通过加载与切换测试。
- 本轮未跑全 CTest、42 Continuation、30 Enrichment、FL Host Harness 或 Validator，未安装、未做 DAW 验收、未构建 Setup。历史宿主报告不替代本轮真机验收。

## 待验收与下一步

兼容桥自动验收已通过，可在后续明确的任务中开展 Library V3 扩库；本轮停在兼容桥，不自动开始 500–800 条内容制作。扩库必须遵守冻结 schema 和 QUESTIONABLE 规则。

User Schema 2 需要本桥接构建或后续支持版本；v0.8.0 和旧 v0.9.0-dev.1 无法读写它。安装前按 [RISKS](RISKS.md) 定位降级风险，保留用户数据备份；这是版本边界，不是本轮执行阻塞。

之后另行验证 Cubase/FL 的加载、工程重开、个人库和 MIDI 工作流。此前 v0.8 验收不能自动覆盖新包；真实 FL 验收仍待完成。没有承诺交付日期。
