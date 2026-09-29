# v0.6.0-rc.1 Cubase 人工验收

状态：**MANUAL TEST REQUIRED**。自动化、Demo 和 SDK Validator 不能代替 Cubase 15 实测。请保存工程后安装 RC 构建，逐项记录通过、失败及复现步骤；遇到卡死或闪退，保留工程、操作顺序和崩溃日志。

| 区域 | 操作与通过标准 |
| --- | --- |
| 安装 | 确认实际加载的是 `0.6.0-rc.1`；安装目录的插件二进制、`factory.db`、语言资源及 `moduleinfo.json` 的 SHA-256 与本次 RC 构建一致。 |
| 语言 | 首开默认简中；进行库短名称可读、无明显异常英文；切到 English 并重开窗口确认生效。Rock、Major、Minor、Jazz、R&B、City Pop、MIDI、FULL、SKELETON 可保留。 |
| 和弦轨 | 拖入带绝对 QN 位置的进行；当前乐句、各和弦时长和播放指针与 Cubase 和弦轨对应。 |
| 继续发展 | 检查收束、发展、循环、色彩四组；分别操作 Play、Pin、Compare、MIDI、Why?，确认结果与界面状态一致。 |
| 升级进行 | 对简单大调、次属/经过减、简单小调进行检查润色、丰富、进阶；检查技巧标签、Why?、Preview 和 MIDI。 |
| 进行库 | 搜索名称、完整公式/alias；切换筛选条件；确认 161 条 Factory 短名称；新建、重命名用户进行，设置用户标签、Style、Intent、Favorite、备注并重载，确认数据保留。 |
| 窗口 | 在 Cubase 中拖动窗口大小，检查 Compact / Standard / Wide、DPI 缩放；可用时跨屏幕，确认无空白、卡死、错位。 |
| 工程恢复 | 保存、关闭并重开 Cubase 工程；检查导入进行、语言、模式、调性、Style、Intent、Pins 和窗口大小的合理恢复。 |

## 人工试听清单（18 例）

每例先听默认首选，再比较同组第二候选与跨组结果；分别记录和声是否完成意图、重复感、声部进行，以及预览音色问题。**一步建议不自动判失败**；例如 `ii–V → I` 可由一个 I 完成收束。

| 继续发展（12） | 关注点 |
| --- | --- |
| `bench_031`–`bench_034` | 短输入与单和弦建议的意图完成度 |
| `bench_016`, `bench_019`, `bench_028` | 借用和弦、降七/降六、City Pop / R&B 色彩 |
| `bench_020`, `bench_024`, `bench_035` | 同组重复路径、跨组区分 |
| `bench_037`, `bench_038` | 异常和弦是否被过度普通化 |

| 升级进行（6） | 关注点 |
| --- | --- |
| `001_major_pop_1564`, `027_rock_simple` | 润色：简单四和弦与 Rock |
| `009_passing_dim`, `010_secondary_target` | 丰富：经过减与次属和弦 |
| `012_borrowed_four`, `016_inversion` | 进阶：借用和弦与转位；另用 `006_minor_pop` 或 `021_rnb_cadence` 替换其中一例检查 Minor / R&B |

试听结论只作为后续音乐质量评审输入；本 RC 不据此调整权重或规则。
