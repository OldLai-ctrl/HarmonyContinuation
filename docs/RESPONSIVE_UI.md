# Resizable editor (Phase 4C)

## Sizes and layout

The default editor is 1100 × 900 logical VSTGUI units. The minimum is 900 × 640 and the maximum is 2200 × 1400. Width and height are constrained independently; there is no fixed aspect ratio. The host can resize the editor continuously. The standalone Demo has a native resizable window and accepts `--size 900x640`, `--size 1100x900`, or any other size within the limits.

`src/ui/UILayout.h` is the pure layout model used by both the VST3 editor and Demo `MainView`. Its input is logical width, logical height, content scale, active tab, and whether an inspector or comparison tray is open. It returns bounded rectangles for the top controls, phrase timeline, content, four recommendation lanes, library, inspector, and comparison tray. Drawing and hit testing use these rectangles. Resizing recalculates geometry and playhead position only; it does not run analysis, matching, recommendation, database access, preview rendering, or MIDI export.

| Width | Mode | Recommendation lanes | Inspector |
| --- | --- | --- | --- |
| 900–1049 | Compact | One column, vertical scroll | Overlay |
| 1050–1599 | Standard | Two columns | Overlay |
| 1600–2200 | Wide | Four columns once each card has at least 410 units; otherwise two | Side panel |

Four columns begin at width 1702 without an inspector, or width 2106 with a side inspector. Those thresholds keep every card at least 410 logical units wide.

The top controls use two rows in Compact and one row otherwise. The phrase timeline fills the available width. If the sequence is too long, each chord retains at least 56 logical units where the total width limit allows; the mouse wheel scrolls the phrase horizontally. Chord duration and OPEN hold remain the source of block widths. The yellow structural-weight bar is still computed inside each block and snapped to physical pixels. Playhead X uses the same phrase span and scroll offset. The content area scrolls vertically when the four lanes exceed the window height. The comparison tray allocates space at the bottom and stacks entries in Compact. Library page length follows the available height; Compact shows name, style, and intent first, with other metadata in the scrollable inspector. Recommendation cards keep their timeline, intent and score visible and move actions to a second row when card width is limited. The existing/recommended boundary is drawn inside every candidate timeline.

## VST3 lifecycle and DPI

`PluginView` implements `canResize`, `checkSizeConstraint`, and `onSize`. It uses the inherited `getSize`. For a plugin-initiated request, `requestEditorSize` calls `IPlugFrame::resizeView`; the actual VSTGUI frame and `MainView` resize in the host's `onSize` callback. It never resizes the host's native window directly. If a host rejects the request, the current view remains usable.

The editor implements `IPlugViewContentScaleSupport`. Host scale updates use VSTGUI frame zoom for DPI only. Resize itself never scales fonts or controls uniformly. Layout widths are logical; the requested host rectangle is scaled by the content factor, and `onSize` converts it back to logical dimensions. The weight bar continues using the drawing context's physical scale for pixel alignment. The Demo `--scale 1`, `1.25`, `1.5`, or `2` exercises layout inputs; it is a simulation, not a replacement for OS/Cubase DPI validation. The Windows software drawing path remains enabled and DirectComposition remains disabled.

## Size persistence

Session schema is `HCS3` / version 3. `editorWidth` and `editorHeight` store logical dimensions. `HCS1` and `HCS2` load with the 1100 × 900 default; unknown versions still return `UnsupportedVersion`. The controller requests the saved size when a view is restored and keeps the actual size if the host declines. Size-only changes do not enter the recommendation recompute path and do not affect pins, snapshot files, preview events, or MIDI bytes.

## Verification

`UILayoutTests` checks minimum, breakpoint edges, default, wide, maximum, all four scale inputs, inspector/compare modes, bounds, non-overlap, card width, and 16/32/64-chord scrolling. The Demo `--resize-smoke build-vst3/phase4c-resize-smoke.txt` cycles cases A–H through 900 × 640, 1100 × 900, and 1800 × 1000 at scales 1, 1.25, 1.5, and 2. The MIDI test suite confirms that computing resized layouts leaves MIDI bytes unchanged.

**MANUAL HOST TEST REQUIRED:** Cubase resize border and drag behavior; the order and dimensions of Cubase `checkSizeConstraint`/`onSize` calls; acceptance of `resizeView`; saved-size recovery after project close/reopen; movement between monitors with different DPI; high-DPI drag-and-drop; and playhead movement during live resize. No Cubase result is inferred from Demo or EditorHost.
