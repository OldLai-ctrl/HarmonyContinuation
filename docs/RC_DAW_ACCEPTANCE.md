# v0.9.0 宿主与安装验收记录

对象：0.9.0 正式版。用户已确认 Cubase Pro 15 主要功能及 RC4 色彩条修复正常；正式版本未改变相关运行逻辑，复用用户报告，不声称本次正式构建已重新实机测试。FL Studio 为 Not Verified / Pending，本轮不要求执行。RC3 隔离安装 28 项及 RC4 定向显示证据继续复用；下方安装人工结果未新增，仍 Pending。

关闭宿主后运行 正式目录内 HarmonyContinuation-0.9.0-Setup.exe。核对「更多 → 关于」的版本/提交与 BUILD_INFO.json；不要覆盖或删除个人数据。插件在标准 Common Files/VST3 路径；FL 从效果器插槽加载，必要时重新扫描，文件拖入设置参阅 [既有说明](FL_STUDIO_COMPATIBILITY.md)。

| 项目 | 最小操作及预期 | Cubase Pro 15 | FL Studio |
| --- | --- | --- | --- |
| 1. 扫描与加载 | 能扫描插件、加载并正常显示，版本和提交匹配 BUILD_INFO | 复用 RC 用户报告 | Not Verified / Pending |
| 2. 自动推荐 | 拖入受支持的和弦轨片段（Cubase）或标准和弦 MIDI；无需选择色彩目标就出现推荐 | 复用 RC 用户报告 | Not Verified / Pending |
| 3. 两模式与 Why? | 各查看一条继续发展/升级进行；色彩文字、排序和 Why? 正常；提示关闭后隐藏色彩说明但保持排序设置 | 复用 RC 用户报告 | Not Verified / Pending |
| 4. Preview 身份 | 排序后试听当前卡片，切换另一卡片应对应新的完整方案 | 复用 RC 用户报告 | Not Verified / Pending |
| 5. MIDI 身份 | 保存或拖出选中候选 MIDI，核对和弦、低音和时值确实对应卡片；记录使用的输出范围 | 复用 RC 用户报告 | Not Verified / Pending |
| 6. 恢复原顺序 | 记录同组初始候选，启用色彩排序后再关闭，恢复初始顺序（符合保护门槛时才可能换位） | 未单独报告；沿用自动证据 | Not Verified / Pending |

每项填 Pass / Fail / Not tested；失败附宿主完整版本、插件版本/提交、输入样例、最短操作、预期与实际、截图或宿主诊断。Unknown / Uncertain 中性提示属于受支持回退；模型温度不是客观情绪判断。正式发布复用已有用户报告，不自动启动额外矩阵。

English: User-reported Cubase Pro 15 core-function and RC4 color-hint acceptance is reused; this final binary was not separately tested in a DAW. FL Studio compatibility has not yet been verified with the v0.9.0 release. Check scan/load/display, automatic recommendations on import, both modes with Color Hints / Color Preference / Why?, selected-card Preview identity, selected-candidate MIDI identity, and original order restoration when Color Preference is Off. Report exact host/build versions and Pass / Fail / Not tested for each row.

## 统一安装器人工验收

关闭使用插件的宿主后，在有备份的机器逐项按需要验收；不要求为已通过项目重复全套测试。

| 项目 | 预期 | 用户结果 |
| --- | --- | --- |
| 首次安装 | 默认两个独立组件，一个 Windows 管理入口 | Pending |
| 旧版检测与迁移 | 显示真实插件/Factory 版本；已知旧双入口安全合并，未知路径提示保留 | Pending |
| 只更新主程序 | Factory / User Library 不变 | Pending |
| 只更新曲库 | VST3 文件/版本不变，必要兼容提示正确 | Pending |
| 修改与修复 | 明确增删组件，未选更新项保留；修复恢复官方文件 | Pending |
| 完整卸载 | 已管理插件/库及入口移除，个人库/收藏/配置和其它历史数据保留 | Pending |

DAW 仍保留上面的六项范围，重点核对 Cubase Enrichment OPEN 静态提示与具体原因、Continuation 提示、Preview/MIDI 候选身份；FL Studio 后续人工验收。隔离安装验证不代替本表。
