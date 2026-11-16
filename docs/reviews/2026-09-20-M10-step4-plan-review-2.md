## Verdict

**Do not start as written.** The two previous blocking issues are resolved, but one test-evidence correction and two accuracy/sequence corrections are needed first.

## Disposition

- **Scoped suppression:** Resolved. Save-and-restore in both entries correctly covers all mode nestings: replay→replay stays suppressed; replay→live permits the inner reply then restores suppression; and the converse directions restore correctly. This remains true for synchronous signal re-entry. Initialize the member explicitly to “replies allowed.”

- **Observer seam:** Resolved. The revised plan correctly uses existing `bytesWritten`, the source audit, and PTY-peer silence—no new seam or transport injection. This matches SPEC §5.8 and the accepted-write observation in [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:401).

- **Inventory:** Partially resolved. The three reply *sites* are complete, and all CSI sequences ending in `c` reach `doDeviceAttributes()`. But [the note](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step4-reply-free-rendering-note.md:32) incorrectly attributes private/noncanonical CSI-`c` handling to `ctlParam()`: that helper is used only by status reports; device attributes ignores parameters after the dispatcher selects `c` ([komportemulation.cpp](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1265)). Also say “three reply sites in two reply functions,” not “three reply functions.”

- **Broader source audit:** Resolved in design. It now matches the required scope. Stage it with the display adapter in step 6/7; the step-4 commit can audit the two replay entries and reply sites, then extend the audit when the adapter exists.

- **Live-path description:** Resolved. The corrected independent live connections and view-only bookkeeping agree with [komportview.cpp](/home/max/Development/misc/komport-qt6/komport/komportview.cpp:497).

## New blocking finding

`aNestedReplayStaysSuppressedAndAReentrantLiveCallAnswers` must explicitly cause **synchronous signal re-entry during rendering**. A direct nested-call test would not prove the scenario that motivated the correction.

Specify a test hook such as a `cellChanged` receiver—emitted synchronously by `drawChar()`—which, while an outer replay byte is rendering, invokes the same emulation’s replay entry with a query, then separately its live entry with a query. Assert:

- inner replay produces no `bytesWritten` and outer replay remains suppressed afterward;
- inner live produces exactly its expected reply, and outer replay is suppressed again afterward;
- the outer render completes normally.

Also make the replay-query test explicitly drive `KomportView::slotReplayReceivedChar()`, with printable sentinels around the queries, and assert the same rendered/consumed result and scroll bookkeeping as live. That pins the required view forwarding path, not merely the emulation helper.

## What I could not verify

No step-4 code or tests exist yet, so I could not run the source audit, exercise the real PTY, observe `bytesWritten`, or confirm the proposed re-entry test actually interleaves during a render.