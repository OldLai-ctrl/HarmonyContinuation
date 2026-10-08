# 有效设计决定

冲突发生时先搜索决策 ID 或模块，仅读相关条目。当前用户任务可改变决定；记录理由及取代关系，避免保留两个相反的有效结论。

## D01 按需读取与单一事实位置

已确认：沿用现有根目录交接结构，避免搬迁和重复建档。理由是各职责已经覆盖，用户允许沿用等价结构。适用范围为后续项目交接；具体读写规则及权威位置以 [AGENTS.md](AGENTS.md) 为唯一规范来源。来源：用户在本次会话提出的跨设备交接要求及后续文档规范要求。

## D02 宿主能力按实际观察

宿主名称只影响身份和指导，不能证明支持某项能力。时间位置、速度、拍号独立处理有效性；没有标准接口就不编造宿主版本。详见 `docs/V0_9_HOST_ARCHITECTURE.md`。

## D03 音乐与宿主边界

v0.9-dev.1 专注宿主兼容，保留 v0.8 的音乐算法及 Factory 内容；`core/` 独立于 SDK/VSTGUI，UI 不承担音乐逻辑。不增加私有 DAW 协议、现场音符捕获或音频识别。完整一步 Resolve/Loop 可以有效，不按和弦数量直接判失败。依据 AGENTS 与 `docs/CONTINUATION_ENGINE.md`。

## D04 库与产品独立版本

Factory 使用 ProgramData 的不可变版本目录及活动指针，失败可回退随包库；用户库留在 LocalAppData。安装/更新/卸载保留历史 Factory 和用户数据；同版本不同内容不能直接覆盖。依据 `docs/INSTALLER_AND_LIBRARY.md`。

## D05 试听与输入输出

试听使用 Windows 默认设备，PlaySound 为进程共享资源，通过所有权保护多实例。FL 输入使用标准 MIDI 文件；拖出是 FilePath，拒收可保存 MIDI 回退。真实 Wrapper/拖出目标需人工验证。依据 `docs/FL_STUDIO_COMPATIBILITY.md`。

## D06 验证强度与证据

修改只做相关测试，完整回归集中执行；未测不能写 PASS。仓库报告、当前执行、用户真机反馈分别注明来源。SDK 模拟不能证明真实 FL 行为；旧版本验收不能自动覆盖新包。需求确定前不主动扩大开发范围。

## D07 V3 以可获取的 GitHub 基线重建

用户于 2026-10-08 授权基于 GitHub 现有代码继续并整理旧目录。实际来源提交为 `1ecef08`；任务文本中的 capability 提交 `b7c9ef9…` 未获得，不能沿用其 PASS 或推送声明。旧目录整体可恢复归档，新目录独立开发。有效映射归 [MAP.md](MAP.md)。

## D08 完整和弦存储及冻结契约

旧匹配投影无法无损保存 bass/extension，因此采用 additive、事务式 User Schema 2，并保留原 payload 和一次性备份；不重建 user.db。Factory 和快照的冻结格式、重定向规则及降级边界以 [V3 兼容契约](docs/LIBRARY_V3_COMPATIBILITY.md) 为权威说明。新 metadata 只参与保存与 realization，不调分析、匹配成本、排名或声部权重。
