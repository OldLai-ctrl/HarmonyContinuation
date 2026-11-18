# G12 乐句衔接与现代和声表达

## 交付判断与身份

**工程及连续续写技术门禁通过；句尾证据已纠正，音乐接续感与现代风格仍待用户听评。不制作Setup。** 本轮没有证明所有不自然连接已解决，也没有把中性合成器演示当成J-Pop/ACG成品。

独立分支 `development/g12-phrase-flow-20261104`，基于干净G11交付 `797c15f3d84e74efac4c3b3fcda5758355b57264`。版本 **0.11.0-dev.13 / Factory V8 / 1288**，BuildCommit **`499c69b3cbd94ce68f35921d247d39acfe5fbd89`**。最终HEAD与插件、数据、样本及媒体校验见 [identity.json](../../HarmonyContinuation-g12-evidence/identity.json)。新增Factory来源 **0**；DB/HCA与G11逐字节一致。

## 三条旧试听为什么突然结束

只调查G11保存的三条链。用户最新定位为：“都是最后两个和弦，我觉得应该在句中的，但突然就结束了”。因此主要检查D尾与文件结束，同时核实全部九个内部接缝，未扩大为全库审查。[实际MIDI/音频时间线](../../HarmonyContinuation-g12-evidence/diagnosis/g11-three-chain-timeline.tsv)、[来源事实](../../HarmonyContinuation-g12-evidence/diagnosis/source-facts.tsv)。

| 原起点 | 旧A→B / B→C / C→D位置（QN） | D实际最后两个和弦 | 全长 |
| --- | --- | --- | --- |
| V3_CYCLE_027 | 16 / 48 / 80 | Dbmaj9/F → Gbmaj9/Bb（010） | 112 QN，28小节 |
| V6_B3_02_054 | 40 / 72 / 104 | Bbm9 → Dbmaj7（009） | 136 QN，34小节 |
| V8_01_006 | 32 / 64 / 96 | Abm9 → Bmaj7（009） | 128 QN，32小节 |

全部为120 BPM、4/4；九个接缝在第1拍，音符结束与下句起点相接，无空隙、重叠或被缩短的时值。D均保留八个原始事件，最后两个各4 QN。第二链原A首尾8 QN均保留。WAV在最后note_off后有100 ms释放、总尾部150 ms；没有发现数值截音。**时间连续只能排除这些工程故障，不能证明音乐自然。**

009/010原审校主要依据八事件来源完整、没有最后主和弦终止；尚缺“为什么恰在第八事件形成阶段边界”的独立依据。010尾部仍以等长节奏推进，009的色彩转接仍等待后文确立中心。它们也可能在具体旋律与段落中形成局部到达，不能一概判成错误；但不能自动承诺独立完整的开放句末。文件在这里停止会放大尚在推进的听感。内部接缝的主观自然度仍需听评，不能因本轮没有时间故障就一并判PASS。

## 实际修订

新增四条轻量编辑审校记录 [g12-reviews.json](../data/phrase-flow/g12-reviews.json)，绑定拍号、完整实际根音/低音/质量/音集/时值签名。CMake离线生成常量索引，无运行时文件搜索、递归或持久格式迁移。只在Factory索引副本标注，不改数据库音乐或原标签。

- **009/010：** 保留全部来源、原结束类型、音符与分页访问；撤回其“已证明独立句末”的高置信资格，作为持续运动的探索材料。修改依据是明确的句尾证据不足，并非出现频率；不是删来源或禁止继续接入E。
- **V7_04_044：** 原有7事件、32 QN：`Cm7–F/C–Bb/C–Cm7 / Eb/G–Dm/A–F/A(8)`，未标处各4。前16 QN持续C低音，后16 QN离开平台，Dorian IV6长停两小节，支持模态阶段停顿，仍期待后文。
- **V7_04_052：** 原有7事件、32 QN：`Cm7(8)–Db/C–Cm7 / Bbm7–Abmaj7–Db–Cm7/G`。前16 QN长主和弦与降二级持续低音色彩，后16 QN回到同一小调背景。用途为有目的循环归位，不能冒称没有回归的独立开放序列。

044/052的4+4组织具有独立编辑说明，支持其真实非等长时值的OPEN识别及结构资格；没有令所有32 QN来源自动合格。未经同类审校的V7_03_008仍完整42 QN，不补拍、不裁句、不放宽G9边界。共同音、低音和调域评估仍只是算法接缝依据；四条审校均标明人工听评待定。

未更改评分权重、来源轮换、通用“现代加分”、全局Voicer或音频时长。旧Exact/60、四入口、升级/祈愿、G9接缝/时间及G10历史/G11分叉算法保持。新的边界证据会改变创意资格与实际选中路线，故不能声称候选与G11完全相同。Session5/Snapshot3不迁移，旧冻结演奏照常恢复。

## 现代风格不足的判断

- **C1，未证明必须扩库：** 三条已有现代调式、借用、持续低音与色彩中心运动，不是普通II–V–I占主导；本轮不足以证明整库缺少现代语汇，因此不增数据。
- **C2，确有覆盖盲点：** 已有044/052具有不同和声节奏，但缺少独立结束/分组证据，未作为高置信完整发展句参与当前选择。本轮针对实际音乐补审，不靠风格标签提分。
- **C3，仍有限制：** 原选中V8句普遍8×4 QN，逐小节均匀变化；复杂和弦不能代替段落、节拍、张力与旋律组织。新的正常首选链仍多为这种时值，044/052未成为冻结批次首选，不能据此宣布和声节奏覆盖已经充分改善。
- **C4，呈现有影响但未经听评量化：** 当前中性Synth约85%正弦、15%三角波，持续和弦块、固定力度与既有声部规则，没有真实流行配器、旋律或律动。保持此呈现以保护已冻结MIDI/Preview，不把换音色当作和声修复。低音音级连续也不等于实际音区连续；既有Voicer可能发生八度回置。

参考 [Li 2026的动漫歌曲调中心与曲式分析](https://mtosmt.org/issues/mto.26.32.1/mto.26.32.1.li.php)、[Duinker 2019的调性平台分析](https://www.mtosmt.org/issues/mto.19.25.4/mto.19.25.4.duinker.html)、[Hudson/Wang 2025的持续低音分析](https://www.mtosmt.org/issues/mto.25.31.4/mto.25.31.4.hudson_wang.html)：这些研究支持结合时值、位置和段落理解功能，不能为某条原创句自动提供完整作品证据。V7_03_008只有Folder5《Believe》机制映射，不是歌曲逐音摘录；044/052及本轮V8后句均为原创编辑材料。

## 先听：原D→真实E边界对照

以下各文件从**完整D起点**开始，保留其32 QN全部音符，再加完整E；总64 QN/16小节/32.15秒。**12–16秒为原最后两个和弦；16秒进入E。** 用来判断尾部在后文中如何交接，不能只因多播一段就宣布自然。

| 对照 | 完整D → 完整E | 正常可见排名 | WAV / MIDI |
| --- | --- | --- | --- |
| 1 模态阶段停顿 | 010（Db坐标）→ V7_04_044（Bb小调坐标） | 8，第一页 | [听](../../HarmonyContinuation-g12-evidence/boundary-auditions/V3_CYCLE_027-D-plus-E-focus.wav) / [MIDI](../../HarmonyContinuation-g12-evidence/boundary-auditions/V3_CYCLE_027-D-plus-E-focus.mid) |
| 2 小调线条发展 | 009（F小调坐标）→ V8_01_005（F小调坐标） | 8，第一页 | [听](../../HarmonyContinuation-g12-evidence/boundary-auditions/V6_B3_02_054-D-plus-E-focus.wav) / [MIDI](../../HarmonyContinuation-g12-evidence/boundary-auditions/V6_B3_02_054-D-plus-E-focus.mid) |
| 3 Phrygian循环归位 | 009（Eb小调坐标）→ V7_04_052（Eb小调坐标） | 14，第二页 | [听](../../HarmonyContinuation-g12-evidence/boundary-auditions/V8_01_006-D-plus-E-focus.wav) / [MIDI](../../HarmonyContinuation-g12-evidence/boundary-auditions/V8_01_006-D-plus-E-focus.mid) |

1的接缝为Gbmaj9/Bb→Bbm7，低音Bb保留，共同音延续、额外Gb进入F；E为`Bbm7–Eb/Bb–Ab/Bb–Bbm7 / Db/F–Cm/G–Eb/G(8)`，末两小节Dorian停顿。2为Dbmaj7→Fm9局部小调到达；E为`Fm9–FmMaj7/E–Fm7/Eb–Bb9/D / Dbmaj7–Cm7–Bbm9–Eb6`，各4 QN，低音F–E–Eb–D–Db–C展开，实际音区有回置。3为Bmaj7→Ebm7长停；E为`Ebm7(8)–E/Eb–Ebm7 / Dbm7–Bmaj7–E–Ebm7/Bb`，以降二级持续低音和循环回接组织后文。均未插入固定终止。

这是从正常合法结果中按明确音乐目的选择的**两句听评拼接**，不是默认首选、第四轮RCR成功或单一Factory来源，也没有写成虚假复合Snapshot。原D前32 QN逐音保持，旧三条Snapshot重开MIDI逐字节相同；选中E-044另有真实输入与候选的 [Snapshot3](../../HarmonyContinuation-g12-evidence/boundary-auditions/V7_04_044-selected-E.snapshot3.json)。完整证据见 [对照清单](../../HarmonyContinuation-g12-evidence/boundary-auditions/g12-auditions.tsv)。

## 一次冻结50起点与工程检查

最终构建之后只运行一次原冻结50起点，生产MIDI累计回导，每轮重新执行真实资格，无回溯、重抽或特设参数。[汇总](../../HarmonyContinuation-g12-evidence/rcr-3/g12-summary.tsv)、[50起点](../../HarmonyContinuation-g12-evidence/rcr-3/g12-start-results.tsv)、[36轮对象与接缝](../../HarmonyContinuation-g12-evidence/rcr-3/g12-rounds.tsv)。

**12/50（24%）完成三轮，均三轮第一页可达；38条首轮无合格完整开放后续。** 11条三轮机制互异；第三代表链回用了宽泛的tonal-open-plane机制，因此不声称全部路径机制互异。保守只计互异机制链也为11/50（22%），不替换原RCR分母或判定。G11的12/50历史记录保持。36轮仍使用11个来源，011出现11次；本轮没有针对频次再调参。不能用来源分散或通过率证明听感成功。

G9/G10历史识别、G11分叉及多目标展示代码保持；这批仍有不同中心、持续低音和悬置方向，但一个宽泛机制回用与011集中是限制，不能笼统宣布多样性全面提高。

- Release插件与专项工具构建通过，BuildCommit相同。
- [8项直接检查](../../HarmonyContinuation-g12-evidence/checks/g12-checks.tsv)通过：四条身份绑定/音乐保持；实际时值变化不继承审校；044真实OPEN回导末8 QN保持；未经审校42 QN不放行；84事件时间与正常分页。
- 3组旧Snapshot/MIDI字节一致；3个D/E对照旧D逐音一致；1组新E-044 Snapshot/Preview/MIDI一致；3条新代表链共9组Snapshot/Preview/SMF及DAWClip身份一致，随媒体导出检查，未另跑历史矩阵。
- 代表84事件，复用CandidateIndex，包含makeMatchQuery及正常推荐，**372.897 ms**，260个实际候选均可分页。仅一次测量，输入与G11计时范围有差异，不作为严格速度对照。
- 未重复旧全库、UI矩阵、安装器或宿主检查；真实DAW未验收，生产插件与真实user.db未操作。

## 三条正常首页连续首选新链

以下均来自唯一复核中正常第一页连续选择，无试听回溯。全部120 BPM、4/4；B/C/D均完整8事件、32 QN，各事件4 QN。来源为原创编辑，接缝为算法评估，未冒称真实作品直接延续。符号可能随MIDI重解释变化，实际音集、低音与时值以冻结对象为准。

### 1 色彩中心移动→平台→悬置

完整来源：**V3_CYCLE_027 → V8_01_016 → V8_01_002 → V8_01_011**。

- A：`F–C–Dm–Am`，16 QN。
- B：`Fmaj9–Am7–Dbmaj9–Cm9 / Bbmaj9–Gm9–Gbmaj7–Abadd9`。
- C：`Abmaj9–Cm9–Bb6–Eb/G / Abmaj9–Fm9–Gm7–Bbsus4`。
- D：`Bbm11–Ebsus4–Cm11–Fsus4 / Dbmaj9–Gbadd9–Ab/Eb–Bbm9/F`。

接缝16/48/80 QN，即第5/13/21小节；总112 QN/28小节。B的色彩转接在C中获得平台解释；C末Bbsus4进入D的Bbm11保留中心音并引入小调色彩。D结束是悬置低音支撑的阶段边界，不是已完成终止；是否形成足够自然的停顿仍待听评。

[完整WAV](../../HarmonyContinuation-g12-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-3.wav)；累计MIDI：[A+B](../../HarmonyContinuation-g12-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-1.mid)、[A+B+C](../../HarmonyContinuation-g12-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-2.mid)、[A+B+C+D](../../HarmonyContinuation-g12-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-3.mid)。

### 2 持续低音→相对小调→悬置序列

完整来源：**V6_B3_02_054 → V8_01_013 → V8_01_014 → V8_01_011**。

- A：`C(8)–Bb/D–F/A–Eb/G–Fm/G#–C/G–F–C(8)`，未标处各4，完整40 QN。
- B：`Fmaj9/C–G/C–Am7/C–Bbmaj7/C / Csus4–Bb/C–Am7/C–Gm9/C`。
- C：`Dm9–F/C–Bbmaj9–C6 / Dm/A–Gm9–Bbmaj7–Csus4`。
- D：`Gm11–Csus4–Am11–Dsus4 / Bbmaj9–Ebadd9–F/C–Gm9/D`。

接缝40/72/104 QN，即第11/19/27小节；总136 QN/34小节。长C低音上的上方色彩运动转入相对小调组织，Csus4→Gm11保留C/G而改变其支撑。D尾继续悬置，不承诺下一段必然可用。这条与第一条共享011，不能仅因调高不同就当作新的末句机制。

[完整WAV](../../HarmonyContinuation-g12-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-3.wav)；累计MIDI：[A+B](../../HarmonyContinuation-g12-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-1.mid)、[A+B+C](../../HarmonyContinuation-g12-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-2.mid)、[A+B+C+D](../../HarmonyContinuation-g12-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-3.mid)。

### 3 调式借用→悬置→循环张力

完整来源：**V8_01_006 → V8_01_008 → V8_01_011 → V8_01_001**。

- A：`Am7–Bb/A–C/A–Bb/A / Am9–Gm9–Bbmaj7–Gm9`，32 QN。
- B：`Ebmaj9–F/Eb–Bb/D–Cm9 / Abmaj7–Bb/Ab–Fm9–Gm7`。
- C：`Fm11–Bbsus4–Gm11–Csus4 / Abmaj9–Dbadd9–Eb/Bb–Fm9/C`。
- D：`Abmaj7–Bb6–Cm7–Eb/G / Abadd9–Cm7/G–Fm9–Bbsus4`。

接缝32/64/96 QN，即第9/17/25小节；总128 QN/32小节。上方借用色彩转向悬置序列，再由Fm9/C交给Abmaj7形成张力回收与新平台。D末Bbsus4期待循环或后段，保留完整句尾，不虚构终止。B/D同属宽泛平台机制，当前仅能作为有不同局部张力的再用，不能宣称三轮完全异质。

[完整WAV](../../HarmonyContinuation-g12-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-3.wav)；累计MIDI：[A+B](../../HarmonyContinuation-g12-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-1.mid)、[A+B+C](../../HarmonyContinuation-g12-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-2.mid)、[A+B+C+D](../../HarmonyContinuation-g12-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-3.mid)。

新链可与[G11原三链](G11_CONTINUATION_NETWORK_REVIEW.md#三条真实首选试听链)按同一起点比较。当前三个自动代表仍有共享材料、等长组织与开放EOF限制；边界对照另展示已有非等长来源的可能性。**用户听评前，G12音乐接续感与现代J-Pop/ACG适用性保持待验收，尚不具备进入Setup的最终音乐条件。**

## 实际使用的Skills与分工

已确认并读取：`deja-search`检索既往约定（未发现可替代本轮诊断的G12既有修复）；`diagnosing-bugs`围绕三条已知音乐与真实时间线定位；`codebase-design`把边界审校限制在现有索引/资格接口；`stop-that-shit`约束范围与验证次数。没有声称安装了专门音乐、C++或SQLite Skill。

主代理负责音乐裁决、四条编辑审校采纳、核心修改及最终判断；LUNA负责实际时间线、来源事实、确定性检查和媒体辅助。未安装新Skill、操作用户数据库、制作Setup、推送、合并main、建Tag或Release。
