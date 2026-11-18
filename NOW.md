## 第三批开工：工程脏标记（v0.14.12）

- 插件此前从未调用 `IComponentHandler2::setDirty`：改和声、改筛选都不算"工程已修改"，切标签页、
  缩放、滚动反而把整份 session state 推给宿主。
- `Controller::applySessionState` 现在只在**音乐状态**变化时调 `markProjectDirty()`
  （`recomputeScope != None` 或置顶候选变化）；UI 几何、缩放、滚动、导航不再上报脏。
- `markProjectDirty()` 优先用 `EditController::componentHandler2`，退化到
  `getComponentHandler()` + `FUnknownPtr<IComponentHandler2>`；宿主没实现时静默跳过。
- `ctest` 21/21；`ProductizationTests` 107 checks 通过。
