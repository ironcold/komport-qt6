Round 1 gate review: **do not flip either document to Accepted.** The proposal has unresolved Blocker/High findings.

1. **BLOCKER** — replay rendering can invoke real transmit code.  
   [SPEC-M10 §5.7](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:522) feeds RX replay bytes into `KomportView::slotReceivedChar()`. The existing emulation answers terminal queries with `serial()->putStr()` ([`komportemulation.cpp`](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1266), [line 1290](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1290)). Thus replayed `CSI 5 n`, `CSI 6 n`, `CSI c`, or `ESC Z` has an output path, contradicting ADR-007 and ADR-011 D14.  
   Minimal fix: specify a replay-only rendering entry point/mode that suppresses terminal-generated replies, and add a test replaying those sequences which proves no `putStr`/transport write occurs.

2. **HIGH** — the loader redefines ADR-006’s v1 acceptance contract as an M9-local writer-profile whitelist.  
   [ADR-011 D2](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:111) and [SPEC-M10’s table](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:250) require exactly the M9 root/schema/local values (`sourceId == 1`, `process-monotonic`, no extra clock-domain members). ADR-006 permits optional decoder hints/notes and optional `wallClockCorrelation`, and defines both local and future agent clock-domain kinds ([ADR-006](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:63)). The proposed reader would refuse such otherwise v1-conformant files. It also repeats ADR-006’s refusal/recovery rules in a new normative 30-row layer rather than citing them.  
   Minimal fix: make ADR-006 the sole normative format/loading rule; have M10 cite and apply it unchanged. Parse the required v1 facts, preserve/ignore permitted optional facts as appropriate, and restrict only the owner-decided `sources[].size() > 1` case.

3. **HIGH** — offline controls do not meet the stated “live controls disabled” / no-output-path scope.  
   [SPEC-M10 §5.7](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:512) omits `Record Live Session...` and Edit → Paste. The latter is a transmit path: [`slotEditPaste()`](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1172) calls `KomportView::slotSimKeyPressed()`, which reaches emulation TX. The planned UI test repeats the incomplete list ([SPEC-M10](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:754)).  
   Minimal fix: enumerate and disable every connection/output action, at minimum live-session recording and Edit → Paste, and test their invocation while offline.

4. **HIGH** — changing timing to `Step` while `Playing` has no coherent state transition.  
   The contract permits `setTiming()` in `Playing` ([SPEC-M10](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:390), [line 420](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:420)), while `Step` schedules nothing and steps are refused in `Playing` ([line 444](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:444)). After the already-pending callback, the player can remain `Playing` with no timer and no permitted step.  
   Minimal fix: either refuse selecting `Step` while playing, or define cancellation plus transition to `Paused`; add a deterministic scheduler test.

5. **MEDIUM** — the stated mechanical safety proof and several planned tests are not actually checkable with the present topology.  
   [SPEC-M10 §5.8](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:561) says the QtCore-only compile target rejects transport types. It cannot: [`ITransport`](/home/max/Development/misc/komport-qt6/komport/itransport.h:25) is itself QtCore-only. Further, UI tests promise a “transport double,” but `KomportDoc` owns a concrete by-value `KomportSerial` ([`komportdoc.h`](/home/max/Development/misc/komport-qt6/komport/komportdoc.h:132)), with no substitution seam.  
   Minimal fix: replace the false compile claim with an explicit, repeatable dependency/source audit plus dynamic no-write evidence; either specify a document test seam or revise UI tests to assertions observable through the real pty and serial state.

6. **MEDIUM** — extra multi-clock-domain refusal is unapproved scope creep.  
   [ADR-011 D2](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:134) and [SPEC-M10](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:257) add a fixed multi-clock-domain policy/message. The owner decision requires only multi-source refusal with the specified M10 message.  
   Minimal fix: remove this additional normative refusal/tests, or obtain an explicit owner decision and record it as scope.

Residual points accepted:

- Existing symbol references checked are real: `SessionRecordCodec` facts, `SessionController` state/anchor APIs, recorder state, document close operation, display slots, and M9’s test parser.
- The declared player/value contracts are QtCore-only; no widget or application type leaks into those contract snippets.
- The timing ladder, forward-only boundary, M11 decoded-frame exclusion, M12 active-output exclusion, and M14 multi-source boundary are otherwise stated consistently.
- Most planned tests describe observable properties; the safety-proof tests above need correction.

Commit decision: **Rejected for this round; Status remains Proposed.**