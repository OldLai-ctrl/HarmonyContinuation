# dev.15 与 v0.10.0 RC 技术评审

本轮基线为 dev.14 / b4e6f65。用户明确报告 dev.14 已在 Cubase 全部验收通过；该结论不转移到 dev.15 新音频路径。

## 交付身份与结论

结论：**CONDITIONAL**。五项功能及其集成检查完成，可评审本开发包；仍有三项历史测试契约未通过，不能写为 RC 门禁全绿。Cubase 新音频路径与人工听评仍为 Pending。

- 版本：`0.10.0-dev.15` / Release / Windows x64。
- 安装包源码 / BuildCommit：`626f164b48793e2098a6d979cf9b2fac951839f1`（`626f164`）。`848d4b0` 是功能提交，`626f164` 是集中审查修正；后续只有测试与交付文档提交，不改变包内容。
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
- 定位：卡片底部细进度带及详细曲线支持点击、拖动。拖动只画目标指示，释放提交一次；映射使用实际 QN、范围与横向滚动偏移，不按和弦等长推算。暂停选点保持暂停；另一候选选点先准备为暂停，不短暂发声；中途立即恢复应有和弦。到末端停止。MIDI 内容和候选身份不随定位改变。

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

## 测试证据

| 检查 | 实际结果与范围 |
| --- | --- |
| 完整相关 CTest | [首轮 34 项](../../HarmonyContinuation-dev15-validation-20261021/ctest-rc.log)：30 通过、4 失败。没有用定向成功覆盖这个结果。 |
| 历史 Worker 对照兼容修正 | 其中 CubaseReliabilityFocused 的 direct 调用仍限制每类10条，Worker 自 dev.14 已输出全部合格结果，产生8个计数差异。只将测试上限对齐生产配置，不改产品、断言或音乐规则；[定向复验通过](../../HarmonyContinuation-dev15-validation-20261021/cubase-contract-final.log)。其余3项仍失败，因此累计31/34，未重复完整34项。 |
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

### 剩余三项回归契约

1. `HarmonyAnalysisTests`：要求短A7不进入骨架，当前分析结果保留它。测试及核心分析代码在dev.14基线已是同一版本；没有单独重建dev.14复现，不把“本轮无diff”说成旧版测试通过。
2. `RecommendationReliabilityFocused`：仍硬编码Worker Factory条数657，V5实际为658；该过期断言之后还包含历史矩阵，本轮不改门槛或重做阶段审计来补绿。需版本化测试契约并确认余下分支。
3. `RecommendationRhythmDecoupling`：明确要求Legacy显式调用等同当前默认，统一引擎默认与历史Legacy的差异使断言失败。旧实验对照需要与正式引擎门禁分开定义，不能重新引入三算法或恢复旧音乐规则。

这些未通过项必须被明确处理后才能称为技术门禁全绿。涉及音乐策略的结论交决策端确认；本轮不调整核心评分、骨架、Factory或资格策略。

## Standards

独立只读审查发现1项硬分层问题：UI内的和弦来源分类应属于音乐领域。已移至 `enrichment/EnrichmentProvenance.h`，视图只消费标记。另将逐样本sin/asin列为性能热点候选；当前测量未超过块预算，没有据此扩大DSP重写。未发现其他明确RT所有权或SDK合同问题。

## Spec

独立只读审查发现2项呈现缺口，均已修正：主卡片路径加入逐事件新增/替换标识；OPEN补全时值显示“OPEN ≈”和曲线规划时值提示。复核撤回了一个索引误报：升级播放路线就是候选进行的同序复制，不是原查询数组。原始2项均由主代理复核修正，不改变音频或MIDI。

两轴记录：Standards 1项已修正、1项性能观察有实测；Spec 2项已修正。主代理负责架构、RT门控、集成、证据与RC结论；Luna承担纯模型/测试/Demo补丁及独立审查。

## Cubase 人工复验（dev.15 Pending）

1. 关闭 Cubase 后安装唯一 dev.15 包，About 核对版本、BuildCommit、Factory V5 / 658。
2. 在现有可输出音频的轨道加载原插件并启用必要监听；观察 Host Audio 默认选择，试听经过 Cubase 路由；关闭处理后应提示暂停，可切 Windows。
3. 两后端分别暂停、继续、停止；在和弦中途、边界、末端定位；Full / Suffix、自定义 BPM、非 4/4 和 OPEN 对齐。切换后端无持续双声，位置规则可预测。
4. 比较升级新增、替换、扩展和转位标记及曲线；R01 默认与三条探索、R02 两种节奏色彩、15634125、借用 iv、七和弦完整来源及分页/总览保持可用。
5. 播放时开关详情、滚动/翻页、切换候选及分类；试听、完整/后续 MIDI、拖放、Session 重开和旧 Snapshot 指向正确对象。
6. 多实例、切换音频设备、关闭/重开工程及退出 Cubase，确认无残留声或挂起。人工听评包括新增结构、曲线直觉、音色、和弦切换及定位处的点击声/尾音。

FL Studio 实机仍为 Pending。正式版号、正式包及发布决定不在本轮执行；dev.14 安装包和分支保留用于回退，Factory V5 与个人库不迁移。
