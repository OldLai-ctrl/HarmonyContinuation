# 当前库身份与重建链

## v8（2026-11-17，现行）

| 项 | 值 |
|---|---|
| 条目 | `progressions=4126` |
| 版本 | `library_version=9`、`schema_version=2` |
| 字节 | 18,661,376 |
| sha256 | `1aeb206fb3cc06ac99e7d16ee6cad45c3f5612e912e2b68db8156bb7d39d2182` |

三处安装副本逐字节相同：`build/factory.db`、`build/Release/factory.db`、
`build/VST3/Release/HarmonyContinuation.vst3/Contents/Resources/factory.db`
（均在 `.gitignore` 的 `/build*/` 之外，不进 git；带 sha 的只读副本见下）。

## 入库副本（进 git，用于重建与回滚）

- `data/library/factory-v8.db` —— 上表那条库的只读副本（18.6 MB）
- `data/library/v8_base.db` —— 编库前的基线库（1,288 条，4.8 MB）
- `data/library/v8_phrases.tsv` —— v8 的 2,854 个乐句单元（可读文本，抽检用）
- `data/library/corpus/` —— 语料原样：`billboard-salami.tar.xz`、`billboard-index.csv`、
  `rs200.txt`、`rs200_harmony_exp.zip`（4.0 MB）

## 重建链

```
# 1. 入库（切分 + 标注），见 tools/ingest/
python tools/ingest/build.py billboard --sources data/library/corpus/sources --out <工作目录> --split-units

# 2. 基线 1,288 条
./build/Release/factory_v6_compiler.exe --v8-incremental data/library/v8_base.db ... 

# 3. 编库
python tools/library_compile/compile_bb.py data/library/v8_base.db <candidates.json> <out.db>
#   输出行形如 V9-A PASS schema=2 inherited=1288 added=2838 total=4126

# 4. 安装
cp <out.db> build/factory.db && cp <out.db> build/Release/factory.db
cp <out.db> "build/VST3/Release/HarmonyContinuation.vst3/Contents/Resources/factory.db"
```

完整的分步记录（为什么这么切、每轮的实测数字）见 `_handover/STATE.md` 第 6、7 步与
`DECISIONS.md` D38–D43。

## 不进 git 的东西

- `build/`（737 MB 构建产物，含三份 `factory.db`）
- 仓库外的历史库：`D:\VibeCoding\factory-bb*.db`、`_ingest_bb_v2…v8\`、`_hc_evidence\`（已拷入
  `data/library/` 的部分除外）
- 这些是派生物；git 只保证源码、语料、编库脚本可回到同一状态。

## 注意

git 在同一个盘上，防的是误删误改，不是盘坏。要防盘坏，需要额外把仓库推到远端或拷到另一块盘。
