# Factory V6 / B3-R 集中评审

本轮停在 **B3 内容与工程门禁**。默认检索已改为完整扫描；恢复20条A级内容，新增两批204条并采纳202条，另修正旧025/052标签后采纳。最终隔离V6为 **1,014条 / 1,005个跨移调完整音乐身份 / 965个低置信核心家族桶**。建议提交B4集成评审，安装与DAW验收待B4执行。

## 代码与数据身份

- 分支：`development/factory-v6-20261024`；接手`25d3308`。R1代码提交`1f0e428`，恢复810基线提交`047d160`，最终HEAD见本批本地提交。
- 运行代码仅修改`ProgressionMatcher.h/.cpp`默认扫描策略、`ModeAnnotations.cpp`注释字节容量；其余代码变更为必要的定向测试入口。Worker、Unified匹配/60分资格/评分/去重/排序、展示上限、无输入祈愿抽样逻辑保持。
- Library6 / Factory Schema2；User Schema2、Session5、Snapshot3与HCA格式1保持。V5 658条原payload/音乐身份保留；真实user.db未访问。
- [当前完整候选表](../data/factory-v6/CANDIDATE_REVIEW.md)、[音乐数据](../data/factory-v6/candidates.json)、[分级/来源/归属与关系](../data/factory-v6/metadata.json)、[实际采纳ID](../data/factory-v6/approved-ids.txt)。

## 检索容量与实际故障修复

`MatchWeights.shortlistLimit=0`表示默认完整有序扫描，正数只保留为显式预筛配置；没有把800换成另一个固定预算。Factory与User仍在既有Worker合并并于后台匹配，输入约束Wish也自然使用完整索引。

真实810索引的3个代表输入都扫描全部810源；与显式800旧行为相比，没有新增或丢失返回源，共同分数相同、Top20各保留20/20。一次同输入耗时：全扫1.5059ms / 旧cap1.5090ms。隔离UserLibrary的一个来源与Factory合并为811源并实际匹配。四个代表输入合计覆盖Resolve6/Develop1/Loop1/Color3，Enrich得到4个候选，输入约束Wish得到1条。这些是指定样例的核心API证据。

最终编译时实际触及HCA的1MiB字节上限；统一读取/解析/序列化上限改为2MiB，保留8192记录和原字段长度限制，未压缩或截断审校依据。最终HCA为1,080,025字节，已成功编译、读取与绑定。

最终1,014条库，同一代表输入一次默认推荐为2.0787ms，实际扫描1,014源。未观察到实质性能异常；这里是一次工程采样，不是宿主时延承诺。

## 采纳、身份与现代和声覆盖

| 本轮内容 | 结果 |
| --- | --- |
| 恢复旧20A | 全部恢复；ID、完整音乐与原归属不变；810独立数据基线保留 |
| batch02 | 104候选，103A采纳、1C排除；净增103核心桶 |
| batch03 | 100候选，99A采纳、1C排除；净增99核心桶 |
| 旧025/052 | 删除不可靠genre标签，改functional用途与contextual归属说明；B→A，音符/低音/QN/拍号不变 |
| 最终全部候选 | 374条：356A采纳、1B与17C隔离；proposal含V5共1,032条，experiment1,014条 |

02-086与B1_029、03-035与B1_041仅扩展/节奏不同，未提供独立新增用途，均C不采纳。两新批次无完整或去节奏重复；原V5九组整体移调同形关系保留ID。完整音乐统计沿用已核验V5的649身份，加入356个唯一采纳音乐；核心统计611＋354新增桶，两个旧采纳源010/028与V5同族。核心桶是相似性分类，不能替代完整音乐身份。

| 本轮新增采纳方向 | 条数 | 依据与用途 |
| --- | ---: | --- |
| Dorian / Mixolydian / Lydian / Phrygian结构句 | 14 / 12 / 12 / 10 | 中心、特征音和回接逐句设计 |
| 当代流行与另类摇滚应用 | 32 | 借用iv/降六/降七、开放句、反向或半音低音 |
| 延展键盘 / R&B教学语汇应用 | 61 | maj9/m9/m11/13、sus、转位低音与局部副属 |
| 现代爵士应用 | 23 | 替代属、后门、变化属及非传统解决 |
| 场景/持续色彩 | 20 | 踏板、常音、同质和弦与慢速声部设计 |
| Aeolian与专项小调 | 8 / 10 | 自然小调弱终止、和声小调、静态爵士旋律小调 |

主要新采纳技法交叠计数：SlashBass90、ModalMixture49、PedalPoint40、SecondaryDominant29、TritoneSubstitution15、Backdoor13、JazzMelodicMinor8。每条均为手工原创完整句，再确定性编码；[批次蓝本](../data/factory-v6/b3r/batch02.tsv)、[第二批蓝本](../data/factory-v6/b3r/batch03.tsv)、[增量核验摘要](../data/factory-v6/b3r/audit-summary.json)可追溯。

外部资料仅支持概念和教学应用，完整候选直接摘录数为0。新增333项风格证据均为technique-supported editorial application，常用程度unrated；未宣称这些句子属于广泛使用的典型Neo-Soul/City Pop。[本轮补强来源](../data/factory-v6/b3r/sources.md)分别记录R&B、Neo-Soul、City Pop教师与游戏作曲者的证据范围；未复制商业MIDI、谱面或受限数据资产。

## 实际固定C调式卡池

归属reviewed与来源指纹绑定先于音阶筛选。下表是实际`makeFreePool`计数；每格非严格/严格，列间可交叠。新两批108条有静态音阶外音，保留借用/局部功能解释并由严格池拒绝。

| 调式 | 原790池 | 当前1,014池 |
| --- | ---: | ---: |
| Ionian | 487 / 238 | 586 / 241 |
| Dorian | 25 / 24 | 48 / 43 |
| Phrygian | 13 / 13 | 29 / 28 |
| Lydian | 20 / 20 | 40 / 39 |
| Mixolydian | 24 / 24 | 42 / 41 |
| Aeolian | 115 / 65 | 130 / 79 |
| Locrian | 0 / 0 | 0 / 0 |
| 和声小调 | 123 / 33 | 143 / 45 |
| 爵士静态旋律小调 | 6 / 6 | 19 / 19 |
| 古典双向旋律小调 | 0 / 0 | 0 / 0 |

Locrian与古典双向旋律小调继续空池，未为稀有调式补配额。和声大调/Phrygian Dominant未扩展到本轮模式契约。

## 本轮实际验证与后续

- R1：Release构建、既有Matcher定向测试、新810完整扫描/3个旧cap对照/少量推荐功能及隔离User来源检查。
- R2：20个恢复源逐条DB/HCA/metadata/完整身份和一个合法qualified前缀通过；无样本导出。
- R3：本次204个选中源（202新＋2标签修正）逐条读入/身份/归属/合法qualified前缀通过；本轮首个合格前缀均为1和弦，口径为完整合格结果集，默认每组最多10条展示规则保留，UI可见性留B4。4个代表完整MIDI读回与Preview输出通过。
- 唯一故障追加：HCA超1MiB后，运行现有annotation检查入口并验证真实新HCA读取。最终卡池计数与推荐耗时各做一次；没有重复原全量FactoryV6Focused、多规模/十二调组合、全部MIDI/Snapshot、UI、安装或DAW测试。

[本轮工程原始证据与产物](../../HarmonyContinuation-v6-b3r-evidence/engineering/r3/R3-RESULTS.md)；[R1证据](../../HarmonyContinuation-v6-b3r-evidence/engineering/R1-RESULTS.md)、[810回退基线](../../HarmonyContinuation-v6-b3r-evidence/engineering/r2/R2-RESULTS.md)。数据包入口：[experiment1,014 DB](../../HarmonyContinuation-v6-b3r-evidence/engineering/r3/factory-v6-experiment.db)、[配套HCA](../../HarmonyContinuation-v6-b3r-evidence/engineering/r3/factory-v6-merged.hca)。旧[790 DB](../../HarmonyContinuation-v6-b3-evidence/engineering/build/factory-v6-experiment.db)保留作回退。

主代理负责完整扫描决策、音乐与标签修正、两条低价值近亲排除及最终门禁；LUNA负责限定来源核验、增量数据核对和必要编译/测试。原070仍B，17C继续隔离；可靠作品级风格/频率证据仍是内容缺口。新版V6需在B4与配套HCA及本轮读取器一起集成；4组新媒体是工程输出，未新增用户人工听评。

未安装、未激活V6、未改正式v0.10.0/Factory V5、未推送/合并main/Tag/Release。等待决策端集中审查B3-R与B4集成建议。

早期证据：[V5覆盖矩阵](../data/factory-v6/audit/REPORT.md)、[旧56来源与等级复核](../data/factory-v6/b3/SOURCE_REVIEW.md)、[B3 790工程快照](../data/factory-v6/b3/ENGINEERING.md)、[首轮人工试听索引](../../HarmonyContinuation-v6-evidence/engineering/factory-v6-build/factory-v6-media/media-index.tsv)。这些保持历史范围。
