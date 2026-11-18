## 第三批推进：缓存、打包入口、测试纪律（v0.14.13）

- 库列表不再每帧重算：`MainView::filteredLibrary()` 以"库版本 + 全部筛选 + 搜索词"为键缓存行表与
  折叠计数；`setLibrary()` 递增 `libraryStamp_`。详情面板的 `nearDuplicate` 全库扫描也按库版本缓存。
- 打包入口修好：`tools/build-installer.ps1` 默认 `build` / `build-installer/output` / `build/factory.db`
  （之前默认指向不存在的 `build-v3-plugin`），并保留 PowerShell 5.1 需要的 BOM。
- `CMakePresets.json`：`windows-x64`（核心+测试，无需 VST3 SDK）、`windows-x64-plugin`、
  `windows-x64-library`（带 `data/library/factory-lib11.db`，HCA 走 `$env{HC_FACTORY_HCA}`）。
- `ctest` 统一加 `TIMEOUT 300` 与 `LABELS harmony`；`.github/workflows/build.yml` 跑核心构建与测试。
- 核心预设实测：19 项测试 20.8 s 全过；插件构建 `ctest` 21/21。

仍未做（第三批剩余）：Compare 死代码与两套卡片渲染器收敛、尺寸口径与 640 高卡片视口、
恢复被删掉的 7 个测试 target。
