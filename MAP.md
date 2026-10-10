# 有效路径与同步边界

## 跨设备约定

`<checkout>` 表示已通过 `git rev-parse --show-toplevel`、`git status -sb`、`git log -1` 核实的工作目录。目录同名不代表同版本。移动到另一设备时先核对 Git 远端、分支、HEAD 与未提交差异，再操作；构建缓存不得跨 checkout 混用。

| 对象 | 有效映射 | 边界 |
| --- | --- | --- |
| 远端仓库 | `https://github.com/OldLai-ctrl/HarmonyContinuation.git` | 开发主线 `v0.9/dev`；`library/v3` 保留已完成内容历史 |
| 主开发 checkout | `HarmonyContinuation-release/` → `v0.9/dev` | 独立 Git 仓库；不依赖旧目录的 worktree 元数据 |
| 旧目录归档 | 同级 `_archive/HarmonyContinuation-20261008/` | 保留三个旧目录全部文件；仅用于恢复。归档入口记录本机恢复条件，不在归档内开发 |
| SDK 配置 | `VST3_SDK_ROOT` 环境变量或 CMake 缓存项 | 完整离线 SDK；各设备实际路径待核实，不提交 SDK |
| 工具配置 | 本机开发终端的 CMake / MSVC / Ninja，打包脚本的 `Iscc` / `RuntimeDirectory` | 实际版本与路径按操作前置条件核实 |
| 构建与打包输出 | `<checkout>/build-v3/`、`build-v3-plugin/`、`build-installer/` | RC4 VST3 验证产物使用 `build-installer/rc-0.9.0-rc.4-color-visibility/`，不生成安装器；统一 RC3 使用 `build-installer/rc-0.9.0-rc.3-unified/`，RC1/RC2 目录保留，不覆盖正式产物；`.gitignore` 排除，另一设备独立构建 |

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
