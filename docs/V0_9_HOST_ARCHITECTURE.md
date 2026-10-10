# v0.9 host boundary

Baseline: formal tag `v0.8.0`, commit `5082eb0133df3d87d21b2a3cf671aedcad463200`.

- `src/host/HostEnvironment` owns host identity, observed capabilities, independently optional timeline fields and privacy-safe diagnostics. It depends on no musical algorithm or SDK types.
- `src/plugin/HostAdapters` chooses Generic / Cubase / FL input guidance. Cubase delegates to the unchanged VST-XML parser; Generic can use explicitly observed compatible XML. FL uses standard MIDI through the common file workflow.
- `src/plugin/HostContextAdapter` copies fixed atomic words in `process()`. Decode, formatting, notifications and diagnostics run outside audio processing. No SQLite, file I/O, JSON, mutex or recommendation work is added to audio processing.
- `src/io/AsyncMidiImport` serializes each instance's bounded, cancelable import tasks. Parsing reuses the original SMF reader and extractor. Only the latest valid result can replace a phrase, on the UI thread. A new state restore or chord drop cancels previous imports.
- `src/io/FileDropRouter` accepts bounded `.mid` / `.midi` FilePath items. Multiple paths are processed until the first valid result. Unknown file types are rejected without replacing the phrase.
- `src/io/MidiDragService` delegates to the unchanged MIDI writer / temporary-file workflow. Candidate identity is captured before dragging; no Save dialog appears during dragging. Save MIDI remains a separate fallback.
- `src/ui/EffectiveScale` separates the host's physical-to-view transform from user zoom. VSTGUI performs host scaling once; MainView performs user zoom once. PluginView guards resize recursion and rebuilds native frames when parents change. The Windows system DXGI runtime is pinned for process lifetime: the SDK EditorHost exposed a message-loop hook pointing into unloaded DXGI after the final plug-in unload. Software Direct2D and disabled DirectComposition remain unchanged.
- Session schema stays 5. Editor selection/scroll state is preserved in memory across removal/reattachment, validated against candidate IDs and fingerprints. Enrichment results are cached in the controller as well as continuation results. Transient drag callbacks lose controller actions on detach.
- Each instance owns its recommendation/import jobs and session. Factory data is read-only; user DB sharing uses the existing SQLite transaction/timeout behavior. Windows PlaySound is process-wide, so an atomic owner prevents one instance's stop/destroy from stopping another's sound.

## SDK basis

Actual local SDK 3.8.1 interfaces inspected: IHostApplication::getName, IPlugView/IPlugFrame, IPlugViewContentScaleSupport, ProcessContext validity flags, MemoryStream, hosting Module/PlugProvider/HostApplication. No fictional API and no private host wire format is used. The SDK exposes no host-version method.

## Test boundary

HostContractHarness centralizes 40 cases and loads the actual VST3 through SDK hosting APIs. Host names change only identity/guidance; simulations use standard callbacks and native windows. Cases cover contexts, import/drop, drag files, ownership, scale/layout, state ordering, resize negotiation, reattached parents, actual 2/4/8 instances, shared library jobs and editor keyboard workflows. It is not an FL emulator. See the gate report for results and standard host availability.
