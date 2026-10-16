# 有效路径与同步边界

## 跨设备约定

`<checkout>` 表示已通过 `git rev-parse --show-toplevel`、`git status -sb`、`git log -1` 核实的工作目录。目录同名不代表同版本。移动到另一设备时先核对 Git 远端、分支、HEAD 与未提交差异，再操作；构建缓存不得跨 checkout 混用。

| 对象 | 有效映射 | 边界 |
| --- | --- | --- |
| 远端仓库 | `https://github.com/OldLai-ctrl/HarmonyContinuation.git` | 正式发布分支 `main` / Tag `v0.9.0`；保留开发主线 `v0.9/dev`；`library/v3` 保留已完成内容历史 |
| 原开发 checkout | `HarmonyContinuation-release/`；Phase7 开始时为 `development/phase5-20261013`，收尾只读观测为 `development/phase61-20261016` | 存在本轮之外的未提交文档改动，未强制同步或切换该工作树。Phase6 插件身份仍以实际包与既有报告核对，不由当前分支名推断 |
| Phase 7 设计工作树 | 同级 `HarmonyContinuation-phase7/` → `design/phase7-ui-20261016`，从 `66d7f07` 隔离；产物 `design/phase7/` | 用户已选A，A2卡片原型与报告同步至设计/既有开发分支；原开发目录并行改动保留，插件包身份不变 |
| 旧目录归档 | 同级 `_archive/HarmonyContinuation-20261008/` | 保留三个旧目录全部文件；仅用于恢复。归档入口记录本机恢复条件，不在归档内开发 |
| SDK 配置 | `VST3_SDK_ROOT` 环境变量或 CMake 缓存项 | 完整离线 SDK；各设备实际路径待核实，不提交 SDK |
| 工具配置 | 本机开发终端的 CMake / MSVC / Ninja，打包脚本的 `Iscc` / `RuntimeDirectory` | 实际版本与路径按操作前置条件核实 |
| 正式构建与打包输出 | `<checkout>/build-installer/release-0.9.0/`、其 `evidence/` 及 `build-installer/tools/Inno/` | 保留正式产物；`.gitignore` 排除，另一设备独立构建 |
| V4 开发数据与产物 | `data/factory-v4/` → `build-v4-dev/factory.db`；开发统一包 `build-installer/v4-0.10.0-dev.1/`，内含 `stage-4/`、`midi/`、`evidence/` | 仅匹配 0.10.0-dev.1+ 引擎；统一 Setup 管理版本化 Factory/4，保留历史；见 [产品化说明](docs/FACTORY_V4_PRODUCTIZATION.md) |

设备具体路径在运行时核实；关系改变时替换失效行，不追加长期流水账。功能版本与来源提交以 [NOW.md](NOW.md#功能基线) 为文档入口，实时状态仍需核对 Git。

## 安装与持久数据

| 对象 | 位置 | 生命周期 |
| --- | --- | --- |
| 已安装插件 | `%CommonProgramFiles%\VST3\HarmonyContinuation.vst3` | 独立于源码，运行包验证后才确认版本 |
| Factory 历史及活动指针 | `%ProgramData%\HarmonyContinuation\Libraries\Factory\<版本>\factory.db`、`active.txt` | 保留历史；独立库验证后切换 |
| 用户进行 | `%LOCALAPPDATA%\HarmonyContinuation\user.db` | 每台设备本地数据，更新/卸载保留 |
| 插件备份 | `%ProgramData%\HarmonyContinuation\Backups\Plugins` | 保留文件，防止宿主重复扫描 |

## 同步

Git 同步代码、文档、固定测试输入、Factory 源 JSON 和安装脚本。未提交改动未经过 push 就不会到另一设备；接续前明确它们由谁保管。提交/推送由当前任务授权范围决定，更新 NOW 的待同步状态。

SDK、编译工具、缓存、生成数据库、Setup、ZIP、运行日志不经 Git 自动同步；测试包单独分发并带提交和 SHA-256。用户数据库、宿主工程与私有音乐素材不自动复制或上传，不用机器间的绝对路径替代仓库相对链接。

dev.5源码通过 `development/dev5-closeout-20261012` 同步；原 `library/v4` 保留历史基线。完整交接包 `HarmonyContinuation-dev5-Handoff-20261012.zip` 与 `HANDOFF_DEV5_SHA256SUMS.txt` 已上传到 [既有GitHub未发布草稿](https://github.com/OldLai-ctrl/HarmonyContinuation/releases/tag/untagged-45999c0f91b6f2bf11c1)，2026-10-12读回远端digest与本地一致；原4个资产及草稿元信息保留。有仓库权限的账号登录下载，哈希、源码/构建身份和设备接续步骤见 [RUNBOOK](RUNBOOK.md#dev5-另一设备接手)。草稿保持未发布，不作为正式Tag或正式Release。

Phase5源码使用 `development/phase5-20261013`，构建提交 `3a582e4`。同级 `HarmonyContinuation-phase5-delivery-20261013/` 提供三模式便携包、默认Setup、隔离V5评审包、源码、`HarmonyContinuation-Phase5-Handoff.zip` 与逐包SHA；原始证据位于同级 `HarmonyContinuation-phase5-validation-20261013/`。Git与远端交付状态见本轮 `git-proof.json` / 上传读回proof；操作见 [Phase5接手](RUNBOOK.md#phase5-dev6-另一设备接手)。不修改上述历史草稿元信息或旧附件。

Phase5.1 保持插件构建身份 `3a582e4` / dev.6，增补范围仅测试与文档。验证资料位于同级 `HarmonyContinuation-phase51-validation-20261014/`；唯一 Acceptance Supplement 目标目录为同级 `HarmonyContinuation-phase51-delivery-20261014/`，不重打 Phase5 三模式包、Setup、Factory 或 Source ZIP。Supplement 顶层按现有打包器要求组织为 `proofs/`、`safety/`、`blind/`、`answer-key/`、`diagnostics/`、`docs/`；答案与 mapping 不进入 `blind/`。源码提交和远端资产读回分别以 Git proof、上传 proof 为准。详细范围见 [Phase 5.1 收尾报告](docs/PHASE51_CLOSEOUT_20261014.md) 与 [Cubase 清单](docs/PHASE51_CUBASE_ACCEPTANCE.md)。

Phase6 dev.7 使用一个 Setup、一个 VST3 插件和 Legacy / HarmonyFirst / Abstract 三种运行时模式；工程重开后回到 Legacy。Factory V4 内容未改，Phase3A OFF，无 Phase6 Candidate 预设。验证材料在同级 `HarmonyContinuation-phase6-validation-20261014/`，最终分析为其中 `benchmark/RESULTS_ANALYSIS.md`；交付目录为同级 `HarmonyContinuation-phase6-delivery-20261014/`，包与 SHA 以最终交付证明为准。V5 不安装，只附隔离评估 MIDI/证据。详细状态见 [Phase 6 综合报告](docs/PHASE6_ENGINEERING_20261014.md) 与 [Cubase 验收清单](docs/PHASE6_CUBASE_ACCEPTANCE.md)。
