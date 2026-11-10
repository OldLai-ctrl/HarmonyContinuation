# HarmonyContinuation v0.11.0

Windows x64 VST3正式版本，Factory V8 / 1,288条。[下载与校验](https://github.com/OldLai-ctrl/HarmonyContinuation/releases/tag/v0.11.0)。源码由v0.11.0 Tag固定，正式BuildCommit、Setup及SHA256见Release中的BUILD_INFO.json和SHA256SUMS.txt。

## 本版内容

- 四个一级入口：匹配优先、灵感优先、升级进行、和声祈愿。保留现有曲库推荐与匹配评分。
- 灵感优先增加程序性乐句生成，按完整结构、功能连接和受控变奏生成后句。常规、丰富、大胆三个层次与数据库候选共同显示；全部推荐每页10条，原数据库Top3后加入实际合格的生成结果。
- 生成来源明确标记为算法生成，Why与详情显示实际音乐依据。完整D→E试听、MIDI导出、拖放和Snapshot冻结保留音集、低音音区及时值。
- Snapshot4读取旧Snapshot1–3；Session5及旧Factory继续可读。Factory V8含1,288条，旧库和个人数据保留。
- 产品只纳入Billboard CC0聚合先验；IdolSongsJp/RWC非商业研究统计独立保存，不随正式产品分发。

## 安装与验收

关闭宿主，运行唯一统一安装包 `HarmonyContinuation-0.11.0-Setup.exe`，默认安装VST3与Factory V8；可由v0.10.0升级。安装包未数字签名。安装/卸载保护范围及正式构建、一次隔离升级的具体结果以BUILD_INFO.json为准。

用户已认可dev.16产品音乐，并确认Cubase、FL Studio最小加载、生成试听与MIDI操作通过，具体版本未提供。正式版保留已验收实现，仅调整正式版本元数据及发布记录；不声称正式二进制已重新在两款DAW实测。

## 使用范围

生成是确定性、有界的本地程序，不调用生成式AI服务。当前规划需要可用的大调解释；某一风格档没有合格路线时，继续保留数据库候选及其他合格档。大小调重解释或较大胆接缝仍由创作者判断。旧完整导航harness未宣称通过；已完成的集成、旧快照和聚焦原生UI证据见[集成记录](G13C_PRODUCT_INTEGRATION.md)。
