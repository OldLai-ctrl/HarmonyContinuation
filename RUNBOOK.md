# 按任务运行步骤

先搜索所需标题，只读取对应步骤。以下命令是未在本轮执行的模板，需结合已配置方法与本机环境核实；列出命令不代表执行成功，也不授予安装、提交或发布权限。实际对象按 [MAP.md](MAP.md) 核实，任务允许对应操作后再执行。

## 接手与切换

在目标 checkout 执行 `git status -sb`、`git remote -v`、`git log -1 --oneline`；搜索 NOW 的当前任务。对不上基线先定位分支、未提交内容或待同步提交，不能由目录名推断版本。切换分支、清理目录或恢复旧产物前读 RISKS 的对应条目；保留开发者未提交文件。

## 构建

前置：CMake 3.25+、C++20/MSVC x64、完整离线 VST3 SDK 3.8.x（当前基线使用 3.8.1）。在已初始化 MSVC x64 的开发终端使用本机 CMake / Ninja。用全新或匹配此 checkout 的构建目录；已有缓存不得直接改生成器。

```powershell
# 先配置本机已核实的 SDK 环境变量；缺失时停止配置。
$hcSdk = $env:VST3_SDK_ROOT
if ([string]::IsNullOrWhiteSpace($hcSdk)) { throw 'VST3_SDK_ROOT 待配置并核实' }
cmake -S . -B build-v09 -G "Ninja Multi-Config" -DVST3_SDK_ROOT="$hcSdk" -DHC_PRERELEASE=dev.1
cmake --build build-v09 --config Release -j 6
```

多配置构建使用 `Release/library_manager.exe`；`tools/build-installer.ps1` 也支持元数据为 Release 的单配置 Ninja 根目录 `library_manager.exe`，VST3 bundle 仍在 `VST3/Release/`。子进程无法读 Git 时可传当前 `git rev-parse --short=7 HEAD` 到 `HC_GIT_COMMIT_OVERRIDE`；不能沿用旧覆盖值。验证生成的 `ProductVersionGenerated.h` 与 `moduleinfo.json`；构建后置 Validator 结果也要记录，避免重复运行。

## 局部自动验收

前置：相关测试目标已构建。搜索 `CMakeLists.txt` 的 `add_test` 获取真实测试名，先 `ctest --test-dir build-v09 -C Release -N`，再按修改选择 `-R`。例如：

Factory V3 兼容桥只构建 `LibraryCompatibilityTests`，运行 `ctest --test-dir <build> -R '^LibraryV3_' --output-on-failure`。Library 2/3 数据 fixture 和 user.db 均在该测试独有的临时目录内创建、关闭、重新打开及清理，不读生产用户目录。具体格式、降级和测试边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。新生产 DB 目标 `factory_database` 默认 Library 3；`factory_legacy_database` 只用于历史 Library 2 测试。

V3 收口只运行一次已有编译器：`db_compiler data/factory <build>/factory.db 3 data/factory-v3 LIBRARY_V3_SUMMARY.md`。它校验字段、和弦、统一音乐去重、元数据合并和 DB 读回，并输出短统计；不要另跑相同 lint。必要的新增内容冒烟运行 `FactoryCatalogTests --smoke`，仅两条代表案例走加载→推荐→MIDI；低音/转位/扩展音路径未改时复用已有 18 类证据。此前全新增内容消费者检查已通过，不因入口存在而重跑 `FactoryV3Catalog`、兼容桥四组或音乐基准。失败只修复受影响内容，再按当前任务授权校验。

```powershell
ctest --test-dir build-v09 -C Release -R '^(HostContractFLStudio|HostContractGeneric|HostContractCubase|EditorHostSmoke)$' --output-on-failure
```

MIDI 选 MidiImport / MidiProfile，状态/库选 Productization / LibraryStore，UI 选 UILayout 和对应 smoke。未改音乐算法时不因文档或名称变化重跑音乐基线。完整 CTest、Validator、42 例续写、30 例升级和 Demo 只在实际需要的集中关口执行；失败先局部修复。续写基准原有 3 处结构提示可能导致非零退出，需比较具体结果，不能自动调权重消除它们。命令参数搜索对应 CLI 的 usage。

## 真机验收

前置：宿主已关闭并安装正确包，确认版本/提交/哈希与 Factory 版本。FL 使用 `docs/FL_STUDIO_TEST_GUIDE.md`；Cubase 按需求搜索 `docs/CUBASE_TIMELINE_SYNC_TEST.md` 和版本验收文档。记录宿主完整版本、操作、结果及最小复现，区分通过/失败/未测。模拟宿主、Demo、内存往返不能证明真实项目重开。更新 NOW 的待验收项，持久风险更新 RISKS。

## 打包与安装

前置：已确认的 Release 构建、Inno Setup 6.7+、可分发的 MSVC x64 CRT、有效 factory.db。搜索并读取 `tools/build-installer.ps1` 的参数段，再显式传 `-BuildDirectory build-v09 -Iscc <本机ISCC> -RuntimeDirectory <本机CRT>`；脚本默认 build-v08，不能直接沿用。库单独更新使用 `-LibraryOnly -FactoryDatabase <已验证DB>`。

安装逻辑变动时按 `tools/test-installer.ps1` 的隔离包前置条件验收；需要新的工作区内隔离路径，禁止当成生产安装器测试。生产安装先关闭宿主，需要管理员写入标准目录；保留用户库和历史 Factory。`verify-installed-build.ps1` 的默认路径来自旧 build-vst3 和随包 DB，使用独立库时须显式指定实际工作区/安装二进制及实际活动库路径；完整包另核对语言与模块元数据哈希。

V3 已有 `test-v3-installer.ps1` 的一次隔离生命周期、9 项通过结果。安装器逻辑未改时直接复用，不重复安装/卸载。本次从已提交且干净的源码构建一次 Release，关闭构建附带 Validator，清除旧 Git override；生成完整包、Library 3 包及 `SHA256SUMS.txt`，一次确认插件、打包 DB 的 Library 3 / Schema 2、版本、Git 标识与校验和。成功后停止，不自动进入完整回归。完整提交及产物证据随包记录在 `BUILD_INFO.json`。

## 交付与同步

明确源码 HEAD 与生成包的编译提交；包附版本、提交、配置、测试范围、SHA-256 和待测项。打包脚本生成 Setup 校验文件，分发 ZIP 还需自己的清单。未执行的步骤写未测。查 NOW 的授权与待同步状态后再提交/推送指定分支；发布、merge、tag 或正式升级需当前任务授权。完成后精简 NOW，按 AGENTS 规则更新相关记录。
