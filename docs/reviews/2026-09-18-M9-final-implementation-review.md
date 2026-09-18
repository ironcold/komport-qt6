## Closing verdict

**M9 should not yet be declared closed.** The core recorder design is sound, and the three documented amendments are accepted and match the implementation, but the acceptance evidence and one production edge case are not complete.

- §8 and the self-review claim the flush tests are shipped names, but these three do not exist: `pendingTailIsFlushedWithinOneSecondWithoutFurtherEvents`, `burstFlushesAtMostOncePerSecond`, and `flushIntervalBoundsTheBufferedTail`. They remain design-time names in §7/§8. The actual test is [`theProcessBufferIsBoundedByOneTimerPerBurst`](</home/max/Development/misc/komport-qt6/tests/tst_sessionrecorder.cpp:536>), which covers much of the combined behavior. So “two names” is incorrect: it is three, and §8 is not aligned as claimed. [SPEC §8](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:373)

- The “64 KiB upload keeps the recorder memory flat” mapping overstates test evidence. The 64 KiB test exercises `KomportSerial` with a counter, not `SessionRecorder`; the recorder e2e test uses roughly 2.2 KiB RX, and the timer test does not measure recorder memory. [transport test](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:1093) This is an additional test gap beyond the three stated ones.

- A failed *initial header flush* is neither tested nor handled as the accepted text’s damaged transition: [`start()`](/home/max/Development/misc/komport-qt6/komport/sessionrecorder.cpp:212) closes the file and returns a refused start while remaining `Stopped`. ADR-010 says a flush failure is damaged and reported as a write failure. [ADR-010](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:171) This needs either a small code/test fix or an explicit accepted clarification.

- The additional concrete open item is the `quint64` record count being narrowed to `int` for `%n`. Record counts above `INT_MAX` are displayed/pluralized incorrectly, while M9 defines no cap. [report type](/home/max/Development/misc/komport-qt6/komport/sessionrecorder.h:80) [conversion](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1522) This was already identified in the step-5b round-3 review and was not resolved. [review](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M9-implementation-review-step5b-round3.md:25)

The three amendments themselves are correctly recorded and implemented:

- `KomportDoc::closeSession()` is accepted and correctly closes the transport before finalizing the recorder. [ADR-010](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:54) [code](/home/max/Development/misc/komport-qt6/komport/komportdoc.cpp:235)
- The close-path reporting exemption matches: clean close is silent; damaged close logs all required facts. [SPEC](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:213) [code](/home/max/Development/misc/komport-qt6/komport/komport.cpp:966)
- `appliedConfigurationSnapshot()` is accepted in SPEC-M8 and shares the live-metadata section builder as required. [SPEC-M8](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:252) [code](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:909)

I found no other architectural or format deviation in the shipped code. The truncation repair concern is adequately supported by the recorded hash checks.

One process item also remains: the §8 rewrite is modified but uncommitted, and the step-6 and self-review records are untracked in the current tree. Under the binding workflow, commit those records and the corrected acceptance text before closure.

I could not rerun the reported warning build or 17/17 CTest in this read-only environment. Finally, I disagree with the claim that late reviews found only verification weaknesses: the unresolved `quint64`→`int` conversion is a real production correctness defect, not merely weak verification.