# HarmonyContinuation 项目入口

Windows x64 VST3 和声创作助手：分析已有进行、检索曲库、继续发展或升级进行，并提供 MIDI 文件输入输出与本地试听。默认简中。用户教程在 [README.md](README.md)；AI 接手必须先遵守 [AGENTS.md](AGENTS.md)。

## 当前摘要

当前开发主线为 `v0.9/dev`，已集成 `library/v3` 的兼容桥与 Factory V3 内容，后者保留开发历史；新增数据位于 `data/factory-v3/`，历史 Library 2 数据保留。音乐算法和宿主适配保持来源版本。当前状态、证据与下一步只在 [NOW.md](NOW.md) 维护。版本、提交及实际构建以当前 checkout 和产物为准，不能凭目录名推断。

## 按任务定位

| 当前需要 | 首选位置 | 搜索词示例 |
| --- | --- | --- |
| 接续进度、待验收、安排下一步 | [NOW.md](NOW.md) 的对应章节 | `当前任务`、`待验收`、`证据` |
| 冻结 Factory V3 的内容覆盖 | [LIBRARY_V3_SUMMARY.md](LIBRARY_V3_SUMMARY.md) | `Total`、`Major`、`Missing Metadata` |
| 设备路径、分支、产物及同步边界 | [MAP.md](MAP.md) | `本机`、`同步`、`构建` |
| 构建、运行、局部测试、安装、交付 | [RUNBOOK.md](RUNBOOK.md) | `构建`、`验收`、`交付` |
| 设计取舍或已有约束冲突 | [DECISIONS.md](DECISIONS.md) | 决策 ID、模块名 |
| 覆盖安装、库迁移、回滚、真实宿主结论 | [RISKS.md](RISKS.md) | `安装`、`数据`、`宿主` |
| 查找过去解决过的具体问题 | [history/](history/README.md) | 错误文本、模块、提交 |

## 代码与专题路由

| 范围 | 代码入口 | 按需查阅的专题 |
| --- | --- | --- |
| 宿主、窗口、生命周期与诊断 | `src/host/`、`src/plugin/`、`src/ui/EffectiveScale.h` | `docs/V0_9_HOST_ARCHITECTURE.md`、`docs/FL_STUDIO_COMPATIBILITY.md` |
| MIDI 读取、识别、输出及拖放 | `src/midi/`、`src/io/` | `docs/V0_8_DEV2.md`、`docs/MIDI_EXPORT.md` |
| 分析、匹配、续写、约束与倾向 | `src/core/` | `docs/HARMONIC_MODEL.md`、`docs/CONTINUATION_ENGINE.md` |
| 升级进行、试听与声部进行 | `src/enrichment/`、`src/preview/` | 对应 `tests/EnrichmentTests.cpp`、`tests/VoiceLeadingMetricsTests.cpp` |
| 状态、曲库及快照 | `src/session/`、`src/library/`、`src/snapshot/`、`src/persistence/` | `docs/LIBRARY_V3_COMPATIBILITY.md`；安装查 `docs/INSTALLER_AND_LIBRARY.md` |
| UI、Demo、安装及测试工具 | `src/ui/`、`src/demo/`、`packaging/`、`tools/`、`tests/` | `docs/RESPONSIVE_UI.md`、`docs/DEMO_HARNESS.md`、`RUNBOOK.md` |

## 阅读与记录边界

索引是条件路由，不是必读清单。需要定位时搜索相关路由，再定位目标文件中的章节、符号或关键词；规则以 [AGENTS.md](AGENTS.md) 为权威来源。禁止因为这里有链接就通读目标文件。信息不足或冲突才扩大范围，信息足够即停止；不连续分段读取无关全文。旧 Phase 文档是版本上下文，若与当前代码冲突，核对代码和有效决定。

保持单一事实位置：当前状态归 NOW，有效路径归 MAP，操作归 RUNBOOK，持久取舍归 DECISIONS，未解决风险归 RISKS；已结束但有复用价值的过程归 history。其他位置放链接。仅在状态或内容改变时更新，不机械刷新日期，不复制整段聊天。
