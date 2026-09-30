# Cubase 仍显示 RC1：定位与修复

## 用户现象

完整安装后 Cubase 仍显示 `0.6.0-rc.1` / `c574d62`，无法继续 v0.8 人工验收。

## 本机证据

- 正常系统安装位置的插件为 `0.8.0-dev.1-installer.1`，SHA-256 与工作区 Release 构建一致。
- 系统 VST3 目录中还存在 `HarmonyContinuation-rc1-backup-20260929`，其模块为 `0.6.0-rc.1`。
- Cubase 15 的 `vst3plugins.xml` 同时记录了两份模块，两者 processor/controller ID 相同；扫描日志也列出两份 HarmonyContinuation。
- 完整安装日志显示新文件写入成功。因此当前阻塞是扫描目录中的旧备份冲突，不能归为新版没有复制成功。

## 已实施

- 确认 Cubase 已关闭，把 RC1 备份完整移到 `C:\ProgramData\HarmonyContinuation\Backups\Plugins\HarmonyContinuation-rc1-backup-20260929`，移动前后模块校验一致。
- 备份 Cubase VST3 缓存，仅移除该旧备份对应的一条记录；保留其余插件及新版记录。未清空全部插件缓存、未改变个人进行库。
- 完整安装器新增已知旧备份目录迁移：移出标准 VST3 扫描目录，保留旧文件；被占用、目录为链接或迁移失败时拒绝继续。已有同名备份不会被覆盖。
- 独立进行库安装器不迁移插件，和声 Core、Session 和 UI 未改动。

## 验证与待办

仅做旧备份迁移和占用拒绝的隔离安装检查；未重复音乐 benchmark、CTest 或 Validator。插件二进制保持原构建。实际扫描目录与已备份的缓存检查均只剩新版模块。

仍需用户重新打开 Cubase，确认「更多 → 关于」显示 `0.8.0-dev.1-installer.1`。在取得此人工结果前，Cubase load Gate 仍不标记 PASS。
