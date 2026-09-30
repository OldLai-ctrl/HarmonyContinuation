# 独立进行库与 Windows 安装程序

当前构建：`0.8.0-dev.1-installer.1`，在 v0.8 开发分支增加本地库管理与安装器；没有实现 MIDI 导入。

## 安装、更新、卸载

- 完整安装包：`HarmonyContinuation-0.8.0-dev.1-installer.1-Setup.exe`。默认安装 VST3 与本地进行库，也可选择只更新库。
- 独立库安装包：`HarmonyContinuation-Library-2-Setup.exe`。以后更新进行内容只需分发新版库安装包，无需重新构建或替换插件。
- 已安装的应用中可以卸载；完整安装还提供开始菜单的库更新和卸载入口。
- 更新插件前关闭使用插件的宿主。安装器拒绝替换被占用的插件，不主动关闭宿主。
- 所有包离线可用。这里的“更新”是运行新版安装包，尚无联网检查或自动下载。
- 安装器要求管理员权限，安装到 Windows 标准 VST3 目录；隔离测试包使用独立路径，不登记系统安装项。
- 未签名的开发包可能出现 Windows 的发布者提示。尚未做本轮 Cubase 人工验收。

## 数据位置与保存规则

| 内容 | 位置 | 更新或卸载时 |
|---|---|---|
| VST3 插件 | `%CommonProgramFiles%\VST3\HarmonyContinuation.vst3` | 更新替换，卸载移除 |
| 内置库历史 | `%ProgramData%\HarmonyContinuation\Libraries\Factory\<版本>\factory.db` | 保留所有已安装版本 |
| 当前库指针 | 同目录 `active.txt` | 验证成功后原子切换 |
| 个人进行库 | `%LOCALAPPDATA%\HarmonyContinuation\user.db` | 保留，继续使用原位置 |
| 维护程序 | `%ProgramFiles%\HarmonyContinuation` | 随应用卸载移除 |

个人数据库与内置数据库分别管理，不合并覆盖。旧工程无需迁移。插件实例重新打开时载入当前库；独立库损坏或丢失时回退到插件随附库，并在库界面报告原因。过去安装的库版本继续保存在本地，程序默认使用当前版本。插件与库管理程序卸载均保留进行数据；需要彻底清理时由用户自行备份和删除上述数据目录。

## 本次架构调整

1. `LibraryStore` 集中管理本机库与个人库路径，供插件、后台推荐及 Demo 共用。
2. 库内容版本与程序版本分开；数据库格式仍为 1，新内容版本无需修改插件版本判断。
3. 安装库前检查数据库及进行数据，复制后再次确认；采用不可变版本目录与原子切换。拒绝同版本不同内容，旧安装包不能降级当前库，并发更新互斥。
4. 界面显示实际加载的库版本，避免一直显示编译时固定版本。
5. 测试用内嵌进行从生产数据库解析模块移出，只有开发测试目标链接这些数据。
6. 修复数据库打开失败时的句柄释放；增加数据库大小、条目数和单条数据的读取上限。
7. 标准安装器代替手工复制，随包携带 MSVC 运行库，不要求用户安装开发工具。

## 生成新库包

1. 编辑 `data/factory/*.json`，保留稳定 ID 和现有格式。
2. 用 `db_compiler data/factory output/factory.db 3` 编译，第三个参数是独立库内容版本。修改内容必须递增版本。
3. `tools/build-installer.ps1 -LibraryOnly -FactoryDatabase output/factory.db` 生成独立库安装包及 SHA-256 校验文件。
4. 如果需要完整安装包，先构建 Release 插件和 `library_manager`，再运行 `tools/build-installer.ps1`。

构建需要 Inno Setup 6.7+；可通过 `-Iscc` 指定编译器、`-RuntimeDirectory` 指定 MSVC x64 可分发运行库。当前工作区使用下载后验证发布者签名的 Inno Setup 6.7.3，以便携带方式放在构建目录中，没有安装到系统。

安装脚本中文翻译来自 [Inno Setup 官方源码中的 ChineseSimplified.isl](https://github.com/jrsoftware/issrc/blob/main/Files/Languages/ChineseSimplified.isl)，保留文件内贡献者说明。安装和更新沿用 [Inno Setup 的追加卸载记录机制](https://jrsoftware.org/ishelp/topic_appendnotes.htm)。

## 验证范围

只运行本次改动相关的库保存/更新、个人库读写、实际隔离安装/更新/卸载检查；构建附带 VST3 Validator。未重复运行和声基线或全量测试，也不把隔离安装视为 Cubase 验收。
