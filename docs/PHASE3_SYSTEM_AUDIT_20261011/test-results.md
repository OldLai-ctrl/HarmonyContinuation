# 审计测试记录

日期：2026-10-11
配置：Windows x64，CMake/Ninja Release，MSVC 14.43；Factory V4 SHA-256 与报告一致。

## 离线审计

命令：`build-v4-dev2/RecommendationReliabilityTests.exe reliability system-audit`

最终结果：`SYSTEM_AUDIT runs=5801 wall_ms=58567.1 recognitionFailures=8`；`matrix failures=0`。8 个失败均为省略五度变形后未识别出和弦进行，未计入引擎调用数。另有 266 条评分消融反事实记录；Phase 3A 例外默认关闭。

## 相关自动化测试

13 项目标测试通过：FactoryV4Focused、LibraryV3_snapshot、EnrichmentTests、MidiExportTests、MidiImportTests、PreviewTests、ProductizationTests、HarmonyAnalysisTests、ProgressionMatcherTests、ContinuationEngineTests、VstXmlChordParserTests、RecommendationReliabilityFocused、CubaseReliabilityFocused。总计 13/13，68.60 秒。

另行补跑评分相关测试：TendencyTests、IntentCompletionTests，2/2 通过，0.25 秒。

CTest 共登记 31 项。本报告执行了上述 15 项与审计范围直接相关的测试；其余 16 项未运行，不把本次结果称为全仓库全套测试。

`CubaseReliabilityFocused` 是自动化模拟/契约测试，不是真实 Cubase 宿主验收。
