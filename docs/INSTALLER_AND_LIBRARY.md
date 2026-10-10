# Windows 统一安装管理器

当前正式版：0.9.0，Factory Library V3 / 629 条，唯一面向用户的包为 `HarmonyContinuation-0.9.0-Setup.exe`。保留 RC4 修复和 RC3 统一组件维护逻辑；仅正式输出文件名和版本说明改变，内部管理入口继续使用 HarmonyContinuation-Setup.exe。实际提交、哈希和复用证据见 [NOW](../NOW.md)。

## 安装与维护

沿用 Inno Setup 6.7+，`packaging/UnifiedSetup.iss` 为当前入口，`tools/build-installer.ps1` 仅生成一个 Setup。原 `HarmonyContinuation.iss` 保留作旧版迁移 fixture，不再生成面向用户的双安装包。运行新版 Setup 或开始菜单「管理安装」即可离线安装、更新、修复、修改组件、完整卸载；Windows 只有一个 HarmonyContinuation 管理入口。无联网更新。

- 默认同时选择 VST3 和 Factory Library；可只装其中一个。
- 安装/更新或修复页取消勾选只代表跳过，现有组件保持原状。
- 修改组件使用独立「明确移除」页，删除项会从本次安装选项排除；可移除插件或曲库。两者均移除但仍保留管理器时可重新添加；完整卸载才移除管理入口。
- 修复恢复所选组件的官方文件。已登记的损坏 Factory 可修复；无法确认归属的同版本不同内容不覆盖。修改过的托管文件先修复再卸载，避免删除不明内容。
- 检测插件 moduleinfo 的实际版本和 Factory active.txt；较新插件降级需明确确认，静默默认拒绝；较新 Factory 始终保留不降级。仅库更新遇到旧于 dev.2 或无法确认的现有插件时拒绝并提示一起升级；无插件时提示数据需要兼容主程序。
- 只装 VST3 不含隐藏 Factory 副本，未装官方库时官方库推荐不可用；可随时添加数据组件。只装库不重新复制插件。

## 文件归属与数据保护

| 对象 | 位置 | 管理边界 |
| --- | --- | --- |
| 插件 | `%CommonProgramFiles%/VST3/HarmonyContinuation.vst3` | 哈希清单逐文件移除，不递归清空目录；未管理文件保留 |
| Factory V3 | `%ProgramData%/HarmonyContinuation/Libraries/Factory/3/factory.db` | 安装选择明确接管经验证的官方 V3；仅移除已登记且哈希匹配的数据文件 |
| 活动指针 | 同一 Factory 根目录 `active.txt` | 仅在指向被移除的 V3 时清除；其它版本指针不动 |
| 历史 Factory | 同根目录其它版本 | 不删除，不自动激活历史版本 |
| 个人库、收藏与配置 | 原 `%LOCALAPPDATA%/HarmonyContinuation/` 等位置 | 不读取、迁移或删除；无删除个人数据选项 |
| 管理器 | `%ProgramFiles%/HarmonyContinuation` | Setup 自身、运行依赖、库工具、组件记录和原卸载器；完整卸载删除已登记文件 |

安装前要求关闭占用插件/数据库的宿主；不强关 DAW，不强制覆盖。既有库工具仍执行格式验证、不可变版本目录及原子指针切换。安装器为直接涉及的文件和指针保留本次恢复副本，失败/取消时恢复；插件文件复制仍复用 Inno Setup。强断电/系统崩溃后的自动跨进程事务恢复不在本轮实现，必要时重新运行修复。

拒绝接管链接目录、无法验证的组件清单或无注册信息的手工插件；这些情况保留文件并提示用户备份后处理。默认保留用户目录、其它 VST3 和宿主文件。未签名正式安装包可能显示 Windows 发布者提示。

## 旧双安装器迁移

复用 `HarmonyContinuation-Application` AppId 和原应用路径，沿用 [Inno 追加卸载记录](https://jrsoftware.org/ishelp/topic_appendnotes.htm)，不执行旧主程序卸载器后再覆盖新文件。

只对 `HarmonyContinuation-FactoryLibrary` 的已知标准路径、Publisher 与精确卸载路径均匹配的旧记录，先运行它自己的卸载器；旧契约只移除曲库维护工具，Factory 历史和用户数据保留。确认旧记录消失后才继续安装统一管理器，不强删注册表。无法可靠匹配的旧路径/身份停止并提示先使用原卸载器。

旧主程序未选择更新时其文件不变。首次仅库迁移不会自动接管旧插件逐文件清单，因此需先对 VST3 执行一次更新/修复后，才能通过新的组件移除页安全删除它；完整卸载仍沿用原主程序的卸载记录。更新插件时仅移除哈希匹配的旧官方 bundled Factory 副本，使数据组件独立；未知内容保留并阻止接管。旧较早手工备份目录不再由新安装器自动移动，重复扫描时按原说明手工确认处理。

## 构建与定向验收

从干净提交构建 Release 插件和 library_manager，关闭 Validator；`tools/build-installer.ps1 -BuildDirectory <build> -OutputDirectory <RC独立目录> -Iscc <ISCC> -RuntimeDirectory <CRT>`。输出唯一 Setup、SHA256SUMS.txt 和暂存清单。`-LibraryOnly` 已拒绝；数据组件通过统一 Setup 的 `/COMPONENTS=library` 选择。

`tools/test-unified-installer.ps1` 只验证安装器范围：隔离路径、独立 per-user 测试 AppId，合并全新安装、组件增删/修复、旧双记录迁移、独立更新、文件占用拒绝与完整卸载。测试个人库仅为专用哨兵，不访问真实 user.db；测试包不分发。实际通过/失败范围以 NOW 为准；不运行音乐回归、Validator 或 DAW 矩阵。真实标准目录/UAC 与 Cubase/FL 验收交给用户，见 [人工清单](RC_DAW_ACCEPTANCE.md)。

不分发购买的原 PDF、Excel、付费数据库或转换副本。Factory / User / Session / Snapshot Schema 保持 2 / 2 / 5 / 3 / 2。
