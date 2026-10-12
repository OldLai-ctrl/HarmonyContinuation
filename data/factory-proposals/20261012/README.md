# 小调半音下行候选

状态：隔离技术候选，等待人工试听与内容决策。`candidate.json` 不属于生产编译输入。

路线：Am → G#dim → G → D/F# → F → G → Am → C。A 小调，4/4，每个和弦 4QN；Develop，非循环，无终止承诺。低音前五步为 A–G#–G–F#–F。D 大三和弦引入 Dorian 六级色彩，F 再回到自然小调色彩。

ID：`CANDIDATE_V5_CHROMATIC_MINOR_001`。建议下一内容版本 V5；包含此候选的隔离库为 658 条。正式开发基线仍是 V4/657，SHA-256 `74C529828F8C4B934BBAF4ADA719AA95491AB498B58885522124FF6505671534`。

使用现有 Schema 2 的 `typedData.full`：D/F# 的 `bassInterval=4`；明确保存三和弦音程。`events` 和 `key` 留空，表示设计数据，没有伪造录制 MIDI。G#dim 保留为经过节点；Dorian IV 在现有大/小调功能模型中使用 Unknown 功能，音乐解释保存在描述与标签中。

对当前 V4 的 657 条进行了全部连续窗口和十二种移调比较，身份包含根音、和弦性质与相对低音：完整八和弦、输入四和弦、F–G–Am–C 四和弦均无命中。固定 C Major/A Minor 的 Color 软家族窗口扫描只命中 COMMON_MINOR_015 的末尾 F–G–Am，后面没有续写。详细 JSON、独立 DB、编译结果与 MIDI 随候选验收包保存。

可复用当前 `db_compiler`：从已核验 V4 DB 导出完整 payload 作为隔离 baseline JSON，与此文件放入独立目录，执行 `db_compiler <隔离目录> <新DB> 5`。编译器检查 ID、canonicalization、重复、元数据和 DB 读回。不得把输出写到 V4 原路径。

技术门禁复用 `Phase4DTests <证据目录> <隔离DB>`：原调及 C/D 小调移调，六拍号、2QN/按小节 QN、OPEN/closed、D/F# 抽象为 D、两个检索模式、八度与上方声部转位；明确低音 D/A 和核心三度冲突为负例。候选以普通评分进入排序，没有 ID 优先级。人工音乐质量保持 Pending。
