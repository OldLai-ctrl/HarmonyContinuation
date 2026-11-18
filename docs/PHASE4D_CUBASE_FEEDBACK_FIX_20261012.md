# Phase 4D Cubase 反馈修复与交付报告

日期：2026-10-12。基础源码 `library/v4@6efacf3cbe78beed7d9645724242fec258f4b19f`，开发版本 `0.10.0-dev.4`，二进制 Build ID `6efacf3-phase4d`。该标识明确包含未提交的 Phase4D 补丁，不冒充纯 HEAD 构建。补丁和完整源码快照随交付保存。

## 证据范围

已阅读 Phase4B 核销/综合实现、3A/4A/4B 算法证据和现有工作区规则。起始工作区干净。本轮用户进一步要求：工程拍号可变，应根据输入时值估计；此要求优先于初稿中按当前宿主拍号映射的方案。

没有用户原始 Cubase MIDI/XML，也不能读取另一设备已安装插件。所有本轮离线输入均标为模拟，来自用户文字和截图描述；根音、和弦性质、QN、OPEN 与描述一致，不声称真实音符排列/原始字段完全相同。没有运行 Cubase 或人工试听。SDK ProcessContext 测试、离屏宿主测试、文件读回不等于真机验收。

## R01：差异来源

ON 包的编译宏现在通过同一 `developmentContext` 入口用于 Controller 提交和诊断。OFF/ON 测试分别验证该入口实际模式；结果对象记录实际 HarmonyFirst 与 Phase3A 状态；UI 高级分析和宿主诊断显示状态，导出同时记录构建和实际调用模式。Phase3A 是独立推荐参数，生产两包均 OFF；四组合仅用于离线隔离风险测试，不暴露成用户开关。

不能确认另一设备旧 ON 文件真正生效。新包具备验证它的最小证据链，且可见状态无需猜测 ZIP 名称。分析前空结果的 UI 状态不代表已完成调用，应等待结果。

R01 对照覆盖 2/4QN × OPEN/非OPEN × Auto/A Minor/A Major × HarmonyFirst OFF/ON × Phase3A OFF/ON，共48组合。逐匹配 alignment、结构、调性、候选来源与门槛记录于 `candidate-traces.txt`，事实提取表随包提供。

在截图对应的模拟配置（2QN、OPEN、Auto、HF ON、P3 OFF）中：

| 环节 | A08 实测 |
|---|---|
| 初检 | 657条 Factory 全部进入 shortlist，A08在内 |
| Auto 调性 | A Major 1.47625，置信度0.1908788；A Minor 1.02625，置信度0.0776055 |
| 匹配解释 | Auto保留24调性；A08经 A Minor 解释，similarity 0.769533 |
| 有效路径/门槛 | 有可续写后缀；eligibility 60.1784 > 60 |
| 分数 | 候选60.7241；支持合并/排序后60.9041 |
| 意图与Top-K | Loop 第6，保留于 worker Top10 |
| UI | 默认不可见，“查看更多”可见（默认展示规则实际计算；More列出全部worker保留项，真实界面顺序另有UI_MORE诊断） |

同输入固定 A Minor 时 A08 第1、约60.9041，FULL4/SKELETON3；固定 A Major 无通过门槛的输出。Auto FULL4/SKELETON4。截图与此前固定小调报告比较了不同的检索语境，而非同一查询。

Auto A Major 来自已有评分，不是展示字符串把小调标成大调：大调下 G#dim 是自然 vii°、G 可作借用 bVII、D 是 IV；A Minor 下 D 大三和弦不是自然小调的 diatonic iv。首个 Am 的主和弦证据不足以抵消整体评分。该模型只有大/小调假设，且最高置信度仅约19%，这组输入本身不能证明唯一调性；保留全部调性检索。未强制 A Minor、未改全局权重、未写 A08 ID 特判。实际解析字段若与模拟不同，应以新诊断复核。

Loop 前两项：`V3_NIGHT_035` 经E Minor解释的 Em→Am，68.0438；`V3_CYCLE_025` 经G Major解释、合并 `POP_003` 支持的 Em，67.1713。前者把G#dim作低结构插入（成本0.211536），其余三节点同功能匹配；后者也把该减和弦作插入，保留其他三节点。它们是局部功能匹配后沿现有 loopable Factory 路线续写，非实时生成，也不等同逐和弦完全一致的Factory命中。它们高于A08且占默认展示位；完整成本/尾部/对齐见trace，不能把这个现象当成A08检索丢失。未提高A08优先级。实际截图若仅两行，还受窗口、风格/倾向影响，不宣称复现全部截图像素。

四态隔离显示：截图配置A08仅HF ON为60.9041；P3 ON（HF OFF/ON）为68.9041，仍Loop第6。两机制参数独立，P3的8分插入惩罚退款确实可能与HF并存，不能称为同一个开关或正常用户自动双重补偿；实际交付两包都不启用P3。

## 时值问题及修复

Factory时值以 QN 保存；原有实际音符布局已由候选时值驱动，并由 Preview/保存/拖放共同消费。问题不在仅缺 MIDI 拍号 meta：旧估计器要求至少两个结构匹配的已知时值比率；短前缀/OPEN 常只剩一个有效比率，于是默默回退比例1，4QN原型保持4QN。两个已知节点的输入可算出3QN，同一条路线末和弦变OPEN后却又退回4QN，因此不同条目表现不一。

新估计器优先结构匹配比率；结构证据不足时使用其他有效对齐节点的时值比率；允许单个有效观测，降低置信度；内部未知时值可由下一输入起点取得。采用中位数，保留0.25–4比例边界和既有双样本冲突回退；无有效证据保留原型比例1且置信度0，在诊断明确标记。OPEN保持时值与后缀共用估计比例。

宿主 ProcessContext 当前拍号仅是参考，可能不覆盖输入所处区段。XML导入不再把播放指针处拍号写成所选输入拍号；SMF拍号保留在Session及文件诊断，原始变化事件也导出，不由当前宿主强行覆盖。QN节奏估计不依赖宿主当前拍号；不根据时值虚构唯一拍号。未知拍号仍使用既有4/4文件元数据兼容回退，但真实音符长度按输入证据估计，不能据meta声称工程已被识别为4/4。未传入复合拍分组或完整宿主meter map，不宣称推断2+2+3等分组。

十条实库原型：V4_A08、V4_B08、V4_B11、POP_011、FUNCTIONAL_011、V3_SHADE_004，以及 CITYPOP_JPOP_003/005/010/018（含特殊相对节奏）。覆盖4/4、3/4、6/8、5/4、7/8、9/8 × closed/OPEN，共120组。测试按输入构造目标时值；拍号是测试标签，不是生产算法缩放依据。逐项旧/新比例和末音保持见 `meters.tsv` 与事实对照表。比如A08 OPEN：旧3/4输出4QN、保持4QN、总长23QN；新输出3QN、保持3QN、总长18QN。4/4仍4QN/24QN；6/8同为3QN，5/4为5QN，7/8为3.5QN，9/8为4.5QN。特殊节奏保持原型长短比例，不把每个和弦一律变成一小节。

新增真实 `recommendContinuations` 路径验证：V4_B11正常质量门槛、六拍号时值、OPEN、HF两态均得到正确比例、保持时值和后缀，并比较实际 Preview与保存/拖放音符。其余120组合覆盖公共估计器、原型实际实现、输出/快照读回。没有声称120组合都等于完整UI中每条路线默认可见。

续写起点沿输入QN与估计OPEN结束处衔接；没有将切分输入强行对齐到小节。Preview、SMF保存、拖放 clip使用相同最终候选布局；测试比较音符起点和持续长度而非仅meta，允许SMF tick量化误差。60个六拍号输出MIDI随包，可检查输入加续写路径。

## FULL / SKELETON 与音乐测试

固定 C Major，F4→F#dim7 0.5→G3.5→C4：Legacy 的减七结构权重0.35、检索权重0.35，Skeleton3/4；HarmonyFirst结构权重0.35、检索权重0.72，Skeleton4/4。当前Skeleton阈值0.55。HarmonyFirst有意去除短时值和Approach对检索骨架的直接惩罚，短促并不等于无功能；因此不强删该节点。所有节点、功能、两类权重与匹配成本见trace。

HF下另一个实测可区分样例是固定 A Minor 的 Am→G#dim→G→D/F#（2QN，末OPEN），Skeleton3/4；已保存MIDI。中文说明明确结构视图与演奏内容区别，FULL里非Skeleton节点灰化；原始输入不因Skeleton被删掉，Preview和导出保留它们。

半音下行低音：C→Cmaj7/B→C7/Bb 输入前三个，各4QN，V4_B08约81.8924，后续F/A→Fm/Ab；HF两态、有/无五度的符号输入均通过，B/Bb低音保持。MIDI包含输入和完整五和弦对照；缺五度MIDI保留开头完整C，只对后续七和弦缺五度，且重新导入验证核心性质。单纯C/E双音缺乏唯一MIDI和弦证据，不宣称可无歧义识别。

Side-slipping：Cmaj9→Dbmaj9输入前两个，各4QN，V4_B11约77.1305，后续Cmaj9；HF两态、有/无五度均通过。验证大七度11而不是小七度10，未把Cmaj9变C9。完整/输入/缺五度MIDI均随包，所有输入文件实际SMF导入读回通过。

## R02

全部为模拟：F→G→Am；A每和弦4QN closed，B为3QN/5QN/OPEN；Auto、C Major、A Minor；HF两态，共12组合。Auto A+HF OFF Color0、HF ON Color5；Auto B两态Color5；固定C Major或A Minor两种时值两态均Color0。

Color候选经 F Major 解释，来源包括V3_SHADE_002/019/024/031/008，实际分数和路径见事实表/trace。这说明只测试固定C/A调性不足以解释Auto宿主现象；没有放宽核心性质、强行延长不可循环耗尽路径或生成新和声。真实原始输入缺失，不能确定用户Cubase R02的根因。新诊断导出包含原始字段、MIDI音符、时间及候选淘汰链，供最小复现。

## 回归、性能及保护

Release、Factory4、OFF/ON独立目录。全量CTest均33/33，含Factory V4、核心性质负例、匹配、续写、Enrichment、MIDI Import/Export、Preview、Snapshot/旧状态、UI/Productization、库兼容与模拟宿主。测试加强后单项Phase4D在两构建再次通过，各12364检查；只增加实际推荐链及样例读回，没有更改生产代码或既有测试预期。VST3 Validator两包各47/47。

原1440矩阵严格模式0失败。另在相同工具链上链接HEAD原始ContinuationEngine进行基线观测，重现历史矩阵哈希 `3B3494E862DAA0F26538CC550BF3740A0F3AC0BF140E650C0FBB8318F4858909`。新版 `8DC3784E84CE4BC6D99EC5B1F70CE02214EB69EC3D81326FDEB26FD0663E54D8`。57行变化，标准4QN rhythm=0全部不变；差异局限变时值/OPEN的输出节奏置信度、评分/合并及候选数量。不是字节等同回归；它们是扩大有效时值样本的实际行为变化，完整差异保留供评审。不得把“0失败”解释为所有候选完全未变。旧timing观测harness允许记录预期失败，只作为before观测，不能计入通过门禁。

性能：同机R01固定小调90次/模式，前10次预热，80次统计。旧引擎P50 Legacy0.6604/HF0.6564ms、P95 0.8781/0.9510ms；新版OFF最终P50 0.6222/0.6261ms、P95 0.7889/0.8810ms，max0.8524/0.9380ms；两模式测量CPU时间62.5/46.875ms，工作集约15.6MB、峰值19.2MB。运行期间系统调度会改变尾部耗时；原始OFF/ON性能表和旧观测表均提供。未见这组离线样本明显资源退化，不当作Cubase实时性能结论。

OFF Setup已生成；隔离TestRoot安装/卸载10项通过，安装二进制与staging一致、活动库V4/657、个人数据哨兵保留、独立测试注册清除。未改安装器逻辑，未重新运行全部历史升级生命周期。实际生产安装未执行。

Factory两个构建都是657条，SHA-256 `74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534`。Factory源数据、Schema、User Library、生产用户库、正式main/Tag/已发布包未修改。仅测试隔离库写入。没有commit/push/merge/tag/Release操作。执行期间检测到AGENTS.md新增多代理规范，属于并发外部变更，本轮没有编辑或回退它；源码快照保留当前文件，本轮功能补丁排除该外部差异。

实际源码：ContinuationEngine输入节奏证据、RecommendationSet实际状态、DevelopmentMode提交共用入口、Controller输入/状态诊断和XML拍号来源、RecommendationWorker调性/拍号/模式缓存失效、ProductServices逐层诊断、MainView状态与弱化视觉、中英资源、CMake dev.4及新增Phase4D测试。Session/Snapshot格式版本保持。相关文档更新NOW/RUNBOOK及本报告/清单。Git差异与新增文件完整随源码快照/补丁交付。

分工：主代理负责根因、时值策略、评分/和声边界、代码及失败性质判断、测试与最终门禁；受用户授权的Luna max子代理仅整理日志/CSV事实表及按明确清单打包、计算哈希。真实Cubase待验证项仍由上文明确保留，子代理完成不等于真机通过。

## 交付与回退

`build-phase4d-delivery` 内有OFF统一Setup、OFF/ON便携ZIP、完整源码ZIP、交接ZIP、外层SHA256SUMS。便携包含完整VST3/CRT/Factory4、BUILD_INFO、中文清单、MIDI和对应验证证据；主交接包另含旧/新对照、完整日志和补丁。外层ZIP哈希位于同目录SHA256SUMS（不放进自身ZIP造成循环依赖）；逐文件清单位于包内。

具体操作见 [中文验收清单](PHASE4D_CUBASE_ACCEPTANCE.md)。关闭宿主并备份旧完整bundle后替换，一次只放一个同身份插件；回退恢复完整备份或旧Setup。独立Factory历史与个人数据均保留。dev.3旧包仍在原独立目录，不覆盖。本轮交付可供再次Cubase验收，真实R01身份与R02需诊断闭环后才能讨论正式发布。

## 最终标记

R01 ROOT CAUSE：PARTIAL（模拟Auto/手动调性与Top-K差异已确认；另一设备旧ON身份及原始输入未取得）

METER-AWARE MIDI OUTPUT：PASS（按最新要求由输入时值驱动，拍号仅参考/元数据；不宣称唯一拍号识别）

PREVIEW-EXPORT CONSISTENCY：PASS

FULL-SKELETON VERIFIED：YES

R02 HOST ROOT CAUSE：UNKNOWN

OFF REGRESSION：33 / 33

ON REGRESSION：33 / 33

FACTORY DATA CHANGED：NO

HARMONYFIRST DEFAULT ENABLED：NO

PHASE 3A DEFAULT ENABLED：NO

READY FOR CUBASE RE-ACCEPTANCE：YES

READY FOR FORMAL RELEASE：NO
