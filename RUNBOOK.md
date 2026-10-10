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
cmake -S . -B build-v09 -G "Ninja Multi-Config" -DVST3_SDK_ROOT="$hcSdk" -DHC_PRERELEASE=rc.3 -DSMTG_RUN_VST_VALIDATOR=OFF
cmake --build build-v09 --config Release --target HarmonyContinuation library_manager -j 6
```

多配置构建使用 `Release/library_manager.exe`；`tools/build-installer.ps1` 也支持元数据为 Release 的单配置 Ninja 根目录 `library_manager.exe`，VST3 bundle 仍在 `VST3/Release/`。子进程无法读 Git 时可传当前 `git rev-parse --short=7 HEAD` 到 `HC_GIT_COMMIT_OVERRIDE`；不能沿用旧覆盖值。验证生成的 `ProductVersionGenerated.h` 与 `moduleinfo.json`；构建后置 Validator 结果也要记录，避免重复运行。

## 局部自动验收

RC3 只构建一次 Release 插件和 library_manager；一次生成统一 Setup，运行 `tools/test-unified-installer.ps1` 的两个隔离生命周期，检查首次安装、组件独立更新/增删/修复、旧双记录迁移、文件占用及完整卸载。测试包只写编译进的项目内路径及独立 HKCU 测试 AppId，不操作生产数据；失败只定向修复和继续未通过步骤。打包说明见 [统一安装器](docs/INSTALLER_AND_LIBRARY.md)。不运行插件测试或回归。

以下 RC2 为历史入口，本轮不执行：RC2 仅构建一次 Release 的插件、library_manager 与 WhyV2Smoke，直接运行 `WhyV2Smoke rc2-hints` 一次，合并普通完整路径、截图 OPEN 静态/时值回退、多方向中性回退、开关及 Continuation 色条像素检查；不运行旧冒烟入口。仅本轮定向项目通过后打包，复用已有 factory.db 与 dev.5–dev.8 证据；一次调用 build-installer.ps1，输出到独立 build-installer/rc-0.9.0-rc.2 目录，检查版本、Git 标识、关键文件、依赖和 SHA-256，不运行安装生命周期或测试程序。人工验收见 [RC 清单](docs/RC_DAW_ACCEPTANCE.md)。

以下 dev.8 为历史入口，本轮不执行：只构建一次 Release 的 `HarmonyContinuation` 和 `WhyV2Smoke`，直接运行后者一次，合并一个 Continuation 与一个 Enrichment 的卡片/Why?/开关 UI 场景，顺带未知状态及 Preview/MIDI 身份。复用 dev.5–dev.7 算法证据；若失败仅指定 `continuation` 或 `enrichment` 定向重验。禁止扩大为历史回归或 UI 矩阵，通过后停止。详见 [Why? V2](docs/WHY_V2.md)。

以下 dev.7 为历史入口，本轮不执行：只构建一次 Release 的 `HarmonyContinuation` 和 `EnrichmentColorSmoke`，直接运行后者一次，合并一个完整 Enrichment 张力目标、关闭恢复及重排卡片/Why/Preview/MIDI/比较身份的定向冒烟。复用 dev.5/dev.6，不执行下列历史入口，不扩展矩阵；通过即停止，不启动 dev.8。

以下 dev.6 为历史入口，本轮不执行：只构建一次 Release 的 `HarmonyContinuation` 和 `ColorPreferenceSmoke`，直接运行后者，覆盖关闭身份/顺序、一个张力弧目标、Unknown/Uncertain 回退和恢复原排序；同一场景顺带绘制工具栏及 Why?，不额外扩展 UI 矩阵。dev.5 证据复用，详见 [色彩排序](docs/COLOR_PREFERENCE_RERANKER.md)。通过即停止，不启动 dev.7。

以下 dev.5 为历史入口，本轮不执行：只构建一次 Release 的 `HarmonyContinuation` 和 `HarmonyColorSmoke`，直接运行 `HarmonyColorSmoke.exe`：三个色彩代表案例加一个中英 UI / Why? / 开关冒烟。该入口不注册 CTest，不触发历史套件；通过即停止。详见 [Color Analysis V1](docs/COLOR_ANALYSIS_V1.md)。

以下 dev.4 主线集成步骤为历史入口，本轮不执行：只编译一次 Release，显式构建插件及 `FactoryCatalogTests`、`LibraryCompatibilityTests`，直接运行 `FactoryCatalogTests --smoke-one`（一例 Factory V3 加载/推荐）和 `LibraryCompatibilityTests user-smoke`（一个隔离旧 Schema 1 用户库恢复案例）。只在实际改动宿主连接入口时增加一次 Generic Host 加载；不因下面历史入口存在而执行它们。全量测试统一推迟到 v1.0 正式发布前，v0.9 正式版也不执行全量回归。通过即停止，本次不重做安装包或安装验证。

前置：相关测试目标已构建。搜索 `CMakeLists.txt` 的 `add_test` 获取真实测试名，先 `ctest --test-dir build-v09 -C Release -N`，再按修改选择 `-R`。例如：

Factory V3 兼容桥只构建 `LibraryCompatibilityTests`，运行 `ctest --test-dir <build> -R '^LibraryV3_' --output-on-failure`。Library 2/3 数据 fixture 和 user.db 均在该测试独有的临时目录内创建、关闭、重新打开及清理，不读生产用户目录。具体格式、降级和测试边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。新生产 DB 目标 `factory_database` 默认 Library 3；`factory_legacy_database` 只用于历史 Library 2 测试。

V3 收口只运行一次已有编译器：`db_compiler data/factory <build>/factory.db 3 data/factory-v3 LIBRARY_V3_SUMMARY.md`。它校验字段、和弦、统一音乐去重、元数据合并和 DB 读回，并输出短统计；不要另跑相同 lint。必要的新增内容冒烟运行 `FactoryCatalogTests --smoke`，仅两条代表案例走加载→推荐→MIDI；低音/转位/扩展音路径未改时复用已有 18 类证据。此前全新增内容消费者检查已通过，不因入口存在而重跑 `FactoryV3Catalog`、兼容桥四组或音乐基准。失败只修复受影响内容，再按当前任务授权校验。

```powershell
ctest --test-dir build-v09 -C Release -R '^(HostContractFLStudio|HostContractGeneric|HostContractCubase|EditorHostSmoke)$' --output-on-failure
```

其他阶段按任务授权选择 MIDI、状态或 UI 的定向入口；未改音乐算法时不因文档或名称变化重跑音乐基线。完整 CTest、Validator、42 例续写、30 例升级及其他全量回归统一留到 v1.0 正式发布前，不作为 v0.9 的关口。续写基准原有 3 处结构提示可能导致非零退出，不能自动调权重消除它们。命令参数搜索对应 CLI 的 usage。

## 真机验收

前置：宿主已关闭并安装正确包，确认版本/提交/哈希与 Factory 版本。FL 使用 `docs/FL_STUDIO_TEST_GUIDE.md`；Cubase 按需求搜索 `docs/CUBASE_TIMELINE_SYNC_TEST.md` 和版本验收文档。记录宿主完整版本、操作、结果及最小复现，区分通过/失败/未测。模拟宿主、Demo、内存往返不能证明真实项目重开。更新 NOW 的待验收项，持久风险更新 RISKS。

## 打包与安装

前置：已确认的 Release 构建、Inno Setup 6.7+、可分发的 MSVC x64 CRT、有效 factory.db。搜索并读取 `tools/build-installer.ps1` 的参数段，再显式传 `-BuildDirectory build-v3-plugin -Iscc <本机ISCC> -RuntimeDirectory <本机CRT>`。只更新库由同一 Setup 选择 Factory Library 组件，脚本不再生成独立库安装器。

统一安装逻辑变动时按 `tools/test-unified-installer.ps1` 的隔离包前置条件验收；需要新的工作区内隔离路径，禁止当成生产安装器测试。生产安装先关闭宿主，需要管理员写入标准目录；保留用户库和历史 Factory。`verify-installed-build.ps1` 的默认路径来自旧 build-vst3 和随包 DB，使用独立库时须显式指定实际工作区/安装二进制及实际活动库路径；完整包另核对语言与模块元数据哈希。

旧 V3 九项安装证据保留为历史。本轮统一安装器有实际逻辑变动，只做一次隔离生命周期，覆盖独立组件维护、旧双安装器迁移、文件占用拒绝和保留个人数据的完整卸载。从干净提交构建一次 Release，关闭 Validator、清除旧 Git override；只生成 `HarmonyContinuation-Setup.exe` 及 `SHA256SUMS.txt`。核对插件、Library 3 / Schema 2、版本和 Git 标识；完整提交及证据随包记录在 `BUILD_INFO.json`。通过后停止，不进入音乐回归。

## 交付与同步

明确源码 HEAD 与生成包的编译提交；包附版本、提交、配置、测试范围、SHA-256 和待测项。打包脚本生成 Setup 校验文件，分发 ZIP 还需自己的清单。未执行的步骤写未测。查 NOW 的授权与待同步状态后再提交/推送指定分支；发布、merge、tag 或正式升级需当前任务授权。完成后精简 NOW，按 AGENTS 规则更新相关记录。
