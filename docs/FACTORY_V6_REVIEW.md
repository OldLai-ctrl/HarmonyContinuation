# Factory V6 工作包 B：首轮集中评审

当前裁决：完成 B1 结构审计并建立 60 条首批候选；先完成首批音乐听评，再决定下一批规模。1,000–1,200 条是长期覆盖规划。首轮不批准进入大规模扩建或正式活动库切换。

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
