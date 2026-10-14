# Phase 5.1 集中 Cubase 验收

本清单由用户在真实 Cubase 中执行，当前尚未完成。继续复用 dev.6 / Build ID `3a582e4` 的三种包；增补包只含诊断、试听和测试资料。完整结论见 [Phase5.1报告](PHASE51_CLOSEOUT_20261014.md)。

## 安装与记录

关闭宿主并备份旧完整 VST3 bundle、用户库和工程。三个包共用插件身份，每次只保留一个供扫描：DEFAULT-OFF=HF/P3/Abstract 0/0/0；HF-ON=1/0/0；ABSTRACT-ON=0/0/1。更换后重启宿主，在 About 核对版本、Build ID、Factory V4/657 和开关；分析前 PENDING，分析后再核对实际状态。P3 本轮始终 OFF，没有提供联合开关插件包。

每个案例记录输入文件/来源、拍号、BPM、各 startQN/durationQN、OPEN、Auto/固定调性、实际包、默认卡/更多卡、Factory ID、分数和诊断文件。SMF 的末音有限；导入时用“作为未完成进行”区分 OPEN，再从诊断确认末 durationQN 确实缺失，不能只看文件名。

## 一次集中检查

| 场景 | 操作与观察 |
|---|---|
| R02 最小对照 | F→G→Am，4/4，等时4/4/4QN与3/5/4QN；各自分别Closed/OPEN。先DEFAULT-OFF，再HF-ON；Auto、C major、A minor分别记录。对照增补包24格离线结果，核对五个V3_SHADE_002/008/019/024/031的匹配调性、G插入轨迹、淘汰阶段和真实后缀。保存成功/失败各一份本地诊断。 |
| Abstract 主卡替换 | 固定C major、Closed，C–E7–Am（4/4/4QN）与C–Am–A7–Dm–G（4/2/1/2/2QN）；比较DEFAULT-OFF/ABSTRACT-ON。第一例涉及更多列表的rank10→退出，第二例涉及默认rank3→4，不能将两例都称作默认Top3替换。先盲听增补包匿名WAV，再揭示ID/开关映射；记录和声衔接与实际采用意愿。 |
| B11 抽象/完整性质 | C→Db→C，与Cmaj9→Dbmaj9→Cmaj9对照；比较V4_B11召回、分数及原型扩展音。C/Cm、Cmaj7/C7、sus/增减与显式矛盾低音不应获得该方向性优惠。 |
| R01 默认库 | Am–G#dim–G–D/F#，各2或4QN，OPEN；Auto/固定A minor。记录V4_A08及其真实后缀，不能当作F–G–Am–C已进入默认库。 |
| R01 隔离评审 | 从独立材料试听完整Am–G#dim–G–D/F#–F–G–Am–C，检查最低音A–G#–G–F#–F（MIDI 45/44/43/42/41，以编号为准）及后缀。仅D/F#在候选中声明bass；其余规则化根音低音不是作者声明。V5不能通过本次验收擅自激活为默认Factory。 |
| FULL/SKELETON | F4QN–F#dim7 0.5QN–G3.5QN–C4QN，以及R01输入；对照HF状态与骨架，试听/导出仍应保留完整输入。 |
| B08 低音 | C→Cmaj7/B→C7/Bb，固定C major；核对后续F/A、Fm/Ab的明确低音，比较转位和缺五度输入。 |
| 拍号与导出 | Cmaj9→Dbmaj9，分别3/4、4/4、6/8、5/4；核对有效QN、Preview和MIDI文件/拖放的实际音符起止、扩展音。拖放由宿主决定工程速度/拍号，不能凭文件显示推断。 |
| 保存与重开 | 保存带输入、固定调性/Auto选择、视图和候选的工程；退出Cubase，再用同一包重开、重算并导出诊断。HF/Abstract由当前二进制决定，Session不保存这两个算法偏好；换包后不能要求复现原包开关。 |
| 用户库与回退 | 使用已备份的个人库，添加一个有明确名称的验收条目，重启后确认保留。换回DEFAULT-OFF旧完整bundle后再核对Build ID与用户条目。不要清空用户库，不通过覆盖active.txt或生产Factory来试验V5。 |

## 交回结果

在“诊断/宿主诊断”使用本地输入与推荐诊断导出，连同对应工程、输入MIDI和必要截图交回。保留实际候选ID、匹配键、两条分数轨道、默认/更多可见性；仅卡片数量不足以定位问题。

R02离线根因与真实Cubase根因分开记录，后者仍 UNKNOWN。匿名试听只准备材料，音乐合理性由用户判断；规则化MIDI/WAV不是原始作者演奏。联合开关、V5采纳及正式发布均未获本阶段支持。
