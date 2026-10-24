# B3 最终内容核验

最终审阅使用冻结的合并 `candidates.json`、`metadata.json` 与 `approved-ids.txt`。仓库与外部 evidence 目录的这三份文件 SHA-256 一致。报告里的 132 条是批准子集；raw `new-metadata.json` 是 B3 110 条的前置元数据快照，保留旧的 90 approved / 20 pending 状态，不能代替 final combined metadata。

## 独立性与重复

- `approved-ids.txt` 含 132 条：B1 45 条 + B3 87 条。按完整实响签名，132 条全都唯一；与 V5 的完整、去节奏签名命中均为 0，因此完整音乐数为 132。
- 这 132 条有 132 个去节奏签名和 132 个低置信度核心家族签名。核心家族中有 2 条与 V5 同族（B1-010、B1-028），故相对 V5 新增 130 个低置信度核心家族桶。
- 全部 170 条合并候选有 170 个完整签名、170 个去节奏签名；低置信度核心家族桶为 169 个。唯一内部同族关系是 B1-039 ↔ B3-073。
- 对完整 B3 110 条批次，完整/去节奏重复命中 V5+B1 均为 0；新增核心家族桶 108 个。近根亲族线索有 B3 内部 3 对、对 V5+B1 17 对；长线拼接候选为 B3-046、B3-100。它们是相似关系线索，不是完整重复认定。
- 两个仅扩展/音集关系：B3-073 ↔ B1-039，B3-079 ↔ V5:V3_BLUE_007。二者为 C 级且未纳入 approved 子集。B3-070 为 B 级、针对 G7sus4/B 低音路线待听评；其余 B3 107 条为 A。

## 最终等级、状态与标签

- 最终分级：B1 为 A45/B2/C13；B3 为 A107/B1/C2；合计 A152/B3/C15。最终 approved 132 条均为 A；另有 20 条 A 级预算保留、3 条 B 级和 15 条 C 级待定。
- final combined metadata 状态为 approved 132 / pending 38；其中 B3 为 approved 87 / pending 23。raw `new-metadata.json` 仍记录 90 / 20，这是前置快照，不是最终门禁结果。
- 最终 metadata 中 224 条 styleEvidence 全为 `editorial-application`，只表示建议使用场景，不证明来源作品流派。`Keys` 是分类标签，不能据此声称已证实 NeoSoul 或 CityPop。

| Family 标签 | 合并 170 | approved 132 |
|---|---:|---:|
| Aeolian | 4 | 0 |
| Dorian | 18 | 17 |
| ExtendedKeyboard | 12 | 10 |
| FunctionalChromatic | 10 | 8 |
| Jazz | 13 | 10 |
| Keys | 19 | 17 |
| Locrian | 2 | 0 |
| Lydian | 16 | 15 |
| Minor | 8 | 0 |
| MinorSpecial | 4 | 3 |
| Mixolydian | 16 | 15 |
| ModernPop | 20 | 17 |
| PedalColor | 8 | 1 |
| Phrygian | 12 | 11 |
| Screen | 8 | 8 |

## 音阶外音与 membership

- B3 110 条中有 44 条 membership 不满足整条静态音阶严格兼容。严格筛选应把它们排除在“严格兼容”候选池外；这不等于否定有上下文依据的 membership 归属。
- 最终元数据已补明：B3-048 的 Bb/D 中 Bb 是平行 C 自然小调的 bVII 借用；B3-051 的 Eb/G 中 Bb 来自 C 自然小调借用；B3-056 与 B3-058 的 Em9 内 F# 是Em和弦自身的局部九音色彩（C中心下Em为iii），再经A7去Dm，不属于 C Ionian。
- 已完成的 B3 全量符号/时值审计记录：结构对齐、时值/拍号、和弦符号与编码音集均为 0 项错误，44 条严格音阶失败不改变该结果。这里沿用已完成核验；最终版本只刷新合并身份、grade、状态和 family 标签统计。

## 可复算文件与边界

- [final-summary.json](final-summary.json) 保存最终 132 子集、V5/B1 关系、等级/状态/family 计数和输入哈希。
- [approved-compact.tsv](approved-compact.tsv) 每个 approved ID 一行，包含 final family、mode membership 依据、和弦串及完整/去节奏/核心家族关系标记。B3 解码根音、低音、逐和弦音集和时值见此前的 `per_candidate.tsv`。旧 `summary.json` 与 `per_candidate.*` 保留完成音符/重复核验时的原始字段；其中等级、状态、family 标签及最终补充的音符说明以本报告和 final combined files 为准。
- 符号解码与签名使用仓库 `data/factory-v6/content-check/audit.py` 和 `data/factory-v6/audit/audit_factory_v5.py` 的既有逻辑；V5 数据库 658 行，SHA-256 在 `final-summary.json` 中。raw B3 手工 TSV 与 B3 原始文件哈希也记录在那里。
- 本次没有人工试听。style/source evidence 仍是概念依据与编辑性使用建议；不得由 family/style 标签推出实际来源流派。

原始逐事件音集与时值明细保存在工作区同级 `HarmonyContinuation-v6-b3-evidence/content-check/per_candidate.tsv`；最终状态以本报告、final-summary与metadata为准。
