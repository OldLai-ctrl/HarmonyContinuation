# G01 归属标注 v1

本目录独立于已发布 Factory V5 / Schema2。`factory-v5.hca` 仅含三条真实进行的四个逐条编辑审校样例；其余条目保持无标注。Reviewer 是本次主代理的音乐结构审校，不代表用户实听验收或外部音乐学家签字。

本批来源为基线 `c8d8dc3` 的Factory生成源；重建V5与已安装V5的SHA-256一致：`F989A83EA9EF92BA7C272BF0E5E3C7073C225899DC81D60DD46C10EA6B48CB3C`。每条标注仍独立校验源音乐指纹，不能仅凭库版本或上述文件哈希复用归属。

## 文件格式与身份

UTF-8、无 BOM；首行 `HC_MODE_ANNOTATIONS 1`。其余非空非注释行各保存一个归属，按顺序为：带引号的 Factory ID、整数相对主音偏移、带引号的源音乐指纹、调式、常用程度、常用程度依据、音乐证据、审查状态、审校者、旋律上下文。字符串采用标准 quoted 反斜杠转义。同一源的多行合成一个记录；重复 ID/主音/调式或同 ID 冲突指纹均拒绝。

`source-v1` 指纹包含既有 canonicalMusicalFingerprint 及在 C 参考主音的完整实化身份，校验和弦、低音、扩展、拍号、时值、原始和弦实化及相对参考系；它是完整结构化指纹，不是文件 SHA-256。它不使用推荐排序指纹。旧 ID 不自动跳转到新 ID。标注加载不会改数据库、原评分投影或音符。

归属保存 `tonicOffset`（相对模板参考主音，0..11）、`mode`、`usage`、`usageBasis`、`musicalEvidence`、`review`、`reviewer`、`melodicContext`。常用程度 high/medium/low/unrated 只表示编辑评定；未评定无需虚构依据。review 为 pending/reviewed/unconfirmed；另有运行时无标注、源失配和无效状态。已审校记录必须有非空音乐证据、审校者；已评级必须有评级依据。解析上限1 MiB / 8192条。

自然大调和 Ionian、自然小调和 Aeolian 分别共用身份。另支持 Dorian、Phrygian、Lydian、Mixolydian、Locrian、和声小调、固定上行旋律小调、古典旋律小调的枚举。古典旋律小调不提供静态音阶mask，已审校记录须有旋律上下文；本批没有该归属。

## 本批审校依据

- `COMMON_MINOR_007`：`i VI III VII`，循环进行。以模板主音为中心审校 Aeolian；以相对大调主音（+3半音）为中心审校 Ionian，分别评级高、中。依据是小主和弦起点与相对大调解释同时可成立的 Axis 循环，区别于仅检查音符都在某音阶中；此二义性有 [Richards 的 Axis 分析](https://www.mtosmt.org/issues/mto.17.23.3/mto.17.23.3.richards.php)及[教材四和弦模式](https://viva.pressbooks.pub/openmusictheory/chapter/4-chord-schemas/)支持。评级差异是此模板呈现的编辑判断，不是该论文提供的频率或概率。
- `COMMON_MINOR_001`：`i iv V i`，中心偏移0，和声小调，高。小主和弦与小下属和弦保留降三级、降六级，属大三和弦含升七级并解决回主和弦，构成和声小调的和声证据；未推断其具有古典旋律小调旋律。[小调音阶依据](https://viva.pressbooks.pub/openmusictheory/chapter/minor-scales/)。
- `V4_A08`：`i IV i VII IV i`，中心偏移0，Dorian，高。反复的小主和弦与大IV形成Dorian特征的自然六级，降七级和声及回主提供中心依据。[Dorian和声模式](https://viva.pressbooks.pub/openmusictheory/chapter/modal-schemas/)。此次归属来自对完整进行的复核，原 `DorianColor` 标签仅作检索线索。

## 抽样消费边界

无调式限制时，合法且无标注的Factory仍可抽取；固定主音指模板参考主音。设置调式时仅消费来源校验通过的 reviewed 归属，多选为并集；固定主音此时指归属中心，移调参考主音按偏移换算。严格筛选额外检查完整路线全部音高；无选定调式的严格请求返回错误。全局模式枚举不表示本批已有对应归属卡池。

源、标签与展示解释不增加同一完整音乐的票数。完整音乐身份含实际根音、低音、音集、性质、精确音集标志、扩展、相对onset/QN、时值、顶声部与拍号。不同前缀不能仅因后缀一致合并。家族桶只使用移调无关的连接轮廓；桶间及桶内变体采用随本轮已抽次数渐减的柔性权重，保留全部合格音乐。每个结构变体先选一次，再选其实际主音实例，避免12份移调凭数量增加结构权重。

纯随机完整路线通过现有实化形成，空续写与空match；Snapshot3冻结完整音符。输入约束路线复用 Unified 和 sourceRoute，投影后仍检查硬旋律约束。这里的同步接口只供非实时工作路径调用，G2尚未接入插件UI或音频回调。
