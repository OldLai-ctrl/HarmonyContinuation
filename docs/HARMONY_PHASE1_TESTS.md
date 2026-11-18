# Phase 1 黄金测试

`tests/fixtures/harmony/` 的 JSON 文件由开发命令行和 `HarmonyAnalysisTests` 共用。测试优先检查结构化的调性、级数、角色、目标、功能及相对结构权重；格式化字符串只用于少量独立核对。

| Fixture | 主要断言 |
|---|---|
| `major_basic` | C 大调；I–IV–V–I；T–PD–D–T |
| `minor_natural` | A 自然小调的 i–iv–v–i |
| `minor_harmonic_dominant` | A 小调 E7 为主属 V7 |
| `ii_v_i` | C 大调 ii7–V7–Imaj7 与终止链 |
| `relative_minor` | A 小调和 C 大调都保留为候选 |
| `secondary_dominant` | C 大调 A7 为 V7/ii，短时可从骨架省略 |
| `v_of_v` | D7 为 V7/V |
| `v_of_vi` | 自动判断可偏 A 小调；强制 C 大调时 E7 为 V7/vi |
| `leading_tone` | C#dim7 为 vii°7/ii |
| `borrowed_iv` | Fm 为平行小调借用 iv |
| `borrowed_biii` | Eb 为借用 bIII |
| `borrowed_bvi` | Ab 为借用 bVI |
| `borrowed_bvii` | Bb 为借用 bVII |
| `short_approach` | 短 C#dim7 权重低于目标 Dm，骨架可略 |
| `short_dominant` | 短 G7 仍保留在 ii–V–I 骨架 |
| `long_secondary_dominant` | 整小节 A7 可成为结构和弦 |
| `duration_base` / `duration_scaled` | 整体时值倍增后调性与相对权重不变 |
| `last_open` | 最后一颗无时长和弦正常分析、权重非零 |
| `weird` | 不崩溃，允许 Unknown 功能及较低置信度 |
| `structured_cubase` | `keyNote`、`mask`、`pitches` 归一化；名称冲突给诊断 |
| `cubase_triads_mask` | 2026-09-25 实测 C–G–G#dim–Am–Em 的 `0x48/0x24/0x44` mask；五颗和弦性质可识别，C Major 为首选且标记调性歧义 |

可用 `build-core/harmony_cli.exe tests/fixtures/harmony/secondary_dominant.json` 单独检查一次分析，也可增加 `--key C:major` 测试指定调性。插件分析视图仅在新拖放后更新；播放头移动不调用分析函数。
