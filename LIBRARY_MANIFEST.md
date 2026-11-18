# 当前库身份与重建链

## v10 / library_version 10（2026-11-18，现行）

| 项 | 值 |
|---|---|
| 条目 | `progressions=4214`（Billboard 2,926 + 手写/继承 1,288） |
| 版本 | `library_version=10`、`schema_version=2` |
| 字节 | 19,185,664 |
| sha256 | `db8415e9571244f45e293b88f6e789f3a9cf80fa91789d4d02a434cbd4369f47` |

与 v10d 数据完全一致，只有两处不同：272 条模板默认风格的继承条目按各自和声重打风格分
（`tools/library_compile/relabel_styles.py`，保留 pop/rock 作底），以及 `library_version`
从 9 抬到 10（内容变了必须抬版本，见 D56）。只读副本：`data/library/factory-lib10.db`。

重建链：

1. `python tools/ingest/build.py billboard --sources data/library/corpus --out <工作目录> --split-units`
2. `python tools/library_compile/compile_bb.py data/library/v8_base.db <工作目录>/candidates.json/candidates.json <库>.db <丢弃名单>`
   （必须用 v8_base 作基线；直接给 C++ 编译会因 16 条同演奏重复而 FAIL）
3. `python tools/library_compile/relabel_styles.py <库>.db <库>-lib10.db --library-version 10`
4. `cmake -S . -B build -DHC_FACTORY_LIBRARY_VERSION=10 -DHC_FACTORY_PREBUILT_DB=<库>-lib10.db -DHC_FACTORY_PREBUILT_HCA=_tools/factory-v8-annotations.hca`
5. `MSBuild build/HarmonyContinuation.sln /p:Configuration=Release /m:1 /nodeReuse:false /p:TrackFileAccess=false`
6. `powershell -File tools/build-installer.ps1 -BuildDirectory build -OutputDirectory build-installer/output -Iscc _tools/InnoSetup-6.7.4/ISCC.exe -FactoryDatabase build/factory.db`

## v10d（2026-11-18，上一版）

| 项 | 值 |
|---|---|
| 条目 | `progressions=4214`（Billboard 2,926 + 手写/继承 1,288） |
| 版本 | `library_version=9`、`schema_version=2` |
| 字节 | 19,181,568 |
| sha256 | `9e0246e2e182d50068eadcd05af03086424da30c124589720ac6ed03a53b793c` |

v10d 只改入库与编译两处：回收 12/8（32 首、BB 条目 +88），去重下沉到"库实际存储的块"一级，
丢弃名单写进产物（`dropped.tsv`），另出 `phrase-links.tsv` 给"乐句级发展推荐"占位。
交叉基线仍丢弃 16 条（与继承条目同演奏），名单见 `_ingest_bb_v10d/dropped-compile.txt`。

只读副本：`data/library/factory-v10d.db`。

## v10c（2026-11-18，上一版）

| 项 | 值 |
|---|---|
| 条目 | `progressions=4126`（Billboard 2,838 + 手写/继承 1,288） |
| 版本 | `library_version=9`、`schema_version=2` |
| 字节 | 18,747,392 |
| sha256 | `2de2605f6b7405cc5e01b9bbae6f14c1cfeeb7ba7680dc61d1d12ce5f589f274` |

v10c 与 v10 只差风格打分：classical / functional 从"全条件满足才给分"改成按证据累加，
classical 35 → 1,119 条、functional 838 → 1,880 条；其余字段逐条比对零差异。
`boundaries.pickup_length` 同时加了"首小节时值不满即弱起"的判据（本语料命中 0 条）。
安装包 `HarmonyContinuation-0.14.1-Setup.exe`。

只读副本：`data/library/factory-v10c.db`。

## v10（2026-11-18，上一版）

| 项 | 值 |
|---|---|
| 条目 | `progressions=4126`（Billboard 2,838 + 手写/继承 1,288） |
| 版本 | `library_version=9`、`schema_version=2` |
| 字节 | 18,726,912 |
| sha256 | `d73b41f2cab13ec59e30a39c738948b5d737fc5c5a5e1b31b6a7150092e532b2` |

v10 改入库层的判定与标签，不动切分：`classify_phrase` 按"怎么收尾"重排（落主/落属→收束，
周期或回卷→循环，变化音→色彩，其余发展）；新条目用 `tools/ingest/naming.py` 起中文/英文名；
风格扩到 15 类（新增 classical、other）。BB 2,838 条的意图分布：resolve 1,076 / loop 788 /
develop 500 / color 490；纯级数名 0 条。

只读副本：`data/library/factory-v10.db`。

## v9（2026-11-18，上一版）

| 项 | 值 |
|---|---|
| 条目 | `progressions=4126`（Billboard 2,838 + 手写/继承 1,288） |
| 版本 | `library_version=9`、`schema_version=2` |
| 字节 | 18,636,800 |
| sha256 | `cf7d654e68b35196c465f92ec26aaf306fcaf2b2c25ef758fb3862f03f243e42` |

v9 只改了一件事：`nameEn` 的段名从 `[cverseverse 3 u1]` 修成 `[verse 3 u1]`
（根因在 `tools/ingest/billboard.py` 把 `(letter+name, name)` 元组当字符串用，
`build.py` 再过滤成 `cverseverse`）。逐条比对 v8：`id` / `sequence` / `rhythm` /
`intent` / `cadence` 完全相同，差异只在段名与由此产生的体积。

三处安装副本逐字节相同：`build/factory.db`、`build/Release/factory.db`、
`build/VST3/Release/HarmonyContinuation.vst3/Contents/Resources/factory.db`
（在 `/build*/` 下，不进 git；带 sha 的只读副本见下）。

## v8（2026-11-17，上一版）

`progressions=4126`、18,661,376 字节、sha256
`1aeb206fb3cc06ac99e7d16ee6cad45c3f5612e912e2b68db8156bb7d39d2182`。内容与 v9 相同，
只是段名全部拼串。

## 入库副本（进 git，用于重建与回滚）

- `data/library/factory-v9.db` —— 上表那条库的只读副本（18.6 MB）
- `data/library/factory-v8.db` —— 上一版（18.6 MB）
- `data/library/v8_base.db` —— 编库前的基线库（1,288 条，4.8 MB）
- `data/library/v8_phrases.tsv` —— v8 的 2,854 个乐句单元（可读文本，抽检用）
- `data/library/corpus/` —— 语料原样：`bb/`（已解开的 SALAMI 语料）、
  `billboard-salami.tar.xz`、`billboard-index.csv`、`rs200.txt`、`rs200_harmony_exp.zip`

## 重建链

```
# 1. 入库（切分 + 标注）。--out 是工作目录，实际写出 <out>/candidates.json/{candidates.json,phrases.tsv}
python tools/ingest/build.py billboard --sources data/library/corpus --out <工作目录> --split-units

# 2. 基线 1,288 条
./build/Release/factory_v6_compiler.exe --v8-incremental data/library/v8_base.db ...

# 3. 编库
python tools/library_compile/compile_bb.py data/library/v8_base.db <candidates.json> <out.db>
#   输出行形如 V9-A PASS schema=2 inherited=1288 added=2838 total=4126

# 4. 安装
cp <out.db> build/factory.db && cp <out.db> build/Release/factory.db
cp <out.db> "build/VST3/Release/HarmonyContinuation.vst3/Contents/Resources/factory.db"
```

## 本机构建环境注意（2026-11-18 实测）

- MSBuild 的 CL.exe 文件跟踪在本机会失败（`TRK0002` / `MSB6006`，`CL.exe` 退出码 2，
  且一条编译器错误都不打印）。必须加 `/p:TrackFileAccess=false` 且用 `/m:1`；
  用 `cmake --build --parallel` 会直接失败。
- Windows PowerShell 5.1 读无 BOM 的 UTF-8 脚本会把中文解码坏并导致语法错误，
  `tools/build-installer.ps1` 已改成 UTF-8 with BOM。

## 不进 git 的东西

- `build/`（737 MB 构建产物，含三份 `factory.db`）
- 仓库外的历史库：`D:\VibeCoding\factory-bb*.db`、`_ingest_bb_v2…v9\`、`_hc_evidence\`
  （已拷入 `data/library/` 的部分除外）
- 这些是派生物；git 只保证源码、语料、编库脚本可回到同一状态。

## 注意

git 在同一个盘上，防的是误删误改，不是盘坏。要防盘坏，需要额外把仓库推到远端或拷到另一块盘。
