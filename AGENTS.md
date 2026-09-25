# HarmonyContinuation — Phase 1

- Scope: independent Harmony Core analysis. Cubase host integration is frozen. No recommendations, databases, ML, synth, MIDI export, network access or automatic dependency downloads.
- `core/` must compile without Steinberg or VSTGUI headers. Host integration and XML translation belong in `plugin/`. UI only renders state and handles interaction; no music logic.
- Audio thread: no XML, file I/O, logging, mutexes, database queries, expensive allocation or GUI updates. Transfer a bounded lightweight snapshot through an SDK-verified realtime-safe mechanism.
- The visible editor polls the transport snapshot at a bounded rate (about 20 Hz while playing, slower while stopped or hidden). Repaint only changed timeline regions; never call UI from the audio thread.
- Keep state instance-owned and memory bounded. Analyze only after a new import or an explicit key override; transport updates only locate the current chord. No service locators or unnecessary abstractions.
- Inspect the supplied SDK source before implementing VST3/VSTGUI interfaces. Do not invent API names or infer Cubase behavior from synthetic tests.
- Unknown time domains are errors, never silently converted. Preserve raw structured chord fields; an unknown final duration is normal.
- Every experimental host path must expose payload type, size, readable summary, metadata and failure stage. Do not report inaccessible formats as absent.
- Catch exceptions at plugin callback boundaries. Tests must cover malformed input and resource limits.
- Update docs/HOST_SPIKE.md with evidence. Distinguish user-reported Cubase tests, archived logs, and tests run in the current workspace. Do not infer project reopen persistence from an in-memory session.
- Build and run standalone tests when a C++20 compiler and CMake are available. Never claim a build or host test that was not executed.
- In this Codex sandbox the restored folder is owned by Administrators, so Git may report dubious ownership. Pass `-c safe.directory=D:/VibeCoding/HarmonyContinuation` to Git commands for this checkout; do not alter global Git settings.
