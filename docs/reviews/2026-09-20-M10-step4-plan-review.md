## Verdict

**Do not start as written.** Start after two blocking corrections:

1. Replace the plain boolean guard with scoped save-and-restore reply mode for **both** live and replay entries, so nested replay stays suppressed while a re-entrant live entry remains able to reply.
2. Reject the proposed emulation observer seam. Use the existing `ITransport::bytesWritten` observation plus a real PTY peer and the mandated source audit.

The reply-site inventory has no missed write site.

## Findings

- **Blocking — nested/re-entrant suppression is incorrect.**  
  [2026-09-20-M10-step4-reply-free-rendering-note.md](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step4-reply-free-rendering-note.md:55) proposes a plain `bool` set true/false by a replay-only guard. A nested replay call would clear the flag when its inner guard exits, allowing the outer replay call to answer. Conversely, a re-entrant live `slotReceivedChar()` during replay would inherit suppression and fail to answer, contrary to SPEC §5.7’s live-path requirement. Rendering emits synchronous cell-array signals, so this must be safe.  
  Minimal fix: use a save-and-restore RAII scope (for example `QScopedValueRollback<bool>`) in both entries: live scopes set “allow replies,” replay scopes set “suppress replies.” Add nested-replay and re-entrant-live tests.

- **Blocking — the proposed observer seam contradicts the accepted no-substitution decision.**  
  [2026-09-20-M10-step4-reply-free-rendering-note.md](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step4-reply-free-rendering-note.md:75) recommends a new emulation test observer. SPEC §5.8 expressly chooses no transport substitution seam and fixes the evidence: existing `ITransport::bytesWritten`, source audit, and PTY peer silence. `bytesWritten` is emitted from the sole accepted-write primitive at [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:408).  
  Minimal fix: choose **existing `bytesWritten` + PTY + source audit**, no new friend observer and no `ITransport` injection into emulation.

- **Same-pass — query-trigger inventory is narrower than the implementation.**  
  The three write sites are complete: [komportemulation.cpp](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1271), [1277](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1277), and [1292](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1292). But [the note](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step4-reply-free-rendering-note.md:32) understates their triggers: any CSI ending in `c` reaches `doDeviceAttributes()` ([komportemulation.cpp](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1406)), and private/noncanonical forms of `CSI 5 n`/`CSI 6 n` are accepted because `ctlParam()` strips `?` ([komportemulation.cpp](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:682)).  
  Minimal fix: say “all CSI `c` forms currently accepted by the dispatcher” and add one parameterized/private hostile variant to the replay suppression test.

- **Same-pass — source audit must use the accepted, broader contract.**  
  [The proposed static test](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step4-reply-free-rendering-note.md:96) limited to “dispatchers” cannot establish that replay entries themselves have no direct write or that a helper cannot become a bypass.  
  Minimal fix: implement the specified `replayEntriesAndReplySitesPassTheSourceAudit`: audit both replay entries and, once added, the display adapter; require the three reply functions to call `sendTerminalReply()`, and make that choke point the only direct reply write. This is worth its modest maintenance cost because it is the structural part of the safety argument.

- **Same-pass — current live-path description is inaccurate.**  
  [The note](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step4-reply-free-rendering-note.md:38) says the live view slot forwards to emulation. It does not: the view only performs scroll bookkeeping at [komportview.cpp](/home/max/Development/misc/komport-qt6/komport/komportview.cpp:497); emulation independently receives the serial signal at [komportemulation.cpp](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:655).  
  Minimal fix: correct the reasoning and explicitly preserve both existing live connections/bodies unchanged.

## Reach and boundaries

A stored sequence routed through the proposed replay entry has no existing path to key simulation, macros, transfers, line-ending output, or the view’s `keyPressed` signal. Those are active user paths: keyboard at [komportemulation.cpp](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:696), simulated paste at [1581](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1581), macro output at [komport.cpp](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1402), and transfer output at [komporttransfer.cpp](/home/max/Development/misc/komport-qt6/komport/komporttransfer.cpp:56). They are not terminal-generated replies and must remain handled by the later offline guards.

With the corrections, the slice stays Qt 6/C++17-only, does not widen ADR-003, respects ADR-009’s view/emulation split, and can leave the legacy RX signal wiring and RX buffer untouched.

## What I could not verify

No step-4 implementation or tests exist yet, so I could not dynamically verify PTY silence, `bytesWritten` absence, re-entrant behavior, or preservation of live rendering. The later application display adapter and offline guards are also not yet present; their full no-transmit evidence belongs to the specified step-7 tests.