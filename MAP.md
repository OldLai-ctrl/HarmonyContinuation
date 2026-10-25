# 有效路径与同步边界

## 跨设备约定

`<checkout>` 表示已通过 `git rev-parse --show-toplevel`、`git status -sb`、`git log -1` 核实的工作目录。目录同名不代表同版本。移动到另一设备时先核对 Git 远端、分支、HEAD 与未提交差异，再操作；构建缓存不得跨 checkout 混用。

| 对象 | 有效映射 | 边界 |
| --- | --- | --- |
| 远端仓库 | `https://github.com/OldLai-ctrl/HarmonyContinuation.git` | 仅保留 `main` 和正式 Tag `v0.6.0`、`v0.8.0`、`v0.9.0`、`v0.10.0`；完整主线历史保留 |
| 原开发 checkout | `HarmonyContinuation-release/`，本轮读取时为 `development/g01-g4u-experience-20261024` | 本轮不写入；已有协作文档及其他工作保留。实时分支以Git为准；正式发布来源仍为 `v0.10.0` / `c8d8dc3` |
| Factory V6 工作包B | `HarmonyContinuation-factory-v6/` → `development/factory-v6-20261024`，父基线 `2421c6f` | 仅本地内容线；[集中评审](docs/FACTORY_V6_REVIEW.md)，独立proposal/experiment输出，不改生产V5 |
| V6 数据/验证/听评产物 | 同级 `HarmonyContinuation-v6-b3r-evidence/engineering/r3/`；790与810旧产物保留 | Library6/Schema2，proposal1032/experiment1014；HCA格式1配套2MiB读取器。哈希及4组MIDI/WAV见[集中评审](docs/FACTORY_V6_REVIEW.md)，未安装激活 |
| 当前验证与测试包 | 同级 `HarmonyContinuation-v010-validation-20261023/`、`HarmonyContinuation-v010-delivery-20261023/` | 正式 v0.10.0 构建、门禁日志与唯一Setup；RC1/dev.15目录保留 |
| Phase 7 UI | `design/phase7/` 与正式 `src/ui/` | 已集成至main；原型和正式UI临时工作树已撤除，来源历史仍可追溯 |
| 仓库精简恢复资料 | 同级 `_archive/HarmonyContinuation-repo-cleanup-20261023-180326/` | 全部原引用的Git bundle、原工作区文档ZIP及补丁；仅用于恢复，不在备份内开发 |
| 旧目录归档 | 同级 `_archive/HarmonyContinuation-20261008/` | 保留三个旧目录全部文件；仅用于恢复。归档入口记录本机恢复条件，不在归档内开发 |
| SDK 配置 | `VST3_SDK_ROOT` 环境变量或 CMake 缓存项 | 完整离线 SDK；各设备实际路径待核实，不提交 SDK |
| 工具配置 | 本机开发终端的 CMake / MSVC / Ninja，打包脚本的 `Iscc` / `RuntimeDirectory` | 实际版本与路径按操作前置条件核实 |
| 正式构建与打包输出 | 同级 `HarmonyContinuation-v010-delivery-20261023/` 与 GitHub Release；临时发布工作树已撤除 | `v0.10.0` Tag、正式源码、About BuildCommit、moduleinfo及Setup固定于 `c8d8dc3`；main后续文档提交不改变已发布身份 |
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

旧开发分支、阶段Tag及dev.1–dev.7交接用未发布草稿已按用户确认清理。历史源码从main提交历史或精简冷备份恢复；历史报告内的分支名和草稿下载地址只代表当时状态。现行分发入口为正式 [GitHub Release](https://github.com/OldLai-ctrl/HarmonyContinuation/releases/tag/v0.10.0)。

Phase5历史构建提交 `3a582e4`。同级 `HarmonyContinuation-phase5-delivery-20261013/` 的既有交付文件与 `HarmonyContinuation-phase5-validation-20261013/` 的证据未纳入本轮清理。旧交接操作见 [Phase5接手](RUNBOOK.md#phase5-dev6-另一设备接手)，其中远端草稿入口已撤除。

Phase5.1 保持插件构建身份 `3a582e4` / dev.6，增补范围仅测试与文档。验证资料位于同级 `HarmonyContinuation-phase51-validation-20261014/`；唯一 Acceptance Supplement 目标目录为同级 `HarmonyContinuation-phase51-delivery-20261014/`，不重打 Phase5 三模式包、Setup、Factory 或 Source ZIP。Supplement 顶层按现有打包器要求组织为 `proofs/`、`safety/`、`blind/`、`answer-key/`、`diagnostics/`、`docs/`；答案与 mapping 不进入 `blind/`。源码提交和远端资产读回分别以 Git proof、上传 proof 为准。详细范围见 [Phase 5.1 收尾报告](docs/PHASE51_CLOSEOUT_20261014.md) 与 [Cubase 清单](docs/PHASE51_CUBASE_ACCEPTANCE.md)。

Phase6 dev.7 使用一个 Setup、一个 VST3 插件和 Legacy / HarmonyFirst / Abstract 三种运行时模式；工程重开后回到 Legacy。Factory V4 内容未改，Phase3A OFF，无 Phase6 Candidate 预设。验证材料在同级 `HarmonyContinuation-phase6-validation-20261014/`，最终分析为其中 `benchmark/RESULTS_ANALYSIS.md`；交付目录为同级 `HarmonyContinuation-phase6-delivery-20261014/`，包与 SHA 以最终交付证明为准。V5 不安装，只附隔离评估 MIDI/证据。详细状态见 [Phase 6 综合报告](docs/PHASE6_ENGINEERING_20261014.md) 与 [Cubase 验收清单](docs/PHASE6_CUBASE_ACCEPTANCE.md)。

Phase6.1 dev.8 的构建与诊断为同级 `HarmonyContinuation-phase61-validation-20261016/`，交付为同级 `HarmonyContinuation-phase61-delivery-20261016/`；具体门禁、哈希和实际和弦听评见 [Phase6.1 报告](docs/PHASE61_ENGINEERING_20261016.md)。dev.7 目录和 Factory V4 原件保留作回退对照。
