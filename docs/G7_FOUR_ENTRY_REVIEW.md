# G7 四入口与灵感首页验收

2026-10-27：**工程通过；五条音乐首轮听评已完成，连接可接受，继续创作的灵感目标未通过；真实DAW未验收。**

独立分支 `development/g7-four-entry-20261027`，版本 **0.11.0-dev.8**，实现与插件 BuildCommit **`58dfce0fb78c712def4902d81b8103747b746ea2`**。从G6交付头`8da40f7`建立，G6实现基线`03fad728769de139d5016abb02db2a7f0e151cb9`保留。Factory V7/1272、DB/HCA字节均与dev.7一致；dev.6/V7及其他已交付产物未操作。[完整身份和SHA](../../HarmonyContinuation-g7-evidence/identity.json)。

## 四入口结构与保护范围

| 一级入口 | 实际行为 |
| --- | --- |
| 匹配优先 | 紧凑四选：收束/发展/循环/色彩。使用原Exact Unified；保持原始分数、60分资格、来源与完整演奏身份。切换子功能保留输入，并沿用旧匹配偏好重算语义。 |
| 灵感优先 | 默认直接显示跨分组混合列表；不要求先选意图。稳定/丰富/大胆对应稳定延续、丰富变化、大胆转向，另标直接延续、局部语境续写或跨段转接。 |
| 升级进行 | 原和声丰富化/升级算法保持。 |
| 和声祈愿 | 原纯随机、输入关联、调式筛选、清空、历史、试听和MIDI路径保持；本轮只迁移导航，不改抽样规则。 |

同一输入保存Exact和灵感两个结果集；一级入口切换只换视图，不清输入、不重算、不停止正在试听的对象。选择、分页、固定候选和其指纹跨入口保留。播放目标额外绑定代表来源ID，防止相同后续ID在两个视图中有不同解释时换错完整来源。

导航、窗口内浏览状态仅在实例运行期保留；Session5/Snapshot3无迁移。G6快照的演奏与后句来源ID继续可用，缺失的通道/中心/接缝元数据明确显示为“冻结演奏”，不从默认字段冒称稳定或直接来源。旧色彩排序适用于匹配/升级；灵感保留色彩曲线与说明，按独立混合顺序展示。

## 灵感首页：实际FGEmAm结果

以同一个真实 `F–G–Em–Am` / Auto 输入生成。首页前10为 **2条Exact直接延续、5条语境续写、3条算法跨段**；方向为稳定2、丰富4、大胆4。20/50/30为软目标，本次没有为了比例替换音乐。225条跨段仍在全量列表里，首页不按它们的数量占比铺满。

| 首页位次 | 来源ID | 路线类型 / 来源类型 | 完整后续 |
| --- | --- | --- | --- |
| 1 | V7_01_012 | 丰富 / 整段语境 | Am/G–Fmaj7 |
| 2 | CITYPOP_JPOP_002 | 稳定 / Exact直接 | Dm7–G7–Cmaj7 |
| 3 | V7_03_005 | 大胆 / 算法跨段 | C/G–F–G7/B–C–Am7–Dm7–G7–C–Dbmaj7–Bb7–Eb–Abmaj7–Bb7–Eb |
| 4 | V3_CYCLE_052 | 丰富 / 尾三语境 | F–G |
| 5 | V7_01_081 | 大胆 / 整段语境 | Ab–Bb7–Eb |
| 6 | V7_04_055 | 丰富 / 算法跨段 | Am9–Fmaj7–Dm9–G–C/E–Am/E–F–Gsus4 |
| 7 | CITYPOP_JPOP_015 | 稳定 / Exact直接 | Em7–Fmaj7–G7–Cmaj7 |
| 8 | V7_04_034 | 大胆 / 算法跨段 | Cmaj9–Bb13–Ebmaj7–Fm9–Dm7b5–G7b9–Cm9 |
| 9 | V7_04_077 | 丰富 / 整段语境 | Dm7–G7–C/G–F |
| 10 | V7_04_016 | 大胆 / 尾二语境 | Gm9–Fmaj9–Abmaj9–Bb13–Gsus4 |

结构上存在低音下行、普通结束、悬属、新Eb区域、长跨段和借用色彩等差异；这些差异尚不足以证明创作启发。用户听评指出五条代表材料整体仍趋向终止，4、5相对有意思，但4在新调再次收束。该反馈针对下表五条材料，未覆盖首页全部十条；首页也包含第4条，且若干候选明确走向C、Eb或Cm结束，需要据此定向修正展示选择。普通C回归继续保留为稳定基准。

排序只作用于展示：独立资格不放宽，原始分数只在同通道比较；方向和来源类型软平衡，再按首和弦、低音、家族、真实目标中心、用途及完整路线分散。单和弦色彩片段后移但未删除。Exact的方向依据当前输入语境判断，来源匹配坐标不冒充实际调中心变化。全量294个独立保留对象可分页，其中旧探索项继续明确标注；数量仅说明访问未丢失。[首页原始来源/指纹记录](../../HarmonyContinuation-g7-evidence/ui/g7-homepage.tsv)。

![中文灵感首页](../../HarmonyContinuation-g7-evidence/ui/g7-home-zh.png)

[中文匹配与四子项](../../HarmonyContinuation-g7-evidence/ui/g7-zh.png) · [英文灵感第2页](../../HarmonyContinuation-g7-evidence/ui/g7-en.png)。截图来自原生UI专项；播放状态为受控测试状态，不表示已执行人工音乐试听。

## 五条代表音乐：首轮听评与未通过项

五条均取自G6实际候选和已验证媒体，不重复其13组身份检查。检索输入均为 **F–G–Em–Am**。下列“完整后续”从推荐边界开始；Exact沿用既有完整来源试听，语境/跨段使用真实输入与冻结后段。没有作品级转录证据的条目不冒称真实作品延续。

| 角色、来源 | 完整后续 | 来源与结构判断 | 材料 | 人工听评 |
| --- | --- | --- | --- | --- |
| 1 正常延续 · CITYPOP_JPOP_002 | Dm7–G7–Cmaj7 | Exact直接库内延续，ii–V–I提供可信普通结束；保留作传统基准。 | [WAV](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-exact-1-CITYPOP_JPOP_002.wav) / [MIDI](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-exact-1-CITYPOP_JPOP_002.mid) | 可接受；整体反馈为缺乏后文启发 |
| 2 悬属结尾 · V7_01_020 | Dm7–G7–Am7–Gsus4 | 完整输入对应的库内直接后段；G7转vi后悬属，结构上未落主和弦，但本次听评未认可其发展启发。 | [WAV](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-full-1-V7_01_020.wav) / [MIDI](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-full-1-V7_01_020.mid) | 可接受；整体反馈仍有单向终止感 |
| 3 现代色彩 · V7_04_033 | Fmaj9–G13–Em7 | 尾二语境真实后段；扩展音提供色彩，根音路线仍接近熟悉的进行。末和弦非主和弦不足以证明后文启发。 | [WAV](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-two-9-V7_04_033.wav) / [MIDI](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-two-9-V7_04_033.mid) | 可接受；整体反馈为缺乏后文启发 |
| 4 跨调收束 · V7_01_081 | Ab–Bb7–Eb | 库内完整前后关系，非算法跨句接缝；Am→Ab由共同音C与半音移动连接，IV–V7–I明确落入Eb区域。应标作新调收束，不能充当开放发展的验收样本。 | [WAV](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-full-2-V7_01_081.wav) / [MIDI](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-context-full-2-V7_01_081.mid) | 可接受、稍有意思；最后仍终止，不能继续拓展 |
| 5 大胆跨段 · V7_04_042 | Am9–D13–Am9–Bm7–Gmaj7–D/F#–Em7 | **算法评估跨段**，后句为V7编辑原创；D13中的F#带来A中心Dorian色彩。接缝无独立编辑审核。 | [WAV](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-phrase-transition-4-V7_04_042.wav) / [MIDI](../../HarmonyContinuation-g6-evidence/final-check/media/F_G_Em_Am-phrase-transition-4-V7_04_042.mid) | 可接受、稍有意思；整体灵感目标仍未通过 |

用户已试听上表五条，反馈：“都可以接受，但还是缺乏对后文的灵感启发。它们都会向一个方向终止。像4、5两个就稍微有意思一点，但4最后也落入了一个终止，还是不能继续拓展。”这是对音乐可接受程度与创作启发的不同结论；并未完成真实DAW功能验收。**五条连接可接受；开放发展与后文启发未通过，G6音乐质量不能标记最终PASS。**

### 针对反馈的定向定位

源码核对发现两项局限：`ContextualContinuation.cpp`从每个锚点取到来源乐句末尾，保留完整来源身份，也继承其成句倾向；发展资格中的`finalCadence`和非开放末端检查对`centerShift`例外放行，因此第4条这种新调收束也能进入发展组。混合首页分散了来源、中心和家族，但没有单独判断实际终止趋向及下一句发展价值。它们解释了为何“更多候选、更多调中心”仍未解决用户反馈；不能据此推断所有候选都收束。

小范围回查现有G6候选发现以下对照路线，均不在已保存的G7首页前十。表内排名是G6分组排名，不是G7混合排名；未新增生成、未改来源或时值，也未试听这些对照材料。

| 现有来源 / G6位置 | 实际完整后续 | 可供复核的方向与局限 |
| --- | --- | --- |
| V7_04_044 / develop 28 | Am7–D/A–G/A–Am7–C/E–Bm/F#–D/F# | A持续低音后的调式展开；最后D/F#保持8 QN，仍需判断是否形成停顿感。 |
| V7_04_058 / develop 14 | AmMaj9–Bm7–Caug–D13–G#m7b5–AmMaj7–Bm7 | 旋律小调色彩与尾部Bm7；和声较复杂，不能仅凭开放末和弦认定启发更好。 |
| V7_04_052 / loop 14 | Am7–Bb/A–Am7–Gm7–Fmaj7–Bb–Am7/E | 降II与持续低音、回环演化；仍会回到Am身份，需要区分有效循环和单次结束。 |

三条均为**算法评估跨段**：后句有V7编辑来源，连接无独立审校记录，引用支持技法而非这条完整演奏。证据来自[既有真实候选记录](../../HarmonyContinuation-g6-evidence/final-check/g6-routes.tsv)、[V7来源目录](../data/factory-v7/candidates.json)和上述G7首页记录。现有数据提供了定向复核对象，尚未证明首页重排即可解决问题。

下一轮修正应先区分实际收束、悬置、循环和持续展开，明确新调收束的用途，再检查首页是否给不同的发展机制正常机会。普通可信延续保留；扩展音、换调或非主和弦结尾均不能单独作为创作启发证据。来源完整演奏与旧Exact资格继续保护。本次反馈接收仅更新验收记录与定向定位，算法及已构建二进制未改。

## 必要验证与响应

Release插件、Demo和专用 `HostContractHarness --g7-ui` 构建通过。只执行同一FGEmAm输入、800×780、100%尺寸的G7专项；真实投影包含来源完整演奏和匹配轨迹。检查四入口/四子项、跨入口输入/选择/分页/固定对象、混合卡片按group/index取得试听/MIDI/Snapshot回调对象，指纹一致。没有重新执行G6的13组演奏身份验证，底层Preview/MIDI/Snapshot沿用该有效证据。

旧Exact结果与同一旧Core路径的独立查询逐条比较：**30条的分组、原始分数、全部子分数、来源证据、索引与音乐身份一致**。混合顺序覆盖全部合法唯一对象，无分页丢失。专项发现并修复祈愿返回新增入口的动作路由；真实对齐轨迹增加卡片高度时，验收操作先滚动至可见的Snapshot按钮，不通过冻结模式绕开实际来源投影。

一次代表性原生UI异步流程：G1为真实worker首次请求，约 **476 ms**；其结果延后至G2请求后送达，通过生产视图generation门禁被实际拒收。G2约 **201 ms**后被接受显示。两次等待期间分别持续32/14次绘制和frame idle，共46次；这是单例观察，不是性能分布或宿主验收。[原始UI日志](../../HarmonyContinuation-g7-evidence/g7-ui.log)、[构建日志](../../HarmonyContinuation-g7-evidence/g7-build.log)。

没有运行布局矩阵、压力、多规模、完整宿主、V7/G5/G4全量或安装器测试。中英文基本布局经实际截图检查；真实系统/宿主音频输出与DAW操作不在工程通过结论内。五条音乐的用户听评单独记录如上。

## 最终边界与下一步

主代理负责四入口语义、双结果视图、混合排序、音乐结构判断、问题裁决和最终门禁；LUNA负责UI迁移、确定性修改与原生专项辅助。当前工程具备制作下一独立开发Setup的条件；**本轮未制作/安装Setup**，未操作生产安装或真实user.db，未推送、合并main、建Tag或发布Release。

五条首轮听评已完成，下一步针对终止趋向、持续展开与首页选择修正并做少量对照听评。工程通过结论保留；灵感目标未通过，暂不推进Setup、RC或正式发布。
