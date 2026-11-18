## 色彩和弦不再被当成插入（v0.14.11）

- 用户输入的 dim / dim7 / m7b5 / aug 以前几乎总是被判成 `QUERY_INSERTION`：DP 里插入一个经过和弦的
  代价约 0.2，而按级数不匹配配对的代价要 0.9 起，所以对齐器永远选择"删掉它"。
- `ProgressionMatcher` 新增色彩音配对：当一侧是 dim 家族 / aug、另一侧是普通和弦，且两者同根或相差
  半音（导音 / 辅助音）时，按 `chromaticColourPairCost = 0.15` 配对并标记 `QualityVariant`。
  `Bdim` 现在与它解决的 `I` 是 `SUBSTITUTE`（cost 0.15），不再被剔除。
- 40 条 5–6 和弦查询：四栏全空 1/40 → **0/40**，空栏 47.5% → 48.8%（色彩和弦参与配对后
  端点选择略有变化）。`ctest` 21/21。
- 试过"骨架层零交集预筛"（查询与模板骨架级数毫无交集就跳过 DP）：`ProgressionMatcherTests` 的
  "unrelated still returns a low-score match" 契约被破坏，且空栏变差，已回退；骨架层目前只有
  "骨架等于完整序列时复用 full 对齐"这一项优化（在 0.14.10 里）。
