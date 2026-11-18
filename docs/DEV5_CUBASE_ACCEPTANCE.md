# dev.5 Cubase 中文验收

开发版本 0.10.0-dev.5。使用包内 BUILD_INFO.json 的真实提交及 SHA256SUMS.txt。默认 OFF 与 ON 实验包都使用原始 Factory V4/657；Phase3A 均 OFF。

## 安装、身份与回退

关闭并重新启动 Cubase 是每次换包的必要步骤。先备份完整 HarmonyContinuation.vst3 bundle；OFF 和 ON 使用相同插件身份，扫描目录内一次只放一个。默认使用 OFF Setup 或 OFF 便携包；ON 只作算法对照。

打开版本信息：确认 dev.5、提交、Build HarmonyFirst 和 Build Phase3A。未分析时实际状态应为 PENDING；导入并等待分析后，实际状态应与构建一致。高级诊断与“导出本地输入与推荐诊断”也应一致。若显示旧版本，核对扫描目录中的完整 bundle 和包内哈希，再按 Cubase 自身的重新扫描流程处理缓存。

回退时关闭 Cubase，恢复备份的完整 bundle 或旧 Setup，保留个人进行及历史 Factory。候选 V5 数据仅在独立候选包中，等待内容批准。

## 时间与音符位置

依次检查 4/4、3/4、6/8、5/4、7/8、9/8。一小节的 QN 分别为 4、3、3、5、3.5、4.5。普通一小节原型应按可靠输入 QN 缩放；特殊长短节奏保留相对比例。

导入 `Cmaj9 → Dbmaj9`，固定 C Major，首和弦使用对应小节 QN，末和弦设 OPEN。检查后续 Cmaj9、试听、保存及拖出 MIDI 的真实音符起止。保存文件与拖放文件采用同一 QN 布局；拖放文件省略 conductor 元数据，由宿主工程决定拍号/速度。

时间来源顺序：匹配输入的有效 duration/onset；没有有效样本时使用导入区段的一致 Session 拍号；二者未知时保留原型。播放头当前拍号只作诊断参考，不能证明历史选择区段的拍号。变拍文件不会把首条拍号冒充整段；未知拍号的 annotated 保存仍使用已注明的 4/4 元数据兼容默认，不能据此认定输入拍号。复合拍与不规则分组保持 Unknown。

连续导入带 3/4 拍号的文件、再导入无拍号文件，后者 Session 拍号应为未知。只改变播放头拍号而不改变已有输入 QN，实际音符长度应保持。

## 推荐与结构样例

| 输入与设置 | 操作及观察 |
|---|---|
| Am → G#dim → G → D/F#；各 2QN 或 4QN，最后 OPEN；Auto/A Minor/A Major | 记录 Factory ID、分数、意图、默认及“查看更多”。A08 路线为 Am → Am → D；截图是否来自该 ID 需要本机诊断确认。 |
| F 4QN → F#dim7 0.5QN → G 3.5QN → C 4QN；C Major | OFF 的 FULL 4/SKELETON 3；ON 可为 4/4。试听和导出均保留四个输入和弦。 |
| Am → G#dim → G → D/F#；2QN，末 OPEN；A Minor | ON 下已验证可显示 FULL 4/SKELETON 3 的替代样例。 |
| C → Cmaj7/B → C7/Bb；各 4QN；C Major | V4_B08，后续 F/A → Fm/Ab。检查明确低音及缺五度对照 MIDI。 |
| Cmaj9 → Dbmaj9；各 4QN；C Major | V4_B11，后续 Cmaj9。大七度与九度保留。 |
| F → G → Am；4/4/4QN 与 3/5/OPEN；Auto/C Major/A Minor | 导出成功和失败两组本地诊断，核对原始音符、调性、OPEN、候选消失环节和 UI。真实 R02 根因仍 UNKNOWN。 |

包内 OPEN MIDI 使用有限末音，因为 SMF 没有通用 OPEN 编码；导入后必须按界面设为 OPEN。包内 MIDI 均是明确标记的合成测试数据。

## 人工音乐验收

试听 A08 的 Am–Am–D、隔离候选的 F–G–Am–C、实际可见 Color、FULL/SKELETON 和六拍号输出。记录低音连接、发展方向、重复和弦、色彩合理性及实际采用意愿。候选 MIDI 文件名带 CANDIDATE；它不改变插件默认 V4。

同时检查工程保存/重开、手动调性变化、重复导入、窗口缩放及高 DPI 拖放。真实宿主和音乐项目完成后才可以讨论正式发布。
