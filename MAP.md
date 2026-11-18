# 有效路径与同步边界

## 跨设备约定

`<checkout>` 表示已通过 `git rev-parse --show-toplevel`、`git status -sb`、`git log -1` 核实的工作目录。目录同名不代表同版本。移动到另一设备时先核对 Git 远端、分支、HEAD 与未提交差异，再操作；构建缓存不得跨 checkout 混用。

| 对象 | 有效映射 | 边界 |
| --- | --- | --- |
| V9-B 工作树与实验库 | `experiment/factory-v9-b-20261115`，起点 V9-A `b451ce118d88918f47317519369b039c1ca005e7`；同级 `HarmonyContinuation-v9-b-evidence/library/` | `factory.db` 纯素材，`factory-functions.db` 仅对新增项补功能；两者均为 Library 9 / Schema 2 / 1,332 条 |
| V9-A 工作树与实验库 | `experiment/factory-v9-a-20261114`，起点 B7 `3b58159`；同级 `HarmonyContinuation-v9-a-evidence/library/factory.db` | Schema 2 / Library 9 / 1,312 条；工具从正式 V8 增量构建。当前 HEAD 用 Git 核对 |
| B7 实验工作树 | `HarmonyContinuation-b7-factory-coverage-20261114/` → `experiment/b7-factory-coverage-20261114`，起点 B6 `f95f2af37a915b1f93019b342db96f3f883e80e2` | `git log -1` 查当前 HEAD，正式源码身份为 v0.11.0 标签 |
| B7 回放与素材证据 | 同级 `HarmonyContinuation-b7-evidence/`；`tools/b7/` 复用 B5 冻结数据、B6 适配配置与 Release 回放 | [B7 报告](docs/B7_FACTORY_COVERAGE.md) 的 V9 规格为方案，当前无 V9 数据 |
| B6 实验工作树 | `HarmonyContinuation-b6-retrieval-statistics-20261113/` → `experiment/b6-retrieval-statistics-20261113`，起点 B5 `bbd1f4da6b472cb845260df96f5aacfff72e7c58` | `git log -1` 核对实际实验 HEAD；正式 BuildCommit 仍按 v0.11.0 Tag |
| B6 回放与统计证据 | 同级 `HarmonyContinuation-b6-evidence/`；`tools/b6/` 使用 B5 冻结集和原统计模型 | 回放程序和输出仅用于离线研究。B6 未生成新模型权重，`reliability/selection.json` 固定适配配置 |
| B5 实验工作树 | `HarmonyContinuation-b5-corpus-statistics-20261113/` → `experiment/b5-corpus-statistics-20261113`，基于正式 `d888e2c8d5f78a19a20a7936e763d407100b6a32` | `git log -1` 核对实际实验 HEAD。正式身份另见 v0.11.0 Tag 和 [发布说明](docs/V0_11_RELEASE_NOTES.md) |
| B5 语料、划分、模型和离线证据 | 工作树同级 `HarmonyContinuation-b5-evidence/`，可迁移到任意目录；`sources/` 按 [语料清单](docs/B5_CORPORA.md) 恢复，`benchmark/freeze.json` 标记 `b5-v1-20261113` 及固定划分哈希 | `benchmark/selection.json` 记录 `b5-ngram-v1` 选型与模型哈希。训练源仅 Billboard CC0，MPB 仅外测；模型与数据不随代码提交同步 |
| B5 Factory 回放 | 取 v0.11.0 交付目录 `stage-8/factory.db` 与同目录 `factory.hca`；发布包文件身份见同级 `HarmonyContinuation-v011-delivery-20261110/BUILD_INFO.json` | `factory-replay/` 保留候选、原序和离线评分证据。复现见 [RUNBOOK](RUNBOOK.md#b5-真实音乐续接统计) |
| G13-C产品集成 | `HarmonyContinuation-g13/` → `development/g13c-product-integration-20261109`，代码提交`7b4000e` | dev.16测试构建，默认仅CC0先验；[技术报告](docs/G13C_PRODUCT_INTEGRATION.md) |
| G13-C可运行测试包 | 同级 `HarmonyContinuation-g13c-test-20261110/`，Demo、VST3、examples、BUILD_INFO、宿主清单 | 未覆盖生产安装、无Setup；真实宿主待验收 |
| G13-C检查与实际显示 | 同级 `HarmonyContinuation-g13c-evidence/` | 414/606/75、原生UI聚焦PASS、真实列表/5组媒体/快照；失败诊断日志保留 |
| G13.1当前实现与交付身份 | 已保存commit `b1b855d`，从基线`26783c7a`接续 | [技术报告](docs/G131_PHRASE_GRAMMAR_REVIEW.md)；同级 `HarmonyContinuation-g131-evidence/identity.json`记录源码/CLI/先验哈希 |
| G13.1试听与运行 | 同级 `G13_1_GENERATED_LISTENING/`；CLI、研究先验、许可、索引、三档JSON/WAV/MIDI | 仅F–G–Em–Am，各档首选；独立非商业实验，三条音乐已获用户正面评价 |
| G13.1语料研究与检查 | 同级 `HarmonyContinuation-g131-corpus/`、`HarmonyContinuation-g131-evidence/` | Idol/RWC资料与CC0聚合分离；原始/派生研究材料未公开分发，跨设备须单独迁移 |
| G13独立生成实验 | `HarmonyContinuation-g13/` → `development/g13-relation-generation-20261109`，基于dev.14交付`416a7e3a` | 仅实验CLI，代码提交与最终状态见[集中报告](docs/G13_RELATION_GENERATION_REVIEW.md)，没有新产品Setup |
| G13试听与运行入口 | 同级 `G13_GENERATED_LISTENING/`；`phrase_generate.exe`、`LISTENING_INDEX.md`、各组INPUT/JSON/WAV/MIDI | 旧六条完整D+E；用户认可多样性但认为偏实验性，当前改进见G13.1 |
| G13检查及身份 | 同级 `HarmonyContinuation-g13-evidence/`；`identity.json`、`generated-cases.tsv`、`media-verification.json`、`database-comparison.tsv` | 只覆盖五固定输入、必要约束与六组实际导出，不重复RCR |
| dev.15研究成果 | `HarmonyContinuation-dev15/` → `development/dev15-chord-map-20261109`，HEAD `33db6f5…` | 新排序保持关闭；G13复用核心连接/轮廓证据，原工作树保留 |
| dev.14 安装交付线 | `HarmonyContinuation-dev14/` → `development/dev14-delivery-20261106`，基于 `dd6703c8`，BuildCommit `eb05cb2…` | G12 音乐研发结项，Factory V8 / 1,288 条，独立 Release x64 |
| dev.14 Setup、SHA-256 与证据 | 同级 `HarmonyContinuation-dev14-delivery-20261106/`，`BUILD_INFO.json` 对应最终身份，`evidence/` 保留隔离 V7→V8 记录 | 使用顶层统一 Setup，[实机清单与回退](docs/DEV14_DAW_ACCEPTANCE.md)，用户已确认dev.14验收完成，G13不重复宿主验收 |
| G12乐句衔接与现代和声 | `HarmonyContinuation-g12/` → `development/g12-phrase-flow-20261104`，基于G11交付`797c15f`；BuildCommit `499c69b3…` | dev.13/Factory8/1288，新增0；[集中报告](docs/G12_PHRASE_FLOW_REVIEW.md)，工程与24% RCR通过，12条扩展音乐试听已获认可 |
| G12定向证据与试听 | 同级 `HarmonyContinuation-g12-evidence/`；`identity.json`为构建/交付身份，`boundary-auditions/`为原完整D与合格E，`rcr-3/representative-paths/`为正常三轮新链 | 一次原冻结50复核；G11树与旧包保持，不制作Setup，真实DAW未验收 |
| G11连续路径分叉 | `HarmonyContinuation-g11/` → `development/g11-continuation-network-20261103`，基于G10交付`fa2de2f`；产品/RCR BuildCommit `c1b3743b…` | dev.12/Factory8/1288，新增0；[集中报告与试听](docs/G11_CONTINUATION_NETWORK_REVIEW.md)，工程与24% RCR通过，末轮集中仍在 |
| G11定向证据与媒体 | 同级 `HarmonyContinuation-g11-evidence/`；`identity.json`记录构建/最终HEAD与校验，`rcr-3/representative-paths/`为三条首选连续链 | 一次原冻结50复核，三代表各两条首页B实际回导分叉；G10基线保留，不制作Setup，人工音乐与DAW待验收 |
| G10语境多样性与创作覆盖 | `HarmonyContinuation-g10/` → `development/g10-contextual-diversity-20261103`，基于G9交付`921b7f3`；产品/RCR BuildCommit `80487ab5…` | dev.11/Factory8/1288，新增0；[集中报告](docs/G10_CONTEXTUAL_DIVERSITY_REVIEW.md)，工程与RCR通过，持续创作覆盖仅部分达到 |
| G10定向证据与试听 | 同级 `HarmonyContinuation-g10-evidence/`；`identity.json`记录最终构建及交付HEAD，`rcr-3/representative-paths/`为三条三轮材料 | 一次原冻结50起点复核；保留G9基线，不制作Setup，音乐与真实DAW待验收 |
| G9乐句连接与历史多样性 | `HarmonyContinuation-g9/` → `development/g9-phrase-diversity-20261102`，基于G8交付`3fedd827`；产品BuildCommit `f105a0b4…` | dev.10/Factory8/1288；[根因、结构门禁与试听](docs/G9_PHRASE_DIVERSITY_REVIEW.md)，保留G7/G8及生产安装 |
| G9定向验证与媒体 | 同级 `HarmonyContinuation-g9-evidence/` | 结构反例、原三链、长输入及原冻结50起点复核；音乐与DAW验收单独记录；不制作Setup |
| G8完整开放与递归续写 | `HarmonyContinuation-g8/` → `development/g8-open-recursive-20261027`，基于G7交付`7a7021e`；产品/RCR BuildCommit `0936a483…` | dev.9/Factory8/1288；[历史专题](docs/G8_OPEN_RECURSIVE_REVIEW.md)，原RCR46/50；用户音乐缺陷反馈与整改见[G9](docs/G9_PHRASE_DIVERSITY_REVIEW.md)，G7基线保留 |
| G8必要证据与代表媒体 | 同级 `HarmonyContinuation-g8-evidence/` | 定向检查及唯一一批50起点RCR，音乐/DAW与工程结果分别记录；不制作Setup |
| G7四入口整合 | `HarmonyContinuation-g7/` → `development/g7-four-entry-20261027`；实现/BuildCommit `58dfce0f…` | dev.8/Factory7；[集中验收](docs/G7_FOUR_ENTRY_REVIEW.md)，保留G6工作树与产物 |
| G7工程与UI证据 | 同级 `HarmonyContinuation-g7-evidence/`；首页/截图在`ui/`，身份`identity.json` | 固定一输入/一尺寸的原生UI及真实worker；工程通过、人工音乐待验收，未制作Setup |
| G6算法线 | `HarmonyContinuation-g6/` → `development/g6-harmony-engine-v2-20261027`；实现/BuildCommit `03fad728…` | dev.7/Factory7；[算法与音乐门禁](docs/G6_ENGINE_V2_REVIEW.md)，独立源码与构建，不安装 |
| G6代表媒体及必要证据 | 同级 `HarmonyContinuation-g6-evidence/final-check/`；身份 `HarmonyContinuation-g6-evidence/identity.json` | 三输入、13组样本；原dev.6产物及真实用户库保持 |
| Factory V7-I集成线 | `HarmonyContinuation-factory-v7-integration/` → `development/factory-v7-integration-20261026`；内容父提交`8cc16c1`，BuildCommit`40210d3` | dev.6/Factory7；[集成交付](docs/FACTORY_V7_REVIEW.md#v7-i-独立集成与开发交付2026-10-26)，不改原dev.5/V6工作树 |
| V7-I开发包与验证 | 同级`HarmonyContinuation-v7i-delivery-20261026/`；原始证据`HarmonyContinuation-v7i-validation-20261026/` | 正常Setup未执行；独立TestRoot安装验证；四组媒体与SHA保留，真实DAW待验收 |
| Factory V7 内容线 | `HarmonyContinuation-factory-v7/` → `development/factory-v7-20261025`；从G5交付`24bdc239`建立 | [V7门禁](docs/FACTORY_V7_REVIEW.md)；独立内容与工具，不安装/激活 |
| V7冻结基线与产物 | G5输入为同级`HarmonyContinuation-g5-delivery-20261025/stage-6/`；V7输出为同级`HarmonyContinuation-v7-evidence/engineering/` | G5/dev.5/Factory6保留；V7实验DB与配套HCA只供隔离集成 |
| 远端仓库 | `https://github.com/OldLai-ctrl/HarmonyContinuation.git` | 2026-11-13 核对 `main` 为 `d3552c2`、Tag `v0.11.0` 指向正式构建提交；B5 分支留在本地 |
| 原开发 checkout | `HarmonyContinuation-release/`，本轮读取时为 `development/g01-g4u-experience-20261024`、HEAD `b1809b2` | 已有未提交文档保留于原工作树；B5 使用独立实验目录 |
| Factory V6 工作包B | `HarmonyContinuation-factory-v6/` → `development/factory-v6-b4-20261025`；B3基线分支保留在`aa42b56` | dev.4/5ae1b5a与V6配套；[验收](docs/FACTORY_V6_REVIEW.md)，未改生产V5或合入A UI |
| V6 数据/验证/听评产物 | 同级 `HarmonyContinuation-v6-b3r-evidence/engineering/r3/`；790与810旧产物保留 | Library6/Schema2，proposal1032/experiment1014；HCA格式1配套2MiB读取器；B3历史内容证据保留，B4开发包与隔离安装证据在同级`HarmonyContinuation-v6-b4-delivery-20261025/`，生产未激活V6 |
| 当前验证与测试包 | 同级 `HarmonyContinuation-v010-validation-20261023/`、`HarmonyContinuation-v010-delivery-20261023/` | 正式 v0.10.0 构建、门禁日志与唯一Setup；RC1/dev.15目录保留 |
| Phase 7 UI | `design/phase7/` 与正式 `src/ui/` | 已集成至main；原型和正式UI临时工作树已撤除，来源历史仍可追溯 |
| 仓库精简恢复资料 | 同级 `_archive/HarmonyContinuation-repo-cleanup-20261023-180326/` | 全部原引用的Git bundle、原工作区文档ZIP及补丁；仅用于恢复，不在备份内开发 |
| 旧目录归档 | 同级 `_archive/HarmonyContinuation-20261008/` | 保留三个旧目录全部文件；仅用于恢复。归档入口记录本机恢复条件，不在归档内开发 |
| SDK 配置 | `VST3_SDK_ROOT` 环境变量或 CMake 缓存项 | 完整离线 SDK；各设备实际路径待核实，不提交 SDK |
| 工具配置 | 本机开发终端的 CMake / MSVC / Ninja，打包脚本的 `Iscc` / `RuntimeDirectory` | 实际版本与路径按操作前置条件核实 |
| 正式构建与打包输出 | 同级 `HarmonyContinuation-v010-delivery-20261023/` 与 GitHub Release；临时发布工作树已撤除 | `v0.10.0` Tag、正式源码、About BuildCommit、moduleinfo及Setup固定于 `c8d8dc3`；main后续文档提交不改变已发布身份 |
| V4 开发数据与产物 | `data/factory-v4/` → `build-v4-dev/factory.db`；开发统一包 `build-installer/v4-0.10.0-dev.1/`，内含 `stage-4/`、`midi/`、`evidence/` | 仅匹配 0.10.0-dev.1+ 引擎；统一 Setup 管理版本化 Factory/4，保留历史；见 [产品化说明](docs/FACTORY_V4_PRODUCTIZATION.md) |

设备具体路径在运行时核实；关系改变时替换失效行，不追加长期流水账。功能版本与来源提交以 [NOW.md](NOW.md#功能基线) 为文档入口，实时状态仍需核对 Git。

## V9-B 外部证据身份

外部证据根可迁移，未同步。每组的四路 Worker 输出为 `rock-{v8,v9a,v9b,v9b-functions}/`、`jazz-{v8,v9a,v9b,v9b-functions}/`。正控与硬约束在 `controls-v9b*/`、`hard-both-v9b-functions/`。另存 `diagnosis_projection.cpp`、相应构建脚本、`diagnosis_projection_replay/` 及 `diagnosis_projection_development/`，这些是重投影 V9-A 的开发对照。研判不使用其中的库作为新基线。原语料与作品窗口按各自许可保存。模型继续沿用 B5/B6 下列身份。跨设备步骤见 [RUNBOOK](RUNBOOK.md#factory-v9-b-建库与独立回放)。

| 文件 | SHA-256 |
| --- | --- |
| `library/factory.db` | `78e8f8ae16516f42a6ce98ea944ac94206f1c81854106be057dd348e15c306f9` |
| `library/factory-functions.db` | `08252b9bc8a3ba37fbf562095df08c5ab088165841a38103ebd2ac69ca580bf7` |
| `materials/candidates.json` | `2ca77f157e269a330b2a07fd9bd7af5e312d144a4ae202a704240197ffd59feb` |
| `materials/manifest.json` | `113dc9693e43dfc6d4d02397ff96dc872a0d72e4304940265e52933c0f667aa9` |
| `materials/controls.jsonl` | `65ef73c2a6c8340911102e72d939943be3b982ca42a41db7f77053a86635cfe8` |
| `materials/hard-control-both.jsonl` | `cc37ad55a8c4cbef9281e4b8f7e173afb231bcafb779796327a6f5b78da3560a` |
| `diagnosis.json` | `0b2e2dc27bcb39bc8a6e114f1f6f8a33566164dfdfd833df00ebd456d36bfe60` |
| `diagnosis_projection_summary.json` | `bf9662b5ef4400d1c3c96a8b1c186e22c48068b04aae833bba7f65cf3274c4bd` |
| `confirmation/rock-freeze.json` | `8aa7696e6248501a22ab7863d817bc03920996cbf272400ed70cb06c76ea0662` |
| `confirmation/jazz-freeze.json` | `0b512689caf6a703676bff420c34237a536ffb2bf6622ce0278deaf87b4caebd` |
| `confirmation/sources/wjazzd-range.db` | `af6a0d9debf042c3581565bd75baad591d32e166cd2ec7298519883de614bf12` |
| `evaluation.json` | `564e0c4cda3a4bc3f772860a70777d19de0f94b154b42ae8682e33043bc4a9bc` |

## V9-A 外部证据身份

证据根为 `HarmonyContinuation-v9-a-evidence/`，可迁移到任意目录，尚未同步。定点检查在 `controls-v9/`、`controls-v9-hard/`，两库独立回放在 `confirmation-v8/`、`confirmation-v9/`，路线观测在 `confirmation-v9-trace/`。源和弦标注及 A2 研究 CSV 属于研究资料，权利按 [B5 清单](docs/B5_CORPORA.md) 保留。实验库为独立原创增量，不作为正式分发包。源代码内 [素材表](data/factory-v9-a/phrases.tsv) 维护人工编辑数据，生成文件和哈希在本段。模型未改动，沿用 B5 的句序模型和 B6 适配配置。

| 文件 | SHA-256 |
| --- | --- |
| `library/factory.db` | `377abf120a153a1d301290d7af7614ccb87e8ce88e766a6892bf1a164cfe6b96` |
| `materials/candidates.json` | `96dba1aa29ce6217c2f1d16e7e317aa2dc68e2389ce38aa8b8298d9f9855b6b7` |
| `materials/manifest.json` | `5fe91d667a74ede854306b26846f8f7553d06c71a7bcbde560bc58ecd878a180` |
| `materials/hard-melody.jsonl` | `34cf2c672b43c2be4dfff343f16823f2ea579284041f5879cb112b5c0a3814ef` |
| `source-review.json` | `64bf3cc9411549c25254266bda78806c5dbda141a09e3fbaabaa58add375ebab` |
| `sources/a2-candidate-v8-comparison.csv` | `5e84080e1fbd16b42d6b2402e88ebcaaaa46f405ba1004e178804f86d86c758c` |
| `sources/rs200_harmony_exp.zip` | `7054ae6599280732bfba2ce2d531b93ca06274004380c2549756e468a8793fee` |
| `sources/rs200.txt` | `7882b29b420e70b7493c66d593239f62a391ac62de84326af25e8ffdff8a1f3c` |
| `confirmation/freeze.json` | `03a048fd87a6db40dbc6f2f2dd29bde1018fad057992ad8f367d0b01e42df6ac` |
| `confirmation/confirmation.jsonl` | `d5472a10a839ae39c8f55248eadfd58d8cd8ec4523d2bd61e79d0d08f5a87959` |
| `evaluation.json` | `df81aa61fa292c1bbec2c830ff24cece19d45a48c539ef69c66c9d864496f841` |

## B7 外部证据身份

路径相对 B7 证据根，可迁移。保留 `cohorts-final/`、`cohorts-replication/`、`replay/`、`replication/`、`trace-replay/`、`content/` 和 `time-audit/`。只需迁移证据或按 [RUNBOOK](RUNBOOK.md#b7-factory-覆盖与时值) 重建。B5/B6 模型及适配配置沿用下列原哈希。B7 没有新模型、插件或 Factory 制品，外部资料尚未同步。

| 文件 | SHA-256 |
| --- | --- |
| `cohorts-final/cohort.json` | `0cf062e7655789f8523fb0f5b9abd4abd2ba4d88ac6f931ecb9fb21990318c71` |
| `cohorts-replication/cohort.json` | `cc2699cfd6f393945516cf28fc7d14062daac773f49aadc23e68e68d954e3f34` |
| `timing-results.json` | `c363bf66a00e2303171b0fc38e41774e4d0498f03c9d9f75568767109fd5ffbb` |
| `replication-results.json` | `65cc30b09838c34d5f75886933d103e34420244458a0e5e13cf78558c01257b8` |
| `trace-inputs.jsonl` | `93ca5eee3c38d63bb3115cc1baf56a6e2f52f25f311e41f191918f3868e69c8c` |
| `trace-summary.json` | `7491a47855fe697ce61504a4fb7a2c267160c8116a5b6458d2d04ae63d4265fe` |
| `content/summary.json` | `185cd48cdb285a20ee642e15b237f1ce0acc4f5e517320e439abe8689d25df7c` |
| `content/source_rows.jsonl` | `f03a6d405a800aae891bc3c95a990a5facfdf16444a165131178656b809c9ca2` |
| `time-audit/timing_audit.py` | `b73be32bf8c391de216dfd6ebef3d0c54219b40bbb81eff82e00ec19f8b99b4b` |
| `time-audit/timing_audit.json` | `e8747319d1de52d893b85972ff3e1fea9d41dc5f42a2c26fedbc14aefae53ade` |

## B6 外部证据身份

外部目录尚未同步。可迁移 `cohorts/`、`diagnostic-off/`、`development-{off,on}/`、`holdout-{off,on}/`、`endpoint-probe/`、`reliability/`、`sample-986-hard.jsonl` 和 `sample-986-hard-on/` 及下表汇总，或用已冻结的 B5 输入按 [RUNBOOK](RUNBOOK.md#b6-factory-检索与统计适配) 复现。探针保留 `witness.py`、`run.cmd`、`EndpointProbe.cpp` 和 `ProbeMatcher.cpp`，用于定位特定来源落点。计时只对应本次顺序回放。

| 文件 | SHA-256 |
| --- | --- |
| `cohorts/confirmation.jsonl` | `582192a1f2b4fe8d826380df5ee16dfaead297a7d1e9bebc3d01f22bee2e51b2` |
| `diagnosis.json` | `e78a2b5e5768f19d6e3ca7979409505a50370a3e38258c7fc3ea46078981c7b9` |
| `endpoint-probe/summary.json` | `920f301a2fbd350c0034913828d34e7c056a5e497d791716432e3f08f05b820e` |
| `reliability/selection.json` | `309e6c4391fe0166cc5340df44b625afb4e479e7a99601ec46ddfad809db308e` |
| `reliability/test_metrics.json` | `03466556f7ee9c9da5034d63a15f9d14db4849ed2c0bd903fc2c37d27001e23d` |
| `development-comparison.json` | `9726220b159ae542684056d3addee57db056eb692819fe52320a79d491a6ac89` |
| `holdout-comparison.json` | `e0f5a36a2179b714c47e4cd0f82d6d38698b590ab49937b34b76a7f42c97848e` |

## B5 外部证据身份

以下路径相对 B5 证据根目录。源码及交接文档在本地实验分支，外部数据、模型和回放尚未同步。跨设备可迁移该目录，或从 [语料清单](docs/B5_CORPORA.md) 与正式 Factory 文件按 [RUNBOOK](RUNBOOK.md#b5-真实音乐续接统计) 重建。精确哈希针对本次保存的字节，重跑的性能字段会变化。

| 关键文件 | SHA-256 |
| --- | --- |
| `benchmark/freeze.json` | `89f0121b793a7021eac95df9b155be44ff63dc2ffd3568a039ea886a4c5723de` |
| `benchmark/selection.json` | `e84d7e146a62a11540b72cd29a380940875035185e30b5d2d2a7a5650e4e458d` |
| `benchmark/variable_anchor.json.gz` | `a52b3bb4e7c995cd12f24960850eea9dd93316c5099a6cd809181d9def750e7d` |
| `benchmark/test_metrics.json` | `63569c978abf6b09691a4ae38c54ea299debc61abd6aa1f0f02aa9567720cc44` |
| `factory-replay/factory_evaluation.json` | `627e08832800207aa89ea4ec915c8dcfca8c0a716681d5c243264ec231724fce` |

## 安装与持久数据

| 对象 | 位置 | 生命周期 |
| --- | --- | --- |
| 已安装插件 | `%CommonProgramFiles%\VST3\HarmonyContinuation.vst3` | 独立于源码，运行包验证后才确认版本 |
| Factory 历史及活动指针 | `%ProgramData%\HarmonyContinuation\Libraries\Factory\<版本>\factory.db`、`active.txt` | 保留历史；独立库验证后切换 |
| 用户进行 | `%LOCALAPPDATA%\HarmonyContinuation\user.db` | 每台设备本地数据，更新/卸载保留 |
| 插件备份 | `%ProgramData%\HarmonyContinuation\Backups\Plugins` | 保留文件，防止宿主重复扫描 |

## 同步

Git 同步代码、文档、固定测试输入、Factory 源 JSON 和安装脚本。未提交改动未经过 push 就不会到另一设备；接续前明确它们由谁保管。提交/推送由当前任务授权范围决定，更新 NOW 的待同步状态。

SDK、编译工具、缓存、生成数据库、Setup、ZIP、运行日志不经 Git 自动同步；测试包单独分发并带提交和 SHA-256。用户数据库、宿主工程与私有音乐素材不自动复制或上传，不用机器间的绝对路径替代仓库相对链接。

旧开发分支、阶段Tag及dev.1–dev.7交接用未发布草稿已按用户确认清理。历史源码从main提交历史或精简冷备份恢复；历史报告内的分支名和草稿下载地址只代表当时状态。现行分发入口为正式 [GitHub Release](https://github.com/OldLai-ctrl/HarmonyContinuation/releases/tag/v0.10.0)。

Phase5历史构建提交 `3a582e4`。同级 `HarmonyContinuation-phase5-delivery-20261013/` 的既有交付文件与 `HarmonyContinuation-phase5-validation-20261013/` 的证据未纳入本轮清理。旧交接操作见 [Phase5接手](RUNBOOK.md#phase5-dev6-另一设备接手)，其中远端草稿入口已撤除。

Phase5.1 保持插件构建身份 `3a582e4` / dev.6，增补范围仅测试与文档。验证资料位于同级 `HarmonyContinuation-phase51-validation-20261014/`；唯一 Acceptance Supplement 目标目录为同级 `HarmonyContinuation-phase51-delivery-20261014/`，不重打 Phase5 三模式包、Setup、Factory 或 Source ZIP。Supplement 顶层按现有打包器要求组织为 `proofs/`、`safety/`、`blind/`、`answer-key/`、`diagnostics/`、`docs/`；答案与 mapping 不进入 `blind/`。源码提交和远端资产读回分别以 Git proof、上传 proof 为准。详细范围见 [Phase 5.1 收尾报告](docs/PHASE51_CLOSEOUT_20261014.md) 与 [Cubase 清单](docs/PHASE51_CUBASE_ACCEPTANCE.md)。

Phase6 dev.7 使用一个 Setup、一个 VST3 插件和 Legacy / HarmonyFirst / Abstract 三种运行时模式；工程重开后回到 Legacy。Factory V4 内容未改，Phase3A OFF，无 Phase6 Candidate 预设。验证材料在同级 `HarmonyContinuation-phase6-validation-20261014/`，最终分析为其中 `benchmark/RESULTS_ANALYSIS.md`；交付目录为同级 `HarmonyContinuation-phase6-delivery-20261014/`，包与 SHA 以最终交付证明为准。V5 不安装，只附隔离评估 MIDI/证据。详细状态见 [Phase 6 综合报告](docs/PHASE6_ENGINEERING_20261014.md) 与 [Cubase 验收清单](docs/PHASE6_CUBASE_ACCEPTANCE.md)。

Phase6.1 dev.8 的构建与诊断为同级 `HarmonyContinuation-phase61-validation-20261016/`，交付为同级 `HarmonyContinuation-phase61-delivery-20261016/`；具体门禁、哈希和实际和弦听评见 [Phase6.1 报告](docs/PHASE61_ENGINEERING_20261016.md)。dev.7 目录和 Factory V4 原件保留作回退对照。
