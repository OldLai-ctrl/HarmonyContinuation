# Phase 1 回传网页端（2026-09-24）

1. **主要文件**：新增 `src/core/HarmonyAnalysis.h/.cpp`、`src/core/ChordNormalizer.cpp`，开发用 `tools/ProgressionJson.*` 与 `tools/harmony_cli.cpp`，`tests/HarmonyAnalysisTests.cpp`、`tests/fixtures/harmony/` 及 `docs/HARMONIC_MODEL.md`、`docs/HARMONY_PHASE1_TESTS.md`；修改 `CMakeLists.txt`、插件 Controller、MainView、README、AGENTS 和宿主记录。
2. **ChordNormalizer：IMPLEMENTED**。优先使用结构化根音、mask、pitches；名称辅助核对。冲突保留诊断并降低置信度。基本三和弦、七和弦、sus、增减及常见扩展颜色已覆盖。复杂拼写与所有 Cubase 实例仍需补样本。
3. **KeyAnalyzer**：12 Major + 12 Minor 候选。Minor 兼容自然小调及常见和声小调属功能，不另输出 Harmonic Minor 模式。
4. **DegreeAnalyzer：IMPLEMENTED**。`ScaleDegree` 独立保存 1–7 级和半音变音，格式化时生成罗马数字。
5. **Secondary Dominant**：可按解决目标识别 `V/X`、`V7/X`，已测 `V7/ii`、`V7/V`、`V7/vi`；接口也支持其他调内非主级目标。
6. **Secondary Leading Tone：IMPLEMENTED**。可识别紧接目标且根音低半音的 dim、dim7、half-dim7；已测 `vii°7/ii`。未做转位或延迟解决。
7. **Borrowed Chords**：大调的 `iv`、`bIII`、`bVI`、`bVII`，标记 `Borrowed`；暂不做其他调式互借。
8. **HarmonicFunction**：`Unknown`、`Tonic`、`TonicProlongation`、`Predominant`、`Dominant`、`ChromaticPredominant`、`ChromaticDominant`。
9. **StructuralWeight**：使用相对时值、功能、终止链、起止位置、已知拍号的小节起点、短时解决及趋近证据。所有权重限制在 0–1；OPEN 不按零时值处理。重复和弦与完整声部进行未参与。
10. **SkeletonBuilder**：保留首尾、终止链和超过阈值的事件；短暂趋近和弦可以省略，短但有终止功能的 V7 保留。整小节次属可保留，短次属不自动视为结构和弦。
11. **Key Override：IMPLEMENTED**。`AnalysisContext.forcedKey` 与 CLI `--key C:major` 可指定分析调性；插件 UI 暂无调性选择器。
12. **Harmony CLI：READY**。输入 JSON 和弦列表，输出前三调性候选、FULL、每颗结构权重与 SKELETON；只用于开发，不进入插件运行时。
13. **UI Analysis View：PARTIAL**。已显示前三候选、罗马数字/功能/角色、权重细条、FULL/SKELETON 开关；编译通过，但本轮未在 Cubase 中人工检查布局、拖放和重开后的行为。
14. **Build：PASS**。VS 2022 + 本地 SDK 构建插件和 CLI 成功；`HC_BUILD_PLUGIN=OFF` 且 SDK 路径不存在时，核心亦可单独配置、构建、测试。
15. **Validator：47/47**。SDK 验证器全部通过。
16. **CTest：4/4**。完整构建目录四组测试均通过；不带 SDK 的核心构建目录 2/2 通过。
17. **黄金测试：21 个 fixture 均 PASS**。`major_basic`、`minor_natural`、`minor_harmonic_dominant`、`ii_v_i`、`relative_minor`、`secondary_dominant`、`v_of_v`、`v_of_vi`、`leading_tone`、`borrowed_iv`、`borrowed_biii`、`borrowed_bvi`、`borrowed_bvii`、`short_approach`、`short_dominant`、`long_secondary_dominant`、`duration_base`、`duration_scaled`、`last_open`、`weird`、`structured_cubase`。附加断言覆盖指定调性、名称与 mask 冲突、maj9 颜色音及罗马数字显示。
18. **五组实际输出**：如下。候选数值是规则评分后 24 调归一化的相对权重，不能视作统计正确率；FULL 中每行末尾数字为 StructuralWeight。

### C → F → G → C

- Key candidates：C Major 0.42；F Major 0.18；A Minor 0.08。
- FULL：`C I T 0.82` → `F IV PD 0.72` → `G V D 0.88` → `C I T 0.98`。
- SKELETON：`I → IV → V → I`。

### Dm7 → G7 → Cmaj7

- Key candidates：C Major 0.76；D Minor 0.11；A Minor 0.06。
- FULL：`Dm7 ii7 PD 0.94` → `G7 V7 D 0.88` → `Cmaj7 Imaj7 T 1.00`。
- SKELETON：`ii7 → V7 → Imaj7`。

### C → Am → A7 → Dm → G7 → C

- Key candidates：C Major 0.44；D Minor 0.18；A Minor 0.10。
- FULL：`C I T 0.82` → `Am vi T-prol 0.60` → `A7 V7/ii Chr-D 0.34 [SecondaryDominant, Approach]` → `Dm ii PD 0.88` → `G7 V7 D 0.88` → `C I T 0.98`。
- SKELETON：`I → vi → ii → V7 → I`。

### C → F → Fm → C

- Key candidates：C Major 0.31；F Major 0.20；F Minor 0.09。自动提示调性有歧义。
- FULL：`C I T 0.82` → `F IV PD 0.72` → `Fm iv Chr-PD 0.76 [Borrowed]` → `C I T 0.82`。
- SKELETON：`I → IV → iv → I`。

### C → C#dim7 → Dm → G7 → C

- Key candidates：C Major 0.59；F Major 0.09；A Minor 0.09。
- FULL：`C I T 0.82` → `C#dim7 vii°7/ii Chr-D 0.30 [SecondaryLeadingTone, Approach]` → `Dm ii PD 0.88` → `G7 V7 D 0.88` → `C I T 0.98`。
- SKELETON：`I → ii → V7 → I`。

19. **已知薄弱情况与分析错误**：
    - `C → E7 → Am` 自动首选 A Minor（0.45），而 C Major 仅 0.15；指定 C Major 后可正确给 `E7 = V7/vi`。短进行无法从和弦本身可靠区分两个读法。
    - `Cmaj7 → F#maj7 → Ebm` 自动首选 Db Major 但仅 0.19，出现 `VIImaj7` 的 Unknown 功能。异常进行的首选调性可能不符合人的听觉，需看候选和低置信度。
    - `C → F → Fm → C` 虽识别借用 iv，C Major 仅 0.31，对平行调借用的调性把握仍偏弱。
    - 次属与次导音目前要求下一颗和弦直接解决，延迟解决、转位、替代解决可能漏判；借用范围有限。`Passing`、`Substitution` 尚未自动判定。
    - 当前 `pitches`/mask 的解释基于已有 VST-XML 样本与测试结构；更多真实 Cubase 拖放数据及 UI 观感仍需在宿主中核对。
    - `structuralWeight`、骨架阈值和候选百分比都是首版规则值；尚无音乐语料校准。当前导入会话没有工程再次加载后的自动恢复机制。

Phase 1 到此为止，未加入数据库、相似度检索、续写或 Phase 2 功能。
