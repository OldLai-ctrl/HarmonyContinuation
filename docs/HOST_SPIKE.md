# HarmonyContinuation — Cubase 宿主集成记录

记录日期：2026-09-24。测试宿主：Cubase Pro 15.0.30 Build 287（x64）。

## 阶段状态

- **Phase 0A — 基础设施：通过。** C++20、VST3 SDK 3.8.1、VSTGUI、CMake、MSVC x64；处理器、控制器、中文编辑器、宿主快照和拖放入口均已建立。
- **Phase 0B — Cubase 实测：通过。** Cubase 和弦轨多选事件可拖入插件，收到 1087 字节 UTF-8 VST-XML 1.4，并成功解析出四个和弦。
- **Phase 0C — 播放同步与导入会话：通过，宿主接入冻结。** 2026-09-24 用户确认非零位置、前后跳转和循环播放正常。工程关闭重开也报告正常；本源码尚未保存导入和弦，因此不能据此推断无需重新拖入即可恢复导入内容。

## 已确认的 Cubase 行为

用户实测拖入的和弦为：

| 和弦 | 工程位置 | 推导时长 |
|---|---:|---:|
| C7 | 0 QN | 4 QN |
| Amin7 | 4 QN | 2 QN |
| A7 | 6 QN | 1 QN |
| Dmin7 | 7 QN | 开放时长 |

用户已另行确认：拖入 XML 中的 QN 按 Cubase 工程绝对位置记录。因此正式拖放会话使用 `AbsoluteProjectQN`，本阶段不要求再做 +32 QN 坐标偏移校准。播放期间的 ProcessContext 曾报告 QN 0.587、3.211、4.811、6.581、7.456，速度 120 BPM、拍号 4/4、工程位置有效、播放状态有效。测试中的 `ProcessContext::Chord` 均无效，所以产品定位不依赖该字段。

Ctrl+C 后显式检查未得到 VST-XML：VSTGUI 暴露 0 个剪贴板数据项；Windows 只列出 `DataObject`（ID 49161，8 字节）和 `Ole Private Data`（ID 49171，120 字节）。剪贴板仅保留为用户主动触发的诊断入口，不是正式输入路线。

正式输入路线：**Cubase 和弦轨拖放 → VSTGUI IDataPackage → Clipboard VST-XML 解析 → 当前导入会话**。

## Phase 0C 实现

- `src/core/ImportedProgression.h` 保存当前实例最近一次成功导入的和弦事件、坐标模式和递增 revision。非法、乱序或坐标模式未知的数据不会替换当前有效会话。
- `src/core/CurrentChordLocator.h` 是纯 C++ 核心定位函数，只按相邻事件起点定位。它不依赖 VST3/VSTGUI，能直接处理前后跳转；最后一颗无结束时间的和弦会一直保持 active。
- 音频处理线程只把固定大小的 ProcessContext 字段写入 lock-free 原子快照，并增加 generation；不解析 XML、不格式化字符串、不调用 UI。
- 编辑器可见时由 VSTGUI UI 计时器请求快照：播放期间约 20 Hz，停止时约 4 Hz。连续三次轮询没有新快照时按空闲状态降频；编辑器窗口隐藏时降至约 2 Hz。generation 未变化时跳过 UI 更新。
- 和弦块布局只在新导入时重建。位置变化只失效播放头附近区域；当前和弦变化只失效前后两个和弦块。播放刷新不会重绘整个编辑器，也不会重新解析 XML。
- `ProcessContext::Chord` 仅作为诊断数据显示。当前导入和弦会话在插件实例运行期间保留；当前尚未加入 Cubase 工程关闭再打开后的会话持久化。

## 稳定性

此前在 Intel 图形驱动 `igd10um64xe.DLL` 31.0.101.4032 下出现白屏和 Cubase 崩溃。当前 Windows VSTGUI frame 禁用 DirectComposition，并使用 Direct2D 软件绘制。之后用户提供的 Cubase 截图显示编辑器可正常绘制，并完成拖放与播放快照实测。本阶段保留该稳定性设置，不提高界面刷新率。

## 自动验证

当前构建使用本机 Visual Studio/MSVC x64、Windows SDK 10.0.26100、CMake/Ninja 和离线 VST3 SDK 3.8.1。每轮汇报会记录本次实际执行的插件构建、VST3 Validator 和 CTest 结果；Cubase 手测必须由用户在宿主中完成。构建产物与系统安装目录不会自动同步，手测前运行 `tools/verify-installed-build.ps1` 核对两者 SHA-256。

## 当前结论

- Cubase Pro 15.0.30 Build 287：插件窗口绘制及多和弦拖放解析已实测通过。
- VST-XML 1.4：生产拖放路径成功解析四和弦；QN 为用户确认的工程绝对位置。
- ProcessContext 工程 QN：播放期间有效且持续变化；`ProcessContext::Chord` 无效且非必需。
- 剪贴板：未发现 VST-XML，不作为产品输入路线。
- Phase 0C：用户报告 [播放同步手测](CUBASE_TIMELINE_SYNC_TEST.md) 中的非零位置、前后跳转和循环播放均正常，宿主输入与播放同步路线据此冻结。当前源码的 `Processor::getState`/`setState` 只保存和读取 `HC00` 标记，未保存导入和弦；工程重开后的导入内容自动恢复仍未由代码证实。
- Phase 1：从拖放事件到独立和声核心的连接已建立。播放头移动不会触发和声重算；详见 [和声模型](HARMONIC_MODEL.md)。
- 2026-09-25：用户报告 Phase 1 安装版插入正常，但打开编辑器时 Cubase 无响应。独立 SDK EditorHost 复现；排查与修复记录见 [编辑器打开卡住](EDITOR_OPEN_HANG_20260925.md)。
- 2026-09-25：用户补充实测 C–G–G#dim–Am–Em 拖入正常，但调性候选接近平均、播放头经过后文字消失、权重黄条与和弦块边框重叠。对应修复与复测见 [真实三和弦与界面刷新](CUBASE_TRIAD_UI_FIX_20260925.md)。
- 2026-09-25：用户再次提供 Cubase 截图，确认调性与文字已正常，仍觉得黄条边界不齐。截图显示黄条底边基本一致，但旧版用不同长度表示权重。后续改为全宽黄条，以粗细表示权重，并按 VSTGUI 实际缩放比对齐物理像素；本机构建、Validator 47/47、CTest 5/5、EditorHost 窗口响应、安装版哈希一致均已确认，Cubase 新版画面尚待实测。
- 2026-09-25：用户第三次实测反馈全宽黄条的粗细差不醒目，要求恢复长度表达；播放线在和弦块上下方留下旧位置残影。已恢复长度表达但保留 DPI 像素对齐，同时使播放线旧、新位置的整条高度失效；构建、Validator 47/47、CTest 5/5、EditorHost 窗口响应通过。Cubase 关闭后已安装并核对哈希一致；用户随后报告同一段和弦在 Cubase 中重拖并播放，两项均正常。
- 2026-09-25：Phase 3 加入 161 条种子曲库、SQLite 运行库、后台推荐与 RECOMMEND 开发视图。本轮插件构建、CTest 7/7、Validator 47/47、独立 EditorHost 打开响应正常；用户本轮无法操作 Cubase，因此 **Phase 2 新基线和 Phase 3 界面均未完成 Cubase 实机 smoke**，不能写为宿主 PASS。细节见 [Phase 3 验证记录](PHASE3_TESTS.md)。

公开格式参考：Steinberg [Clipboard VST-XML 定义](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Clipboard%2BVST-XML/Index.html)。
