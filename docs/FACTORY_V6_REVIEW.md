# Factory V6 工作包 B：B3 集中评审

核验对象：B3批次01，2026-10-25，独立Release工具与冻结数据；当前裁决：用户已批准 B3 连续内容建设。完成旧 56 条分级及一批 110 条手工候选；本轮隔离采纳 132 条（旧45＋新87），实验库 **790 条**。全部音乐 A 级内容若收录则为810条，超过800 shortlist输入预算；保持20条A预算待决，暂停扩大收录。受测810对照尚未出现结果召回丢失，不据此宣称未来千条公平。正式活动库仍为V5。

## 当前交付与计数口径

| 对象 | 实际状态 |
| --- | --- |
| 分支 / 接手 | `development/factory-v6-20261024` / `d9d4539`；本轮仅本地提交 |
| V5基线 | Library5 / Schema2 / 658 IDs，649个跨移调完整音乐身份，原内容不改 |
| V6全部提案 | 170候选，连同V5为828条proposal；含待审，不可作活动卡池 |
| 音乐分级 | A152 / B3 / C15；其中20A因预算隔离 |
| V6隔离采纳 | 132候选，连同V5为790条experiment；781个跨移调完整音乐身份、741个低置信核心家族桶；Library6 / Schema2 |
| 用户听评 | 已听首轮10组，未发现明显音乐问题；风格准确性未确认 |
| 本批听评 | 主代理逐句符号/中心/声部连接审查；针对5条生成新媒体，未声称新增人工音频听评 |
| 集成状态 | 核心API定向通过；未重建插件、未安装、未激活；DAW/UI与安装生命周期留在B4 |

[全部候选逐条表](../data/factory-v6/CANDIDATE_REVIEW.md)、[音乐数据](../data/factory-v6/candidates.json)、[来源/归属/分级/关系元数据](../data/factory-v6/metadata.json)、[采纳ID](../data/factory-v6/approved-ids.txt)是当前数据入口。[B1覆盖审计](../data/factory-v6/audit/REPORT.md)与下方首轮工程证据是冻结历史；其中旧准入/听评状态不代表当前决定。

## 旧56条与来源裁决

初审36A/11B/9C，补充来源并降低标签后为 **45A/2B/9C**。原4条待审另列C，持续隔离。A表示完整原创音乐、中心解释及受限标签达到本轮准入要求，不表示风格已经用户听评或发表频率已知。

- 016/017/018：补Phrygian原作者教学语境，风格降为rock应用建议；036/045/056：补爵士旋律小调原始教学材料；044/055/058：撤掉Jazz归属，改为functional用途。
- 025、052保留B：风格与混合调式证据仍不足。024/026/042/047/048/049/050/053/054无可核验全句membership，保留C。
- 046/057/059/060继续待审；Locrian与古典双向旋律小调不为补齐配额晋级。
- NeoSoul、JazzFusion、Ambient旧家族名改成ExtendedKeyboard、FunctionalChromatic、PedalColor，避免家族名代替流派证据。132条采纳记录的styleEvidence均为editorial-application；常用程度统一unrated，并说明无发表频率证据。
- 所有外部链接都是**概念支持**，完整原创进行直接引用数为0。来源类型、引用范围、标签改动和主代理决定分别保存；版权教学谱、商业MIDI及数据集未复制入库。

[56条来源逐项核验](../data/factory-v6/b3/SOURCE_REVIEW.md)保留初审与补充裁决；[旧60条分级台账（含原56）](../data/factory-v6/b3/b1-grading.json)和metadata记录最终结果。此前听过的001/011/016/017/025/032/045/050/059/060精确标为user-representative-pass，其他记录仍pending-human；听过的B/C项仍可隔离。

## 新增批次与覆盖

[110条手工蓝本](../data/factory-v6/b3/batch01.tsv)按独立完整句设计后确定性编码，没有排列组合扩写。新批次107A/1B/2C，采纳87A、保留20A预算待决。070的sus4与独立三音低音保持B；073与B1_039、079与V3_BLUE_007只有扩展/音集差异，保留C。各条完整和弦、低音、扩展音集、QN、拍号、中心、归属与严格外音可追溯。

| 新增采纳结构方向 | 条数 | 证据边界 |
| --- | ---: | --- |
| Dorian / Mixolydian / Lydian / Phrygian | 12 / 10 / 10 / 8 | 逐句中心与特征音核对；风格为有限应用建议 |
| 当代流行借用与开放句 | 12 | 不依赖单一V-I；含非传统回接与低音连接 |
| 延展键盘长线 | 17 | maj9/m9/m11/13/sus/slash等；未确认为Neo-Soul、R&B、City Pop流派 |
| 功能/半音爵士应用 | 10 | 副属、替代属、后门、变化属等；原创用途判断 |
| 踏板与持续色彩 | 8 | 不承诺通用VoiceLed渲染产生严格平行声部 |

132条采纳（含旧45）stored intent为loop67/develop36/resolve29；该标签与实际推荐组分开。采纳技法交叠计数：SlashBass35、ModalMixture27、PedalPoint23、ExtendedHarmony21、SecondaryDominant13、TritoneSubstitution8、Backdoor5、AlteredDominant5、JazzMelodicMinor6。不能相加当作独立音乐数量。

132条采纳完整/去节奏签名均各132个，且无V5命中；核心家族132个，其中010/028与V5同族，净增130桶。完整110条新批次相对V5+B1净增108桶，采纳的新87条贡献87个新桶。

110条新增中44条含标注静态调式之外的音，均保留局部副属、借用或连接依据；它们的全句归属与严格音阶兼容分开。045蓝本依据中F/A的文字已纠正为F和弦内A自然音，不改变低音；048的Bb、051的Bb、056/058的F#已逐音解释，strictPitchAudit明确记录拒绝条件。没有根据音集兼容自动建立调式归属。[独立内容核验](../data/factory-v6/b3/CONTENT_CHECK.md)列出重复、近亲、外音与统计定义；低置信核心家族桶不能替代音乐身份判断。

九组V5疑似完整重复已核实为**统一移调后的同形重复**，全部4/4、4个4QN事件；绝对音高并不相同。完整签名按首根音归一，保留根/低音/音集与节奏。旧语义和ID保留，未据重复审计删改V5；逐组移调量与内容见来源核验表。

## 实际祈愿卡池（固定主音C）

先筛reviewed归属和指纹绑定，再按原卡池身份去重。非严格允许有依据的借用；严格核对全部实际和弦音与低音。列间可重叠。

| 调式 | V5非严格 / 严格 | V6 experiment790非严格 / 严格 |
| --- | ---: | ---: |
| Ionian | 437 / 238 | 487 / 238 |
| Dorian | 3 / 3 | 25 / 24 |
| Phrygian | 1 / 1 | 13 / 13 |
| Lydian | 3 / 3 | 20 / 20 |
| Mixolydian | 8 / 8 | 24 / 24 |
| Aeolian | 114 / 64 | 115 / 65 |
| Harmonic minor | 115 / 29 | 123 / 33 |
| 爵士静态旋律小调 | 0 / 0 | 6 / 6 |
| Locrian / 古典双向旋律小调 | 0 / 0 | 0 / 0 |

Ionian严格池没有增加，是借用/变化属句经严格规则拒绝的预期结果。和声大调/Phrygian Dominant尚无已审核内容；不伪造平衡配额。

## 工程、召回与性能

Release x64、插件OFF、V6实验开关ON。定向FactoryV6Focused通过；132条完整MIDI覆盖781个和弦事件，SMF音高/音集/低音/时值读回与Preview非静音通过；132条均有合法前缀实际推荐，再经Snapshot3保存/恢复得到同源Preview/MIDI。底层合法前缀517例、十二调召回1584例、显式C minor49例通过。048/056/065/071分别用最长合格前缀3/7/5/1和弦，12调来源序列、组与排名分数一致，同输入重复稳定；071证据仅覆盖单和弦前缀，未据此声称其长前缀可达。

最长非完整前缀114/132合格；扩查部分前缀集合达到121/132，逐源Snapshot前缀搜索补齐132/132。该结论表示至少一个合法前缀可达。最长前缀的新增87条中75合格且默认前三可见：Loop48/Resolve23/Develop2/Color2；旧45条中39合格，38默认可见，038在Resolve第7需More。原四入口能消费新来源；Enrich以132条完整输入得到528个可用输出（polish141/rich306/advanced81），这是输入消费验证，不把Enrich说成Factory检索。

| 真实规模 / 本次采样 | 中位ms | p95 ms | 输入预算 |
| --- | ---: | ---: | --- |
| V5 658 | 2.2511 | 5.2573 | 全扫描 |
| V6 experiment790 | 3.1609 | 6.2174 | 全扫描 |
| V6 proposal828（含未准入） | 3.5284 | 6.8589 | cap800 |
| 仅V5＋全部152A的810对照：cap / full | 2.96 / 2.4906 | 6.2614 / 5.0059 | cap800 / full810 |

810对照用152条真实A源最长非完整前缀：输入810→800；受测full-only匹配来源0、查询源丢失0、Top20保留3040/3040。当前790实验同样没有cap/full差异。**触发的是输入截断门禁，尚未观察到这组查询的结果损失**。表中为最后一次完整运行采样；另一有效运行的experiment中位1.9214ms/p95 2.7665ms，说明绝对时间受运行负载影响。上述时间是隔离工具采样，不是DAW音频线程承诺；千条压力仍为合成证据。

最小建议：将千条内Factory检索预算提高至规划规模上界（可先对比1200）或在该范围全扫描；保持评分、资格、排序权重不变。须确认共享检索对个人库大规模输入的边界，并以真实扩建数据复测短前缀和跨调排名。当前不改算法、不做索引重构，继续收录须先裁决预算方案。20条A持有预算待决状态，不包装成工程通过。

V5的658个ID、序列化payload和音乐指纹逐条一致；旧60条音符、QN、拍号、sequence/skeleton/typedData逐字段保持。User Schema2、Session5、Snapshot3保持；真实user.db未访问。新增分级拒绝例通过：B级进入approved、grade/adoption冲突、降级无labelActions均退出失败。

[工程证据与产物哈希](../../HarmonyContinuation-v6-b3-evidence/engineering/RESULTS.md)、[真实预算逐query比较](../../HarmonyContinuation-v6-b3-evidence/engineering/build/factory-v6-media/real-shortlist-gate.tsv)、[新增针对性WAV/MIDI索引](../../HarmonyContinuation-v6-b3-evidence/engineering/build/factory-v6-media/media-index.tsv)。数据库是隔离数据产物：使用[experiment790](../../HarmonyContinuation-v6-b3-evidence/engineering/build/factory-v6-experiment.db)进行后续工程验证，勿替换生产活动指针。

## 集中裁决与下一步

主代理完成音乐中心/借用解释、来源层级、风格降标、073/079排除及预算暂停决定；LUNA承担来源核验、独立身份统计、编译和定向验证。普通A无需用户逐条重听。070与原争议候选继续隔离；现阶段最大的内容缺口仍是具有可靠风格依据的Neo-Soul/R&B/City Pop，而非再加同类扩展和弦。

B3本批在检索预算门禁停下，未到1000–1200。下一步先裁决最小预算改动，随后优先补风格来源明确的独立家族；B4再做插件/DAW/UI与必要安装生命周期验收。未安装、未激活V6、未推送、未合并main、未创建Tag或Release。


---

## 首轮历史快照（d9d4539，2026-10-24）

以下保留首轮当时裁决及有效证据；当前准入与阶段状态以上文B3裁决为准。首轮裁决：完成 B1 结构审计并建立 60 条首批候选；先完成首批音乐听评，再决定下一批规模。1,000–1,200 条是长期覆盖规划。首轮不批准进入大规模扩建或正式活动库切换。

## 工作线与保护边界

独立工作树 `D:\VibeCoding\HarmonyContinuation-factory-v6`，分支 `development/factory-v6-20261024`，父基线 `2421c6f`。该提交包含 G2，亦包含后续 G3/G4；B 复用已有音乐语义及数据接口，未依赖工作包 A 的新 UI/循环试听修改。原工作树及其已有未提交文档保留。

正式 Factory V5 保持 Library5 / Schema2 / 658，SHA256 `F989A83EA9EF92BA7C272BF0E5E3C7073C225899DC81D60DD46C10EA6B48CB3C`。V6 以独立 Library6 / Schema2 输出，不改 User Schema2、Session5、Snapshot3，不改 Unified 基础资格或权重，不访问个人 user.db，不连接生产活动指针或安装器。

## 交付入口

- [V5 完整覆盖矩阵](../data/factory-v6/audit/REPORT.md)：按风格、意图、技法、节奏、归属与音乐家族分层统计。
- [658 条逐 ID 台账](../data/factory-v6/audit/per_id_audit.tsv)，[原始JSON证据](../../HarmonyContinuation-v6-evidence/audit/per_id_audit.json)：保留每条 membership 的主音偏移、调式、usage/basis、证据和状态。
- [60 条完整候选审校表](../data/factory-v6/CANDIDATE_REVIEW.md)、[实际谱例数据](../data/factory-v6/candidates.json)、[来源与归属元数据](../data/factory-v6/metadata.json)。
- [隔离准入 ID](../data/factory-v6/approved-ids.txt)：56 条结构编辑审校，4 条待审排除；全体人工听评 Pending。
- 工程产物、测试和听评入口以本报告后文的实际执行结果为准。

## B1 覆盖判断与缺口优先级

658 个 ID 对应 649 个完整独立音乐身份；存在 9 组旧库完整重复、23 组去节奏近亲及 42 组低置信核心连接家族。V6 保留旧 ID 与原音乐，审计结果不触发旧库改写或删除。237 条缺少 stored techniques；这是标注缺口，不能解释成这些进行没有和声技法。

G2 v2 共 727 条归属，687 reviewed、40 unconfirmed；来源绑定 658/658。旧 Major474 / Minor184 与调式审核分开：旧中心 Ionian 有437条显式reviewed，旧中心Aeolian有107条显式reviewed，不能据其余旧标签补写Reviewed。

| 优先级 | 真正需要补充的音乐用途 | V5 固定主音严格池 | 本批候选方向 |
| --- | --- | ---: | --- |
| P0 | Dorian 有自然六度且小主音稳定的句子 | 3 | 5条代表句 + 1条Neo-Soul踏板句 |
| P0 | Lydian 升四音与主音踏板、开放式连接 | 3 | 5条代表句 + 1条慢速踏板句 |
| P0 | Phrygian 降二、暗色小下属及稳定中心 | 1 | 3条代表句 + 1条含借用的氛围句 |
| P0 | Mixolydian 降七回接、非大属终止 | 8 | 5条代表句，含slash与6/8 |
| P0 | R&B/Neo-Soul/City Pop 的完整延展与半音低音长线 | 不能由style标签数量推断 | 12条完整设计，非单纯给旧循环添扩展音 |
| P1 | 爵士替代属、后门、变化属与三度连接 | 已有基础，缺覆盖广度 | 10条完整路线，保留1条争议回接待审 |
| P1 | 氛围踏板、非功能根音路径与不对称拍号 | 已有同质和弦材料；不声称整个家族缺失 | 8条，允许不赋全曲调式 |
| 专项 | 爵士静态旋律小调 | 0 | 3条可核对静态音集的完整句 |
| 专项 | 古典双向旋律小调 / Locrian | 0 / 0 | 1条方向设计 + 2条Locrian实验，均待审排除 |

上述严格池数字以固定同一主音统计，另附全部12移调数量，避免将移调数当作独立创作选择。Ionian238、Aeolian64、HarmonicMinor29为旧库同口径基数。兼容检查始终在有证据的Reviewed归属之后，不能反向创造归属。

主代理复核了首末均非标注主音的12条线索：CITYPOP_JPOP_011、JAZZ_013/017均有内部ii–V–I或归主；OPEN_021/023–026/028/041/043/044也有内部I与开放功能连接。首末根音启发式不足以否定其Ionian框架解释，本轮保留其归属，并保留其他中心解释的可能。81个“外音但无显式Borrowed/变音标记”的source也只作为标注复核线索，蓝调或局部功能不自动构成归属错误。本轮未据这些启发式新增或提升旧归属。

## 首批内容与音乐裁决

60条为逐句手工设计，再确定性编码为现有事件格式。未批量排列组合，未转录商业作品MIDI或外部数据集。理论链接支持概念，不声称原创句子来自那些教材。常用程度是明确标出的编辑用途分级，未伪装成作品频率调查。

实际音集、slash低音、拍号及QN完整保存在typedData。9含七度与九度；11另含十一度；13采用七/九/十三并省十一度；省略约定与每个实际音集均可核对。参考主音统一C；23条小调中心用Minor坐标，其余37条用Major坐标，逐音级重映射保持实际音乐不变。该字段参与旧检索资格，不能等同于调式归属。9条全曲中心/大小三度解释不充分的色彩句不生成全曲调式membership；HCA不为它们虚构标签。

本轮修正了036/045中不符合C静态旋律小调的E自然扩展：Dm9改Dm7，036的G13改G7。038保留普通小七主和弦与小大七主和弦交替，明确Bb借用，严格HarmonicMinor拒绝。052尾Cm11含D自然音，保留为Phrygian语境借用，严格Phrygian拒绝。

| 同族关系 | 保留依据 |
| --- | --- |
| 016 / 052 | 踏板中段Bbm/C与Abmaj7/C不同；052尾部含D借用，严格资格不同 |
| 021 / 025 | 同属半音低音家族；021在C/G开放，025以Cmaj9闭合，上层和弦与扩展路径不同 |
| 036 / 038 | Dm7/G7与Dm半减/G7b9、主小大七与普通小七的语境不同，是旋律/和声小调对照 |
| 010 / 028与旧库核心家族 | 核心家族相近，但实际完整和弦、低音、长线与用途不同；详见逐条比较 |

候选内部及与旧库均无完整或仅节奏不同的重复；60条候选不等于60个全新音乐家族。046复杂侧门末段、057古典上下行方向、059/060半减主音中心保留Pending。其余56条仅准入隔离实验，人工听评未通过前不进入正式库。

理论区分采用[Open Music Theory 的音集与主音说明](https://openmusictheory.github.io/scales2.html)：音集本身不能证明中心；[Mark Feezell 的planing说明](https://learnmusictheory.net/PDFs/pdffiles/06-06-ParallelismPlaningImpressionism.pdf)用于区分和弦平移与保证具体声部平行的配器。候选记录同质和弦运动；通用VoiceLed渲染不保证严格平行声部。

[独立音集与近亲核验](../data/factory-v6/content-check/report.md)逐项核实60条记谱与实际事件，未代替产品集成证据。旧库多归属沿用已核验来源绑定的G2 v2；本轮没有足够证据新增旧库归属，未批量提升任何旧审核状态。

## 当前实际调式池

以下为本轮C++实际 `makeFreePool` 输出，固定主音C，先审核归属与来源绑定，再按原抽卡身份去重；“非严格”允许有证据的借用，“严格”逐实际音集/低音检查。各列不能相加，来源可有多种归属。

| 调式 | V5非严格 | V5严格 | V6实验非严格 | V6实验严格 |
| --- | ---: | ---: | ---: | ---: |
| Ionian | 437 | 238 | 455 | 238 |
| Dorian | 3 | 3 | 9 | 9 |
| Phrygian | 1 | 1 | 5 | 4 |
| Lydian | 3 | 3 | 9 | 9 |
| Mixolydian | 8 | 8 | 13 | 13 |
| Aeolian | 114 | 64 | 115 | 65 |
| Locrian | 0 | 0 | 0 | 0 |
| HarmonicMinor | 115 | 29 | 119 | 32 |
| 爵士旋律小调 | 0 | 0 | 3 | 3 |
| 古典双向旋律小调 | 0 | 0 | 0 | 0 |

本批增加了真实可抽的调式句，但3首爵士旋律小调与4首严格Phrygian仍不足以称为充分覆盖。Locrian与古典旋律小调继续如实返回空池。

## 工程执行证据与召回限制

独立开关 `HC_BUILD_FACTORY_V6_EXPERIMENT=ON` 生成 `factory_v6_database`，默认OFF。它只以已验证V5追加编译，输出proposal718、experiment714及合并HCA；旧HCA字节作为完整前缀保留，新增51个有归属的源记录，9个未断言调式的源不造记录。658条V5的ID、完整音乐指纹与序列化payload在两个DB中逐条读回一致。

本轮定向CTest `FactoryV6Focused` 通过（最终10.45秒）。对56条准入候选：底层合法前缀194/194、12调移位672/672、显式固定C minor 20/20；完整MIDI56句306个和弦事件，验证根音/低音/精确音集、SMF音高及时值读回。一个代表性可推荐路线经Snapshot3序列化/反序列化再生成Preview/MIDI；Session5与Factory/User Schema2的版本契约保持。PreviewSynth生成非静音是运行证据，不是音乐听审。

底层召回与Unified合格结果分开核实：最长非完整前缀只有50/56通过默认门槛。对其余022/023/027/029/030/039逐个测试全部2至n−1前缀，32次中13次合格；六条均有至少一个合法前缀通过，合并为56/56有实际合格召回例。这不表示任意前缀均可召回。

| ID | 可召回前缀 | 当前可见性与限制 |
| --- | --- | --- |
| 022 | Cm Abmaj7；或再加Eb/G | 该例组内第1；较长前缀受minimum_score/minimum_match限制 |
| 023 | 前2个和弦；前3–5个也有合格例 | 最短例组内第11，较长合格例第1 |
| 027 | 前2或3个和弦 | 最短例第13，3和弦例第1 |
| 029 | 前2或4个和弦 | 组内第1/2；其他前缀未达门槛 |
| 030 | 前2个和弦 | 第11，需完整列表/More；其他受测前缀未达门槛 |
| 039 | 前2或3个和弦 | 第27/7，需完整列表/More；较长前缀未达门槛 |

合格来源检查用 `perGroup=0` 保留全部合格结果，资格/评分门槛未调低；Top3按组内排序位置另列，不把全结果可达表述成默认卡片可见，GUI尚未实机确认。038另有组内第7的例子。source合并时核对primary/supporting来源，未要求某ID必须夺取同后续路线的primary身份。

### 检索预算裁决

| 数据规模 | 本轮采样中位ms | p95 ms | 800预算是否截断 |
| --- | ---: | ---: | --- |
| V5 658真实内容 | 1.9065 | 4.4405 | 否，全扫描 |
| V6 proposal718真实草案 | 1.9093 | 3.0219 | 否，全扫描 |
| V6 experiment714 | 2.1036 | 4.2600 | 否，全扫描 |
| 1,000合成压力 | 3.4101 | 10.1351 | 是 |
| 1,200合成压力 | 3.7865 | 7.2387 | 是 |

合成压力仅复制音乐并变更ID，未进入内容数据。单个对照查询001的4和弦前缀，在1,000/1,200下全扫描与cap800分别保持303/303、389/389来源，Top20均20/20；确定性乱序未改变该查询的来源集或顺序。此证据只证明该合成查询未漏，不证明千条真实独立音乐公平，也不代表DAW线程时延分布。

主代理裁决：当前718规模无需改变预算或索引；不改Unified资格/评分。越过800前，以届时真实内容和多类短前缀测cap/全扫描、来源保持率、跨12调与排序，再比较最小提高预算/千条内全扫描方案。当前没有依据批准索引重构。

编译器负向样例全部按预期退出1：把pending加入准入、漏metadata、同音只改role/标签、V5原路径和Windows大小写别名覆盖、HCA及summary覆盖。V5 DB与输入HCA的hash前后一致。审计读取release路径的LF版HCA（FE2C3685…），本工作树checkout为CRLF版（2D9E61DC…）；解析内容及658个绑定一致，合并输出保留所输入版本的原始字节前缀。

### 产物身份与最短使用

产物位于工作区同级 `HarmonyContinuation-v6-evidence/engineering/factory-v6-build/`，不是安装包：

- [experiment714 DB](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-experiment.db)，SHA256 `5F99B8C8DFF1889D6CDBCA0D2F23C5A226ECE949856E16B2CA5587B6926AA6B3`。
- [proposal718 DB](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-proposal.db)，SHA256 `F8153C3AC4EABA8F1B4B7E250C4C90E5845B013D4CFB06191020EAF974ECEEFE`。
- [合并HCA](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-initial.hca)，SHA256 `72ABF032B228E09C6FD00485B932803B210C1F9BB0E844437C9E7680E6BCEEAF`。
- [精确构建命令与负向证据](../../HarmonyContinuation-v6-evidence/engineering/ENGINEERING_EVIDENCE.md)。本轮只构建测试工具，配置中的Git override指向父基线，不构成新产品插件的BuildCommit。
- [实际定向日志](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-focused-test.log)、[构建摘要](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-build-summary.txt)。
- [代表音乐索引](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/media-index.tsv)：完整MIDI可拖入DAW，WAV可直接播放；025核对C–B–Bb–A–Ab–G–C低音；059/060仅供待审听评，不在experiment中。


| 听评对象 | 音频 | 完整MIDI |
| --- | --- | --- |
| 001 Dorian | [WAV](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_001-preview.wav) | [MIDI](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_001-complete.mid) |
| 011 Lydian | [WAV](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_011-preview.wav) | [MIDI](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_011-complete.mid) |
| 017 Phrygian | [WAV](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_017-preview.wav) | [MIDI](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_017-complete.mid) |
| 016 Phrygian踏板 | [WAV](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_016-preview.wav) | [MIDI](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_016-complete.mid) |
| 025 半音低音长线 | [WAV](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_025-preview.wav) | [MIDI](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_025-complete.mid) |
| 032 sus到变化属 | [WAV](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_032-preview.wav) | [MIDI](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_032-complete.mid) |
| 050 悬挂和弦平移 | [WAV](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_050-preview.wav) | [MIDI](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_050-complete.mid) |
| 045 爵士旋律小调 | [WAV](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_045-preview.wav) | [MIDI](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_045-complete.mid) |
| 059 Locrian待审 | [WAV](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_059-preview.wav) | [MIDI](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_059-complete.mid) |
| 060 Locrian不对称句待审 | [WAV](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_060-preview.wav) | [MIDI](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/V6_B1_060-complete.mid) |

无需安装即可先听WAV，再把完整MIDI拖入DAW核对声部与循环边界。勿用该DB替换生产活动指针。该分支未重建或安装插件；当前产物身份是Library6数据及定向测试程序，产品插件版本和实机安装状态另行管理。

## 阶段门禁与后续范围

| 门禁 | 当前裁决 |
| --- | --- |
| B-Gate1 | 结构审计、覆盖矩阵与首批方案交付；不等于658首人工听审 |
| B-Gate2 | 60条草案与逐条依据形成，结构及工程核验见执行证据；人工音乐听评待完成 |
| B-Gate3 | 暂不批准千条级内容建设；先审听代表句及上述4条争议项，再按剩余缺口选下一批 |
| B-Gate4 | 当前提供隔离数据与必要核心路径验证；完整插件、五功能实机与安装生命周期验收留在正式集成门禁 |

主代理负责收录原则、60条音乐设计、关键音集修正、同族保留裁决、检索影响判断与最终门禁；LUNA分别承担658条统计、工程编码和测试、独立谱例证据检查。自动化的非静音音频检查和成功抽卡不代替实际听评。

未安装、未改变生产活动曲库、未推送、未合并main、未创建Tag或Release。下一项依赖人工的工作是以实际MIDI/WAV听评长线连接、循环边界、声部舒适度及主音稳定性；验收结果与争议裁决应逐ID回填metadata。
