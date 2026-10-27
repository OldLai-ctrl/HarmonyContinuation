# G6 / Harmony Engine V2 集中交付

## 门禁结论

2026-10-27：独立分支 `development/g6-harmony-engine-v2-20261027`，实现提交 **`03fad728769de139d5016abb02db2a7f0e151cb9`**；Release 插件 **0.11.0-dev.7 / Factory7 / 1272 条**。工程门禁通过；限定三输入及首批候选的和声结构审查显示明确改善。主观听感与真实 DAW 交互待用户验收。

从最新本地 V7 集成头 `4a0402f` 分出；其相对 `40210d3` 只有交付文档变化。dev.6/V7 原工作树与已交付安装包保留。本轮未制作安装包、未安装、未操作真实 user.db、未推送或合并 main、未建 Tag/Release。完整身份和 SHA 在[构建证明](../../HarmonyContinuation-g6-evidence/identity.json)，[构建日志](../../HarmonyContinuation-g6-evidence/ui-build.log)。

## 旧版为什么少

同一 V7 库、Auto 调性、Unified 生产权重、全量保留，`F–G–Em–Am` 的一次定向诊断：

| 关口 | 实际结果 |
| --- | --- |
| 检索范围 | 1272/1272 来源，13287 个模板–调解释对齐结果 |
| 最低整段相似度 | 354 个通过；12933 个解释被排除 |
| 末端对齐 | 352 个通过，排除 2 个 |
| 明确性质/低音冲突 | 排除 8 个，剩 344 个 |
| 剩余后缀/循环 | 26 个无后续；318 个进入路径处理，另排除 1 个原地重复 |
| 完整实际音乐去重 | 217 条独立路线 |
| 60 分资格 | 30 条通过；187 条独立路线未通过，解释级 `minimum_score` 记录为 259，两个计数不混用 |
| 用途与默认可见 | 收束21、发展0、循环7、色彩1；另保留1条探索性发展 `Dm`，默认隐藏 |

严格同音集连续来源覆盖：完整四和弦29、尾三55、尾二173。这一统计不含可兼容的扩展变体，也不表示这些来源都有剩余后缀。完整库已经被扫描，主要问题是整段匹配与后缀通道的覆盖定义、随后资格损失；末端对齐和分组不是主要数量瓶颈。用途证据把唯一的单和弦发展降为探索，进一步造成发展入口实际为空。排序不能把被资格排除的路线找回来。

旧收束以 ii–V–I/稳定主音结束为主，循环已有王道及其扩展变体；因此只增加库内完整句数量很难独立解决本案例。

## 新版实际结果

下表为完整列表的独立音乐路线数；首批优先位置与全量分页分别处理。仅保留合理路线，不补足方向配额。

| 输入 | 旧/新保留总数 | 旧/新发展可见数 | Exact | 完整输入语境 | 尾三 | 尾二 | 算法跨段 | 旧/新计算耗时 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| F–G–Em–Am | 30 / **294** | 0 / **122** | 30 | 18 | 4 | 17 | 225 | 372 / **563 ms** |
| Cmaj7–Dm7–Em7–Gsus4 | 0 / **212** | 0 / **96** | 0 | 0 | 0 | 0 | 212 | 273 / **430 ms** |
| Cmaj7–Fm6–Bb7–Ebmaj7 | 0 / **71** | 0 / **38** | 0 | 0 | 2 | 0 | 69 | 278 / **367 ms** |

F 输入新完整列表分组：收束21、发展123、循环62、色彩88；发展中的旧探索项1条继续保留并默认隐藏。Exact / 局部及完整语境 / 跨段为 **30 / 39 / 225**，约10.2% / 13.3% / 76.5%。这是不同实际后续，不等于294个独立家族，也不等于全部已经人工听评。跨段占比高，是需要重点试听的部分。

耗时为本机 Release、已建好内存索引的一次代表查询，不是压力结果或分位数。新增语境检索约116/105/72 ms；完整推荐总耗时包含排序。索引为5431条来源内部锚点和1427条终端类入口记录，共6858条；不是1272×1272的句对组合。

## F–G–Em–Am 的可试听音乐路线

以下均接在真实输入之后。标题中的“直接”指库内原句连续关系；V7 原句是编辑原创构造。

| 路线与价值判断 | 来源、通道 | 试听 / MIDI |
| --- | --- | --- |
| **Dm7–G7–Am7–Gsus4**：属功能转入vi并停在悬属，保留继续写下去的张力；发展第1。 | V7_01_020，完整输入直接后段 | [WAV](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-full-1-V7_01_020.wav) / [MIDI](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-full-1-V7_01_020.mid) |
| **Ab–Bb7–Eb**：Am后共同音C与半音移动接入Ab，后续IV–V7–I确立Eb区域；发展第2。原中心解释A小调，实际目标Eb大调；C大调仅是来源还原坐标。 | V7_01_081，完整输入直接后段；原说明明确为新Eb区域 | [WAV](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-full-2-V7_01_081.wav) / [MIDI](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-full-2-V7_01_081.mid) |
| **F–G**：短而明确的开放回升，可继续进入下一句；并不把单个Dm当成充分发展。发展第3。 | V3_CYCLE_052，尾三直接后段 | [WAV](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-three-3-V3_CYCLE_052.wav) / [MIDI](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-three-3-V3_CYCLE_052.mid) |
| **Am9–D13–Am9–Bm7–Gmaj7–D/F#–Em7**：A中心中以F#带来Dorian色彩和后段延伸；发展第4。入口延长Am的色彩，后面有独立方向，首屏不再让三条Am扩展占满。 | V7_04_042，算法跨段；后句已编辑审校，接缝无独立编辑审核 | [WAV](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-phrase-transition-4-V7_04_042.wav) / [MIDI](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-phrase-transition-4-V7_04_042.mid) |
| **C/G–Em/B–Dm/F–Gsus4–G/B–C/E–Am/C–F/A**：实际slash低音和悬属形成较长跨段构思；发展第8。 | V7_02_017，算法跨段 | [WAV](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-phrase-transition-8-V7_02_017.wav) / [MIDI](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-phrase-transition-8-V7_02_017.mid) |
| **E/G#–Am/G–D/F#–F–Em**：低音G#–G–F#–F–E构成真正不同的半音下行路线；发展第11，保留在第2页。 | V7_01_065，尾二直接后段 | [WAV](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-two-11-V7_01_065.wav) / [MIDI](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-two-11-V7_01_065.mid) |

循环首批还出现 `Am/G–Fmaj7`，与完整王道循环、长线新段落分开。上述判断针对实际和弦、低音、终点与声部接近性；没有代替用户听评。旧库家族标签不统一，排序同时使用首和弦、低音、用途、调中心和完整身份，不将标签数当作独立音乐构思数。

## 来源、资格与兼容边界

- **直接来源延续**：库内完整句确实含有匹配语境与后段；完整/尾三/尾二由连续锚点检索。输入的明确七度、六度、alteration、实际音集和显式低音都要符合来源；简单三和弦可以匹配保留其音集的扩展。
- **算法评估转接**：后句限于已准入V7编辑原创；跨句接缝另验共同音、实际低音、潜在声部运动、调中心、用途和硬旋律。大/小调相对解释与真实转调分别记录。邻接新中心需要共同枢纽和目标V–I；来源自身的跨区段落还核对实际predominant–V7–I目标。
- **独立编辑审核接缝**：本轮为0。V7的句级审核和家族/机制引用都不能冒充接缝审核或作品完整出处。没有为新增路线编造作品级证据。

旧 Unified 的相似度、60分资格和原始分数不变。新通道采用独立的连接/语境/中心/来源/演奏/独立句值资格，经过可比质量层级后排序；原始分数只在同通道比较。软20/50/30方向比例不补不合格路线；多样性只改显示顺序，完整分页保留合法后续。

六个主入口保留，发展/循环/色彩区增加轻量“灵感优先”，运行时默认开启，关闭即使用旧路径。收束入口沿用Unified；祈愿随机器和旧输入资格不接入新通道。实际界面已通过插件编译，按钮交互与真实宿主显示尚未实测。

新路线的Preview/MIDI使用**真实输入＋冻结来源后段**，不把完整后句之前的来源前段替换成用户输入的历史。旧Exact仍保持既有完整来源播放。Factory DB/HCA与dev.6构建字节一致；Factory/User Schema2、Session5、Snapshot3未改。

Snapshot3冻结实际音符、低音、扩展、时值、约束和后句来源ID；**通道、双中心及接缝理由尚不持久化**，重开明确显示冻结路线，不生成匹配轨迹或伪造来源关系。“灵感优先”的开关也只在实例运行期保存。下一次若要保存完整来源元数据，应另审Snapshot可选字段扩展；本轮未擅自修改持久化结构。

必要元数据方案建议为一个版本化的可选 `contextProvenance` 对象：输入音乐指纹、后句完整来源ID及音乐指纹、来源起点/语境长度、来源还原坐标、原/目标中心及其关系、推荐通道、接缝证据类型、评估策略版本/理由、独立审核ID（若有）。旧读取端继续依靠冻结演奏；用户输入没有库内来源ID时只绑定其音乐指纹。此方案尚未实施。

## 必要验证与下一步

复用 `FactoryV6Tests --g6-focused`，只固定上述三输入。旧输入的30条Exact逐条核对分组、实际音乐指纹及原分数保持；新直接语境在F输入核对118个来源锚点和弦，复杂输入核对6个，开放输入无直接来源语境。三输入各一个明确硬旋律冲突均被拒绝。所有算法接缝均未进入Resolve。

13组样本覆盖三输入：来源音集/低音/扩展对照、Preview非静音、MIDI构造，以及Snapshot3保存重开后的**WAV采样完全一致、MIDI字节完全一致**通过。[媒体清单与快照文件名](../../HarmonyContinuation-g6-evidence/final-check/g6-media-index.tsv)、[实际来源逐和弦记录](../../HarmonyContinuation-g6-evidence/final-check/g6-sample-audit.tsv)、[完整候选及理由](../../HarmonyContinuation-g6-evidence/final-check/g6-routes.tsv)、[门禁日志](../../HarmonyContinuation-g6-evidence/final-check.log)。

测试在本地实现提交前运行，源文件与`03fad728…`相同；测试程序内嵌的是接手HEAD `4a0402f`。后续插件使用完整`03fad728…`重新编译并核对二进制身份，仅改变构建身份和SDK开关，未再次重复音乐验证。没有执行大矩阵、压力、全量宿主、安装器或旧阶段测试。没有新增来源旋律；排序中的导向音轮廓仅衡量和弦音的连接潜力，实际试听由既有配音器演奏。

主代理负责架构、和声资格/调中心、多样性取舍、首批结构审查和最终门禁；LUNA负责既有来源核查、固定输入验证辅助和独立只读复核。现有证据支持制作独立开发包的工程条件；**按本轮边界先停在此处，优先听评以上代表路线，再决定开发包**。用户音乐验收、Cubase/FL交互和正式发布均待后续授权与实际证据。
