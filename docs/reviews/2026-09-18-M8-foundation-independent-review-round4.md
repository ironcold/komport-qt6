# Round 4 independent review — M8 foundation

**Verdict: not accepted.** P1, P3, P5, and P6 close their stated issues, but P2 remains a BLOCKER and the layered amendments retain a HIGH cross-domain timing contradiction.

## P1–P8 assessment

| Item | Assessment | Evidence |
|---|---|---|
| P1 | **Resolved** | The reference pair and runtime `lastEmittedSessionTimestampNs` are now distinct, initialized and updated in a definite order. The formula is implementable for every accepted transport observation: `raw = source - reference.source`, then `timestamp = max(lastEmitted, raw)`. The anchor is deterministically zero. [v2.2 P1](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.2.md:22) |
| P2 | **Not resolved** | The table contradicts its tests and does not define result/event ownership consistently; details below. |
| P3 | **Resolved** | It replaces ADR-003’s remaining claim that replay feeds the controller with the correct live-transport/replay-player split. [v2.2 P3](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.2.md:144) |
| P4 | **Partially resolved** | Activation IDs on all seven transport signals make old-activation filtering implementable and preserve the synchronous legacy error path. But the required delayed-old-ID test conflicts with the stated transport conformance rule that `closed()` is the final signal for an activation. |
| P5 | **Resolved** | It correctly restores ADR-006’s actual widths: `sourceId` and lengths are 32-bit; only specified 64-bit JSON quantities are decimal strings. [v2.2 P5](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.2.md:205) |
| P6 | **Resolved** | The per-domain origin is now clearly mandatory capture metadata, not a mapping between clock domains. The revised ADR-005/008 wording makes the ban apply to cross-domain mappings. [v2.2 P6](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.2.md:217) |
| P7 | **Not resolved** | It depends on P2’s contradictory test contract and P4’s ambiguous conformance-versus-robustness test. |
| P8 | **Partially resolved** | Correctly makes application of the notes an acceptance precondition, but says “two architecture documents” although the seven locations are in **three** documents: session/replay, multiport, and multi-executable. [v2.1 N9](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:294) |

## Required cross-checks

### P1 against P6

**Yes.** These are now consistent. P1’s `reference.sourceTimestampNs` / zero-valued session reference describes the origin used to create that domain’s own timeline. P6 expressly excludes only mappings *between* domains—offset, scale, uncertainty, synchronization, and alignment UI. No persisted cross-domain mapping is thereby authorized.

### P2 against real call sites and SPEC §3/§14

**The authorized exception is substantively sufficient, but P2’s operational text is not.**

The real paths are:

- Profile load explicitly closes, stages all six setters while closed, then unconditionally opens. [komport.cpp](/home/max/Development/misc/komport-qt6/komport/komport.cpp:625)
- Preferences stages setters on the current port and calls `open()` only if it is closed. [komport.cpp](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1250)

P2 correctly retains the real `open()` semantic: current `open()` always calls `close()` first. [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:64) Thus the table’s “already open” row correctly requires Close → Open → one result.

But P2-6 requires “none for a redundant `open()` on an open port,” directly contradicting both the table and current behavior. [v2.2 P2-2/P2-6](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.2.md:83) Also, P2-2 incorrectly says the conditional `if (!serial->isOpen()) serial->open()` exists at *two* application call sites; it exists only in preferences, while profile loading uses an unconditional `open()`.

The §14 replacement correctly names the compatibility expectations needed by the authorized §3 exception—unchanged settings vocabulary/persistence/dialog presentation, one final result per profile/dialog action, and preserved synchronous failure notification—but it cannot make the inconsistent table testable.

### P4 against C5 and `tst_profileerror.cpp`

**Yes.** P4 preserves the required synchronous legacy path. The current test depends on:

```text
errorOccurred → slotPortError → settingsFailed
```

during the failed `open()` inside profile loading. [tst_profileerror.cpp](/home/max/Development/misc/komport-qt6/tests/tst_profileerror.cpp:64) The direct connections and legacy emission are present today. [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:36) P4 explicitly leaves `settingsFailed()` untouched and confines duplicate suppression to the new event path.

### Seven signals, one-observation/one-event, FIFO

**The ID addition itself is compatible.** Each of the seven signals can carry one complete, correlated observation; an accepted current-activation observation can produce exactly one corresponding event, and FIFO/non-reentrancy remains viable.

Two qualifications remain:

1. A configuration transaction can apparently yield `TransportConfigChanged` and `Error`, but P2 never states whether those are two transport observations or two events derived from one “configuration result.” That prevents verification of the one-observation/one-event invariant for configuration failures.

2. P4 says a conforming transport emits `closed()` last for an activation, but then mandates a test where an old-activation data signal arrives after a new `opened()`. [v2.2 P4](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.2.md:175) Either label that test as deliberate handling of a non-conforming/queued stale delivery, or revise the conformance wording to distinguish emission from delayed controller delivery.

## Remaining findings

| Category | Finding | Minimal fix |
|---|---|---|
| **BLOCKER** | **P2 is internally contradictory.** The table requires one result for `open()` while already open, but P2-6 requires none. The legacy-setter row says hardware is applied once for all six setters, while P2-3 says RX-queue/flush-only changes leave hardware unchanged. “Configuration result” is not mapped definitively to `configurationChanged`, `transportError`, or another observable object, especially for failed-with-no-change cases. | Make the table normative and derive every test from it. Remove the “none for redundant open” test; change the hardware column to distinguish transaction execution from hardware application; explicitly define which transport observation/event carries each full, partial, and failed result, including failed-with-no-change. Add no-op behavior where relevant. |
| **HIGH** | **Cross-domain ordering remains contradictory.** P1 makes `timestampNs` non-decreasing *per domain*, and N2 says different domains may not be ordered directly. But retained N7 requires the controller to enforce non-decreasing “session time” for the whole stream. [v2.1 N7](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v2.1.md:266) | Amend N7/B2-2: require non-decreasing time within each clock domain; retain FIFO/sequence as the global ordering rule until a versioned cross-domain mapping exists. |
| **TEST GAP** | **P4-5’s stale-ID test is inconsistent with P4-2’s conformance rule.** It is unclear whether the test double is intentionally non-conforming or whether queued delivery is permitted after `closed()`. | State which it is, and require one explicit test for that condition and one conforming transport-order test. |
| **DOCUMENTATION** | **Replay interface authority is inconsistent.** B3-5 says its exact shape is deferred to M10; B7-1 points to ADR-003; P3 calls it an ADR-007 interface. The safety boundary agrees, but the normative owner does not. | Name M10 as the future interface specification everywhere; ADR-003/007 should state only its live/replay and safety constraints. |
| **DOCUMENTATION** | **P8 says two architecture documents, but N9 identifies three.** | Correct P8 and apply all seven notes across the three named documents. |

## Earlier-text contradictions still present

P1/P6 are consistent, and P5 supersedes the incorrect B2-3 width statement. However, the following layered contradictions remain:

- P2-2’s already-open `open()` row versus P2-6’s “none for a redundant open.”
- P2-2’s “hardware applied once” for every legacy setter versus P2-3’s local-buffer-only result with unchanged hardware.
- P4-2’s “`closed()` is the last signal” versus P4-5’s post-new-open old-ID signal test.
- B3-5, B7-1, and P3 disagree whether ADR-003, ADR-007, or M10 specifies the replay-player interface.

## Final acceptance decision

**No.** Even assuming v2, v2.1, and v2.2 are applied to ADR-002…ADR-009, SPEC-M8, and all seven supersession-note locations, the foundation must not be marked Accepted until the BLOCKER and HIGH items above are fixed.

The documented follow-ups remain outside M8:

- M9 streaming writer/persistence implementation.
- M10’s exact passive replay-player interface.
- Cross-domain synchronization/mapping estimation and uncertainty UI.
- Separately reviewed legacy RX-buffer clearing behavior.
- Actual Qt 6.3 CI/release-gate execution.

## What I could not verify

- No M8 implementation, controller, transport interface, result seam, or new tests exist yet, so proposed behavior could not be executed.
- I did not build or run tests because builds create artifacts in this read-only review environment.
- I verified the synchronous error-path dependency from source and test text, but not Qt runtime timing on this host.
- I could not verify real serial partial-write behavior, electrical delivery, PTY chunk boundaries, or Qt 6.3 compatibility.