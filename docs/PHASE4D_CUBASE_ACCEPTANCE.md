# Phase 4D Cubase 中文验收清单

版本：0.10.0-dev.4，开发构建 `6efacf3-phase4d`。这是 `6efacf3` 基础上的本轮源码补丁，尚未正式发布。

## 先安装或替换

1. 关闭 Cubase。备份现在的整个 `HarmonyContinuation.vst3` 文件夹，并保留旧安装包。
2. 普通安装使用本轮 OFF Setup；HarmonyFirst 默认为 OFF，Phase 3A 默认为 OFF。
3. 对比算法时，解压 OFF 或 ON 便携 ZIP，把其中完整 `HarmonyContinuation.vst3` 文件夹替换到原插件位置。两者使用相同插件身份，不能同时放入扫描路径；一次只保留一个版本。便携包已含所需运行库与 Factory V4。若使用独立 Factory，必须核对当前活动库为 V4/657 条。
4. 重开 Cubase，确认版本 dev.4。在“高级分析”查看 `HarmonyFirst=ON/OFF` 和 `Phase3A=OFF`；“宿主诊断”也显示编译状态。先导入和弦并等待分析完成，再记录实际推荐状态。这里没有运行时 ON/OFF 按钮，切换需要换包并重启宿主。

## 按输入时值检查输出

不依赖播放指针处的宿主拍号。工程可以变拍号，当前宿主拍号不能代表所选输入；XML 可能只有 QN 起点。插件从匹配输入的已知时值及相邻起点估计比例，保留 Factory 相对节奏。不能仅凭时值唯一推断 3/4 或 6/8。

1. 在 3/4 工程输入 `Cmaj9 3QN → Dbmaj9 OPEN`，固定 C Major。查找返回 Cmaj9 的候选（V4_B11）。末和弦估计保持 3QN，后续 Cmaj9 应为 3QN。
2. 分别试听、保存 MIDI、拖入 Cubase。比较实际起点和音符长度，不能只看 MIDI 拍号。
3. 以 4QN、3QN、5QN、3.5QN、4.5QN 输入重复操作，覆盖 4/4、3/4 或 6/8、5/4、7/8、9/8。
4. 仅改变工程当前拍号而不改变输入 QN，输出时值不应被强制缩放。特殊节奏应保留长短比例；包内 `midi/*_output.mid` 为十条 Factory 的六拍号对照样本（含输入和续写）。
5. MIDI 中 OPEN 没有通用编码：包内 OPEN 样例使用有限末音保持时值。导入后请按当前界面操作把最后一个和弦设为 OPEN，再作对应测试；不要把文件有限音长当成插件已恢复 OPEN。

## R01：截图场景

导入 `R01_Am_Gsharpdim_G_DoverFsharp_2QN_OPEN.mid`，确认四个和弦 `Am → G#dim → G → D/F#`，起点 0/2/4/6 QN，前三个各 2QN，最后设为 OPEN。Style/Intent 保持 Auto，倾向保持默认。

- ON / Auto：模拟实测选 A Major，FULL 4、SKELETON 4；Loop 前两项 Em→Am 约 68.04、Em 约 67.17。A08 约 60.90，Loop 第 6，需“查看更多”。候选评分会随输入音符、OPEN、风格和倾向变化。
- 固定 A Minor：A08 第 1，约 60.90，SKELETON 3/4。
- 固定 A Major：该模拟输入没有通过门槛的续写。
- 同时检查每和弦 4QN、末和弦非 OPEN 的差别。手动调性只用于对照，不是绕过 Auto 问题的修复办法。

## FULL / SKELETON

OFF 包导入 `FULL_SKELETON_F_Fsharpdim7_G_C.mid`，固定 C Major：`F 4QN → F#dim7 0.5QN → G 3.5QN → C 4QN`。预期 FULL 4、SKELETON 3；FULL 中经过减七块灰化，Preview 与 MIDI 仍保留原始输入四个和弦。

ON 包该例可以为 4/4：HarmonyFirst 检索结构权重不由短时值直接压低。另导入 `FULL_SKELETON_HF_Am_Gsharpdim_G_DoverFsharp_Aminor.mid`，固定 A Minor，末和弦设 OPEN：实测 FULL 4、SKELETON 3。结构视图影响检索描述，不删除演奏内容。

## 可直接执行的音乐样例

固定 C Major，分别测试 OFF 和 ON。

| MIDI | 输入 | 观察 |
|---|---|---|
| Descending_bass_INPUT_3chords.mid | C → Cmaj7/B → C7/Bb，各4QN | V4_B08，约81.89，后续 F/A → Fm/Ab；B、Bb 低音保留 |
| Descending_bass_INPUT_3chords_omitted5.mid | 起始C完整，随后两和弦缺五度 | 核心七和弦与明确低音仍能导入识别 |
| Descending_bass_FULL_5chords.mid | 完整五和弦路径 | 用作音乐对照 |
| Side_slipping_INPUT_2chords.mid | Cmaj9 → Dbmaj9，各4QN | V4_B11，约77.13，返回Cmaj9；不能变成C9 |
| Side_slipping_INPUT_2chords_omitted5.mid | 两个maj9缺五度 | 大七度与九度仍保留 |
| Side_slipping_FULL_3chords.mid | 完整三和弦路径 | 用作音乐对照 |

仅 C/E 两个不同音高不能唯一证明 C 大三和弦；缺五度 MIDI 样例保留开头完整 C，后续七和弦才缺五度。带和弦名/根音的宿主 XML 与仅有音符的 MIDI 证据不同。

## R02 与问题记录

测试 `R02_F_G_Am_4QN.mid` 和 `R02_F3_G5_AmOPEN.mid`（后者需把末和弦设 OPEN）。比较 Auto、固定 C Major、固定 A Minor；记录 Color 数量、来源及分数。模拟结果不是实际 Cubase 根因：Auto 下可能经 F Major 解释得到 Color，固定两个调性可能为空。

出现差异后，等待分析完成，使用菜单“导出本地输入与推荐诊断”，保存本地文本；同时保存原始 MIDI 或可重现的和弦拖入片段。文本包含构建状态、实际两个算法状态、输入字段/音符/起点/OPEN、调性候选、完整匹配链、默认及“查看更多”可见性、实际续写 QN。MIDI 原始音符最多记录4096个，超过时明确标注截断；拍号/速度事件也会记录。

请记录 Cubase 版本、工程拍号变化、输入入口、截图、操作顺序及实际 Preview/导出结果。完成本轮实机验收前，不据此包宣布正式发布。

## 回退

关闭宿主，移出本轮完整 VST3 文件夹，恢复备份；或运行保留的旧 Setup。保留个人库和历史 Factory；不要用删除用户数据解决扫描问题。ON/OFF 替换仅用于开发验收，恢复默认使用 OFF 包。
