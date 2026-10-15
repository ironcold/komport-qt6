## Verdict

**No — do not start implementation yet.**

## Disposition per item

- Private/noncanonical CSI-`c` correction: accurate. The dispatcher selects on final `c`; `doDeviceAttributes()` ignores parameters. [Note](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step4-reply-free-rendering-note.md:32), [dispatcher](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1406).
- “Two reply functions, three sites”: accurate everywhere in the current note.
- Explicit default “replies allowed”: present and correct. [Scoped design](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step4-reply-free-rendering-note.md:60).
- Staged audit: correctly limits step 4 to the two replay entries and reply sites, adding the display adapter in steps 6–7.

The proposed `QScopedValueRollback<bool>` design and private choke-point placement are otherwise sound: scope both public receive entries immediately around the shared renderer; route exactly the two status-report sites and one device-attributes site through the choke point.

## Remaining findings

1. **Blocking — the re-entry test remains underspecified.**  
   The plan still merely says “in both directions.” [Test row](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step4-reply-free-rendering-note.md:109). It must explicitly create synchronous re-entry—for example, a `cellChanged` receiver, which is emitted synchronously during `drawChar()`. [Emission](/home/max/Development/misc/komport-qt6/komport/komportcellarray.cpp:251). Test nested replay and re-entrant live queries while the outer replay render is active, including that suppression is restored after each inner call.

2. **Blocking — the replay-query test does not pin the required view path or ordering.**  
   It currently drives an unspecified “replay entry.” [Test row](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step4-reply-free-rendering-note.md:105). It must drive `KomportView::slotReplayReceivedChar()` with printable sentinels and assert identical rendering/scroll bookkeeping to live input. The view should forward to emulation before its copied bookkeeping, matching current live connection order: emulation is connected first, view bookkeeping second. [View setup](/home/max/Development/misc/komport-qt6/komport/komportview.cpp:88), [live bookkeeping](/home/max/Development/misc/komport-qt6/komport/komportview.cpp:497).

These are the unresolved requirements from the prior round, not new architecture work. Once added to the plan, implementation may start.

## What I could not verify

No step-4 code or tests exist, so I could not exercise PTY silence, `bytesWritten`, source-audit behavior, or actual scoped re-entry.