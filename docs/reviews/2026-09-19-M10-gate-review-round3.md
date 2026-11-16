## Verdict: rejected for round 3

Keep both `Status: Proposed` lines unchanged. M10 still has architectural and contract-level blockers, not merely editorial residue.

### Round-2 dispositions

| Item | Disposition | Evidence |
|---|---|---|
| 1. `setTiming()` refusal value | resolved | Both contracts now use `SessionReplayTimingChange`; `Step` while `Playing` returns the specified refusal and preserves state/timer/policy. [ADR-011:297](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:297), [SPEC-M10:508](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:508) |
| 2. `New &Window` bypass | resolved, within the newly stated per-window boundary | The table now names the real action/call chain and requires a direct guard. It matches current code. [SPEC-M10:683](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:683), [komport.cpp:1014](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1014) |
| 3. duplicate clock-domain IDs / malformed correlation | resolved | Rows 15 and 17 now enforce both ADR-006 requirements and name tests. [SPEC-M10:301](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:301), [SPEC-M10:303](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:303) |
| 4. record-length ordering | partially resolved | SPEC-M10 correctly adds pre-recovery checks, but ADR-011 still says every final record with fewer than 44 bytes is recovered, contradicting those checks; checked arithmetic is also not specified. [ADR-011:229](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:229), [SPEC-M10:304](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:304) |
| 5. diagram used live display slot | resolved | The component diagram now uses `slotReplayReceivedChar()` and labels the reply-free emulation route. [SPEC-M10:155](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:155) |
| Cleanup: “one local restriction” | resolved | D2 now correctly distinguishes one format restriction from loader I/O preconditions. [ADR-011:172](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:172) |
| Cleanup: audit scope / pty evidence | resolved | The source audit now includes replay entry points and reply sites; the pty is limited to dynamic evidence. [SPEC-M10:753](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:753) |

Nothing from round 2 appears silently dropped. The unresolved record-length issue is a stale contradiction, not a removed requirement.

### New blocking findings

1. **ARCHITECTURE QUESTION — blocking: offline scope is contradictory across windows.**

   ADR-011 calls this an application-wide exclusive offline state and rejects live-plus-loaded coexistence. [ADR-011:444](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:444), [ADR-011:476](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:476)  
   SPEC-M10 instead makes it a single window’s document state and expressly permits a previously opened window to retain a live transport. [SPEC-M10:707](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:707)

   The latter is implementable with the present topology: each `KomportApp` owns its own `KomportDoc` and serial transport. This is not a text-only choice, however. The owner must decide whether offline exclusivity is process-wide or per window. Then amend ADR D11/D14, the preconditions, and tests consistently.

2. **HIGH — record recovery remains inconsistent and lacks a required overflow rule.**

   SPEC rows 18–19 correctly refuse a declared length below 40 or above 64 MiB before recovery. [SPEC-M10:304](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:304)  
   But ADR D3 still unconditionally recovers “fewer than 44 bytes remaining,” including a final record that contains a four-byte invalid length. [ADR-011:229](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:229)

   Further, neither document requires checked/widened arithmetic for `4 + recordLength` and `40 + metadataLength + payloadLength`. This permits a `u32` implementation to overflow while validating malicious field lengths.

   Minimal fix: amend D3 to distinguish fewer than four bytes (recoverable) from a present `recordLength` (validate 40…64 MiB before EOF recovery), require checked arithmetic or subtractive bounds checks, and add an overflow fixture.

3. **HIGH — header numeric shapes remain weaker than ADR-006.**

   ADR-006 requires a non-zero 32-bit `sourceId`, and `reference.sourceTimestampNs` / optional `precisionNs` are signed 64-bit values. [ADR-006:69](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:69), [ADR-006:76](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:76)  
   SPEC only requires a “non-zero JSON number” for `sourceId` and “canonical decimal strings” for the 64-bit fields. [SPEC-M10:299](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:299), [SPEC-M10:302](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:302)

   As written, it does not refuse `sourceId: 1.5`, an out-of-range integer, or a canonical-looking decimal outside `qint64`; storing either in the proposed typed facts would alter or fail to represent it.

   Minimal fix: require integral exact `1..UINT32_MAX` for `sourceId`, and canonical decimal values representable as `qint64` for both reference fields and `precisionNs`; add boundary/overflow fixtures.

4. **HIGH — the exposed timing UI has no specified refusal handling.**

   The player correctly returns a result, but the UI design only says the timing ladder is enabled; it never specifies consuming `SessionReplayTimingChange`, restoring the selector to `change.timing`, or reporting `change.reason`. [SPEC-M10:621](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:621), [SPEC-M10:693](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:693)

   A user selecting Step while Playing can therefore be left with a UI showing Step while the player remains in its previous timing policy. This violates the stated “no silent refusal” goal in the user-facing path.

   Minimal fix: specify the control handler’s result handling and add an offscreen UI test for the refused Step selection.

### Same-pass cleanup

- **MEDIUM — the output-control table misstates File Close and Quit.** It says Close/Quit have “no transport,” but `closeEvent()` calls `doc->closeSession()`, which calls `mSerial.close()`; Quit applies that to every top-level window. [SPEC-M10:690](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:690), [komport.cpp:953](/home/max/Development/misc/komport-qt6/komport/komport.cpp:953), [komport.cpp:1130](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1130)  
  Correct the call chains and explicitly state why teardown remains enabled, subject to the owner’s cross-window decision.

- **MEDIUM — `close()` while `Playing` is unspecified.** Its public contract says it detaches and returns Idle, but the transition table does not permit it from Playing. [SPEC-M10:442](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:442), [SPEC-M10:470](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:470)  
  Define it as cancelling the pending callback and transitioning to Idle, or make any refusal value-returned.

- **DOCUMENTATION — dynamic evidence is overstated.** `bytesWritten` is emitted only after a positive accepted write, so its absence proves no accepted TX observation, not that `writeRaw()` was never entered. [komportserial.cpp:401](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:401), [SPEC-M10:772](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:772)  
  Narrow that wording; the replay/reply source audit is the appropriate structural evidence.

No remaining items are editorial only.

I did not build, run `ctest`, run a pty, GUI/offscreen checks, or the proposed source audits. I verified statically the modified documents, prior ADRs/reviews, format/writer limits, action/slot wiring, window/profile construction, reply paths, the single write primitive, document close path, and test/CMake topology.