# 接手状态快照（2026-11-15 建立）

本目录是**无历史快照**：为了省流量与体积，没有保留 `.git`，只保留最新工作树 + 最新实验增量。
远端：`https://github.com/OldLai-ctrl/HarmonyContinuation.git`

## 这份快照是什么

| 项 | 值 |
| --- | --- |
| 工作树内容 | `main` @ `d3552c24cc6d080d8cc749adeb1e2b45fa81458a`（= 正式发布 v0.11.0 之后的文档交接提交） |
| 正式发布身份 | Tag `v0.11.0` / 发布源码 `d888e2c8d5f78a19a20a7936e763d407100b6a32` |
| 产品版本 | 0.11.0，Windows x64 Release，Factory V8 / Schema 2 / **1,288 条**，内嵌仅 Billboard CC0 先验 |
| 最新实验分支 | `experiment/factory-v9-b-20261115` @ `216c0ed86064e70d04ada5f5e6a244e74ea12240` |

`_handover/v9b-20261115-vs-main.patch` 是 v9-b 相对 main 的完整增量（41 文件，+5,143/−162）。
`_handover/latest/` 是其中 6 篇新报告与 V9-A/V9-B 两张素材表（只读副本，方便直接看）。

其余各分支 tip（供核对，历史未下载）：

| 分支 | tip |
| --- | --- |
| `experiment/b5-corpus-statistics-20261113` | `bbd1f4da` |
| `experiment/b6-retrieval-statistics-20261113` | `f95f2af3` |
| `experiment/b7-factory-coverage-20261114` | `3b58159f` |
| `experiment/factory-v9-a-20261114` | `b451ce11` |
| `experiment/factory-v9-b-20261115` | `216c0ed8` |
| `development/g5-integration-20261025` | `24bdc239` |

**注意**：本树里的 `NOW.md` / `DECISIONS.md` / `RISKS.md` / `MAP.md` / `RUNBOOK.md` 都还是 main 的版本，
不包含 B5–V9 的记录；v9-b 的更新版在这 6 个文件里，只在 patch 中。看最新叙述请读 patch 或 `_handover/latest/`。

v9-b 相对 main 的 41 个文件（`git diff --name-status` 原样）：

```
M AGENTS.md DECISIONS.md MAP.md NOW.md PROJECT_INDEX.md RISKS.md RUNBOOK.md
M docs/G13C_PRODUCT_INTEGRATION.md history/README.md
M src/core/ContextualContinuation.cpp src/core/ContinuationEngine.cpp src/core/ContinuationEngine.h
M src/core/ProgressionMatcher.cpp src/core/ProgressionMatcher.h tools/factory_v6_compiler.cpp
D history/2026-11/2026-11-10-v011-formal-release.md
A data/factory-v9-a/phrases.tsv data/factory-v9-b/phrases.tsv
A docs/B5_CORPORA.md docs/B5_CORPUS_STATISTICS.md docs/B6_RETRIEVAL_STATISTICS.md
A docs/B7_FACTORY_COVERAGE.md docs/FACTORY_V9_A.md docs/FACTORY_V9_B.md
A tools/b5/{b5_beat,b5_benchmark,b5_data,b5_factory,b5_model}.py tools/b5/factory/{CMakeLists.txt,factory_replay.cpp}
A tools/b6/{b6_factory,b6_reliability}.py tools/b7/{b7_content,b7_timing}.py
A tools/v9a/{encode,evaluate,freeze_rock}.py tools/v9b/{diagnose_v9a,evaluate,freeze_confirmation}.py
```

产品源码的真实改动只有 B6 那 5 个文件（`ProgressionMatcher` 重写 266 行 + 引擎里的默认关闭开关）；
V9-A/V9-B 只加数据与离线脚本，不动引擎。

## 可用的更新路径

本机 `git`/`curl` 的 TLS 坏掉（`schannel: AcquireCredentialsHandle failed: SEC_E_NO_CREDENTIALS`），
`gh` 未登录，未认证 GitHub API 只有 60 次/小时。可用通道只有：

```
set HTTPS_PROXY=http://127.0.0.1:7890
python <脚本>   # ssl 校验需关闭（代理做了 MITM）
```

`D:\VibeCoding\_tools\gh_smart_clone.py` + `gh_smart_finish.py` 用 Python 手写了 git smart-HTTP v0，
能拉全库或单分支。**若只需最新一版，用 `--depth 1` 语义（只 wants 目标 commit 的 tips）即可，不必拉全史。**

## 构建

前置：CMake ≥ 3.25、C++20、MSVC x64、VST3 SDK 3.8.x。SDK 在 `D:\VibeCoding\VST_SDK\vst3sdk`
（`CMakeLists.txt:80` 会自动回退到 `../VST_SDK/vst3sdk`，故不需要设 `VST3_SDK_ROOT`）。
本机 PATH 无 cmake/cl，须用 VS 自带：

```
CM="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
"$CM" -S . -B build -G "Visual Studio 17 2022" -A x64 -DHC_PRERELEASE= -DSMTG_RUN_VST_VALIDATOR=OFF -DSMTG_CREATE_PLUGIN_LINK=OFF
"$CM" --build build --config Release --target HarmonyContinuation library_manager
```

**已修的阻塞（2026-11-15，改动只在 CMakeLists.txt 的生成规则）**：原 `CMakeLists.txt:178-179` 把 82,591 字节的
`data/generation-priors/billboard-cc0.tsv` 写进**单个**原始字符串字面量，MSVC 19.43.34808 报
`error C2026: 字符串太大，已截断尾部字符`（单字面量上限约 65,535 字节）。现改为按 8,000 字符切成 11 段相邻字面量
（`R"HC_PRIOR(...)HC_PRIOR"` 连续拼接），翻译期合并成完全相同的字节序列。验证：生成头 85,748 字节 / 11 段；
与 TSV 往返比对内容一致；插件可编译，且能被 VST3 validator 加载。

**三个构建环境陷阱（与仓库无关，但会浪费很多时间）**：

1. **不要加 `-j`**。`cmake --build ... -j 8` 会走 MSBuild `/m`，ZERO_CHECK 被要求执行 `ALL_BUILD` 目标，报
   `MSB4057: 该项目中不存在目标"ALL_BUILD"`（84 条），并且多数情况下**静默 exit 1、日志只有
   `Checking File Globs`**。串行构建（不带 `-j`）一切正常。
2. 不带 `--target` 的全量构建会停在 `ContinuationEngineTests` 的 **LNK2019——预先存在的仓库缺陷，不是本机问题**：
   该 target 编译 `src/plugin/RecommendationWorker.cpp`（引用 `appendProductGeneration`），却只链接
   `harmony_library harmony_enrichment`，没有链接定义该符号的 `harmony_generation`（`CMakeLists.txt:751-752`）。
   已用 GitHub raw 的 main 原文核对，上游同一行完全相同，所以仓库的 `ALL_BUILD` 在任何机器上都无法整体链接；
   RUNBOOK 只构建 `HarmonyContinuation` + `library_manager` 正是绕开这一点。
3. **RUNBOOK 里的 Ninja Multi-Config 路线在本机跑不通**：`cmake -G "Ninja Multi-Config"` 在
   `-- Detecting C compiler ABI info` 处**无限挂起**（vcvars64 环境已就绪、ninja.exe 路径已显式指定，
   两次尝试都停在同一行，其中一次在后台挂了 10 分钟以上）。VS 生成器路线不需要 vcvars 环境、能正常出包，
   所以本机基线全部走 VS 生成器；Ninja 路线的可用性需另找机器或先排查 `try_compile` 卡住的原因。
   原始日志见 `_handover/logs/ninja-configure-hang.log`。

**本机实测基线（2026-11-15，修复后，串行 Release）**：

- 40 个工具/测试可执行文件全部产出。`ContinuationEngineTests` 的链接缺陷已在本地修掉（见下节"本轮本地修复"）。
- 插件包 `build/VST3/Release/HarmonyContinuation.vst3/Contents/x86_64-win/HarmonyContinuation.vst3` 4,937,728 字节；
  Resources 内 `factory.db` 4,812,800、`factory.hca` 1,505,577、`en-US.json` 16,135、`zh-CN.json` 15,476、
  `moduleinfo.json` 1,075。
- `validator.exe "build/VST3/Release/HarmonyContinuation.vst3"` → **47 tests passed, 0 failed**；同时说明 DLL 加载期的
  `parseStylePrior(hcGenerationPrior)` 没有抛异常，即先验数据完整。
- `ctest -C Release` → **28/35 通过**（首次基线 26/35，两处本地修复后 28/35）。剩余 7 条失败：
  `ConstraintBenchmark`（自检 `hard filters conflicts`）、
  `HostContractFLStudio` / `HostContractGeneric` / `HostContractCubase`（同一条
  `FAIL 40 current compact browser MIDI identities and UI restore: current compact interaction contract`）、
  `Phase4DCubaseFeedback`、`CompactEditorUI`（`compact UI interaction smoke failed`）、`WishEditorUI`。
  后四条同源：`tests/HostContractHarness.cpp:550` 的 `runCompactSmoke` 在
  `!dc || workerBusy_ || !recommendationResultReady_` 时直接返回 false（`src/ui/MainViewCompact.cpp:1050`）。
- 生成器测试：`PhraseGenerationTests <build/factory.db> <outdir>` → **1954 focused assertions PASS**，5 组短语各 10 条
  （`expanded` 7,664–8,592 条，324–368 ms）。该 target 与 `phrase_generate` 需 `-DHC_BUILD_GENERATION_EXPERIMENT=ON`
  （默认 OFF，不影响产品构建）。
- `GenerationIntegrationTests`（EXCLUDE_FROM_ALL）能编译，但需要 `factory.db listening-directory evidence-directory`
  三个参数指向 G13 听评证据目录，该目录不在仓库内，因此不能当常规回归测试用。
- 已安装 v0.11 的插件 Resources 里**没有** `factory.db`/`factory.hca`：构建期 post-build 才把它们放进 bundle
  （`CMakeLists.txt:583-593`），而 Setup 装完后把库迁到
  `%ProgramData%\HarmonyContinuation\Libraries\Factory\<版本>\` 并清掉 bundle 残留
  （见 `docs/INSTALLER_AND_LIBRARY.md`、`docs/INSTALLER_DEV2_UPGRADE_FIX.md`）。两者不矛盾。

## 本轮本地修复（2026-11-15，共 4 处，全部可用 `git apply -R` 回退）

| 文件 | 改动 | 依据 |
| --- | --- | --- |
| `CMakeLists.txt`（生成规则） | 先验 TSV 切成 11 段相邻字面量 | MSVC C2026，见上节 |
| `CMakeLists.txt`（`ContinuationEngineTests`） | 链接追加 `harmony_generation` | 该 target 编译 `RecommendationWorker.cpp`，需 `appendProductGeneration` |
| `tests/WishTests.cpp` 的 `HC_WISH_FACTORY_DB_PATH` | 构建 V6/V7/V8 时改指 `factory-v5.db`；测试断言的是 V5 契约（658 条） | 注解包也是 V5 |
| `tests/ContinuationEngineTests.cpp` / `tests/ProductizationTests.cpp` | worker 冷启动预算 | 实测冷 worker 建 1,288 条索引 ≈1.6 s，原预算 1 s / 2 s |

冷启动实测（`recommend_cli`，V8 库）：wall 1.74 s，其中 `PERF totalMatchMs=74.489`，**≈1.6 s 在装库建索引**。
故 `ContinuationEngineTests` 改用临时 store 目录（避免依赖 ProgramData 装机库）、
`ProductizationTests` 把等待轮次 200→1200。两者修复后分别 323/323、107 checks 通过。

## 本轮修剪与引擎改动（2026-11-15 第二段，用户授权）

修剪（删的全部是测试/门禁/基线/死码，产品音乐逻辑未删）：

| 范围 | 前 | 后 |
| --- | --- | --- |
| `tests/` | 22,693 行 / 40 个 exe / 35 条 ctest | 6,447 行 / 17 个 exe / **21 条 ctest 全通过** |
| `CMakeLists.txt` | 58,916 字节 | 45,287 字节 |
| `src/ui/MainView.cpp` | 179,631 字节（含 17 个 `run*Smoke`） | 138,359 字节 |
| `src/ui/MainViewCompact.cpp` / `MainViewWish.cpp` | 136,046 / 48,980 | 70,371 / 29,316 |

删掉的 ctest（16 条，正是原先 7 条红测的来源）：`HostContract{FLStudio,Generic,Cubase}`、`CompactEditorUI`、
`WishEditorUI`、`Phase4DCubaseFeedback`、`FactoryV6Focused`、`FactoryV7IntegrationSmoke`、`ConstraintBenchmark`、
`TendencyTests`、`EnrichmentTests`、`BenchmarkTests`、`RecommendationReliabilityFocused`、
`RecommendationRhythmDecoupling`、`CubaseReliabilityFocused`、`GenerationIntegrationTests`、`EditorHostSmoke`、
`VST3PluginTestHostSmoke`。同时删 `src/demo/DemoMain.cpp`（离屏冒烟宿主）、`harmony_benchmark`、
5 个基准 CLI、`data/benchmark/`；`harmonicReranking` 全链与 `PreviewOwnership`。
`src/demo/DemoScenario.cpp` 保留（Productization/MidiExport/Preview 测试在用）。

引擎改动：

1. **P1 续写长度进排序**（`src/core/ContinuationEngine.cpp`，`subscores` 段）：`evidence = n/(n+2)`，
   `s.match = similarity*(0.5+0.5*evidence)`，`s.continuation = n/(n+1)`；**资格分仍用未缩放的 `similarity`**
   （否则 `I–vi–IV–V` 的 COLOR 栏会被 60 分门槛清空）。效果：`I–vi–IV–V` 的 RESOLVE 从单和弦 `C`(80.1)
   变成 `C→F→C`(72.9) / `Cmaj7→Fmaj7→Cmaj7`(69.5) / `C`(68.2)。
2. **P2 路径距离取头尾较短**（`src/session/ProductServices.cpp` 的 `pathDistance`）：两和弦答案若是四和弦答案的
   开头或结尾复述，判为同一想法，被 `minimumDiversityDistance` 去重。
3. **P3 整句试听**（`src/ui/MainView.{h,cpp}`）：栏头「整句 / Full phrase」开关，开启后 `continuationRoute`
   改走 `preview::templateRoute`，试听播放借来的来源整句而不是只有续写尾巴。

`ContinuationEngineTests` 的 J4 断言改为 `longRoute->rankingScore > shortRoute->rankingScore`
（"J4 more continuation evidence outranks the shorter suffix"）。对应决定见 `DECISIONS.md` 的 D23。

## 第 1 步：风格体系从 6 位单选扩到 13 位多选（2026-11-15 第三段）

数据库扩建的第一步（引擎侧），仅此一步，未动 Schema、匹配打分与 V5–V8 那 1,288 条。

| 项 | 前 | 后 |
| --- | --- | --- |
| `Style` 枚举 | 6 位（pop/rock/rnb/jazz/citypop/functional） | **13 位**，前 6 位值冻结，新增 folk/electronic/cinematic/latin/blues/anime/experimental |
| 查询 | `RecommendationRequest::style` 单个 `optional<Style>` | `styleMask`（`StyleFlags`，0=不限，多位置=任一命中） |
| 风格评分 | `styleScore(item, optional<Style>, exactHarmony)` 精确相等 | `styleScore(item, StyleFlags hintMask, bool)`，取命中位上的 **max** 权重 |
| 界面 | 单选 6 项 | 勾选列表 13 项（主工具栏、库筛选、愿望面板；愿望面板仍单选） |
| CLI | `--style X` 单次 | `--style X` 可重复，累加位掩码；`--style auto` 清零 |

实现要点：
- `src/core/ProgressionMatcher.h`：新增 `StyleInfo{id,slug,label}`、`kStyleTable[13]`、`kAllStyles=(1u<<13)-1`、
  `styleBit/styleSlug/styleLabel/styleFromSlug`。位值向后兼容——旧库只写前 6 位，读进来仍是合法掩码。
- `src/session/PluginSessionState`：字段换 `styleMask`，**序列化字节布局不变**（`u8(非零)` + 可选 `u32`），
  读取时 `style & ~kAllStyles` 报错，故旧会话文件仍可读。
- 快照（`RecommendationSnapshot`/`EnrichmentSnapshot`）的 `style` 字段与候选 `styles` 上限同步放宽到 `kAllStyles`。
- `ProgressionEnrichmentEngine` 的 `enrichProgression/polish/finalize/styleFit` 全部改收 `StyleFlags` 并按位测试。

验证：`cmake --build build --config Release` EXIT=0；`ctest` **21/21 通过**；
`recommend_cli` 的 `--style none/citypop/jazz` 输出与改动前存档 `_baseline_*.txt` **逐字节一致**（仅 `PERF` 耗时行不同）；
`--style citypop --style jazz` 与单 citypop 相比分数有变化（命中位取 max 生效）；`--style bogus` 报 `invalid style`。

## 第 2 步：真实语料入库管线（2026-11-16，RS200 试跑）

新工具 `tools/ingest/chords.py`（和弦/调式解析）与 `tools/ingest/build.py`（语料适配、分段、门槛、编码）。
第一份语料是 Rock Corpus RS200 v2.1（200 首，CC BY 4.0），产出 `candidates.json` 后用
`factory_v6_compiler --v9-a-incremental build/factory.db ... out.db` 叠加到 V8 之上。

| 项 | 值 |
| --- | --- |
| 解析覆盖 | 20,722 token 中 96.95% |
| 产出条目 | **847 条**（分段+拆条后候选 1,884，余下为重复 408 / 过薄 245 / 两段都过短 384） |
| 入库结果 | **+838 条，总 2,126 条**，schema 2，`10.7 MB` |
| 末和弦落在主音 | **28.0%**（手写库 66%） |
| 和弦数中位数 | 8（≥8 和弦占 65.9%） |
| 不同末两和弦签名 | 189 种（手写库 116 种） |
| 含 dim / sus | 5 条 / 2 条 |
| 弱起（anacrusis） | 19 首有弱起，11 首的短语保留为独立首小节 |
| 休止拆条 | 124 条来自"小节内休止处切成两段" |

如实记录规则（这轮定稿，后续语料一律照此）：

- **弱起保留**：`rephase()` 不再丢弃首个对齐点之前的事件，而是把它们作为短语的首个短小节，
  写成显式 `@` 时值；`symbols()` 因此允许首/末小节不足整小节。
- **休止切分而不是丢弃**：`render_bars()` 遇小节内有休止时抛 `RestInside`，
  `split_at_rests()` 在该小节处把短语切成前后两段，两段各自入库；只有两段都短于
  `MIN_CHORDS(4)` 时才丢（`silent_bar` 874 → 384）。
- **不再按编辑距离去重**：删除 `near_duplicate()`（距离 ≤1 即判重，曾删掉 327 条真实变体，
  包括两拍 dim 经过和弦）。现在只按 `performance_key` 去掉**完全相同的演奏**
  （同一和弦序列 + 同一组时值 + 同一低音），其它一律保留。

已知未决：编译期会把与既有条目"听感完全相同"的新条目判重（这轮 9 条）。

## 第 2 步续：Billboard 2.0 入库（2026-11-16，第二个真实来源）

McGill Billboard 的 `salami_chords.txt`（890 首）。**边界不推断**：语料自带 `A, chorus,` 这类段标记，
段即一条短语，段内休止与弱起沿用上一节的规则。

| 项 | 数值 |
| --- | --- |
| 解析 | 838 首（12/8 的 32 首跳过）、86,009 个和弦事件 |
| 入库 | 3,093 条短语；+3,077 条；库总 **4,365** 条（`factory-bb.db`，35,258,368 字节） |
| 末和弦落主音 | **34.5%**（手写库 66%、RS200 28.0%） |
| 末两和弦签名 | **650 种**（手写库 116、RS200 189） |
| 和弦数中位/均值 | 12 / 14.6 |
| 小节数中位 | 9（8 小节 795、16 小节 473、4 小节 359、12 小节 218） |
| 风格分布 | pop 3093、folk 1069、anime/cinematic 各 814、experimental 380、jazz 244 |
| 编译 | `V9-A PASS schema=2 inherited=1288 added=3077 total=4365`，16 条按"听感重复"剔除 |
| 测试 | `ctest` 21/21（18.31s） |

三条如实记录规则已升级为 D26：**斜杠要分流**（`/3 /5 /b7` 是低音，`/9 /11 /13` 是色彩扩展）、
**12/8 整首跳过而不是改写成 4/4**（库的小节时值只接受 `[1-9]/4|8`）、
**超长段按小节线二分**（最长段 140 和弦 > `phraseLength<=64`，二分到每段 ≤64，不在小节内部下刀，
也不留单和弦边角——曾因 1 和弦尾段编译失败）。

### 复现命令（本机已验证）

```bash
# 1. 下载语料（跳过缓存；不校验 SSL 是因为代理做 MITM）
set HTTPS_PROXY=http://127.0.0.1:7890
python tools/ingest/fetch_billboard.py
#    -> _hc_evidence\sources\billboard-index.csv  (61,542 B, sha fc394350...)
#    -> _hc_evidence\sources\billboard-salami.tar.xz (219,100 B, sha 92d57977...)

# 2. 解析并编码（billboard 分支）
python tools/ingest/build.py billboard --sources D:\VibeCoding\_hc_evidence\sources --out D:\VibeCoding\_ingest_bb

# 3. 把 candidates.json 叠加到 V8 正式库上（输出路径必须不存在）
build/Release/factory_v6_compiler.exe --v9-a-incremental build/factory.db \
    D:\VibeCoding\_ingest_bb\candidates.json D:\VibeCoding\factory-bb.db
#    报 "duplicate V9 ID or performed music: <ID>" 时删掉该条重试（本轮 16 条）

# 4. 冒烟
build/Release/library_manager.exe inspect D:\VibeCoding\factory-bb.db
build/Release/recommend_cli.exe <query.json> --factory D:\VibeCoding\factory-bb.db
```

语料与中间产物都在仓库外的 `D:\VibeCoding\_hc_evidence\sources\`、`_ingest_bb\`、`_ingest_rs200\`；
编好的库是 `factory-bb.db`（4,365 条）与 `factory-rs200.db`（2,126 条）。**两者都还没装进产品**，
`%CommonProgramFiles%` 下仍是 0.11.0 + Factory V8 / 1,288 条。

### 源码改动清单（本轮）

| 文件 | 改动 |
| --- | --- |
| `tools/ingest/chords.py` | 新增 `mMaj7 / maj11 / maj13 / m13` 性质；`parse_symbol` 支持 `/3 /5 /b7` 低音 |
| `tools/ingest/build.py` | 新增 `load_billboard / _billboard_key / section_events / build_billboard / chunk_bars / bars_tokens`；`main()` 加 `billboard` corpus；`BAR_QN` 加 5/4、7/4 |
| `tools/ingest/billboard.py` | 新文件：SALAMI 解析（`parse_header / parse_lines / parse_song / chord_tokens / resolve_bass`） |
| `tools/ingest/fetch_billboard.py` | 新文件：两份语料下载 + sha256 校验（含代理下的 SSL 处理） |

### 已知未决（下一步的入口）

1. **长条目 + 短查询**：随机 12 条 BB 条目取前 4 和弦反查，只有 2 条命中自己。条目平均 14.6 和弦，
   前缀只覆盖一小角，`match` 打不到门槛。这是检索与显示问题，不是解析问题。
2. **`Dm7 Am C Dm7 F A7` 依旧 0 条结果**：调性判到 D 小调后没有候选过门槛，本轮未动。
3. **RS200 与 Billboard 有重叠**：两库目前是"叠加"而不是"合并"，同一首歌可能两份。
4. **`units`/`repeats` 还没做**：D26 里说的显示层，用来把长段按内部重复切成可点击单元。

## 标签修复：罗马级数 + 主音推断（2026-11-17）

语料入库从第一天起就把罗马级数标错了：`BB_0253_01` 实际是 C 大调的 `C C Am Am F F G G`
（`I vi IV V`），库里存成 `#III #III ii ii #VI #VI I I`。全库带升号的音符占 **52.3%**——流行曲
不可能有这么多变化音，是 bug 不是风格。

### 两个根因（都在 `tools/ingest/build.py`）

1. `degree_tokens()` 先试"最接近的音阶级数 + 升降号"，而升号分支（`base == 1`）在 scale 里排在
   降号前面，于是任何"从主音往下数"的音先撞上 `#` 写法。C 大调里的 F 被写成了 `#III`（走的是
   G 调视角下 E 的 `base == 1` 分支）。改成三趟：**先 exact（同音）、再 flat（−1）、最后 sharp（+1）**，
   并把 `bV` 特判成 `#IV`（出现 4 次，避免整条被当成无调性）。
2. `_billboard_key()` 直接拿 SALAMI 头的 `# tonic:` 当主音。这个字段按段重复、常与全曲不符
   （`BB_0589_05` 头部出现 `A/D/A/D/A` 五行）。改成对全曲和弦做打分：主和弦 ×2、属 ×1.5、
   下属 ×1、关系小调 ×0.5、末和弦 ×2.0，头部 tonic 只作 0.5 的平票裁决；mode 由该主音上
   小三和弦 vs 大三和弦的多少决定。

### 重建结果（新目录，不覆盖旧库）

`python tools/ingest/build.py billboard --sources D:/VibeCoding/_hc_evidence/sources --out D:/VibeCoding/_ingest_bb_v2`
→ `songs=838 tokens=86009 phrases=3124 short=450 duplicate=3361 silent_bar=22`，12/8 的 32 首照旧跳过。

库里带 `#` 的音符 **52.3% → 5.3%**；末和弦分布从 `#III 322 / I 318 / #V 284` 变成
`IV 328 / I 315 / bVII 277 / V 246 / bIII 234 / II 231`。

### 重建库

`build/factory.db` 当时已是 V9（旧标签），V8 的 1,288 条被覆盖过，所以先按链条重建基线：

```
./build/Release/factory_v6_compiler.exe --v8-incremental build/factory-v7.db \
    data/factory-v8/candidates.json data/factory-v8/metadata.json \
    data/factory-v8/approved-ids.txt D:/VibeCoding/_v8_base.db
# Factory V8 increment PASS schema=2 baseline=1272 added=16 families=13 total=1288

python D:/VibeCoding/_tools/compile_bb.py D:/VibeCoding/_v8_base.db \
    D:/VibeCoding/_ingest_bb_v2/candidates.json D:/VibeCoding/factory-bb-v2.db \
    D:/VibeCoding/_tools/dropped_bb_v2.txt
# V9-A PASS schema=2 inherited=1288 added=3107 total=4395 dropped=17
```

`D:/VibeCoding/factory-bb-v2.db` = **4,395 条 / 35,442,688 字节**（旧库 4,365 条）。
`build/factory.db` 已重新指向新库（`-DHC_FACTORY_PREBUILT_DB=D:/VibeCoding/factory-bb-v2.db`）。

### 验证

- `ctest -C Release` **21/21 通过（18.91 s）**。
- `recommend_cli` 对 `C–Am–F–G` 的四栏结果与修前基本逐条一致（RESOLVE #1 72.9 `C→F→C`、
  DEVELOP #1 73.0 `BB_0623_03`、LOOP #1 75.8 `POP_002`），说明标签修复没有动到音乐输出。

### 环境坑（会浪费半天的那种）

`ctest` 的若干用例把临时目录建在 `%TEMP%` 下。本机必须给**反斜杠的 Windows 路径**：
`export TEMP='D:\VibeCoding\_tools\tmp'`。用正斜杠（`D:/VibeCoding/_tools/tmp`）时
`std::filesystem::temp_directory_path()` 会静默回退到 `C:\Users\<user>\AppData\Local\Temp`，
在被沙箱拒绝写入的会话里表现为 `ProductizationTests FAIL: create_directories: 拒绝访问` 和
`ContinuationEngineTests` 直接 `0xC0000409` 崩掉——**两个假警报，都不是代码问题**。

### 续：入库的 intent / cadence 改成推导（2026-11-17 同日）

原先所有入库条目硬编码 `intent="develop"`、`cadence="none"`。`intent` 是引擎真读的字段
（`tools/TemplateJson.cpp:58`，分组 `groupFor()` 直接按它决定进哪一栏），所以 3,107 条真实素材
一律先落"发展"，正是 I01"收束栏短终止式、发展栏长复读"的机制来源。

`tools/ingest/build.py` 新增 `classify_phrase(tokens, mode)`，按引擎自己会认证的证据判：

| 判据（按顺序） | intent | cadence |
| --- | --- | --- |
| 完整周期复读（长度能被周期整除、逐位相同） | loop | loop_closure |
| 末和弦是主和弦，且前一个是 4/5/7（无变化音） | resolve | 5→authentic，4→plagal，7→modal |
| 末和弦等于首和弦且至少 4 个和弦 | loop | loop_closure |
| 出现变化音 / dim / aug | color | none |
| 前一个是 5 且末和弦是 6 | develop | deceptive |
| 末和弦是 5 | develop | half |
| 其余 | develop | none |

3,124 条短语的分布：`loop 1397 / develop 976 / color 499 / resolve 252`；
cadence `loop_closure 1397 / none 1017 / half 446 / authentic 122 / plagal 121 / deceptive 12 / modal 9`。
`builtInTags` 的第一个标签随之改成同一个 intent（原来恒为 `develop`）。

重建：`build.py billboard --out D:/VibeCoding/_ingest_bb_v3` → `compile_bb.py` →
**`V9-A PASS schema=2 inherited=1288 added=3107 total=4395`**，`factory-bb-v3.db` 35,434,496 字节，
已进 `build/factory.db`；`ctest` **21/21（18.61 s）**。

可见效果（查询 `C–Am–F–G`）：`BB_0623_03`（`C Am F G` 复读 4 遍）从**发展 #1 挪到循环 #2**，
发展 #1 换成 `BB_0568_08`；RESOLVE 与 COLOR 前三名没变。

### 仍未做（下一轮才动手）

1. 长条目的**排序**劣势没变：`C–F–G` 的 RESOLVE 前三名仍是短模板（`POP_011`、`FUNCTIONAL_014`、
   `V3_VELVET_034`），252 条 resolve 标签的真实短语进不了前三——这是"长模板 + 短查询"的
   `match` 打分问题（I04 的主战场），不是标签问题。
2. 截断的循环（如 `BB_0568_08` 的 `C Am F G C Am F G C Am F`）被留作 develop，因为它不是完整周期。
   想让它进"循环"栏，得先决定"循环"的定义是"完整闭环"还是"周期性素材"。
3. `nameEn` 的段标签拼串 bug：`('a','intro')` 经 `re.sub(r"[^a-z]","",str(label).lower())` 变成
   `aintrointro`（`BB_0003_00` 显示 `[aintrointro 1]`）。属 I03。
4. 12/8 的 32 首仍整首跳过（库格式只支持 `[1-9]/4|8`）。

## 安装包 0.12.0 / Library V9（2026-11-16）

**产品侧已从 v0.11.0 / Factory V8 推进到 v0.12.0 / Factory V9（4,365 条），并产出离线安装包。**

### 改动（4 处，全部是"让插件接受 V9 库"）
- `CMakeLists.txt:2` 版本 0.11.0 → 0.12.0。
- `CMakeLists.txt` 新增 `HC_FACTORY_LIBRARY_VERSION=9` 首分支：用缓存变量 `HC_FACTORY_PREBUILT_DB` / `HC_FACTORY_PREBUILT_HCA` 把预编译库拷进 build，并 `target_compile_definitions(harmony_library PRIVATE HC_BUNDLED_FACTORY_VERSION=9)`；原 4–8 分支改 `elseif`；`factory_database` 的无条件分支加 `AND NOT ... STREQUAL "9"`（否则 target 重名报错）；hca 的 POST_BUILD 拷贝与 `HC_WISH_FACTORY_DB` 条件都扩到 9。
- `src/wish/WishCatalog.cpp:64-70`：V9 语料没有人工调式标注，`unannotatedPhrase` 条件加 `|| version >= 9`，否则祈愿入口报 `annotation source is missing`。
- `packaging/UnifiedSetup.iss`：`FactoryWithAnnotation` 加 `"9"`；`MinimumPluginVersion` 加 `"9" → "0.12.0"`（不加会落到 `"0.9.0-dev.2"`，旧插件配 V9 库会在加载时报版本不匹配）。

### 构建与出包（复现）
```
cmake -S . -B build -DHC_FACTORY_LIBRARY_VERSION=9   -DHC_FACTORY_PREBUILT_DB=D:/VibeCoding/factory-bb.db   -DHC_FACTORY_PREBUILT_HCA=D:/VibeCoding/_tools/factory-v8-annotations.hca
```
（`factory-v8-annotations.hca` 是 `build/factory.hca` 的副本，1,505,577 字节：V9 语料没有新的调式标注，hca 沿用 V8 那份。）

`tools/build-installer.ps1` 是无 BOM UTF-8 + 中文，PowerShell 5.1 直接 `-File` 会 `ParserError: Unexpected token`；必须先写一份 `utf-8-sig` 副本再跑（副本用完即删）：
```
powershell -File tools/_tmp-build-installer.ps1 -BuildDirectory build   -OutputDirectory D:/VibeCoding/build-installer/output   -Iscc D:/VibeCoding/_tools/InnoSetup-6.7.4/ISCC.exe   -FactoryDatabase D:/VibeCoding/factory-bb.db
```

Inno Setup 6.7.4 在本机只能用便携模式（注册表与 Program Files 写入被拒）：
`innosetup-6.7.4.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /NOICONS /PORTABLE=1 /DIR=D:\VibeCoding\_tools\InnoSetup-6.7.4`，且 `TEMP`/`TMP` 必须先指到可写目录，否则不生成日志。

### 产物
- `D:\VibeCoding\build-installer\output\HarmonyContinuation-0.12.0-Setup.exe`，6,192,170 字节，sha256 `c57b5fbd604c447ad5fbe6ee289934d1ee79f7efb24764230bba2af920688900`（同目录有 `.sha256` 与 `SHA256SUMS.txt`）。
- 载荷核对：`stage-9/factory.db` 与 `factory-bb.db` 同 sha（`3be55075ece6783b…`），`library_manager inspect` = version 9 / schema 2 / 4365；`factory.hca` sha 前 16 位 `f7677935ab7ccf16`；VST3 bundle 的 `Contents/Resources` 只剩三个 json（`factory.db`/`factory.hca` 由安装器写进 `%ProgramData%`，不在 bundle 内）；插件 dll 4,653,568 字节 + 10 个 VC143 运行库。

### 验证到哪一步（重要）
- `ctest` 21/21 通过（14.70s），前提是 `TEMP`/`TMP` 指到 `D:\VibeCoding\_tools\tmp`（进程沙箱拦 `%LOCALAPPDATA%\Temp`）。
- 隔离安装（`-TestRoot`）能把文件全部装完，但**卡在写 `HKCU\...\Uninstall\...` 注册表失败（错误代码 5）后回滚**——本机连 HKCU 都写不了，装完的实际效果只能在目标机验证。

### 目标机使用
以管理员运行 Setup，勾选"主程序 + Library V9"；插件装到 `C:\Program Files\Common Files\VST3`，库装到 `%ProgramData%\HarmonyContinuation\Libraries\Factory\9\`。

### 便携包（绕过安装器，2026-11-17 追加）
用户在本机运行 Setup 时报 `安装程序无法创建目录 "C:\Users\22867\AppData\Local\Temp\is-XXXX.tmp"。错误 5: 拒绝访问。`
——这个对话框是**提升后的子进程**报的，而本会话无法复现：用 3 行最小 iss 做对照实验，
`PrivilegesRequired=lowest` → `EXIT=0`，`PrivilegesRequired=admin` → `EXIT=1`（本会话弹不出 UAC，
`ShellExecute(runas)` 直接失败）。所以"提升后能否写 %TEMP%"只能在目标机验证。

因此补一条不依赖 Inno 的安装路径：`D:\VibeCoding\build-installer\output\HarmonyContinuation-0.12.0-portable.zip`
（3,879,555 字节，sha256 `79282f656fe64ac0179a99d85ef6f32825d6746d4cdd1236d549e890e52693f8`），
内含 `VST3\HarmonyContinuation.vst3`、`Library\factory.db`、`Library\factory.hca`、`install.bat`、`README.txt`。

`install.bat` 只做三件事：xcopy 插件到 `%CommonProgramFiles%\VST3`、copy 两个库文件到
`%ProgramData%\HarmonyContinuation\Libraries\Factory\9\`、把 `active.txt` 写成 `9`。

**坑（已避开）**：`active.txt` 必须是恰好 1 字节的 `9`。`echo 9 > file` 会写成 `9 `（带尾随空格），
而 `activeVersion()`（`src/library/LibraryStore.cpp:21-33`）只裁 `\n`/`\r`，`from_chars` 读不完就抛
`invalid active library pointer`，插件会退化成"没有库"。脚本里用的是 `<nul set /p "=9" > active.txt`，
实测产物是单字节 `9`；把三条拷贝命令在 `D:\VibeCoding\_installer_test\` 下空跑过一遍，落点与大小都对
（`factory.db` 35,258,368 字节、`factory.hca` 1,505,577 字节）。

## V9-B 后续实验：本机重建结论

完整报告见 [`V9B_RECONSTRUCTION.md`](V9B_RECONSTRUCTION.md)。核心三条：

1. 作者的 12 见证**无法原样复现**：A2 研究 CSV 与 `HarmonyContinuation-v9-a-evidence/` 都不在仓库、也无公开来源。
   用 RS200 v2.1（CC BY 4.0）重建 111 首，得到 16 见证 / 22 路线，退出分布与作者报告**结构同构**
   （exact 19+3 vs 14+3；contextual 14+7+1 vs 10+6+1；qualified 0）。
2. 新发现两个机制问题：`reviewed()` 白名单不含 V9 标签（`V9A-original-editorial`），
   V9 素材与 1,016 条 V5/V6 素材一样永远拿不到 `+4` 发展加分；
   `increment_seam_near` 在素材**每个**索引扫描，不锚定 `context_events` 设计边界，
   本批 22 条路线全部落在索引 9–10 而设计边界是 4。
3. Exact 的 `alternate_endpoint` 是枚举不足（每个 (template,key) 只物化一个落点，
   `src/core/ProgressionMatcher.cpp:275-287,380-385`），不是阈值问题；
   B6 的 `--recover-consumed-tail` 对中段落点无效（22 条只尝试 3 条、保留 0 条）。

## 现在的产品状态（要接手的核心）

v0.12.0 已有：四入口（匹配优先 / 灵感优先 / 升级进行 / 和声祈愿）、Factory V9 4,365 条（Billboard 3,077 + 手写 1,288，见「安装包 0.12.0 / Library V9」）、
G13-C 生成候选接入推荐 Worker、Session 5 / Snapshot 4、试听（Host 优先，回退 waveOut）、
MIDI 拖出（DAWClip）与带标注保存（AnnotatedFile）。

安装包：`D:\VibeCoding\build-installer\output\HarmonyContinuation-0.12.0-Setup.exe`。
另有一份**未安装**的候选库 `factory-rs200.db`（2,126 条，Rock Corpus）在 `D:\VibeCoding\` 根下；
`factory-bb.db`（4,365 条）已进产品。

### 已知问题台账（2026-11-17 建立，I02 已修，其余未修）

用户在使用 0.12.0 时报出的四条实感问题，连同代码定位与实测数字记在 [ISSUES.md](ISSUES.md)：
I01 长度与意图错位（收束=短终止式、发展=长复读）、I02 打开 UI 卡约十秒（两遍全量解析 35 MB 库）、
I03 推荐名是级数串（另含入库引入的 `[aintrointro 1]` 段名拼接 bug）、I04 长进行违背"步进"意图。
四条的共同根因是"库里没有乐句/单元这一层"与"加载路径没有缓存"。

**2026-11-17 进展：I02 已修。** 修法四处（`annotationsForFactory` 接受已加载库、Controller 按
`active.txt`/size/mtime 缓存、读库路径砍掉 `checkTemplate` 的序列化往返、`decodeTemplateData` 不再
`dump` 后再 `parse`），并让 `setLibrary` 在库字节未变时跳过 4,365 次 fingerprint 拼串。
V9 库单次加载 5.6–6.8 s → **0.97 s**；首次打开编辑器 ≈ 11.7 s → **≈ 1.3 s**，之后每次 ≈ **60 ms**；
`ctest` 21/21。新增 `tools/attach_bench.cpp`（目标 `attach_bench`）用于复测分项。
细节见 [ISSUES.md](ISSUES.md) I02 节。I01/I03/I04 仍未动。

## 最新四轮实验的结论（全部未进产品）

- **B5 语料统计**：变阶模型在 Billboard 上 Top-1 37.1%→42.0%、困难 MRR 0.409→0.465，
  但调外根音 NLL 5.098→6.371 变差；Factory V8 回放 100 输入，实际 Worker 可达只有 3/9。决定：只留离线。
- **B6 检索统计**：62 个"库里有但没进列表"的输入中 30 个卡资格门槛、32 个没形成窗口/调性解释、
  **0 个是去重/容量/分页吃掉的**；只找到 1 个真缺陷（`V3_HOME_055` 尾端被最优路径耗尽）。
  新增 `recoverConsumedFactoryTail` 开关默认关闭。
- **B7 覆盖审计**：4/4 的 249 条测试句里 **115 条在库里找不到近似完整尾段**；
  3 个事件的行命中 78/84，7–8 个事件的行只有 4/65 且 0/65 有原生接缝近似。
  结论：瓶颈是**素材长度与长度分布**，不是检索。给出 V9 建设优先级（>6 变化事件的连续发展 / 明确和声节奏 / 12/8）。
- **V9-A / V9-B**：把库从 1,288 加到 1,312 再加到 1,332（全部为"原创编辑"，商业入库批准数 0）。
  独立作品测量：RS200 实际近似可达 V8 18 首 → V9-A 18 首 → V9-B 15/50 组；WJazzD 7/72；
  Top3/Top10 命中四路完全相同。**长目标、混合时值、小调、调外根音组四路零增益。**
  决定（D21/D22）：保留实验库，不改引擎。

## 决策与硬约束（改代码前必读）

`DECISIONS.md`：D26 / D25 / D24 / D23 及 D22 / D21 / D20 / D19 / D18 / D17 / D16 / D15 各自绑定一轮实验的验收结论。
`AGENTS.md`：按需读取（禁止全目录遍历）、不编造验证结果、每类事实单一权威来源、
GitHub 推送必须走代理、**严禁写非必要的新测试脚本/程序**、表达禁止防御性套话。
`MAP.md`：安装位置 `%CommonProgramFiles%\VST3\HarmonyContinuation.vst3`、
Factory `%ProgramData%\HarmonyContinuation\Libraries\Factory\<版本>\factory.db` + `active.txt`、
用户库 `%LOCALAPPDATA%\HarmonyContinuation\user.db`、备份 `%ProgramData%\...\Backups\Plugins`。

## 本次清理记录（2026-11-15）

已删除 D:\VibeCoding 下约 9.5 GB 被取代的副本，删除前逐项核对过"无独有源码、无构建依赖、无安装器"：

| 已删 | 内容 | 核对结果 |
| --- | --- | --- |
| `HarmonyContinuation-v4` (3.7G) | 6efacf3 / phase4b | 源码在 v4 历史中，产物是 build-dev/installer 输出 |
| `HarmonyContinuation-release` (2.4G) | 9d70c86 / library v3 | 同上；含 duplicate-smoke 安装演练输出 |
| `HarmonyContinuation-phase6` (2.1G) | dev.7 时代，`.git` 已损坏 | 只有 build-rc2 产物与 VS 缓存 |
| `_archive/HarmonyContinuation-20260925-phase4c` (2.1G) | 9 月工作副本 | 只有 build-core/build-vst3 与 .vs |
| `HarmonyContinuation-v06-compare` (14M) | 39a7bd1 / v0.6.0 | 283 个改动是把新文件覆盖到旧 tag 的产物 |
| 4 个 `HarmonyContinuation*.zip` (1.4G) | 9/24–9/30 快照 | 内容同上述目录 |

保留并确认可用的参照物（在 D:\VibeCoding 之外，不受清理影响）：

- 已安装插件 `%CommonProgramFiles%\VST3\HarmonyContinuation.vst3`（8.2 MB）
- 活动 Factory 库 `%ProgramData%\HarmonyContinuation\Libraries\Factory\8\`（6.1 MB），`active.txt` = 8

唯一需要留意的残留引用：`data/factory-v6/audit/audit_factory_v5.py:20-24` 里硬编码了
`D:\VibeCoding` 下的 `HarmonyContinuation-g04r-delivery-20261024/…`、`HarmonyContinuation-v6-evidence/…` 等路径。
这些目录在清理前就已不存在，脚本是历史一次性审计用途，**不是构建依赖**，不影响任何编译或测试。

## 如果要恢复 git

```
git init && git add -A && git commit -m "snapshot: main@d3552c2 (history intentionally dropped)"
git apply --3way _handover/v9b-20261115-vs-main.patch   # 需要 v9-b 增量时
```

### 续：谐波节奏归一化（2026-11-17 同日）

`CCGGAmAmBB` 这类"每个和弦跨两小节"的语料，在库里的表现是相邻小节重复同一个和弦；
它让条目凭空变长、让去重失效、也让匹配时对不上用户的一小节一和弦的查询。
现在入库前按"相同小节的连读长度取最大公因数"折叠：8 小节的两小节一和弦 → 4 小节的一小节一和弦。

- 归一化位置：`tools/ingest/boundaries.py:collapse_run_repeats()`，在 `build.py` 的
  `normalize_rhythm()` 里对**整段**与**切分后的每个单元**各跑一次（`build.py:791` 起）。
- 只折叠"整段/整单元都成比例"的情况；混合谐波节奏（一个和弦真持两小节、旁边是一小节一个）
  按 gcd=1 原样保留，不做伪归一化。
- 效果：`normalised=2312`（被折叠的段/单元数）；条目 3,719 → 3,465（折叠后出现 1 和弦的单元被 `MIN_CHORDS=4` 挡掉 211 条）；
  库 4,994 → **4,732** 条、25.4 → **24.0 MB**；小节数中位 5 → **4**。
- 验证：`ctest` 21/21（19.02 s）；`library_manager inspect` = `library_version=9 schema=2 progressions=4732`；
  `C–Am–F–G` 的发展栏仍是 3/4/4 和弦，收束栏未变。
- 仍未做：混合谐波节奏的条目仍有 1,672 条含相邻重复小节（真实持音，不是冗余），
  若要进一步压成一拍一和弦需要改成"小节内多和弦"的存储形式，属格式变更。

## 第 4 步：乐句单元入库（I04，2026-11-17）

用户报的四件事里，第 1 条（收束栏推荐太长）和第 4 条（长进行太多、不符合"步进"思想）同一个根因：
库里存的是整个语料段（中位 9 小节 / 12 个和弦），引擎又明令禁止截断
（`src/core/ContinuationEngine.cpp:414` 的 `wrapped_before_source_end`），于是推荐必然长。
本次把切分放到入库层：每个入库条目是一个乐句单元。

### 做法

`tools/ingest/boundaries.py`（新文件，纯函数、无副作用）：小节线级边界证据 + 全局 DP。
五条证据（重复重启 / 终止式落点 / 句首和弦重现 / 休止 / 密度突变），加两项约束：

1. **只在小节线上切**，并且奖励落在 4 小节超节拍线上的切点（默认 +0.35）。
2. **长度硬下限 4 小节**、软上限 8 小节：低于 4 小节的残余片段不允许产生（除非整段本来就短），
   因为 1–3 小节的尾巴在库里就是噪声。

参数在**按歌曲 id 切分**的调参组（1,222 段）上网格搜索后冻结，留出组（1,039 段）只跑一次：

| 指标 | 调参组 | 留出组 |
|---|---|---|
| 切点落在 4 小节超节拍线 | 87% | 89% |
| 切点前一和弦是属/下属 | 60% | 56% |
| 单元级去重 | 48.2% | 48.4% |
| 单元长度中位 | 4 小节 | 4 小节 |

### 结果

`python tools/ingest/build.py billboard --sources … --out D:/VibeCoding/_ingest_bb_v4 --split-units`

```
songs=838 tokens=86009 phrases=3735 short=450 duplicate=9017 no_style=0 silent_bar=22 static=615
```

- `static=615`：全段只有一种和弦、或只有一种小节型的"静态 vamp"整条剔除，不入库。
- `duplicate=9017`：单元级重复（同一乐句在不同段落反复出现）被裁掉，这正是用户预期的"去重"。
- 编库：`compile_bb.py` → **`V9-A PASS schema=2 inherited=1288 added=3706 total=4994`**，
  `factory-bb-v4.db` 25,427,968 字节（v3 是 35,434,496，反而更小）。
- 条目长度：小节数分布 `{2:43, 3:26, 4:1716, 5:402, 6:513, 7:248, 8:787}`，中位 5 小节 / 6 个和弦
  （改造前中位 9 小节 / 12 个和弦）。
- `ctest` 21/21（18.83 s）；`recommend_cli` 用 `C–Am–F–G`：DEVELOP 前三从"17/20/13 个和弦"
  降到"3/4/4 个和弦"，`totalMatchMs` 213 ms（v3 时 299 ms）。

### 仍未做

- `Dm7–Am–C–Dm7–F–A7` 仍 0 结果（调性判到 D 小调后无候选过门槛），与本步无关的既有缺陷。
- 收束栏前三仍是短模板（`support=76` 的单个 `C`），那是 I01 的 support 计分问题，不是长度问题。
- 手写模板库（`V3_*`/`V7_*`）里仍有 14 和弦的长条目，本步只改了真实语料部分。

## 第 5 步：六类入库根因修复（2026-11-17）

### 做了什么

| 类 | 根因 | 修法 | 位置 |
|---|---|---|---|
| ① 级数标注双重转调 | `render_bars` 已把整句重拼到 C/A，`bars_tokens` 又按原曲调再减一次 key，87.8% 条目名字错 | 新增 `TONIC_OF = {"major": 0, "minor": 9}`，标注与边界证据统一按它读 | `tools/ingest/build.py` |
| ② 调性推翻 header | 纯和弦计数打分，header 仅 0.5 平票，285/855 与 SALAMI header 不符（202 首正好差五度） | 改成 header 先验，仅当 `header_share < 0.05` 且另一调解释力 ≥3 倍才推翻 | `_billboard_key()` |
| ③ 重复乐段未折叠 | 静态 vamp 判据只认"全段一种和弦/一种小节型"，2 小节 cell 循环 ×8 逃过判据被切碎 | 新增 `period_of()` 找最小周期，`cycles = ceil(4/period)`，单元 = cell×cycles（最小 4 小节） | `tools/ingest/boundaries.py` |
| ④ 弱起当正文 | 首小节在段内不再出现（1,621 段 / 20.5% 段首有 ≥2 和弦） | 新增 `pickup_length()`，弱起留在首单元但只换回 `PICKUP_WEIGHT=0.6` 长度信用，切点证据在弱起后 1–2 小节内被禁止 | 同上 + `build.py` |
| ⑤ 单个坏 token 丢整段 | `chord_tokens()` 一个 token 解析失败就 `return []` → 整段丢弃 601 段 | 改逐 token 容错；`&pause`/`*` 映射为 `R`；补 `maj(9)`/`7(#9)` 括号扩展与 `/11` `/13` | `tools/ingest/billboard.py` |
| ⑥ 谐波节奏 | `CCGGAmAmBB` 已折叠，但长段仍按原样存 | 由 ③ 的周期折叠一并处理 | — |

另修：`tools/TemplateJson.cpp:222` 的解析期模板上限 4,096 → 16,384。它只是解析候选文件的护栏，
不是库的上限（v4 的 4,994 条一直能加载），4,360 条候选在它上面直接失败。

### 结果

- 重建：`phrases=4189 songs=858 tokens=108017 normalised=2683 short=1804 duplicate=6703 static=776 pickup=413 folded=2167 folded_units=4140`
  - 对比 v5：`phrases=3735 tokens=86009`。token 涨 25% 来自不再丢小节。
- 编库：`V9-A PASS schema=2 inherited=1288 added=4167 total=5455`，`factory-bb-v6.db` 28,241,920 字节。
- 单元长度：`{2:46, 3:39, 4:1954, 5:550, 6:608, 7:291, 8:701}`，中位 4 小节
  （v5 中位 5 小节且 8 小节以上有 936 条）。短于 4 小节的 85 条（2.0%）不是切出来的残渣，
  而是原段落本身就只有 2–3 小节（例如 `BB_0083_02` 全段 `C@2 F@2 | G/D@2 F9@2`），DP 对它们没有切点。
- 末和弦落主音 23.9%（v5 按同口径 9.6%，但 v5 的标签是错的、这个数没有意义；手写库 66.0%）。
- 留出组指标（`_tools/holdout_v6.py`，按 song md5 奇偶分组，与调参同一划分）：
  `sections=3491 units=5880 median=4 cut-on-4-grid=74.1% dup=58.8%`；调参组 73.5%。
  切点网格率从 v5 的 89% 降到 74%，**不是退步而是口径与证据都变了**（`_tools/why_grid2.py`
  在同一批段落上只换标签重算）：
  - 用修好的 C/A 标签：72.7% 在网格上；用修前的双重转调标签：84.7%。
    也就是说旧标签下 84.7% 的"对齐"是错的音乐对齐出来的假象——网格奖励在错误的和声空间里
    压过了其余证据。
  - 网格奖励扫描（`_tools/tune_metre.py`，只跑调参组）：bonus 0→0.9 时网格率 41%→99.6%，
    但"切点前一和弦是属/下属"从 78.0% 掉到 42.5%；两者同时成立的比例在 0.35–0.55 之间有
    平台（43.7%→45.3%）。所以提高奖励只是把切点变成算术切点，不是更像乐句。
- `ctest` 21/21（24.55 s）。`recommend_cli` 三组查询：`I–vi–IV–V` 的 DEVELOP #3 出现真实素材
  `BB_0568_02_u1`（Bb 大调，match 0.852）；`C–G` 的 RESOLVE #1 变成 `BB_0367_14`（match 1.000）；
  `C–F–G` 的 RESOLVE #1 是 `POP_011`（`Am–F–G–C`）。
- `Dm7–Am–C–Dm7–F–A7` 仍 0 结果（既有缺陷，本轮未动）。

### 仍未做

- `BB_0705_00` 段首那个 `IV`：SALAMI 原标注首小节就是 `F/C C Am7`，`pickup_length` 判 0（该小节在两轮里都出现）。
  需要更细的"哪些音是弱起"证据（用户已确认这个 4 不属于段首，但现有信号不支持）。
- `BB_0111`/`BB_1300` 一类"交替 vamp"（两种小节型交替）现在被折叠，但折叠后仍是 4 小节，未验证是否等于用户心里的乐句。
- 拐点：整体切点网格率从 89% 降到 74%，需人工抽查确认不是切点退步。

## 第 6 步：v7 按五条人工判据重切（2026-11-17）

用户逐条看过 15 段留出组抽查后给出五条判据（m06630），本轮全部落进入库层。引擎与 UI 未改。

**改动**（`tools/ingest/build.py`、`tools/ingest/boundaries.py`）：
1. **vamp 剔除**（第 7 条）：新增 `tonic_dominant_share(bars, mode, meter)`，按发声时长算调内
   主+属占比，单元级超过 `VAMP_SHARE = 0.66` 不入库。段级不筛。
2. **持音合并**（第 8 条）：新增 `merge_held_bars(bars)`，`G@4|G@4|G@4|G@3` → `G@15`；
   调用点在切分之后、算和弦数之前。
3. **ABAC 单元**（第 9 条）：新增 `chain_cells(bars, size)`，先试 2 小节一块，只有每块都够
   `MIN_CHORDS` 才用，否则退到 4 小节块配对。新增 `attach_held_tails()` 把纯持音尾并进前一条。
4. **时值全标注**（第 5 句）：`render_bars` 不再对单和弦小节省略 `@`；`symbols()` 额外放行
   "单 token 且时值 > 小节跨度"的持音单元。
5. `boundaries.collapse_run_repeats` 修掉丢 `@时值` 的 bug（改为保留 `members[0]` 原文）。

**为什么 2 小节块只在 13.4% 上生效**：全语料 15,481 个 2 小节块里 13,401 个（86.6%）不到
4 个和弦（分布 `2:11002, 3:2399, 4:1684, 5:220, 6:133, 7:22, 8:21`），按 2 小节切会立刻被
`MIN_CHORDS = 4` 滤掉，所以只有够和弦的那批用 size=2，其余退回块配对。

**结果**：`phrases=2773`（v6 是 4,189）、`vamp_unit=4103`、`merged=5719`、`chains=2269`、
`pickup=413`、`folded_units=4140`。小节直方图 `{2:164, 3:215, 4:1607, 5:342, 6:197, 7:127, 8:121}`，
和弦数最少 4、id 全唯一、无一行超 `VAMP_SHARE`、17,625 个 token 全带 `@时值`。
编库 `V9-A PASS schema=2 inherited=1288 added=2757 total=4045`，19,304,448 字节；
三处 `factory.db` 已同步；`ctest` 21/21（24.11 s）。

**已知偏差**：`BB_0698_02` 的 u2/u4 与更早段落 `BB_0698_01` 逐字相同，被全局去重吃掉，
只剩 `_u0`。全库 3,226 个多单元段里有 2,659 个（82.4%）最后只落 0–1 条——`seen` 是跨段共享的。

## 第 7 步：v8 去掉小节下限，最小可用乐句（2026-11-17）

用户看过 v7 后指出两件事：**小节数不是限制乐句的因素**（"我一个 60bpm 的曲子还有可能一小节
一个乐句"），以及**ABAC 被去重吃掉正是他要的结果**（"按最小可用乐句进行切分，然后库里仅保留
最小乐句单位进行匹配和构造"）。后者不改，前者是这一轮的全部内容。

**改动**（`tools/ingest/boundaries.py`、`tools/ingest/build.py`）：
1. 删除 `MIN_UNIT_BARS = 4`（全仓已无引用）。
2. `segment()` 新增 `content` 参数：下限 `low` 变成**和弦变化数**，上限仍是小节（`high`）
   与和弦数（`amount_high`）。理由：语料无 tempo，2 小节在 60 bpm 是 8 秒、170 bpm 是 2.8 秒，
   跨曲不可比；和弦变化数是唯一不依赖 tempo 的度量。
3. 周期折叠改成存**最小完整周期**：1 个周期，只有该周期不够 `SPLIT_LOW` 个和弦变化才存 2 个。
   原来按"铺满 4 小节"取 N 个周期，会把 2 小节 vamp 存成 2 轮、4 小节短语存成整段。
4. `length_cost` 删掉主动奖励长单元的项（`-0.35*weight*...`，注释还是
   "the user asked for whole phrases"，与现在的取向相反）。
5. `attach_held_tails` 加小节上限检查，避免把短单元并成超过 `SPLIT_HIGH` 的长条目。
6. `chain_cells` 的 2 小节门槛改用 `SPLIT_LOW`（和弦数），不再用 `MIN_CHORDS`。

**结果**：`phrases=2854`（v7 是 2,773）。小节直方图
`{1:16, 2:325, 3:347, 4:1580, 5:311, 6:149, 7:64, 8:62}`——**首次出现 1 小节条目**（16 条，
都是 4 个和弦变化挤在一小节里，如 `BB_1203_01_u0 = I VI IV V`、Tainted Love 的
`I bIII bVI IV`）。和弦数 4–18，中位 5；发声长度 4–64 QN，中位 16 QN。
**没有一条超过 8 小节**（`SPLIT_HIGH` 生效）。
编库 `V9-A PASS schema=2 inherited=1288 added=2838 total=4126`，18,661,376 字节，三处已同步，
`ctest` 21/21（22.52 s）。

**续写空间的量化**（这是长度变短的代价）：`q_IV`（C, G 两个和弦）查询下，
库中留有 ≥2 个和弦续写的条目比例——查 2 个和弦 100%、查 3 个 64%、查 4 个 44%、查 5 个 27%。
短查询（1–3 和弦）资源充足，长查询（5+）会出现续写几乎为零的条目。

## 第 8 步：I03 段名修复与 0.13.0 安装包（2026-11-18）

用户看抽检表后定："不要设小节限制。没有"明显不像乐句"的条目。试着传到远端并打包吧。" 于是只修段名、不动切分。

- **根因**：`tools/ingest/billboard.py:156` 把段名存成 `(字母+名, 名)` 元组（如 `('Averse','verse')`），
  `tools/ingest/build.py:1013` 又对 label 做 `re.sub(r"[^a-z]","",str(label).lower())`，`str(tuple)` 只留字母
  → `cverseverse`。改为直接存纯段名 `marker.group(2).strip()`，VST3 里名字从 `[cverseverse 3 u1]` 变成
  `[verse 3 u1]`。
- **重跑入库**：`phrases=2854`，与 v8 同一批（858 首、同一套切分参数）。
- **编库**：`V9-A PASS schema=2 inherited=1288 added=2838 total=4126`，18,636,800 字节，
  sha256 `cf7d654e68b35196c465f92ec26aaf306fcaf2b2c25ef758fb3862f03f243e42`。
  与 v8 逐条比对 `id`/`sequence`/`rhythm`/`intent`/`cadence`：**0 条差异**，只有段名和体积变了。
- **构建**：`cmake --build` 在本机静默失败（MSBuild 文件跟踪 `TRK0002`/`MSB6006`，CL.exe 退 2），
  必须 `MSBuild.exe HarmonyContinuation.sln /p:Configuration=Release /m:1 /nodeReuse:false /p:TrackFileAccess=false`。
  细节写进 `LIBRARY_MANIFEST.md` 的"本机构建环境注意"。`ctest -C Release` 21/21（22.94 s）。
- **打包**：`tools/build-installer.ps1` 原为无 BOM UTF-8，PowerShell 5.1 直接语法错，已加 BOM。
  产物 `build-installer/output/HarmonyContinuation-0.13.0-Setup.exe`，5,641,000 字节，
  sha256 `84cb8509463caeb27646b3fb7cf0367583d0740072ca25e1e007d47b8c00e9fd`。
- **推送**：本会话沙箱的 schannel 证书通道坏了（`SEC_E_NO_CREDENTIALS`），需 `danger-full-access` 才能 HTTPS。
  远端 `main` 在 v0.11.0 且与本地是两套无关历史，故推成新分支 `v9-2026-11-18`（未覆盖 main）。
- **未做**：BB_0705_00 弱起、
  `Dm7–Am–C–Dm7–F–A7` 零结果仍在 ISSUES。

## 第 9 步：意图三分、命名与风格扩类（v10 / 0.14.0，2026-11-18）

用户提三件事：发展栏里全是循环、名字仍是级数、新条目没有十二大类风格。

- `tools/ingest/build.py::classify_phrase` 按"怎么收尾"重排：落主→resolve、落属→resolve/half、
  周期或末和弦回卷（四/五度、≥3 和弦、≥2 小节）→loop、变化音→color、其余 develop。签名加 `bars`。
- 新增 `tools/ingest/naming.py`：句式名 → "某进行变式"（前三位相同、编辑距离 ≤2）→ "起句 · 走向 · 收尾"。
  来源歌名写进 `note`，`aliases` 带上 id 和歌名；两处入库路径（RS200 / Billboard）都接上。
- 风格：`ProgressionMatcher.h` 加 `Classical=1u<<13`、`Other=1u<<14`（`kAllStyles` 改 15 位），
  `style_affinity` 加 classical / functional 两条规则。
- 数据：candidates 2,854；纯级数名 0；句式名 53 / 变式 56 / 生成式 2,745。
  库 4,126 条 / 18,726,912 字节 / sha256 `d73b41f2…2e532b2`；`ctest` 21/21。
- 坑：`factory_v6_compiler` 只在 `HC_FACTORY_LIBRARY_VERSION` 为 6/7/8 时生成 target，
  本机编 9 时用的是旧 exe，遇到新风格名直接 `unknown style`。用
  `cmake -S . -B build-tools -DHC_FACTORY_LIBRARY_VERSION=8` + `MSBuild build-tools/HarmonyContinuation.sln /t:factory_v6_compiler`
  单独编一个即可。
- 坑：`ctest` 需要 `TEMP`/`TMP` 指到工作区，`%TEMP%` 在本沙箱不可建目录。

## 第 10 步：v10c / 0.14.1（2026-11-18）

风格打分多维化（classical 1,119、functional 1,880），弱起判据加"首小节时值不满"（命中 0 条）。
库 `factory-bb-v10c.db` sha256 `2de2605f…89f274`，包 `HarmonyContinuation-0.14.1-Setup.exe`
sha256 `a9599cce…c62a1`，`ctest` 21/21。版本号规则见 DECISIONS D48。
下一步：架构评审（用户要求）→ 大优化。

## 第 11 步：v10d / 0.14.2（2026-11-18）

第一批"地基"的前三件：12/8 回收、块级去重 + 丢弃名单、乐句邻接占位。

- 拍号：`build.py` 白名单放宽到 `[1-9][0-9]?/(4|8)`；`TemplateJson.cpp` 拍号解析改成两个小整数
  （1–32 / 4|8）。语料 890 首全入（原 858），BB 2,838 → 2,926，新增 88 条 12/8。
- 去重：`performance_key` 移到 `chunk_bars` 循环内（与库存储粒度一致），丢弃行写
  `_ingest_bb_v10d/candidates.json/dropped.tsv`；交叉基线仍丢 16 条（名单 `dropped-compile.txt`）。
- 邻接：`phrase-links.tsv`（prev/next/song/section），段内与跨段都连，给乐句级发展推荐占位。
- 库 `factory-bb-v10d.db` 4,214 条 / 19,181,568 字节 / sha256 `9e0246e2…3b793c`；`ctest` 21/21。
- 下一步（仍是第一批）：继承条目幂等重标注、派生 `familyId` 折叠、库版本协商与未知风格位兼容。

## 第 12 步：v0.14.3 向前兼容（2026-11-18）

库版本高于插件时不再拒载。

- `tools/TemplateJson.cpp`：未知风格名从"抛错"改成"跳过"，删掉 `styleFromName`。
- `src/library/LibraryStore.cpp`：已装库版本高于内置版本时给 `warning`，库照常返回。
- `tests/ContinuationEngineTests.cpp`：原断言"拒绝未知风格"改成"新库可加载 + 未知风格位被丢掉"。
- `ctest` 21/21（24.75 s）。库未变（仍是 v10d）。

## 第 13 步：v0.14.4 列表折叠（2026-11-18）

- `src/ui/MainView.cpp`：`filteredLibrary()` 按 `libraryFamilyKey()`（调式 + 级数/变化音串，
  与引擎 family 同口径）折叠，`libraryFoldCounts_` 记录族大小；解析不出事件的条目按 id 成键。
- 列表行显示代表条目并追加 `×N`；检索/筛选/分页作用在折叠后的行上。
- v10d：4,214 条 → 3,924 行，隐藏 290 行，最大一族 7 条；`ctest` 21/21。
- 待办：1,288 条继承条目重标注（等用户确认是否覆盖人工调过的风格权重）。
