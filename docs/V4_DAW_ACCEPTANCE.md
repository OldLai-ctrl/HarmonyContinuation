# 0.10.0-dev.2 / Factory V4 人工验收

本构建的 Cubase、FL Studio 和风格听审均 Pending。自动 MIDI/VST XML 夹具检查不代表真实 Cubase 验收；正式 v0.9.0 的既有 Cubase 结论也不代表本二进制已验收。关闭宿主后使用本轮新生成的独立核心修复包内统一 Setup；核对“更多 → 关于”版本及 Build ID 与 BUILD_INFO.json。安装会更新标准 VST3 路径中的插件；保留 v0.9.0 原包和个人数据备份。

| 项目 | 操作与预期 | Cubase Pro 15 | FL Studio |
| --- | --- | --- | --- |
| 扫描与版本 | 扫描加载，0.10.0-dev.2 / Build ID 正确；Factory 4 共 657 条 | Pending | Pending |
| 三种输入路径 | 对比直接拖放和弦、保存后重新导入同一 MIDI、Cubase 的和弦轨导入；名称、根音、指定低音和时值应合理一致 | Pending | Pending |
| 短前缀与普通进行 | 检查 C–Cmaj7/B 的 B08 前缀、E7–A7 的 B01 前缀、Cmaj9–Dbmaj9 的 B11 前缀，以及 C–Am–F–G；无需先选调或额外点击 | Pending | Pending |
| B09 扩展候选 | C 大调、Pop、Resolve；导入 C/E–F–F#dim7–G，查 More candidates；选 B09 后 Preview/导出对应 Am–G/B–C | Pending | Pending |
| 半音下降低音 | C 大调、Pop、Resolve，逐步测试 C–Cmaj7/B–C7/Bb–F/A–Fm/Ab；B08 后续 C7/Bb–F/A–Fm/Ab–C/G–G7–C；听低音 C/B/Bb/A/Ab/G | Pending | Pending |
| 连续副属 | C 大调、Jazz、Resolve，测试 E7–A7 与 E7–A7–D7；B01 后续 D7–G7–C，检查目标和时值 | Pending | Pending |
| Side-slipping | C 大调、Jazz、Resolve，测试 Cmaj9–Dbmaj9 与 Cmaj9–Dbmaj9–Cmaj9；B11 再接 Am9–Dm9–G13–Cmaj9 | Pending | Pending |
| 摇摆与切分 | 同一段进行使用 Swing、弱拍提前及非整数时值；推荐可以换序，但不得仅因时值不齐就变成零条 | Pending | Pending |
| FULL/SKELETON / Why | 核对 FULL 总数与 SKELETON 结构节点数；简化查看可省略短经过和弦，仍保留定义性低音及特殊连接。切换不改变候选或导出 MIDI；Why 中英文指向当前候选 | Pending | Pending |
| Preview / MIDI 同一性 | 切换候选后试听、保存或拖出 FullPhrase MIDI，核对当前候选及输入、低音、精确九/十三音、时值一致 | Pending | Pending |
| 四份独立 MIDI | 导入 midi/V4_B01、B08、B09、B11.mid；120 BPM / 4/4；听审低音、连接、节奏与和弦张力 | Pending | Pending |
| Enrichment 保护 | 对普通进行及带斜杠、maj9、13 的输入执行 Polish/Rich/Advanced；只应用不破坏指定低音、关键音和连接的改写；无安全候选时应说明原因 | Pending | Pending |
| Color Harmony | 提示、排序及恢复原顺序正常；Unknown / Uncertain 为有效回退，不要求特殊和弦必有确定标签 | Pending | Pending |
| 持久化与个人数据 | 保存/重开项目及候选；个人进行、收藏、设置保持 | Pending | Pending |

默认音色/配器不保证固定内声部或严格 Planing。Neo-Soul/Gospel 风格仅为编辑性候选，填写听审判断，不能只根据九和弦数量确认风格。

| 安装维护项目 | 预期 | 人工结果 |
| --- | --- | --- |
| 两组件一起安装 | 开发版 VST3 + V4，单一管理入口 | Pending |
| 旧 0.9.0 或无主程序，只选 V4 | 明确阻止，提示同时更新主程序到 0.10.0-dev.2+ | Pending |
| 只更新主程序 | 已有支持的 Factory 保留；个人数据不变 | Pending |
| 匹配主程序，只更新 V4 | VST3 不改变，活动库为 V4；历史库保留 | Pending |
| 修改 / 修复 / 卸载 / 占用 | 未选更新项保留；删除需明确选择；关闭宿主再更新；卸载保留历史库和个人数据 | Pending |

FL：Options → Manage plugins 重新扫描；文件拖入按 Wrapper Accept dropped files 设置。拖出不接受时用保存 MIDI 再导入，参阅 [FL 说明](FL_STUDIO_COMPATIBILITY.md)。每项填写 Pass / Fail / Not tested；失败记录宿主完整版本、Build ID、输入、Style/Intent/调性、操作、预期与实际，必要时附 MIDI 或截图。完成后回传结果；不自动执行 v1.0 全量回归。
