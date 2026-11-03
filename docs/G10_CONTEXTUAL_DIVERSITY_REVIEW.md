# G10 语境多样性与创作覆盖

## 对象与验收边界

分支 `development/g10-contextual-diversity-20261103` 从干净的G9交付 `921b7f3f2b4b4fda5e1080a402d1cf5794db3159` 建立。版本 **0.11.0-dev.11 / Factory8 / 1288**，BuildCommit **`80487ab51373e20670af656bc5f0183b107d996a`**。交付HEAD、二进制和证据哈希见同级证据目录 [identity.json](../../HarmonyContinuation-g10-evidence/identity.json)。

用户已确认G9乐句接缝与小节逻辑基本自然，整体路线仍偏相似。G10只调整合格候选的语境适配与创作多样性，G9的OPEN、完整来源、接缝/重叠资格冻结。G9的12/50为旧版结果；本轮结果与听评另记。**不制作Setup，不自动推送、合并main、建Tag或Release。**

## 一次定向诊断：主要是A/C，未证明需要扩库

使用G9原12条成功链的36轮冻结输入及来源记录，未重审Factory全库。前6轮完成后，诊断解析器发现“实际音集含低音”与“上层和弦音集”的区别；保留原显式root/quality、实际低音/音集及全部时值严格往返后，仅续跑余30轮。没有重抽样，也没有改变产品和弦识别或真实数据。

[逐层记录](../../HarmonyContinuation-g10-evidence/diagnosis-complete/g10-gates.tsv)、[完整候选及原排名](../../HarmonyContinuation-g10-evidence/diagnosis-complete/g10-candidates.tsv)、[逐轮汇总](../../HarmonyContinuation-g10-evidence/diagnosis-complete/g10-summary.tsv)。

| 层次 | G9冻结语境下的实际范围/结果 | 判断 |
| --- | --- | --- |
| V8结构门禁前合格来源/键解释 | 每轮10–20个 | 不是所有解释都能通过调中心和实际连接资格 |
| 全通道结构合格演奏身份 | 每轮38–151个，18–31个家族 | 家族数和身份数不能直接当成独立音乐构思数 |
| 完整、审校未终止、结构合格演奏身份 | 每轮10–19个 | 已存在可重排的完整后续 |
| 007 Lydian | 35轮通过结构资格，中位最佳排名47，仅1轮进首页 | A：被排序埋没 |
| 009共同音/中介中心转向 | 34轮通过，中位最佳排名58，仅1轮进首页 | A：被排序埋没 |
| 013持续低音 | 34轮通过，中位最佳排名52，仅1轮进首页 | A：被排序埋没 |
| 005半音低音线、012借用小调、016侧移 | 分别34/34/35轮通过，中位最佳排名13/33/63 | 有实际不同的库内备选 |

调中心与接缝确实排除部分解释：记录中有479条“目标中心尚未成立”、194条潜在中心/共同枢纽不符、36条实际低音/音集接缝未过的去重解释记录。这些是`query+source+key+stage`记录，阶段并非互斥，不能相加当成来源数或独立候选数。最终候选资格以真实结果对象为准。

直接反例：`F–C–Dm–Am`下002创意原分85.45、未来索引4条，排名1；013同为85.45/4条，却排57。007为84.70/4条排53，009为83.95/4条排61。**固定来源先验或连接数量不是主要差别。**旧混合排序对首和弦、低音简图、色彩标记、家族等分类特征累加惩罚，并结合机制标签和二元历史相似度筛选；近似分类可能重复扣分，整句音乐轮廓和具体输入语境没有充分参与。

C类相似也真实存在：010、003、014虽然家族名不同，常在同一相对大小调音高集合内使用相近的maj9/m9/6/add9/sus色彩。G9的29/36次集中于三来源；7/12条链全由这三来源组成，11/12共享平台、上行低音、隐主音三类词汇。所审阅的V8切片又均为8和弦、各4 QN，节奏组织的相似性没有由G10消除。

**新增来源0。** 当前有合格且变化较大的替代，先处理A/C。没有依据启动B类数据扩建；这也不意味着已证明全库在所有语境下内容充足。

## 采用的调整

1. **音乐资格先行。** 实际音符、低音、QN/OPEN、完整来源、G9结构与硬约束不变；合法候选的排序下降不记作结构失败。旧Exact原始分数与60门槛保持，创意分不与Exact原始分直接相加比较。
2. **只读的实际路线轮廓。** 在音集、低音音级运动、时间归一的8个位置、持续时间加权音高分布及张力曲线中比较路线。低音方向是音级的最短运动解释，不冒称恢复了未知的实际八度。张力来自实际半音/三全音密度及调性色彩运动，是排序线索，不是听感合格证明。
3. **具体语境适配。** 使用最近输入的音集、低音流向、已有调中心解释、可能的功能延续及张力变化；区分后句早段的延续和后段的色彩转向。持续低音若接在明确延长的句尾或既有踏板后，有不同于移动低音中泛用停留的依据；规则适用于实际低音组织，不按来源ID设黑名单。
4. **实际历史与边际多样性。** 历史使用有界4/8事件窗口的实际音集、低音与时值，不要求保留源ID。新排序比较与已选路线的最大实际相似度，避免重复分类特征的线性累计；同音集别名或移调副本不能假装成新构思。历史相似性只影响合格候选的优先级，允许主题复现和合法循环。
5. **保留稳定路线和完整访问。** 稳定/丰富/大胆仍为软方向；没有随机打乱、来源均匀配额、权重矩阵试验或按最终RCR数字回调门槛。首页优先提供不同的合格轮廓；只有2个方向时显示2个，其他合法变体从下一页访问。四入口、试听对象、输入和Snapshot契约不改。

实际小链又暴露了语境解释缺口：009后的末8和弦，Bb大调/F小调置信度分别0.137539/0.135661，但所有新中心都被迫与首解释Bb大调比较，含Db/Ab的实际末端因此被拒绝。修复后，当首解释不能涵盖末端实际音集时，仅保留既有前三个局部解释中能涵盖全部末端音符的中心，供同中心或关系大小调路线逐条检验。没有加入任意新调、伪造V–I或重写源音符；真实跨中心路线仍须原有枢纽和中心成立证据。连接理由明确标记这是备选语境解释，不能冒称已经确立转调。[失败接缝定位](../../HarmonyContinuation-g10-evidence/failed-probe/failed-probe.tsv)。三个首轮原有资格集合没有改变，G9结构/时间规则未放宽。

实现集中在`CreativeDiversity`的共享判断与正常混合排序；没有实时音频线程搜索，没有新增参数页。Factory音乐指纹与演奏数据继续使用原完整契约，轻量轮廓仅为运行期排序信息，不能取代完整音乐身份。

## 必要验证与RCR

Release插件和定向工具构建通过，版本、moduleinfo、二进制提交标记与本报告BuildCommit一致。Factory DB/HCA哈希与G9相同；G9工作树干净且HEAD仍为`921b7f3`。没有安装或加载到真实DAW，没有重跑完整SDK Validator。

[最终实现的定向检查](../../HarmonyContinuation-g10-evidence/context-checks/g10-checks.tsv)通过：

- 三个固定语境的301/269/307个合法音乐身份与原重建实体基线相同，结构资格、完整音乐与旧分数保持，排名单独比较。
- 同实际音集的符号别名及来源/家族改名不改变轮廓；历史相似度只重排，不删除合法候选或更改资格。
- 两个轮廓/六个合法对象的首屏为2，后续分页完整可达。仅检查直接受影响的分页范围，未执行G7全套UI或宿主布局矩阵。
- 三条固定小链共九轮正常选择、Preview生成、生产DAWClip完整与OPEN回导通过；没有为小链额外生成WAV。先前断路由上述局部中心解释修复，未降低门槛。
- 84事件的一次代表性完整流程为 **395.948 ms**，含`makeMatchQuery`及正常两视图推荐，复用已有索引。[计时范围](../../HarmonyContinuation-g10-evidence/context-checks/g10-checks-long-input.tsv)。此前失败定位阶段的计时不作为最终结果；本轮不作性能矩阵。

定向检查使用与最终提交相同的实现源码完成；最终构建只更新提交身份。预检查工具的原标记和保存哈希在identity中单列，不冒充最终RCR工具。本次50起点、媒体及其Snapshot身份使用最终BuildCommit的工具实际执行。

### 一次原冻结50起点复核

原起点顺序、ID与完整音乐指纹保持，样本文件SHA-256与G8/G9相同；无新随机样本，无回溯，无隐藏候选，无放宽G9门槛。选择正常混合列表中首个满足原完整开放资格、原不重复规则及G9结构资格的对象，分页按实际动态首屏计算。

| 指标 | G9 | G10 |
| --- | ---: | ---: |
| 三轮完成、结构合格 | 12/50（24%） | **12/50（24%）** |
| 所选轮次 / 实际首页可达 | 36 / 36 | 36 / 36 |
| 采用来源数 | 7 | 8 |
| 首轮采用来源数 | 4 | **7** |
| 本版本最常用三来源集中度 | 29/36（80.6%） | **27/36（75.0%）** |
| 原010/003/014集中度 | 29/36 | 16/36（44.4%） |
| 旧机制标签互异链，仅测量 | 12 | 11 |

[汇总](../../HarmonyContinuation-g10-evidence/rcr-3/g10-summary.tsv)、[50起点](../../HarmonyContinuation-g10-evidence/rcr-3/g10-start-results.tsv)、[逐轮实际输入/音符及轮廓](../../HarmonyContinuation-g10-evidence/rcr-3/g10-rounds.tsv)。36段均为算法评估的整句跨段，精确/局部/跨段占0/0/36；不能据此宣称这批证明了所有通道的三轮成功率。38个失败起点均在首轮停止，保留原样，不把低排名合法路线记成结构失败。

**判断：技术连续性达标，初始方向和色彩覆盖有改善，持续创作多样性目标仅部分达到。** 009出现12次，即每条成功链一次；010仍出现11次。其余为001×4、003×3、014×2、008×2、007×1、002×1。新的最常用三来源只是小幅分散，不能只报原三来源下降来宣布成功。多轮仍会收敛到“中心转向＋上行低音”；持续低音和半音线的自动首选覆盖尚不足。

本批完成后未据结果继续调整权重或重跑50样本。尚不能认定G10全面满足“不同起点不再普遍汇聚少数路线”。下一开发Setup的音乐质量条件**未达到**，需要用户试听新材料，同时保留上述明确的覆盖不足。

## 三轮试听

三条来自同一批正常首选，均4/4、120 BPM，每个B/C/D为完整8和弦、32 QN。下文三位后句ID均为完整来源 `V8_01_` 后缀，例如014即 `V8_01_014`、009即 `V8_01_009`、010即 `V8_01_010`。所有后句均为既有原创编辑来源，接缝仅为算法评估，未冒称作品直接延续或独立编辑接缝审核。DAWClip MIDI不携带tempo/meter元事件，宿主按给定速度拍号试听。斜线两侧表示前/后四小节。

原模板导入与“冻结音集重建”的首轮实体可能存在显式音符约束元数据差异，因此首项可不同；冻结指纹不被冒称为完整原始宿主状态。最终RCR和以下材料以真实Factory导入及正常DAWClip回导为准，原源ID始终取自实际选中对象。

### 1. 相对小调低音发展 → 半音中心转向 → 新区域上行

A=`F–C–Dm–Am`。B/C/D为 **014→009→010**。接缝QN **16/48/80（8/24/40秒）**，总112 QN/28小节。

- B：`Dm9–F/C–Bbmaj9–C6 / Dm/A–Gm9–Bbmaj7–Csus4`。
- C：`Dm9–Bbmaj7–Dbmaj7–Eb6 / Fm9–Abmaj7–Bbm9–Dbmaj7`。
- D：`Bbm9–Cm7–Dbmaj9–Eb6 / Fm9–Gm7b5–Abmaj9/C–Dbmaj9/F`。

低音从D–C–Bb的回折发展进入新的降号区域；第三段在Ab大调/F小调音域上行，末Dbmaj9/F保留开放性。投影键D小调/F大调/Ab大调是实化解释，不等于每个段落都完成确立调性的终止。

| 累计 | WAV | MIDI |
| --- | --- | --- |
| A+B | [第一轮](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-1.wav) | [第一轮](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-1.mid) |
| A+B+C | [第二轮](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-2.wav) | [第二轮](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-2.mid) |
| A+B+C+D | [完整](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-3.wav) | [完整](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-3.mid) |

### 2. Lydian悬置张力 → 中介色彩 → 新调域展开

A为原40 QN借用和声乐句`V6_B3_02_054`，首尾C各8 QN。B/C/D为 **007→009→010**。接缝QN **40/72/104（20/36/52秒）**，总136 QN/34小节，G9修复的末C时值保持。

- B：`Fmaj9–G/F–Am7/G–G/F / Fmaj7/A–G/B–Am9–Bm7b5`。
- C：`Am9–Fmaj7–Abmaj7–Bb6 / Cm9–Ebmaj7–Fm9–Abmaj7`。
- D：`Fm9–Gm7–Abmaj9–Bb6 / Cm9–Dm7b5–Ebmaj9/G–Abmaj9/C`。

B的升四级与半减七和弦形成悬置，进入C的Am9后继续向降号区域展开；D不接传统V–I结尾。投影键F大调/C大调/Eb大调，实际中心仍允许关系小调解释。

| 累计 | WAV | MIDI |
| --- | --- | --- |
| A+B | [第一轮](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-1.wav) | [第一轮](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-1.mid) |
| A+B+C | [第二轮](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-2.wav) | [第二轮](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-2.mid) |
| A+B+C+D | [完整](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-3.wav) | [完整](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-3.mid) |

### 3. Phrygian踏板起点 → 借用下属 → 更远的降号区域

A为完整`V8_01_006`：`Am7–Bb/A–C/A–Bb/A / Am9–Gm9–Bbmaj7–Gm9`。B/C/D为 **008→009→010**。接缝QN **32/64/96（16/32/48秒）**，总128 QN/32小节。

- B：`Ebmaj9–F/Eb–Bb/D–Cm9 / Abmaj7–Bb/Ab–Fm9–Gm7`。
- C：`Cm9–Abmaj7–Bmaj7–Db6 / Ebm9–Gbmaj7–Abm9–Bmaj7`。
- D：`Abm9–Bbm7–Bmaj9–Db6 / Ebm9–Fm7b5–Gbmaj9/Bb–Bmaj9/Eb`。

C的Cm9/Abmaj7向Bmaj7移动保留Eb/Bb等共同音，同时改变C/G等关键音；后段落在Ebm/Gb音域，D继续该区域的低音上行。B的拼写是实际输出；可作Cb等音解释，不改MIDI音符。仍不以单一调名冒称完成真实作品转调。

| 累计 | WAV | MIDI |
| --- | --- | --- |
| A+B | [第一轮](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-1.wav) | [第一轮](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-1.mid) |
| A+B+C | [第二轮](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-2.wav) | [第二轮](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-2.mid) |
| A+B+C+D | [完整](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-3.wav) | [完整](../../HarmonyContinuation-g10-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-3.mid) |

同目录同名`.snapshot3.json`共9个，重开后的Preview与生产MIDI一致；WAV均非静音。**三条后半仍共享009→010，不能当成三套完全独立词汇。** 最终音乐效果须用户试听；本轮交付保留该局限，不以来源名、轮廓阈值或RCR通过替代听感验收。

## 保护、分工与Skills

G9/dev.10工作树与产物、Factory V8、生产安装及真实user.db保留。Session5/Snapshot3不迁移；新增轮廓不持久化，快照仍冻结实际演奏。Exact、Upgrade、Wish及四入口语义不改。仅为“首页不凑相似变体、余项仍可翻页”调整必要分页接线，不复跑G7全套UI。

主代理负责根因、音乐/架构裁决、核心实现、权重选择、验证执行和门禁；LUNA负责冻结数据整理、确定性UI接线及必要测试辅助。实际使用`deja-search`（历史决定检索）、`diagnosing-bugs`（冻结反例/定位/回归）、`codebase-design`（共享核心判断接口）、`stop-that-shit`（执行范围与最小检查）。查看了当前Skills清单与可发现工具，未发现专用音乐/C++/CMake/MIDI/SQLite技能入口，使用现有工程工具；未安装额外技能。
