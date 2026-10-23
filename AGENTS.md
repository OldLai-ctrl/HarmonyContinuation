# HarmonyContinuation

## 按需读取（必须遵守）

1. 接手时确认项目根目录、适用的项目/子目录规则和当前任务。完整理解适用规则，不能为节省上下文跳过它们；已加载且仍有效的内容不重复读取。
2. **按当前问题选择资料，不因文件存在、可能有用或被索引链接就读取。禁止默认读取文件夹中的所有内容，也不默认全文读取选中的文件。** 找到文件后继续定位相关章节、条目、函数或配置项。
3. 优先使用已有有效信息。需要查找时按“索引或限定范围搜索 → 标题、符号或关键词 → 命中片段及必要上下文”读取。找文件用 `rg --files <范围> -g '*关键词*'`，找事实用 `rg -n '关键词|符号|错误文本' <相关路径>`。
4. 只有证据不足、存在冲突或需要核对依赖时才扩大范围；信息足够即停止。禁止连续分段读取以变相遍历无关全文；片段过窄无法理解语义时读取必要上下文，避免断章取义。
5. 全文读取仅用于短小且全部相关的文件、完整理解适用规则，或用户明确要求整份审查的任务；这不构成全目录或全项目读取的理由。
6. [PROJECT_INDEX.md](PROJECT_INDEX.md) 是条件路由，不是必读清单。历史、日志、依赖、产物和数据目录不默认读取；不读取密钥或无关私人数据。
7. 发生矛盾时核对事实来源、适用范围和版本。当前用户要求、实际代码/产物、有效决定优先于旧阶段文档或聊天摘要；不能凭目录名否认已有进展，也不能把交接描述当作当前执行证据。

## 写入与接续记录（必须遵守）

1. 仅在任务允许且信息有实质变化时更新；只读任务不改记录。没有状态变化不机械刷新日期。缺失事实标注“待核实”，不编造环境、安排或验证结果。
2. 每类事实只有一个权威来源，其他位置使用简短摘要和项目内相对链接。文件分工见下面的路由；已有同等职责的内容优先沿用，不重复建档或强制改名。
3. 修改前读取目标部分及必要上下文；局部修改使用定点编辑，不得根据局部阅读覆盖整份文件。保留既有有效成果。
4. 当前状态直接修订，有价值的旧过程归档到 history；不反复追加矛盾快照，不保存完整聊天、全量日志或操作/发布流水账。已完成的临时待办从当前状态移走。
5. 明确区分事实、计划、建议、已确认决定和待核实项；分别记录代码完成、测试通过、安装验证、用户验收、发布授权，不用一个“完成”代替所有阶段。
6. 命令已配置不等于执行成功，历史结果不等于当前证据，文档中的步骤不构成执行授权。证据注明对象、版本/提交、配置、范围及来源；记录核验日期时说明核验对象，不把日期当作所有结论的新鲜度证明。
7. 使用清晰标题和项目内相对链接；不写入秘密、无关个人信息或个人设备私有绝对路径。设备路径用环境变量或占位符，操作时在本机核实。结束时记录改动、相关检查与局限、待完成事项和提交/推送状态；不可访问的包/日志明确标为待核实。

## 条件路由与维护位置

| 当前需要 | 定位位置 | 实质变化后写入 |
| --- | --- | --- |
| 了解/使用项目 | README 对应使用章节 | 实际能力或使用方式 |
| 找上下文或入口 | PROJECT_INDEX 对应路由 | 定位、职责或入口；保持约 2–4 KB |
| 接续目标、进度、排期或待验收 | NOW 对应章节 | 当前状态、阻塞、下一步及证据入口 |
| 定位版本、配置、运行/部署对象 | MAP 对应关系 | 有效映射及同步边界，替换失效行 |
| 运行、构建、验收或交付 | RUNBOOK 对应步骤 | 有依据的方法、前置条件与验证步骤 |
| 设计选择冲突 | DECISIONS 相关条目 | 已确认决定、理由、范围、来源与取代关系 |
| 对应高风险操作或验证缺口 | RISKS 相关条目 | 风险证据、状态及保护措施 |
| 追溯已结束过程 | 搜索 history，再读命中上下文 | 必要归档及 history/README 的入口 |

权威位置由此表定义；代码事实仍以代码为准，证据指向原始结果。专题文档负责细节，入口不复制专题正文。无独立用途不新建 PROJECT_NOTES 或其他汇总副本。

## 执行与表达规则（必须遵守）

1. **GitHub 推送必须通过网络代理。** 推送前核实当前环境可用的代理配置，并确保实际推送命令使用该代理；地址与端口以本机有效配置为准。
2. **严禁编写非必要的新增测试脚本或程序。** 优先复用现有测试、工具、命令和仍然有效的证据。只有现有手段无法覆盖当前任务的具体缺陷、行为变化或必需验收条件时，才允许增加最小必要测试，并简要说明覆盖缺口。文档、格式等低影响改动使用直接检查；必要检查完成即停止。
3. **每开启一个新步骤，运用 Stop That Shit 技能。** 首次使用读取当前可用的 `stop-that-shit` SKILL.md；后续步骤复用已加载且有效的规则，按 Stop Ladder 核对当前职责、最直接方案、具体缺口与完成条件。按任务授权区分只读和修改范围；检查融入执行，不额外生成清单、报告、测试程序或审批环节。技能指引与 Guard 实际启用状态分别表述。
4. **使用简明扼要的科学语言。** 直接陈述结论、事实、证据、适用条件和必要的不确定性。拒绝“对比否定、否定纠正性对比、元语言否定、防御推论式否定、无依据意图归因、假设性框架、回声式复述”等常见 GPT 表述。省去无关对照、主观揣测、防御性铺垫和对用户原话的机械重复；假设仅在任务确有需要时明确标注并给出依据。此规则适用于对话、文档和子代理交付。

## 多代理协作与高成本模型用量优化规范

本项目由**主代理（Main Agent）**担任技术负责人。主代理的具体模型由用户决定，可以是 Astra、6.1-Sol 或其他模型；不得自行假定主代理固定为某一个模型。

### 一、核心原则

**高成本模型负责高价值判断，低成本模型负责标准化执行。**

项目开发任务由主代理主动安排必要的子代理工作，用户无需逐次重复要求。分配遵守下述并发预算、文件归属和最终审核责任；不为增加代理数量拆分简单任务。

在保证代码质量、音乐合理性与项目安全的前提下，尽可能减少主代理的重复性工作和不必要的上下文消耗。

### 二、任务分配规则

主代理承担最终责任的职责如下；当主代理为 Astra 时，细节分析、判断和审查按第三节委派给 6.1-Sol-high，Astra 只保留最高层规划、重大决策及最终门禁决策：

1. 需求理解与技术架构决策。
2. 推荐算法、和声逻辑、评分机制的关键判断。
3. 高风险修改的方案审批。
4. 判断测试失败属于真实缺陷、测试错误还是预期行为。
5. 决定是否需要调整算法或回退。
6. 审查重要代码差异和关键测试证据。
7. 最终质量门禁与交付结论。

在满足上述执行与表达规则、确认工作必要后，优先委派给 **6-Luna-max-fast**：

1. 历史报告检索、阅读与摘要。
2. 代码结构扫描及调用链整理。
3. 批量 MIDI 测试数据生成。
4. 已明确规格的单元测试编写。
5. 自动化测试执行。
6. 日志解析与失败信息归类。
7. CSV 分析与统计。
8. 小范围、低风险且实现方案明确的代码修改。
9. 性能数据采集。
10. 文档整理、构建、哈希及交付文件检查。

子代理不承担未经授权的重大技术决策。

### 三、可选的分层代理模式

当主代理使用 Astra 且执行环境支持时，优先采用以下分工，以尽可能减少 Astra 用量：

- **Astra：** 只承担最高层目标与工作包规划、重大架构及音乐语义取舍、重大风险处置、最终验收与发布门禁决策。基于结构化审查摘要和必要的关键证据作决策，不常规阅读长日志、逐行审查代码或重复分析细节。
- **6.1-Sol-high：** 承担细节技术判断和日常技术统筹，包括根因分析、和声与评分细节分析、代码审查、测试失败性质判断、局部方案选择、兼容性与性能评估，以及汇总 Luna 的执行结果。在已批准方案和授权范围内推进工作；涉及第五节重大事项时，提供结论、备选方案、风险与证据位置，交 Astra 作最终决策。
- **6-Luna-max-fast：** 测试、实现、数据处理、构建和其他标准化工作。

这不是强制三级架构。规格明确、无需细节技术判断的工作可直接采用主代理 + Luna 两级协作；Astra 模式下需要细节判断时，优先交给 6.1-Sol-high，不以省去中间代理为由让 Astra 接管常规分析。主代理需要根据实际任务选择最经济的组织方式，并遵守执行环境的并发与模型可用性限制。

6.1-Sol-high 集中提交简明决策包：结论、必要决策点、关键证据位置、未解决风险和真实 Cubase 待验证项。普通测试通过、低风险修复和文档完成不逐项唤醒 Astra；达到工作包交付节点或遇到关键失败、架构冲突、重大风险时再集中升级。

### 四、上下文与额度优化

1. 优先让低成本代理阅读长日志和大量报告。
2. 子代理返回结构化摘要、关键证据位置、测试结果与异常清单。
3. 主代理按需查看原始证据，不重复阅读无关内容。
4. 已验证且没有变化的结论，不反复重新分析。
5. 相互独立的低风险任务尽量并行。
6. 每项委派必须明确输入、输出、限制和验收条件。
7. 低风险任务完成后可以自动进入后续已授权工作。
8. 不因单个普通测试通过或文档完成而频繁唤醒高成本主代理。
9. 发生关键失败、架构冲突或重大风险时再升级。
10. 对于已经明确的修复方案，不要求主代理亲自编写每一行实现代码。

并发安排：

- 默认采用主代理 + 最多 2 个并发子代理；存在第三项独立、低风险且收益明确的工作，并且执行环境支持时，可增加至 3 个。并发预算包括中间代理及其子代理，不按层级重复计算；简单任务不为凑并发而拆分。
- 优先并发历史报告与日志整理、调用链扫描、MIDI 案例与测试准备、独立验证，以及构建期间对已有证据和文档的核对。
- 每个工作包明确输入、输出、文件归属、依赖、输出位置、验收条件和停止条件。有依赖的任务按顺序执行；根因未确定时可并发取证，不并发猜测并修改算法。
- 同一源码文件、Git 工作树及构建目录只安排一个写入者；其他代理只读，或使用独立工作树和输出目录。产物哈希与交付检查必须等待对应构建完成。
- 性能测量避免与大型构建或重负载测试争用资源。集中审核并发结果，避免主代理逐项重复分析。

### 五、质量与风险控制

节约额度不能成为跳过必要审核的理由。以下情况必须由主代理作最终决策；Astra 模式下，细节分析与审查由 6.1-Sol-high 完成后提交决策包，不要求 Astra 重复执行分析：

- 核心和声性质兼容规则改变；
- Factory 匹配或评分语义改变；
- 多个评分机制可能重复奖励；
- Schema 或 Snapshot 兼容性风险；
- User Library 数据安全问题；
- 大规模架构修改；
- 无法解释的关键回归；
- 重要音乐质量争议；
- 正式发布质量门禁。

子代理不得自行决定忽略失败测试、降低评分阈值或修改受保护数据。

### 六、批量工作与集中交付

主代理应一次安排尽可能完整的相关工作包，包括分析、实现、增量测试、回归、性能验证、构建及文档。

除非触发明确停止条件，否则无需逐项返回用户重新申请批准。子代理之间应避免并发写入同一源码、Git 工作树或构建目录。

最后由主代理统一负责下列审核；Astra 模式下由 6.1-Sol-high 汇总详细审查结果，Astra 核对决策所需的关键证据并作最终门禁决策：

- 任务完成情况；
- 代码修改；
- 测试证据；
- 音乐语义；
- 性能；
- 兼容性；
- Git 状态；
- 交付产物；
- 未解决风险。

### 七、最终责任

**子代理负责执行，主代理对最终结果承担技术审核责任。**

子代理报告“已完成”不等于任务已通过验收。主代理必须核对关键证据，但不需要机械重复子代理已经完成且可验证的全部工作。Astra 模式下，6.1-Sol-high 承担详细技术审核，Astra 保留最终决策责任；不得把最终责任解释为要求 Astra 亲自完成所有细节判断。

最终交付应同时说明主代理亲自判断了哪些问题、委派了哪些工作，以及哪些结论仍需真实 Cubase 验证。

目标：以尽可能低的高成本模型消耗，完成可靠、可回退、可验证的 HarmonyContinuation 开发任务。

## 工程边界

- Current scope and acceptance status: search NOW.md and PROJECT_INDEX.md. Capability observations take precedence over host names. Preserve established musical algorithms unless the current task explicitly changes them; no private host protocols, live note capture, audio detection or network features in the current compatibility scope.
- `core/` must compile without Steinberg or VSTGUI headers. Host integration and XML translation belong in `plugin/`. UI only renders state and handles interaction; no music logic.
- Audio thread: no XML, file I/O, logging, mutexes, database queries, expensive allocation or GUI updates. Transfer a bounded lightweight snapshot through an SDK-verified realtime-safe mechanism.
- The visible editor polls the transport snapshot at a bounded rate (about 20 Hz while playing, slower while stopped or hidden). Repaint only changed timeline regions; never call UI from the audio thread.
- Keep state instance-owned and memory bounded. Analyze only after a new import or an explicit key override; transport updates only locate the current chord. No service locators or unnecessary abstractions.
- Inspect the supplied SDK source before implementing VST3/VSTGUI interfaces. Do not invent API names or infer Cubase behavior from synthetic tests.
- Unknown time domains are errors, never silently converted. Preserve raw structured chord fields; an unknown final duration is normal.
- Every experimental host path must expose payload type, size, readable summary, metadata and failure stage. Do not report inaccessible formats as absent.
- Catch exceptions at plugin callback boundaries. Tests must cover malformed input and resource limits.
- Update the relevant docs/HOST_SPIKE.md section when host evidence changes and the task permits recording it. Distinguish user-reported Cubase tests, archived logs, and tests run in the current workspace. Do not infer project reopen persistence from an in-memory session.
- For modified behavior, reuse existing targeted tests and build checks first. Add test scripts or programs only for a concrete coverage gap under the execution rules above. The user explicitly requests limited automatic checks; do not repeat full regressions without a concrete failure or a required gate. Never claim a build or host test that was not executed.
- If Git reports dubious ownership, pass `-c safe.directory=<actual-checkout-path>` for that command; do not alter global Git settings.
- Factory content versions live under ProgramData independently of the plugin. Preserve all previously installed library packages and LocalAppData user.db during install/update/uninstall. Keep database work outside the audio thread. Update package validation and isolated installer checks when changing this contract.
