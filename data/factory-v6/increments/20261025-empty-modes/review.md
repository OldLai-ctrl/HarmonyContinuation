# Factory V6 空调式卡池：独立增量审查

G5更新：本目录两条显式归属和冻结来源已接入dev.5的Factory V6/1016构建；源音乐、原创说明及人工听评Pending状态不变。[实际整合与交付](../../../../docs/G01_G4_ACCEPTANCE.md#g5-集中整合交付2026-10-25)。以下为专项时的独立审查记录，旧dev.4仍保持1014条和原哈希。

基线：0.11.0-dev.4 / `5ae1b5ab4294e2e0d51b02372639579a35ead7e5`，Factory6 / Schema2 / 1014。分支 `development/factory-v6-empty-modes-20261025`。本目录是独立候选，不进入现有构建输入，不替换已验证 dev.4 数据或安装。

## 零候选的实际原因

| 选项 | dev.4 原始 HCA | 绑定实际 1014 源后 | 过滤器/加载结论 |
| --- | --- | --- | --- |
| Locrian | 059、060 两条待审核，已审核 0 | 两源均未入选实际 DB，归属 0 | 加载及来源投影正常；Locrian 静态严格音集可用，零池源于缺少已审核调中心归属 |
| 古典旋律小调 | 057 一条待审核，已审核 0 | 该源未入选实际 DB，归属 0 | 加载正常；另有严格方向校验能力缺口，`modePitchMask` 为 0，严格筛选拒绝，不能只补文字标签解决 |

完整旧候选 ID 为 `V6_B1_059`、`V6_B1_060`、`V6_B1_057`。原始 HCA 1023 源/1092 归属正确投影到实际 1014 源；没有吞掉已审核的这两种调式标签，也没有把加载失败当作空池。

现有内容先行复核：059 的 C 踏板提供中心线索，但旧审查仍对 Db/Gb 的稳定感有异议，继续隔离；060 的 `Cm7b5–Ebm–Gb–Bbm–Cm7b5` 只有兼容音集，缺少充分 C 中心依据，不提升状态。已入库 `V6_B3_02_042`、`V6_B3_03_066` 是含自然 G 的 C Phrygian 语境，出现 Gm7b5 或转位低音不足以改标 Locrian。旧音符和标注状态全部保留。

## 两条新增 Locrian 原创候选

由主代理编写并作完整句结构审校，不是作品转录，不宣称统计常用。两条均 C 中心、4/4、32QN，五个事件时值依次 8/4/4/4/12QN，全部实际低音为 C，音集均为 C Db Eb F Gb Ab Bb。常用程度为“未评定”，人工听评另列 Pending。

| ID | 完整路线 | 中心和独立性依据 |
| --- | --- | --- |
| `V6_EMPTY_LOCRIAN_001` | Cdim–Db/C–Cdim–Gb/C–Cdim | Cdim 占 24/32QN，首、中、尾结构回返；两个短时上层和弦不取得根位终止，持续 C 低音支持减主中心 |
| `V6_EMPTY_LOCRIAN_002` | Cm7b5–Ab7/C–Fm/C–Gb/C–Cm7b5 | 首尾 Cø 占 20/32QN；上层 Ab–F–Gb 的侧向拱形回 C，Ab7 不解到 Db；与第一条的根音/性质路线不同 |

归属判断综合作曲规定、中心低音、时值、反复及非功能回返；不由单个半减七和弦推导，也不声称听觉解释唯一。Locrian 的音阶结构及以踏板音帮助保持中心的通用依据见 [Open Music Theory：调式](https://viva.pressbooks.pub/openmusictheory/chapter/intro-to-diatonic-modes-and-the-chromatic-scale/) 与 [modal schemas](https://viva.pressbooks.pub/openmusictheory/chapter/modal-schemas/)。这些教材不构成对两条原创作品的外部审校。

`metadata.json` 逐条保存明确的 Locrian 归属、相对中心 0、未评定依据、审校者和本次审校冻结的完整来源指纹；`candidates.json` 保存完整音符/低音/QN及来源 ID。构建检查先验证当前音符与冻结指纹一致，再验证 DB/HCA 往返后逐源一致，避免未来改音符沿用旧审校。独立于旧库的两个完整音乐身份和两个核心家族检查通过。

## 古典旋律小调：归属 0，条件性材料与显示

057 的 `Cm–F–G–Cm–Bb–Ab–Gm–Cm` 只能说明和声区域可能支持旋律小调的不同音级，不包含可检验的旋律走向、配对音符或作品来源，故仍是隔离的**条件性和声兼容材料**，不新增已确认归属，不写入可抽卡 HCA，不伪造旋律。

古典旋律小调的上/下行及语境差别见 [Open University：melodic minor](https://www.open.edu/openlearn/history-the-arts/music/an-introduction-music-theory/content-section-5.9) 和 [Open Music Theory：scales](https://open-musictheory.github.io/docs/fundamentals/scales/)。静态和弦音集不能代替逐音旋律上下文。现有验证器要求 reviewed classical 具备非空 `melodicContext`，但文本非空本身不证明音乐依据，现有严格过滤器也尚不能校验方向；保留此边界，不能把升降两套音集合并后宣称支持。

界面：两项无审核来源显示 `0`；Locrian 简述“无已审校中心证据”，古典选项关闭严格时说明“缺旋律上下行证据”，开启严格时说明“严格方向校验未实现”。加载错误显示数量未知，不冒报 0。不再错误提示已有候选的爵士旋律小调为空。未放宽任何硬约束。

## 本轮实际最小验证

私有输出：`D:/VibeCoding/HarmonyContinuation-v6-empty-modes-20261025/`。Release 构建仅 `FactoryV6Tests`、`HostContractHarness` 和依赖；没有生成或安装新插件/Setup。

- `FactoryV6Tests --empty-mode-check <dev.4 factory.db> <本目录> <candidate-output>`：退出 0。基线两调式普通/严格固定 C 池均 0；来源投影成功，没有加载或指纹错误。
- 独立增量 Locrian 固定 C 普通/严格池各 2；十抽实际 2，剩余及再抽 0；不固定主音的单池为 24 个合法演奏对象。古典新增审核归属及严格池保持 0。
- 私有 `candidate-not-for-install.db` 为原 1014 + 新 2 = 1016，Schema2；配套 HCA 1016 源，新增两源的指纹和显式归属往返一致。因既有 V3+ 历史重定向校验需要原库目标，未制作缺少这些目标的“两条孤立 DB”。该文件仍用测试版本号 6，**禁止安装或替换 dev.4**；将来整合须形成独立内容/构建身份。
- 两条候选复用既有 Preview/MIDI/Snapshot3 完整路线往返检查通过，完整音乐身份与去时值核心家族均不同于基线及彼此。没有重审其他调式。
- `HostContractHarness --empty-modes-ui <dev.4 factory.db> <ui-evidence>`：退出 0；真实 VSTGUI 100%/150% PNG 已目视检查，两个 0 标签与原因无截断。只验证本轮空池界面，未冒充实际宿主或音频播放验收。
- 首次核心检查发现新候选误用了未定义内建标签 `modal-study`；仅修正新数据为内建 `loop`，附带清理未定义 technique 后，同一窄入口复跑通过。未改解析器、门槛或测试预期。

日志：最终冻结指纹检查为 `core-check-final.log` / `build-final.log`；首次修正后结果为 `core-check.log`，UI 为 `ui-check.log`；图：`ui-evidence/empty-modes-100.png` 和 `empty-modes-150.png`。dev.4 数据/Setup 与 Factory V5 源 DB 的保护哈希由交付前检查核对，结果记录在 NOW。没有用既有生产安装记录冒充本轮实际安装验收。

## 纳入建议与边界

建议下一独立开发构建纳入这两条增量和空池说明；先明确新内容身份及生成链，再做受影响的安装/加载窄验收。人工听评仍需判断 C 中心是否足够稳定，不能把结构审校写成人工听评 PASS。古典保持 0，补充可靠旋律证据和方向校验另行评审。本轮不修改评分、Factory V5、Schema2、Session5、Snapshot3、音频线程或安装器；不动生产及真实 user.db，不制作新 Setup，不自动推送/合并/Tag/Release。
