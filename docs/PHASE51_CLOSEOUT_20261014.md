# Phase 5.1 质量收敛与集中验收交付

2026-10-14。工程范围完成；真实 Cubase 和音乐取舍待用户验收。保留 dev.6 原二进制，本轮只修改测试和文档，没有修改推荐算法、评分、插件、Factory、Schema 或安装程序。主动安排必要子代理已写入 AGENTS；主代理负责最终判断和交付。

## 最终状态

| 字段 | 结果 |
|---|---|
| ENGINEERING COMPLETE | YES |
| R02 OFFLINE ROOT CAUSE | CONFIRMED |
| R02 CUBASE ROOT CAUSE | UNKNOWN |
| ABSTRACT MUSIC REVIEW READY | YES |
| R01 V5 CANDIDATE VERIFIED | PARTIAL |
| FEATURE FLAG COMBINATIONS VERIFIED | PARTIAL |
| FACTORY V4 CHANGED | NO |
| ALL EXPERIMENTAL DEFAULTS OFF | YES |
| READY FOR CUBASE ACCEPTANCE | YES |
| READY FOR FORMAL RELEASE | NO |

PARTIAL 的边界：R01 召回、默认展示、路线及实际 MIDI 低音已验证，音乐合理性未由人试听确认；联合开关仅有有限 core 组合证据，不提升为插件支持。没有合并 main、创建正式 Tag、发布 v0.10.0 或安装到真实用户环境。

## 身份与复用证据

开发分支 `development/phase5-20261013`。插件版本 `0.10.0-dev.6`，生产 BuildCommit 始终为 `3a582e4d8ea9bca5df4507a3b7abfabb08d2ecd9`。本轮测试/文档提交与该生产构建身份不同，最终完整 HEAD 见增补包 `proofs/git-proof.json`。开始时本地及远端 HEAD 均为 `c06cf852c8c4d835fa969de68cf76c614f4b160c`。

| 对象 | SHA-256 |
|---|---|
| Factory V4 / 657 / Schema 2 | `74c529828f8c4b934bbaf4ada719aa95491ab498b58885522124ff6505671534` |
| DEFAULT-OFF 插件 | `6e0c05ce97500a141f9adc2c432d981169d80120c99e42896378d7b7dc0d3ad5` |
| HF-ON 插件 | `84ba1780d7340dd7bce1f6c054403892cf9cf0b0b260f521fd56331c4d718aea` |
| ABSTRACT-ON 插件 | `cebe5ccf3a4200396ca284cbb031b1d5e8c18dda2f855d9c7f5ae16d376e0d22` |
| 隔离 V5 / 658 | `db15757038c6c333e386499ef5be5480a5d279215b34020e403a92e0ba607297` |
| 原 Phase5 Handoff ZIP | `0e90c4fb56fb2d2ef75ed423ea17cd7a10c91c4f069fa5c3e1d8e66716f246d5` |

本轮开始及冻结时分别核对二进制、Factory、原交付 ZIP 和生成版本头。原交付 ZIP 为 52,658,174 字节。本轮增补包的独立 SHA 文件和 `SHA256SUMS.txt` 记录新材料身份；不在本报告内循环嵌入自身 ZIP 哈希。

有效历史门禁继续复用：三个单模式各 CTest 33/33、VST3 Validator 47/47、V4 专项 13,048、隔离候选专项 41,105；Phase5 专项 8,361；默认 OFF 首写竞争 barrier 20/20、隔离安装 21/21。1440 基准矩阵保持原哈希。详见 [Phase5 综合报告](PHASE5_COMPREHENSIVE_20261013.md)。这些不是本轮新跑的结果。

## R02：最小差异已闭环

新实测：Auto / C major / A minor × 4/4/4QN / 3/5/4QN × Closed / OPEN × HF OFF / ON，共 **24 格**；固定追踪五个 Factory ID，共 **120 行**。P3、Abstract 均 OFF，4/4，保留 minimumMatch 0.60、minimumScore 60 和每组 worker 上限 10。

| 调性设置 | HF | 时值 | Closed Color | OPEN Color |
|---|---|---|---:|---:|
| Auto | OFF | 4/4/4 | 0 | 0 |
| Auto | OFF | 3/5/4 | 0 | 5 |
| Auto | ON | 4/4/4 | 5 | 5 |
| Auto | ON | 3/5/4 | 5 | 5 |
| C major | OFF/ON | 两种 | 0 | 0 |
| A minor | OFF/ON | 两种 | 0 | 0 |

因此，在本次输入与冻结 V4 中，五条路线通过条件为 **Auto 且〔HF ON，或 3/5QN 与 OPEN 同时成立〕**。时值单改、OPEN 单改都不足以复现旧的成功条件。

Auto 的分析选中 A minor，但五个目标模板的最佳匹配解释均为 **F major**。G 是该解释下的未知功能和弦，基础结构权重 0.66 减未知功能 0.12 得 0.54。Legacy 等时 G 从 QN4 小节首开始，加 0.05 得 0.59；3/5/4 Closed 的 G 从 QN3 开始，时值调整后为 0.578631；3/5/4 OPEN 移除末 Am 时值，剩余 [3,5] 取上中位数 5，使 G 为 0.54。HF 检索不使用这些时值/小节加权，均为 0.54。

0.55 的骨架阈值将前两种视为 StructuralInsertion，将 0.54 视为 EmbellishingInsertion。单次匹配插入 cost 分别为 0.609600、0.604598、0.211536；完整 raw cost 分别为 0.809600、0.804598、0.411536。续写阶段仍另有未解释插入扣分，不能把它与匹配 cost 混为一项。

等时 Closed/HF OFF 的五条相似度约 0.683139，ranking 52.9025、eligibility 51.6767，均在最低分淘汰；HF ON 相似度约 0.925632，两条分数轨均超过 60。HF 同时改变 gate 使用轨道，但这五条差异不能只归因于 gate 切换。

| Factory ID | 有效完整后缀（F major 匹配） | 成功时 Color rank | 默认卡 | HF ON ranking / eligibility |
|---|---|---:|---|---|
| V3_SHADE_002 | Bb → Bbm → F | 1 | 是 | 66.9715 / 66.9690 |
| V3_SHADE_019 | Db → C → F | 2 | 是 | 66.9715 / 66.9690 |
| V3_SHADE_024 | Bbm → C → F | 3 | 是 | 66.9715 / 66.9690 |
| V3_SHADE_031 | Eb → Bb → F | 4 | 更多 | 65.7215 / 65.6103 |
| V3_SHADE_008 | Dm → Bbm → F | 5 | 更多 | 64.4715 / 64.2516 |

固定 C major：五目标相似度仅约 0.246–0.411，在 minimum_match 淘汰，后续分数 -1 表示未计算。固定 A minor：这些 Major 模板在模式过滤时退出，尚未对齐，不能把它写成低相似度拒绝。

详证：增补包 `diagnostics/r02/findings.md`、`r02-cases.tsv`、`r02-targets.tsv`、`r02-trace.txt`，包含每格实际调性、身份、结构权重、轨迹、两条分数轨、淘汰阶段、后缀和默认展示。五条 worker 候选不等于五张默认可见卡。没有证据证明此处为核心和弦性质缺陷，保持算法和阈值；**真实 Cubase 根因 UNKNOWN**。

## Abstract 音乐评审材料

固定 C major / Closed，使用冻结 V4、现有 core 推荐路径和 `presentationIndices` 计算默认展示；不是新线程 worker 或实装宿主验收。

| 输入 | Abstract | Factory ID | 相似度 / 总分 | RESOLVE rank / 默认Top3 | 完整续写 |
|---|---|---|---|---|---|
| C4–E7 4–Am4 | OFF | V3_HOME_011 | 0.719154 / 67.4548 | 10 / 否 | Em4–Dm4–G4–C4 |
| 同上 | ON | V4_B14 | 0.864459 / 74.2345 | 5 / 否 | Ab13 4–G13 4–Cmaj9 4 |
| C4–Am2–A7 1–Dm2–G2 | OFF | V3_HOME_018 | 0.767889 / 62.4213 | 3 / 是 | Am2–F2–G2–C2 |
| 同上 | ON | V4_B11 | 0.785012 / 68.7132 | 3 / 是 | Cmaj9 4 |

第一例两张目标都不在默认 Top3，不能称为默认主卡替换；原 V3_HOME_011 的进出轨迹见全 Top10 表。第二例 ON 时旧 V3_HOME_018 退到 rank4，而 V4_B11 从 OFF rank4 升到 ON rank3；这是本次实际默认主卡替换。新增方向性 AbstractExtensionCompatibility 影响匹配和排序；保留原评分，不用自动分数代替听感。

`blind/` 提供 Q1/Q2 各 A/B、完整输入加续写/仅续写，共 **8 WAV + 8 MIDI**，答案独立放在 `answer-key/`，先听后揭示。WAV 为同一 PreviewSynth、120 BPM、48kHz PCM16；完整文件从新合成器状态渲染，仅续写文件从完整渲染边界截取，保留原声部上下文和自然尾音。每对同范围向较安静者 RMS 做单一线性衰减，没有增益放大或动态处理。长度不同源于实际续写不同，不强行截短。

MIDI 使用同一 Preview 声部和完整上下文，仅续写起点归零，upper/bass velocity 80/90、channel 0，不写 Program Change；在宿主试听必须选同一音色和音量。解码 SMF 后逐事件核对全部音高集合及起止，而非只检查文件存在或最低音。PreviewSynth WAV 不使用 MIDI velocity，也不是 GM 录音或作者演奏。

ID/轨迹/完整路径/排名在 `diagnostics/music/`；会揭示答案的电平、MIDI和卡片映射另放 `answer-key/`。未证明核心和弦性质错误，未改生产代码；**材料 READY，音乐采用判断未完成**。

## R01 / V5 隔离候选

历史 V4 完整路线缺失审计保留在原 Phase5 包。新隔离运行固定 A minor / OPEN / 4QN，输入 Am–G#dim–G–D/F#；V5 的 `CANDIDATE_V5_CHROMATIC_MINOR_001` 在 DEVELOP rank1、默认可见，实际续写 **F–G–Am–C**，与存储路线一致。

完整规则化 Preview/MIDI 的最低音为 **45,44,43,42,41,43,45,48**，即 A–G#–G–F#–F–G–A–C；前五音下降连接真实成立。只有 D/F# 声明 root-relative bassInterval=4，其他七个事件为 null；规则渲染为根音低音不代表原作者明确声明。typed G#dim 为 degree7、alteration+1、Diminished、function Unknown、Embellishing role 0x2，未标 Passing role；记录供音乐判断，未擅改候选。

`diagnostics/music/r01/` 含 typed 事件、路线、Top3、解码最低音、完整及后缀 MIDI、完整 WAV。**技术验证完成，音乐衔接仍待人试听，整体 PARTIAL**。V5 未启用、未修改，不扩充 Factory。

## Feature Flag 支持边界

位序 HF / P3 / Abstract；三个开关默认均 OFF。

| 组合 | 本轮范围 |
|---|---|
| 000 | 默认包；用户集中验收 |
| 100 | 既有 HF-ON 单实验包；用户集中验收 |
| 001 | 既有 ABSTRACT-ON 单实验包；用户集中验收 |
| 010、110、011、101、111 | 仅 core 实验，无对应交付插件包/UI；禁止宣称为已支持的用户配置 |

已有 32 行矩阵实际覆盖 V4/V5 × 显式 D/F#/未指定 bass × 八组合，另有一个 2QN A08 基线。固定 A minor R01 输入中，V4_A08 在 HF=P3=0 时不 eligible/presented，任一为1时 eligible/presented/worker Top3/Top10 均为1；Abstract 单开不改变它。隔离 V5 候选八组合四项均为1，V4库不存在它。两种查询 bass 没有被错误当作同一声明，但该样例结果相同。覆盖表不是完整负例/联合音乐安全证明，因此总体 PARTIAL。

Phase3A 只退还一次该运行实际扣除的未解释经过减和弦插入罚分，不另给 bonus；已解释插入不退款。历史 R01/V4_A08 实测扣8退8，57.4039→65.4039；已解释 F#dim7 实测零扣零退。HF 与 P3 效果可叠加，不能泛称“无重复收益”。没有发现必须禁用的确定 core 崩溃组合；尚未充分验证的组合不进入产品验收范围。

分析前诊断 PENDING，分析后核对实际权重状态。开关不写入 Session：同一二进制重开使用同包默认；换包后使用新包默认，工程不会恢复原包算法开关。复现必须记录包哈希与实际诊断。详见 `safety/phase51-userlibrary-flag-safety.md`。

## User Library 与必要回归

保留既有 BEGIN IMMEDIATE 修复。本轮在临时测试 DB 的 metadata 插入处注入失败，确认事务回滚后 metadata 为空、未留下部分 progression_data 表；移除测试 trigger 后同库重试成功，新 UserLibrary 实例重新打开读回 Schema2、正确 ID 和条数。测试不读写真实 user.db。

新增 `LibraryV3_user` **283 checks PASS**。首次竞争的独立连接 barrier20/20、旧库重开及升级/卸载保留证据复用；安装逻辑未改，不重复安装。UserLibrary 多进程竞争 **本轮 NOT RUN**；不将同进程多连接证据冒称跨进程全面验证，也不为此新建进程测试框架。

实际代码变动仅 `tests/Phase4DTests.cpp` 的可选 phase51 诊断/MIDI/试听入口，及 `tests/LibraryCompatibilityTests.cpp` 的初始化失败回滚夹具；其他是文档。新统一诊断 **713 checks PASS = R02 200 + music 513**。开发中一次测试辅助代码悬空引用导致退出，已改为持有续写副本；复核时修正 suffix WAV 的声部上下文，并加强解码音高核对。失败、重试及最终日志全部保留。生产代码、CMake、数据未变，因此复用同一 BuildCommit 的全量门禁，不重新构建插件或重复完整 CTest/Validator。

## 历史核销与十二项交付

| # | 交付/遗留 | 本轮结论与入口 |
|---:|---|---|
| 1 | Phase5 历史核销 | R02数量报告已换成正交追踪；两张 RESOLVE 卡评审材料齐备；V5低音补实证；开关支持范围与工程重开边界明确。原 dev.5 十项7DONE/3PARTIAL保持历史口径。 |
| 2 | R02最小差异 | 24格/120行和完整轨迹，`diagnostics/r02/`，离线CONFIRMED。 |
| 3 | Abstract主卡音乐评审 | `blind/`与独立`answer-key/`；排名、完整续写见`diagnostics/music/`，人工判断未代办。 |
| 4 | R01/V5召回低音MIDI | `diagnostics/music/r01/`；技术通过，音乐待听，不采纳V5。 |
| 5 | 组合安全 | 本报告及`safety/`；联合仅实验，PARTIAL。 |
| 6 | 用户库兼容 | 283检查与回滚patch；复用barrier20、安装21，跨进程未测。 |
| 7 | 代码及回归 | 两份测试patch、最终713日志及用户库283日志；生产门禁复用。 |
| 8 | Build/Git/哈希 | `proofs/`、外部ZIP SHA及包内`SHA256SUMS.txt`。 |
| 9 | 中文Cubase清单 | [集中验收清单](PHASE51_CUBASE_ACCEPTANCE.md)。 |
| 10 | 未解决事项 | 真实Cubase根因、两例听感、V5音乐设计、联合负例与真实工程重开待人工；不扩展开发。 |
| 11 | 工作量 | 下述实际计时、原始日志；总工时未记录。 |
| 12 | 增量及回退 | 唯一Acceptance Supplement，沿用原dev.6包；下述回退方法。 |

## 耗时与交付/回退

确有日志的本轮时长：UserLibrary 定向编译 **6.08秒**，定向CTest墙钟 **0.976秒**（CTest自身0.61秒）；最后一次 Phase4DTests 增量编译 **15.955秒**，最终713诊断运行 **2.559秒**。先前编译/失败/重试日志也保留，不将最后成功耗时当成全部成本。打包耗时在交付目录 `packaging-final.log`；历史 OFF 单模式全量CTest的74.24秒、barrier的64.77秒不计入本轮。总任务起止、阅读/编码/协作各项时间未单独记录，**总工时未记录，不推算**。

主要成本来源是复用证据核对、24格逐候选追踪、试听/MIDI辅助代码及其修正、报告整合和跨设备交付。没有重复大型回归、容量扩展、Factory扩充或重制已有ZIP。

唯一新包 `HarmonyContinuation-Phase5.1-Acceptance-Supplement.zip` 位于同级 `HarmonyContinuation-phase51-delivery-20261014/`；只含新诊断、试听、日志、文档、proof和测试patch。原始材料保留在同级 `HarmonyContinuation-phase51-validation-20261014/`。基线插件/Setup/Factory继续从原 Phase5 Handoff 取得，不重复打包。GitHub沿用既有未发布草稿，只添加新增补ZIP和独立SHA；实际上传读回以交付目录proof为准。

本轮没有新插件需要安装或数据库迁移。验收换包前关闭Cubase，备份旧完整bundle、用户库和工程；三个单模式每次只扫描一个。回退时恢复原DEFAULT-OFF完整bundle并核对 dev.6/3a582e4及哈希，不删除用户库、不切换到V5、不重置工程。测试/文档提交的回退可在开发分支审阅后revert，不改生产库或main。集中真实验收完成前，保持正式发布NO。
