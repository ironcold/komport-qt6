Verdict: **do not commit**.

Minimum blocking fix: make the view-level test exercise real scrollback and preserved scroll state, and extend the structural audit to `KomportView::slotReplayReceivedChar()` with syntax-independent detection of direct `putChar`/`putStr`/`writeBytes` calls.

Findings:

1. **Blocking — the new view equivalence test does not exercise scroll bookkeeping.**  
   [tests/tst_replayreplyui.cpp:131](/home/max/Development/misc/komport-qt6/tests/tst_replayreplyui.cpp:131) sends only a cursor move plus one short row; it never scrolls. Consequently [lines 216–218](/home/max/Development/misc/komport-qt6/tests/tst_replayreplyui.cpp:216) merely prove both scrollbar values are zero/current-bottom, not that scrollback content, maximum, or mid-stream auto-follow behavior match. The disabled-key test also does not establish retained focus, selection, or a non-bottom scroll position.

   Minimal fix: use a fixed shown geometry and nonzero scroll buffer; send enough complete rows to scroll, end the final row mid-line, compare full history plus grid/cursor/bar state, and place both views at a non-bottom position before further input. Assert disabled key input preserves focus/selection/scroll position.

2. **Blocking — the source audit still omits a replay object mandated by SPEC-M10.**  
   [tests/tst_replayreply.cpp:273](/home/max/Development/misc/komport-qt6/tests/tst_replayreply.cpp:273) reads only `komportemulation.cpp`; the compiled paths at [tests/CMakeLists.txt:71](/home/max/Development/misc/komport-qt6/tests/CMakeLists.txt:71) likewise omit `komportview.cpp`. That conflicts with SPEC-M10 §5.8’s required audit of `KomportView::slotReplayReceivedChar()`.

   The current spelling list at [tests/tst_replayreply.cpp:283](/home/max/Development/misc/komport-qt6/tests/tst_replayreply.cpp:283) is evaded by a direct valid call such as `(*getSerial()).putStr(...)` in the view, or `(*serial()).putStr(...)` in the emulation. Neither contains `->putStr(`. `QMetaObject::invokeMethod(serial(), "putChar", ...)` also evades it.

   Minimal fix: audit the view replay slot as well; lexically strip comments/strings and detect direct transmit method identifiers independent of member-access spelling, with brace-aware function extraction. State the unavoidable limitation for indirect/meta-object calls.

3. **Blocking — the reply-mode helper is a new public capability without specification.**  
   [komport/komportemulation.h:570](/home/max/Development/misc/komport-qt6/komport/komportemulation.h:570) exposes `KomportEmulationReplyModeScope`, with public constructor at [line 573](/home/max/Development/misc/komport-qt6/komport/komportemulation.h:573). Any consumer can now suppress or re-enable replies for any emulation. The `friend` is narrow syntactically, but its public constructor turns it into an externally usable writer of the private flag—broader than the previous in-method rollback.

   Minimal fix: leave only a forward declaration/friend in the header and define the helper privately in the `.cpp` (or make it a private nested implementation type).

4. **High, same-pass — the round-one lifetime finding is only partly closed.**  
   The `QPointer` correctly prevents the scope destructor’s own write-after-free at [komportemulation.cpp:1475](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1475). The documented contract explicitly covers both the view and emulation at [komportemulation.h:682](/home/max/Development/misc/komport-qt6/komport/komportemulation.h:682).

   But it is prose only. A `cellChanged` receiver can delete either object during [drawChar](/home/max/Development/misc/komport-qt6/komport/komportcellarray.cpp:251); rendering then resumes and accesses the dead emulation/cell array at [komportemulation.cpp:1603](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1603). Deleting the view also deletes `mEmulation` at [komportview.cpp:108](/home/max/Development/misc/komport-qt6/komport/komportview.cpp:108). No guard enforces the stated precondition.

   The `static_cast` itself is not presently a mis-cast: the pointer is created only from a `KomportEmulation`; a destroyed QObject clears the `QPointer`, and a same-address replacement is not adopted. That does not repair the resumed render.

Attack log:

- The proposed live-chunk/replay-byte mismatch does **not** exist in this implementation: `readAll()` is buffered, then [KomportSerial::slotFlushRxBuffer()](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:561) emits one `receivedChar` per byte. Live and replay invoke view bookkeeping per byte.
- The stream covers: cursor move, `CSI 5 n` status report, `CSI 6 n` cursor report, `CSI c` attributes, `ESC Z` legacy attributes, and private `CSI ?1 c`. The tail-marker synchronization is sufficient for ordered delivery of this isolated stream. Exact live replies and per-query replay silence are additionally covered by `tst_replayreply`.
- The 300 ms peer-silence window is bounded evidence, not a general electrical guarantee; no current deferred reply path was found.
- In this read-only sandbox, `tst_replayreply`’s PTY tests skipped because lock files cannot be created; `tst_replayreplyui` fails rather than skips on the same condition. I could verify only its source audit dynamically here.