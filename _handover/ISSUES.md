# 已知问题台账（用户实感，2026-11-17 建立）

这里只记**用户在使用中真的撞到的问题**，以及它们背后的代码事实。**状态全部是「未修」**——
本轮只做记录与定位，不动代码。修之前先读 [STATE](STATE.md) 与 [DECISIONS](../DECISIONS.md)，
改完把条目移出「未修」并写清修法与验证。

编号规则：`I01`…，按发现顺序，不按优先级；编号一旦分配不复用。

来源：用户 2026-11-17 一次给出四条（原话保留在每条下面）。

---

## I01 收束栏给的是"短终止式"，发展栏给的是"长复读"，长度与意图错位

**用户原话**："收束推荐了很多太长的推荐，而发展却没什么有长远发展的推荐，这与意图冲突"

**实测（V9 库 / 4,365 条，`recommend_cli`，2026-11-17）**

| 查询 | 栏 | 各候选续写的和弦数 |
| --- | --- | --- |
| `I–V`（C, G） | RESOLVE | 5 / 5 / 4 |
| `I–V` | DEVELOP | 5 / 14 / 13 |
| `I–V` | COLOR | 13 / 4 / 3 |
| `I–vi–IV–V` | RESOLVE | 3 / 3 / 1 |
| `I–vi–IV–V` | DEVELOP | **17 / 20 / 13** |

`I–vi–IV–V` 的 DEVELOP #1 是 `BB_0263_04`：`C–Am–F–G` 原样重复 **4 遍**（17 个和弦）；
#2 是 `BB_0355_03`（20 个和弦）。**长度全部来自重复，不来自任何发展**——这正是用户说的
"没什么有长远发展的推荐"。

**机制（读码确认）**

1. 候选长度 = 匹配点 → **模板末尾**，从不截断。创意路径还显式禁止截断：
   `src/core/ContinuationEngine.cpp:414` 把"在来源结束前就结束"的匹配直接丢弃
   （`match.disposition="wrapped_before_source_end"; continue;`）。
2. 分组标签由**末端两和弦**重新贴：`src/core/ContinuationEngine.cpp:464` 按
   `{Resolve, Loop, Color, Develop}` 顺序取第一个"能达标"的意图，
   `IntentCompletionEvaluator` 对 `(IV|V|ii)→I` 直接给 0.96/0.84。**长度不参与任何分组门槛**，
   所以 20 个和弦的复读和 1 个和弦的终止式可以同栏。
3. 反过来 RESOLVE 栏长期被 V3 教科书模板统治：一次查询里 `support=76` 的那条（1 个和弦的 `C`）
   score 68.2，与 3 和弦的变格终止 72.9 一起挤满前三——因为 `support` 与 `match` 权重最高
   （`src/core/ContinuationEngine.h:130-131`：match 0.34 / continuation 0.12 / cadence 0.07）。

**影响**：用户要的是"快速找灵感"，现在一栏是短而无悬念的终止式，一栏是 4 小节循环复读 4 遍。

**方向（未实施）**
- 把长度/句法终点变成候选的**显示属性**并参与排序，而不是让"末端终止式"独占分组。
- DEVELOP 栏加"差异"判据：与输入或其自身重复的片段降权（`units`/`repeats` 已有雏形）。
- RESOLVE 栏排除"与输入已有的收束重复"的条目。

---

## I02 打开 UI 卡约十秒（库加载卡在 UI 线程）—— **已修，2026-11-17**

**用户原话**："每次打开UI的时间都太长，考虑把库加载成本挪到其它时间，不要卡使用的手"

**实测**

| 库 | 条目 | 文件大小 | 单次 `loadFactory` |
| --- | --- | --- | --- |
| V7 | 1,272 | 4.7 MB | 368 ms |
| **V9（现装）** | **4,365** | **35.3 MB** | **5,600–6,800 ms** |

**机制**

1. `Controller::attach()`（`src/plugin/Controller.cpp:250`）在**UI 线程同步**调 `reloadLibraries()`。
2. `reloadLibraries()` 先 `loadAvailableFactory()`（`:362`）全量解析一次；
   接着 `annotationsForFactory()`（`:368`）在 `src/wish/WishCatalog.cpp:32` **又调一次
   `library::loadFactory()`**——同一个库第二遍全量解析。两遍 ≈ 11–13 s。
3. 后台 worker 第三遍（`src/plugin/RecommendationWorker.cpp:227`，外加对 35 MB 算 SHA-256），
   这次不阻塞界面，但开 UI 后仍占满一个核约 6 s。
4. 单遍为什么贵：`readAll()` 逐行 `parseTemplateJson`，随后 `loadFactory()` 对**每一条**再调
   `checkTemplate()`（`src/library/ProgressionLibrary.cpp:215`），后者把刚解析出来的模板
   `serializeTemplateJson` 再 `parseTemplateJson` 一遍——纯校验性往返，每条付 2 次解析 + 1 次序列化。
5. 附带：`MainView::setLibrary()`（`src/ui/MainView.cpp:523`）为全部 4,365 条各算一次
   `wish::sourceFingerprint()`（`src/wish/MusicIdentity.cpp:28` 会为每条重建一次完整演奏序列）来拼 identity。

**归因**：V9 库（4,365 条、35 MB）是 2026-11-16 入库的，字节数 7.5×、单条时长 5.3×，
而加载路径两遍、无缓存——这是本次入库引入的回归。

**方向（未实施）**
1. `annotationsForFactory` 增加接受已加载 `LoadResult` 的重载，`reloadLibraries` 直接传入（−6 s）。
2. Controller 缓存 `FactorySelection` + 标注，按 `(path, size, last_write_time)` 失效（第二次开 UI ≈ 0）。
3. `reloadLibraries()` 移出 UI 线程，attach 先显示"载入中"。
4. 砍掉工厂库的 `checkTemplate` 往返（数据刚由同一个序列化器写出并已解析）。
5. `setLibrary` 的 identity 改用库元数据（version + 条目数 + mtime）。

### 已做的改动（2026-11-17，方向 1/2/4/5）

| 改动 | 位置 |
| --- | --- |
| `annotationsForFactory` 增加接受已加载 `LoadResult` 的重载，`reloadLibraries` 不再二次解析 | `src/wish/WishCatalog.{h,cpp}` 新三参重载 |
| Controller 缓存 `FactorySelection` + 标注，按 `active.txt` 字节 + `(file_size, last_write_time)` 失效 | `src/plugin/Controller.{h,cpp}` `reloadLibraries()`、`struct FactoryCache` |
| 库读取路径不再对每条已解析模板做 `serializeTemplateJson`→`parseTemplateJson` 往返（写入路径保留） | `src/library/ProgressionLibrary.cpp` `checkTemplate(..., bool roundtrip)`，`loadFactory` 传 `false` |
| `decodeTemplateData` 不再 `dump` 出 `events` 子树再重新 `parse` | `src/persistence/ChordData.cpp` 新增 `decodeChordsValue(const Value&)` |
| `setLibrary` 在调用方证明库字节未变时跳过 4,365 次 `sourceFingerprint` 拼串 | `src/ui/MainView.{h,cpp}` 新增 `factoryUnchanged` 形参 |

`tools/attach_bench.cpp`（新，构建目标 `attach_bench`）把这段成本拆开量：

```
$ ./build/Release/attach_bench.exe build/factory.db 3
library=build/factory.db entries=4365 version=9
cold load             965.2 ms   (sql select 46.6 + json parse 867.2 [no typedData 59.1] + roundtrip 2345.2)
cold annotations       70.6 ms
cold identity         305.0 ms
warm copy              15.9 ms
warm identity         338.6 ms
```

（`roundtrip` 一栏是 bench 自己模拟的旧行为，库里已经不再执行。）

**前后对比（V9 库，4,365 条 / 35.3 MB）**

| 场景 | 修前 | 修后 |
| --- | --- | --- |
| 单次 `loadFactory` | 5,600–6,800 ms | **965 ms** |
| 第一次打开编辑器 | ≈ 11,700 ms（两遍全解析 + identity） | **≈ 1,340 ms** |
| 之后每次打开编辑器 | ≈ 11,700 ms（每次都重算） | **≈ 60 ms**（15 ms 拷贝 + user.db + 界面重排） |

`ctest` 21/21 通过（22.89 s，修前同套 34.42 s）。

**仍未做**：`867 ms` 里 808 ms 是 `benchmark::json::parse` 解 `typedData`（去掉该字段只需 59 ms），
要再降需要换解析器或把 typedData 换成二进制；`RecommendationWorker`
（`src/plugin/RecommendationWorker.cpp:227`）仍会自己加载一遍库并对 35 MB 算 SHA-256（后台，不卡 UI）。

---

## I03 推荐名是一串级数，看不出风格与特征 —— **已修，2026-11-18（v10）**

**修法**：`tools/ingest/naming.py` 三层命名（句式名 / 某进行变式 / 起句·走向·收尾），2,854 条新条目
纯级数名 0 条；来源歌名移入 `note`。旧库条目与手写条目维持原名（它们本来就有名字）。


**用户原话**："推荐进行的名字是一串级数，根本无从得知其风格和特征，命名考虑修改"

**证据**

- UI 上候选的标题是把和弦逐个拼起来的路径，从不显示模板的 `nameZh`：
  `src/ui/MainView.cpp:890`（`for(const auto& event:candidate->progression){...path+=event.name;}`）、
  `src/ui/MainView.cpp:1464`（同一模式，用于导出/展示）。所以用户看到的就是 `C → Am → F → G`。
- 真实语料条目的 `nameZh` 本身也是级数串：
  `BB_0003_00` → `vi vi I I vi vi I I vi vi I I`（手写条目则有文学化名字，如 `City Pop · 紫色回环`）。
- **附带缺陷（2026-11-16 入库引入，2026-11-18 已修，v9）**：`nameEn` 的段名曾被拼坏——
  `BB_0003_01` → `I Don't mind - James Brown [aintrointro 1]`。
  原因：`tools/ingest/billboard.py:156` 把段名存成 `('aintro','intro')` 元组，
  `tools/ingest/build.py` 再对 label 做 `re.sub(r"[^a-z]", "", str(label or "section").lower())`，
  `str(tuple)` 后只剩字母 → `aintrointro`。
  修法：`billboard.py` 改存纯段名（`marker.group(2).strip()`），重跑入库；
  v9 库 4,126 条，段名形如 `[verse 3 u1]`，与 v8 内容零差异。

**方向（未实施）**
1. 显示层改三段式：`调性 · 句法角色 · 显著特征`（例：`C 大调 · 主歌起句 · 借用 bVII`），
   级数串降为副标题（用户仍然需要它来读和声）。
2. 语料侧已有 title/artist/段功能（verse/chorus/intro…），可生成可读名而不是级数串。
3. 修 `aintrointro`，并让"来源段"与"续写内容"在命名上分开。

---

## I04 进行太长，违背"步进"意图 —— **已改入库层，2026-11-17**

### 已做的改动（2026-11-17，入库层切分）

代码一行未动引擎；改的是入库：`tools/ingest/boundaries.py`（新）+ `build.py --split-units`。
每个入库条目现在是一个乐句单元：只在小节线上切，四条独立证据（重复重启/终止式/句首重现/休止）
+ 4 小节超节拍奖励，长度硬下限 4 小节、软上限 8 小节。

| 指标 | 改造前（v3） | 改造后（v4） |
| --- | --- | --- |
| 入库条目 | 3,107 段语料 | 3,706 个单元（615 段静态 vamp 剔除、9,017 个重复单元裁掉） |
| 条目中位长度 | 9 小节 / 12 和弦 | 5 小节 / 6 和弦 |
| 库总条目 | 4,395 | 4,994 |
| 库体积 | 35.4 MB | 25.4 MB |
| `C–Am–F–G` 的 DEVELOP 前三 | 17 / 20 / 13 和弦 | 3 / 4 / 4 和弦 |

`ctest` 21/21；`recommend_cli` 的 `totalMatchMs` 213 ms（改造前 299 ms）。

**仍未做**

1. 手写模板（`V3_*`/`V7_*`）里仍有 14 和弦的长条目，它们同样会长进推荐；要不要一起处理是下一个决定。
2. 收束栏前三仍是 `support=76` 的短模板（单个 `C`）——这是 I01 的 support 计分，不是长度问题。
3. `Dm7–Am–C–Dm7–F–A7` 仍 0 结果（调性判到 D 小调后无候选过门槛），既有缺陷。

---

## I05 入库六类系统性失真 —— **已修，2026-11-17（v6）**

用户点名抽查发现的问题（`BB_0081_10` 的重复单元、`BB_0183_02`/`BB_0276_00`/`BB_0589_01` 级数标错、
`BB_0705_01` 的首个 `IV`、以及"真的每个和弦都占一小节吗"），逐条定位到六个根因：

| 类 | 根因 | 影响面（实测） | 状态 |
|---|---|---|---|
| ① 级数双重转调 | `bars_tokens` 在已重拼到 C/A 的文本上又减一次原曲 key | 7,517/8,566 段（87.8%）名字错，`sequence` 本身没错 | 已修 |
| ② 调性推翻 header | 纯计数打分，header 仅 0.5 平票 | 285/855 与 header 不符，202 首差五度 | 已修 |
| ③ 重复 cell 未折叠 | 静态 vamp 判据只认"全段一种"，2 小节 cell 循环逃过 | 865/6,849 段（12.6%）被切成伪单元 | 已修 |
| ④ 弱起当正文 | 不看"首小节是否在段内再出现" | 1,621 段有弱起候选，20.5% 段首有 ≥2 和弦 | 已修（部分） |
| ⑤ 一个坏 token 丢整段 | `chord_tokens` 失败即 `return []` | 整段丢弃 601 段；补 `/11` `/13` 可救 287 段 | 已修 |
| ⑥ 谐波节奏 | 保留正确（20.8% 小节有 ≥2 和弦），归一化只做对一半 | 由 ③ 一并处理 | 已修 |

修后重建：`phrases=4189`，库 `factory-bb-v6.db` 5,455 条 / 28.2 MB，`ctest` 21/21。

**仍未做**

1. `BB_0705_00` 段首那个 `IV`：SALAMI 原标注首小节就是 `F/C C Am7`，`pickup_length` 判 0。
   需要别的"弱起"证据（比如首小节和弦时值明显短于后续）。
2. 切点网格率从 89% 降到 74%（口径变了：现在按弱起偏移计数，且折叠段不产生切点），
   需要人工抽查确认不是退步。
3. `Dm7–Am–C–Dm7–F–A7` 仍 0 结果（I01/调性推断的既有问题）。
4. 交替 vamp（两种小节型交替）折叠后是否等于用户心里的乐句，未验证。

## I06 v7 五条人工判据落地 —— **已改，2026-11-17**

用户看过 15 段抽查后给出五条判据（m06630），逐条对应改动：

| 用户原话 | 根因 | 改法 | 结果 |
|---|---|---|---|
| 7「C 和 G 的按需变换……不写入库」 | 静态 vamp 只认"全段一种和弦/小节型"，交替 vamp 逃过 | 新增 `tonic_dominant_share()`，按**发声时长**算调内主+属，单元级 >0.66 不入库 | 剔掉 4,103 个单元；Reelin' & Rockin' 整曲 0 条 |
| 8「相邻和弦合并为一个更长的和弦」 | 逐小节重复的同和弦各算一个和弦 | 新增 `merge_held_bars()`，`G@4|G@4|G@4|G@3`→`G@15`，放在切分之后、数和弦之前 | 合并 5,719 处；`BB_0656_01` 结尾 `G@12` 并进 u2 |
| 9「典型 ABAC 型，两小节一切」 | 无 ABAC 判据，只有整段周期折叠 | 新增 `chain_cells()`，先试 size=2，块不够和弦再退 size=4 | 命中 2,269 段 |
| 「C 和 G 变形占比 >66% 不入库」 | 同上 vamp 条 | 同 `VAMP_SHARE` | 库 5,455 → 4,045 |
| 「入库就标注好时值」 | `render_bars` 对单和弦小节省略 `@` | 改为一律写 `@`；`symbols()` 放行跨小节持音 | 17,625 token 全带时值，24.2% 条目含跨小节持音 |

**硬约束**：2 小节切分只在 13.4% 的段落上可行。全语料 15,481 个 2 小节块里 13,401 个（86.6%）
不到 4 个和弦（`2:11002, 3:2399, 4:1684, 5:220, 6:133, 7:22, 8:21`），按 2 小节切会立刻被
`MIN_CHORDS = 4` 滤掉。所以只有够和弦的那批用 size=2，其余退回 4 小节块配对。

**仍未做**

1. `BB_0705_00` 段首那个 `IV`。SALAMI 首小节就是 `F/C C Am7`，`pickup_length` 判 0。
2. 多单元段的关系在去重里丢失：3,226 个多单元段中 2,659 个（82.4%）最后只落 0–1 条，
   因为 `seen` 是跨段全局共享的。`BB_0698_02` 的 u2/u4 与 `BB_0698_01` 逐字相同被吃掉，
   用户想看的 ABAC 关系因此在库里看不到。
3. `Dm7–Am–C–Dm7–F–A7` 仍 0 结果。

## I07 小节数被当成乐句长度 —— **已改，2026-11-17**

用户原话："小节数不是限制乐句的因素，我一个 60bpm 的曲子还有可能一小节一个乐句呢。"
这是 I04 引入 `MIN_UNIT_BARS = 4` 时的根本错误：把一个**时长**度量当成了**结构**度量。

| 位置 | 原来 | 现在 |
|---|---|---|
| `boundaries.MIN_UNIT_BARS` | 常量 4，折叠单元至少 4 小节 | 删除 |
| `segment(low=...)` | 小节下限，短于 4 小节的单元被禁止 | **和弦变化数**下限（`content` 参数） |
| 周期折叠 `cycles` | `ceil(4/period)`，2 小节 vamp 存 2 轮 | `1`，只有 1 轮不足 `SPLIT_LOW` 个和弦才存 2 轮 |
| `length_cost` | 有 `-0.35*weight*(...)` 主动奖励长单元 | 删除，只惩罚超界 |
| `chain_cells` 的 2 小节门槛 | `MIN_CHORDS` | `SPLIT_LOW`（和弦数） |

**语料为什么不能有小节下限**：890 首 SALAMI 标注只有拍号、没有 BPM；2 小节在 60 bpm 是 8 秒、
在 170 bpm 是 2.8 秒，跨曲不可比。和弦变化数是库里真正被匹配的东西，也是唯一不依赖 tempo 的度量。

**结果**：首次出现 1 小节条目（16 条），最长 8 小节，中位 5 个和弦 / 16 QN。

**代价（必须记着）**：条目变短 → 长查询的续写空间变小。库中留有 ≥2 和弦续写的条目比例：
查 2 和弦 100%、查 3 和弦 64%、查 4 和弦 44%、查 5 和弦 27%。如果用户反馈"长片段查出来没东西"，
这是原因，不是 bug。
