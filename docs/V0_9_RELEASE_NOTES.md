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
