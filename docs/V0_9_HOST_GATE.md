# 0.9.0-dev.1 host gate

Baseline: formal `v0.8.0` (`5082eb0133df3d87d21b2a3cf671aedcad463200`). Feature freeze started at `ef6db14327c77de4a13a7129d8294a1b84473d96`; the subsequent scoped fix addresses the SDK EditorHost exit callback into unloaded DXGI.

| Check | Result |
|---|---|
| Final CTest | 25/25 |
| FL-name / Generic / Cubase SDK contracts | 40/40 each; simulations, not real DAW gates |
| SDK EditorHost | 2/2 open / editor child / resize / close; second launch |
| Official VST3PluginTestHost 3.11.0.29 | Actual build DLL loaded and its VSTGUI editor child observed |
| Continuation | 42 cases, 214 candidates, path/ranking changes 0; same 3 pre-existing structural hints |
| Enrichment | 30 cases, 161 candidates; exact case data including inversion/scores unchanged |
| Constraint | 16/16 |
| MIDI Import / round-trip | 27/27 / 12/12; quality fixtures 16/16 |
| Demo / Zoom / Resize | MIDI and scrolling PASS / 9/9 / 96/96 |
| Musical Core / MIDI extractor / Factory content diff | None versus v0.8.0 |
| Final package | Validator and SHA-256 results supplied beside Setup in BUILD.txt / SHA256SUMS.txt |

One concentrated regression exposed one failed EditorHost case. Only that window-lifetime problem was repaired and checked locally; one final CTest completed 25/25. Musical benchmark CLI was run once, and was not rerun after the window fix. Validator is deferred to the final packaged plug-in so its build commit is definitive.

Additional boundary fixes: recommendation worker starts after mutex construction; canceled/background imports cannot replace a later restore; enrichment results and validated candidate selection survive editor recreation; non-owning preview instances cannot stop another instance; removed editors clear controller callbacks.

**IMPLEMENTATION READY; REAL FL TEST PENDING.** No FL device is available. Real scan/Wrapper drop delivery/Playlist and Piano Roll targets/DPI/Detached/project reopen/ProcessContext remain pending as listed in FL_STUDIO_COMPATIBILITY.md. No later v0.9 music features are included.

Evidence files live in the ignored `build-v09` directory; final distribution includes the package verification record. Standard [TestHost documentation](https://steinbergmedia.github.io/vst3_dev_portal/pages/What%2Bis%2Bthe%2BVST%2B3%2BSDK/Plug-in%2BTest%2BHost.html) describes its scope; it is not an FL emulator.
