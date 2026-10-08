# Factory V3 兼容契约

本分支从 GitHub `v0.9/dev` 的 `1ecef08` 重建必要能力，未获得交接文本中的 `b7c9ef9…`，不沿用该提交的测试结论。当前交付状态与证据只在 [NOW](../NOW.md) 维护；本文件定义冻结格式和兼容边界。

## 冻结版本

| 对象 | 写入格式 | 读取范围 |
| --- | --- | --- |
| Factory Library 3 | SQLite Schema 2 | Factory Schema 1/2；Library 2 保留 161 条历史记录 |
| User Library | SQLite Schema 2 | Schema 1/2；旧库读操作不自动迁移 |
| Session | Schema 5 | 沿用现有旧版读取，不新增格式 |
| Continuation Snapshot | Schema 3 | 1/2/3；成功读取后运行时为 3 |
| Enrichment Snapshot | Schema 2 | 1/2；成功读取后运行时为 2 |

Factory Schema 2、Continuation 3、Enrichment 2 从此冻结。批量扩库期间表达不了或 quality 未确认的和弦进入 QUESTIONABLE；不得临时增字段或升级快照。匹配成本、分析、排名、声部权重、约束及倾向算法不因数据存储而调整。

## Factory ID 和所有引用入口

映射唯一权威源是 [LegacyFactoryIdResolver.h](../src/library/LegacyFactoryIdResolver.h)。六个 legacy ID 直接指向最终 canonical ID；编译期拒绝链、环和重复源，V3 数据编译及读取拒绝缺失目标和 legacy duplicate row。

`resolveFactoryId()` 先查 active entries 的原 ID，缺失才重定向。因此 Library 2 保持原 ID，Library 3 生产 DB 只含 155 条 canonical 记录。编译 V3 时只去除这六条重复音乐记录，旧名称进入目标条目的 aliases，不批量增库，也不修改原始 Factory JSON。

引用审计入口：

- 固定候选/快照：`candidate.primaryTemplate`、`supportingTemplates`、`match.templateId`，以及等于 primary ID 的 candidate ID。Continuation 读取接受 active library，恢复成功后 canonical 化；在 V3 保存时再次保证 canonical 写入。
- Session 固定候选及对比：候选 ID 和 `continuationFingerprint` 中带长度前缀的 primary ID。迁移时只替换该 ID 字段；合流到同一 canonical ID 的 pin 去重，避免 Session 5 因重复 pin 加载失败。路径改变仍不能仅按 ID 替换原方案；真正缺失的引用保留轻量 unavailable 状态。
- Demo 快照读取、MIDI CLI 快照读取传入当前 Factory 与可用 User entries；个人 ID 不转成 Factory ID。缺失引用不丢弃原进行、调性或其他设置。
- Library 选择保存的是当前页面/过滤与临时条目索引，没有另一份持久 Factory ID；Compare 使用 Session pins。Enrichment 快照保存完整进行和操作，不包含 Factory entry ID，因此无需伪造其迁移引用。
- 新推荐从已验证的 V3 entries 生成，天然使用 canonical ID。插件及 Demo 保存快照时带上实际 Factory 版本。

同一旧快照分别在 2 → 3 → 2 打开：在 2 直接恢复旧 ID，在 3 重定向到 canonical，回到 2 再读同一旧快照恢复原 ID。已经重存的 V3 快照记录 canonical；不会凭空反推多个 legacy ID。V3-only ID 在 Library 2 中 unavailable，但原进行仍可加载。

## 结构化和弦存储

[ChordData](../src/persistence/ChordData.cpp) 保存完整 ChordEvent：root、bass、quality、producer note numbers、raw 字段、extensions 的 mask/pitches/type/color、起点、时值、OPEN 和来源。用户条目同时保留原调性及原始进行，匹配投影只负责原有算法；匹配事件另携带 bassInterval、intervalMask、colorMask、displaySuffix，realization/试听/MIDI 不再丢掉这些信息。

Factory Schema 2 在已有 payload 中增加 `typedData` JSON 字符串，保存完整匹配投影与可选原始和弦。Continuation 3 保留旧字段，增加 `chordData`、每个续写事件的 `harmonicData` 和 Factory 版本。Enrichment 2 的原进行/候选进行使用完整 ChordEvent 编码。

用户库新增 `progression_data(id, data)`，通过 ID 与既有 `progressions(id, payload)` 一一对应。旧 payload 不因迁移重写；typed data 保存完整和弦、匹配事件及精确的标签/别名向量。保存推荐时在归一化之前保留原始进行；重新打开后的试听和 MIDI 使用该数据。转调改变 tonic 的半音距离，不用新 mode 重写用户和弦。

## User Schema 1 → 2 与恢复

审计结论：旧版只保存归一化的级数/quality/节奏串，无法无损保存 bass/inversion/extensions；不能留在 Schema 1 后让旧版静默丢字段。Schema 2 为 additive，不重建 user.db。

迁移仅在写入时进行：

1. 校验旧库类型/版本/行内容，取得数据库写锁并复查版本。
2. 用 SQLite backup API 创建一次 `user.db.v1-backup`；备份从独立只读连接取一致视图，既有有效备份不覆盖。
3. 同一事务创建新表，逐行添加 typed data，校验行数、语义和元数据；最后更新 schema metadata 并 COMMIT。
4. 任一步失败 ROLLBACK。旧 payload 仍存在；普通保存/修改对两个表也使用一个事务，注入新表写失败时无部分写入。
5. 无变化的 load → save → load 不刷新 timestamps；真实编辑仍更新 updatedAt。全新 Schema 2 库无需迁移备份，成功迁移后不重复备份。

**User Library Schema 2 requires v0.9+，具体须为本兼容桥的 `0.9.0-dev.2` 或之后支持此格式的构建。** 已封版 v0.8.0 和此前 v0.9.0-dev.1 都拒绝该 schema，不能安全读写新库；没有修改 v0.8。Factory Library 2 能继续由新版使用，不代表 User DB 能降级。

需要回到旧插件时，先关闭宿主并另存当前 user.db，再依据一次性 v1 备份恢复旧数据。备份不含之后的新条目，不允许为了降级直接覆盖它们。恢复方法和实际路径先按 [RUNBOOK](../RUNBOOK.md) 与 [RISKS](../RISKS.md) 定位；这些说明不是执行授权。本轮没有访问或迁移生产用户 DB。

## 针对性验收入口

[LibraryCompatibilityTests](../tests/LibraryCompatibilityTests.cpp) 四组：resolver、snapshot、user、factory。覆盖全部六映射、链/环/冲突/缺失目标、旧/新快照与 Library 2/3 切换、缺失候选、pins、个人引用、完整结构化和弦、旧库元数据及迁移/写失败回滚。

18 类用户和弦包括 Major triad、第一/第二转位、七和弦第三转位、非和弦低音、maj7/m7/7/6/m6/9/maj9/m9/dim7/m7b5/sus、借用和弦转位、次属和弦转位；保存后关闭重开，比较完整内容、试听音高结构及 MIDI 字节。历史音乐测试继续显式使用 Library 2 fixture，不把删除重复记录后的 V3 内容误作旧语料。

本轮不跑全 CTest、42 例续写、30 例升级、FL Host Harness，不安装到宿主扫描目录，不宣称 Cubase/FL 真机验收或发布。纯字段传递涉及 Core 数据结构和 realization；分析、匹配成本、排序、声部权重及 Host/FL 实现保持基线。
