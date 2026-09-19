## Verdict

**Rejected for round 2.** Do not change either `Status: Proposed` line yet: [ADR-011:3](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:3), [SPEC-M10:3](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:3).

The six round-1 issues are substantively addressed, but unresolved blocking contradictions remain in the specification.

## Round-1 coverage

| Finding | Status | Evidence checked |
|---|---|---|
| 1. Replayed rendering could transmit replies | **Partially resolved** | ADR-011 defines a replay entry point and reply choke point ([ADR-011:524](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:524)); SPEC names suppression lifetime and query tests ([SPEC-M10:610](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:610), [SPEC-M10:957](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:957)). Current code confirms the risk: live query handlers call `putStr` ([komportemulation.cpp:1266](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1266), [komportemulation.cpp:1290](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1290)). However, the design diagram still routes replay to the live `slotReceivedChar()` ([SPEC-M10:154](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:154)). |
| 2. M9-local writer-profile whitelist | **Resolved** | ADR-011 now makes ADR-006 sole normative format text and accepts non-local profile values ([ADR-011:111](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:111), [ADR-011:140](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:140)); SPEC has explicit optional-content and alternate-profile tests ([SPEC-M10:889](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:889)). New ADR-006 conformance gaps are listed below. |
| 3. Incomplete offline control coverage | **Partially resolved** | Paste, macros, recording, logging and connection controls are now enumerated ([SPEC-M10:664](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:664)). The real paste and macro paths do transmit ([komport.cpp:1178](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1178), [komport.cpp:1427](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1427)). But `New Window` is omitted and can open a new live port. |
| 4. Selecting Step while Playing | **Partially resolved** | The state rule itself is coherent: selection is refused and leaves timer/state intact ([ADR-011:357](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:357), [SPEC-M10:494](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:494)). But the declared `void setTiming(...)` API cannot return that required refusal. |
| 5. False mechanical proof / nonexistent transport seam | **Partially resolved** | The documents correctly limit the QtCore compile claim ([ADR-011:543](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:543)), and correctly use a real pty because `KomportDoc` owns `KomportSerial` by value ([SPEC-M10:732](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:732), [komportdoc.h:132](/home/max/Development/misc/komport-qt6/komport/komportdoc.h:132)). The proposed source audit does not cover the replay adapter/emulation that it drives. |
| 6. Unapproved multi-clock-domain refusal | **Resolved** | It is explicitly removed; M10 adds no clock-domain-count rule ([ADR-011:155](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:155), [SPEC-M10:893](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:893)). |

Nothing from the six findings was silently dropped. The multi-clock-domain refusal/test was explicitly replaced, and the prior output-target test was explicitly replaced by the audit-plus-pty approach.

## Blocking findings

1. **HIGH — `setTiming()` is specified to return a refusal but is declared `void`.**

   Both contracts declare `void setTiming(...)` ([ADR-011:291](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:291), [SPEC-M10:432](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:432)). Yet the specification requires every refusal to be returned as a value ([SPEC-M10:492](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:492)), and the planned Step test says this call “returns a refusal” ([SPEC-M10:927](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:927)).

   Minimal fix: introduce a timing-change result type, e.g. `SessionReplayTimingChange { bool ok; QString reason; }`, and change both ADR and SPEC signatures to return it. Update the state/operation text and test names accordingly.

2. **HIGH — the offline-control table misses `File → New Window`, a real connection bypass.**

   The table labels “File → New” as harmless ([SPEC-M10:671](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:671)), but the actual menu action is `New Window` ([komport.cpp:130](/home/max/Development/misc/komport-qt6/komport/komport.cpp:130), [komport.cpp:235](/home/max/Development/misc/komport-qt6/komport/komport.cpp:235)). Its slot constructs another `KomportApp` ([komport.cpp:1014](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1014)); construction calls `initProfiles()` ([komport.cpp:94](/home/max/Development/misc/komport-qt6/komport/komport.cpp:94)), which loads a profile ([komport.cpp:597](/home/max/Development/misc/komport-qt6/komport/komport.cpp:597)) and opens the serial port ([komport.cpp:670](/home/max/Development/misc/komport-qt6/komport/komport.cpp:670)).

   This contradicts D11’s requirement to disable every connection control and guard its entry point ([ADR-011:449](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:449)).

   Minimal fix: add `New Window` / `slotFileNewWindow()` as a disabled-and-guarded row, or explicitly revise the offline-state boundary to permit a separate live window. The former fits the existing D11/D14 claim. Add a direct-slot test proving no window and no port activation occur.

3. **HIGH — the loader still accepts malformed ADR-006 header content.**

   ADR-006 requires each clock-domain `id` to be unique and specifies the shape of an optional `wallClockCorrelation` when present ([ADR-006:73](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:73)). SPEC-M10 validates only that a domain has an `id`, `kind`, and `reference` ([SPEC-M10:296](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:296)); it neither requires unique IDs nor validates `wallClockCorrelation`. Instead it accepts that member without qualification ([SPEC-M10:298](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:298)).

   Minimal fix: validate every declared clock-domain entry, including unique IDs; if `wallClockCorrelation` exists, validate its UTC wall-clock string and canonical decimal `precisionNs`, then discard it from M10’s typed facts. Add malformed-optional and duplicate-domain-ID fixtures.

4. **HIGH — record-length validation permits recovery before enforcing ADR-006’s record limit.**

   ADR-006 requires every length to be validated and caps a record at 64 MiB ([ADR-006:57](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:57)). SPEC-M10 mandates table order ([SPEC-M10:274](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:274)): it classifies `4 + recordLength` beyond EOF as truncation/recovery ([SPEC-M10:300](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:300)) before its size check, which only runs after metadata/payload fields are available ([SPEC-M10:302](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:302)). A truncated final record declaring an impossible length can therefore be recovered instead of refused. A `recordLength < 40` is also not explicitly rejected before parsing fields outside its declared record.

   Minimal fix: immediately after reading the leading `recordLength`, reject a length below the 40-byte post-length prefix or above the 64-MiB record-body limit, before the EOF/recovery check; use checked arithmetic. Add malicious truncated-record fixtures for both cases.

5. **DOCUMENTATION — blocking: the component diagram contradicts the reply-free rendering design.**

   The diagram routes delivered events to `KomportView::slotReceivedChar()` ([SPEC-M10:154](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:154)), while the later normative design requires `slotReplayReceivedChar()` ([SPEC-M10:630](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:630)). Today the former only performs scroll bookkeeping ([komportview.cpp:497](/home/max/Development/misc/komport-qt6/komport/komportview.cpp:497)); it does not render through the emulation.

   Minimal fix: change the diagram to `KomportView::slotReplayReceivedChar()` and label it reply-free.

## Same-pass cleanup

- **MEDIUM — “exactly one M10-local loader restriction” is not literally true.** Check 1 independently rejects any non-regular readable path ([SPEC-M10:282](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:282)), while D2 says exactly one M10-local restriction ([ADR-011:160](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:160)). Either remove the regular-file refusal, or amend D2 to say “one format restriction” and justify the I/O precondition.

- **TEST GAP — the claimed audit scope does not match D14.** D14 calls for an audit of offline units *and objects the replay path drives* ([ADR-011:548](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:548)). SPEC audits only the four reader/player sources ([SPEC-M10:726](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:726)), not the replay adapter or emulation. A pty can establish “peer received no bytes,” but without a seam it cannot establish that no in-process write method was entered ([SPEC-M10:732](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:732)).

  Minimal fix: add a scoped static audit/test for the replay entry functions and reply handlers: replay functions contain no direct write, and all terminal-generated reply sites route through `sendTerminalReply()`. Limit the pty assertions to observable no-peer-byte behavior.

## Explicit cross-check conclusions

- The proposed reply suppression is the right design, and its “set on entry, clear on every exit” lifetime is stated ([SPEC-M10:621](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:621)). It needs the diagram and audit corrections above.
- The CMake-injected source-root mechanism is reasonable and repeatable; the current QtCore contract target is correctly limited because `ITransport` is QtCore-only ([itransport.h:25](/home/max/Development/misc/komport-qt6/komport/itransport.h:25)).
- The new empty-`sources[]` refusal is justified. M10’s value type has one required source descriptor, and “exactly one source” is a coherent single-source restriction; it does not pull M14 work forward.
- Replayed `BEL` may continue to call `QApplication::beep()` ([komportemulation.cpp:1505](/home/max/Development/misc/komport-qt6/komport/komportemulation.cpp:1505)). It is a local UI side effect, not a transmit path, and M10 need not suppress it.

## Not verified here

Per the review constraint, I did not build or run tests, pty tests, GUI/offscreen tests, or the proposed source audit. I verified statically: both modified documents, ADR-002/004/005/006/007/008/009/010, the round-1 record, current CMake topology, and the relevant application, document, view, emulation, transport, recorder, and pty-test call paths.