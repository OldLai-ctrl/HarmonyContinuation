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

## 2026-09-30 · dev.2 MIDI 工作流

- 用户决策端指令确认 dev.1 安装器 / 独立库 Cubase Gate PASS，依用户报告进入 dev.2。
- 新增标准 SMF Format 0/1 PPQ 文件导入与和弦块提取；更多候选完整操作；标准 VSTGUI 文件路径 MIDI Drag-Out。SDK API 以本地 `vstgui/lib/dragging.h`、`cdropsource.h`、`idatapackage.h` 为依据。
- 本机构建、定向测试及集中回归结果另见 `V0_8_DEV2_REPORT.md`；Demo 自动检查只能证明候选对应、回调及拖动准备，不能证明 Cubase 接受 drop、实际发声或工程重开。
- dev.2 Cubase 人工验收待执行。Cubase 私有 MIDI Part 直接拖入未实现、未测试，不属于已支持输入。

### dev.2 人工 Gate 准备

收到 Cubase Manual Acceptance 指令后，仅更新 21 项人工清单，并准备 `manual-tests/dev2-midi` 文件。本轮未重跑产品自动回归，未宣称任何新 Cubase PASS，未修改插件或安装包、未推进 RC。等待用户实际结果；此前自动报告不代替人工验收。

### 2026-09-30 · dev.2 人工结果与两项 UX 收口

- 用户决策端最新指令报告：dev.2 Cubase 人工测试总体通过；拖出 MIDI 会产生额外 Marker Track / 标记，主界面分组缺少滚动。这是用户宿主证据，未收到逐项截图或独立记录。
- 本轮只增加 AnnotatedFile / DAWClip 导出配置和七组共用的 ScrollableCandidateList。两个文件从同一 ExportSequence 写出；点击保存保留说明，拖出仅保留 PPQ、Note On/Off 和 End Of Track，单轨 Format 0，不写 tempo / meter / text / key / marker。
- 候选生成、排名、质量/去重门槛保持原状；移除 MainView 的两行展示上限，使用既有策略默认最多 3 条，约两行高度独立滚动。滚轮在组内（包括边界）消费；空隙走父容器。无候选视图复用；绘制、点击映射共用原候选下标。
- 本机针对性验证：MidiProfileTests 8/8，实际拖出临时文件事件精简，两个布局/三个范围音乐一致，保存 metadata 保留；Demo 七组 × 0/1/2/3/6 条 × 三种缩放，试听/拖動准备/Why/Snapshot/候选身份检查通过。实际宿主 drop、发声和 Marker Track 消失仍待用户复测，不由 Demo 推断。
- 本轮未重复完整 CTest、Validator、音乐基线、Import fixtures 或 Round-trip。用户定向 PASS 后才运行最终集中回归并决定推进 RC，见 `V0_8_RC_TARGETED_MANUAL_TEST.md`。

### 0.8.0-rc.1 Gate

- 用户明确回传 dev.2 Cubase 人工验收 **PASS**：精简拖出无额外 Marker Track / 标记、七组滚动、滚动候选试听与拖出、More、MIDI Import 及其它 dev.2 人工项目；未发现新问题。两份人工清单同步完成状态。
- 功能冻结在 `58e765e`。本轮集中回归仅执行一次：CTest 20/20，Release Validator 47/47；Continuation 42 例 / 214 候选，Enrichment 30 例 / 161 候选，所有逐例数据与既有基线一致。原有三处结构质量提示未新增、未修音乐算法。
- CTest 内含 Constraint 16/16、MIDI Import 27/27、Round-trip 12/12；Demo MIDI / More / Scroll / Why PASS、Zoom 9/9、Resize 96/96。没有重复运行这些测试。
- 集中回归未发现新 bug。只更新 ProductVersion 为 0.8.0-rc.1 与 RC 文档；库 / 数据 / 快照版本保持 2 / 1 / 5 / 2 / 1。
- RC1 安装包是从确定 RC 提交重新构建的产物；其资源检查和最终 Validator 结果记录在随包 BUILD.txt。**dev.2 Cubase PASS 不等于 RC1 安装包已经重新人工测试**。完成 RC 打包后停止。

## 2026-10-10 · RC1 用户报告 / RC2 待复验

用户报告 Cubase Pro 15 中 RC1 的加载、自动推荐、排序切换、Why?、Preview、MIDI 导出及 Continuation 色彩提示通过。截图的 Dmin → C → Bb → A 末和弦 OPEN；升级 Dmin → C/E → Bb → A 显示灰色「不足以判断」，Why? 为音集、方向或时值不足。源码确认末时值缺失会使整段摘要不可用，并不代表单和弦静态指标全部失败。RC2 只分开展示可靠静态指标和整段时间信息不足；真实 Cubase 定向复验 Pending。未新增 FL Studio 结果，未宣称跨工程恢复或完整宿主矩阵通过。

## 2026-10-10 · v0.9.0 发布时的用户确认

用户明确确认 Cubase Pro 15 主要功能及 RC4 色彩提示可见性修复通过人工验收。v0.9.0 只更新版本、构建元信息与交付文档，不改相关运行逻辑，复用该报告；正式构建未重新运行实机验收。FL Studio Not Verified / Pending，本轮不要求执行，也不因此推迟发布。
