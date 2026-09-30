# HarmonyContinuation 0.8.0-dev.2 回传

1. 产品版本：**0.8.0-dev.2**。
2. 源码 / 构建提交：`eedbf78c18ae98b16937a004e61f4430f5ce0629`。之后仅补充本回传文档，不影响安装包。
3. 已 push：**是**，`v0.8/dev`；未合并 main，未创建正式发布 tag。
4. More candidate audition：**READY**；引用原候选、沿用播放 / 停止 / 替换语义，关闭详情不强制停声音。
5. More candidate MIDI：**READY**；详情具备保存、拖出、Why、快照，按指纹保护候选对应。
6. MIDI Drag-Out：标准 VSTGUI `CDropSource` / `doDrag`，共用已有 PreviewSequence / ChordVoicer / MidiClip / SMF Writer；单击保存、移动超过原生阈值才拖出。
7. Drag payload：`IDataPackage::kFilePath`，标准 `.mid` 文件的 UTF-8 路径。
8. 临时 MIDI：`%TEMP%\HarmonyContinuation\MidiDrag`；安全唯一文件名，保留 48 小时，创建新拖出时清理过期自有文件；拖出结束不删除，音频线程无文件操作。
9. Visible Continuation drag：**READY，Cubase 待测**；默认 VoiceLed、Existing + Recommendation。
10. More Continuation drag：**READY，Cubase 待测**；原候选、同一导出内容。
11. Visible Enrichment drag：**READY，Cubase 待测**；完整变换后的进行。
12. More Enrichment drag：**READY，Cubase 待测**；原候选、同一导出内容。
13. SMF Format 0：**READY**，文件 / 内存读取。
14. SMF Format 1：**READY**，保留轨道与通道身份。
15. PPQ：**READY**；SMPTE / Format 2 明确 Unsupported。异常、损坏与资源超限安全拒绝。
16. Multi-track：同时音密度、复音、持续证据自动选和弦轨，不混合所有轨道；鼓通道 9 排除识别但保留解析信息；高级信息显示选轨。
17. MidiHarmonyExtractor：**READY**；可配置起音聚类、活动音切片、短经过音抑制、相邻合并、原 QN 节奏、完整片段 / 最后 OPEN；进入既有双模式。失败保留原进行，最高音不自动成为旋律约束。
18. Chord qualities：Major、Minor、Dim、Aug、6、m6、7、maj7、m7、dim7、m7b5、sus2、sus4、9、maj9、m9；**16/16**。缺五音可识别并降低置信度。
19. Inversion：**READY**；Root 与实际最低持续音 Bass 分开，`C/E`、`G/B` 用例通过。
20. Round-trip：**12/12 PASS**，6 套进行 × Block / VoiceLed，和弦身份与节奏通过；包含转位、经过减和弦、副属及小调。
21. Import fixtures：**27/27 PASS**，含 Format 0/1、Running Status、重触发 / 重叠、鼓轨、多轨、起音偏移、短音、八度、OPEN、损坏、限制、Session、统一导出及临时文件。
22. Continuation baseline：**42 例、214 候选，变化 0**；Preview / MIDI / Snapshot 各 214/214。原有 3 处结构质量问题仍在，因此 benchmark 按既有规则返回 1；未调算法、没有新增问题。
23. Enrichment baseline：**30 例、161 候选，变化 0**；Preview / MIDI / Snapshot 各 161/161。
24. Constraint：**16/16 PASS**；默认 Constraint Off / Tendency Balanced 行为保持。
25. CTest：最终 **19/19 PASS**。首次 18/19，发现旧测试 Factory 夹具缺中英文名称；仅补测试名称与隔离目录，定向通过后执行最后一次集中回归。
26. Validator：最终 **47/47 PASS**，Release 构建。
27. Demo：MIDI 工作流四类入口 × 三档缩放 **PASS**；含指纹、试听 / 保存 / 快照回调、拖动准备、原生 FilePath、详情关闭、实际文件导入 / OPEN。Zoom **9/9**；Resize **96/96**。这些检查不等于真实 Cubase drop / 发声通过。
28. Installer/package：完整 `HarmonyContinuation-0.8.0-dev.2-Setup.exe`、独立 `HarmonyContinuation-Library-2-Setup.exe` 与 SHA-256 清单 **READY**，位于 `build-installer/output/`；包内版本及两份新增语言资源已核对，语言各 221 键。Library 2 / 161 条、SQLite 1、Session 5、两类快照 2/1 保持；安装器 / 用户库 / 回退设计未改。
29. Cubase 人工待测：两种模式 More 试听 / 保存 / 快照，四处 MIDI 拖出产生 Part、音符与时值、scope、无保存框，文件选择 / Explorer 导入、完整 / OPEN、失败保留、工程重开、三档缩放与中英文；见 [简短清单](V0_8_DEV2_MANUAL_TEST.md)。
30. Cubase MIDI Part 私有直接拖入：**NOT IMPLEMENTED / NOT TESTED**；仅支持宿主或 Explorer 提供标准 `.mid` 文件路径的情况。
31. 当前 blocker：**无新增自动验证阻塞；dev.2 Cubase 人工 Gate 待验收**。已有 3 个结构质量问题保留。
32. 未进入下一版本；完成后停止，等待 Cubase 人工验收。

本机证据：`build-v08/dev2-final-build.log`、`dev2-final-ctest.log`、`continuation-dev2.json`、`continuation-dev2-changes.json`、`enrichment-dev2.json`、`enrichment-dev2-changes.json`、`constraint-v08.json`、`dev2-final-midi-ui.txt`、`dev2-final-zoom.txt`、`dev2-final-resize.txt`；首次失败保留在 `dev2-ctest.log`。无关 benchmark 未重复运行。
