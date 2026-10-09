# 当前风险与读取触发

先搜索风险 ID、操作或模块，读取对应条目。解决后移除活动风险，必要的复现/原因归档 history；不要堆积已结束事故。

## R01 接错设备、分支或包

触发：接手、切换 checkout、同步、回滚、安装、交付。相同目录名曾对应不同分支；核对 [MAP.md](MAP.md) 与实时 Git。源码与分发包提交的差异见 [NOW.md 的证据](NOW.md#证据)，实际包匹配情况待核实，不把差异自动当构建错误。未提交内容/ignored 证据不会随 push 到另一设备。

## R02 真实宿主仍待验收

触发：宣称 FL 兼容、推进 RC/正式版、修改窗口/拖放/恢复行为。当前待验收状态由 [NOW.md](NOW.md#当前任务) 维护，RC 的 Cubase Pro 15 / FL Studio 六项人工检查见 [RC 清单](docs/RC_DAW_ACCEPTANCE.md)，两者当前 Pending；旧兼容说明见 `docs/FL_STUDIO_COMPATIBILITY.md`。SDK 测试不能证明真实 FL 行为；无可访问真机记录的结论标为待核实。

## R03 安装与持久数据

触发：覆盖标准 VST3、迁移/删除目录、Factory 更新、用户 DB 操作。关闭宿主，验证目标路径与备份；保留个人进行和所有已安装 Factory。不得为了测试对生产数据做删除或并发覆盖。脚本部分默认指向 build-v08 / build-vst3 / 随包 DB，独立活动库和新包必须显式定位。

User Schema 2 无法由 v0.8.0 或旧 v0.9.0-dev.1 安全读写；它们会拒绝打开。迁移前一次性备份及降级数据保护见 [V3 契约](docs/LIBRARY_V3_COMPATIBILITY.md#user-schema-1--2-与恢复)。本轮只测试隔离 fixture，生产 user.db 未迁移。旧源码目录整体归档，已安装插件和用户数据没有移动。

## R04 线程与窗口生命周期

触发：process()、异步导入、worker、窗口 detach/reopen、试听所有权修改。音频线程不得文件/DB/XML/日志/锁/昂贵分配/GUI；过期任务不能覆盖恢复状态。DXGI 卸载回调及多实例 PlaySound 是已有边界，先搜索源码/相关测试和 `docs/V0_9_HOST_ARCHITECTURE.md`，避免重复引入已修问题。

## R05 旧文档与基准提示

触发：schema 迁移、版本判断、算法质量结论。部分 Phase 文档仍描述旧 schema；以相关源码常量和有效版本文档核对。既有续写 3 处结构提示不等于本轮回归；不靠大范围调权重消除提示，不把未试听的差异判为音乐改善。
