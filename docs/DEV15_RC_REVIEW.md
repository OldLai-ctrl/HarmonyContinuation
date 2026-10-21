# dev.15 与 v0.10.0 RC 技术评审

RC1 接续基线为 `7336c1d`，dev.15 包为 `626f164`。用户在本轮明确报告 dev.15 的 Host/Windows 双输出、MIDI、工程恢复与稳定性已在 Cubase 验收通过。RC1 三项 UI 变化仍需用户复验，FL Studio 实机仍 Pending。下文保留 dev.15 的原始工程证据和历史测试契约。

## RC1 UI 修复与精简门禁

- 版本：`0.10.0-rc.1` / Release / Windows x64；源码 / BuildCommit：`435ed852d50346d3c1ae8163451e6580fe4dac9e`（`435ed85`）。源码提交之后的交付文档提交不改变包身份。
- 唯一 [RC1 Setup](../../HarmonyContinuation-rc1-delivery-20261021/HarmonyContinuation-0.10.0-rc.1-Setup.exe)；[SHA-256](../../HarmonyContinuation-rc1-delivery-20261021/HarmonyContinuation-0.10.0-rc.1-Setup.exe.sha256)：`74fda52d8b0958460db4ff34a8af1edde3b3c15040fd3c3334d493ee58c0e324`。保留 dev.15 包用于回退。隔离安装 helper 仅用于开发验证，不作为第二个用户包交付。
- 修改范围：四个 `MainView` UI 文件、CMake 默认预发布版本及发布说明；未触及 PreviewTransport、音频后端、DSP、Processor、推荐评分、Session、Schema、Factory 或安装器逻辑。Factory 继续为 V5 / 658。
- 光标：下端三角手柄，沿整根光标提供约 22 个逻辑像素的命中宽度；悬停/拖动用暖色反馈。任意进度带点击保留，QN 映射包含水平滚动偏移。命中区裁到可见卡片范围，MIDI 控件优先。
- 升级高亮：保留 `EnrichmentProvenance` 分类，原始/插入/替换/修改分别使用既有中性/暖色/缺口色系/蓝色背景。名称无新增或替换前缀；底部图例及曲线节点形状保留区别。
- 曲线：节点由区间中心改为 `startQN`，按真实 QN 映射；标签位于事件时段内，末项延伸至终点并画结束刻线。色彩指数和推荐分数不变。

| 本轮检查 | 实际结果与范围 |
| --- | --- |
| Release 增量构建与身份 | PASS；生成版本 `rc.1 / 435ed85 / Factory V5`，commit override 为空；[最终构建日志](../../HarmonyContinuation-rc1-validation-20261021/build-final-435ed85.log)。Setup 版本、校验文件及构建/暂存插件字节[一致](../../HarmonyContinuation-rc1-validation-20261021/delivery-identity.txt) |
| 定向 UI 测试 | [2/2 PASS，2.11 秒](../../HarmonyContinuation-rc1-validation-20261021/ctest-ui-final.log)：`UILayoutTests`、`CompactEditorUI`；整根光标/手柄、进度带点击、水平滚动、暂停释放、MIDI 优先、视口裁切、四类高亮、等时/不等时 QN 与曲线起点 |
| 普通推荐与升级进行最小集成 | PASS，既有原生 UI smoke 内核对试听/暂停/定位回调、简略/详细按钮切换、选中对象、QN 与 MIDI 对象身份；复用同一音频实现的 dev.15 实际 SDK 音频及用户 Cubase 证据 |
| RC 安装身份与隔离升级 | [PASS](../../HarmonyContinuation-rc1-validation-20261021/upgrade-result.txt)：一次 dev.15 / 626f164 → RC1 / 435ed85 升级；16 个插件文件与清单一致，Factory V5 / 658；用户样本与 V4 历史样本字节保留。隔离 AppId 清理完成，真实安装未改动；[升级日志](../../HarmonyContinuation-rc1-validation-20261021/upgrade-rc1-install.log) |
| 安装后 SDK 编辑器加载 | [2/2 PASS](../../HarmonyContinuation-rc1-validation-20261021/installed-editorhost.log)：安装后的真实模块打开、子编辑器、resize、关闭；是 SDK 开发宿主，不是 Cubase 实机 |
| 历史门禁复用 | dev.15 单次 CTest 34/34、Validator 47/47、原生宿主 42/42、隔离生命周期 26 项及 Factory V5 完整性；原始日志见下文。本轮没有重跑完整矩阵 |
| Cubase 人工验收 | dev.15 已由用户报告通过；RC1 下方四项 Pending |
| FL Studio / 正式发布 | FL Studio Pending；正式发布等待用户明确确认 RC1，通过前无 main 合并、Tag 或 Release |

本轮主代理审查命中区域、时间语义、截图及发布门禁；LUNA实施明确的 UI 修正、既有 smoke 补充、构建与打包，主代理完成最小隔离升级/加载及最终身份核验。截图为实际 VSTGUI 开发端渲染，部分无音频接收器的按钮禁用，不作为 Cubase 截图。

| 对比截图 | 修复前 | RC1 修复后 |
| --- | --- | --- |
| 升级高亮与起点曲线 | [dev.15](../../HarmonyContinuation-rc1-delivery-20261021/screenshots/before-enrichment-curve.png) | [RC1](../../HarmonyContinuation-rc1-delivery-20261021/screenshots/after-enrichment-curve.png) |
| 主列表与播放手柄 | [dev.15 主列表](../../HarmonyContinuation-rc1-delivery-20261021/screenshots/before-main.png) | [RC1 暂停/悬停手柄](../../HarmonyContinuation-rc1-delivery-20261021/screenshots/after-cursor-handle.png) |

手柄图展示暂停悬停状态；前后主列表图的播放状态不同。完整最终渲染仍保留在验证目录。综合结论 **CONDITIONAL**：本轮开发技术门禁通过，等待 RC1 精简 Cubase 验收；尚未执行正式发布。

## dev.15 历史交付身份与结论

结论：**CONDITIONAL**，等待 RC1 UI 的 Cubase 确认。dev.15 五项功能与三项历史契约审计完成；2026-10-21 独立完整 CTest **34/34通过、0失败**。dev.15 Cubase 双输出、MIDI、工程恢复与稳定性由用户报告通过；开发端自动化、模拟宿主与用户验收分别记录。

- 版本：`0.10.0-dev.15` / Release / Windows x64。
- 安装包源码 / BuildCommit：`626f164b48793e2098a6d979cf9b2fac951839f1`（`626f164`）。`848d4b0` 是功能提交，`626f164` 是集中审查修正；后续仅测试、测试配置与交付文档提交，不改变包内容。本轮以 `267ed3a8` 为接续基线，重新编译测试目标；未修改生产源码或重新构建插件、安装器。
- 分支：[development/preview-transport-20261021](https://github.com/OldLai-ctrl/HarmonyContinuation/tree/development/preview-transport-20261021)。本地与远端最终 HEAD 在最终回传中记录；源码提交、分支最新文档提交与构建身份分别核对。
- 唯一 [dev.15 Setup](../../HarmonyContinuation-dev15-delivery-20261021/HarmonyContinuation-0.10.0-dev.15-Setup.exe)，[SHA-256](../../HarmonyContinuation-dev15-delivery-20261021/HarmonyContinuation-0.10.0-dev.15-Setup.exe.sha256)：`354d7b8c8e9a33d857f123e999d4f252a155d3965841a66e32c4d1f57c0d0311`。
- Factory V5 / 658 条 / Schema 2；SHA-256 `f989a83ea9ef92ba7c272bf0e5e3c7073c225899dc81d60dd46c10ea6b48cb3c`。与保留的 dev.14 Factory 字节相同；V4 的 657 条继承数据及独立 15634125 路线通过现有检查。
- Session 5、Snapshot 3、插件 FUID、Fx|Tools 类别和原音频总线不变。安装暂存插件与构建二进制逐字节身份一致；moduleinfo 与包版本一致。未合入 main、未创建 Tag / Release，dev.14 包保留。

关闭 Cubase 后运行 Setup；About 核对 `dev.15 / 626f164 / Factory V5`。Host Audio 需宿主实际处理音频并正确监听路由；无回调时会明确使用或提示切换 Windows Audio。

## 五项功能与状态规则

- 升级高亮：引擎保留原始事件起点，插入只分割原时段。呈现层以完整、有序的原始起点映射识别内部新增；缺失或歧义来源标为未知。原始起点上的低音、转位或延伸变化归为修改，根音及定义性质变化归为替换。主卡片和曲线使用同一映射；不依赖和弦字符串或总数量，不使用会随多次插入漂移的操作索引。此“新增”不同于 Matcher 的“输入插入”。
- 详细卡片：主播放带保留实际库路线、时值、级数及续写边界；原来重复的结果区域显示色彩曲线。输入与来源有替代或缺口时保留真实 Matcher 对照，输入插入和模板跳过继续使用不同颜色及文字。Why 保留来源、用途、约束、匹配轨迹等独立信息。
- 输出：Host Audio 使用原有 Fx|Tools、原 FUID、原单声道/立体声输入输出总线，在原透传输出上混入试听。Windows Audio 使用独立线程上的 waveOut 原生输出，不再调用 PlaySound、生成临时 WAV 或凭墙钟估计播放位置。不直接打开 ASIO。
- 播放、暂停、继续、停止：两个后端共用 PreviewTransport。暂停保持样本位置，继续不重置；停止回到当前范围起点。切换候选或分类、新输入和音乐设置变更停止旧对象；详情、滚动、翻页和总览保留当前试听对象。BPM / 范围修改重新准备配音，保留 QN 并裁到新范围，保持 Playing / Paused。升级进行固定完整范围。
- 定位：卡片进度带及详细曲线支持点击、拖动；RC1 增加整根光标的命中宽度、下端三角手柄及悬停反馈。拖动只画目标指示，释放提交一次；映射使用实际 QN、范围与横向滚动偏移，不按和弦等长推算。暂停选点保持暂停；另一候选选点先准备为暂停，不短暂发声；中途立即恢复应有和弦。到末端停止。MIDI 内容和候选身份不随定位改变。

## 相对色彩指数定义

独立纯函数 `color/RelativeColorCurve` 读取与输出相同的和弦语义，低音、扩展及精确 harmonicData 均保留。所有路线使用同一固定尺度，不按路线的最大最小值拉伸。

`index = clamp(10 + seventh + extensions + altered + special + outsideKey + contrast, 0, 100)`。

| 项 | 固定贡献 | 解释与边界 |
| --- | --- | --- |
| 基础 | 10 | 给明确三和弦保留基线，不表达优劣 |
| 七度 | 12 | b7、maj7 或 dim7 的七度结构；dim7 的第 9 半音不重复当作 13 音 |
| 扩展 | 每类 8，最多 24 | 有三度时计 9、11、13 音；sus2 / sus4 不直接当作扩展 |
| 变化音 | 每类 8，最多 16 | b9、#11/b5、#5/b13；定义 dim / aug 的五度不在这里重复计入 |
| 特殊结构 | dim 20 或 aug 18 | 需要相应三度及变化五度，且不存在普通五度；混合附加变化音另处理 |
| 调外音 | 每个不同音级 10，最多 20 | 相对当前解释的自然大调或自然小调，包含实际低音；不据此判断和声功能正确与否 |
| 相邻变化 | 10 × Jaccard 距离 | 相邻完整绝对音级集合的变化；首项或未知前项为 0 |

这些是可解释的显示权重：将七度、扩展和明显变化结构分层，并限制同类叠加；没有通过调参拟合推荐排名。指数不是好听程度、推荐质量或情绪强度，不反馈到引擎资格或评分。

在 C 大调，单独 C 为 10，Cmaj7 为 22；相同进行与调性一起移调得到相同指数。QN 起点与时值原样映射；未知调性、无法解释的结构或未解析时值留空并中断连接，不能伪造分数。OPEN 使用已确定的实际输出时值；没有确定值时降级。

## 音频线程与后端切换

非实时侧先完成路线构建与配音，最多 128 个事件、600 秒。SDK 消息传输固定大小的完整目标状态：计划、clipId、seekId、serial、Playing / Paused / Stopped。三缓冲通过原子交换转移所有权，单生产者/单消费者；更新不会因合并命令而丢失 load。

音频回调只消费有界计划、调度固定声音、合成并叠加输出；无文件、数据库、消息分配、等待锁或 GUI 调用。采样率准备及音高相位步长计算在非实时设置阶段。32/64 位、单/双声道保持原输入透传；没有试听时输出保持一致。

Host 可用性来自最近实际输出处理回调，不能由“Cubase”宿主名称或消息发送成功推断。默认优先有效 Host；未观察到处理时明确使用 Windows。手动选 Windows 后本实例保持该选择；偏好不写 Session，重开采用安全默认。Host 停止回调会暂停并关闭发声门控，不会静默启动另一后端。

切换先请求旧端停止，确认后才启用新端，保留上次确认的 QN 和播放/暂停状态。正常 Host 等待停止后的处理边界；停止回调时还要求新轮询确认门控为零且没有正在执行的音频回调。Windows 要求关闭设备的停止确认，原生 API 失败不能当作成功。切换异步、有超时，失败明确显示且不启动第二端。Windows 暂停/定位以原生设备样本位置重建，避免队列超前造成重播。

开发端证明的是 SDK 合法性、实际模块输出缓冲和本机原生设备控制；Cubase 监听路由、硬件缓冲尾音及设备切换仍需实机验收。

## dev.15 测试证据（RC1 复用）

| 检查 | 实际结果与范围 |
| --- | --- |
| 本轮完整 CTest | [独立完整运行](../../HarmonyContinuation-dev15-validation-20261021/rc-closeout/ctest-full-final.log)：2026-10-21 13:37:19–13:40:09 +08:00，34注册项、34通过、0失败、164.90秒、exit 0；Release、parallel 2，未筛选测试或累加先前结果。[逐项输出](../../HarmonyContinuation-dev15-validation-20261021/rc-closeout/LastTest-full-final.log)覆盖统一引擎、Factory V5、Preview、宿主契约、Session/Snapshot及MIDI。 |
| 本轮首轮完整运行 | [27/34、7失败](../../HarmonyContinuation-dev15-validation-20261021/rc-closeout/ctest-full-initial.log)，167.21秒；[原始输出](../../HarmonyContinuation-dev15-validation-20261021/rc-closeout/LastTest-full-initial.log)保留。失败集中于受限沙箱Temp的Windows原子替换拒绝；同一34项和parallel 2，在允许执行权限下显式TEMP/TMP设为工作区临时目录后全部通过。未为此改产品或断言。 |
| 初次功能交付历史证据 | [最初30/34](../../HarmonyContinuation-dev15-validation-20261021/ctest-rc.log)及[旧Worker上限修正后的定向通过](../../HarmonyContinuation-dev15-validation-20261021/cubase-contract-final.log)保留。先前累计31/34仅为历史记录；本轮结论使用上面的单次完整34/34。 |
| 本轮契约定向复验 | [三项均通过](../../HarmonyContinuation-dev15-validation-20261021/rc-closeout/contracts-targeted-final.log)；修订依据与保留的历史实验见下节。 |
| 最终改动定向检查 | [Preview、Phase4D、Compact UI](../../HarmonyContinuation-dev15-validation-20261021/ctest-final-changes.log)通过；该日志中旧上限的 Cubase 失败由上一行复验处理。 |
| 音乐与输出 | [8个固定案例 / 3512检查](../../HarmonyContinuation-dev15-validation-20261021/final-music/unified.txt)：R01一条默认与三条探索、R02两种节奏色彩、借用iv、四和弦循环、15634125均保留。308为累计候选，不表示质量提升。 |
| 实际 SDK 模块与原生编辑器 | [最终42/42](../../HarmonyContinuation-dev15-validation-20261021/host-final-native.log)：真实子窗口鼠标驱动 Controller/Processor，Host播放/暂停/恢复/停止、Host↔Windows、回调中断后静音、32/64位单/双声道、2/4/8实例、重建视图、状态和MIDI对象检查通过。属于开发模拟宿主，不是真实Cubase或FL Studio。 |
| 纯播放核心 | 129检查；load / seek / process 无堆分配、暂停样本位置、恢复连续性、范围/BPM/边界、中途恢复和弦、Cmaj7合法配音、20k并发SPSC完整载荷。 |
| Windows 原生设备 | 显式设备检查通过；[120秒压力检查](../../HarmonyContinuation-dev15-validation-20261021/windows-stress.log)349轮 seek/pause/resume，无API错误。其音频代码与最终包一致；未据此宣称无可闻爆音或真实DAW无掉音。 |
| VST3 Validator | [最终47通过 / 0失败](../../HarmonyContinuation-dev15-validation-20261021/validator-final.log)。 |
| 隔离安装生命周期 | [26检查通过](../../HarmonyContinuation-dev15-validation-20261021/installer-lifecycle.log)：新装、组件增删、修复、锁定拒绝、dev.14→15升级、卸载；个人数据、V4历史及非托管文件保留。使用独立Test AppId，在工作区用保留的dev.14载荷重编译隔离安装器，不操作真实安装或个人库。 |
| 原生界面/来源输出 | [R01](../../HarmonyContinuation-dev15-validation-20261021/final-R01.log)、[模板跳过](../../HarmonyContinuation-dev15-validation-20261021/final-skip.log)、[七和弦来源](../../HarmonyContinuation-dev15-validation-20261021/final-source.log)通过。七和弦28QN、配音×范围共6种MIDI及冻结Snapshot一致。0/1/3/4/10/11/20/21分页及100/125/150%用户缩放沿用并通过当前原生交互用例。 |
| 开发端性能 | 最终SDK每种声道/位深4000块，256帧/48k预算5.333ms；平均0.024–0.028ms、观测最大0.7903ms。浏览55次绘制平均6.306ms、最大17.247ms；七和弦场景最大22.201ms。是本机开发测量，不是Cubase ASIO CPU或音频中断验收。 |

最终 [6张原生VSTGUI截图](../../HarmonyContinuation-dev15-delivery-20261021/screenshots/) 包含新增、详细曲线、R01对齐、模板跳过、七和弦来源和推荐总览；截图来自开发渲染夹具，部分音频按钮无接收器而禁用，不冒充Cubase运行截图。

### 已修订的三项历史契约

原始三项失败已[重新复现并保留](../../HarmonyContinuation-dev15-validation-20261021/rc-closeout/original-contracts.log)。修改范围限现有两个测试程序及其 CMake 测试配置，没有禁用测试、删去关键 MIDI/轨迹断言或修改60分质量门槛。

**HarmonyAnalysisTests。** 从 dev.14 `b4e6f65` 与 dev.15 `626f164` 的独立源码归档实际编译和执行同一测试。dev.14 原测试[复现相同A7失败](../../HarmonyContinuation-dev15-validation-20261021/rc-closeout/dev14-original-contract.log)；修订后两版本均[dev.14通过](../../HarmonyContinuation-dev15-validation-20261021/rc-closeout/dev14-revised-contract-final.log)、[dev.15通过](../../HarmonyContinuation-dev15-validation-20261021/rc-closeout/dev15-revised-contract-final.log)。

| 事件与上下文 | 时值 / 分析权重 | Legacy 检索与骨架 | Unified 检索与骨架 |
| --- | --- | --- | --- |
| C–Am–A7–Dm–G7–C 中的 A7，C大调 V7/ii | 1QN；中位4QN；0.340 | 0.340，骨架外 | 0.720，骨架内 |
| C–C#dim7–Dm–G–C 中的 C#dim7，vii°7/ii | 0.5QN；0.300，低于目标Dm的0.857 | 0.300，骨架外 | 0.720，骨架内 |
| Dm–G7–C 中的终止 G7 | 1QN；0.760 | 0.760，骨架内 | 0.880，骨架内 |

A7分析权重为 `0.66 + 0.06 - 0.12 - 0.13 - 0.13 = 0.34`，分别含功能、相对时值、短次属及趋近影响。已批准的Unified检索解耦保留 `0.66 + 0.06 = 0.72`，达到0.55结构阈值；分析仍保留Approach与V7/ii目标，检索角色去掉Approach。A7的C#向Dm的D解决支持次属解释，短时值仍降低分析权重。原有Legacy可省略、长A7可结构化、短终止V7保留与时值缩放断言全部保留；为Unified新增独立契约。两版本[逐事件A7](../../HarmonyContinuation-dev15-validation-20261021/rc-closeout/dev14-secondary_dominant-final.log)、[次导音](../../HarmonyContinuation-dev15-validation-20261021/rc-closeout/dev14-short_approach-final.log)、[短V7](../../HarmonyContinuation-dev15-validation-20261021/rc-closeout/dev14-short_dominant-final.log)与对应dev15轨迹一致；这些事件colorMask为0且根音等于低音，没有扩展或转位保护干扰。

**RecommendationReliabilityFocused。** 现有1440格历史矩阵明确加载冻结 `factory-v4.db`、断言V4/657并保留原检查；生产V5另行加载并断言V5/658。30次真实Worker/直接Unified调用使用同一正式权重配置，对比完整匹配解释、音乐身份、来源、硬约束、资格及排序分。现有 `FactoryV5Focused` 在完整回归中验证657条继承payload逐条一致、独立15634125路线、12调召回/同分、版本化store及降级保护。

继续执行657断言之后的检查，修正了三类旧前提：模板内续写位置改为完整Matcher/evidence映射；音乐候选按来源证据查找，使用music-v1身份；原来固定Loop/输出数量/绝对分数改为实际资格、真实用途与补偿差分。保留Am–Am–D精确路线与匹配轨迹、负例不返还、合法正例、改名不影响资格、破坏轨迹及无后续检查及0/1/8/12点单事件惩罚。FullPhrase七事件与ContinuationOnly三事件均核对音高、低音、4QN和相邻Am不合并；相同音乐候选在有限/完整保留下的两种MIDI载荷字节一致。该旧直接导出夹具使用输入加后续；正式完整来源输出另由既有Factory/Phase4D检查覆盖。

**RecommendationRhythmDecoupling。** Legacy、BarPositionIndependent、ContinuousStructure、HarmonyFirst作为显式历史测试调用保留，使用冻结V4数据；产品仍只执行Unified。新增331组隐式默认/显式Unified正式配置一致性检查；保留331次SMF往返、1324次历史模式调用、33组×两模式×OFF/ON的132次实验，以及505项诊断和13250次精确匹配补偿比较。本次诊断关联歧义为0。匹配关联包括调性、窗口、续写位置与轨迹，避免把同一模板的不同调性解释拼接。

旧Legacy固定57.40387是完成度微调前的中间分；当前最终MatchResult为57.60387，OFF/ON仍相差8。断言改为同一实际解释的资格/排序差分及退款上限，保留事件惩罚抵消性质。Phase3A补偿仅在测试显式开启；正式Worker继续OFF。没有修改当前音乐输出、分类、权重、Factory、Schema或生产算法分支。

### 更新后的 RC 门禁

| 门禁 | 状态与依据 |
| --- | --- |
| 完整34项工程回归 | PASS，单次34/34，0失败；受限Temp失败日志单独保留 |
| 三项历史契约及有价值的实验性质 | PASS；跨版本A7、V4历史数据、V5正式Worker与Unified默认明确分开 |
| Factory V5、Session/Snapshot、MIDI | PASS，本轮完整回归；数据/格式和生产源码未修改 |
| 开发端音频 | PASS，复用同一626f164的SDK模块42/42与Windows原生设备/120秒349轮证据，本轮Preview及宿主契约完整复验；无新增设备听评 |
| dev.15 Cubase双输出、MIDI、工程恢复与稳定性 | PASS，用户在本轮明确报告；不视作 RC1 三项 UI 的验收 |
| RC1 三项 UI 的 Cubase 复验 | Pending；使用下方精简清单 |
| FL Studio实机 | Pending |
| 正式发布 | 未执行；无main合并、正式Tag或Release |

dev.15 门禁收尾由主代理决定音乐语义与旧契约取舍，并集中复核测试差异、实际日志和门禁；LUNA负责隔离复现、测试维护及重复执行。该工程门禁已完成；当前整体RC为CONDITIONAL，待 RC1 精简 Cubase 验收。

## Standards

独立只读审查发现1项硬分层问题：UI内的和弦来源分类应属于音乐领域。已移至 `enrichment/EnrichmentProvenance.h`，视图只消费标记。另将逐样本sin/asin列为性能热点候选；当前测量未超过块预算，没有据此扩大DSP重写。未发现其他明确RT所有权或SDK合同问题。

## Spec

独立只读审查发现2项呈现缺口，均已修正：主卡片路径加入逐事件新增/替换标识；OPEN补全时值显示“OPEN ≈”和曲线规划时值提示。复核撤回了一个索引误报：升级播放路线就是候选进行的同序复制，不是原查询数组。原始2项均由主代理复核修正，不改变音频或MIDI。

两轴记录：Standards 1项已修正、1项性能观察有实测；Spec 2项已修正。主代理负责架构、RT门控、集成、证据与RC结论；Luna承担纯模型/测试/Demo补丁及独立审查。

## Cubase 人工复验（RC1 Pending）

关闭 Cubase 后安装唯一 RC1 包，About 核对 `0.10.0-rc.1 / 435ed85 / Factory V5 / 658`。仅复验：

1. 光标与下端手柄容易发现；整根光标可拖动，进度带可点击；横向滚动及暂停后定位正确。
2. 升级进行插入为暖色，名称保持纯和弦名；简略/详细模式高亮一致。
3. 等时值与不等时值进行的曲线节点位于和弦起点，播放指示与时间轴一致。
4. 普通推荐和升级进行的试听、MIDI、工程恢复无明显回归。

FL Studio 实机仍为 Pending。用户明确确认 RC1 通过后，按本轮有条件授权继续正常合并 main、正式构建、Tag 与 Release；确认前停在 RC1 交付。dev.15 及更早安装包保留，Factory V5 与个人库不迁移。
