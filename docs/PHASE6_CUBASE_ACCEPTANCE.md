# Phase 6 单插件 Cubase 验收清单

适用于 0.10.0-dev.7 单插件算法切换测试包。安装包 SHA-256 见交付校验文件；生产 BuildCommit 见 About、BUILD_INFO.json。顶部“开发测试模式”提供 Legacy、HarmonyFirst、Abstract 三个单选按钮。切换自动重新推荐，不需再次导入或重启宿主。离屏 MainView PNG 只确认实际界面渲染，不代表 Cubase 实机验收。

## 安装与操作约定

A 项使用唯一 HarmonyContinuation-0.10.0-dev.7-Setup.exe，安装一次。关闭 Cubase 后运行安装器，保留插件与 Factory 曲库组件；完成后在 Cubase 插件管理器扫描/刷新一次。B-K 均在同一工程、同一插件实例和同一输入中对照算法，不卸载、重装或替换 VST3。

尽量只导入一次相同 MIDI，并固定工程 BPM、拍号、调性、OPEN/Closed、Intent。切换预设后确认界面和诊断都显示当前模式，等待自动推荐完成；记录默认 Top3 和“更多”候选、Factory ID、分数、Preview 与 MIDI。发生问题时另存工程副本、输入 MIDI、诊断和截图。

| 项 | 输入与操作 | 算法 | 观察或试听 | 请反馈 |
|---|---|---|---|---|
| A. 一次安装 | 安装唯一 Setup，在 Cubase 扫描并插入插件。 | 首次应为 Legacy。 | 安装和扫描是否完成；About/诊断中的版本、Build ID、Factory V4/657。 | 成功或错误文字；Cubase 版本、截图。 |
| B. 同实例切换 | 使用一组已导入 MIDI，不重新导入；点击顶部“开发测试模式”的对应按钮，等待本次自动推荐完成。 | Legacy → HarmonyFirst → Abstract → Legacy。 | 选中模式、诊断算法和候选变化；输入、调性、Intent、偏好保持；Cubase 不需重启。 | 是否即时切换；有无旧结果闪回、卡住或输入被改。 |
| C. R01 | 导入 Am–G#dim–G–D/F#；比较 2QN/4QN、不同拍号、OPEN/Closed，并测 Auto/固定 A minor。 | 每项条件依次比较三模式。 | V4_A08、Factory ID、完整后缀、默认 Top3；如出现 F–G–Am–C，记下它来自的 ID/模式；试听衔接。 | 哪种最自然、是否愿意采用；低音或拍点是否异常。隔离 V5 材料只作独立试听，不默认提供。 |
| D. R02 | F–G–Am；Auto 与固定 C major/A minor；4QN 等时和 3/5/4QN；Closed/OPEN。 | 每格比较三模式。 | V3_SHADE_002/008/019/024/031 的匹配调性、G 插入轨迹、淘汰阶段、后缀、Color 顺序及默认/更多位置。 | 哪些条件给出可用路线；Color 偏好是否改变排序且听起来合理；反馈成功与失败组合。 |
| E. Abstract 扩展和弦 | 分别输入 C–Db–C 与 Cmaj9–Dbmaj9–Cmaj9，保持拍号、QN、调性、Intent 相同。 | Legacy 对照 Abstract；可用 HarmonyFirst 作参照。 | V4_B11 是否召回、扩展音是否正确、回到 C 是否顺畅；核对 Preview/MIDI。 | 色彩与解决感是否改善；有无错误兼容或声部不自然。 |
| F. 两组已知候选替换 | 听 Phase5.1 匿名 Q1/Q2 的完整进行+续写和仅续写；如最终包有等输入材料，再于插件复现并记录 ID。 | 每组比较 Legacy 与 Abstract，HarmonyFirst 可作参照。 | Q1：C–E7–Am，记录 V3_HOME_011 与 Abstract 候选排名；历史数据是更多列表 rank10→rank5，均非默认 Top3 替换。Q2：C–Am–A7–Dm–G，记录 V3_HOME_018/V4_B11 排名；历史数据为 rank3→4，V4_B11 进入 rank3。 | 每组哪条续写更自然、是否采用；不要只按分数选。若无等输入试听材料，注明插件实时复现。 |
| G. Intent 与色彩偏好 | 选同一输入，固定调性和算法后分别改变 Resolve/Develop/Loop/Color Intent 与色彩偏好。 | 固定一个模式逐项对照，再选其他模式抽查。 | 合格候选池、Top3/更多、排序、说明和 MIDI 是否响应；无合格候选时是否明确提示。 | 哪些设置真正改变听感/排序；有无不相关候选被抬高或只有文案变化。 |
| H. 拍号与速度 | 使用 Cmaj9–Dbmaj9 或同一进行，分别设 3/4、4/4、6/8、7/8，至少两档 BPM。 | 三模式使用相同输入对照。 | startQN/durationQN、结构稳定性、候选排序、播放拍点与延音；注意宿主会按工程速度解释 MIDI。 | 时值/拍号最稳的模式；异常小节、BPM、音高或时值。 |
| I. Preview 与 MIDI | 同一输入、同一 Top3 下试听 Preview，并保存/拖放 MIDI；每次切换后重新计算再导出。 | Legacy/HarmonyFirst/Abstract。 | 当前模式候选 ID、试听和 MIDI 音符/起止/低音是否一致；是否仍可导出过期候选。 | Preview/MIDI 是否一致；具体 ID、导出方式和错音/时值。 |
| J. 保存与重开 | 在工程副本中导入 MIDI，选 Auto/固定调性并切到 HF 或 Abstract，保存，退出 Cubase 后用同一包重开。 | 重开前使用实验模式；重开后应安全恢复 Legacy。 | 同一实例未退出时保留所选模式；项目重开后面板和诊断都显示 Legacy，输入/调性等其他工程状态保留。 | 重开是否成功、恢复模式、显示与执行是否一致、是否丢状态。 |
| K. 多实例与 User Library | 两个插件实例载入不同输入并选不同模式；测试前只读记录个人库条目数、收藏和设置。 | 实例甲 Legacy，实例乙 Abstract/HarmonyFirst，再分别切换。 | 实例模式互不串扰；重新推荐正常；个人库和设置前后未丢失。保留个人库备份；仅在你愿意时添加一个明确命名的测试条目，保存/重开后核对，禁止清空或批量删除。 | 有无实例串扰、卡顿或错结果；个人数据前后是否一致；附版本和截图。 |

## 可复制测试记录

~~~text
Phase 6 Cubase 测试记录
日期 / Windows / Cubase 版本：
安装包版本 / SHA-256：
工程副本 / MIDI 文件：
案例项（A-K）：
拍号 / BPM / startQN-durationQN / OPEN或Closed：
调性（Auto或固定）/ Intent / 色彩偏好：
算法模式及界面、诊断显示：
默认Top3及Factory ID / 更多列表：
Preview、MIDI导出/拖放观察：
主观听感（自然度、解决感、节奏、是否采用）：
异常小节、音高、时值或重现步骤：
截图 / 诊断 / 工程 / 导出文件位置：
结论（通过 / 不通过 / 不确定）及补充说明：
~~~

## 「需要你在 Cubase 中试听与测试的事项」

按 A-K 完成表中的输入/操作，比较对应模式，观察候选、Preview/MIDI 并填写反馈。最终要判断哪种和声续写更值得采用。自动化、离屏截图和 VST3 Validator 不能替代真实 Cubase 试听。
