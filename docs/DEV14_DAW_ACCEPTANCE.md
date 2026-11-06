# HarmonyContinuation 0.11.0-dev.14 安装与实机验收

G12 扩展试听的 12 条 D→E 路线均获用户认可，音乐研发阶段结项。本包用于 Cubase 和 FL Studio 实机验收，使用 Release x64 VST3、Factory V8 / Schema 2 / 1,288 条。BuildCommit 为 `b45916001064118464b48a8a5e82cda76581d0de`。安装包、校验值与落盘身份见交付目录的 `BUILD_INFO.json` 和 `SHA256SUMS.txt`。

## 安装或升级

1. 关闭 Cubase、FL Studio 和其他扫描 VST3 的程序。备份用于验收的工程原件、个人曲库目录 `%LOCALAPPDATA%\HarmonyContinuation`，以及当前活动 Factory 的 `active.txt` 与对应版本目录。保留原版本 Setup。若个人库有 SQLite 日志文件，随同备份。
2. 双击 `HarmonyContinuation-0.11.0-dev.14-Setup.exe`，保留 **VST3** 和 **Factory Library V8 (1288)** 两个组件。已有正常托管安装时沿用现有标准路径。若 Setup 提示手工安装冲突或文件占用，关闭宿主并按提示处理后再继续。
3. 在宿主刷新 VST3，打开插件 About 核对 `0.11.0-dev.14`、BuildCommit 前缀 `b459160` 和 Factory V8。首次查询后可在宿主诊断确认 `Factory Entries: 1288`。活动库位于 `%ProgramData%\HarmonyContinuation\Libraries\Factory`，`active.txt` 内容为 `8`，`8` 目录中有配套 `factory.db` 和 `factory.hca`。

一次隔离升级从实际装好的 dev.6 / Factory V7 开始，旧活动指针为 `7` 加换行。升级后指针为 `8` 加换行，V5、V6、V7 历史文件和隔离个人库哨兵的 SHA-256 全部保持。托管 V8 的 DB 与 HCA 校验值均写入安装状态。

## 卸载与回退

1. 关闭宿主，通过 Windows「已安装的应用」卸载当前 dev.14。托管卸载按安装状态处理本版本插件和 Factory，历史目录和个人库保留。
2. 运行所选旧版本的原 Setup，并安装它对应的插件与 Factory 两个组件。直接用旧 Setup 覆盖时，安装器会保留活动的较新 Factory，因此完整回退应先卸载 dev.14。正式稳定版可用保存的 `HarmonyContinuation-0.10.0-Setup.exe`，About 核对 `0.10.0` / `c8d8dc3`，库为 V5 / 658 条。
3. 用验收前的工程副本重开宿主，核对旧版 About 和活动库。回退到 `0.11.0-dev.8` 时，超过 64 事件的 Session5 或 Snapshot3 会被旧版拒绝读取。保留原工程和原快照，按需要用 MIDI 在旧版重建较短段落。

## Cubase 与 FL Studio 各验一次

1. **加载与导航。** 打开插件，核对 About，在匹配优先、灵感优先、升级进行、和声祈愿之间切换。
2. **推荐。** 导入一小段现有 MIDI 或输入 `F–G–Em–Am`，查看普通匹配与灵感续写。可用升级进行或一抽各确认一个结果。
3. **试听与拖放。** 选一条后续，用 Host 和 Windows 两个音频后端各试听一次。把同一候选拖入 Cubase MIDI／乐器轨，或 FL Studio 钢琴卷帘，核对音符、低音与时值。
4. **工程重开。** 保存验收工程副本，关闭后重开，确认和弦输入、时值及已保存音乐对象恢复。出现阻断时记录宿主完整版本、About 身份、最短复现和实际结果。

隔离安装和工程冒烟的范围见 `BUILD_INFO.json`。实机结果由用户在上述两个宿主中确认。
