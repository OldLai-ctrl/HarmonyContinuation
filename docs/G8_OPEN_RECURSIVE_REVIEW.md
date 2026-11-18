# G8 完整开放乐句与连续三轮续写

2026-10-30交付核对：**工程与RCR-3通过：唯一一批50起点中46条连续完成三轮（92%），46条成功链三轮均在第一页。人工创作价值听评与真实DAW验收待完成，未制作Setup。**

## 身份与保护范围

独立工作树 `HarmonyContinuation-g8/`，分支 `development/g8-open-recursive-20261027`，从G7交付HEAD `7a7021e67933899956e81d34f9b27ce58321190d`建立。开发版本 **`0.11.0-dev.9`**，产品源码/插件及本批RCR对象BuildCommit **`0936a4837ed669162fc4554e5fa86ce397a5f3b4`**。后续提交仅含验收记录及固定记录的媒体补导出工具，不改变已验收的产品算法、内容或RCR结果。G7工作树保持干净，其BuildCommit `58dfce0fb78c712def4902d81b8103747b746ea2`和dev.8产物未变。

Release插件构建完成，自动SDK Validator为47/47；二进制包含完整BuildCommit和dev.9版本字符串。插件SHA-256为`82C1421CBF4571A4403B6B3BC42DA8CBAD6AC4A161EE65DDF10319E8DC1716B6`。这证明构建身份与SDK检查，不构成真实宿主音频验收。[最终构建日志](../../HarmonyContinuation-g8-evidence/g8-final-build.log)。

[完整身份、文件SHA及分层验收状态](../../HarmonyContinuation-g8-evidence/identity.json)区分产品BuildCommit、交付HEAD和原RCR执行对象。

Factory V8为独立Schema2内容增量：V7的1272条加16条完整新句，共1288条。重建的V7中间DB与G7冻结DB SHA-256相同（`C2DA3C51…666C`），原HCA字节相同（`9E6B7E70…50B1`）。V7源码未修改，既有HCA直接保留；新句的结束审校不冒充严格调式归属审核。

## 完整内容与结束行为

[V8编辑源与编码](../data/factory-v8/encode.py)、[完整事件及来源审校](../data/factory-v8/metadata.json)只含新增内容。16条均为独立完整编辑原创，每条8个和弦、32 QN，保留全部低音、扩展音、时值及原始结束；并非已有收束句删尾，也未用移调或节奏副本扩数。

共13个内容家族。其中15条归为开放/未终止，覆盖12个家族；`V8_01_015`保留末尾`Cm9–Fm9`及全部时值，因局部小调到达感保守降为阶段性停顿，不计开放增量或RCR-3资格。两条循环句仍有独立完整展开，后续测试不允许反复抽同一音乐身份。

结束行为支持明确收束、转调后收束、阶段性停顿、开放未终止、悬置、回接循环、调式展开、中心转换过程及未确认。它与用途、来源证据、真实下一段机会分别记录；实际终止证据优先于开放标签。中心转换过程不冒称已有明确目标主调。

平台循环、持续音、半音中介及段落转换的机制参考分别来自[Duinker](https://www.mtosmt.org/issues/mto.19.25.4/mto.19.25.4.duinker.html)、[Hudson与Wang](https://www.mtosmt.org/issues/mto.25.31.4/mto.25.31.4.hudson_wang.html)、[Li](https://mtosmt.org/issues/mto.26.32.1/mto.26.32.1.li.php)。这些引用支持家族或机制，**不构成16条完整演奏的作品转录证据**。主代理完成结构审校，用户听评待完成。

## 检索与可发展性

灵感通道使用最多8事件的近期乐句窗口与尾4/3/2锚点，局部严格检查实际音符、低音及连接，较早历史的调中心仅补充解释。输入及已知时值不改；OPEN延长取近期节奏依据，完整历史继续参与硬性旋律约束检查。

跨段只接完整来源句，内部锚点只取到原始来源结束。灵感过滤旧Exact中停在来源中间的部分循环回绕，匹配优先原有循环及评分语义保持；下一完整周期仍合法。正常worker复用已算好的Exact证据，灵感不新增随历史增长的整段动态规划。普通可信结束仍保留在列表中。

离线连接索引只评估已审校完整开放子集，同mode/已知meter兼容、不同家族，每末端最多四个不同入口。运行时再次检查实际低音和音集，后续约束下一轮重新验证。**后句有来源，接缝为算法评估，未独立编辑审核。**索引既不保证未来旋律约束一定可接，也不能代替三轮实际推荐。

混合排序增加完整结构、可靠结束行为及具体后续连接机会的展示权重，仍保留原独立资格和多样性；新奇度不能修复不合理接缝。四入口、旧Exact原始分数及60分门槛、升级和祈愿抽样算法保持。

## 回导、容量与兼容

正常导入及输入加续写的组合演奏支持最多128事件；单条来源句与单轮后缀仍最多64。Session5沿用u8计数，Snapshot3与Factory Schema2不迁移。新代码可读旧会话/快照；旧dev.8读取超过64事件的新会话/快照仍会拒绝。升级、祈愿及保存为单条用户库乐句的原容量契约保持。

RCR通过正常DAWClip MIDI实际音符回导，不借助隐藏候选、特殊资格门槛或理想和弦数组。本批采用现有公开的“末和弦OPEN”导入选项；A及每轮累积输入的最后和弦作为OPEN处理，延长采用近期节奏建议，B/C/D来源句本身的8和弦与32 QN不裁剪、不缩放。拍号沿用A的已知拍号，不把非4/4起点改成4/4。

DAWClip不显式携带和弦root/quality；同音集允许不同和弦解释。验收严格比较生产导出的实际音集、最低低音、起点、时值和OPEN，另记录解释差异；明确的输入字段在查询前后保持，来源与Snapshot的符号身份仍严格比较。本批138轮中68轮记录了至少一处符号重解释，例如`C/Bb→C7/Bb`；它们没有被伪装为原输入符号，实际发声音集未丢失。该工程流程不等同于真实宿主重导出实测。

## 必要验证与RCR-3

复用现有FactoryV6Tests入口，新增仅供本轮的`--g8-checks`和`--g8-focused`，不执行旧G6十三项、G7整套UI、全库旧内容审核或安装器测试。

录入核对及定向检查发现并修复：结束标签读写格式不一致、A小调F/F#及G/G#音级映射、实际持续低音和独立色彩音未计入语境接缝音集。最后一项曾使`Bbmaj7→C/Bb`被错误拒绝，现尾三案例实际通过。MIDI验收也按文件所携带的实音与推断符号区分，未通过改动实音或降低资格来消除失败。

| 必要检查 | 结果与范围 |
| --- | --- |
| 新内容与投影 | 16句、128事件全部通过音级/根音/低音/音集/时值核对；开放15句、12家族，阶段停顿1句排除。 |
| 末端检索 | 无关历史后的尾4/3/2分别实际取得语境后续；原始来源结束保持。 |
| 音乐反例与硬约束 | G7第4条`Ab–Bb7–Eb`不再被认定未终止；OPEN与硬旋律冲突被拒绝。 |
| 长输入与兼容 | 84事件查询取得233个合格对象；84+8事件MIDI正常回导、Session5及Snapshot3往返通过。 |
| 成功链身份 | 自动导出的3条成功链、每条3轮，共9份原始捕获Snapshot3，重开后的Preview与生产MIDI字节一致；离线WAV非静音。 |
| 未重复项 | 未重跑G6十三组、G7整套UI、旧Factory音乐审校、安装器生命周期、十二调矩阵或压力测试。 |

[最终定向检查](../../HarmonyContinuation-g8-evidence/checks-final/g8-checks.tsv)。最终源码提交前只另修正了复用Exact统计时重复计数的诊断字段；音乐检索/资格/演奏行为的上述有效证据复用，未无故重复整组。

RCR实际使用`std::mt19937`种子`20261027`，有效池1288条（按库内规范化完整身份去重），不放回抽50。先写冻结抽样清单，再按正常混合分页顺序取首条合格完整未终止后续。不回溯重选，不重复A/B/C/D音乐身份或来源，B/C/D不重复家族；未按拍号筛掉起点。

**结果46/50、92%，达到10/50最低目标；138轮全部来自正常第一页的算法评估跨段，未使用隐藏候选。**旧Exact与局部通道继续保留，但本批三轮成功链实际全部选到完整跨段句，不能把该结果冒称三种来源通道各自的成功率。不把单批92%宣称为全库高精度估计。

四个失败均在首轮没有合格完整未终止后续：`V6_B3_02_010`（7/4）、`V6_B3_02_019`（5/4）、`V6_B3_01_030`（3/4）、`V6_B3_02_029`（6/8）。本轮新句全为4/4，已知拍号相容规则排除将它们强行套入这些起点；这构成当前内容覆盖缺口，没有为通过验收改拍号或重抽。未追加逐候选大审计。

[唯一批次日志](../../HarmonyContinuation-g8-evidence/g8-rcr-3.log) · [50起点冻结清单](../../HarmonyContinuation-g8-evidence/rcr-3/sampled-starts.tsv) · [50条结果](../../HarmonyContinuation-g8-evidence/rcr-3/g8-start-results.tsv) · [138轮来源、分页、完整和弦及实际输入指纹](../../HarmonyContinuation-g8-evidence/rcr-3/g8-rounds.tsv)。原验收可执行文件保存在同级证据目录，防止后续媒体工具改动覆盖执行对象。

一次代表性推荐：`V3_CYCLE_027`第3轮，实际输入20个和弦，约 **96.813 ms**，整轮导出回导约100.646 ms。推荐计时含分析、旧Exact复用及新语境/混合列表准备，索引已建立；不含WAV渲染，不是性能分布或DAW界面响应实测。

## 连续路线与试听

每个B/C/D都是一条完整的8和弦、32 QN来源句。以下材料包含实际OPEN输入A及完整B+C+D；全量来源与每轮后续在上述138轮表内。

1. **平台展开→借用下属→自然小调回环**。A=`F–C–Dm–Am(OPEN)`，来源链`V3_CYCLE_027 → V8_01_002 → V8_01_008 → V8_01_014`。B为`Bbmaj9–Dm9–C6–F/A–Bbmaj9–Gm9–Am7–Csus4`；C引入`Ebmaj7–F/Eb`的借用色彩；D进入G小调音域，末`Fsus4`保留回接潜力。[完整WAV](../../HarmonyContinuation-g8-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-3.wav) / [MIDI](../../HarmonyContinuation-g8-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-3.mid) / [原捕获Snapshot3](../../HarmonyContinuation-g8-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-3.snapshot3.json)。

2. **自然小调抬升→隐主音展开→平台回环**。A=`C–Bb/D–F/A–Eb/G–Fm/Ab–C/G–F–C(OPEN)`，来源链`V6_B3_02_054 → V8_01_014 → V8_01_003 → V8_01_002`。B从`Dm9–F/C–Bbmaj9`展开；C以`Bbadd9–C/Bb–Am7–Dm9`继续；D进入`Fmaj9–Am9–G6–C/E…Gsus4`。与上一链的来源顺序及低音组织不同。[完整WAV](../../HarmonyContinuation-g8-evidence/rcr-3/representative-paths/3-V6_B3_02_054/V6_B3_02_054-round-3.wav) / [MIDI](../../HarmonyContinuation-g8-evidence/rcr-3/representative-paths/3-V6_B3_02_054/V6_B3_02_054-round-3.mid) / [原捕获Snapshot3](../../HarmonyContinuation-g8-evidence/rcr-3/representative-paths/3-V6_B3_02_054/V6_B3_02_054-round-3.snapshot3.json)。

3. **多利亚持续低音→平台抬升→借用和声流动**。A=`C–Dm–G–C–F–G–C(OPEN)`，固定批次第24个起点，来源链`FUNCTIONAL_015 → V8_01_004 → V8_01_002 → V8_01_008`。B完整为`Am9–D/A–G/A–D/A–Am7–Cmaj7/E–D9/F#–Bm7`；C以`Cmaj9–Em9–D6–G/B`展开；D为`Cmaj9–D/C–G/B–Am9–Fmaj7–G/F–Dm9–Em7`。[完整WAV](../../HarmonyContinuation-g8-evidence/recorded-replay-24/FUNCTIONAL_015-sample-24/FUNCTIONAL_015-sample-24-round-3.wav) / [MIDI](../../HarmonyContinuation-g8-evidence/recorded-replay-24/FUNCTIONAL_015-sample-24/FUNCTIONAL_015-sample-24-round-3.mid) / [重放来源与指纹](../../HarmonyContinuation-g8-evidence/recorded-replay-24/FUNCTIONAL_015-sample-24/replay-info.txt)。这条补充材料从已有三轮记录重放，不重新查询、不增加RCR样本或成功计数；逐轮实际回导指纹与原记录完全相等，未伪造未保存的原始排名或Snapshot。[补导出日志](../../HarmonyContinuation-g8-evidence/g8-recorded-replay-24.log)。

WAV按120 BPM渲染；MIDI为不改宿主速度的DAWClip，建议在120 BPM、4/4工程中对照。每个目录同时保留第1、2轮累计材料，可以分段比较进入下一句的自然度。

自动导出的另一条`V6_B3_02_070`与路线1使用同一组B/C/D来源，仅调高不同，保留作移调对照，**不作为独立创意路线计数**。

本批自动首选共使用9个新增来源、10种三段来源顺序；138段中100段集中在`V8_01_002/008/014`。这提示自动首选仍有家族集中，不等于用户浏览全部候选后的分布，也不足以宣布创作启发最终通过。需要听评持续展开感、跨段是否自然、重复家族是否仍令人缺乏后文灵感。

## 当前交付边界

主代理负责音乐定义、数据采纳、检索/资格/排序、持续低音缺陷裁决与最终门禁；LUNA负责录入、身份辅助及有界验收实现/执行。工程与连续三轮门禁通过，新增音乐的结构审校完成；人工听感与真实DAW仍待验收。工程具备后续隔离集成基础，**暂缓制作交付Setup**，不进入RC或正式发布。

未运行生产安装，未操作真实user.db，未推送、合并main、建Tag或发布Release。音乐与连续续写门禁通过前不制作新的正式交付Setup。
