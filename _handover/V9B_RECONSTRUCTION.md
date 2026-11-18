# V9-B 后续实验：本地重建与见证诊断

日期：2026-11-15。对象：`main`(=v0.11.0) + v9-b 增量（`_handover/v9b-20261115-vs-main.patch`）+ 本地 C2026 修复。

## 1. 结论

1. 作者的 12 个"原生接缝见证"在本机无法直接复现：所需外部证据根 `HarmonyContinuation-v9-a-evidence/` 与 A2 研究 CSV 均不在本机、也不在仓库；`tools/v9b/diagnose_v9a.py` 对这些文件有 `assert sha256(...)` 硬校验。交接文档只记录了**公共语料**的来源与哈希，A2 没有出处。
2. 用公开语料（RS200 v2.1，CC BY 4.0）重建了同一条链路：冻结 111 首 → V8 回放 → 24 条 authored 素材编出 V9-A 库 → 回放 → `evaluate.py` → `diagnose_v9a.py`。**全部跑通**，得到 16 个见证 / 22 条路线，退出分布与作者报告同构。
3. 重建暴露两个作者报告未记录的机制问题：
   - `reviewed()` 白名单只含 version 7/8 的 editorial 标签，V9 素材（tag `V9A-original-editorial`）永远拿不到 reviewed 待遇；
   - `increment_seam_near` / `diagnose` 的"接缝"是在**素材的每个位置**扫描 motif，不锚定 `context_events` 设计边界。本次 22 条路线全部落在 authored 索引 9–10，而这些素材的设计边界 `context_events=4`。
4. Exact 通道的失败不是阈值，而是**每个 (template,key) 只物化一个落点**：全局 DP 的最优终点。B6 的 `--recover-consumed-tail` 只在"终点吃掉模板末事件"（`end==m`）时补一个更早落点，对这类中段落点无效——实测候选 +767 条、见证路线命中 0 条。

## 2. 可复现链（本机实测）

证据根：`D:/VibeCoding/_hc_evidence/`。工具：`build/Release/factory_v6_compiler.exe`、`_b5replay/Release/b5_factory_replay.exe`（由 `tools/b5/factory` 独立配置构建）。

```bash
# 1. 公共语料（RS200 v2.1，CC BY 4.0）
#    rs200.txt                      sha256 7882b29b420e70b7493c66d593239f62a391ac62de84326af25e8ffdff8a1f3c
#    rs200_harmony_exp.zip          sha256 7054ae6599280732bfba2ce2d531b93ca06274004380c2549756e468a8793fee
python tools/v9a/freeze_rock.py --sources <rs200> --b5 <stub> --a2 <stub> --output v9a-freeze
#    → 111 works（作者 97：其 A2/Billboard 排除集更严）；seed v9a-rock-v1

# 2. V8 回放（身份钉住）
b5_factory_replay build/factory.db v9a-freeze/confirmation.jsonl confirmation-v8 \
    --expect 8 1288 4853d2136e2508c091beaa417e06fd3883cd8f7c5c599a54780c3518136d7b87

# 3. 编码 + 编库（A2 CSV 不可得，donor provenance 记为 unreconstructed）
python tools/v9a/encode.py --a2 a2-stub.csv --catalog confirmation-v8/factory_catalog.jsonl --output materials
factory_v6_compiler --v9-a-incremental build/factory.db materials/candidates.json factory-v9a.db
#    → V9-A PASS schema=2 inherited=1288 added=24 total=1312，5,009,408 字节
#      （作者记录 5,009,408 字节，同尺寸；sha256 因 provenance 不同而不同）

# 4. 回放 + 评估 + 诊断
b5_factory_replay factory-v9a.db v9a-freeze/confirmation.jsonl confirmation-v9 --expect 9 1312 <sha>
python tools/v9a/evaluate.py --evidence <E> --output trace-queries.jsonl --prepare-trace
b5_factory_replay factory-v9a.db trace-queries.jsonl confirmation-v9-trace --expect 9 1312 <sha>
b5_factory_replay factory-v9a.db materials/controls.jsonl controls-v9 --expect 9 1312 <sha>
python tools/v9a/evaluate.py --evidence <E> --output evaluation.json --trace confirmation-v9-trace
python tools/v9b/diagnose_v9a.py --v9a <E> --output diagnosis.json
```

产物：`_hc_evidence/{v9a-freeze,confirmation-v8,materials,factory-v9a.db,evidence/{confirmation*,materials,controls-v9,evaluation.json,diagnosis.json}}`。

## 3. 数字对照

| 项 | 作者（97 首） | 本重建（111 首） |
| --- | --- | --- |
| 见证作品 | 12 | 16 |
| 路线 | 17 | 22 |
| 设计接缝在 Index 中近似（`increment_seam_near`） | 12 | 16 |
| Exact 首次退出 | alternate_endpoint 14 / minimum_match 3 | alternate_endpoint 19 / minimum_match 3 |
| Contextual 首次退出 | internal-center 10 / source-context 6 / untried 1 | internal-center 14 / source-context 7 / untried 1 |
| 末事件兼容 | 11 | 12 |
| 双事件前缀兼容 | 2 | 2 |
| 设计落点被实际尝试 | — | **3 / 22** |
| 设计落点保留 | 0（`native_qualified_routes`） | **0 / 22** |

评估层：`increment_catalog_near_works=34`，`increment_seam_near_works=16`；trace 级 Exact `TARGET_RETAINED=246`、`minimum_match=31443`、`minimum_score=101`；Contextual `qualified-before-structure=397`、`internal-center=40048`、`source-context=27445`；按作品 `exact_qualified=62`、`contextual_qualified=87`（111 首）。

## 4. 机制定位（代码为准）

**4.1 Exact：每个 (template,key) 只产出一个落点。**
`src/core/ProgressionMatcher.cpp:275-287` 的 `betterEndpoint` 以最小 `adjustedCost`、同分比 `pairs` 选唯一终点；`:380-385` `r.continuationStartIndex = min(t.full.size(), full.end + 1)`。其余落点不 materialize，故 `alternate_endpoint` 是枚举不足，不是打分不足。
`src/core/ProgressionMatcher.cpp:369-388`：`earlierTail` 恢复只在 `end == m && at(n,m).predecessor == 1` 时触发，即**模板尾端被吃掉**时才补；本批见证的最优终点在中段，不触发。
实测：`--recover-consumed-tail` 后候选 36,531→37,298、`exact_trace` 31,828→32,116，22 条见证路线的设计落点仍只被尝试 3 条、保留 0 条。

**4.2 Contextual：中心一致 + 双事件前缀两道门。**
`src/core/ContextualContinuation.cpp:514` `if(modulation&&!transition){trace("internal-center");continue;}`——origin 与 target 既非同中心也非关系大小调时，只有 `anchor.terminalPrototype`（transition 通道）才允许；素材中段换中心一律拒。
`:527-535` 要求在 {recentSize,4,3,2} 中找到一个 n≥2，使查询尾 n 事件与素材锚点前 n 事件在 target 移调下 `compatible()`；否则 `source-context`。实测 22 条路线仅 2 条满足双事件兼容。

**4.3 V9 素材拿不到 reviewed 待遇（新发现）。**
`src/core/ContextualContinuation.cpp:99-103` 的 `reviewed()` 只认 `version==7 + V7-editorial` 与 `version==8 + G8-editorial`；`tools/v9a/encode.py` 给 V9 素材打的是 `V9{edition}-original-editorial`。后果：`editorialEnding()` 恒 Unknown → `rankCreativeContinuations` 的 `+4` 发展加分不生效、`authoredDestination()` 不被查询。24 条新素材与 1,016 条 V5/V6 一样按"未审查素材"排序。

**4.4 "原生接缝"指标不锚定设计边界（新发现）。**
`tools/v9a/evaluate.py:27-43` 的 `catalog()` 对素材**每个**索引 i 生成 motif，`increment_seam_near` 取其中任一近邻；`diagnose_v9a.py:88-110` 的 `explain_witnesses()` 同样遍历所有 i。本批 22 条路线全部落在 authored 索引 9–10，而全部见证源素材的 `context_events=4`。即"原生接缝见证"实际测的是"素材任意后缀与真实作品后句在 1 个编辑内相似"，不是"作者设计的接缝被到达"。

## 5. 与"总是 V-I"的关系

不同问题，同一层：检索端拿不到长后句（本节），排序端偏好终止型结尾（`PHRASE`/`cadence` 子分与 `Closed` 惩罚）。本节结论不支持"V9 素材会缓解 V-I"——因为 24 条素材里没有任何一条被实际到达。

## 6. 可执行候选（按性价比，均未实施）

1. **改度量**（半天）：`increment_seam_near` 改用 `manifest.entries[].boundary` 与设计后句，只在设计边界上判定。先做这一步，否则后续所有"零增益"结论都建在松判据上。
2. **Exact 多落点**（1–2 天）：`materialize` 已按终点可重入，改为对每条 (template,key) 取 k≤3 个互异 `continuationStartIndex` 的终点（按 `adjustedCost` 排序），沿用现有打分与门槛。预期直接消掉本批 19/22 的 `alternate_endpoint`。
3. **reviewed 白名单加 v9**（半天）：`reviewed()` 接受 `version==9 && tag 含 "V9A-editorial"/"V9B-editorial"`；并在 encode 里把 TSV 的 `ending` 语义映射到 `G12-ending-*` 标签，否则 `PhraseEnding` 仍 Unknown。
4. **供给量级**（周）：+24 / +20 条相对 1,288 条不可能改变覆盖面；B7 已量化 7–8 事件句的缺口（65 条中 0 条有原生接缝近似）。

## 7. 局限

- A2 CSV 与 V9-A 冻结证据不可得：donor provenance 记为 `unreconstructed`，排除集比作者宽（111 vs 97），无法逐字节对齐作者冻结身份。
- 本重建使用本机 V8 库 `build/factory.db`（sha256 `4853d213…`），与作者构建的 V8 若是不同字节，见证集合会有差异。
- 未运行 B6 适配与统计排序；未做听评、未改产品代码。
