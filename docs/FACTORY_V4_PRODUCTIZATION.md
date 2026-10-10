# Factory V4 开发候选版产品化

版本：**0.10.0-dev.1**，分支 `library/v4`。Factory 4 / 657 条，629 条继承及 28 条有效新增。Factory / User / Session / Continuation / Enrichment Schema 仍为 **2 / 2 / 5 / 3 / 2**。正式 v0.9.0 的 main、Tag、安装包和 V3 源数据不变。

## 可发现性与产品路径

`FactoryV4Tests <output-directory>` 批量构造 28 个带技法特征的前缀，使用模板首个声明 Style、对应 Intent、C 大调或 C 小调指定调性，末和弦 OPEN。保留上限为 RecommendationWorker 实际使用的每组 10 条，通过界面共用的 `presentationIndices` 判定默认显示；其余候选可由现有 More candidates 菜单访问。没有把前 100 条诊断结果当成产品可访问结果，也没有置顶 V4 或改变全局排序权重。

结果：**26 DEFAULT_VISIBLE / 2 MORE_ACCESSIBLE / 0 BLOCKED**。A11 和 B09 在对应组第 4，均可通过 More candidates 访问。B09 无 Style 的既有输入仍为第 5，使用声明的 Pop Style 为第 4；默认前三不可见属于正常排序。第一次导入后仍自动显示完整的第一批推荐。

逐条前缀、Style 枚举、Intent、组内排名、完整后续和状态见随包 `evidence/discoverability.tsv`。Style 数值：Pop=1、Rock=2、R&B=4、Jazz=8、City Pop=16、Functional=32。本轮范围为这些指定输入；不保证任意片段或自动调性推断都命中同一模板。真实 DAW 菜单点击由人工清单验收。

首轮发现 V4 的保守完整骨架与通用输入简化不一致，A06/A08/A09 的有效节点被输入骨架删除。局部修正仅对 version>=4 且骨架包含全部节点的模板使用输入 FULL 路径作结构比较，保留 V3 的匹配方式。B02 使用 C–B7 这个更早且含主音化证据的前缀，保留 Em–A7–Dm–G7–C 完整后续；C–B7–Em 在 Jazz 下会受现有候选多样性规则影响，不为该单个前缀修改排序。

全部 28 条逐个核对后续根音、低音、音集合和长度，构建 Preview、渲染非静音、导出 VoiceLed FullPhrase MIDI 并读回音高及时值；循环 A08 采用原有循环语义，在后续末尾附回起点。四个试听文件来自同一实际候选路径：B01 连续属链、B08 下降低音、B09 上行低音、B11 Side-slipping。均为 120 BPM / 4/4，含输入及后续。

原有定向契约另覆盖 D 调移调、负例低音冲突、精确音集合区别、双语 Why、Snapshot、安装旧库时开发 bundle 选择。候选标题沿用实际 Slash Bass / 扩展和弦显示。没有改变 Color Harmony 数学公式、分析或排序；Unknown / Uncertain 继续有效。

## 音乐与标签审核

这是源码、音集合、低音线和功能关系审核，**没有人工听审**。NeoSoulCandidate、GospelCandidate 仍为编辑性建议，不等于风格认证。共同和弦性质不声称严格 Planing；共享音不声称固定声部；主音化不声称正式转调。

本轮调整：A03 去除证据不足的 CommonToneConnection 标签及说明；Em 与 Ebdim7 没有共同音，保留下降减七连接身份。A03/A04/A05/A06/B09 的短减七采用前和弦 3 拍、减七 1 拍、目标 4 拍，移到弱拍进行连接（A06 两个连接连续按此规则布局）。未改变根音、和弦音、低音或数量。

| ID | 核查结构与结论 |
| --- | --- |
| A01 | C–Db–Dm–Eb–Em–F–G–C；前段根音半音上行，回归完整 |
| A02 | C–B–Bb–A–Ab–G–C；根音下降，末尾回归不声称继续半音下降 |
| A03 | C–Em–Ebdim7–Dm–G7–C；下降经过减七，去除共同音标签 |
| A04 | C–Am–Abdim7–G7–C；下降减七，与 G7 共享 B/D/F，不标副导 |
| A05 | C–F–F#dim7–G–Am–Dm–G7–C；副导目标 G，欺骗到 Am 后回收 |
| A06 | C–C#dim7–Dm–D#dim7–Em–G7–C；两个副导分别指向 ii、iii |
| A07 | C–Eb–F–Fm–C–G7–C；借用与小下属解决 |
| A08 | Cm–F–Cm–Bb–F–Cm；大 IV 含自然六级，Dorian 色彩，不声称建立新调 |
| A09 | Cm–Db–Ab–G7–Cm；根位降二级属前区域，区别于 N6 |
| A10 | C–F–G7–Ab–Bb–C；属七绕经借用降六级，调式式回归 |
| A11 | C–Dm–G7–Fm–C；回避属到主，转小下属结束 |
| A12 | C–F–G7–Am–Dm–G7；欺骗后再次开放属结尾 |
| A13 | C–Eb–Ab–E–G–C；色彩三度关系与功能性回归，无影视风格认证 |
| A14 | C–Fm–Ab–Db/F–G7–C；bII 第一转位，F 低音的 N6 |
| B01 | E7–A7–D7–G7–C；每个属七按实际下一根音指定目标 |
| B02 | C–B7–Em–A7–Dm–G7–C；分别临时主音化 iii、ii |
| B03 | C–E7–A7–Dm–G7–C；属链最终进入小 ii |
| B04 | Cmaj7–Em7–Eb7–Dmaj7–Dm7–G7–Cmaj7；Eb7 替代 A7 指向 D |
| B05 | Cmaj7–Am7–Ab7–Gmaj7–G7–Cmaj7；Ab7 替代 D7 指向 G |
| B06 | Cmaj7–Em7–A7–Dm7–Fm7–Bb7–Cmaj7；副属接小下属 Backdoor |
| B07 | Cmaj7–Gm7–C7–Fmaj7–Fm7–Bb7–Cmaj7；IV 主音化后 Backdoor |
| B08 | C–Cmaj7/B–C7/Bb–F/A–Fm/Ab–C/G–G7–C；C/B/Bb/A/Ab/G 定义性低音完整 |
| B09 | C/E–F–F#dim7–G–Am–G/B–C；E/F/F#/G/A/B/C，仅开头段为半音 |
| B10 | C–F/C–Fm/C–C–G7/C–C；所有低音为 C，Pedal Point |
| B11 | Cmaj9–Dbmaj9–Cmaj9–Am9–Dm9–G13–Cmaj9；精确移出及返回，Db 不标属前借用 |
| B12 | Cmaj7–Ebmaj7–Dbmaj7–Cmaj7–Fmaj7–G7–Cmaj7；同性质连接，不保证固定配器 |
| B13 | C–C7/E–F–Fm/Ab–C/G–G7–C；带转位的小下属路线，Gospel 编辑性候选 |
| B14 | Cmaj9–E7/G#–Am9–Ab13–G13–Cmaj9；局部主音化、A/Ab/G 下降，Neo-Soul 编辑性候选 |

13 和弦的明确音集合为根音、9、3、5、13、b7，不额外强制加入 11；这是作者指定的省略配器，测试不删除这些定义音。等音显示可能使用升号（如 Ab 显示 G#），听觉音高等价；审核表按功能记谱。

## 安装与兼容

统一 Inno Setup 保持两个独立组件：VST3 和 Factory Library V4 (657)。包名含 0.10.0-dev.1；安装前说明明确开发候选版及人工验收待定。只选 V4 时，缺少插件、未知版本、版本低于 0.10.0-dev.1 或同时要求移除插件均阻止安装并提示升级。两个组件一起安装匹配引擎；仅升级主程序保留活动旧库。编译脚本校验库版本与插件生成头一致。

版本检测复用已安装 moduleinfo.json 的 SemVer；这是现有安装管理契约，不是对任意第三方篡改安装的二进制能力证明。运行路径与占用检测、事务、修复、已知旧版迁移、所有权保护均沿用统一安装器。V4 使用版本化 Factory/4，保留历史 Factory/3；同版本不同内容拒绝覆盖，不覆盖历史备份。User Library、收藏、配置不进入卸载删除清单。

定向隔离验证由 `tools/test-v4-installer.ps1` 使用相同安装器源码和 payload，仅将路径与卸载注册重定向到工作区/HKCU；覆盖无插件及真实 v0.9.0 旧插件拒绝、只更新主程序保留 V3、兼容引擎只装 V4、占用拒绝、配套修复及卸载保留个人哨兵和旧库。既有完整旧版迁移生命周期记录继续复用，未重跑全部矩阵。隔离验证不能替代真实管理员安装和宿主验收。

## 产物、验证与剩余边界

开发包目录：`build-installer/v4-0.10.0-dev.1/`；Setup、`stage-4/HarmonyContinuation.vst3`、`stage-4/factory.db`、`evidence/`、`midi/`、`BUILD_INFO.json`、`SHA256SUMS.txt`。数据库及插件源构建位于 `build-v4-dev/`。构建提交与文档交付提交分别记录，不将后续文档提交冒充编译身份。

继承记录维护保持上一轮数量：139 条，包含 138 条技术列表变化、57 条空列表补全、10 条副导语义修正、13 条骨架恢复（计数重叠）；本轮新增记录调整 6 条。V3 原文件不改，旧 629 条音乐身份保留，精确重复及新增结构族重复均为 0。

保留 C01–C08：固定内声部/Line Cliché、指定声部共同音、严格 Planing、枢纽双功能、分段调性、真实转调、跨乐句解决及相关声部表达能力。定义见 `data/review/factory-v4-capabilities.json`，不宣称本包已支持。Enrichment 沿用明确低音/音集合/连接保护，不能安全升级时允许无候选，不调用 Factory V4 进行推荐。

本轮仅执行 Factory 批量定向、相关 Matcher/Continuation/Enrichment 检查、必要 Release 构建及安装定向验证。没有全量 CTest、Validator、音乐 Benchmark、DAW 矩阵。Cubase 和 FL Studio 对本开发二进制均 **Pending**；风格听审 Pending。下一步仅为用户人工验收，不自动开展 v1.0 或特色卡池。
