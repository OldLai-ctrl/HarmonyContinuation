# 2026-09-25 编辑器打开卡住：排查与修复

## 现象

用户在 Cubase 15 中可以插入 Phase 1 插件，但打开编辑器后宿主无响应、没有白屏。修复前已核对 Cubase 安装版与当时工作区构建版的 SHA-256 完全一致，排除了安装文件版本不一致。

## 定位

用本地 VST3 SDK 的 EditorHost 打开同一插件，可复现打开阶段停滞。Windows 调试器在数秒后仍将主线程定位在 `PluginView::open → Controller::attach → std::function` 的回调赋值；指令位置异常地落在堆地址。排查 Ninja 构建记录发现 `PluginEntry.cpp.obj` 与 `PluginView.cpp.obj` 的头文件依赖数都是 **0**，且它们的编译时间早于 Phase 1 对 `Controller.h` 的类布局修改。`PluginEntry.cpp` 中内联的 `Controller::create` 与 `PluginView.cpp` 中的 `MainView` 创建因此可能沿用旧对象大小，而其他文件已按新布局编译。插入插件不需要创建界面；打开时才触发损坏。

此机中文 MSVC 的 `/showIncludes` 输出未被当前 CMake/Ninja 规则可靠识别。只构建被修改的 `.cpp` 无法保证使用这些头文件的对象同步更新。

## 修复

`CMakeLists.txt` 为本项目 `src/`、`tools/`、`tests/` 内的 C++ 源文件显式列出本项目头文件依赖。此项目文件规模小，头文件变化时多编译几个对象可接受；类布局始终重新对齐。重新配置后 Ninja 的 `-t query` 已确认 `PluginEntry.cpp.obj` 和 `PluginView.cpp.obj` 都直接依赖 `Controller.h`、`MainView.h` 等头文件。随后完整重编译了上述旧对象。

## 本轮验证

- 修复后完整插件构建成功；VST3 Validator **47/47**。
- 修复后 SDK EditorHost 可打开编辑器，主线程进入正常的 `PluginView::notify` 计时循环；此前在 `Controller::attach` 的停滞不再出现。
- Cubase 安装目录中的旧插件已备份，修复版已复制并经 SHA-256 核对一致。旧版备份路径以本机 `install-debug.ps1` 输出为准。
- Cubase 本体的再次打开需要在用户的实际宿主会话中确认；EditorHost 验证不能代替这一项。
