# Cubase 15 基础实测记录

测试宿主：Cubase Pro 15.0.30 Build 287（x64）。这份记录汇总 Phase 0A/0B 已完成的测试；播放头自动同步的后续测试见 [CUBASE_TIMELINE_SYNC_TEST.md](CUBASE_TIMELINE_SYNC_TEST.md)。

## 编辑器加载与渲染

**结果：通过。** 此前曾出现 VSTGUI 白屏和 Cubase 闪退；Windows 应用程序错误事件指向 Intel 图形驱动 `igd10um64xe.DLL` 31.0.101.4032。当前插件 Windows frame 禁用 DirectComposition 并使用 Direct2D 软件绘制。用户后续截图显示编辑器完整绘制，能够接收和弦拖放并显示播放快照。

## 和弦轨拖放与解析

**结果：通过。** 在 Cubase 工程中多选和弦轨事件并拖入插件，VSTGUI 收到一个 UTF-8 Text 数据项，大小 1087 字节，根节点为 VST-XML 1.4。解析结果：

| 和弦 | 工程位置 | 时长 |
|---|---:|---:|
| C7 | 0 QN | 4 QN |
| Amin7 | 4 QN | 2 QN |
| A7 | 6 QN | 1 QN |
| Dmin7 | 7 QN | OPEN |

用户确认拖入的 QN 是工程绝对位置。原始 `keyNote`、`bassNote`、`mask`、`pitches` 和 `color` 字段均由插件保留；在验证字段编码前，和弦性质显示为“未知”。

## ProcessContext 实测

**结果：工程位置可用，宿主和弦不可用。** 播放时手动获取的快照记录到 QN 0.587、3.211、4.811、6.581、7.456；速度 120 BPM、拍号 4/4、播放状态和工程位置有效。五次快照的 `Chord Valid` 均为 `no`，所以定位使用导入的 VST-XML 和 `ProjectTimeMusic`，不会依赖 `ProcessContext::Chord`。

## 剪贴板观察

Ctrl+C 后用户主动点击剪贴板检查：VSTGUI IDataPackage 为 0 项；Windows 原生格式为 `DataObject`（ID 49161，8 字节）及 `Ole Private Data`（ID 49171，120 字节），未观察到 XML。剪贴板不用于正式导入，本阶段不再要求进一步检查这些格式。

## 后续测试

请按 [播放同步手测步骤](CUBASE_TIMELINE_SYNC_TEST.md) 检验自动播放跟随、当前和弦高亮，以及前后定位和循环。安装目录和本次构建产物应先通过 `tools/verify-installed-build.ps1` 核对 SHA-256。
