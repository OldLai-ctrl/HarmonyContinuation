# G11 连续续写路径分叉与现代和声覆盖

## 交付判断与身份

**工程与原RCR-3技术门禁通过；首选路径有所分散，可选分叉已实际回导验证；末轮集中仍明显，音乐质量待用户听评。暂不制作Setup。**

独立分支 `development/g11-continuation-network-20261103` 从干净G10交付 `fa2de2fb51fe042bd2975c3b0775273ffa3a7b56` 建立。版本 **0.11.0-dev.12 / Factory V8 / 1288**；BuildCommit **`c1b3743b0d3b517e44b5365666eb1014ffc2980c`**。最终交付HEAD、插件与数据哈希在 [identity.json](../../HarmonyContinuation-g11-evidence/identity.json)。新增来源 **0**，DB/HCA与G10逐字节一致。

## 009/010汇合的原因

仅重建原G10十二条成功链的第二、三轮，共24个实际和声音集/低音/时值语境。[诊断摘要](../../HarmonyContinuation-g11-evidence/diagnosis/g11-summary.tsv)、[候选与实际指纹](../../HarmonyContinuation-g11-evidence/diagnosis/g11-candidates.tsv)、[资格阶段](../../HarmonyContinuation-g11-evidence/diagnosis/g11-gates.tsv)。冻结指纹不保留原MIDI每个声部或全部宿主元数据；该重建只用于定向诊断，最终验收使用真实Factory导入和DAWClip回导。

- **A，主要成立：** 每轮11–19个完整开放且通过既有结构资格的身份、9–12个来源家族；替代路线并非不存在。`F–C–Dm–Am＋014`之后，009与004实际边界分同为0.730，旧排名却为1与23；013边界分0.840，排30。旧语境分还混入了“持续低音是否沿用近期低音趋势”等偏好，绝对色域变化又降低了历史相似度，使桥接色彩路线容易长期领先。原创意原分与数据库连接数不是此差别的充分解释。
- **B，未证明为主要原因：** 三个代表A的首页各取两个不同轮廓B，六次真实MIDI回导后都仍有多个合格C。新的源句级一步索引中，009有13条通过现有检索与结构资格的连接，010有16条。它们不是唯一出口。这里是来源标准语境下的结构可能性，不能保证未来旋律、全部硬约束或每次MIDI重解释都通过。
- **C，未成立为扩库依据：** 上述定向语境已有不同低音、悬置、调式和借用方向，未发现必须靠新内容才能连接的证据。本轮不扩库，也不将结论外推为全库内容已经充分。
- **D，成立：** 旧后两轮中的009、010分别反复使用同一完整音型的不同调高。根据实际音集、低音和时值归一，可辨认其重复；来源名不同或改变调高本身不足以证明出现新构思。现有开放增量仍普遍为8和弦、32 QN，本轮保留其真实结构。

旧 `nextPhraseSources` 只保留每端最多四条同调式边界连接，没有验证完整下一轮结构，还漏掉了实际可走的009→010。因此既不能把连接数当作音乐质量，也不能据此宣布某条路线后面无路可走。

## 技术与音乐选择

1. **一步结构连接信息。** `CandidateIndex`利用原开放来源和既有语境检索，为15条已审校完整开放句建立227条带实现调坐标的连接。调用原接缝、OPEN、完整边界、拍号与乐句结构判断；不预先拼接1288条曲库，不穷举三轮。不同调域解释可对应同一来源，227不是独立构思数量。[连接明细](../../HarmonyContinuation-g11-evidence/branches/network.tsv)。索引随Factory在现有后台Worker构建，音频线程不搜索。
2. **当前音乐先行。** 原资格与原音乐数据保持。保留G10质量层级的效用，不把来源审核层级机械变为首页硬配额。对同一层级，先比较实际边界与近期语境；与该层级最佳当前适配差超过0.075的路线暂后排，仍可分页访问。该范围是展示次序，不是新的音乐资格门槛。
3. **未来机会是小偏好。** 只为当前合格的完整开放句评估下一句可能性，按实际轮廓去重、结合已有音乐历史，最多考虑三个不同方向，效用最多2分。没有按照原始出度、来源频率或全局轮换分配排名；009/010仍正常保留。字段明确区分已评估、合法连接、独立轮廓、家族、支持的调域解释和新材料机会。处于中心转换中的句子不被冒称已有确定目标调。
4. **实际历史与当前候选间的多样性。** 保留G10音集、低音、时值与绝对色域判断，补充与绝对调高无关的相对音集/低音路径/张力轮廓相似度。同一骨架换调不能仅凭色域变化获得全部新颖度优势。重复只影响排序；主题再现、循环与所有合法变体仍可访问，不需要回导后保留隐藏来源ID。
5. **有界成本。** 同一输入、同一可信OPEN时值的历史比较可复用；只补算新增及跨接缝的4/8事件窗口。不递归调用推荐。源句级索引的下一步结构可能性在真实回导后仍须重新检查。

## 一次冻结50起点验收

使用与G8/G9/G10相同的50起点文件、顺序和完整音乐指纹；按正常混合列表首个满足原资格的结果选择，累计生产DAWClip回导。没有回溯、重新抽样或放宽G9门禁。完成这一批后未据统计结果继续修改算法。

| 指标 | G10 | G11 |
| --- | ---: | ---: |
| RCR-3 | 12/50（24%） | **12/50（24%）** |
| 实际所选轮次 / 首页可达 | 36 / 36 | 36 / 36 |
| 采用来源数 | 8 | **11** |
| 首轮采用来源数 | 7 | **8** |
| 本版最常用三来源占比 | 27/36（75%） | **24/36（66.7%）** |
| 009使用链数 | 12/12 | **10/12** |
| 010使用链数 | 11/12 | **9/12** |
| 不同后两句来源组合 | 6 | **8** |
| 最常见后两句组合 | 6/12：009→010 | **4/12：014→009** |
| 旧机制标签种类，仅诊断 | 5 | 8 |

[50起点结果](../../HarmonyContinuation-g11-evidence/rcr-3/g11-start-results.tsv)、[逐轮真实对象/接缝/指纹/未来机会](../../HarmonyContinuation-g11-evidence/rcr-3/g11-rounds.tsv)、[验收摘要](../../HarmonyContinuation-g11-evidence/rcr-3/g11-summary.tsv)。全部36段为算法评估的完整跨段接续；Exact/局部/跨段为0/0/36。38个起点首轮未找到原资格要求的完整开放后续，未重抽或改判。本批达到20%的精简产品技术标准，不作为全库成功率的精确估计。

**尚未解决：** 末轮仍有10/12选择009或010，与G10相同；本批11种来源中，009×10、010×9、014×5仍占多数。低音踏板、悬置和调式路线开始进入首选，但目前不能认定多轮发展已全面摆脱共同词汇。统计与标签不代替用户听评。

## 正常首页的可选分叉

每个A只取首页最先出现的两个不同实际轮廓B，分别导出、真实回导一次，再正常查询C；不搜索D，不回溯挑选。它们是分叉诊断，不计入RCR成功链。[实际B与回导结果](../../HarmonyContinuation-g11-evidence/branches/g11-branches.tsv)、[全部合格C与首页位置](../../HarmonyContinuation-g11-evidence/branches/g11-branches-candidates.tsv)。轮廓数量是同一相似度规则下的工程测量，不等同人工认可的音乐构思数。

| A | 首页B（排名） | 回导后合格C | C家族 / 轮廓 | C首页合格数 |
| --- | --- | ---: | ---: | ---: |
| V3_CYCLE_027 | 009（1） | 12 | 11 / 12 | 8 |
| 同上 | 014（2） | 16 | 11 / 14 | 8 |
| V6_B3_02_054 | 013（1） | 16 | 11 / 14 | 9 |
| 同上 | 007（2） | 10 | 8 / 10 | 7 |
| V8_01_006 | 008（1） | 15 | 11 / 13 | 6 |
| 同上 | 002（2） | 14 | 11 / 12 | 7 |

例如第三个起点的两个B回导后，都能在首页看到悬置序列011，以及桥接、上行、循环等其他完整路线。不是所有首选都会不同，也没有为了分叉数量而改写实际音乐。

## 必要工程验证

Release插件与定向工具构建通过；没有重复历史全量测试、完整SDK Validator、布局矩阵或安装器测试。[定向检查](../../HarmonyContinuation-g11-evidence/checks/g11-checks.tsv)涵盖：三个旧第二轮语境17/11/19个合格完整开放身份集合保持；实际根音字段、低音、音集、时值往返；历史符号/元数据别名不变；未来机会不能改变资格或使明显较差连接越位；重复仍能分页；一条真实完整/OPEN回导后再查询。

初实现84事件查询为1.293秒，定位到重复历史计算；缓存修复后，增量与全量轮廓逐项一致。因这一真实性能问题追加对应定向复核，最终一次84事件 `makeMatchQuery＋正常两视图推荐` 为 **518.915 ms**，复用现有索引。[最终计时](../../HarmonyContinuation-g11-evidence/checks/g11-checks-84-event.tsv)。未做性能矩阵；该值不作为不同历史前缀下G10耗时的严格同比。所有计算在后台，真实宿主响应待验收。

以下九组代表对象在本批实际完成Snapshot3保存重开，Preview与生产MIDI字节一致，WAV非静音。新增分叉/轮廓字段仅运行时使用，不迁移Session5/Snapshot3；快照冻结实际演奏与来源，不伪造后续约束已通过。

## 三条真实首选试听链

全部来自本次正常首页连续首选，未为试听另选隐藏对象或回溯。120 BPM、4/4；每个B/C/D均为完整8和弦、32 QN，保留原末端。三位编号统一为完整 `V8_01_` 来源后缀。所有后句是既有审校原创编辑乐句，跨句接缝为算法评估，没有冒称已有作品直接延续或独立编辑接缝审核。MIDI为DAWClip，宿主按上述速度/拍号播放。

### 1. 调域转向 → 开放循环 → 低音上行

完整来源链 **V3_CYCLE_027 → V8_01_009 → V8_01_002 → V8_01_010**。

- A：`F–C–Dm–Am`，16 QN。
- B：`Dm9–Bbmaj7–Dbmaj7–Eb6 / Fm9–Abmaj7–Bbm9–Dbmaj7`。
- C：`Dbmaj9–Fm9–Eb6–Ab/C / Dbmaj9–Bbm9–Cm7–Ebsus4`。
- D：`Ebm9–Fm7–Gbmaj9–Ab6 / Bbm9–Cm7b5–Dbmaj9/F–Gbmaj9/Bb`。

桥接先进入降号色域，C以持续开放的悬置结尾组织回环，D换成连续上行的低音组织；不是原G10的009→010直接相连。实现坐标为F/Ab/Db大调，属支持的局部调域解释，不等同三次已经确立的转调。接缝16/48/80 QN，即8/24/40秒；总112 QN、28小节。

| 累计 | WAV | MIDI |
| --- | --- | --- |
| A+B | [第一轮](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-1.wav) | [第一轮](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-1.mid) |
| A+B+C | [第二轮](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-2.wav) | [第二轮](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-2.mid) |
| A+B+C+D | [完整](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-3.wav) | [完整](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/1-V3_CYCLE_027/V3_CYCLE_027-round-3.mid) |

### 2. 持续低音下的上声部变化 → 低音释放上行 → 色域展开

完整来源链 **V6_B3_02_054 → V8_01_013 → V8_01_010 → V8_01_009**。

- A：`C(8)–Bb/D–F/A–Eb/G–Fm/G#–C/G–F–C(8)`，未标处各4 QN；完整40 QN。
- B：`Fmaj9/C–G/C–Am7/C–Bbmaj7/C / Csus4–Bb/C–Am7/C–Gm9/C`。
- C：`Gm9–Am7–Bbmaj9–C6 / Dm9–Em7b5–Fmaj9/A–Bbmaj9/D`。
- D：`Dm9–Bbmaj7–Dbmaj7–Eb6 / Fm9–Abmaj7–Bbm9–Dbmaj7`。

B低音保持C，上声部逐步改变色彩与悬置程度；C释放为G–A–Bb–C–D–E等上行低音，D才进入共同音支持的半音色域变化。保留原G9首尾C各8 QN；接缝40/72/104 QN，即20/36/52秒；总136 QN、34小节。实现坐标均F大调，D内部明显移动到Fm/Ab音域，不能仅用同一个key字段判断音乐未变化。

| 累计 | WAV | MIDI |
| --- | --- | --- |
| A+B | [第一轮](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-1.wav) | [第一轮](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-1.mid) |
| A+B+C | [第二轮](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-2.wav) | [第二轮](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-2.mid) |
| A+B+C+D | [完整](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-3.wav) | [完整](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/2-V6_B3_02_054/V6_B3_02_054-round-3.mid) |

### 3. 调式踏板起点 → 借用下属 → 交替悬置张力

完整来源链 **V8_01_006 → V8_01_008 → V8_01_011 → V8_01_009**。

- A：`Am7–Bb/A–C/A–Bb/A / Am9–Gm9–Bbmaj7–Gm9`，32 QN。
- B：`Ebmaj9–F/Eb–Bb/D–Cm9 / Abmaj7–Bb/Ab–Fm9–Gm7`。
- C：`Fm11–Bbsus4–Gm11–Csus4 / Abmaj9–Dbadd9–Eb/Bb–Fm9/C`。
- D：`Cm9–Abmaj7–Bmaj7–Db6 / Ebm9–Gbmaj7–Abm9–Bmaj7`。

C交替使用十一和弦与sus4，后半改用借用色彩和转位低音，末Fm9/C保留张力；D进入Ebm/Gb音域。实现坐标Bb/Eb/Eb大调，G9资格及实际音符接缝保持。接缝32/64/96 QN，即16/32/48秒；总128 QN、32小节。Bmaj7为实际输出拼写，可作Cb等音解释，不改音符。

| 累计 | WAV | MIDI |
| --- | --- | --- |
| A+B | [第一轮](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-1.wav) | [第一轮](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-1.mid) |
| A+B+C | [第二轮](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-2.wav) | [第二轮](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-2.mid) |
| A+B+C+D | [完整](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-3.wav) | [完整](../../HarmonyContinuation-g11-evidence/rcr-3/representative-paths/3-V8_01_006/V8_01_006-round-3.mid) |

三条后两句依次为002→010、010→009、011→009，已不同于G10三条共用009→010；但三条仍都经过009，其中两条末句为同一桥接音型的不同调高。这是当前词汇集中，不能用前三段标签不同掩盖。相邻完整对象、QN、音集与来源见同目录九个 `.snapshot3.json` 和逐轮表。

## 保护、分工、Skills与下一门禁

G10工作树、原交付插件及既有安装包保留；G9结构/OPEN规则、Exact/60、四入口、升级、祈愿与完整分页保持。没有持久化格式迁移，没有操作生产安装或真实user.db，没有推送、合并main、Tag或Release。源码构建通过，真实DAW加载/响应和人工音乐听评未完成。

主代理负责音乐与架构判断、核心实现、关键差异审核、实际执行及门禁。LUNA负责既有链的确定性取证、受限诊断与检查入口实现；其报告由主代理核对，未代替音乐验收。

实际读取并使用的Skills：`deja-search`（历史检索，本轮无有用的G11命中）、`diagnosing-bugs`（固定24语境及真实性能问题定位）、`codebase-design`（共享索引/资格接口与有限一步深度）、`stop-that-shit`（范围、最小检查与停止条件）。当前清单未发现专用音乐理论/C++/CMake/MIDI/SQLite技能，未安装额外技能。

**下一开发Setup条件：** 工程与24%连续门禁已具备；音乐上出现可辨认的新踏板及悬置首选，但末轮汇合仍未解决，用户听评尚待完成。因此本轮停在音乐门禁，不自动进入Setup验收或制作安装包。
