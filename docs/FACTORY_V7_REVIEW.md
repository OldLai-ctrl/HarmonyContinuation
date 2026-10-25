# Factory V7 / Contemporary J-Pop & ACG

## 对象与门禁

独立分支 `development/factory-v7-20261025`，从 G5 交付 HEAD `24bdc239b2f81895f93300f39e769d8210f5f79b` 建立。保留 `0.11.0-dev.5` / BuildCommit `b3d1fce6225593c76bc98ca0dec083bc469c5e00`、Factory6/1016 及原工作树。

2026-10-26内容与窄工程门禁完成：339稿，**256 A采纳、45 B待审、38 C排除**。隔离Factory7 **1272条**，Schema2 / HCA Format1；Session5、Snapshot3、User Schema2不变。内容与工具提交 `310ff6e`，实际测试源文件/输入/工具二进制哈希见证据清单；没有新插件BuildCommit或安装包。短前缀发现性限制保留为下一轮集中裁决项。

原始固定乐句：[batch01–06及编码器](../data/factory-v7/)；最终音乐与逐条资料：[candidates](../data/factory-v7/candidates.json)、[metadata](../data/factory-v7/metadata.json)。[主代理裁决](../data/factory-v7/primary-decisions.json) 是采纳依据，[初审](../data/factory-v7/review-decisions.json) 保留第三组筛稿过程；[allowlist](../data/factory-v7/approved-ids.txt) 由前者生成。每条都有音集、低音、QN、拍号、用途、结尾、边界、调中心、独立归属证据、来源范围及审校记录。

## V7-A 定向缺口

仅检查 G5 实际 DB 的相关家族、用途与端点。形状命中不等于作品出处。

| 结构 / 事实 | G5 / V6 新增 | 本轮处理 |
|---|---:|---|
| 王道 IV–V–iii–vi | 4 / 0 | 完整前后句、其他家族换轨、开放落点 |
| 小室 vi–IV–V–I | 18 / 1 | 与王道、卡农及借用色彩结合 |
| 丸サ IVmaj7–III7–vi7–I7 | 0 / 0 | 最后定向补 6 条完整跨家族路线 |
| 丸サ vi 后展开 Vm7–I7 | 0 / 0 | 局部 IV 属链、半音小七过桥 |
| 卡农 I–V–vi–iii–IV–I–IV–V | 1 / 0 | 旋律性低音与段落转换 |
| 相对小调链 IVmaj7–III7–vi7–ii7–V7 | 2 / 1 | 单独命名，不计作丸サ本体 |
| Develop / Loop / Color | 209 / 269 / 89 | V6 增量为117 / 142 / **0**；优先新增开放、循环、Color |

G5 Develop 中123条末和弦为I/i，112条未标authentic；127条loop_closure中117条不落I/i。原有端点标签不足以代表音乐收束，本轮按全句和边界单独审核。V6新增358条均为原创设计，未找到其作品级J-Pop/ACG出处；旧20条CITYPOP_JPOP也没有可核验的作品段落记录。本轮保留它们的原始音乐与标注。

原始定向证据：[G5审计](../../HarmonyContinuation-v7-evidence/audit/G5_FACTORY1016_AUDIT.md)。早期审计把相对小调五度链称为丸サ，已改正；旧mask=0事件按quality补实际音集后再比较，未把空音集当作有效音乐。

## 收录规模、家族与证据

| 固定稿组 | 候选 | A | B | C | 重点 |
|---|---:|---:|---:|---:|---|
| 01 | 100 | 98 | 0 | 2 | 王道及家族转换、开放副歌、闭合对照 |
| 02 | 35 | 34 | 1 | 0 | 日系摇滚应用、脆弱主音、踏板释放 |
| 03 | 100 | 20 | 44 | 36 | PR/RP、延迟落点、后副歌回返 |
| 04 | 90 | 90 | 0 | 0 | 低音设计、色彩和弦、调式段落与能量 |
| 05 | 8 | 8 | 0 | 0 | 增三和弦与独立低音的Japanese-sixth示范 |
| 06 | 6 | 6 | 0 | 0 | 真实缺失的丸サ本体及跨家族出口 |

均为逐句指定的原创完整设计；编码器不排列组合音乐，不下载商业MIDI、旋律或长篇谱例。增加用途标签不复制音乐身份。`encode.py`仅将固定PSV与明确裁决转换为现有模板格式；未有明确裁决的新ID默认B。既有PSV行次与ID对应，后续不可重排或重编号。

256条A归入27个编辑家族。覆盖矩阵（互斥编辑归类，非27种全新理论家族）：

| 方向 | A |
|---|---:|
| 王道 / 小室交叉 / 卡农交叉 / 小调五度链 | 30 / 10 / 10 / 9 |
| 开放副歌 / 段落连接 / 闭合对照 | 20 / 9 / 10 |
| 开放摇滚 / 上行平台 / 调式平台 | 5 / 1 / 7 |
| 脆弱主音 / 踏板释放 / 功能交叉 / 调式应答 | 7 / 8 / 3 / 3 |
| PR / RP / 半音枢纽 / 延迟与欺骗落点 | 6 / 4 / 2 / 2 |
| 后副歌回返 / 段内双中心 | 4 / 2 |
| 低音结构 / 色彩转向 / 调式段落 / 段落能量 | 20 / 20 / 20 / 20 |
| 跨调色彩桥 / Japanese-sixth / 丸サ交叉 | 10 / 8 / 6 |

结尾：开放149、调式落点33、转接44、欺骗13、真实收束12、歧义5。**70条**有审核的循环边界；03_052/083为50/42QN的一次性跨段收束，撤去整小节循环声明。其他用途与结尾分别保留，不把所有未落I的乐句归为错误。

归属（每条当前一个源中心解释）：Ionian212、Dorian9、Mixolydian9、Lydian8、Phrygian6、Aeolian10、和声小调1、爵士静态旋小1。A中心的稿保留tonicOffset=9，未误写为C。跨调条目的归属是起始/参照中心的语境解释，具体后续目标在structure记录；严格筛选仍按实际全部音集判断。Locrian与古典旋小不新增配额；所有新增常用程度均为unrated。

证据分层：41条家族模型、49条已发表作品分析的机制、90条作曲教学应用、62条一般调性/循环概念、8条Japanese-sixth技法、6条丸サ教学家族。**完整新路线被外部原作直接支持的数量为0**；256条均由主代理按原创用途审核。49条机制型中，29条明确绑定作品与分析位置（23条Li歌曲例、6条《Lunar Whale》的升四成分）；其余20条只对照文章机制，不充作作品级实例或使用频率。

核心资料及边界：

- [Ramage 2023](https://academic.oup.com/mts/article/45/2/238/7224663)：王道范型及调性灵活性。仅访问到原始摘要，未把付费全文中的歌曲实例写成已核实。
- [Li 2026](https://www.mtosmt.org/issues/mto.26.32.1/mto.26.32.1.li.php)：PR/RP与pre/postchorus机制。条目保留VE、作品、段落及机制对应；未复制原曲整段。小调对小调的泛化及机械近亲继续隔离。
- [Spicer 2017](https://mtosmt.org/issues/mto.17.23.2/mto.17.23.2.spicer.html)、[Duinker 2019](https://www.mtosmt.org/issues/mto.19.25.4/mto.19.25.4.duinker.html)：脆弱/涌现/缺席主音与平台循环。缺旋律和重音证据时，不声称已经实现hybrid tonic。
- [内山作曲教学](https://sakkyoku.info/beginner/popular-chord-progression-pattern/)：低音、经过减和弦、副属与借用iv的创作应用；不代表每条新路线的日系作品出处。
- [Atkinson 2019 §25/Example11](https://www.mtosmt.org/issues/mto.19.25.2/mto.19.25.2.atkinson.html)：《Final Fantasy IV / Lunar Whale》的利底亚升四成分；未重建旋律、配器或完整飞行主题。
- [Nazaré 原作者研究页](https://www.tannazare.com/research)：Japanese sixth的增三和弦/独立低音与解决关系，属技法依据。
- [FlaTools原教学页](https://flatools.com/pages/theory/pops/chords)：4361及相关家族。作品列表没有明确段落/小节，故不计入29条作品位置对照。

运行style仍采用现有pop/rock应用权重，未新增流派枚举、频率奖励或V7加分。新标签的A表示在上述边界内具有独立创作用途，不等于“典型日系作品已证实”。

## 音乐身份与排除

修正后的比较直接使用根音、实际完整音集、低音、QN及拍号，按12音级移调归一；不把mode、target、Borrowed或exactIntervals标注当作不同音乐。

| 指标 | G5基线 | V7隔离 | 新增 |
|---|---:|---:|---:|
| 数据行 | 1016 | 1272 | 256 |
| 完整音乐身份（含QN/拍号） | 1007 | 1263 | 256 |
| 无QN完整和声身份 | 990 | 1246 | 256 |
| 三和弦核心＋低音 | 854 | 1110 | 256 |
| 三和弦核心（忽略低音） | 830 | 1086 | 256 |

核心桶统一把七/扩展和弦投影到三和弦，dom7并入major，忽略拍号/QN；因此与B3-R保留部分质量/低音信息的旧桶数不可直接相减。桶数是筛查指标，采纳还需逐句用途判断。256A与基线及彼此无完整、无QN或上述核心整句碰撞。继承的9组完整重复保持原样，不改V6。

重点排除：01_083只在01_013上改扩展/时值；01_059近似旧V6_B3_03_004删末主；03_086/091分别重复03_061/066。02_006空五riff三音未定，未自动归Mixolydian。第三组大量C–G7–C前句微调及来源外推未采纳。近亲完整理由逐条保存在metadata，C/B不入实验DB。

独立数据证据：[前333稿](../../HarmonyContinuation-v7-evidence/audit/BATCH01-05_V6_SIGNATURE_COMPARISON.md)；最后6稿单独增量核对，其合并结果汇总于上表，原始乐句为batch06。全部339新稿的和弦、低音、音集、时值和拍号按独立解析核对，未重跑旧V6音乐验收。

## 必要工程验证

隔离编译及最终V7专项通过：proposal1355、experiment1272、HCA1为1,506,306 bytes。主代理另以SQLite只读逐行确认：**原G5的1016个payload字节全部相等**，原HCA前缀逐字节保留。29项source/input/tool/output/media哈希与实际文件一致；既有G5宿主/安装验收不在本轮重跑。

六条实际短输入，均完整扫描1272源；完整列表和Top3/Top10分开记录，self检查同时识别primary/supporting source：

| 查询源 / 前缀和弦数 | 实际源结果 | 解释 |
|---|---|---|
| 01_001 / 3 | 未入列表 | 相似度.652681；资格59.3159＜60 |
| 01_010 / 3 | 未入Loop | 相似度.591884＜.60；同输入有新增跨调Develop/Color可见 |
| 04_023 / 4 | Color第1 | 资格67.7679，原排序规则保留 |
| 01_091 / 5 | 未入列表 | 相似度.599296＜.60；原Resolve返回3条路线 |
| 04_052 / 3 | Loop第1 | Phrygian主音/降二设计，完整回接证据 |
| 03_008 / 8 | Develop第1 | 原调前句之后接降三新段 |

观察到的是进入列表前的资格/相似度限制，未发现shortlist预算截断，也没有“合格却因V7身份被排序压低”的证据。普通开放长句并未因内容被采纳而自动保证短前缀召回；**当前不声称所有70条循环都已验证可见**。

InputWish的01_022晚期8/9前缀仅余Fmaj7：相似度.718924、资格58.643＜60；虽然排序分62.5516，仍被minimum_score排除。此负例保留，验证未绕过基础资格。04_023前缀4为正例，资格67.7679且实际进入输入池。初始“所有自身前缀必定召回”的测试预期已按真实契约修正，数据和规则不改。

FreeWish在固定参照C、不限调式的同一池中有1272 items，其中256项/256个来源ID来自已采纳V7；该运行时口径与纯音乐指纹1263不同。01_022经选定来源子池实际draw，证明可消费；此抽取不代表256条全部逐条听评。

四组完整MIDI往返与Preview非静音通过：05_001（增三/独立低音）、04_023（替代与变属）、04_052（Phrygian）、03_008（跨调）。未重跑旧V6试听、Snapshot、UI、安装、DAW或性能矩阵。

真实失败及最小修复：工具记号解析补已有sus2/sus4/aug枚举；V7编译允许保留G5的9个历史HCA孤儿并逐条验证1016个绑定源（V6默认严格行为保持）；窄测试接受原HCA的CRLF与LF。原DB/HCA输入不重写。未改变运行时和声兼容、Unified资格、排序权重或60/.60门槛。

Enrich以01_022的9个输入事件返回5个候选。最后一次推荐计时为01_001、前缀3、完整1272源扫描，**8.3158ms**；仅为本次输入的基本耗时样本，与不同输入下的V6历史计时不作同比。没有运行多规模、十二调或分位数性能套件。

本次只构建既有compiler/FactoryV6Tests目标，最终测试PASS在[日志](../../HarmonyContinuation-v7-evidence/engineering/validation.log)最后一段。[逐组排名](../../HarmonyContinuation-v7-evidence/engineering/media/v7-recommendations.tsv)、[四组媒体索引](../../HarmonyContinuation-v7-evidence/engineering/media/v7-media-index.tsv)、[工程原始结果](../../HarmonyContinuation-v7-evidence/engineering/RESULTS.md)、[29项SHA清单](../../HarmonyContinuation-v7-evidence/engineering/SHA256SUMS.txt)。修复具体失败后只重编受影响目标；未运行默认FactoryV6Focused或G1–G5全套。最终数据编译成功后仅修正测试预期与换行判断，DB/HCA没有重新生成。

| 隔离产物 / 冻结输入 | SHA-256 |
|---|---|
| [V7 experiment.db](../../HarmonyContinuation-v7-evidence/engineering/experiment.db) | `C2DA3C51709C5B83EBC3DA880F5855EFE9FD31C9EB2213F1C4ACC1E08ACF666C` |
| [V7 factory-v7.hca](../../HarmonyContinuation-v7-evidence/engineering/factory-v7.hca) | `9E6B7E70887668ECC3F9C3579C677B51F2DA2A10F6D2B995BF2DA093BCD950B1` |
| G5 Factory6/1016 DB | `DC442EED0AB8D8BB8D5D4316FC53D83E92DABC5833019B65FD13F6D74A9FB108` |
| G5 HCA | `66F446A81CCFADEF705F5F66DA69FECE3CEA1A0DB4A715CF389C33D7CFB79BB1` |

显式CLI复用既有工具，不改变V6默认路径；[命令与输入输出约定](../RUNBOOK.md#factory-v7-隔离内容编译与窄验证)。

输出与输入路径必须分离。proposal含全部稿，experiment只含256A；HCA保留原始G5前缀并追加候选归属，消费时仍逐源绑定当前DB。B/C音乐不得因出现在proposal或HCA中进入活动卡池。

## 后续门禁

本轮停在V7内容门禁。当前可准备隔离工程集成；短前缀发现性限制与来源缺口需保留为集中裁决项。先只核对上面三条前缀的导入/模板功能、结构投影与匹配成本，判断是可据音乐证据修正的标注，还是需要局部算法提案；暂不降低.60/60或提高V7分数。多数条目仍为明确目的的原创示范，作品级风格典型性不能由本轮通过的工程检查推出。

尚无V7安装、DAW或新的用户听评；未改生产安装、真实user.db、dev.5二进制、Unified资格/评分、main、正式Tag或Release。下一阶段优先补Vocaloid、日系摇滚与游戏音乐的作品/段落证据和授权素材；不把1,500–1,700作为本轮强制行数。

主代理负责来源边界、逐句音乐/用途/中心裁决、近亲排除和最终门禁；Luna负责来源索引、定向统计、独立音集/身份核对、既有工具扩展及窄验证。
