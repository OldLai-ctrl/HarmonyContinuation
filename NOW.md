
## 长查询切句（v0.14.9）

`ContinuationEngine` 在严格匹配四栏全空时，用库的级数签名判断输入里是否有"完整乐句 + 残端"：
切点左侧等于某库乐句后缀、右侧等于某库乐句前缀，两侧都成立才切，取最短；切完用 `makeMatchQuery`
对残端重建查询再跑一遍，非空才采用。`HC_NO_CUT=1` 可关，`HC_CUT_DEBUG=1` 打印候选数。

实测（40 条 5–6 和弦查询，160 栏）：严格模式空栏 86.9% → **58.1%**；四栏全空 26/40 → **5/40**；
兜底后空栏 52.5% → **51.2%**、全空 3/40 → **1/40**。`ctest` 21/21。

切片式实现（直接改 `interpretations`）会让匹配返回空——分析结果依赖整条进行，必须重建查询。
# 当前接续状态

## 本轮工作与下一步

### 进行中：长输入只匹配最后一句（0.14.8）

用户定的口径（D61）：长输入切成乐句后，前面的句子只当上下文，最后一句进匹配；
同时给"接下一句"的邻接候选，两类混在同一列表，不分栏。库仍是乐句单元，邻接是元数据。

实现顺序：①运行期乐句切分（复用入库边界判据）；②匹配入口只喂最后一句（保留原索引映射，
不重排事件）；③邻接表进库（`library_version 12.1`，小数版本支持一并做）；④40 条查询上量收益。
本轮只落决策与计划，代码在下一轮实施。

第一版实现与实测（同日，已回退）：在 `makeMatchQuery` 里按"主音到达 + 距上一句≥2 小节"
切出最后一句，再只喂这一段。40 条 5–6 和弦查询的结果：

| 配置 | 空栏 | 四栏全空 |
|---|---|---|
| 不切句 + 兜底（0.14.7） | 84/160 = 52.5% | 2–3/40 |
| 切句（尾句≥2 和弦）+ 兜底 | 92/160 = 57.5% | 2/40 |
| 切句（尾句≥3 和弦）+ 兜底 | 93/160 = 58.1% | 2/40 |
| 切句、关兜底 | 129–135/160 | 21–23/40 |

严格模式下切句确实让更多查询过门槛（86.9% → 80.6%），但候选总量变少，兜底后的空栏反而变多。
结论：主音到达不是可靠的乐句边界，必须用入库那套"小节线对齐 + 终止式 + 前后最短长度"判据，
且切句后要重新按类别补候选，而不是简单截断。代码已回退，主line 保持 0.14.7 行为。

### 长查询不再空栏：严格失败时放宽兜底（0.14.7，2026-11-18）

第二批第一件：5–6 和弦的完整查询在四栏里全空。

- 度量脚本（工作副本 `_tmp_measure.py`）：从库里取 5–6 和弦的 BB 条目，按事件拼查询，
  逐条跑 `recommend_cli`，数各类里的 `#N` 行。40 条查询共 160 格。
- 改前：`空栏 139/160 = 86.9%`、`四栏全空 26/40`；`PERF` 行显示最高被拒资格分 57.95，
  门槛 60——整个查询只差一点点。
- 改法：`recommendContinuations` 拆成 `recommendContinuationsCore`（原样）+ 公开包装。
  严格版四栏全空且有输入时，把 `minimumMatch 0.60→0.45`、`minimumScore 60→45` 再跑一遍；
  有结果就返回并置 `RecommendationSet::relaxedFallback=true`。`HC_NO_RELAX` 可关掉兜底做 A/B。
- 改后：`空栏 84/160 = 52.5%`、`四栏全空 3/40`（24 条查询走了兜底）。
- 另外试过"长查询前缀结构松绑"（`queryPrefixStructuralFactor`，只对 n>4 的前缀打折），
  对结果无改善且带偏 3 和弦用例，默认值已回到 1.0，字段保留不用。
- UI：兜底生效时状态行追加"（严格条件下无结果，已放宽匹配）"，避免用户以为匹配质量没变。
- 诊断：`MatchRunStats` 增加 `maxRejectedEligibility/maxRejectedMatch`，CLI 的 `PERF` 行打印。
- 包：`HarmonyContinuation-0.14.7-Setup.exe` 5,772,680 B / sha256
  `ec70c46bf477d3862450372360bd79b3e73d32bca2d2fb2b055939e5739ed72f`；`ctest` 21/21。库未变（v11）。

### 色彩不再是第四栏：color 变成独立维度（0.14.6，2026-11-18）

用户问"能不能让 color 只判断续写部分、忽略用户输入"，并决定让色彩与收束/发展/循环**非互斥**。
两件事一起做了。

- **只看续写，本来就这样**：运行期 `evaluateIntentCompletion` 只吃 `candidate.continuation`
  （匹配到的 suffix），用户输入的和弦不在里面。真正的漏洞是旧 `hasExtensionColor` 把普通
  七和弦、Sus 也当成"扩展色彩"；七和弦是织体，不该让一条进行整段落进色彩栏。
- **color 变成独立分数**：库里 `intent` 只留 resolve/develop/loop 三值，另加 `color`（0–1）与
  `colorReason`。新 `evaluateColorEvidence`（`IntentCompletionEvaluator.cpp`）看续写：副属/导音
  0.95、增/减和弦 0.9、借用小和弦 0.9、张力音 0.85、借用大三/单一变音 0.6。
- **色彩视图**：`colorScore >= 0.7` 的候选在功能栏照常出现，同时再拷一份进第四栏；
  色彩栏的卡片徽章显示它真实的功能意图，其它栏里带色彩的候选显示"色彩/Colour"。
- **旧库照读**：`intent=="color"` 的旧行（本库 173 条）映射成 develop + colorScore 0.9（reason
  "legacy"），不丢语义。
- 入库结果（v11）：`resolve 1119 / develop 1019 / loop 804`；色彩 `none 1438 / secondary 736 /
  mixture 638 / altered-quality 130`，≥0.7 共 866 条。
- 库身份：4,214 条 / 19,312,640 B / `library_version 11` / sha256
  `3dfa5b1f3f119f56c59d107ad26f2023f324a8197b4e9e84f52a2ef90e12742d`。
- 包：`HarmonyContinuation-0.14.6-Setup.exe` 5,775,427 B / sha256
  `21b186bb0c60f5e7ddc0dfa504dd35389c20040bd71056c7c27f454765fed494`；`ctest` 21/21。

### 风格重标注与库版本抬到 10（0.14.5，2026-11-18）

第一批地基的最后一件：那 272 条一直挂着模板默认风格 `pop:0.55,rock:0.35` 的继承条目，现在按
自己的和声重新打了分。

- `tools/library_compile/relabel_styles.py`：把 `sequence` 解析回和弦（`_parse_token` 复用入库用的
  同一套级数解析），跑 `features` + `style_affinity`；`pop:0.55,rock:0.35` 保留为底，证据分只做
  "加维度"，不把目录原来的流行/摇滚先验抹掉。
- 结果：272 条全部重标（解析失败 0 条）。相对 v10d，functional +114、classical +129、
  anime/cinematic 各 +77、folk +43、experimental +23、blues +57；pop、rock 计数不变。
- 库 `library_version` 9 → 10：内容变了必须抬版本，否则已装 v9 的机器会报"同版本号不同内容"。
- 为了抬版本改了四处接线：`CMakeLists.txt` 的 version-10 预编译分支（与 9 同构）、
  进 VST3 包的 `factory.hca` 条件、`WishTests` 读 V5 契约库的条件、`packaging/UnifiedSetup.iss`
  的 `MinimumPluginVersion`（10 → 0.14.4）与 `FactoryWithAnnotation` 判定。
- 库身份：4,214 条 / 19,185,664 B / sha256 `db8415e9571244f45e293b88f6e789f3a9cf80fa91789d4d02a434cbd4369f47`。
- 包：`HarmonyContinuation-0.14.5-Setup.exe` 5,768,958 B / sha256 `0231a83c35f5a405c5b14de67a0d9efd260e04be1042e6a24356ecc16ef7de8e`；`ctest` 21/21。

### 库列表按骨架折叠（0.14.4，2026-11-18）

用户答复"3，折叠"。

- 库列表按"同一骨架"折叠：同一调式、同一串级数与变化音（与引擎判定 family 的口径一致）只显示一行，
  行尾标 `×N`；点开仍是代表条目。
- 没有解析出事件的条目（继承库里那批）用 `id` 单独成键，**绝不互相折叠**——否则 1,288 条会并成一行。
- 现状：v10d 库 4,214 条折叠成 3,924 行，隐藏 290 行，最大一族 7 条。
- 检索、筛选、分页都作用在折叠后的行上；`ctest` 21/21。

### 向前兼容：库比插件新时仍然能读（0.14.3，2026-11-18）

用户要求"就算现在不抬 `library_version`，后面也要抬的，要做好插件对新版库的兼容"。

- 原来 `tools/TemplateJson.cpp` 遇到不认识的风格名直接抛 `unknown style`，整个库加载失败——
  等于新版库一旦出现新风格，旧插件就完全打不开。
- 现在改成跳过未知风格名、保留认识的那几个；`LibraryStore` 在"已装库版本高于本插件"时
  返回一条 `warning`（"library vN is newer than this plugin…；unknown style names are ignored"），
  库照常可用。
- 测试从"拒绝未知风格"改成"新库能加载、未知风格位被丢掉"；`ctest` 21/21。

### 地基第一批：12/8 回收、块级去重、乐句邻接（v10d / 0.14.2，2026-11-18）

用户拍板了评审的 5 个问题：1 小节条目算最小可用乐句（且要为"乐句级发展推荐 A→B/C/D"留位）；
库版本后面要抬、插件要先兼容新版库；重复条目只折叠不删；打包走可复用流程；12/8 要回收。

- **12/8**：`build.py` 的拍号白名单从 `[1-9]/(4|8)` 放宽到 `[1-9][0-9]?/(4|8)`；
  `TemplateJson.cpp` 的拍号解析从"三个字符"改成两个小整数（1–32 / 4|8），于是 12/8 能进库。
  语料 890 首全部入内（原 858），BB 条目 2,838 → 2,926，新增 88 条 12/8 条目。
- **去重下沉**：`performance_key` 从"单元"挪到"块"（`chunk_bars` 循环内），与库实际存储的粒度一致；
  被丢弃的行写进 `_ingest_bb_v10d/candidates.json/dropped.tsv`（reason/id/song/section/unit/chunk）。
  交叉基线（与继承条目同演奏）仍丢 16 条，名单在 `_ingest_bb_v10d/dropped-compile.txt`。
- **乐句邻接**：新增 `phrase-links.tsv`（prev/next/song/section），段内相邻单元与跨段相邻单元都连线，
  给后续"识别完整乐句 A，推荐常接的 B/C/D"占位；本条目不改库格式。
- **库**：`factory-bb-v10d.db` 4,214 条 / 19,181,568 字节 /
  sha256 `9e0246e2e182d50068eadcd05af03086424da30c124589720ac6ed03a53b793c`；只读副本
  `data/library/factory-v10d.db`。
- **下一步**（同属第一批）：全库幂等重标注（1,288 条继承条目从未重算 intent/风格）；
  派生 `familyId`（同骨架只计一次 support、UI 折叠）；库版本协商与未知风格位的向前兼容。

### 风格多维化与弱起判据（v10c / 0.14.1，2026-11-18）

用户看过 0.14.0 报告后定：古典要多认（同一进行在流行和古典都常用是常态），风格是多维非互斥打分；
版本号规则也定了——不叫升大版本就只递增小版本（0.14.1、0.14.2…）。

- **古典/功能分档**：`style_affinity` 从"四个条件全满足才给 0.55"改成按证据累加：
  自然音 ≥90% 起 0.35、七和弦 ≤15% +0.15、借用音 ≤10% +0.10、转位 ≥25% +0.20、主音收尾 +0.10（封顶 0.9）；
  functional 同样分档。结果 classical 35 → **1,119** 条、functional 838 → **1,880** 条，pop 仍 3,690。
- **弱起判据**：`boundaries.pickup_length` 加"首小节时值不满"一项（`bar_total(cells[0]) < span`，span 由
  `BAR_QN[meter]` 传入），原先的周期折叠判据保留。**这条判据在本语料命中 0 条**：SALAMI 不写弱起空拍，
  `render_bars` 又把弱起直接铺进第一小节，所以 BB_0705_00 仍是 `4级起 · 停在属音`（首小节
  `F/C@1 C@1 Am7@2` 恰好 4 拍）。I05 是否改用"首和弦 ≤1 拍且非主音"判，等用户定。
- **库**：`factory-bb-v10c.db` 4,126 条 / 18,747,392 字节 /
  sha256 `2de2605f6b7405cc5e01b9bbae6f14c1cfeeb7ba7680dc61d1d12ce5f589f274`；只读副本
  `data/library/factory-v10c.db`；`ctest` 21/21。
- **安装包**：`build-installer/output/HarmonyContinuation-0.14.1-Setup.exe`，5,619,621 字节，
  sha256 `a9599cceeba32b3ff68b9890bd32226fcb85215b282268496db5e8540ecf62a1`。

### 意图三分、命名与风格扩类（v10 / 0.14.0，2026-11-18）

用户提的三件事：发展栏里全是循环、名字仍是级数串、十二大类风格没有落到新入库条目上。

- **意图**：`classify_phrase` 改成先判"怎么收尾"再谈其余——落主音→`resolve`(authentic/plagal/modal)、
  落属→`resolve`(half)、周期或"末和弦回卷到首和弦（四/五度、≥3 个和弦变化、≥2 小节）"→`loop`、
  有变化音→`color`、其余 `develop`。BB 2,838 条的分布从 develop 1261 变成
  `resolve 1076 / loop 788 / develop 500 / color 490`（全库 `resolve 1521 / loop 1122 / develop 821 / color 662`）。
- **命名**：新增 `tools/ingest/naming.py`。老句式（王道、丸之内、小室、卡农、安达卢西亚、二五一、
  Just the Two of Us…）优先，其次"某进行变式"（前三个数字相同、编辑距离 ≤2），
  否则生成式"起句 · 走向 · 收尾"（如 `4级起 · 五度循环 · 回主`）。2,854 条里纯级数名 0 条：
  句式名 53、变式 56、生成式 2,745。来源歌名/段落移进 `note`（详情面板显示），`nameEn` 是同一套的英文版，
  库里条目仍可用 `id`/歌名搜到。
- **风格**：`Style` 从 13 位扩到 15 位（新增 `classical`、`other`，UI 勾选自动多两项）。入库加两条规则：
  classical（全自然音三和弦、无七和弦、转位 ≥25%）、functional（收在主的终止）；`other` 只给完全无法归类者兜底。
  新库 classical 35 条、functional 838 条、`other` 0 条。
- **库**：4,126 条 / 18,726,912 字节 / sha256 `d73b41f2cab13ec59e30a39c738948b5d737fc5c5a5e1b31b6a7150092e532b2`，
  `library_version=9`、schema 2；只读副本 `data/library/factory-v10.db`。`ctest` 21/21。
- **本机测试注意**：`ctest` 会往 `%TEMP%` 建目录，本沙箱下报"拒绝访问"会让 5 个用例假失败；
  跑之前把 `TEMP`/`TMP` 指到工作区（`TEMP=D:/VibeCoding/_tmp_hc`）即可，不是用例本身的问题。
- **安装包**：`build-installer/output/HarmonyContinuation-0.14.0-Setup.exe`，5,615,690 字节，
  sha256 `f642619ed2ccc288753493219179083f8c9e6595b8a5bfc602eb12ab583156ec`。
- **下一步**：远端分支；BB_0705_00 弱起、`Dm7–Am–C–Dm7–F–A7` 零结果仍在 ISSUES。

### 段名修复 v9 与 0.13.0 安装包（2026-11-18）

抽检后用户定："不要设小节限制"，所以没加大节跨度上限。这轮只修 I03 的段名拼串：根因在
`tools/ingest/billboard.py:156` 存了 `(字母+名, 名)` 元组，`build.py` 再过滤成 `cverseverse`。
改成纯段名后重跑入库（2,854 个乐句单元），编库 `4,126` 条 / 18,636,800 字节 /
sha256 `cf7d654e…f243e42`，与 v8 逐条比对 `sequence`/`rhythm`/`intent`/`cadence` 零差异，
只有段名从 `[cverseverse 3 u1]` 变成 `[verse 3 u1]`。

安装包 `build-installer/output/HarmonyContinuation-0.13.0-Setup.exe`（5,641,000 字节，
sha256 `84cb8509…0e9fd`）。`ctest` 21/21。构建坑见 `LIBRARY_MANIFEST.md`：
本机必须 `/p:TrackFileAccess=false /m:1`，`cmake --build` 会静默失败。

已推到 GitHub 分支 `v9-2026-11-18`（`github.com/OldLai-ctrl/HarmonyContinuation/tree/v9-2026-11-18`，工具链 `tools/build-installer.ps1` 已加 BOM）：远端 `main` 停在 v0.11.0、与本地历史无关，所以另开分支没有覆盖任何东西，合并留给用户定。
下一步：BB_0705_00 弱起与 `Dm7–Am–C–Dm7–F–A7`
零结果仍在 ISSUES。


### 乐句单元入库（I04，2026-11-17）

库里原来存的是整个语料段（中位 9 小节 / 12 和弦），引擎又禁止截断，所以推荐必然长。现在切分放在入库层：
`tools/ingest/boundaries.py` 做小节线级边界证据 + DP，`build.py --split-units` 产出乐句单元。
3,107 段 → 3,706 个单元（615 段静态 vamp 剔除、9,017 个重复单元裁掉），条目中位 5 小节 / 6 和弦，
入库前还折叠了"每个和弦跨两小节"的写法（`CCGGAmAmBB` → `CGAmB`），1,672 条混合谐波节奏原样保留。
库 `factory-bb-v5.db` 4,732 条 / 24.0 MB 已进 `build/`，`ctest` 21/21。
`C–Am–F–G` 查询的发展栏前三从 17/20/13 和弦降到 3/4/4 和弦。引擎代码一行未改。

### 标签修复（2026-11-17）

Billboard 入库的罗马级数从第一天起就是错的（`I vi IV V` 被标成 `#III #III ii ii #VI #VI I I`，
全库 52.3% 音符带升号）。已修两处根因（级数匹配改成 exact→flat→sharp 三趟；主音改成按全曲和弦
打分），重建后带 `#` 音符降到 5.3%，新库 `factory-bb-v2.db` 4,395 条已进 `build/`，`ctest` 21/21，
`recommend_cli` 四栏结果与修前一致。同日又修了入库条目的 `intent`/`cadence`（原来全写
`develop`/`none`）：改成按句末证据推导，3,124 条短语分成 `loop 1397 / develop 976 / color 499 /
resolve 252`，复读型长串（`BB_0623_03`）从发展栏挪到循环栏。新库 `factory-bb-v3.db` 4,395 条已进
`build/`，`ctest` 21/21。仍未修：`nameEn` 的段标签拼串 bug（I03）。

### 用户报出的四个问题（2026-11-17，I02 已修，其余未修）

用户在实际使用 0.12.0 后一次报出四条，**本轮只记录、只定位，没改代码**：

1. **I01 长度与意图错位**：收束栏给的是短终止式，发展栏给的是 4 小节循环复读 4 遍的长串（`I–vi–IV–V` 实测 17 / 20 个和弦），与"收束 / 发展"的语义冲突。
2. **I02 打开 UI 卡约十秒** —— **已修（2026-11-17）**：`attach()` 原本在 UI 线程把 35 MB / 4,365 条的库完整解析两遍
   （`Controller::reloadLibraries` + `annotationsForFactory`）。现在只解析一遍并缓存，读库路径还砍掉了
   `checkTemplate` 的序列化往返与 `decodeTemplateData` 的 dump/parse 往返：单次加载 5.6–6.8 s → **0.97 s**，
   首次开 UI ≈ 11.7 s → **≈ 1.3 s**，之后每次 ≈ **60 ms**。
3. **I03 推荐名是级数串**：界面标题由和弦逐个拼成，从不显示模板名；真实语料条目的 `nameZh` 本身也是级数串。附带一个入库引入的缺陷：段名被拼成 `[aintrointro 1]`（`tools/ingest/build.py:698`）。
4. **I04 长进行违背"步进"**：库里没有"乐句/单元"这一层，条目 = 整个 SALAMI 段（中位 12、均值 14.6 个和弦），而推荐一律输出到条目末尾。

四条的共同根因、实测数字、代码行号与（未实施的）修法方向见
[ISSUES](_handover/ISSUES.md)。建议动手顺序：I02（纯性能）→ I04/I01（同一层数据：句法单元）→ I03（显示层）。

### 真实语料入库（2026-11-16）

**两份真实语料已入库；Billboard 那份（4,365 条）已随安装包进产品，产品侧现在是 v0.12.0 / Factory V9。**
Rock Corpus 那份（2,126 条）仍未安装，留在 `D:\VibeCoding\factory-rs200.db`。

- 第 1 步（Style 体系 6 位 → 13 位多选，2026-11-15）：界面改勾选列表，权重语义为"在该风格内的典型程度"，
  四档 {0, 0.35, 0.65, 0.9} 不归一化；旧库不重标，未命中时回退，已用三种风格档逐字节比对确认现有输出不变。
  见 [D24](DECISIONS.md#d24-style-从-6-位扩到-13-位多选2026-11-15)。
- 第 2 步（入库管线，2026-11-16）：`tools/ingest/` 新增 `chords.py` / `build.py` / `billboard.py` /
  `fetch_billboard.py`。Rock Corpus RS200 → `factory-rs200.db`（2,126 条，末和弦落主音 28.0%）；
  McGill Billboard 按 SALAMI 段入库 → `factory-bb.db`（4,365 条，末和弦落主音 34.5%）。
  如实记录规则见 [D25](DECISIONS.md#d25-入库如实记录弱起保留休止切分只去完全相同演奏2026-11-16) 与
  [D26](DECISIONS.md#d26-billboard-20-按-salami-段入库2026-11-16)：弱起保留、休止切分、只去完全相同演奏、
  斜杠分流、12/8 跳过、超长段按小节线二分。复现命令与改动清单在 [STATE](_handover/STATE.md)。

**下一步**：先解决长条目与短查询的错配（随机 12 条 Billboard 条目取前 4 和弦反查只有 2 条命中自己），
再决定是否把新库装进产品；`Dm7–Am–C–Dm7–F–A7` 零结果这个既有缺陷仍未修。

### Factory V9-B 完整后句与发展路线（2026-11-15）

**V9-B 结项，保留两份实验库。** 20 条新素材、19 个编辑家族。纯素材和离线补功能各有独立库。122 首确认组新增合格不同路线，实际近似后句维持 RS200 15/50、WJazzD 7/72。12 个原生接缝见证的主要限制是 Exact 落点、Contextual 的中心和实际前缀，暂无引擎改动依据。[建设与验证](docs/FACTORY_V9_B.md)

后续用冻结轨迹定位有完整来源且前缀可兼容的落点，继续保留原资格与硬约束。如改变建库规则或检索行为，另冻结新作品验收。库与证据在本地，复现与恢复从 [MAP](MAP.md#v9-b-外部证据身份)、[RUNBOOK](RUNBOOK.md#factory-v9-b-建库与独立回放) 开始。

### Factory V9-A 泛用后句建库（2026-11-14）

**V9-A 已建库并完成独立验证。** 实验库为 Library 9 / Schema 2 / 1,312 条。24 条原创编辑中 19 条通过指定接缝的精确演奏正控。97 首独立作品中 88 首获得新增合格候选，近似原后句可达仍为 18 首。保留实验库，产品采用暂缓。[建设报告](docs/FACTORY_V9_A.md)

原生接缝追踪已由 V9-B 接续。V9-A 的原库和回放仍按 [MAP](MAP.md#v9-a-外部证据身份) 与 [RUNBOOK](RUNBOOK.md#factory-v9-a-建库与独立回放) 恢复。

### B7 Factory 真实后句覆盖（2026-11-14）

**B7 已结项，建议先建 Factory V9 小批素材。** 249 个 4/4 输入中，115 个缺近似完整尾段，目录有尾段的 112 个未达输入中有 58 个进入匹配或构造后被原规则排除。三种时值条件命中同一批 22 个输入。B6 开启后近似可达无增益，B7 只交付离线分析。优先建设长发展、明确节奏和低音的变体，少量验证 12/8。[B7 报告](docs/B7_FACTORY_COVERAGE.md) · [D20](DECISIONS.md#d20-b7-以完整带接缝的素材为建设重点)

B7 原始证据与复现见 [MAP](MAP.md#b7-外部证据身份) 和 [RUNBOOK](RUNBOOK.md#b7-factory-覆盖与时值)。首批实验制库已由 V9-A 接续。B6 检索默认关闭，B5/B6 统计继续离线。

### B6 Factory 检索与统计适配（2026-11-13）

**B6 结项，保留默认关闭的单端点检索实验和离线统计回退。** 原 100 个输入恢复 1 个近似后句，67 首独立确认增加 2 个合格候选，近似可达仍为 3 首。低频和非调内概率估计改善，Factory 的可达候选规模仍不足以支持产品接入。[结果](docs/B6_RETRIEVAL_STATISTICS.md) · [D19](DECISIONS.md#d19-b6-来源落点与统计回退)。

B6 原始回放按 [MAP](MAP.md#b6-外部证据身份) 与 [RUNBOOK](RUNBOOK.md#b6-factory-检索与统计适配) 恢复，扩大时值对照与素材缺口诊断已由 B7 接续。

### B5 真实音乐续接统计（2026-11-13）

**离线评估结项，保留统计模型，B5 产品接入暂缓。** [评估报告](docs/B5_CORPUS_STATISTICS.md)记录同来源与外测的完整后句收益、非调内及低频转移的概率退化，以及 Factory 可达性限制。100 个 Factory 回放输入有 71 个在库内找到近似尾段，9 个在实际推荐列表中有近似后句。B1—B4 的结论和接入门槛见 [D18](DECISIONS.md#d18-b5-用独立作品评估完整后句)。

接手先按 [MAP](MAP.md) 核对实验分支、语料、冻结划分及模型哈希，再用 [RUNBOOK](RUNBOOK.md#b5-真实音乐续接统计) 恢复。下一步定位库内有近似尾段但未到达 Worker 列表的 62 个输入在哪一步退出，再用独立语料复核非调内与低频转移。实验成果仅保存在本地代码提交和证据目录。无 B5 插件构建、宿主测试、听评或发布。

### v0.11.0 正式发布（2026-11-10）

**v0.11.0已正式发布。** [下载唯一统一Setup及校验](https://github.com/OldLai-ctrl/HarmonyContinuation/releases/tag/v0.11.0)。Tag、正式源码、About BuildCommit及Setup固定于 **`d888e2c8d5f78a19a20a7936e763d407100b6a32`**；main保留完整历史，后续仅文档交接提交不改变该发布身份。Windows x64 Release / Factory V8 / Schema2 / 1,288条，产品仅Billboard CC0先验。

Setup：`HarmonyContinuation-0.11.0-Setup.exe`，5,213,652字节，SHA256 `ab0c1dbcd860b9d91e99701d1e9dbe2056bd0450f05111415d67d2c5597ea1c4`。Release四项资产的GitHub SHA256 digest与本地逐项一致，非草稿、非预发布。交付目录为同级`HarmonyContinuation-v011-delivery-20261110/`，证据为`HarmonyContinuation-v011-release-evidence/`；精确构建和检查范围见包内BUILD_INFO及[发布说明](docs/V0_11_RELEASE_NOTES.md)。

正式版相对已验收dev.16只调整正式元数据与发布记录，算法/产品数据/打包逻辑不变。复用414/606/75与聚焦原生UI及G13.1的198/152有效证据；正式元数据核对通过，一次官方v0.10/Factory5→v0.11/Factory8隔离升级17项通过、5个旧库/个人哨兵文件保持，生产安装与真实用户数据未触碰。用户确认dev.16在Cubase和FL Studio最小加载、生成试听、MIDI操作通过，宿主版本未提供；正式二进制未由代理重新实测两宿主。安装包未签名。当前无发布待办。

### G13-C 产品集成（2026-11-10）

用户已正面评价G13.1三条研究试听并批准C阶段。G13.1成果已保存为`b1b855d`；当前独立分支 `development/g13c-product-integration-20261109`，产品集成代码/BuildCommit **`7b4000e364dc3506bd097f07319e7e67c2b67054`**，测试版本 **0.11.0-dev.16 / Factory V8 / 1,288条**。当前[技术报告](docs/G13C_PRODUCT_INTEGRATION.md)维护实现、实际输出、许可、检查与限制。

**工程集成及聚焦UI检查完成，CC0产品音乐已获用户认可，真实宿主待验收。** 生成候选接原RecommendationWorker，灵感完整列表固定每页10条，数据库Top3后加入常规/丰富/大胆，数据库候选及相对排序保持。Snapshot4冻结独立来源、实际音集/低音/时值，可读旧1–3；Session5保持。414项集成、606项MIDI/快照、75项旧快照检查通过，F–G–Em–Am原生界面首屏1–3数据库、4–6生成已核对。旧G13.1的198/152证据沿用。

同级 `HarmonyContinuation-g13c-test-20261110/` 为可直接打开的Demo和独立VST测试构建，包含5条实际生成的D→E与最小宿主清单；`BUILD_INFO.json`记录最终身份与哈希。证据在同级 `HarmonyContinuation-g13c-evidence/`。第二输入Am–G#dim–G–D/F#实际有丰富/大胆两条，常规无合格完整路线；所选A大调解释及接缝需用户听评。产品仅嵌入独立Billboard CC0先验，与已验收Idol研究profile输出不同；研究统计未打入产品。

G13.1的3条研究音乐、语料及CLI保留于原目录；其技术快照见[原报告](docs/G131_PHRASE_GRAMMAR_REVIEW.md)。生产安装仍是用户已验收dev.14，本轮未自动覆盖、制作Setup、推送、合并、Tag或发布。当前源码本地已提交，交接提交与最终HEAD以Git核实；[本轮历史](history/2026-11/2026-11-10-g13c-product-integration.md)记录必要根因。

### 0.11.0-dev.14 安装交付（2026-11-06）

用户认可 G12 扩展试听 12 条 D→E，音乐研发阶段结项。独立分支 `development/dev14-delivery-20261106` 从 G12 交付 HEAD `dd6703c8` 建立。BuildCommit `eb05cb2070f0f9a713cabd74729540dfdf897a61`。Release x64、Factory V8 / Schema 2 / 1,288 条，DB/HCA 与 G12 字节一致。安装逻辑纳入 V8 的 HCA 打包、提取和所有权校验，祈愿绑定允许 V8 的 16 条未标注调式的新句，既有来源仍严格核对。

交付目录在同级 `HarmonyContinuation-dev14-delivery-20261106/`。`BUILD_INFO.json` 记录 Setup、完整校验、隔离升级与冒烟。[安装、卸载回退和实机清单](docs/DEV14_DAW_ACCEPTANCE.md)随包交付。一次隔离托管 dev.6 / V7 → dev.14 / V8 升级通过，V5/V6/V7 历史与个人数据哨兵保留。HCA 修复后只在已验证的 V8 上换装最终插件，落盘身份、加载和处理器试听通过。原生界面冒烟通过四入口、匹配子项、完整灵感来源、升级、祈愿一抽、Preview/MIDI/DAWClip 及 84 事件显示。后续用户已确认dev.14验收完成；G13另行离线听评，RC/正式发布未获授权。

### G12 乐句衔接与现代和声表达（2026-11-04工程交付，扩展听评通过后结项）

独立分支 `development/g12-phrase-flow-20261104` 从干净G11交付`797c15f`建立，**0.11.0-dev.13 / BuildCommit `499c69b3cbd94ce68f35921d247d39acfe5fbd89` / Factory8 / 1288**。[G12集中报告、完整新链与D/E对照](docs/G12_PHRASE_FLOW_REVIEW.md)为本轮权威记录。用户已认可G11三个方向的多样性，最新澄清不自然处主要是三条D末两个和弦似在句中却突然结束；本轮未围绕009/010频率调排序。

三条原MIDI均保留原D完整8事件与末两事件各4 QN；九个内部接缝位于小节第1拍且无空隙/重叠，WAV在最后note_off后正常释放100ms并保留150ms尾部。主要待判断的是移动中的源尾是否有独立听觉边界，不能把原源末尾直接当成已证实的完整开放句末。

已加入4条实际音集/低音/时值绑定的运行时审校补充：009/010保留原句与探索分页，撤回未经证明的独立句末资格；现成V7_04_044的末8QN模态停顿及V7_04_052的循环归位获得具体4+4分组说明。新增Factory来源0，DB/HCA与G11逐字节一致；不改全局Voicer。三条完整D/E边界对照均在16秒进入正常合格E，旧D演奏不变；不是第四轮RCR或虚构复合来源。

Release构建、8项定向检查、3组旧Snapshot/MIDI、3组D前缀、E-044及新链9组Snapshot/Preview/MIDI身份通过。唯一一批冻结50起点仍12/50（24%），全部三轮第一页可达；11条机制互异，1条宽泛机制回用。84事件一次正常查询372.897ms（复用索引）。原工程证据在同级`HarmonyContinuation-g12-evidence/identity.json`。用户认可同级 `G12_EXTENDED_LISTENING/` 的12条扩展试听，本轮转入上方独立安装交付，真实DAW由用户验收。

### G11 连续续写路径分叉（2026-11-03工程交付，音乐待验收）

独立分支 `development/g11-continuation-network-20261103` 从干净G10交付`fa2de2f`建立，**0.11.0-dev.12 / BuildCommit `c1b3743b0d3b517e44b5365666eb1014ffc2980c` / Factory8 / 1288**。[G11集中报告与三轮试听](docs/G11_CONTINUATION_NETWORK_REVIEW.md)维护交付细节。G10工作树、G9完整乐句/时间/接缝门禁及既有Exact/60、四入口和持久化身份保持。

已对原12链的第二/三轮24个语境进行一次定向诊断：每轮11–19个完整开放合格身份、9–12个家族；主要为排序与语汇重复，未证明必须新增内容，本轮新增0。建立15条受审开放句的227条一步结构连接，包含不同调域解释，不能当作227种构思。只在当前质量接近时小幅参考未来机会，补充实际相对音集/低音/张力历史比较；不递归搜索，下一轮仍重验真实约束。三代表输入首页各两个B经真实回导后，C首页仍有6–9个合格对象。

Release构建、原17/11/19合格身份保持、历史别名/机会不改资格、增量与全量轮廓一致、实际MIDI回导检查通过。初版实际性能问题修复后，84事件一次完整流程 **518.915ms**（复用索引）。唯一一批原冻结50起点 **12/50（24%）**，36轮均首页；来源8→11，最常用三来源75%→66.7%，最常见后两句6/12→4/12。**末轮仍10/12落在009/010；多轮集中只缓解，不能宣布音乐最终通过。** 三条首选试听链的后两句不同，仍都经过009；九组Snapshot/Preview/MIDI身份通过。音乐与真实DAW待验收，不制作Setup、不改生产或真实user.db、不推送/main/Tag/Release。证据在同级`HarmonyContinuation-g11-evidence/`。

### G10 语境多样性与创作覆盖（2026-11-03工程交付，音乐门禁未通过）

从干净G9交付`921b7f3`建立 `development/g10-contextual-diversity-20261103`，版本 **0.11.0-dev.11 / Factory8 / 1288**，BuildCommit **`80487ab51373e20670af656bc5f0183b107d996a`**。[G10集中报告与三轮试听](docs/G10_CONTEXTUAL_DIVERSITY_REVIEW.md)为本轮专题权威记录。用户已确认G9接缝与小节基本自然，方向丰富度仍不足；G9时间/完整性/结构门禁冻结保护。

已完成一次G9固定36轮定向诊断（前6轮＋修复诊断指纹解析后续30轮，未抽新样本）：每轮10–19个合格完整开放音乐身份，007/009/013等在34–35轮通过但中位最佳排名47/58/52，主要为排序与实际语汇相似，非全库内容缺失。本轮新增来源0。诊断资料在同级`HarmonyContinuation-g10-evidence/diagnosis-complete/`，来源原始身份与库文件不变。

已实现实际音集/低音音级/时值/张力轮廓、语境适配与历史多样性，替换混合列表的分类累积与二元优先；动态首屏保留全量分页。修复后补Exact元数据引起的轮廓非幂等，以及只采用首调中心解释造成的合法后续断路；仅采用既有局部解释并保留实际音符/接缝资格。Release构建、首批三个语境301/269/307个合法身份与资格保持、定向历史/分页及九轮MIDI回导检查通过。84事件一次完整流程395.948ms，九组代表Snapshot/Preview/MIDI身份通过；详细范围见专题。

唯一一批原冻结50起点复核 **12/50（24%）**，36轮均首页可达；首轮采用来源4→7，总来源7→8。本版最常用三来源仍占27/36（75%，G9为80.6%），009出现在全部12条成功链、010出现11次。**工程和连续性通过；初始方向有所改善，持续创作覆盖只部分达到，下一Setup音乐条件未达到。** 三条新试听后半仍共享009→010，须用户听评，真实DAW未验收。未据这批结果继续调参或重跑；不制作Setup、不操作生产或真实user.db、不推送/main/Tag/Release。

### G9 乐句连接与历史多样性（2026-11-02）

独立分支 `development/g9-phrase-diversity-20261102` 从G8交付`3fedd827`建立；**0.11.0-dev.10 / BuildCommit `f105a0b4b4dd50281cc5593bdde05dfc166d3347` / Factory8 / 1288**。[G9根因、结构门禁与三轮试听](docs/G9_PHRASE_DIVERSITY_REVIEW.md)为本轮专题权威记录。新增来源0，原DB/HCA字节不变，G7/G8工作树与产物保留。

已修复来源句尾OPEN中位数缩短、无证明局部共享的结构优先级、挂留入口漏检及历史家族集中。V6反例原40 QN保留为两小节准备＋4＋4；原4＋3来源不补拍、不截尾，未经结构审查的接缝保留为探索。Exact/60门槛、四入口、升级与祈愿语义、Session5/Snapshot3/Worker职责保持。

Release构建和定向检查通过。仅用原冻结50起点执行一次G9复核：**12/50（24%）三轮结构合格，12条链机制互异，36轮均第一页可达**；38条在首轮停止，不重抽或放宽。跨起点仍集中于010/003/014（29/36）；单链多样性改善不等于全面音乐PASS。84事件一次代表查询393.697ms（含索引建立）；同批20事件142.713ms（含makeMatchQuery、复用索引），范围不同。三条累计WAV/MIDI及9组Snapshot/演奏身份检查在专题记录。

**工程与自动结构/连续门禁为历史通过；用户后续听评认可接缝与小节基本自然，但整体仍相似，创作方向丰富度继续由G10修订。** G9产物不改；真实DAW未验收，下一Setup音乐门禁未通过。不制作Setup，不操作生产或真实user.db，不自动推送/main/Tag/Release。

### G8 完整开放乐句与递归续写（2026-10-30交付核对）

独立分支 `development/g8-open-recursive-20261027` 从G7交付`7a7021e`建立，**0.11.0-dev.9 / BuildCommit `0936a4837ed669162fc4554e5fa86ce397a5f3b4` / Factory8 / 1288**。[G8集中交付、连续路线与试听](docs/G8_OPEN_RECURSIVE_REVIEW.md)维护唯一专题记录。新增16条完整编辑句，其中15条开放、12个开放家族；一条局部小调到达保留为阶段性停顿。V7基础DB/HCA与G7冻结文件hash相同，原工作树和产物保持。

尾部8/4/3/2检索、结束分类、有限下一段索引与128事件回导路径已实现。定向检查通过：新增内容/投影、尾4/3/2、收束反例、OPEN/硬旋律、84事件MIDI/Session5/Snapshot3。修复了实际持续低音未计入共同音而误拒连接的问题；旧Exact评分与60分门槛、四入口、升级和祈愿抽样代码保持。Release插件/自动SDK47通过。

唯一一批RCR-3，seed20261027，50起点中**46条三轮通过（92%），全部成功链三轮均在第一页**；138轮实际使用完整算法跨段。四个失败起点均为非4/4，本轮开放增量全4/4，未强行改拍号。公开OPEN回导场景、符号重解释、来源集中及代表性20和弦输入96.813ms的范围见专题报告。Session5/Snapshot3不迁移，超过64事件的新对象需新读者。**工程与连续门禁为历史通过；用户后续听评指出乐句逻辑、四转三组织及少数模式循环，创作质量门禁未通过，真实DAW未验收。**反馈及整改以G9专题为准，原G8产品/RCR证据保持冻结。不操作生产或真实user.db，不自动推送/main/Tag/Release。

### G7 四入口与灵感首页（2026-10-27）

独立分支 `development/g7-four-entry-20261027`；实现/插件BuildCommit **`58dfce0fb78c712def4902d81b8103747b746ea2` / 0.11.0-dev.8 / Factory7 / 1272**。从G6交付头`8da40f7`建立，dev.6/V7及dev.7产物保留。[集中交付、五条音乐与听评状态](docs/G7_FOUR_ENTRY_REVIEW.md)，长期架构见[D13](DECISIONS.md#d13-g7-四入口与双推荐视图)。

四入口为匹配优先（四子项）、灵感优先（混合）、升级进行、和声祈愿。双结果集支持一级导航复用，保留输入、试听和固定对象；原Exact分数/60分资格、升级和祈愿算法不改。实际灵感首页为Exact2/语境5/算法跨段3，稳定2/丰富4/大胆4；全量合法对象分页可访问，未按比例凑数。

工程通过：Release插件/Demo、单输入固定尺寸原生UI专项通过；旧Exact30条原始分数/来源/音乐身份对照保持，真实G1/G2约476/201ms，延迟G1拒收、G2显示。Factory DB/HCA字节与dev.7相同，Session5/Snapshot3未迁移；G6的13组演奏身份检查未重复。**五条音乐首轮听评已完成：用户均认为可接受，但缺乏后文灵感，4、5相对有意思，4仍在新调终止；创作启发目标未通过，真实DAW未验收。**定向核对发现发展资格允许换调后收束，现有若干调式展开/循环候选未进入已保存首页；细节与对照来源见集中报告。下一步先修正用途判断与首页发展选择，再做少量对照听评；暂不制作Setup。此次仅更新验收记录，源码与BuildCommit保持；未操作生产或真实user.db，未推送/合并main/建Tag/Release。

### G6 / Harmony Engine V2（2026-10-27）

独立分支 `development/g6-harmony-engine-v2-20261027`，实现/插件BuildCommit **`03fad728769de139d5016abb02db2a7f0e151cb9` / 0.11.0-dev.7 / Factory7 / 1272**，接手最新V7集成文档头`4a0402f`；原dev.6安装包及工作树保留。[集中交付与试听](docs/G6_ENGINE_V2_REVIEW.md)维护算法、音乐判断、三输入及兼容边界；[架构](docs/CONTINUATION_ENGINE.md#g6--harmony-engine-v2)维护实现语义。

完整/尾三/尾二语境与算法跨段已接入，独立资格和质量层级后作多样化排序，新增运行期“灵感优先”。F–G–Em–Am由旧30条（发展可见0）变为294条（发展可见122）：Exact30、语境39、跨段225；旧30条指纹与分数保持。三输入最终约563/430/367ms，13组来源/Preview/MIDI/Snapshot3一致性通过，Release插件编译和二进制身份通过。首批和声结构已有明显不同路线，实际听感与真实UI/DAW仍待用户验收。

Factory DB/HCA字节未变，Session5/Snapshot3/User Schema2未改；Snapshot冻结演奏与来源ID，新通道/中心/接缝元数据暂只在运行期，重开有明确提示。收束与祈愿旧语义保持。**算法与结构检查通过，创作启发门禁未通过**；五条首轮听评及后续定位归入上方G7记录。未制作Setup、未安装、未操作真实user.db、未推送/合并main/建Tag/Release。

### Factory V7-I 独立集成交付（2026-10-26）

独立分支 `development/factory-v7-integration-20261026`，从内容冻结`8cc16c1`建立；唯一Release插件为 **0.11.0-dev.6 / BuildCommit `40210d37c3fce517429fbf39f7a4c20c8709d5fa` / Factory7**。V7为1272条、1263个完整音乐身份，新增256A，83条B/C继续隔离。V7 DB/HCA与内容冻结哈希完全一致，旧V6中间产物与dev.5一致；Session5/Snapshot3/User Schema2、原评分与60分门槛、完整扫描和G5 UI/试听逻辑保持。[集中报告与Setup身份](docs/FACTORY_V7_REVIEW.md#v7-i-独立集成与开发交付2026-10-26)。

一次Release构建、一次独立fresh install、已装1272/HCA读取、代表推荐/Wish/Enrich、四组MIDI/Preview及模块版本检查通过。测试保留V6历史文件和个人哨兵；旧active夹具误写，未证明有效active6升级迁移，未补跑生命周期。Color新路线第1、跨段Develop第1、非传统Loop在完整列表第21；开放发展样本可经Free Wish抽到，短前缀仍受资格限制。三条已知限制保留，已证实Unknown功能惩罚成分；最小语义提案交决策端，未改算法。

[dev.6 Setup](../HarmonyContinuation-v7i-delivery-20261026/HarmonyContinuation-0.11.0-dev.6-Setup.exe)及[四条创作样本/MIDI/WAV](../HarmonyContinuation-v7i-delivery-20261026/CREATIVE_SAMPLES.md)可进入实机体验。dev.5 Setup/V6保留可回退。工程与隔离安装已验证；正常Setup未执行，生产插件和真实user.db未操作，DAW及用户体验待验收。仅本地提交，未推送、合并main、建Tag或发布；本轮不继续扩库。

### G5 集中整合（2026-10-25）

独立分支 `development/g5-integration-20261025`，将 A 线源码 `f147aaf3` 合入 B/Locrian `655fcbb`；原工作树及既存未提交协作文档不动。目标 **0.11.0-dev.5 / Factory6 / 1016 / Schema2 / Session5 / Snapshot3**。清空卡池、替换/保留历史、共享循环试听与 V6 外置 HCA 全部保留；古典旋小仍无已审核归属。实际数量来自查询，不降低硬约束。

源码/BuildCommit **b3d1fce6225593c76bc98ca0dec083bc469c5e00**；一次 Release 构建成功，5核心+UI+隔离升级/模块加载+新增升级路径故障恢复+清理共9场景通过。[Setup与完整身份](../HarmonyContinuation-g5-delivery-20261025/BUILD_INFO.json)、[实际范围和精简Cubase/FL清单](docs/G01_G4_ACCEPTANCE.md#g5-集中整合交付2026-10-25)。未执行历史矩阵、抽卡大检查或完整SDK Validator。正常包SHA `F5889E80…03642DBC8`，插件/DB/HCA在构建、暂存、正常Setup解包和隔离实装一致。

只对已知且受管理、哈希匹配的 dev.4 V6 DB/HCA 开放同版本系列增量升级，替换前将旧对保留到 Factory/history/6-<旧DB SHA>；其他未知冲突仍拒绝，恢复事务保留。after-factory注入失败退出3，37受管文件和登记恢复；重试升级0、清理0。V5、生产及旧dev.3/dev.4 Setup/DB/HCA保护哈希不变。真实user.db未访问；实际DAW/标准路径UAC/旧工程重开/双实例/硬件音频及人工听评Pending，英文少量旧底部文案仍待完善。不启动RC或发布；仅推送独立开发分支，交付记录提交与BuildCommit分开。

### Factory V6 空调式卡池专项（2026-10-25）

独立分支 `development/factory-v6-empty-modes-20261025`，接手 `11dda7d`；dev.4 / `5ae1b5a` 和 1014 条数据库不变。[独立两条 Locrian 候选、音乐审查与实际结果](data/factory-v6/increments/20261025-empty-modes/review.md)。旧 Locrian 059/060、古典旋小057均未审核且未进入实际DB；加载正常。古典另外缺少严格旋律方向校验，不用静态音集代替，归属/严格池维持0。UI显示真实0及原因，加载失败显示未知，修正“爵士旋小为空”的过期提示。

本轮 Release 窄测试及原生UI离屏渲染通过：Locrian增量固定C普通/严格各2，十抽2后耗尽，不固定主音单池24；两条完整音乐身份及核心家族独立，DB/HCA指纹、Preview/MIDI/Snapshot3往返通过。100%/150%截图已检查。独立输出 `../HarmonyContinuation-v6-empty-modes-20261025/` 的1016条测试库明确禁止安装，不替换dev.4。首轮新数据非法内建标签已修正后复跑，未降低校验。主代理负责中心与古典语义、UI及门禁；Luna负责既有标注核查、窄测试实现与独立哈希核对。

dev.4 DB/HCA/Setup SHA分别保持 `3F6A17BB…53707BF9` / `D3C3DB46…1389C90A` / `C61B0766…77C4928A`，匹配其BUILD_INFO；V5源DB保持 `F989A83E…A6B48CB3C`。不访问真实user.db，不改变评分/Schema2/Session5/Snapshot3/安装器，不重跑其他调式或全量回归。建议进入下一独立开发构建的整合审查，先建立新内容身份；Locrian人工中心听评、实际DAW与安装验收仍Pending。本轮无新插件包/Setup，无推送、合并main、Tag或Release。

### 工作包B / Factory V6 B4 集成交付（2026-10-25）

独立分支 `development/factory-v6-b4-20261025`，接手`aa42b56`；冻结源码 **5ae1b5ab4294e2e0d51b02372639579a35ead7e5**，Release x64 **0.11.0-dev.4**。[开发Setup与完整身份](../HarmonyContinuation-v6-b4-delivery-20261025/BUILD_INFO.json)、[集中验收与实机清单](docs/FACTORY_V6_REVIEW.md)。Factory6/1014及HCA与B3-R原产物逐字节一致；V5及旧包保留。工作包A UI未合入。

本轮接入开发构建、外置HCA绑定及V6安装组件，Factory+User沿用完整扫描，评分/60分/排序/展示规则不变。原HCA含1023来源/1092归属，读取时仅投影并逐源绑定当前库1014来源；未采纳9源不进池，磁盘数据和审校状态不改。Format1/2MiB、Schema2、Session5、Snapshot3不变。

B4技术门禁通过：Release构建、SDK47/47；四个代表输入覆盖四续写组、升级4候选；free/inputWish、固定C五调式严格筛选、两卡Preview/MIDI/Snapshot身份及隔离User完整检索通过。dev.2/V5→dev.4/V6隔离升级退出0，实际模块加载、活动V6/1014和1080025字节HCA加载通过；卸载退出0并保留V5历史和个人哨兵。首轮HCA来源计数断言在投影前失败，已修正并通过；没有重跑B3内容矩阵、G4-R58或新增故障注入。

生产插件、V5、正式与旧开发Setup未变，真实user.db未访问。标准路径/UAC、真实Cubase/FL、工程重载、多实例和人工听评仍Pending；主代理负责版本/事务契约与最终门禁，Luna完成局部工程及窄检查实现，主代理接手最终构建/安装。停在B4技术门禁，未推送、合并main、Tag或Release。

### 工作包B / Factory V6 B3-R（历史，2026-10-25）

分支 `development/factory-v6-20261024`，接手`25d3308`；完整扫描R1提交`1f0e428`、恢复810基线`047d160`。[本轮集中报告、来源与产物](docs/FACTORY_V6_REVIEW.md)。最终隔离V6 **1014条 / 1005个跨移调完整音乐身份 / 965核心家族桶**，374候选中356A采纳，1B/17C隔离。恢复20A；新两批204采纳202A，2C为仅扩展/节奏近亲；025/052去不可靠genre标签后B→A，音符不变。

默认shortlistLimit=0完整扫描，Factory+User合并路径与输入Wish使用同一策略；评分/60分门槛/去重/排序/每组展示上限及free Wish抽样逻辑不改。实际HCA超1MiB后将统一读写上限调2MiB，格式1及字段/记录限制不改。R1窄样例、R2 20源和R3 204源各自一次增量检查通过；仅4新样本MIDI/Preview，另定向annotation-only检查通过。最终1014源单次代表推荐2.0787ms，无多规模/12调/全输出/安装/DAW重测。固定C严格池Dorian43、Lydian39、Phrygian28、Mixolydian41、爵士旋律小调19；Locrian/古典双向仍0。

B3-R内容与工程门禁交付，建议进入B4集成评审。新版V6需配套HCA和本轮读取器；实机/安装验收待B4。V5 658及真实user.db保护边界保持，790/810独立库可回退；未安装/激活V6、未推送/合并main/Tag/Release。普通风格标签为编辑应用建议，可靠作品级风格/频率证据仍是内容缺口。下列G01记录保留为历史基线。

### G01 / G4-H 真实环境验收（2026-10-24，Pending）

唯一受控候选保持 **0.11.0-dev.2 / f99dfa1657b04db0af50d76795141ea24b50ae10**，接手交付记录f2c3ee6。未重打包或修改二进制；本轮只读重核Setup SHA为 `08B0D6516BE1BF6EF5F4FD48D0FC523C9B221D6B09DBE1B928201C8C8D621ABA`，候选模块版本/提交/哈希与冻结身份一致。[当前证据](../HarmonyContinuation-g04h-acceptance-20261024/identity-and-environment.json)、[实机逐项清单](docs/G01_G4_ACCEPTANCE.md#g4-h真实验收进度2026-10-24)。

本机标准路径仍为生产0.10.0-rc.1，Factory哈希保持V5基线；尚未获准替换为dev.2，已请求用户指定测试环境及DAW操作者。本机Cubase可执行文件元数据15.0.30 / 15.0.30.287；未启动或测试，FL Studio版本待提供。标准路径/UAC、新版标准安装后加载、Cubase/FL、双实例与音频所有权、实际抽取听评全部Pending，不将G4-R的58/47历史结果当作本轮实际执行。

202诊断/恢复guard已对照源码审查；[人工处理说明](docs/INSTALLER_AND_LIBRARY.md#dev2恢复失败与人工处理)补齐保留日志与整个恢复目录、journal逆序恢复受管理文件、再恢复精确登记、核验后清标记的维护者流程，并修正旧“直接重跑修复”建议。未故意制造不可恢复状态，无人工恢复演练PASS。主代理负责包身份、恢复文档和RC判断，Luna进行只读恢复契约检查。

当前工程基线可继续验收，但真实环境和音乐质量证据不足，**不建议冻结RC1**。等待获准环境及实际操作者结果；既存文档改动保留，未合并、推送、Tag或Release。

### G01 / G4-R 安装失败恢复（2026-10-24）

恢复专项技术门禁通过，可进入RC候选评审；真实Cubase/FL Studio、标准路径/UAC及人工听评仍待验收。本地分支 `development/g01-chord-wish-20261023`，新开发构建 **0.11.0-dev.2 / Release x64**，冻结BuildCommit `f99dfa1657b04db0af50d76795141ea24b50ae10`；后续测试/文档提交与构建身份分开。唯一新[Setup](../HarmonyContinuation-g04r-delivery-20261024/HarmonyContinuation-0.11.0-dev.2-Setup.exe)、[身份和证据](../HarmonyContinuation-g04r-delivery-20261024/BUILD_INFO.json)、[根因/修复及人工清单](docs/G01_G4_ACCEPTANCE.md)。旧dev.1/fb68a6e包保留为失败诊断资产，禁止正常分发。

- 根因与修复：原关键状态写入晚于Inno原生提交，异常不改变成功状态；旧恢复遗漏支持文件/卸载登记且不核验。关键写入已提前，修改前捕获受管理文件/哈希/卸载器/完整登记；失败按顺序恢复并验证，关键失败返回非零。无法恢复时保留材料并阻止重试，不重写安装器或扩大个人数据删除范围。
- 本轮执行：SDK Validator 47/47；隔离生命周期58项通过。原组件状态写入失败、Factory后中断、manifest后中断均exit3，注册后升级/新装中断均exit201。四份升级故障快照中全应用目录/卸载器、登记键值/类型/子键、插件、Factory树/活动指针、V4及个人哨兵前后一致；[故障证据](../HarmonyContinuation-g04r-delivery-20261024/isolation-lifecycle-1/upgrade/test-evidence/)。恢复失败202分支经代码审查，未人为制造不可恢复状态，不计执行PASS。
- 生命周期：新装失败后正常重试、正式0.10.0原脚本/原载荷升级、组件增删及损坏修复、直接静默降级安全拒绝、卸载开发版后重装正式版回退均通过；旧/新真实模块SDK加载和moduleinfo校验通过（工厂/类信息查询，非DAW）。最终隔离卸载exit0、测试注册/托管组件清理、用户哨兵和V4历史保留，见[清理证据](../HarmonyContinuation-g04r-delivery-20261024/isolation-lifecycle-1/cleanup-result.txt)。最终测试脚本两项审查修正仅做语法/直接检查及局部bundle查询，未重复整个生命周期。
- 身份与保护：Setup SHA-256 `08B0D6516BE1BF6EF5F4FD48D0FC523C9B221D6B09DBE1B928201C8C8D621ABA`；构建、暂存、解包及实装插件一致。Factory源码/正式包/新包/实装均为V5/Schema2/658，SHA `F989A83EA9EF92BA7C272BF0E5E3C7073C225899DC81D60DD46C10EA6B48CB3C`。生产插件仍0.10.0-rc.1，前后hash不变，生产Factory不变，真实user.db未访问。Schema/Session5/Snapshot3和调式数据不变。
- 范围与后续：仅安装机制及版本身份重建；G3音乐/UI/原生图和G4标注/音乐12646项证据按未改源码范围复用，未当作本轮新执行。主代理负责根因、事务修复和最终门禁，Luna负责隔离测试及独立只读检查。下一步由决策端审查进入RC候选及实机验收；未推送、未合并main、未建Tag/Release，既存协作文档改动保留。

### G01 / G3 和弦祈愿 UI（2026-10-23）

本地分支 `development/g01-chord-wish-20261023`，G3接手 `58222d0`（包含G2核心及全库标注；`0f0ff63`为其祖先）。实现第六同级入口及A3详细/简略卡片，技术门禁通过后停在G3，等待决策端审查G4。开发构建版本为 `0.10.0-g03.dev.1`；精确源码提交与二进制BuildCommit分别在Git和产物内核对。

- 界面：六入口共用现有布局，800×780及100/125/150%缩放；快捷预设和进阶多选共用 `WishSettings`。默认自由随机，独立输入关联/清除、12主音、10种调式身份（自然大小调含中古同义身份）、严格筛选、六类现有风格及结构色彩方向。未审核归属不进入调式池，数据不足返回实际数量。
- 对接：`WishWorkflow`在独立非实时worker调用G2 `makeFreePool` / `makeInputPool` / `WishRound`，没有另写抽样或资格算法；保存同批次一抽/十抽/再十抽的无放回状态。generation/revision阻止旧任务覆盖；清除和参数/输入约束变化清理结果及手势。自由模式不消费全局进行、调性分析或推荐历史；清除不写Session或用户库。
- 对象：卡片持有 `shared_ptr<const WishItem>`，来源、可选匹配、调式中心及常用程度与完整路线绑定。复用来源播放带、原输入对齐、Why/色彩说明、PreviewTransport、MIDI和Snapshot3。自由卡完整来源/空续写；输入卡保留Unified资格和匹配边界。Controller验证当前对象后取冻结路线；自由卡不受旧“仅后续”试听范围影响。
- 实际验证：Release插件构建成功；现有 `WishCoreFocused` 12646项、`UILayoutTests` 146480项通过，[首轮四目标日志](../HarmonyContinuation-g03-validation-20261023/tests-first.log)。新增覆盖是异步适配、风格/色彩过滤及第六入口交互缺口。`CompactEditorUI` / `WishEditorUI`在最终界面修复后通过，[UI最终日志](../HarmonyContinuation-g03-validation-20261023/tests-ui-final.log)；含真实V5、连续21抽、无自动播放、空输入/空池、跨页、窗口重开、上下文变更、MIDI/Snapshot音乐身份及离线试听非静音检查。插件构建自动执行SDK Validator，47/47通过；未运行无关全量CTest。首次构建遇到SDK调试符号C1902，本轮验证环境使用编译器 `/Z7` 嵌入式调试信息后通过，未改SDK或跳过编译检测。
- 原生证据：[100%简略](../HarmonyContinuation-g03-validation-20261023/build/wish-ui-evidence/wish-100-compact.png)、[150%详细](../HarmonyContinuation-g03-validation-20261023/build/wish-ui-evidence/wish-150-detailed.png)、[150%进阶](../HarmonyContinuation-g03-validation-20261023/build/wish-ui-evidence/wish-150-advanced.png)、[输入匹配](../HarmonyContinuation-g03-validation-20261023/build/wish-ui-evidence/wish-input.png)。使用真实VSTGUI离屏绘制/PNG；首轮中间截图缺少绘制配对导致空白，修复测试绘制后重新生成并人工核图。
- 来源与边界：版本化标注嵌入开发二进制，并测试与v2文本目录一致；正式Factory内容及Unified/Schema2/Session5/Snapshot3未改。本轮构建DB与实际已安装V5的SHA-256再次一致，均为 `F989A83EA9EF92BA7C272BF0E5E3C7073C225899DC81D60DD46C10EA6B48CB3C`。窗口关闭/重开在控制器内存中保留祈愿回合；祈愿参数、聚合来源标签及调式目录不写入Session5或Snapshot3，Snapshot3冻结实际音乐、主要来源与可选匹配。
- 分工与待办：主代理负责UI/Controller、音乐语义、架构和最终门禁；Luna负责worker适配、定向测试与只读审查。原有协作文档改动保留。G4尚待开发Setup/打包哈希、安装生命周期、真实Cubase/FL Studio操作及人工听评；正式Setup包内DB直接比对的既有缺口保留。未推送、未合并main、未建Tag或Release。

### G01 / G2 和弦祈愿核心基线（2026-10-23）

分支 `development/g01-chord-wish-20261023`，接手HEAD `99310f1`，基于正式源码 `c8d8dc3`。G1由用户结项并批准G2；本轮仅完成独立多调式标注及双路径抽样核心。规格和全库编辑标注在[标注目录](data/mode-annotations/v2/README.md)，已批准决定见[D12](DECISIONS.md#d12-g01-多调式归属与独立抽样g2)。以上为G2交付边界；当前G3技术状态见上节，G4尚未验收。

- 实现：精确ID/源音乐身份绑定、多归属与编辑常用程度、明确的未标注/未审校/无法确认/失配状态；纯随机输入隔离、Unified原资格约束路径、12主音、多选并集/严格筛选、完整音乐身份去重、固定种子及连续无放回。输入路径保留全局shortlist预算并逐源资格判定，避免引擎原后缀聚合丢失不同来源前缀。相关原评分、Factory、Schema2、Session5、Snapshot3未改。
- 数据（本次完善）：658条全部有来源绑定记录，共727个归属；618条全部解释已审校、15条兼有已审校/未确认解释、25条仅有未确认解释。633条至少有一个已审校解释；69条多归属。687个已审校归属覆盖7种调式，40个未确认解释不参与调式筛选；Locrian及两种旋律小调缺乏充分证据而保留空池。无约束实化去重仍为7788条完整音乐。评级与音乐证据见标注目录，用户实听待完成。
- G2核心首次验证（本次标注完善前）：Release / HC_BUILD_PLUGIN=OFF，`WishCoreFocused`报告1001检查通过；`MidiExportTests`598、`PreviewTests`129、`ContinuationEngineTests`323/323通过。覆盖真实V5加载、源失配、多归属、12调、完整前缀身份、单轮抽样与完整/空续写Snapshot3及MIDI字节往返。[首轮日志](../HarmonyContinuation-g02-validation-20261023/tests.log)中Wish曾因合成功能投影缺失而失败；修正夹具后[Wish最终日志](../HarmonyContinuation-g02-validation-20261023/tests-wish-final.log)通过，其他三个目标未重复运行。默认Debug探测曾报C1902，改为Release探测后构建成功；未跳过编译器检测。
- 本次标注完善实际验证：Release / HC_BUILD_PLUGIN=OFF，扩展现有 `WishCoreFocused`，12585项检查通过，涵盖全658条记录/源指纹、完整音符严格池、真实未确认条目的无约束可抽/调式排除，以及既有输入隔离、抽样、Preview/MIDI/Snapshot3往返通过；结果见[最终日志](../HarmonyContinuation-g02-validation-20261023/tests-annotations-v2-final.log)。只重跑这一相关目标，独立历史测试未重跑；实际安装库与构建库哈希再次一致。
- 来源证据：[哈希核对](../HarmonyContinuation-g02-validation-20261023/source-hashes.log)显示本轮源码重建库与已安装V5逐字节SHA-256一致，均为 `F989A83EA9EF92BA7C272BF0E5E3C7073C225899DC81D60DD46C10EA6B48CB3C`；正式Setup本体与其既有哈希一致，包内DB尚未直接解包比对，继续保留此证据缺口。
- G2核心工程验证通过；后续G3技术状态见上节，真实宿主、用户实听及开发Setup留待G4。未推送、未合并main、未建Tag或Release；接手前既存协作文档改动保留。

### 正式基线与既有整理

2026-10-23：当前发布版本 **v0.10.0**，正式源码以 `v0.10.0` Tag 固定，main 为正式发布主线。[正式 Release 与安装包](https://github.com/OldLai-ctrl/HarmonyContinuation/releases/tag/v0.10.0)提供唯一 `HarmonyContinuation-0.10.0-Setup.exe`、SHA-256 和精确 BuildCommit；功能与已验收 RC1 相同，仅调整正式版本元数据、构建配置及文档。

用户已明确确认 RC1 / `435ed85` 在 Cubase 和 FL Studio 实机全部验收通过，含播放手柄、升级高亮、起点曲线；此为用户提供的 RC1 证据，正式二进制未另行 DAW 实测。正式构建的相关 CTest、Validator、安装后加载、Factory V5/658、Session/Snapshot/用户库、RC1升级与数据保护的实际结果随 Release 发布；不把历史通过数字当作新的运行结果。

Factory V5 / 658 / Schema2、Session5 / Snapshot3、推荐质量与音频实现不变。发布与回退说明见[正式更新说明](docs/V0_10_RELEASE_NOTES.md)及[安装文档](docs/INSTALLER_AND_LIBRARY.md)，原RC技术证据见[RC报告](docs/DEV15_RC_REVIEW.md)。完整身份与最终门禁见Release资产。

2026-10-23 仓库精简：按用户确认，本地及远端仅保留 `main` 和正式 Tag `v0.6.0`、`v0.8.0`、`v0.9.0`、`v0.10.0`；撤除过程分支、四个阶段Tag、三个临时工作树及旧未发布交接草稿。精简前全部引用和未提交文档已在同级 `_archive/HarmonyContinuation-repo-cleanup-20261023-180326/` 冷备份，包括唯一未合并的两行状态文档提交。保留主工作区协作文档改动、完整主线提交历史及正式发布资产；本轮仅维护文档，不重建插件或移动正式Tag。正式 BuildCommit 仍为 `c8d8dc364b616e8d75402d977fa0cb9083d760e6`；后续文档提交与构建身份分别核实。以下开发分支及草稿记录均为历史。

### dev.14 历史交付（本轮用户报告Cubase验收已全部通过）

2026-10-19：dev.14 推荐展示与匹配解释升级完成。开发分支 `development/recommendation-browser-20261019`；源码 / BuildCommit `b4e6f65ebefad14e7345d797d941af6d49abbb5d`（`b4e6f65`）已推送 origin 同名分支。唯一测试包：[0.10.0-dev.14 Setup](../HarmonyContinuation-dev14-delivery-20261019/HarmonyContinuation-0.10.0-dev.14-Setup.exe)，Release / Factory V5 / Schema 2 / 658条。包、moduleinfo、二进制版本与提交一致，暂存插件/Factory与最终构建字节一致；dev.13源码和安装包保留。

- 推荐浏览：五类入口保留；所有展示合格候选进入同一滚动列表，每页最多10条。Top3仅为优先标识，探索有独立标签。引擎取消资格之后的每类10条截断；检索预算、硬约束、中性资格门槛60、精确聚合、评分与用途策略不变。不能将新增可见数量解释为音乐质量提升。
- 总览：当前分类全部结果与主卡片共享最终索引顺序；可定位到正确页及卡片，复制含来源、指纹、匹配位置、插入/跳过计数及用途依据的完整文本。升级按已有润色→丰富→进阶顺序合并，不作跨组重评分。翻页、分类、详情、试听和MIDI使用原候选引用，刷新清理失效选中及手势。
- 对齐：上方完整来源播放带保留数据库和弦、QN与级数；下方原输入对照消费Matcher轨迹。R01实际得到 Am→iv、G#dim→输入插入、G→III、D/F#→VII、Em→i；模板跳过保留独立红色缺口，输入插入为褐色，均有文字及OPEN/续写边界。没有原query的Snapshot仅显示冻结路线，对齐标为不可用。Session5 / Snapshot3 / UserLibrary格式及Factory数据未修改。
- 自动化：[首轮相关目标](../HarmonyContinuation-dev14-validation-20261019/tests.log)中7项通过；UILayout发现小窗口候选区被分页条挤压，移到固定试听栏末行后，[最终UILayout与CompactEditorUI](../HarmonyContinuation-dev14-validation-20261019/tests-final-ui.log)通过，共9个不同定向目标。新增覆盖0/1/3/4/10/11/20/21分页、完整列表/总览同序、跨页动作身份、保存意图恢复与失效选中、11条合格路线的有限/无限保留同资格同分、trace/缺口与旧快照降级。未重复全量CTest、Validator或安装生命周期。
- 音乐集成：[8组真实Worker检查](../HarmonyContinuation-dev14-validation-20261019/integration/unified.txt)通过，3493检查；C–G–Am保留15634125、四和弦Loop、R02两种节奏的Color、借用iv回接及R01一条默认/三条探索均保留。308是八组结果累计，包含重复输入，不作为覆盖或质量评分。
- 原生界面：[R01](../HarmonyContinuation-dev14-validation-20261019/R01-ui.log)、[模板跳过](../HarmonyContinuation-dev14-validation-20261019/template-skip-ui.log)、[七和弦来源](../HarmonyContinuation-dev14-validation-20261019/source-route-ui.log)通过；含100/125/150%入口与点击、详情、MIDI分发，七和弦离线音频及2种配音×3范围MIDI/Snapshot一致。[四张交付截图](../HarmonyContinuation-dev14-delivery-20261019/screenshots/)为真实VSTGUI渲染；模板跳过使用既有M05测试数据，不加入Factory。

工程、定向自动化、原生渲染与唯一测试包完成。主代理负责候选保留/来源语义、UI集成和最终复核；Luna完成只读链路审计、纯展示模型与性质测试补丁、状态风险审查。未修改独立Phase7工作树，未合并main、未建Tag/Release。现有未提交协作文档保留。

安装、Cubase实机与人工音乐听评均为Pending。关闭Cubase后安装主程序及Factory V5，About核对 `0.10.0-dev.14 / b4e6f65`；复验分页/分类/总览、试听及完整/续写MIDI与拖放对象、保存重开和旧快照。听评原来源与输入差异、R01探索及借用iv回接。旧Snapshot缺原query时无法恢复原输入对齐；总览过长文本可复制完整清单。可进入RC技术评审，人工验收和正式发布待决定。

### dev.13 已交付基线（历史）

2026-10-19：dev.12 同步审计与新增输出/UI要求完成。当前分支 `development/source-route-ui-20261019`，生产源码 / BuildCommit `0db40a3681a2ed0d7b4ebf4599e4d7902da6cccc`（`0db40a3`）已推送 origin 同名分支；后续状态文档提交不改变构建身份。唯一新增测试包：[0.10.0-dev.13 Setup](../HarmonyContinuation-dev13-delivery-20261019/HarmonyContinuation-0.10.0-dev.13-Setup.exe)，Release / Factory V5 / Schema 2 / 658 条；二进制、moduleinfo、Setup版本一致，暂存插件及Factory与最终构建字节一致。原dev.12包保留。

- 主界面：收束、发展、循环、色彩、升级进行五个同级入口。升级复用A3卡片、逐和弦QN、More、试听/MIDI/详情和固定试听栏；不修改独立Phase7工作树。
- 输出：输入作为检索条件保留在顶部和Session；选中候选使用其真实代表来源的完整路线，从头包括匹配位置前的和弦，采用库中性质、扩展、低音、转位、节奏和拍号。循环保留已有有限回接；显式旋律硬约束仍需通过。评分、门槛、用途证据及精确续写聚合不改；多来源仍由统一引擎选择完整代表解释。
- 实际路线检查：真实V4_B12合法匹配4–6/7，通过隔离该库条目的定向检索验证 Cmaj7–Ebmaj7–Dbmaj7–Cmaj7–Fmaj7–G7–Cmaj7，7和弦/28QN，续写边界24QN；全库Worker保留其来源证据，不要求它固定胜出代表竞争。音频渲染、两种配音×三个MIDI范围与快照读回一致。
- 兼容：新快照用现有Snapshot3字段冻结实际演奏前段/后段；旧快照继续使用内嵌音乐，不从当前曲库重建。Session5、Snapshot3、Factory、UserLibrary格式不改。原始输入、评分和宿主音频线程契约保留；过期代表来源操作被拒绝。
- 必要验证：[8项定向检查](../HarmonyContinuation-source-route-validation-20261019/tests.txt)全部通过（UILayout、MIDI、Preview、Productization与Library snapshot/user/factory）；[真实来源输出及原生界面](../HarmonyContinuation-source-route-validation-20261019/source-ui.txt)、[R01探索入口](../HarmonyContinuation-source-route-validation-20261019/exploration-ui.txt)通过。原生图在同目录source-ui/，升级试听/MIDI/详情及100/125/150%入口检查通过。沙箱临时目录的文件替换限制改用项目临时目录后通过；未改数据代码。只制作一个Setup，未重跑全量CTest、Validator或安装生命周期。
- GitHub审计：unified本地/远端 `013b4c2`，Phase7本地/远端 `c17a795`，两分支仅MAP/NOW文档不同，均含dev.12源码 `e164ccd`；main本地/远端仍为 `bf77019`。统一执行链、旧调用归一、V5继承V4的657条及独立15634125路线完整保留；复用dev.12七例/2522检查等原证据，未重新开发算法。

工程、定向自动化和原生界面检查完成。主代理负责输出语义、实现审查及集中门禁；子代理承担只读基线审计和局部UI实现。尚未安装此包、未完成真实Cubase加载/拖放/MIDI/保存重开验收或人工音乐听评，均为Pending。可提交RC技术评审；RC验收与正式发布仍待决定。关闭Cubase后安装主程序及Factory V5，在About核对 `dev.13 / 0db40a3`，听评完整来源适配、循环回接及探索路线。未合并main、未创建正式Tag/Release；原有未提交协作文档保留。

### dev.12 已交付基线（历史）

2026-10-19：统一推荐引擎、版本化流行变体及已批准的 Phase 7 正式界面已完成本地集成。开发分支 `development/unified-engine-20261019`；真实生产源码 / BuildCommit `e164ccdee38683459e74656d0c1555c3bfa1fb0f`（`e164ccd`）。唯一测试包为 [0.10.0-dev.12 Setup](../HarmonyContinuation-dev12-delivery-20261019/HarmonyContinuation-0.10.0-dev.12-Setup.exe)，Release / Factory Library 5 / Schema 2 / 658 条。暂存插件与 Factory 均和该构建字节一致，moduleinfo 与二进制版本/提交一致。

- 单一引擎：复用原和声检索、HarmonyFirst 结构/节奏资格分离及严格方向扩展兼容；保留 dev.11 精确身份、有限多调性解释、硬约束、用途证据和中性资格。门槛仍为60，不求三模式并集、不填满Top3。调性置信度只解释，不再进入音乐质量；候选和调性并列顺序归一到输入根音，十二调同分性质检查通过。旧实验状态未持久化，旧调用归一到同一 Worker；正式 Controller/UI 无三算法分支。
- 曲库：只追加完整 `15634125`，与既有 `15634145` 保持独立身份。冻结 V4 的657条完整payload/typedData逐条一致；新库独立版本目录、防降级及旧库官方内容识别通过。后续按[有限家族审校与前缀门禁](data/factory-proposals/20261019-pop-variants/family-review.md)收录，不自动组合生成。
- 实际输出：C–G–Am 的 Em–F–G–C、Em–F–C–F–G、Em–F–C–Dm–G 均默认可达。Am–G#dim–G–D/F# 仍为默认 Em 1条，加 G–C–Em、C–G–Em、Em–D–C 三条通过资格的探索路线；后者基础分63.27、排序63.51，单独 More 可访问，不宣称优质 Develop/Loop。F–G–Am Auto 保留色彩后续，固定调性继续约束解释；四和弦及借用iv循环通过。排名和数量不代表听感质量。
- Phase 7：按独立工作线提交 `adb88dd` 合并保留设计历史；正式800×780，详细/简略切换、全组More/探索、独立MIDI手柄及固定试听栏。完整/后续与跟随宿主/自定义BPM按实例工作，后续从完整配音裁切，原始QN及MIDI导出独立；旧无尺寸Session迁移默认保留。没有重写算法/配音或宿主音频契约。
- 必要验证：[七个定向目标](../HarmonyContinuation-unified-validation-20261019/targeted-tests.txt)、[七例真实Worker/Snapshot/Preview/MIDI集成](../HarmonyContinuation-unified-validation-20261019/integration-result.txt)（2522检查，含13个扩展兼容方向门禁）、合并后的[五个直接相关目标](../HarmonyContinuation-unified-validation-20261019/merged-tests.txt)及[真实R01原生探索入口](../HarmonyContinuation-unified-validation-20261019/exploration-ui.txt)均通过。V5新路线快照/MIDI回读通过；Session、Snapshot及个人库格式不变。仅一次最终Release插件构建和一次Setup，未跑全量CTest、Validator、安装生命周期或全库排列。

本地工程和测试包完成；未安装或核验真实Cubase。关闭Cubase后安装主程序与Factory V5，About核对 `dev.12 / e164ccd`。待用户听评新长线变体、D/F#→Em及三条探索路线、借用iv回接；新版UI的真实宿主拖放和试听交互仍需验收。2026-10-19 Fetch确认unified与Phase7两分支已在GitHub完整保存e164ccd，源码相同，仅状态文档不同；未合并main、未建Tag、未正式发布。既有未提交协作文档改动保留。

### dev.11 已交付基线（历史）

2026-10-17：方案 A 核心推荐决策链修复已完成。独立分支 `development/core-decision-chain-20261017`；生产源码提交 `1c671521603949768eae666656a120b9f56f5f18`，已推送 origin。唯一测试包为 [0.10.0-dev.11 Setup](../HarmonyContinuation-dev11-delivery-20261017/HarmonyContinuation-0.10.0-dev.11-Setup.exe)，BuildCommit `1c67152`；一次 Release 插件构建和一次打包通过，暂存插件与构建字节一致，moduleinfo 与二进制版本/提交一致，Factory V4 / Schema 2 / 657 条。

- 决策链：有限 template–key 解释保留至实化及硬约束；精确音乐聚合保留低音、性质、扩展、精确 QN、OPEN 和按输入锚点归一的实现约束。每条音乐只占一个位置，代表解释来自真实匹配，不拼接 subscores。来源数只作证据，支持分为零。
- 基础资格固定使用 style/intent/prior=0.5、support=0，门槛 60；保留原权重与三模式节奏尺度。偏好、来源质量、完成度和软旋律适配只排序；多样性只选默认位置。ProductServices 与色彩重排使用显式资格。用途来自实际路径；缺乏充分证据的路线标为探索性，只进入有界 More。
- 关键对照：C–G–Am 的 Em–F–G–C 在三模式均为默认 Develop 首项；单 F Loop 仍合格，现位于 More。C–G–Am–F 保留完整下一轮；C–Fm–G 的 Dm 在 Auto/固定 C major 仍为默认 Loop。F–G–Am 的原五条 F major Color 均保留，Forced Key 只检索指定调性。Am–G#dim–G–D/F# 的两条近似三和弦路线改为探索性；单 Em 唯一，依据实际 D→Em 作 E minor 模态收束解释，未继承 Loop/Develop。计数与排名变化不构成听感质量结论。
- 兼容：Session 5 / Snapshot 3 / Factory / UserLibrary / 宿主及音频线程格式与职责保持；Session pin 字符串资源上限由 8 扩至 16 KiB，以容纳完整实现引用。旧引用完整恢复音乐后匹配，缺失实现约束的旧 pin 保持不可用，避免错误绑定；旧 Snapshot 保留原音乐，重算后才获得新推荐。Why/Snapshot 按真实 key 与匹配窗口取代表解释。
- 必要证据：[8 个定向测试目标](../HarmonyContinuation-core-chain-validation-20261017/checks-final.txt)全部通过；[13 例 worker 集成](../HarmonyContinuation-core-chain-validation-20261017/integration.txt)通过、重复音乐为零，包括实际 Preview、Snapshot、两种配音×三个 MIDI 导出范围的读回，以及有顶声部/事件切分的时间锚点检查。性质检查覆盖复制/重排来源、细小时值/OPEN/低音/扩展、偏好隔离及首选 key 硬约束失败后次解释保留。dev.10 [有限对照](../HarmonyContinuation-core-chain-validation-20261017/before/consistency.txt)与 dev.11 [实际结果](../HarmonyContinuation-core-chain-validation-20261017/after/consistency.txt)保留。算法职责见 [Continuation Engine](docs/CONTINUATION_ENGINE.md)。未重跑完整 CTest、Validator、安装生命周期或全库穷举；安装契约未改。

工程与单包交付已完成；尚未安装或进行真实 Cubase 加载、保存重开和音乐听评。关闭 Cubase 后安装上述 Setup，About 核对 `0.10.0-dev.11 / 1c67152`，使用顶部 Legacy、HarmonyFirst、Abstract 按钮切换；工程重开恢复 Legacy。重点听评经典完整续写、普通循环及借用 iv 回接、D/F#→Em 收束与探索路线。主代理负责算法判断、实现和集中最终复核；Luna 只读取证并提供性质测试补丁。Phase 7 工作树未修改；未合并 main、未建 Tag、未正式发布。原有未提交协作文档差异保留。

## 已完成阶段

Phase 3 系统审计、Phase 3A 受控减和弦插入实验、Phase 4A 决策分析及 Phase 4B 节奏/和声检索审计均已形成报告。Phase 3A 补偿保持默认关闭；相关历史范围和证据边界见各阶段报告。

## 功能基线

- 当前正式版本：`0.9.0`；发布提交与 `v0.9.0` Tag 均为 `ddc4f89babd4520daac3eae74824d6f581ce0e75`。源码清理提交不改变该正式包的构建身份。
- 正式版为 Factory Library 3；本轮独立开发版本为 Library 5（继承冻结V4并追加一条路线）。Factory Schema 2 / User Schema 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2 保持；生产数据与宿主适配未改。
- Factory V3 为 155 条 canonical 历史记录加 474 条新进行，共 629 条；六条 QUESTIONABLE 留在非生产清单。统计见 [LIBRARY_V3_SUMMARY](LIBRARY_V3_SUMMARY.md)，兼容与降级边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。

## 历史接续记录

2026-10-13：Phase 5 / dev.6 代码构建提交 `3a582e4`。新增默认关闭的 Abstract 方向性扩展和弦匹配，改善真实 Factory V4_B11 的简单输入召回；修复 UserLibrary 并发首次初始化，保持 Factory 确定性编译。默认 OFF、HF-ON、Abstract-ON 三模式各 CTest 33/33、Validator 47/47、V4 专项13,048、隔离候选专项41,105通过；原1440矩阵哈希不变。最终Phase5专项8,361检查，OFF同步首写20/20、隔离安装21/21通过。完整对照、性能、历史核销及边界见 [Phase 5 报告](docs/PHASE5_COMPREHENSIVE_20261013.md)。

2026-10-14：Phase 5.1 离线诊断 713/713 通过（R02 200、音乐/映射 513），UserLibrary 定向回归 283 checks 通过。增补仅涉及测试与文档，production plugin 未改；BuildCommit/BuildID 仍为 `3a582e4` / `0.10.0-dev.6`。范围、证据与限制见 [Phase 5.1 收尾报告](docs/PHASE51_CLOSEOUT_20261014.md) 和 [Cubase 验收清单](docs/PHASE51_CUBASE_ACCEPTANCE.md)。

2026-10-14：Phase 6 `0.10.0-dev.7` 构建身份为 BuildCommit `8c27c27a0c50da7fb0cf991f60ecde66890116f0`。交付形态为一个 Setup、一个 VST3 插件，提供 Legacy、HarmonyFirst、Abstract 三种运行时模式；关闭并重新打开工程后恢复 Legacy。Factory V4 内容未改，Phase 3A 保持 OFF，不提供 Phase6 Candidate 预设、不安装 V5；隔离 V5 的评估 MIDI 和证据另附。工程报告与验收入口见 [Phase 6 综合报告](docs/PHASE6_ENGINEERING_20261014.md) 和 [Cubase 验收清单](docs/PHASE6_CUBASE_ACCEPTANCE.md)。

dev.4 源码交付快照与接手源码 435 个项目文件内容一致；详细核销与来源证据保存在同级 `HarmonyContinuation-validation-20261012/HISTORY_PROVENANCE.md`。dev.4 包保留在 `build-phase4d-delivery/`。新人工清单见 [dev.5 验收](docs/DEV5_CUBASE_ACCEPTANCE.md)。Factory V4 SHA-256 仍为 `74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534`；独立候选及其音乐设计见 [候选说明](data/factory-proposals/20261012/README.md)。

## Phase 6：历史验收与交付对照

Phase 6 交付时分支名为 `development/phase5-20261013`；Phase 6 插件 BuildCommit 为 `8c27c27a0c50da7fb0cf991f60ecde66890116f0`。以 Phase 6 报告记录的构建、测试和最终交付证据核对当前 HEAD 与包身份。路径映射见 [MAP](MAP.md)。

Phase 6 的真实 Cubase 验收尚未完成；R02 Cubase 根因仍 UNKNOWN。请按 [Phase 6 Cubase 验收清单](docs/PHASE6_CUBASE_ACCEPTANCE.md) 集中核对版本/Build ID、Factory、三种模式、R01/R02、拍号、Preview/MIDI、保存重开、User Library 和诊断导出。模拟宿主不代替真实 Cubase。Factory V4 SHA-256 为 `74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534`；V5 不在插件内启用，隔离 MIDI 只作听评。运行时模式重开后为 Legacy，Phase 3A 保持 OFF。最终完整 CTest 34/34、Validator 47/47、隔离安装生命周期 21 checks 通过；Setup SHA 与全部交付证明见综合报告及校验清单。正式发布未授权、未执行。

Phase 5 / 5.1 的人工清单、历史结论、交付与设备接续信息保留在 [Phase 5 报告](docs/PHASE5_COMPREHENSIVE_20261013.md)、[Phase 5 验收](docs/PHASE5_CUBASE_ACCEPTANCE.md)、[Phase 5.1 收尾报告](docs/PHASE51_CLOSEOUT_20261014.md) 和 [Phase 5.1 验收](docs/PHASE51_CUBASE_ACCEPTANCE.md)。Phase 6 换设备与安装步骤见 [RUNBOOK](RUNBOOK.md#phase6-dev7-另一设备接手)。

## 发布与验证证据

正式包：`build-installer/release-0.9.0/HarmonyContinuation-0.9.0-Setup.exe`。构建版本、提交、VST3/Factory/Setup 哈希、依赖与验证范围保存在同目录 `BUILD_INFO.json`、`SHA256SUMS.txt` 与 `FILE_MANIFEST.sha256`。Setup SHA-256：`1ee1ba17a3b689ad7b8ea8f7ffe245c2fc08ac71226ae82eed441788628136f7`。

正式发布原有一次 Release 构建与一次打包通过；对应配置、构建、打包及依赖日志保留在正式目录 `evidence/`。此次清理没有重做发布验证。

复用的历史结果（本轮未重跑）：

- RC3：28 项隔离安装生命周期通过，覆盖组件增删/修复、旧双安装迁移、占用拒绝与卸载，保留个人哨兵、历史库及未管理文件。原始结果为 `evidence/rc3-unified-lifecycle.log`；可复用入口保留在 `tools/test-unified-installer.ps1`。
- RC4：两种模式已知/Uncertain 色条、文字、中英资源、独立提示开关的定向离屏检查通过，原始结果为 `evidence/rc4-color-visibility-smoke-final.log`。
- Library V3：四组兼容桥 406 检查通过，原始结果为 `evidence/LastTest.log`。内容收口校验记录为 629 条、六个 legacy 合并 6/6、Exact Musical Duplicate 0、Missing Metadata 0；旧过程记录可在 Git 历史查阅。
- dev.5–dev.8：色彩计算、展示排序/恢复、候选身份和双语 Why 的定向冒烟曾通过；一次性工具及旧日志已清理，历史范围可从清理前 Git 版本追溯，不宣称当前重新验证。

## 待验收与边界

Phase 6 dev.7 尚无真实 Cubase 验收或人工盲听结果；请以 [Phase 6 Cubase 清单](docs/PHASE6_CUBASE_ACCEPTANCE.md) 记录本机版本、操作和结果。Cubase 核心功能与 RC4 色彩显示的既有用户报告及宿主证据见 [HOST_SPIKE](docs/HOST_SPIKE.md)，不代表 dev.7 已验收。FL Studio Not Verified / Pending；既有 RC 人工清单见 [RC_DAW_ACCEPTANCE](docs/RC_DAW_ACCEPTANCE.md)。

User Schema 2 需要本兼容桥或后续支持版本；v0.8.0 和旧 v0.9.0-dev.1 无法读写它。安装前按 [RISKS](RISKS.md) 定位降级风险并保留用户数据备份。已安装插件、生产 Factory 历史、真实 user.db 和同级旧 checkout 归档不属于本次仓库工作区清理。

### 入库六类根因修复（2026-11-17，v6）

用户抽查点名的问题逐条定位到六个根因并全部修掉：级数标注双重转调（87.8% 条目名字错，但 `sequence` 是对的）、
调性推断推翻 SALAMI header（285 首不符）、2 小节 cell 循环被切成伪乐句、弱起当正文、单个坏 token 丢整段、
谐波节奏归一化不完整。重建后 `phrases=4189`、库 5,455 条 / 28.2 MB、条目中位 4 小节、
末和弦落主音 23.9%（手写库 66%，v5 按同口径 9.6% 但 v5 标签是错的、不可比），`ctest` 21/21。另把 `TemplateJson.cpp` 的解析期模板上限从 4,096 提到 16,384
（它只是候选文件护栏，不是库容量）。引擎代码仍是一行未改。

### v7 按五条人工判据重切（2026-11-17）

用户看过 15 段抽查后给的五条判据全部落进入库层：vamp 改成按发声时长算调内主+属占比（>0.66 不入库）、
相邻同和弦小节合并成持音（`G@4|G@4|G@4|G@3`→`G@15`）、ABAC 型优先两小节一切（受"2 小节块 86.6%
不到 4 和弦"的硬约束，只在 13.4% 上生效）、所有和弦一律写 `@时值`、修掉 `collapse_run_repeats`
丢时值的 bug。重建后 `phrases=2773`、库 4,045 条 / 19.3 MB、`ctest` 21/21。引擎仍是一行未改。

### v8 去掉小节下限（2026-11-17）

用户指出"小节数不是限制乐句的因素"，这是入库层最后一次结构性修正：删除 `MIN_UNIT_BARS = 4`，
把切分下限从**小节数**改成**和弦变化数**（语料无 BPM，2 小节在 60 与 170 bpm 差三倍时长，不可比），
周期折叠改为存最小完整周期，并删掉 `length_cost` 里那个主动奖励长单元的负项。
重建后首次出现 1 小节条目（16 条）、最长 8 小节、中位 5 和弦 / 16 QN，库 4,126 条 / 18.7 MB，
`ctest` 21/21。ABAC 被去重吃掉按用户要求保持不动（"库里仅保留最小乐句单位"）。
