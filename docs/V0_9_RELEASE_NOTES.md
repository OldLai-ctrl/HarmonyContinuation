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
