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
cmake -S . -B build-v09 -G "Ninja Multi-Config" -DVST3_SDK_ROOT="$hcSdk" -DHC_PRERELEASE= -DSMTG_RUN_VST_VALIDATOR=OFF
cmake --build build-v09 --config Release --target HarmonyContinuation library_manager -j 6
```

多配置构建使用 `Release/library_manager.exe`；`tools/build-installer.ps1` 也支持元数据为 Release 的单配置 Ninja 根目录 `library_manager.exe`，VST3 bundle 仍在 `VST3/Release/`。子进程无法读 Git 时可传当前 `git rev-parse --short=7 HEAD` 到 `HC_GIT_COMMIT_OVERRIDE`；不能沿用旧覆盖值。验证生成的 `ProductVersionGenerated.h` 与 `moduleinfo.json`；构建后置 Validator 结果也要记录，避免重复运行。

### dev.3 Cubase 输入可靠性构建

使用独立 `build-v4-dev3`，Release / Ninja，配置 `-DHC_PRERELEASE=dev.3 -DHC_FACTORY_LIBRARY_VERSION=4 -DSMTG_RUN_VST_VALIDATOR=OFF -DSMTG_CREATE_PLUGIN_LINK=OFF`。从源码提交构建 HarmonyContinuation、library_manager 和直接相关测试；源码版本写入 ProductVersionGenerated.h，包内 Build ID 必须与源码一致。统一包由 `tools/build-installer.ps1 -BuildDirectory build-v4-dev3 -OutputDirectory build-installer/v4-0.10.0-dev.3 -Iscc <已核实ISCC>` 生成。保留旧 dev.1/dev.2 与 v0.9.0 产物。

只运行相关 12 项检查，具体名称及证据见 [dev.3 报告](docs/DEV3_CUBASE_RELIABILITY_FIX.md)。新 CubaseReliabilityTests 生成明确标记的模拟字段冲突对照；不可替代真实宿主。真实采集操作见 [人工清单](docs/V4_DAW_ACCEPTANCE.md)。诊断只有用户主动导出才写本地文件。

安装检查复用 `tools/test-dev2-upgrade.ps1 -ProductVersion 0.10.0-dev.3 -PackageDirectory build-installer/v4-0.10.0-dev.3 -TestRoot <工作区内全新隔离目录> -Iscc <已核实ISCC> -Cases fresh,official-v09,rc-manual,unknown,official-content-layout`。旧版本夹具和 RC 参考须已核实；不得指向生产安装。受限环境如禁止独立随机 HKCU 测试登记，应保留回滚日志，在获准的隔离环境重跑，不通过关闭保护判断绕过。当前这 5 类共 413 检查通过，日志随包保存；dev.2 历史安装证据见 [升级修复](docs/INSTALLER_DEV2_UPGRADE_FIX.md)。

## 局部自动验收

Factory V4 产品化：在 `build-v4-dev` 配置 `-DHC_FACTORY_LIBRARY_VERSION=4 -DHC_PRERELEASE=dev.1 -DSMTG_RUN_VST_VALIDATOR=OFF`，沿用已核实离线 SDK。从实现提交构建 Release 的 `HarmonyContinuation`、`library_manager`、`FactoryV4Tests`；运行 `FactoryV4Tests <证据目录>` 得到 28 条可发现性 TSV 和 4 份 MIDI，再运行无参数契约。必要时只运行相关 Matcher / Continuation / Enrichment 检查，TEMP/TMP 指向工作区测试目录。版本 0.10.0-dev.1，默认 Factory 4；V3 源数据与正式 staging 不改。用 `tools/build-installer.ps1 -BuildDirectory build-v4-dev -OutputDirectory build-installer/v4-0.10.0-dev.1 -RuntimeDirectory <已核实CRT> -Iscc <已核实ISCC>` 制作统一包，复制 MIDI/证据和人工清单，记录构建提交与哈希。使用 `tools/test-v4-installer.ps1` 的全新隔离路径验证此次兼容拦截与独立组件保护；不操作生产安装。详见 [产品化说明](docs/FACTORY_V4_PRODUCTIZATION.md)。

v0.9.0 正式发布：从最终待 Tag 的干净 main 提交，仅构建一次 Release 插件与 library_manager，显式清空 HC_PRERELEASE / Git override 并关闭 Validator；打包一次到独立 release-0.9.0 目录，输出 HarmonyContinuation-0.9.0-Setup.exe，检查版本、Build ID、Factory、依赖与哈希。复用 RC3 28 项及 RC4 显示证据，不重复生命周期；main/Tag/构建提交一致后按授权推送，不创建 GitHub Release 页面。

前置：相关测试目标已构建。搜索 `CMakeLists.txt` 的 `add_test` 获取真实测试名，先 `ctest --test-dir build-v09 -C Release -N`，再按修改选择 `-R`。例如：

Factory V3 兼容桥只构建 `LibraryCompatibilityTests`，运行 `ctest --test-dir <build> -R '^LibraryV3_' --output-on-failure`。Library 2/3 数据 fixture 和 user.db 均在该测试独有的临时目录内创建、关闭、重新打开及清理，不读生产用户目录。具体格式、降级和测试边界见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md)。正式 main 默认 Library 3；当前 library/v4 默认 Library 4。`factory_legacy_database` 只用于历史 Library 2 测试。

V3 收口只运行一次已有编译器：`db_compiler data/factory <build>/factory.db 3 data/factory-v3 LIBRARY_V3_SUMMARY.md`。它校验字段、和弦、统一音乐去重、元数据合并和 DB 读回，并输出短统计；不要另跑相同 lint。必要的新增内容冒烟运行 `FactoryCatalogTests --smoke`，仅两条代表案例走加载→推荐→MIDI；低音/转位/扩展音路径未改时复用已有 18 类证据。此前全新增内容消费者检查已通过，不因入口存在而重跑 `FactoryV3Catalog`、兼容桥四组或音乐基准。失败只修复受影响内容，再按当前任务授权校验。

```powershell
ctest --test-dir build-v09 -C Release -R '^(HostContractFLStudio|HostContractGeneric|HostContractCubase|EditorHostSmoke)$' --output-on-failure
```

其他阶段按任务授权选择 MIDI、状态或 UI 的定向入口；未改音乐算法时不因文档或名称变化重跑音乐基线。完整 CTest、Validator、42 例续写、30 例升级及其他全量回归统一留到 v1.0 正式发布前，不作为 v0.9 的关口。续写基准原有 3 处结构提示可能导致非零退出，不能自动调权重消除它们。命令参数搜索对应 CLI 的 usage。

## 真机验收

Phase4D dev.4：使用 [中文清单](docs/PHASE4D_CUBASE_ACCEPTANCE.md) 与 [修复报告](docs/PHASE4D_CUBASE_FEEDBACK_FIX_20261012.md)。工程拍号可变，XML输入不以当前宿主拍号覆盖Session；时值比例由对齐输入duration/onset估计，拍号meta不等于实际时间适配。ON/OFF是独立编译包，高级分析显示实际结果模式，宿主诊断显示编译状态；两包Phase3A均OFF。R01/R02需主动导出本地输入与推荐诊断，再与模拟证据比较。该轮完整OFF/ON回归已获用户授权并完成，历史“留到v1.0”范围不限制本轮已授权门禁。

前置：宿主已关闭并安装正确包，确认版本/提交/哈希与 Factory 版本。FL 使用 `docs/FL_STUDIO_TEST_GUIDE.md`；Cubase 按需求搜索 `docs/CUBASE_TIMELINE_SYNC_TEST.md` 和版本验收文档。记录宿主完整版本、操作、结果及最小复现，区分通过/失败/未测。模拟宿主、Demo、内存往返不能证明真实项目重开。更新 NOW 的待验收项，持久风险更新 RISKS。

## 打包与安装

前置：已确认的 Release 构建、Inno Setup 6.7+、可分发的 MSVC x64 CRT、有效 factory.db。搜索并读取 `tools/build-installer.ps1` 的参数段，再显式传 `-BuildDirectory build-v3-plugin -Iscc <本机ISCC> -RuntimeDirectory <本机CRT>`。只更新库由同一 Setup 选择 Factory Library 组件，脚本不再生成独立库安装器。

统一安装逻辑变动时按 `tools/test-unified-installer.ps1` 的隔离包前置条件验收；需要新的工作区内隔离路径，禁止当成生产安装器测试。生产安装先关闭宿主，需要管理员写入标准目录；保留用户库和历史 Factory。`verify-installed-build.ps1` 的默认路径来自旧 build-vst3 和随包 DB，使用独立库时须显式指定实际工作区/安装二进制及实际活动库路径；完整包另核对语言与模块元数据哈希。

正式包复用的 RC3 28 项安装结果保留于 `build-installer/release-0.9.0/evidence/`，旧 V3 过程记录可查 Git 历史。安装维护逻辑未改时复用，不重复生命周期；本轮正式版只版本化输出文件名。从干净提交构建一次 Release，关闭 Validator、清除旧 Git override；正式版只生成 `HarmonyContinuation-0.9.0-Setup.exe` 及 `SHA256SUMS.txt`。核对插件、Library 3 / Schema 2、版本和 Git 标识；完整提交及证据随包记录在 `BUILD_INFO.json`。通过后停止，不进入音乐回归。

## 交付与同步

### dev.5 收尾验证

使用独立 Release OFF/ON 构建，Factory 4、`HC_PRERELEASE=dev.5`，保留 dev.4 包。先初始化本机 MSVC x64 环境；核对缓存中 `CMAKE_CXX_FLAGS_RELEASE` 确实包含 `/O2 /Ob2 /DNDEBUG`，不能把空优化参数的缓存当最终性能基线。关闭自动安装链接和构建后 Validator，从实际源码提交生成 Build ID。

最终运行所有已注册 CTest；33 项包含 1440 严格矩阵及 33×四组合专项。可选候选门禁复用 `Phase4DTests <独立证据目录> <隔离候选DB>`，插件默认仍使用 V4/657。候选 DB 编译方法见 `data/factory-proposals/20261012/README.md`。主报告与包内证据记录最终源码提交、逐项结果和已知限制；真实宿主清单见 `docs/DEV5_CUBASE_ACCEPTANCE.md`。

本机MSVC PDB工具不匹配时，构建进程设置 `_CL_=/Z7`，使用对象内调试信息；无需修改离线SDK。测试将TEMP/TMP设为独立工作区目录，避开沙箱临时目录的原子替换权限限制。当前构建提交、完整33项结果、21项隔离安装结果及日志入口见 [dev.5收尾报告](docs/DEV5_CLOSEOUT_20261012.md)。

### dev.5 另一设备接手

1. 源码使用 `development/dev5-closeout-20261012`。新目录可运行 `git clone --branch development/dev5-closeout-20261012 https://github.com/OldLai-ctrl/HarmonyContinuation.git HarmonyContinuation-release`。已有checkout先确认并保管未提交内容，再fetch、切换该分支和 `git pull --ff-only`；不强制reset。
2. 登录有仓库权限的账号，从 [既有未发布开发草稿](https://github.com/OldLai-ctrl/HarmonyContinuation/releases/tag/untagged-45999c0f91b6f2bf11c1) 下载 `HarmonyContinuation-dev5-Handoff-20261012.zip` 和 `HANDOFF_DEV5_SHA256SUMS.txt`。ZIP SHA-256为 `6c201735e87c34b6c6f967f70a9b717ce8070909bb1fbf82cebc7e82cefa65de`；解压后再核验内部 `SHA256SUMS.txt`。草稿继续保持未发布。
3. 先读 AGENTS、NOW、`docs/DEV5_CLOSEOUT_20261012.md` 和 `docs/DEV5_CUBASE_ACCEPTANCE.md`。包内含OFF Setup、OFF/ON便携ZIP、原V4/657、候选V5/658、源码和811项交接文件及证据；二进制BuildCommit为 `730e819`，包内文档冻结于 `c117163`。最新设备接续记录通过Git补充，不改变冻结二进制身份。
4. 本机ON安装被UAC取消，不能当作已经安装。新设备关闭宿主后备份完整旧bundle，只安装一个同身份OFF/ON版本；About核对dev.5 / `730e819` / HF实际状态 / P3 OFF，再按中文清单验收。现有个人库、真实工程、SDK与工具链不在包内，也不随Git同步；需要迁移个人数据时由用户单独处理。
5. 当前自动化门禁已通过，不因换设备机械重跑。继续开发时重新核实本机MSVC/CMake/Ninja、Inno、CRT及离线SDK路径，使用独立构建目录；下一工作包以真实Cubase、R02两组本地诊断及音乐试听为主，不自动合并main、创建正式Tag或发布。

### 历史 Factory V4/dev.1 另一设备接手

1. 登录有仓库写权限的 GitHub 账号，打开 [未发布的 V4 交付草稿](https://github.com/OldLai-ctrl/HarmonyContinuation/releases/tag/untagged-45999c0f91b6f2bf11c1)。下载 `HarmonyContinuation-0.10.0-dev.1-Handoff.zip`、`HANDOFF_SHA256SUMS.txt`；只需人工安装时也可单独下载同页 Setup。保持草稿未发布。
2. 用 SHA-256 对照外层校验文件验证 ZIP；解压到新目录，再按包内 `SHA256SUMS.txt` 校验文件。包内含 Setup、`stage-4` 的 VST3/Factory DB、`midi`、`evidence`、BUILD_INFO 和人工清单。原始产品交付源码提交 `963c9c7`，实际插件/Setup 构建提交 `86a8641`，版本 `0.10.0-dev.1`；后续交接提交不改变二进制身份。
3. 新设备克隆现有仓库或先处理已有工作区未提交改动，再切到 `library/v4` 并 fast-forward 更新；核对 `git status -sb`、`git log -1` 和远端。不要强制重置或覆盖另一设备未提交内容。读取 NOW 的当前任务及 V4 人工清单；当前下一步只有 Cubase/FL Studio 人工验收，两者对此构建均 Pending。
4. 直接验收使用统一 Setup，无需重编译。需要继续开发时按本 RUNBOOK 构建章节在该设备重新核实 MSVC/CMake/Ninja、Inno Setup、CRT 和离线 SDK 路径；SDK、工具链与本机构建缓存不在交接包内，不复制缓存来冒充可复现构建。不要为接手续跑全量回归；main/v0.9.0 和个人数据继续保护。

明确源码 HEAD 与生成包的编译提交；包附版本、提交、配置、测试范围、SHA-256 和待测项。打包脚本生成 Setup 校验文件，分发 ZIP 还需自己的清单。未执行的步骤写未测。查 NOW 的授权与待同步状态后再提交/推送指定分支；发布、merge、tag 或正式升级需当前任务授权。完成后精简 NOW，按 AGENTS 规则更新相关记录。
