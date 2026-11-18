# B5 语料清单与许可

2026-11-13 核验。B5 用原版 McGill Billboard 2.0 学习和验证统计模型，另用 MPB Corpus v1.1 检查跨来源泛化。两份语料都来自真实作品的人工和声注释。Billboard 对应商业录音中的演奏形式，MPB 对应出版 songbook 中的作品名义形式。

## 输入版本与恢复

`<B5证据目录>/sources/` 存放以下原始注释文件。跨设备可从发布方重新取得，并按 SHA-256 核对。两份 Billboard 文件从 DDMAL 官方链接重新下载，字节与 G13.1 已保存的原始输入一致。

| 文件 | 固定来源 | SHA-256 |
| --- | --- | --- |
| `billboard-2.0-index.csv` | [McGill Billboard 2.0 官方索引](https://www.dropbox.com/s/o0olz0uwl9z9stb/billboard-2.0-index.csv?dl=1) | `fc39435098814f1a4f1dd5c95c72aff399b648b78f33a59682924dfb9b5280fb` |
| `billboard-2.0-salami_chords.tar.xz` | [McGill Billboard 2.0 完整注释](https://www.dropbox.com/s/p4xtixbvt4hw5c6/billboard-2.0-salami_chords.tar.xz?dl=1) | `92d5797743f5bbba070fd233b028918707832066340641bd27f435236ee7457e` |
| `mpb-v1.1-harmony.csv` | [MPB v1.1 汇总 harmony.csv](https://raw.githubusercontent.com/ProjetoMPB/mpb-corpus/cd9cb3ddd5a140d5f1bbed4047f7a449ae6929f5/dataset_aggregated/harmony.csv) | `ab3ffdf8aeba3302ed1093a3dfb62a579bfbcbfbf56c3c306f06e1d826a69620` |

MPB `v1.1` 指向 `cd9cb3ddd5a140d5f1bbed4047f7a449ae6929f5`。作者论文引用同版数据归档，固定输入由上表 GitHub 原文件核验。历史 G13.1 对照文件 `derived/billboard_events.jsonl` 的 SHA-256 为 `a6a15baf155aaf454392c702334eff01f5f67ef66a1fac61141a7ab57f7347a2`，旧 `analyze_billboard.py` 为 `9aae32e7ebf71d8469ae5bf89540722eda75abc85a37b06bd0e3bc1e542ff36a`。见 [MPB v1.1 发布页](https://github.com/ProjetoMPB/mpb-corpus/releases/tag/v1.1)、[MPB v1.1 归档](https://zenodo.org/records/22737013)、[G13.1 来源记录](G131_CORPUS_GRAMMAR.md)。

本轮在 `sources/` 留存相同哈希的 JSONL 与解析脚本。优先迁移已核验的 JSONL。只带原注释和脚本时，在临时目录中按脚本约定摆放 `raw/` 的两份官方 Billboard 文件，运行 `python -B analyze_billboard.py`，将生成的 `derived/billboard_events.jsonl` 按上文校验后放回 `sources/`。本轮已验证从这两份官方原文件重建的 JSONL 字节一致。

## 授权及本轮用途

| 来源与版本 | 原始与转换许可、统计学习、分发条件 | B5 用途 |
| --- | --- | --- |
| **McGill Billboard 2.0 原版** | DDMAL 以 CC0 1.0 发布索引及注释，覆盖注释复制、转换、统计学习和商业使用。B5 保留版本、出处与注释权利范围。公开成果按发布方要求引用 Burgoyne、Wild、Fujinaga（ISMIR 2011，633–638）。原始录音和音乐作品的权利另计。 | 产品许可可用的训练来源。模型仍需独立评估和制品身份核验。 [DDMAL 发布页](https://ddmal.ca/research/The_McGill_Billboard_Project_(Chord_Analysis_Dataset)/) · [原始论文](https://ismir2011.ismir.net/papers/OS8-1.pdf) · [CC0 正文](https://creativecommons.org/publicdomain/zero/1.0/legalcode.en) |
| **MPB Corpus v1.1** | 作者以 CC BY 4.0 发布人工衍生注释，允许统计学习、商用及署名后再分发。汇总 CSV 来自同一项目的分作曲家表。分发适用许可的转换注释或衍生材料时，保留作者及来源、许可链接、变更说明和原有声明，并遵循下游限制条款。原始乐谱、录音和第三方目录 ID 分别受其权利与条款约束。 | 固定跨来源测试。引用 Carlos de L. Almada、Hugo T. de Carvalho、Felipe D. Martins（2026），*TISMIR* 9(1), 565–587。 [发布与许可](https://github.com/ProjetoMPB/mpb-corpus#license) · [论文](https://doi.org/10.5334/tismir.357) · [CC BY 正文](https://creativecommons.org/licenses/by/4.0/legalcode.en) |
| **Rock Corpus RS200 v2.1** | 作者对站点发布的分析、转录、工具授予 CC BY 4.0，可署名后统计学习、商用及再分发适用许可的转换数据。保留 Trevor de Clercq、David Temperley、来源、许可及变更说明。两位作者的分析是同一作品的不同标注。 | 沿用 A2 最小样本的来源证据。现有 RS200 索引与 Billboard 有 24 个归一化标题交集，本轮选 MPB 做跨来源测试。 [原站与许可](https://rockcorpus.midside.com/) · [和声下载](https://rockcorpus.midside.com/harmonic_analyses.html) · [RS200 索引](https://rockcorpus.midside.com/overview/rs200.txt) |
| **ChoCo v1.0.0** | JAMS/RDF 转换数据通常为 CC BY 4.0。Chordify Annotator Subjectivity、Mozart Piano Sonata、Jazz Audio-Aligned Harmony 三个分区为 CC BY-NC-SA 4.0，限制商业用途并要求适用的改编在分发时沿用同许可。集成许可和各分区原始材料的许可应分别核对。例如原版 Billboard 为 CC0，ChoCo 转换数据的通用条款为 CC BY。 | 暂无训练或测试输入。ChoCo 已含 Billboard、Rock Corpus 和多来源版本，候选分区接入前逐源查重及核验授权。 [固定归档](https://zenodo.org/records/7706751) · [许可文件](https://github.com/smashub/choco/blob/v1.0.0/LICENSE.md) · [论文](https://doi.org/10.1038/s41597-023-02410-w) |
| **RWC-P / IdolSongsJp / JSIR** | RWC-P 注释在既有固定提交 `0a1a6c31dbe73a7f5d44f7caef8cd0999402a4c2` 为 CC BY-NC 4.0。AIST 的 IdolSongsJp 允许非商业学术研究且要求引用，商业研究和产品集成需事先许可，音频训练另有规定。JSIR `1dd6b8b0d67244f19feec9c944c9fc215d3fcec0` 的注释为 CC BY-SA 4.0，可在遵守署名与相同许可条件时商用。其 267 个录音只代表 44 首爵士标准作品。 | 沿用 A2、G13.1 的隔离边界，本轮未使用。 [RWC-P 固定许可](https://raw.githubusercontent.com/rwc-music/rwc-annotations/0a1a6c31dbe73a7f5d44f7caef8cd0999402a4c2/LICENSE) · [AIST 数据许可](https://huggingface.co/datasets/imprt/idol-songs-jp/blob/main/LICENSE.md) · [JSIR 注释许可](https://github.com/MALerLab/JSIR/tree/1dd6b8b0d67244f19feec9c944c9fc215d3fcec0#license) |

跨设备可用原发布地址和摘要恢复这三份输入。正式产品只从已核验商业权限的输入构建候选模型，实验语料、受限研究资料和制品保持各自的来源清单。统计制品的分发资格取决于输入、许可、制品实际内容和分发方式，需随制品单独核验。当前 G13 商业先验的来源仍为 [Billboard CC0 先验](../data/generation-priors/billboard-cc0.tsv)。

## 字段与独立性

- **Billboard** 官方 1,300 个抽样槽位中，890 个有注释。已有 G13.1 解析保留 740 个代表录音注释、19,365 个源乐句、107,804 个展开事件，其中 106,196 个可解析和弦、106,193 个可解析和弦有符号 QN、9,533 个和弦有显式斜线低音。`N`、`&pause`、`*` 合计 1,608 个事件，分别代表非和弦/暂停/未标注，形成统计断点。无斜线标签的实际低音留空。源注释有调中心和拍号，调式未知。QN 来自来源小节和符号，秒标记对应乐句首事件，其他内点在 LAB 格式由定速假设插值。索引日期覆盖 1958–1991 的美国 Hot 100，缺逐曲子风格。 [官方格式说明](https://ddmal.ca/research/The_McGill_Billboard_Project_(Chord_Analysis_Dataset)/)
- **MPB** 固定 CSV 有 500 首作品、10 位作曲家各 50 首、17,053 个和弦行。`corpus_id` 与 `composition_id` 组成来源作品 ID。`chord_index` 在每首内连续。`root`、`bass`、`key` 为音级整数，`chord_type` 为区分大小写的谱系类型，`chord_symbol` 保留扩展和变化音，其中 `*` 是根音占位符。共有 1,930 行低音音级异于根音，320 首仅标大调、66 首仅标小调、114 首两种调式均出现，194 首出现多个标注调中心。`position` 是小节与小节分数，3,862 行含非整数位置，和声表缺每事件拍号、精确节拍相位、持续时间和乐句界线。作品名义形式省略器乐引子、间奏、尾声和字面反复，跨来源续接只能使用和弦事件窗。 [语料方法及符号定义](https://arxiv.org/html/2608.13842v1#S2) · [表字段](https://github.com/ProjetoMPB/mpb-corpus#harmonycsv)
- **身份核对** G13.1 将重复 Billboard 槽位按归一化曲名和演唱者归并，剩余 740 个不同曲名/演唱者配对、730 个归一化曲名。作品级划分应将同名翻唱和版本置于同一侧，例如 `Last Kiss`、`Unforgettable` 与 `Unchained Melody`。MPB 归一化曲名无内部重复，和 Billboard 的交集为 `Amor` 一例，MPB 对应 `LINS/8`、59 行，Billboard 表演者为 Ben E. King。标题碰撞是应审慎隔离的线索，异名、翻译题名及改编仍须结合元数据和移调不变的和声序列识别。源文件本身未提供覆盖全库的作品权威 ID 或可用于比对的音频。

## 方法参考

- Pearce（2018）的 IDyOM 根据历史上下文混合不同阶数的条件分布，为长上下文样本稀疏时的回退提供参考。论文的音乐认知证据主要来自旋律，B5 的和声推断由独立作品评估。 [Pearce 2018](https://doi.org/10.1111/nyas.13654) · [IDyOM 说明](https://www.marcus-pearce.com/idyom/)
- Carsault 等（2021）比较多步续接和弦模型，并讨论节拍表示的自转移与保持当前和弦基线。B5 分别观察和弦变化及时间表示，以区分学习到的和声变化和同一和弦的延续。 [Carsault 等 2021](https://www.mdpi.com/2079-9292/10/21/2634)
