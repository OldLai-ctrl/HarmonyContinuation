# HarmonyContinuation

- Current scope: v0.9.0-dev.1 Windows VST3 host compatibility (Cubase, Generic, FL Studio 20+ candidate). Capability observations take precedence over host names. Real FL verification is pending and is not a development dependency. Preserve established musical algorithms; no private host protocols, live note capture, audio detection or network features.
- `core/` must compile without Steinberg or VSTGUI headers. Host integration and XML translation belong in `plugin/`. UI only renders state and handles interaction; no music logic.
- Audio thread: no XML, file I/O, logging, mutexes, database queries, expensive allocation or GUI updates. Transfer a bounded lightweight snapshot through an SDK-verified realtime-safe mechanism.
- The visible editor polls the transport snapshot at a bounded rate (about 20 Hz while playing, slower while stopped or hidden). Repaint only changed timeline regions; never call UI from the audio thread.
- Keep state instance-owned and memory bounded. Analyze only after a new import or an explicit key override; transport updates only locate the current chord. No service locators or unnecessary abstractions.
- Inspect the supplied SDK source before implementing VST3/VSTGUI interfaces. Do not invent API names or infer Cubase behavior from synthetic tests.
- Unknown time domains are errors, never silently converted. Preserve raw structured chord fields; an unknown final duration is normal.
- Every experimental host path must expose payload type, size, readable summary, metadata and failure stage. Do not report inaccessible formats as absent.
- Catch exceptions at plugin callback boundaries. Tests must cover malformed input and resource limits.
- Update docs/HOST_SPIKE.md with evidence. Distinguish user-reported Cubase tests, archived logs, and tests run in the current workspace. Do not infer project reopen persistence from an in-memory session.
- Build and run targeted standalone tests for modified behavior. The user explicitly requests limited automatic checks; do not repeat full regressions without a concrete failure or a required gate. Never claim a build or host test that was not executed.
- If Git reports dubious ownership, pass `-c safe.directory=<actual-checkout-path>` for that command; do not alter global Git settings.
- Factory content versions live under ProgramData independently of the plugin. Preserve all previously installed library packages and LocalAppData user.db during install/update/uninstall. Keep database work outside the audio thread. Update package validation and isolated installer checks when changing this contract.
