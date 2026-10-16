# Phase 6.1：流行和声召回与推荐语义整改

## 结论与范围

本轮修复已有完整流行路线被排序隐藏、循环尾部多出一个起始和弦、缺少风格标签时精确匹配被额外扣分，以及色彩标签与实际后续不一致的问题。产品版本为 `0.10.0-dev.8`，生产源码提交为 `dd76b60046940ee7dee90ae9807f7ca5b3c2246a`，分支为 `development/phase61-20261016`。冻结后的完整回归、安装包与哈希见本文交付段。

保留 dev.7 / BuildCommit `8c27c27a0c50da7fb0cf991f60ecde66890116f0` 作为对照。Factory V4 / 657 条保持原内容，SHA-256 为 `74c529828f8c4b934bbaf4ada719aa95491ab498b58885522124ff6505671534`。未合并 main、未正式发布。Phase 7 位于独立工作树，本轮没有修改 MainView。

## 十二项集中答复

### 1. C–G–Am 为什么看不到 Em–F–G–C？

目标成功进入实际 V4 数据库，ID 仍为 `FUNCTIONAL_016`，别名 `1563451`、七个和弦的顺序及七个骨架节点均保留。实际路线是 **C–G–Am–Em–F–G–C**。编译、规范化和去重没有丢失它。

通过真实 RecommendationWorker，使用合成 Cubase 结构字段输入，执行 Auto/C major × Auto/Pop × 2QN/4QN × Closed/OPEN × Legacy/HarmonyFirst/Abstract，共 48 格。修复前输入均识别为 C 大三和弦、G 大三和弦、A 小三和弦；目标均匹配、均通过资格并保留到“更多”，默认可见为 **0/48**。Closed 相似度 `0.980842`，OPEN `0.981272`；FULL/SKELETON 都为 1，对齐代价为 0。Auto 分析面板有时选 A minor，但 matcher 保留 C major 解释，实际目标匹配 key 是 C major。

主要扣分来自旧多样性规则：它把 Em–F–G–C 与 Em–F–C–F–G 共享的两个开头和弦视为近似路径，扣 13 分。Auto 风格目标约 64.889 分，Auto key 下第 9、固定 C major 下第 7；Pop 下原条目只有 Functional/Jazz 标签，又从中性 0.5 降到 0.25，目标降至约 62.889 分，排名第 10/7。它不是无内容或无法检索，而是默认排名不足。

### 2. 根因属于哪一层？

主因在**排序及默认呈现**，风格元数据缺口进一步放大排序偏差。识别正确、预筛没有截断（657 小于上限 800）、匹配正确、必要门槛通过、完整 MIDI 后续可表达。默认 Top3 与“更多”分别记录，不能把不可默认见写成完全没有候选。

### 3. 其他 Pop 路线有相同问题吗？

常见 I–V–vi–IV、I–vi–IV–V、vi–IV–I–V、I–IV–vi–V 都有曲库内容。专项基准覆盖这些完整句、F–G–Am、转位、七和弦和切分输入，另覆盖 R01/R02 的 OPEN 变体。修复前 9 场景 × 3 调性 × 2 风格 × 3 模式为 162 格；最终增补 3 个 OPEN 场景，共 216 格。各模板的原始匹配、约束、评分淘汰、保留及默认位置都有表和完整轨迹。

同类问题包括：长短分支只因共享前两个和弦被扣分；完整循环被截成“返回一个和弦”；部分循环又多加开头和弦。**C–G–Am → F** 有内容；**C–G–Am → F–C–F–G** 的直接完整路线未在当前源条目中确认，已有相近路线为 **C–G–Am → Em–F–C–F–G**。不补造缺失内容。

### 4. 为什么偏向古典功能和声？

V4 的主标签分布为 Resolve 350、Loop 127、Develop 91、Color 89。约半数模板主意图为收束，很多路线使用属前—属—主。评分同时使用完整匹配、骨架、终止、先验和同路支持；Auto 风格对所有条目给中性风格分，Auto key 保留多种调性解释，因此传统终止式和多模板共同支持的短路线容易靠前。1563451 的 Functional/Jazz 元数据也说明流行用途没有被完整标注。本轮没有整体降低功能和声权重。

### 5. 三种算法为什么经常相同？

三个 preset 共用同一 Factory、和声分析主体、续写评分与分组。HarmonyFirst 改检索结构权重、节奏的资格口径；Abstract 只放宽有明确音集证据且兼容的扩展和弦匹配。普通三和弦精确输入通常不触发这些差异。升级功能由独立 enrichment 引擎完成，preset 不切换其生成算法；色彩偏好只在现有合格候选中重排。这三项功能分别评估。

UI→Controller→Worker 的 preset 传递、取消旧 generation、重新提交和缓存条件已核对。Controller 切换模式会清空旧推荐并重新计算；Worker 还核对 phrase revision、强制调性、拍号与有效 timing mode。本轮没有发现过期缓存证据。用户已确认切换、试听/MIDI、重开与多实例通过，沿用该实机结论。

### 6. 哪些 Loop 不适合直接循环？

明确边界缺陷：**C–G–Am → F–C** 会把四和弦输入加后续变成五和弦句；再次从 C 开始会重复起始 C。修复后只补 **F**。中段 **Am → F–C–G**、**Am–F → C–G** 继续补齐原循环；完整 **C–Am–F–G** 返回下一轮 **C–Am–F–G**，完整 **Am–F–C–G** 同样返回完整下一轮。没有强制循环落主和弦。

Loop 标签描述原模板用途，近似对齐、插入和弦及替代低音仍可能影响真实循环感。尤其 **Am–G#dim–G–D/F#** 的近似 Loop 后续需要听评；两轮文件及时间检查只证明路径和时值，没有自动证明自然。Why 的循环说明已改成可试听验证的片段说明。

### 7. Develop / Resolve / Color 有误分类吗？

已定位 Color 的具体错误：某些模板的色彩和弦已在输入中播放，后续只剩普通 C 或 Dm，仍沿用整条模板的 Color 标签。现在按实际后续核实色彩证据：无证据且落在该调式主三/主七和弦时转 Resolve，其余转 Develop；保留明确 typed color mask，避免丢失扩展色彩。单测覆盖 **C–Fm–G → C** 和 **C–Fm–G → Dm**，以及仍应为 Color 的 **C–F → Fm–C**。

Resolve 的“属到主”说明原来还可由整条模板的 authentic 标签触发，即使实际后续没有 V→I。现在使用实际相邻和弦或当前输入末和弦。Color 解释分别指出借用、副属/副导、变音/减增和弦、七和弦/挂留。Develop 可以包含发展后终止，与 Resolve 的音乐功能有重叠；当前分组不是互斥的作曲定理，较短或提前结束的路线仍需人工判断。

### 8. 已修复哪些问题？

- 多样性比较改为共同前缀严格超过较长路线的一半；完全等价路径去重和原有阈值保留。
- 精确完整和声匹配（rawCost≤0.00001）缺少所选风格标签时使用中性分；已有明确风格权重不变，非精确路线保留原回退分。
- 排序用 0.001 分桶消除浮点噪声；同桶优先较长完整后续，再用稳定 ID 排序。实际曾出现 77.888641 与 77.888649 的无意义先后差异。
- 修正部分及完整循环的续写边界。
- 按实际未来片段修正 Color 分类，修正终止说明，并细分色彩解释。

未改和弦性质兼容规则、全局 0.60/60 门槛、Factory、Schema、User Library、宿主接口或 Phase 7 布局。

### 9. 哪些仍需人工音乐评审？

dev.8 中 Loop 的两轮自然度；F–G–Am 后的色彩方向是否符合创作意图；Am–G#dim–G–D/F# 的近似路线是否可用；发展与提前终止的主观区分。Auto 的候选可能采用不同调性解释，试听时应同时看候选调性。没有真实宿主原始输入与诊断时，不能把某个离线设置推断为用户当时设置。

### 10. 需要补 Factory 吗？

当前核心问题已有内容，先修检索后排序链。本轮不补库。后续可定向审校 1563451 的风格元数据、直接的 F–C–F–G 续写需求，以及下降低音输入后的真实流行发展路线。隔离的 **Am–G#dim–G–D/F# → F–G–Am–C** 继续等待人工听评，没有加入 V4。

### 11. 对原推荐有什么影响？

会改变默认顺序、部分同组保留、循环后续长度及无实际色彩的分组；不是所有旧卡都保持原位置。强匹配缺失风格标签不再额外扣分，分叉明显的完整后续不再仅因共享两个开头和弦受罚。不同性质、转位、明确低音、既有数据/快照/MIDI 与运行时模式通过适用回归保护。候选数量和分数变化单独记录，不作为听感提升证据。

共同162格的 **70,808条 matcher 记录**，前后 key、相似度、rawCost、FULL/SKELETON/节奏子分、匹配窗口、续写起点及完整对齐轨迹全部一致，新增/丢失matcher记录均为0。变化发生在候选生成后的路径边界、分类、评分和呈现。

按“配置格 × intent × 完整实际和弦后续”统计：保留路线集合从2,332变为2,375，1,747未变、585旧路径移除、628新路径出现；默认集合从1,118变为1,094，764未变、354旧路径移除、330新路径出现。这里包含288条同ID Loop路径长度修正，以及29条实际普通终止后续从Color转Resolve，不能将集合差异直接称为错误召回或质量提升。旧候选ID未进入新支持列表的155条中，112为保留上限、25为多样性评分、13为分数门槛、5为等价去重；有损失的实例包括 C–Am–F–G 后 C–Dm–G–Am 的候选在更多列表上限之外。完整原始表保留，人工可按实际路径选择进一步回归案例。

对 **Am–G#dim–G–D/F#（4QN，OPEN）**，Auto/Auto下 Legacy与Abstract各默认2条、另有1条更多；HarmonyFirst默认3条、另有3条更多。固定A minor/Auto下仅HarmonyFirst保留1条 **Am–Am–D**，Legacy/Abstract无卡；固定A minor/Pop均无卡。Auto的分析面板会选A major，但例如 **Em–C** 路线实际匹配的是E minor（相似度约0.614、rawCost1.4），不能用面板调性代替候选调性。V4_A08在Legacy/Abstract约56.405分而低于60，HF约60.724分保留；其不足来自输入插入/结构、评分及有限母句，不能靠默认全库补足三张卡。这一离线配置没有复现用户“所有模式都只有同一条”的完整现场条件，仍缺当时原始输入/配置诊断；不把近似新选项视作已通过音乐评审。

### 12. 适合下一版 Cubase 测试包吗？

适合制作并已制作下一版 Cubase 测试包：Release 构建通过、完整 CTest 34/34、SDK Validator 47/47，48 格目标路线全部默认可见。工程完成；dev.8 安装及真实 Cubase 音乐验收待用户执行，正式发布保持未授权状态。

## 对照、诊断与交付

验证根目录为同级 `HarmonyContinuation-phase61-validation-20261016/`：

- `baseline-full/`：48 格旧行为、162 格共同语料及 12 格 diversity=0 消融；保留相应旧行为诊断可执行文件。
- `after-minfix/`：仅多样性与循环等初步修复，目标默认 24/48。
- `after-stylefix/`：加入精确匹配的未知风格中性回退，目标默认 36/48。
- `final-frozen/`：生产冻结版本的最终矩阵、216 格、逐 Factory 覆盖、完整轨迹、实际和弦媒体与 MIDI/Snapshot 检查。中间批次与最终批次分别保留身份。

以下排名覆盖三个 preset、2/4QN、Closed/OPEN，每格都通过严格后缀检查；分数示例为 Closed：

| C–G–Am 设置 | dev.7 目标排名 / 默认 | dev.8 目标排名 / 默认 |
| --- | --- | --- |
| Auto key / Auto style | 9 / 否，约64.889分 | 2 / 是，约77.889分 |
| C major / Auto style | 7 / 否，约64.889分 | 2 / 是，约77.889分 |
| Auto key / Pop | 10 / 否，约62.889分 | 3 / 是，约77.889分 |
| C major / Pop | 7 / 否，约62.889分 | 2 / 是，约77.889分 |

最终矩阵目标路线：Factory存在48/48、检索48/48、音乐身份/尾部约束通过48/48、评分门槛通过48/48、保留48/48、默认Top3为48/48。原目标没有被错误判为不合格，而是合格后排名不足。未将扩大候选数作为质量通过标准。

最终专项输出共 216 个 worker 输入配置、141,912 行逐 Factory 覆盖；开发过程保留的旧行为语料为162格。最终所有保留的 Color 卡中 `completion.noEvidence` 为0，默认 Color 卡78条；这些是证据覆盖和分类检查，不是音乐评分。新增测试仅覆盖旧35例未覆盖的 C–G–Am/常见Pop、循环边界、风格缺失和Color后续分类缺陷；复用原 worker、MIDI、Snapshot、Preview 及回归入口。

完整回归覆盖和弦识别/匹配、转位低音、Factory V4、User Library、兼容桥、Snapshot、Preview/MIDI、宿主契约及模式切换。`ContinuationEngineTests` 71/71，`ProgressionMatcherTests` 40/40；均包含于完整 CTest 34/34。冻结日志为 `ctest-frozen.log`、`validator-frozen.log`、`final-frozen.log`。安装脚本及数据契约未改，本轮不重复 dev.7 的隔离安装生命周期；未实际安装 dev.8，也未进行真实 Cubase 音乐听评。

## 安装、听评与回退

唯一安装器：[HarmonyContinuation 0.10.0-dev.8 Setup](../../HarmonyContinuation-phase61-delivery-20261016/HarmonyContinuation-0.10.0-dev.8-Setup.exe)。关闭 Cubase，运行 Setup，重新扫描后在 About 核对 `0.10.0-dev.8` / BuildCommit `dd76b60`。一个插件内提供 Legacy、HarmonyFirst、Abstract；Phase3A 关闭，Factory V4/657。本轮没有修改生产安装。

| 产物 | SHA-256 |
| --- | --- |
| Setup | `943421ea443401b25f2d69d412d8d359fef9b38294f10556d07b3e718a69ddf4` |
| VST3 二进制（构建与分发暂存一致） | `a9b12b2fb1487ebd1691f5bc249cd71f5cdddfdae6962713e5720fd8b1715154` |
| Factory V4（dev.7、dev.8构建与暂存一致） | `74c529828f8c4b934bbaf4ada719aa95491ab498b58885522124ff6505671534` |

完整身份与交付文件清单见包旁的 [BUILD_INFO.json](../../HarmonyContinuation-phase61-delivery-20261016/BUILD_INFO.json) 和 [SHA256SUMS.txt](../../HarmonyContinuation-phase61-delivery-20261016/SHA256SUMS.txt)。`moduleinfo.json` 产品版本为 dev.8，二进制包含 `dd76b60`；分发二进制与已验证构建逐字节哈希一致。

本轮用户验收只需检查新的音乐结果，沿用已通过的切换操作、试听/MIDI工作流、保存重开及多实例结论：

1. **C–G–Am → Em–F–G–C**：Auto/C major、Auto/Pop 下应在发展组默认前三；试听完整路线是否愿意采用。
2. **C–G–Am → F**：循环后直接接回输入 C，比较旧 F–C 多一和弦的差别。
3. **C–Am–F–G → C–Am–F–G** 与 **Am–F–C–G → Am–F–C–G**：连续两轮，各32QN（120BPM为16秒，WAV另有尾音），评估乐句完整和首尾连接。
4. **F–G–Am → Bb–Bbm–F**：Auto key / HarmonyFirst 的 Color 例子实际采用 F major 解释，Bbm 是借用和弦；固定 C major/A minor 不应期待同一组F大调候选。比较它是否符合创作方向。
5. **Am–G#dim–G–D/F# → Em–C**：Auto / Legacy / 4QN OPEN 的实际近似 Loop 例子，仅供听评。固定 A minor 下仍可能无卡，尚不能保证三条合格的不同续写；不为凑数改门槛。

听评文件在同级交付目录 `listening/`，7份 WAV、7份对应 MIDI，另保留2份旧 dev.7 循环 MIDI。入口示例：

- [C–G–Am → Em–F–G–C：完整试听](../../HarmonyContinuation-phase61-delivery-20261016/listening/target-C-G-Am-FUNCTIONAL_016-full.wav)、[后续 MIDI](../../HarmonyContinuation-phase61-delivery-20261016/listening/target-C-G-Am-FUNCTIONAL_016-suffix.mid)。
- [C–Am–F–G：新两轮试听](../../HarmonyContinuation-phase61-delivery-20261016/listening/loop-C-Am-F-G-two-cycles-full.wav)、[旧 C–Am–F–G → C MIDI](../../HarmonyContinuation-phase61-delivery-20261016/listening/before-dev7-C-Am-F-G-to-C-full.mid)。旧文件复用 dev.7 原始输出，未冒充本轮重跑。
- [Am–F–C–G：两轮试听](../../HarmonyContinuation-phase61-delivery-20261016/listening/loop-Am-F-C-G-two-cycles-full.wav)。
- [F–G–Am → Bb–Bbm–F：完整试听](../../HarmonyContinuation-phase61-delivery-20261016/listening/color-F-G-Am-open-Auto-HarmonyFirst-full.wav)。
- [Am–G#dim–G–D/F# → Em–C：近似路线试听](../../HarmonyContinuation-phase61-delivery-20261016/listening/r01-open-Am-Gsharpdim-G-DslashFsharp-default-full.wav)。

节奏结论：输入的切分起点与 1.5/0.5/2QN 保留；推荐节奏来自模板时值乘以匹配输入的全局比例，示例后续为各1.5QN。当前没有继承切分样式的节奏生成器；本轮没有开发该能力，导出时值由现有回归和样本MIDI回读保护。

版本回退：关闭宿主，保留个人数据备份，运行原 [dev.7 Setup](../../HarmonyContinuation-phase6-delivery-20261014/HarmonyContinuation-0.10.0-dev.7-Setup.exe)，核对 `0.10.0-dev.7` / `8c27c27`。其 Setup SHA 为 `eb0b9c77c8cdd42f7e679b4de68cd58c57a677e3a6d5ce0e0eda8421202409a0`，本轮已读回核验；个人库、快照和历史 Factory 不删除。Legacy 是算法模式选择，dev.8 三种模式都包含本轮通用修复；恢复旧排序需要回装 dev.7。

源码回退基线为 `66d7f0713b31f0e5180abf97d14d5a2b9dfc51ac`；保留已提交 `dd76b60`，需要回退时在独立分支回退该提交，先保管既有未提交文档，避免 reset 覆盖他人改动。本轮没有推送远端、没有更新 GitHub 资产。

## 工作分工与证据边界

主代理判断并实现评分、排序、循环和后续分类修改，审核关键差异及音乐语义。Luna 子代理负责调用链/历史取证、已有诊断扩展、指定测试与数据统计。首次默认系统 TEMP 下单元测试的数据库原子替换失败；将 TEMP/TMP 指向工作区隔离目录后同一测试通过。媒体是内置 PreviewSynth 合成，MIDI 是实际候选导出；离线数据不等同于真实 Cubase 听评。

用户于 2026-10-16 的 dev.7 实机报告确认运行时切换、试听/MIDI、保存重开和多实例隔离；未提供本轮宿主诊断原文件。生产安装本轮未修改。接手前已有的协作与交接文档改动保留，提交范围与同步状态以最终 Git 记录为准。

生产代码和测试已提交 `dd76b60`；报告、MAP和HOST_SPIKE作为随后文档提交。原有AGENTS/RISKS/反馈文档改动及本轮NOW/索引/RUNBOOK接续更新仍保留在工作区，未将这些既有内容混入生产提交。未推送、未合并main、未发布。
