# 0.10.0-dev.3：Cubase 直接拖放推荐可靠性

## 结论与证据边界

分支 library/v4；源码构建提交 `99017f596cef895f2024677120b7005fa7c4a98e`，Build ID `0.10.0-dev.3 / 99017f5 / Release / Library 4`。源码已由 GitHub 接口同步，逐个 blob 与完整 tree 均与本地一致；不使用 git-remote-https 推送。验证文档另行提交。

**READY FOR CUBASE RE-ACCEPTANCE：YES。真实 Cubase B08/B11 验收仍 Pending。** 本轮完成已能独立处理的输入语义、匹配、诊断、定向回归与开发包工作，没有正式发布授权，也没有宣称真实宿主问题已完全证明修复。

用户明确报告来自 Cubase Pro 15 和弦轨直接拖放。正式路径为 IDataPackage → HostAdapter → VstXmlChordParser → ImportedProgressionSession → RecommendationWorker → MainView；不会绕到 MIDI 提取器。解析保留 name/keyNote/bassNote/mask/pitches/projectTime，projectTime 只接受 quarterNotes；相邻起点推断时长，末和弦正常为 OPEN。旧文档确认曾实测 1087 字节 VST-XML 1.4 四和弦；仓库有旧 1.4 单和弦夹具和结构化三和弦样例，不能证明本轮 B08/B11、缺五度或 Swing 的真实字段。

本轮没有捕获到用户失败时的原始 B08/B11 拖放载荷。以下确定的是代码缺陷及可重复失败机制；它是否就是用户两段实际载荷中的全部根因，需要 dev.3 主动导出的真实诊断确认。所有新 XML 对照都标记为 adversarial synthetic，绝不称作真实 Cubase 捕获。

## B08 / B11 失败机制

旧 1440 组矩阵用名称生成完整的音集合，再生成互相一致的 XML，未覆盖“名称正确、演奏音缺五度”的关键差异。

旧 normalizeChord 的 classify 要求完整 0/4/7 或 0/3/7 三和弦，缺五度会使三度和七度已清楚的七和弦变成 Unknown；存在音集合时不会恢复名称身份。makeMatchQuery 随后将此 Unknown 和演奏精确音集送入 Matcher。B08 三前缀缺五度输入的目标仍进入 Matcher，continuationStartIndex 仍为 3，但匹配度从 0.981247 降至 0.402570，低于 0.60，整个结果为 NO_RECOMMENDATIONS；不是 UI 隐藏。B11 相同缺五度输入从 0.976153 降至 0.393351，目标在最低匹配度处流失；仍有 2 条其它推荐，但目标 NOT_RETAINED，不能算目标成功。理想完整输入在 dev.2 原本就通过。

此外，分析层曾优先采用 bassNote，匹配层又通过 chordPitches 还原名称斜杠低音，导致两个层的低音语义可能不同。仅改变自动配器低音未必重现用户问题，本轮不将其单独宣称为真实根因。

## 节奏与排序

旧 pairCost 把节奏代价放进动态对齐和 harmony similarity；回溯后再以已对齐时长修正代价。Swing / 不等长不仅改变辅助分，还可能改变路径与最低匹配度、最低总分结果。现在路径及和声匹配度由和声计算，节奏偏差在该路径上独立测量，仍参与推荐排序和续写时值估计。原始事件起点、时长和 MIDI 文件不量化、不重写。结构分析仍可根据时值判断经过节点；不承诺所有节奏下排名完全一样。

下表为 Auto / OPEN、每颗省略五度、明确标记的模拟 XML 对照。完整原始输入、各阶段数、排名、节奏分见包内 evidence/cubase-before 与 cubase-after。目标是匹配、保留和可访问，表中同为第一名是算法结果，没有 B08/B11 特例或固定 V4 奖励。

|输入|节奏|旧推荐数|新推荐数|旧目标匹配度|新目标匹配度|新排名|结果|
|---|---|---:|---:|---:|---:|---:|---|
|B08 两和弦|正拍|1|19|0.515845|0.985033|1|DEFAULT_VISIBLE|
|B08 两和弦|Swing 2:1|1|19|0.515845|0.985033|1|DEFAULT_VISIBLE|
|B08 两和弦|7:1 长短|1|19|0.515845|0.985033|1|DEFAULT_VISIBLE|
|B08 两和弦|切分偏移|1|19|0.515845|0.985033|1|DEFAULT_VISIBLE|
|B08 两和弦|非整数 QN|1|19|0.515845|0.985033|1|DEFAULT_VISIBLE|
|B08 三和弦|正拍|0|1|0.40257|0.981247|1|DEFAULT_VISIBLE|
|B08 三和弦|Swing 2:1|0|1|0.371718|0.980149|1|DEFAULT_VISIBLE|
|B08 三和弦|7:1 长短|0|1|0.377552|0.980149|1|DEFAULT_VISIBLE|
|B08 三和弦|切分偏移|0|1|0.379595|0.980379|1|DEFAULT_VISIBLE|
|B08 三和弦|非整数 QN|0|1|0.40257|0.981247|1|DEFAULT_VISIBLE|
|B08 四和弦|正拍|0|5|0.333501|0.98178|1|DEFAULT_VISIBLE|
|B08 四和弦|Swing 2:1|0|5|0.339355|0.980786|1|DEFAULT_VISIBLE|
|B08 四和弦|7:1 长短|0|5|0.339355|0.980786|1|DEFAULT_VISIBLE|
|B08 四和弦|切分偏移|0|5|0.337804|0.98099|1|DEFAULT_VISIBLE|
|B08 四和弦|非整数 QN|0|5|0.333501|0.98178|1|DEFAULT_VISIBLE|
|B08 五和弦|正拍|0|3|0.31789|0.982102|1|DEFAULT_VISIBLE|
|B08 五和弦|Swing 2:1|0|2|0.273022|0.980621|1|DEFAULT_VISIBLE|
|B08 五和弦|7:1 长短|0|1|0.253584|0.980621|1|DEFAULT_VISIBLE|
|B08 五和弦|切分偏移|0|2|0.321121|0.980921|1|DEFAULT_VISIBLE|
|B08 五和弦|非整数 QN|0|3|0.31789|0.982102|1|DEFAULT_VISIBLE|
|B11 两和弦|正拍|2|5|0.393351|0.976153|1|DEFAULT_VISIBLE|
|B11 两和弦|Swing 2:1|2|5|0.393351|0.976153|1|DEFAULT_VISIBLE|
|B11 两和弦|7:1 长短|2|5|0.393351|0.976153|1|DEFAULT_VISIBLE|
|B11 两和弦|切分偏移|2|5|0.393351|0.976153|1|DEFAULT_VISIBLE|
|B11 两和弦|非整数 QN|2|5|0.393351|0.976153|1|DEFAULT_VISIBLE|

B08 三前缀修复后的节奏相似度：正拍 1、Swing 约 0.75、7:1 长短约 0.298、切分约 0.816；目标匹配度约 0.980–0.981，推荐维持 1 条。B11 两颗 OPEN 只有一个已知时长对，无法可靠判别节奏形状，节奏分 1 是证据不足下的中性结果，不是确认 Swing 完全相同；推荐维持 5 条、默认 3 条、More 5 条。

## 实际修改

- ChordNormalizer / HarmonyAnalysis：分开 symbolic identity、performed mask/bass 和字段来源；三度+七度可识别缺五度七和弦。保留原字段，来源未知时仍使用旧结构判断。
- 有有效原始名称的 CubaseDrop/Clipboard：以名称提供根音和性质；只有明确斜杠低音成为匹配硬约束。自动 bassNote 仍供演奏与诊断，不作为用户明确要求的低音。名称、音集合相矛盾会记录不确定性并增加相容性代价；相反三度/七度每类增加 0.18，另有来源冲突代价，不是一律忽略性质冲突。
- ChordPitchSet：保留演奏音集合和自动低音；明确斜杠低音优先。6/9 的数字斜杠不再误当低音，b5/#5 名称含义保留；C9 与 Cmaj9 始终区分。未修改用户原始 MIDI 文件。
- ProgressionMatcher：显式名称身份用于检索；节奏不再决定对齐路径与和声门槛，节奏辅助评分仍在。
- ContinuationEngine：保留现有明确低音/三度/七度冲突保护、最低匹配度和总分阈值、去重及多样性规则；增加每个模板的流失原因、候选分数记录。未强制置顶、未对所有 V4 增奖励、未修改保留上限。
- ProductServices / Controller / MainView：扩展既有主动本地导出，包含字段、规范化结果、来源可信度、OPEN/CLOSED、Key/Style/Intent、各阶段计数、每个模板对齐、续写起点、节奏分、过滤原因、后台结果与实际 UI/More 排名。双语菜单同步。
- tests/CubaseReliabilityTests 与 CMake：800 组冲突/缺音/节奏对照、Worker 目标保留、诊断内容、负向保护、MIDI 时序对照。安装测试增加可选 ProductVersion 参数以复用 dev.2 的隔离维护检查；安装器逻辑未更改。

## 定向验证与保留状态

12 个相关 CTest 全通过：CubaseReliabilityFocused、RecommendationReliabilityFocused、FactoryV4Focused、EnrichmentTests、UILayoutTests、MidiExportTests、MidiImportTests、PreviewTests、HarmonyAnalysisTests、ProgressionMatcherTests、ContinuationEngineTests、VstXmlChordParserTests。未运行全量 CTest、Validator、音乐 Benchmark 或完整 DAW 矩阵。

新 800 组：B08 2/3/4/5 前缀及 B11 两前缀共 500 组均 TARGET_MATCHED、TARGET_RETAINED、DEFAULT_VISIBLE、MORE_ACCESSIBLE；另 300 组普通输入均有推荐，记录为 NO_TARGET_REQUIRED，不能误报为目标模板成功。Auto/Forced C、OPEN/CLOSED、完整音集/自动转位/缺五度/转位兼缺五度/第三颗省略七音、五种时值均覆盖。Worker 对选定缺五度输入核对目标身份及数量。

原 dev.2 的 1440 组继续通过，普通进行与 Polish/Rich/Advanced 保护回归通过；Preview/MIDI 精确音集合、低音及候选身份检查继续通过。独立 MIDI 提取对照覆盖正拍、提前释放、声部起点差异、相邻音符重叠和小幅整体偏移，均提取 4 颗并保留 11 条推荐；该结果不冒充 Cubase 直接拖放验证。

界面离屏检查通过：FULL/SKELETON 切换不改变推荐或导出；诊断中的实际默认与 More 身份正确。C→F→F#dim7→G 为 SKELETON 4/4 合理，F#dim7 是导向 G 的关键连接；短暂经过版可以省略。主结构/关键连接/可省略装饰三级分类只列入后续计划，本轮未强制改写。

Factory V4 657 条；Schema 2/2/5/3/2 不变。生产用户数据未操作；安装隔离测试结果见下方交付部分及 evidence/installer-upgrade-result.txt，保护 User Library、收藏、设置、个人进行、历史库与未管理文件哨兵。v0.9.0 正式包、dev.1/dev.2 原包保留。

隔离安装实跑 **413 项通过**，覆盖 fresh / official-v09 / rc-manual / unknown / official-content-layout，并验证组件取消勾选、显式修改、修复、卸载、先备份后替换与个人哨兵保护。官方 v0.9.0 测试对象为从可信官方标签重建的夹具，不冒称当前重新下载或运行正式发布原包。首轮受限环境拒绝创建随机测试卸载登记，安装器正常回滚；随后在同样隔离路径、独立随机 HKCU 登记下完整通过，生产安装未操作。49 份最终安装日志随包保存。

Factory V4 28 条可发现性专项：26 DEFAULT_VISIBLE、2 MORE_ACCESSIBLE、0 BLOCKED；生成 B01/B08/B09/B11 四份 MIDI 随包保存。

## 开发包与未解决项

统一包目录：build-installer/v4-0.10.0-dev.3/。Setup 位于目录根；VST3 位于 stage-4/HarmonyContinuation.vst3/；Factory 位于 stage-4/factory.db。BUILD_INFO.json、SHA256SUMS.txt、FILE_MANIFEST.sha256 和 evidence/ 保存构建与检查依据。

- Setup SHA-256：`10fe395bdcf2fc40469a75d75186e8ad5424e1ce0e1db4a24cee30dfa1a3aab5`
- VST3 主二进制 SHA-256：`e2e317a7526cb678ba546ad1779421b7e4ffdcbc7951e5a76c8dbee72a00504d`
- Factory DB SHA-256：`74c529828f8c4b934bbaf4ada719aa95491ab498b58885522124ff6505671534`

唯一必要的外部复验是用户在 Cubase Pro 15 重新拖入原问题段落并试听。诊断采集按 [人工验收清单](V4_DAW_ACCEPTANCE.md)；如仍失败，导出的原始字段和每阶段原因会直接显示流失位置。真实 B08/B11 payload 未捕获，故其确定宿主根因仍未闭环；不得把本报告的模拟对照改写为已通过真实宿主。完成 dev.3 交付后停止，等待人工复验，不自动开发其它功能。
