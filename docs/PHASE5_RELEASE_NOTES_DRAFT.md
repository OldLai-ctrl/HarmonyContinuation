# 0.10.0-dev.6 开发验收说明草案

本版本供后续集中验收，尚未正式发布。构建提交、开关、文件哈希及自动化证据以交付包的 `BUILD_INFO.json` 和 [Phase 5 综合报告](PHASE5_COMPREHENSIVE_20261013.md) 为准。

## 本轮变化

- 新增独立 Abstract 检索实验：简单和弦可以更合理地检索同根音、同核心性质的 Factory 扩展和弦原型。例如 C→Db 可检索 V4_B11 的 Cmaj9→Dbmaj9 原型，续写保留原型的扩展音。
- 优惠仅适用于有明确音集、方向正确且核心三度、七度和显式低音不冲突的匹配；完整和弦、反向省略、大小调性质冲突、sus/增减和弦保留既有处理。
- About、本地诊断和结果状态显示实际 HF、P3、Abstract 开关；分析前保持 PENDING。
- 修复同一空用户库被多个实例首次写入时的 metadata 初始化竞争，初始化使用 SQLite 写事务。现有数据格式与迁移流程保持兼容。

## 三个验收模式

| 包 | HarmonyFirst | Phase 3A | Abstract |
|---|---|---|---|
| DEFAULT-OFF（默认开发包与 Setup） | OFF | OFF | OFF |
| HF-ON | ON | OFF | OFF |
| ABSTRACT-ON | OFF | OFF | ON |

三个包共享同一 VST3 身份。关闭宿主并备份旧完整 bundle 后，只保留一个版本供扫描；通过 About 核实实际安装状态。HF 与 Abstract 是两个独立实验。

## 内容与验收边界

默认 Factory 仍为 V4/657。V5/658 仅为独立评审库，新增路线 Am→G#dim→G→D/F#→F→G→Am→C，目标续写 F→G→Am→C 尚未进入默认内容。候选 MIDI 是规则化实现，待音乐试听。

R02 Color 的真实 Cubase 根因仍 UNKNOWN。离线检索改善不能证明真实宿主问题已解决；真实宿主、工程重开、拖放交互和人工试听尚待验收。按 [中文集中验收清单](PHASE5_CUBASE_ACCEPTANCE.md) 记录后续结果，再决定候选采纳及正式发布。

旧本机 HF-ON 安装曾被 UAC 取消，不能作为本版本已安装的证据。用户库及独立 Factory 历史按既有安装保护契约保留；个人数据与宿主工程不随源码或交付包同步。
