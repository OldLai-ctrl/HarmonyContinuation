# G13.1 语料来源、许可与乐句语法

## 研究范围

2026-11-09 用户确认：本轮为独立非商业研究实验，商业产品集成另行取得授权。研究对象是结构化和弦、调性、节拍和结构标注；不下载作品音频，不训练生成式音乐模型。生成器的工程实现和试听门禁见 [技术报告](G131_PHRASE_GRAMMAR_REVIEW.md)。

## 来源与许可核查

| 来源 | 本次核实的许可与数据条件 | 研究、提取和产品集成边界 |
| --- | --- | --- |
| [IdolSongsJp](https://huggingface.co/datasets/imprt/idol-songs-jp) | 15首原创日系偶像流行歌曲；chords含起止秒和Harte标签，keys含调性及转调，两者由至少两位专家参与校订。文件访问需要账户持有人接受条件。 | [LICENSE.md](https://huggingface.co/datasets/imprt/idol-songs-jp/blob/main/LICENSE.md) §1.1允许非商业学术研究，商业研究及商业产品/服务集成须事先获得版权方许可。接受访问条件不等于商业授权；程序性规则提取仍须遵守用途许可。 |
| [RWC 2.0 annotations](https://github.com/rwc-music/rwc-annotations) | [仓库许可](https://github.com/rwc-music/rwc-annotations/blob/main/LICENSE)为CC BY-NC 4.0。当前主仓RWC-P已整理的标注包括chords和beats；README未列出已整理的key或structure。 | 本轮按已确认的非商业研究范围分析，保留归属和修改说明。标注与派生统计独立存放；商业集成须另行解决NC许可，不把公开下载等同于无限制产品使用。 |
| [McGill Billboard 2.0](https://ddmal.ca/research/The_McGill_Billboard_Project_(Chord_Analysis_Dataset)/) | 官方以CC0提供标注；公布890个榜单采样槽、740首不同歌曲，完整文本包含和弦、结构与时间。 | 可以开展统计及形成产品先验，按官方要求的学术规范引用。区分采样槽和歌曲，不能把同曲重复采样当作独立风格证据。 |
| [ChoCo](https://github.com/smashub/choco) | 聚合多个来源的和弦语料。 | 只在出现具体覆盖缺口时选择子集，并核实该子集许可；不凭聚合仓库的许可替代原始来源条件。 |

上述核查针对公开的具体文本，不构成对未提供的单独授权协议的推断。研究产物保留来源、样本数、转换条件与未知字段，后续商业集成须按实际使用的数据和派生物再次核实。

## 标注、推断和解释

- 和弦标签、起止时间、显式低音、原文调性及结构标记分别保留；音级归一化、按拍网格换算时值属于程序变换。
- 缺少可靠调性或结构标注时保持未知；若使用调性估计、固定小节窗口或结构边界推导，明确记录其推断性质。
- 结构段落不自动等于音乐乐句，固定窗口不自动等于经过专家标注的前后句。
- 频数描述当前样本；生成器中的平滑、变奏预算及搜索权重属于工程选择，不能写成语料直接给出的规则。
- Billboard支持跨风格流行语法参考；日系、ACG、City Pop的具体结论须由相应来源证据支持。

## 实际样本与可追溯语法

本轮已读用户提供的IdolSongsJp全部15份chords、15份keys及LICENSE；共2,884段标注，其中2,842段可完整落入一个已标注调性区间。其余包括N或跨调性边界，未跨越这些边界统计条件连接。标注时值以秒保留，未提供beat、phrase或section文件，因此不推断精确谱面拍数或专家乐句边界。文件SHA256、事件与调性段记录保存在同级研究目录的 `derived/idol_nc_research/`。

运行先验 `idolsongsjp15-major-billboard-rhythm-research-v1` 使用Idol明确标为大调的连续同调段，保留一阶root+family、二阶root连接；相邻同根同家族段合并。maj包含maj7，min包含min7等扩展，完整标签另行保留。时值和句尾位置回退使用Billboard的CC0聚合，未把它们标成Idol乐句证据。当前频数保留歌曲内重复，并同时记录歌曲数；不是歌曲等权统计。两来源没有把事件计数混在一个分母中。

| 语料 / 目标条件 | 目标进行次数 / 歌曲数 | 观察到的后继（C参考） |
| --- | --- | --- |
| IdolSongsJp，标注明确大调、连续同调，IV–V–iii–vi家族 | 56 / 8 | IV 26次/4首；ii 25次/7首；I 3次/2首；♭VII7 1次/1首；♭VI 1次/1首 |
| RWC日语子集，根音与性质轮廓匹配，主音为推断 | 35 / 7 | ii 23次/6首；IV 12次/3首 |
| Billboard，标注tonic已知、4/4、mode未知 | 27 / 6 | ii 13次/3首；V 10次/2首；IV 3次/1首；♭VII 1次/1首 |

RWC实际取用100首的chords与beats（80首Japanese/J-pop、20首English/Pop），13,467段；有限结构参考另取目标命中七首。Billboard去重740首，19,365个SALAMI phrase行，其中17,284个完整、tonic已知的4/4行用于时值与句位统计，66,653个和声变化段用于短上下文。去重映射、提取方法、八项字段的可用与未知范围、结构重复/变形实例见[研究摘要](../../HarmonyContinuation-g131-corpus/RESEARCH_SUMMARY.md)。RWC archive内部旧README另有研究限定及禁止未经许可分发条件，原始和逐曲派生资料保留在本地研究目录，未纳入运行先验。

### 对生成器有作用的结论

1. **入口与后续关系**：三库对ii/IV的支持为vi之后的后句入口提供依据，跨库分母保持独立。Idol `f01-intro_juice` 第11行、9.042秒，D大调标注下出现G–A/b7–F#m7–Bm，后接Em7；第107行、101.950秒同类轮廓后回G。说明接ii与回IV均有具体日系实例。
2. **有准备的色彩**：Idol大调段原始相邻标注中I7→IV有26处、7首；如 `f01-intro_juice` 第28行、23.241秒的D7→G，所在key为D:maj。大胆档据此允许一次次属准备并立即抵达目标。生成的Am→C7→F完整三连并未在本次Idol原始相邻标签中出现，是两条受支持连接的理论组合，不称作品原句复现。
3. **结构保持与变尾**：Billboard `0501` 第10→11行以及第17→18行提供同段相邻句保持前三家族、改变尾部的实例；`0389` 第15/16行同bridge重复，第18/19行带撇号的变形结构。它们支持“先保持结构关系再局部变奏”，不是本轮E的直接来源。
4. **节奏**：Idol秒时值是真实标注时间，BillboardQN是符号换算；本轮4小节回答、至多一次半小节细分、终点稳定家族和色彩预算属于工程选择。没有把4小节窗口冒充Idol专家标注乐句。
5. **调性**：Idol有39个相邻标注调性变化，可定位到起止秒。例如 `f01-intro_juice` 在50.335秒D:maj→E:maj、82.446秒返回D:maj。现版本只在所选大调中心内生成，未据此实施或验收转调；RWC无key，结构(+1)注记不等于完整转调路径证据。
6. **低音**：显式slash是标签证据，无slash为标签默认根音；真实录音低音音区未知。本轮低音音区由规划器根据和弦内音、最大跳进和句尾根音位置生成，属于工程配音。

Idol少量样本能支持具体日系实例；RWC提供独立比较；Billboard提供更大范围的流行参考。本轮未证明ACG或City Pop子风格的整体覆盖，未为增加规模使用ChoCo。

## 复算与许可隔离

研究目录为同级 `HarmonyContinuation-g131-corpus/`：`analyze_billboard.py`、`analyze_rwc.py`、`summarize_evidence.py`和`analyze_idol.py`是本轮必要的文本解析与统计工具。Idol工具读取用户本机提供的文件；跨设备运行先调整该输入目录。原始文件hash、原始标签、秒时值、低音来源、未知字段和局部实例均保留。`derived/g131_prior.tsv`是单独CC0先验；`derived/idol_nc_research/idol-research.tsv`才是本次试听使用的混合来源研究先验。后者与许可副本随本地试听交付，未推送、发布或用于产品构建。

## 引用

- Hitoshi Suda et al. *IdolSongsJp Corpus: A multi-singer song corpus in the style of Japanese idol groups*. ISMIR, 2025.
- Masataka Goto, Hiroki Hashiguchi, Takuichi Nishimura, Ryuichi Oka. *RWC Music Database: Popular, Classical and Jazz Music Databases*. ISMIR, 2002, pp. 287–288. RWC 2.0的整理与各标注来源以[仓库说明](https://github.com/rwc-music/rwc-annotations)为准。
- John Ashley Burgoyne, Jonathan Wild, Ichiro Fujinaga. *An Expert Ground Truth Set for Audio Chord Recognition and Music Analysis*. ISMIR, 2011, pp. 633–638.
