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
- Build and run targeted standalone tests for modified behavior. The user explicitly requests limited automatic checks; do not repeat full regressions without a concrete failure or a required gate. Never claim a build or host test that was not executed.
- If Git reports dubious ownership, pass `-c safe.directory=<actual-checkout-path>` for that command; do not alter global Git settings.
- Factory content versions live under ProgramData independently of the plugin. Preserve all previously installed library packages and LocalAppData user.db during install/update/uninstall. Keep database work outside the audio thread. Update package validation and isolated installer checks when changing this contract.
