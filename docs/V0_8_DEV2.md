# 0.8.0-dev.2 · MIDI Workflow

本轮从 `90fb6b7` 接续。用户决策端指令确认 dev.1 安装器 / 独立库 Cubase Gate **PASS**；该结果属于用户报告。本轮新增工作尚待 Cubase 人工验收。

## 功能

- 继续发展、升级进行的“查看更多”使用原候选，详情提供试听、MIDI、快照及原有解释。候选指纹或导入上下文改变时，旧选择失效；关闭详情不停止试听。
- 四类入口（两种模式的主卡、更多详情）均支持单击 MIDI 保存、按住拖到 DAW。超过 VSTGUI 原生 4 单位移动阈值才发起拖动；拖动时不会触发保存对话框。
- 保存和拖出共用 `MidiWorkflow` → 原 PreviewSequence / ChordVoicer / MidiClip / SMF Writer。默认保持 **VoiceLed + FullPhrase**；升级进行导出完整变换后的进行。
- VSTGUI `CDropSource` / `doDrag` / `IDataPackage::kFilePath` 传递标准 `.mid` 的 UTF-8 文件路径。临时文件位于 `%TEMP%\HarmonyContinuation\MidiDrag`，唯一安全文件名，保留至少 48 小时；后续创建时清理过期自有文件，不在拖动结束时删除。
- 更多 → 导入 MIDI → 完整片段 / 作为未完成进行，选择 `.mid` 或 `.midi`。也可从 Explorer 将文件拖到当前进行时间轴。导入失败保留当前进行；OPEN 仅影响最后一个和弦。

## 导入边界

`StandardMidiFileReader` 支持 Format 0/1、PPQ、VLQ、Running Status、音符开关、速度零关闭、Tempo、Time Signature、Track Name、End Of Track、未知合法 Meta / SysEx。SMPTE、Format 2 明确返回 Unsupported。

保留轨道、通道与音符时值。鼓通道 index 9 默认不参与识别。多轨按同时音密度、复音与持续证据选一个和弦轨，显示选中轨道，**不混合所有轨道**。底层支持显式选轨配置；本版界面自动选择。

提取器服务于和弦块 MIDI：可配置起音容差 / 最短持续时间（默认各 0.125 QN），按音符开始和结束切片，抑制依附长和弦的短经过音，但保留独立短和弦块。八度归约、实际最低持续音作为 Bass、原 Core 的音程集合用于 chord-fit、缺五音降低置信度、同名相邻切片合并，保留原始 QN 节奏。

识别范围：Major、Minor、Dim、Aug、6、m6、7、maj7、m7、dim7、m7b5、sus2、sus4、9、maj9、m9。对称和弦 / 等音歧义以实际低音为线索；复杂演奏可能不确定。本版不推断 rootless voicing，不识别完整编曲中的所有声部。

文件上限 32 MiB、128 轨、20 万音符、200 万事件；同音重叠队列 256。提取最多 64 个和弦，沿用现有单和弦 128 QN 上限；超限拒绝导入，不覆盖用户进行。解析异常关闭音符时给提示。

输出进入原 ImportedProgressionSession / HarmonyAnalyzer / 双模式流水线。识别置信度仅用于高级信息，不修改结构权重。最高音不自动成为 MelodyConstraint。

Session schema **5**、Library content **2**、SQLite schema **1**、Continuation snapshot **2**、Enrichment snapshot **1** 保持原值。Factory 161 条内容、和声算法、排名、约束默认值、倾向和声部规则不变。新增文案同步两种语言并进入 VST3 包。

## 验证与交付

定向入口：`MidiImportTests`；Demo `--midi-workflow-smoke REPORT --midi-smoke-input FILE`。覆盖解析、损坏及资源限制、提取、16 类和弦、12 次 Block/VoiceLed 往返、原进行保留、Session 往返、统一导出与临时文件；Demo 检查四种入口在 100/125/150% 的候选指纹、试听 / 保存 / 快照回调、拖动准备和原生文件路径、详情关闭、实际文件导入 / OPEN。

阶段末只集中运行一次 CTest、Validator、42 Continuation、30 Enrichment、16 Constraint、Import/Round-trip、Demo。具体结果见 `V0_8_DEV2_REPORT.md`。生成完整 Windows Setup 和 Library-2 Setup；本轮不自动替换用户系统安装。

Cubase MIDI Part 私有直接拖入 **NOT IMPLEMENTED / NOT TESTED**。Cubase 是否接受标准文件拖出、产生的 MIDI Part 及实际听感均须人工验证，见 `RC_DAW_ACCEPTANCE.md`。
