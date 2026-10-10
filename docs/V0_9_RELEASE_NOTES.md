# HarmonyContinuation 0.9.0

Windows x64 VST3 正式版，保留 RC4 运行行为与色彩提示可见性修复。唯一安装包为 `HarmonyContinuation-0.9.0-Setup.exe`，正式产物目录 `build-installer/release-0.9.0/`；实际 main 提交、Build ID 与文件哈希见随包 BUILD_INFO.json / SHA256SUMS.txt，Tag 为 `v0.9.0`。不额外创建 GitHub Release 页面或上传公开资产。

## 本版内容

- 继续发展生成后续完整路径，升级进行改写已有完整进行；FULL / SKELETON、既有档位、Preview、MIDI 保存与拖出保留。
- Factory Library V3：629 条（155 条历史 canonical + 474 条新增），六条 QUESTIONABLE 不进入生产库。
- 色彩提示默认开启，两模式共用卡片左缘 4 逻辑像素色条与文字：橙金暖色、青蓝冷色、中性灰；静态与整段时间趋势区分，Unknown / Uncertain 不伪造冷暖结论。
- 色彩排序默认关闭，六种偏好：关闭、自动延续前段/原进行、逐渐温暖、逐渐偏冷、张力先升后降、先紧张最后温暖收束。仅有限重排已有候选，最多移动两位，关闭恢复原次序，不改变候选内容。
- Why? V2 合并功能解释、色彩变化和启用后的排序理由，保留中英文与高级详情。
- 统一安装器管理 VST3 与 Factory 两个独立组件，默认同时选择；保留首次安装、更新、修改、修复、单组件移除、完整卸载和已知旧双安装器迁移。取消勾选更新项不删除已有组件；保留 User Library、收藏、配置、个人进行、历史库及未管理文件。

## 验收与验证范围

Cubase Pro 15：用户已确认主要功能及 RC4 色彩提示修复通过人工检查；正式版没有改变相关运行逻辑，因此复用这些证据。**没有声称正式构建经过新的 Cubase 实机测试。**

FL Studio：**Not Verified / Pending**。FL Studio compatibility has not yet been verified with the v0.9.0 release. 本轮不要求 FL Studio 验收，也不以 Pending 阻止发布。

本轮仅一次正式 Release 编译、一次统一安装器打包、版本/Build ID/内容/依赖/哈希及安装器相关逻辑核对；复用 RC3 的 28 项隔离生命周期与 RC4 定向显示证据，不重复安装流程或音乐测试。全量回归统一留到 v1.0 正式发布前，v0.9 不例行执行。实际检查结果见 [NOW](../NOW.md#证据) 指向的随包记录。

## 已知限制

1. 未覆盖音集、多方向、缺少可靠低音或时间信息等结构可能为 Unknown / Uncertain；不可靠时保留中性提示和原排序。OPEN 仅在静态指标可靠时显示静态回退，不编造整段时间趋势。
2. 色彩和声是辅助创作的数学模型，不是客观情绪识别；符号层低音分析不代表最终 MIDI 配音或实际听感已验证。
3. 色彩偏好设置只在当前插件实例内保存，不保证跨工程重载恢复。
4. 高级色彩数值仅展示前八个位置。
5. FL Studio 实机验收 Pending。Preview 使用 Windows 默认播放设备，不经过宿主混音器；宿主不接受 MIDI 拖放时可保存后导入。
6. 安装包尚未数字签名，Windows 可能显示发布者提示。
7. 旧安装器迁移沿用 RC3 边界：未知身份/路径不自动接管；仅库迁移的旧插件需先更新/修复才能通过组件页单独移除；修改过的托管文件先修复再移除；断电/系统崩溃后不提供跨进程自动恢复，必要时重新修复。

Schema 保持 Factory 2 / User 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2，本轮不迁移。v0.8.0 和旧 dev.1 无法安全读写 User Schema 2。更新前关闭宿主并保留个人数据备份。

不分发购买的色彩和声 PDF、Excel、付费数据库或转换副本。音乐数学、Matcher、候选生成/评分、排序权重/窗口、VoiceLeading、MelodyConstraint、Factory 内容、MIDI 和宿主适配冻结。不启动后续版本开发。

---

## 以下为历史 RC / 开发记录
# 0.9.0-rc.4 — 色彩提示可见性修复

Continuation 与 Enrichment 共用固定卡片左缘 4px 色条，增强橙金、青蓝及中性灰对比度；文字在原提示行显示。Unknown / Uncertain 明确显示色彩不足以判断，提示关闭后色条与文字一起隐藏，排序开关独立。保留 RC2 的 OPEN 静态提示及可靠性边界；没有整段证据就不生成时间趋势。数学、排序、候选、Factory 及所有 Schema 不变。

按用户追加请求复用已验证 RC4 二进制生成统一安装器，安装逻辑不变，不重复编译或安装回归；构建标识及最小显示验证见 [NOW](../NOW.md#证据)，真实 Cubase 验收 Pending。

# HarmonyContinuation 0.9.0-rc.3 — Unified Setup

沿用包含 Enrichment OPEN 显示修复的 RC2 源码，只更新安装工程和版本。唯一 `HarmonyContinuation-Setup.exe` 管理 VST3 与 Factory Library V3 两个独立组件，支持离线安装/更新、明确组件增删、修复和完整卸载。默认两组件，跳过更新不等于删除；Windows 沿用一个主程序 AppId。个人库/收藏/配置及其它历史库保留，原 RC1/RC2 包不覆盖。

已知旧双安装器按可验证身份/路径迁移，不强删注册信息。首次仅库迁移保留旧插件，安全的组件级插件移除需先更新/修复它以登记清单。只装插件不再暗含 Factory 数据；官方推荐需要另选库组件。具体迁移与失败恢复边界见 [统一安装器说明](INSTALLER_AND_LIBRARY.md)。

音乐算法、排序窗口/权重、Factory 629 条内容、MIDI、宿主及 Schema 不变。用户报告 RC2 Cubase Enrichment 修复完成；RC3 标准安装与 DAW 人工验收仍待反馈，FL 无新增通过证据。未正式发布、不合并 main、不创建 Tag、不启动 Library V4。

# HarmonyContinuation 0.9.0-rc.2

仅修复升级进行的色彩展示：Cubase 导入的末和弦正常保留 OPEN，缺失时值导致整段色彩汇总为空；RC1 将可靠的逐和弦静态指标一并隐藏。RC2 在全部和弦静态指标可靠、仅时间信息不完整时显示中性「静态：…；整段信息不足」，Why? 明确时值缺失，不编造时长、平均值或时间趋势。正常完整路径沿用原提示；缺失低音、未覆盖音集或多方向仍中性回退。

数学公式、排序门槛/权重、原候选/评分、FULL/SKELETON、档位、MIDI、Factory 629 条及所有 Schema 不变；不改变 Unknown 候选的排序资格。显示提示开关仍独立有效。

用户于 2026-10-10 报告 RC1 在 Cubase Pro 15 的加载、自动推荐、排序切换、Why?、Preview、MIDI 导出与 Continuation 提示通过；截图显示原进行 Dmin → C → Bb → A（OPEN），升级候选 Dmin → C/E → Bb → A 显示「不足以判断」。这是用户报告，不是 Codex 执行的宿主测试。RC2 的 Cubase 定向复验仍 Pending，FL Studio 无新增证据。

RC2 独立目录保留 RC1，仍为候选版；最小验证及包构建证据见 [NOW](../NOW.md#证据)。复验只需同一原进行的升级卡片/Why? 与提示开关，核对新增静态信息和时值说明；不要求重跑已通过项目。

# HarmonyContinuation 0.9.0-rc.1

发布候选，功能冻结，供 Cubase Pro 15 与 FL Studio 真实 DAW 人工验收；不是正式发布。人工结果均 Pending，清单见 [RC 人工验收](RC_DAW_ACCEPTANCE.md)。

- 继续发展生成后续完整路径；升级进行提供原片段的完整改写。保留 FULL / SKELETON、既有档位、Preview、MIDI 保存与拖出。
- Factory Library V3 共 629 条（155 条历史 canonical + 474 条新增），6 条 QUESTIONABLE 不随生产库分发。
- 默认开启色彩提示，卡片显示整段模型温度及张力趋势；独立的色彩排序默认关闭。两模式共用六种偏好：关闭、自动延续前段/原进行、逐渐温暖、逐渐偏冷、张力先升后降、先紧张最后温暖收束。
- 排序仅有限调整已有候选展示，最多移动两位；关闭恢复原顺序，候选内容不变。
- Why? V2 将功能、色彩和启用后的排序理由整合为 1～3 句；高级详情保留数字。
- Windows x64 Setup 复用已有安装器，包含插件、双语资源、Library 3 更新工具及 MSVC 运行库；用户库与 Factory 历史保留。

## 已知限制与兼容性

色彩模型是音乐创作辅助指标，不是客观心理情绪测量。未覆盖音集返回 Unknown，多方向或缺少可靠低音等情况返回 Uncertain；不能可靠分析时保留原推荐。符号低音分析不代表最终 MIDI 配音或实际听感已验收。

色彩偏好只在当前实例保留，不保证跨工程恢复；高级色彩详情仅显示前八位置。Preview 使用 Windows 默认播放设备，不经过宿主混音器；MIDI 拖入/拖出取决于宿主目标，保存文件是既有替代入口。真实 DAW 的扫描、显示、交互与实际导出仍需用户验收。

Schema 不变：Factory 2 / User 2 / Session 5 / Continuation Snapshot 3 / Enrichment Snapshot 2。旧 v0.8.0 / dev.1 不能安全读写 User Schema 2；升级前关闭宿主并备份用户数据。

不分发购买的原始 PDF、Excel、11,880 条数据或其转换副本。包中只沿用项目代码、独立实现的数学规则和项目资源。本轮不运行全量回归；全量关口留到 v1.0 前，v0.9 不例行执行。构建与检查证据见 [NOW](../NOW.md#证据)，包对应提交及 SHA-256 见随包 BUILD_INFO.json / SHA256SUMS.txt。

## 历史 dev.1

# HarmonyContinuation 0.9.0-dev.1

从正式 `v0.8.0` 开始，本轮提供 Windows x64 VST3 宿主兼容候选。Cubase 既有输入保留，FL Studio 20+ 使用标准 MIDI 文件导入及 FilePath 拖放。真实 FL 尚未验收。

- 宿主能力按实际收到的信息记录；速度、拍号、工程位置分别处理缺失情况。
- MIDI 文件在后台读取；无效输入保留原进行，过期导入任务不能覆盖后来恢复的状态。
- 窗口重开保留候选和选择，缩放与尺寸请求增加保护。
- 多实例各自保存状态；关闭一个实例不会停止另一个实例的试听。
- “更多”新增宿主诊断及安全文本导出；FL 文件接收设置说明只在帮助/诊断中出现。
- 仍使用同一独立 Factory Library 2（161 条）、本地 User Library 和标准 Windows 安装器。

音乐算法、161 条内置进行、Session Schema 5、SQLite Schema 1、Continuation Snapshot 2、Enrichment Snapshot 1 未改变。预览仍使用 Windows 默认播放设备。

完整兼容状态及简短中英文安装说明见 [FL_STUDIO_COMPATIBILITY.md](FL_STUDIO_COMPATIBILITY.md)。本轮不加入后续 v0.9 音乐功能、私有 DAW 解析或音频识别。
