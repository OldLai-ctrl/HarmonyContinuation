# HarmonyContinuation

面向 Cubase 的 VST3 和弦进行导入、播放同步与和声分析原型。和弦从 Cubase 和弦轨拖入后，插件显示调性候选、级数、功能、特殊角色和可切换的完整进行／和声骨架。

界面与用户可见诊断以简体中文为主。当前不包含续写推荐、数据库或 MIDI 导出。

## 当前状态

- Phase 0A：基础设施 **通过**。
- Phase 0B：Cubase 15.0.30 Build 287 的和弦轨拖放及 VST-XML 1.4 解析 **通过**。
- Phase 0C：宿主接入 **通过并冻结**。用户于 2026-09-24 报告非零位置、前后跳转和循环播放测试均正常。工程关闭重开也报告正常；当前源码尚未保存导入和弦，自动恢复行为仍需与当时的测试方式核对。
- Phase 1：独立和声核心、黄金测试、开发命令行工具和插件分析视图已实现。用户已在 Cubase 中验证拖入、调性显示、播放时文字与黄条，以及播放线刷新。
- 2026-09-25 编辑器打开卡住的问题已修复，并安装了经校验的新版；原因与验证见 [排查记录](docs/EDITOR_OPEN_HANG_20260925.md)。
- 2026-09-25 实测三和弦的调性分析与播放时文字／黄条显示问题已修复；详见 [修复记录](docs/CUBASE_TRIAD_UI_FIX_20260925.md)。
- 用户已确认拖入的和弦 QN 对应 Cubase 工程绝对位置；当前输入模式为 `AbsoluteProjectQN`。

详细实测见 [Cubase 宿主集成记录](docs/HOST_SPIKE.md)、[Cubase 基础实测](docs/CUBASE_REALITY_TEST.md) 与 [播放同步测试步骤](docs/CUBASE_TIMELINE_SYNC_TEST.md)。

## 构建与测试

项目使用本地 C++20、MSVC x64、CMake、Steinberg VST3 SDK 3.8.1 和 VSTGUI，不会自动联网下载依赖。源码包不含 SDK；在原开发机上 SDK 曾位于 `D:\VibeCoding\VST_SDK\vst3sdk`。新机器可通过 `VST3_SDK_ROOT` 环境变量或 CMake 参数指定完整 SDK 路径。

在 **x64 Native Tools Command Prompt for Visual Studio** 或 **Developer PowerShell for VS** 中进入项目目录，先配置再构建：

```powershell
cmake -S . -B build-vst3 -G Ninja -DVST3_SDK_ROOT="C:/path/to/vst3sdk"
cmake --build build-vst3 --target HarmonyContinuation WeightBarGeometryTests CoreTimelineTests HarmonyAnalysisTests VstXmlParserTests VstXmlChordParserTests harmony_cli --parallel 4
ctest --test-dir build-vst3 -C Debug --output-on-failure
```

只编译和测试和声核心时，不需要 VST3 SDK：

```powershell
cmake -S . -B build-core -G Ninja -DHC_BUILD_PLUGIN=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
./build-core/harmony_cli.exe tests/fixtures/harmony/secondary_dominant.json
```

开发命令行还支持 `--key C:major` 一类调性指定。输入是扁平 JSON 和弦数组，字段及已知边界见 [和声模型](docs/HARMONIC_MODEL.md)、[黄金测试](docs/HARMONY_PHASE1_TESTS.md) 和 [Phase 1 回传报告](docs/PHASE1_HANDOFF.md)。

VST3 Validator 在插件构建时运行。构建产物位于 `build-vst3\VST3\Debug\HarmonyContinuation.vst3`。构建不会自动复制到 Cubase 扫描目录。关闭 Cubase 后，可在管理员 PowerShell 中运行 `tools\install-debug.ps1` 安装并校验；也可运行 `tools\verify-installed-build.ps1` 只比较工作区与安装版 SHA-256。

## 目录

- `src/core/`：不依赖 Steinberg SDK 的和弦数据、QN 定位与和声分析。
- `src/plugin/`：VST3 音频快照、Cubase 拖放、VST-XML 解析与控制器消息。
- `src/ui/`：简体中文界面、和弦时间轴、播放头与诊断面板。
- `tests/`：核心定位、和声黄金测试和 VST-XML 解析测试。
- `docs/`：宿主实测、同步手测和 fixture 说明。
