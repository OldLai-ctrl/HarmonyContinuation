# Factory V5 / G2 v2 只读审计

本审计以 Factory V5 构建数据库及 G2 v2 来源绑定目录为对象，按实际音高与演奏时值复算结构覆盖。全库人工听评未执行；G2 目录的审校语义为结构编辑审校，用户试听仍待完成。

## 对象与边界

- 源基线：2421c6f。数据库：D:\VibeCoding\HarmonyContinuation-g04r-delivery-20261024\stage-5\official-factory\factory-v5.db；SHA-256：F989A83EA9EF92BA7C272BF0E5E3C7073C225899DC81D60DD46C10EA6B48CB3C；SQLite 只读模式，行数 658。
- G2 v2：D:\VibeCoding\HarmonyContinuation-release\data\mode-annotations\v2\factory-v5.hca；SHA-256：FE2C368508525B0CFEE10EA8697B8B3C6E269858AB705C350632A0E0DC0D089F；精确 ID 与源指纹匹配 658/658。
- B1 草案：60 条候选源 SHA-256 98CCD2CD141669A97A656B9C04EBFA34F32A705BEF2366AA7F3D97501827916D；元数据 SHA-256 99B2CBDDF7F66A7D14F8CC6914F33DAA5A2260239FB2F3473C9B8CB10CD058FA。比较不修改草案文件。
- 此流程只打开明确列出的 Factory 数据库，不打开 user.db。

## 覆盖矩阵

| 检查项 | 当前结果 | 解释 |
| --- | ---: | --- |
| Factory V5 行 / 唯一 ID | 658 / 658 | 数据库元数据：{"schema_version": "2", "library_version": "5", "library_type": "factory"} |
| G2 v2 ID / 归属行 | 658 / 727 | 缺 ID 0；多余 ID 0 |
| 来源指纹 | 658/658 exact match | Python 复刻项目 C++ 指纹；数据库与 HCA SHA 锁定，脚本可重算 |
| G2 状态 | 618 reviewed-only；15 mixed；25 unconfirmed-only | 任一 unconfirmed 不计作全已审校 |
| 样式 / 主 intent | 658/658；658/658 | 另有逐 ID 字段和值、格式检查 |
| techniques | 421/658 populated；237 空 | 空标签是实际覆盖缺口；不可从和弦外观自动补写 |
| 节奏文本 | 658/658 与 full durationQN 一致 | 31 种节奏文本；空时值数按全库实际字段核验 |
| 完整严格实际音乐池 | 固定 C 独立内容 649；全 12 调具体演奏 7788（÷12=649） | 主数字按完整实响序列跨移调去重；调式限定数量见下表 |

## 旧 mode 与 G2 v2 分开看

| Factory 旧 mode | 行数 | 仅 reviewed | 混合 | 仅 unconfirmed | 对应旧中心标签在 offset 0：reviewed / mixed / unconfirmed / missing |
| --- | ---: | ---: | ---: | ---: | --- |
| major | 474 | 445 | 15 | 14 | 437 / 0 / 28 / 9 |
| minor | 184 | 173 | 0 | 11 | 107 / 0 / 11 / 66 |

旧 major/minor 是原库参考框架。脚本没有把 major 映射成已审校 Ionian，也没有把 minor 映射成已审校 Aeolian；“对应旧中心”只用于指出显式 G2 记录是否存在。

## G2 中心、兼容与借用分开统计

| 调式 | reviewed membership | unconfirmed membership | 固定 C 严格唯一音乐数 | 全 12 调具体演奏数 |
| --- | ---: | ---: | ---: | ---: |
| ionian | 438 | 28 | 238 | 2856 |
| dorian | 3 | 0 | 3 | 36 |
| phrygian | 1 | 0 | 1 | 12 |
| lydian | 3 | 0 | 3 | 36 |
| mixolydian | 8 | 1 | 8 | 96 |
| aeolian | 119 | 11 | 64 | 768 |
| locrian | 0 | 0 | 0 | 0 |
| harmonic_minor | 115 | 0 | 29 | 348 |
| melodic_minor_ascending | 0 | 0 | 0 | 0 |
| classical_melodic_minor | 0 | 0 | 0 | 0 |

全库 membership 级实际音集结果：352 reviewed 且全音集符合调式；211 reviewed、实际超出静态音阶且有 Borrowed/变音/借用文字标记；124 reviewed、实际超出静态音阶但无 Borrowed role、变音 degree 或 evidence 文字线索；40 unconfirmed 独列。此标签核对的是实响音集，不反推调式归属。
独立机制线索（与 stored techniques 分开，且只作描述）：Borrowed role 107 条 reviewed membership；变音 degree 106；显式 Borrowed/变音文字 201；严格兼容 352；上述线索皆缺但音集超出 124；unconfirmed 40。

每条 membership 的中心 offset、mode、usage、usageBasis、status、审校人、原始音乐证据、实响根音与实际音集差异均在逐 ID JSON/TSV。与 stored techniques 分开的 derivedMechanismEvidence 给出相对音阶级数、属五级属七事件、低音相对根音、和弦音程集合；这些只描述实际音符，不推断 membership。

## 风格、意图、技法、节奏字段

- Style：658/658 非空；格式/权重异常 0 条。权重仅按 0..1 解析有效性核验。
- Intent：{"loop": 127, "resolve": 350, "develop": 92, "color": 89}；Cadence：{"loop_closure": 127, "authentic": 321, "half": 40, "deceptive": 9, "none": 62, "modal": 19, "plagal": 80}。
- Technique：421 条非空、237 条缺失；payload 与 typedData 不一致 0 条。
- 空 technique 的具体影响：{'reviewed_only': 206, 'mixed': 14, 'unconfirmed_only': 17}；其中 reviewed 且实响 strict 190 条、reviewed 但实响 nonstrict 30 条、含实际 Borrowed role 2 条、degree 变音 2 条、完整实响重复组成员 18 条。空标签不表示没有音乐技法，明细按源 ID 见 summary.json 和 per-ID 台账。
- Rhythm：31 种 profile；与逐 chord 时值不一致 0 条。全库以 actual durationQN 为准，明细保留声明文本。

Style 为实际标签赋值数（每条可多标签）：

| Style | 赋值数 |
| --- | ---: |
| pop | 420 |
| functional | 286 |
| jazz | 256 |
| rock | 213 |
| citypop | 180 |
| rnb | 157 |

Technique 为存储标签出现次数，不代表音乐机制穷举；下表列前 12 类，完整频次见 summary.json：

| Technique | 次数 |
| --- | ---: |
| SeventhColor | 246 |
| BorrowedChord | 102 |
| SecondaryDominant | 85 |
| CadentialExpansion | 54 |
| DeceptiveResolution | 46 |
| ModalMixture | 45 |
| OpenEnding | 40 |
| PassingDiminished | 33 |
| SecondaryLeadingTone | 26 |
| TritoneSubstitution | 5 |
| SlashBass | 5 |
| Backdoor | 4 |

拍号分布：

| 拍号 | 条数 |
| --- | ---: |
| 4/4 | 658 |

节奏 profile 分布（文本值与 actual durationQN 已逐行核对）：

| Rhythm | 条数 |
| --- | ---: |
| `4 4 4 4 4` | 199 |
| `4 4 4 4 4 4` | 184 |
| `4 4 4 4` | 153 |
| `4 4 4 4 4 4 4` | 60 |
| `4 4 4 4 4 4 4 4` | 8 |
| `4 2 2 4 4` | 7 |
| `4 4 4` | 7 |
| `4 2 2 4` | 6 |
| `4 1 3 4 4` | 3 |
| `4 2 2 2 2 4` | 3 |
| `4 2 2 2 4` | 3 |
| `4 2 2 4 4 4` | 3 |
| `4 0.5 3.5 4 4` | 2 |
| `4 4 2 2 2 2 4` | 2 |
| `4 4 4 4 2 2 4` | 2 |
| `2 2 2 2 2 2 4 8` | 1 |
| `2 2 2 2 8` | 1 |
| `3 1 3 1 4 4 4` | 1 |
| `3 1 4 4 4 4 8` | 1 |
| `4 1 3 4 4 4` | 1 |
| `4 2 1 1 4` | 1 |
| `4 2 1 2 2 4` | 1 |
| `4 2 2 2 2 2 4` | 1 |
| `4 2 2 4 4 4 4` | 1 |
| `4 3 1 4 4` | 1 |
| `4 3 1 4 4 4` | 1 |
| `4 3 1 4 4 4 4` | 1 |
| `4 3 1 4 4 4 4 4` | 1 |
| `4 4 2 2` | 1 |
| `4 4 2 2 4 4` | 1 |
| `4 4 2 4` | 1 |

## 重复与近似层级

- 完整实响重复：9 组，涉及 18 条，冗余 9 条；包含实际和弦音集、低音、拍号、起点与时值，跨移调归一。此类可作为高可信重复候选。
- 去节奏重复：23 组，涉及 49 条，冗余 26 条；忽略拍号和时值，保留完整和弦音集/低音/顺序，属于相同和声内容、编排可能不同。
- 核心连接家族：42 组，涉及 89 条；复用 WishSampler 的低置信度多样性家族规则，只含相对根音、低音与简化和弦质量，不能视为重复或删除依据。

V5 完整实响重复组：

- COMMON_MINOR_007, POP_003
- COMMON_MINOR_008, POP_009
- V3_CYCLE_006, V3_NIGHT_029
- V3_CYCLE_013, V3_NIGHT_026
- V3_CYCLE_014, V3_CYCLE_029
- V3_CYCLE_021, V3_NIGHT_020
- V3_CYCLE_023, V3_NIGHT_002
- V3_CYCLE_025, V3_NIGHT_006
- V3_CYCLE_031, V3_NIGHT_016

## V6 B1 草案与 V5 对照

- 候选 60 条；候选文件 mode：{"minor": 23, "major": 37}；元数据 status：{"editorial-reviewed": 56, "pending": 4}；auditionStatus：{"pending-human": 60}。
- 与 V5 完整实响完全重复 0 条；去节奏相同 0 条；低置信度核心连接家族相同 2 条；最近相差一个和弦事件 0 条。
- 候选内部完整重复组：[]。
- 低置信度核心家族命中：V6_B1_010 ↔ V3_SHADE_032；V6_B1_028 ↔ JAZZ_018。均不是完整重复。V6_B1_028 的已审校 Ionian membership 实响超出 pitch classes [3, 8, 10]，其 Backdoor/ExtendedHarmony 标签可能解释材料，但应由主代理针对 membership evidence 复核。

命中完整重复的 V6 条目及 V5 来源：

- 无

候选 mode membership 与严格音阶兼容独立列出；严格通过只表示本次完整实响可被该静态音阶解释，不创建归属。借用/半音材料允许存在，但需以具体 membership 的 evidence/status 审查。

## 待人工复核线索与批次建议

- Unconfirmed-only 25 条：CITYPOP_JPOP_014, JAZZ_015, ROCK_017, V3_CYCLE_027, V3_CYCLE_029, V3_CYCLE_031, V3_CYCLE_033, V3_CYCLE_035, V3_NIGHT_021, V3_NIGHT_022, V3_NIGHT_024, V3_NIGHT_025, V3_NIGHT_026, V3_NIGHT_027, V3_NIGHT_030, V3_NIGHT_031, V3_NIGHT_032, V3_NIGHT_033, V3_NIGHT_034, V3_OPEN_022, V3_OPEN_027, V3_OPEN_029, V3_OPEN_030, V3_OPEN_042, V3_OPEN_045
- Mixed 15 条：CITYPOP_JPOP_001, POP_009, V3_CYCLE_021, V3_CYCLE_022, V3_CYCLE_023, V3_CYCLE_024, V3_CYCLE_025, V3_CYCLE_026, V3_CYCLE_028, V3_CYCLE_030, V3_CYCLE_032, V3_CYCLE_034, V3_CYCLE_046, V3_CYCLE_047, V3_CYCLE_048
- Reviewed membership 中首末都未落在其标注主音根音的源条目：12 条：CITYPOP_JPOP_011, JAZZ_013, JAZZ_017, V3_OPEN_021, V3_OPEN_023, V3_OPEN_024, V3_OPEN_025, V3_OPEN_026, V3_OPEN_028, V3_OPEN_041, V3_OPEN_043, V3_OPEN_044。该边界 heuristic 不足以否定 Ionian：主代理按和弦序列复核这 12 条后确认序列内部含 I，边界由功能进行/回接解释；其中 CITYPOP_JPOP_011、JAZZ_013、JAZZ_017 的功能序列各有多种解释。现有 HCA 保持不变；主音中心并非唯一解释，未据此追加 Reviewed。
- 另有实际序列在 reviewed membership 下全程没有标注主音根音的源条目：0 条，名单见 summary.json；这是比“首末无 I”更直接的中心证据缺口。
- Reviewed 实际超出静态音阶且无 Borrowed role、变音 degree 或 evidence 文字标记：124 个 membership / 81 个源条目。ID 家族计数为 {"V3_BLUE": 50, "V3_DUSK": 26, "COMMON": 5}；其中 28 条 techniques 为空。V3_BLUE 只是源 ID 家族线索，可能是有意 blue-note / 蓝调材料或技法元数据缺口；不得作为归属错误自动处理。逐条外音 pitch class 和 evidence 在 per-ID 文件。
- 全库 reviewed 仍只表示结构编辑审校；未完成人工听评。40 条含 unconfirmed 记录须继续保持待审，禁止从旧 Major/Minor 或全音阶兼容自动升为 Reviewed。

建议只针对实际缺口复核：先审 25 条 unconfirmed-only 和 15 条 mixed 中具体 unconfirmed membership；再审 81 条非严格且无显式解释线索的记录（其中 50 条属 V3_BLUE ID 家族，需判断有意蓝调音还是技法元数据缺失）。237 条 techniques 空项应先关联实际角色/变音与功能影响再决定是否补标签，不做全库重读或自动补写。12 条边界主音 heuristic 已由主代理按功能/回接复核，现有 HCA 保持，不作负面判定。

## 方法与复算

audit_factory_v5.py 从上述只读 SQLite 数据库载入 658 个 payload；按 src/wish/MusicIdentity.cpp、src/wish/ModeAnnotations.cpp、src/wish/WishSampler.cpp、src/library/LegacyFactoryIdResolver.cpp 的身份、归一与过滤规则实现可复算投影。HCA 解析遵循 10 字段格式并按 exact Factory ID + source-v1 指纹匹配。严格池统计在 source match + reviewed membership 限定后，逐调式检查 root、bass 与每个 chord pitch；V5 无 topVoice 数据。固定 C 独立音乐数以完整实响音高/低音/拍号/节奏签名跨移调去重，全 12 调的具体演奏数另按 WishSampler performanceFingerprint 去重；音阶兼容从不产生 membership。

重复定义及置信度见 summary.json duplicateStrata；核心连接家族完全复用当前 WishSampler 逻辑。v6_candidate_comparison.json/tsv 给出草案每条与 V5 的精确、去节奏、核心家族和一和弦编辑近邻。

限制：固定 C 去重的完整实响签名忽略 C++ performanceFingerprint 的质量编码/精确标志差异，只比较实际根音、低音、pitch-class 集、拍号和时值；因此这是音乐内容重复口径，运行时身份数另保留于 summary.json。代码不推断键盘配器、声部进行、听感、风格正确性或全曲编曲同一性。结构中心标记和 evidence 供主代理针对具体条目复核；未进行 658 首听评。

归档说明：仓库保留紧凑逐ID TSV、候选比较TSV与summary JSON；完整展开的per_id_audit.json和v6_candidate_comparison.json保存在工作区同级HarmonyContinuation-v6-evidence/audit/，由同一脚本生成。原始JSON与TSV来自相同冻结输入，不作为另一套可编辑归属源。
