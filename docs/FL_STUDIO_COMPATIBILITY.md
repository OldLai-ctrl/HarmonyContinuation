# FL Studio 20+ compatibility candidate — 0.9.0-dev.1

**IMPLEMENTATION READY** is a software delivery status. **REAL FL STUDIO VERIFIED** has not been reached. This workstation has no FL Studio installation. Standard VST3 host simulations cannot establish FL Wrapper behavior.

| Feature | Automated Status | Real FL Status |
|---|---|---|
| Standard VST3 load and editor | SDK HostContractHarness + EditorHost; see gate report | REAL FL TEST PENDING |
| Host identification / Generic fallback | Standard IHostApplication; name never enables capabilities | REAL FL TEST PENDING |
| ProcessContext | Per-field optional QN, tempo, meter, transport; null/invalid safe | REAL FL TEST PENDING |
| MIDI file picker | Existing SMF reader/extractor, background parsing | REAL FL TEST PENDING |
| Explorer .mid/.midi drop | Bounded FilePath routing, first valid file, unsupported files rejected | REAL FL TEST PENDING: Wrapper Accept dropped files |
| MIDI drag source | Existing minimal DAWClip, unique temporary FilePath, Save MIDI fallback | REAL FL TEST PENDING: Playlist / Piano Roll drop targets |
| Continue / Enrich / constraints | Frozen v0.8 musical baseline; see gate report | REAL FL TEST PENDING |
| More candidates / scrolling / Why / snapshot | Existing workflow reused; UI smoke | REAL FL TEST PENDING |
| State restore / close / reopen | Actual plug-in loaded by SDK harness, both state/view orderings | REAL FL TEST PENDING: project save / reopen |
| Multiple instances | 2 / 4 / 8 independent sessions, jobs, drag files; shared user DB | REAL FL TEST PENDING |
| Content scale / user zoom | 4 host scales × 3 user zooms × 3 layouts; host resize rejection/reentry | REAL FL TEST PENDING: actual FL DPI |
| Detached-like lifecycle | Removal / reattach / new native parent simulated | REAL FL TEST PENDING: FL Detached |
| Keyboard focus / Escape | Only received editor events; unhandled shortcuts passed to host | REAL FL TEST PENDING |
| Preview | Existing Windows default-device playback; process-wide ownership guard | REAL FL TEST PENDING |
| Factory / user Library | Library 2, 161 entries, same Library Manager and local user DB | REAL FL TEST PENDING |
| Installation / update / uninstall | Same standard Windows VST3 installer; data-preserving behavior retained | REAL FL TEST PENDING: Plugin Manager scan |
| Host Diagnostics | Advanced menu, safe text export; no music/user library/path/machine IDs | REAL FL TEST PENDING |

## 简短使用说明

1. 关闭使用插件的宿主，运行本版安装程序。安装位置仍是 Windows 标准 VST3 目录。
2. 在 FL Studio 的 Options → Manage plugins 中启动扫描，并启用 Verify plugins。按扫描结果找到 HarmonyContinuation（当前插件仍是 Effect）。
3. 打开插件，用“更多 → 导入 MIDI”导入 `.mid` / `.midi`，或拖到当前进行区域。若 FL 没有把文件交给插件，检查 Wrapper 的 **Accept dropped files**。本插件不改变 FL 设置。
4. 推荐项上的 MIDI 可以拖出；如果目标位置不接受，点击保存 MIDI，再按 FL 的标准 MIDI 文件导入流程使用。
5. “更多 → 宿主诊断 / 导出宿主诊断”可查看运行环境。

## Quick setup (English)

Close hosts using the plug-in and run Setup. In FL Studio, open Options → Manage plugins, scan and enable Verify plugins. Load the detected HarmonyContinuation effect. Import a standard MIDI file through More → Import MIDI, or drop it onto the current phrase area. If files are not delivered, check the Wrapper's **Accept dropped files** setting. No FL settings are modified by Setup or the plug-in. Drag a candidate's MIDI handle to a supported host target, or use Save MIDI and the host's standard MIDI import. Host Diagnostics and its safe text export are in More.

## Boundaries

Cubase's established Chord Track / absolute-QN workflow is retained. FL uses standard MIDI files; private Piano Roll / Playlist data is not decoded. No live capture or audio chord detection is added. Preview still uses Windows' default playback device, independently of the DAW mixer; starting preview in another instance replaces the current process-wide playback. Closing a non-owning instance cannot stop that other preview. Empty recommendation groups and a complete one-chord result remain valid.

Host version is unavailable because this SDK's IHostApplication provides no version method. Capability diagnostics describe observations, not promises inferred from a host name. Export contains controlled host/environment fields only. Save MIDI preserves the existing annotated format; drag files keep the existing minimal DAWClip format.

## Minimal real-machine gate (later)

Load / open UI / MIDI import / Continue / Enrich / Preview / drag-out / save and reopen project / 100% and 150% zoom / Detached / export diagnostics. Also record Plugin Manager scan, Wrapper file delivery, actual DPI and actual ProcessContext fields. These pending checks do not block delivery of this development candidate.

Official references: [Plugin Manager / external plug-ins](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm), [Wrapper file handling](https://www.image-line.com/fl-studio-learning-content/fl-studio-online-manual/html/plugins/wrapper.htm).
