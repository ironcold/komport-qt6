# Round 5 gate review — verdict: **Not accepted**

No. Applying v3 Part A/B/C verbatim still leaves two **BLOCKERs** and one **HIGH** finding. ADR-002…ADR-009 and SPEC-M8 must remain unaccepted.

## Open findings

| Category | Finding | Evidence | Minimal fix |
|---|---|---|---|
| **BLOCKER** | Failed-open handling contradicts the activation conformance rule. A3-9 says a transport “never emits an observation of an activation that never opened successfully,” but the same item requires every failed `open()` attempt to report its failure once. S-5 then expressly consumes an `Error` for an activation never preceded by `opened`. | [A3-9](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v3-consolidated.md:217), [S-5](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v3-consolidated.md:503) | Change the prohibition to: “never emits a **non-error** observation for an activation that never opened successfully”; explicitly retain the one failed-open `transportError` exception. |
| **BLOCKER** | SPEC §3 still forbids a “CMake target split,” while S-10 requires a new dedicated QtCore-only compile/object target. Both cannot be true as written. | [Retained SPEC §3](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:37), [S-10](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v3-consolidated.md:648) | Amend the retained non-goal to forbid a production/shared-library target split, while explicitly allowing the dedicated test-only/public-header compile target. |
| **HIGH** | The new “complete” configuration request omits the existing `StartBits` setting, despite S-1 promising no change to the set or meaning of serial settings and both UI call sites migrating to that request. `setFraming()` currently stores start bits for configuration/UI compatibility, even though QSerialPort cannot apply them. | [S-1](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v3-consolidated.md:373), [S-3](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-foundation-amendments-v3-consolidated.md:415), [current compatibility contract](/home/max/Development/misc/komport-qt6/komport/komportserial.h:60), [current storage](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:289) | Include `startBits` in the request and result metadata as a stored compatibility/UI field, explicitly excluded from hardware application; add its local/compatibility changed-group and test coverage. |

These are all still-open findings and use the section 13 taxonomy shown above.

## Required contradiction checks

- **A5-1 vs A2-2:** consistent. Both make `timestampNs` non-decreasing per clock domain, retain raw source time, and use sequence/arrival order rather than cross-domain timestamp comparison. The round-4 global-ordering contradiction is removed.

- **S-3 table, S-3 mapping, §3/§14, and current call sites:** except for the missing `StartBits` field above, the transaction contract is coherent. There is no longer a “none for a redundant open” rule: an already-open `open()` is Close → Open → one transaction result. The local-buffer-only outcome correctly produces a configuration event without claiming hardware application. The call-site descriptions are factually correct: profile loading closes, stages settings, then opens unconditionally; preferences stages settings and opens only if closed. [Profile path](/home/max/Development/misc/komport-qt6/komport/komport.cpp:625), [preferences path](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1250), [current `open()` behavior](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:64).

- **A3-9 vs S-9 stale-activation tests:** resolved. S-9 labels the stale-ID test as a deliberately non-conforming or queued-late delivery, and separately requires a conforming-order test. This correctly distinguishes signal emission from delivery.

- **Replay authority:** resolved. A3-5, A3-6, and A7-1 consistently assign the replay-player interface’s exact shape to M10; ADR-003/007 only state boundary and safety constraints. The retained SPEC wording about a later downstream consumer interface does not name a competing owner.

- **A3-3 vs A3-2:** consistent. `writeBytes()` reports an accepted count; legacy boolean methods retain their signature and are true only on full acceptance. This matches current `putStr()` behavior, which reports failure on a partial write after emitting legacy characters for its accepted prefix. [Current implementation](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:190)

- **Part C:** correct. It identifies exactly seven locations across exactly three documents: session/replay (2), multiport (4), and multi-executable (1). All stated line references match current text; the magic is correctly located in session/replay §7.2 at line 330, though v3 sensibly cites the section rather than a stale line number.

## Regression check

“Resolved” below means resolved by v3’s proposed text, contingent on applying it. “Open” refers to the findings above.

| Review items | Status in v3 |
|---|---|
| Round-1 F1 clock origin/domain | Resolved |
| F2 all TX entry points and API-boundary chunks | Resolved |
| F3 sequence across activations | Resolved |
| F4 accepted partial-write prefix | Resolved |
| F5 byte-wise transfer volume / 44-byte record cost | Resolved |
| F6 effective configuration transaction | Resolved in its original scope; **HIGH open** for omitted `StartBits` |
| F7 passive replay boundary | Resolved |
| F8 FIFO/non-reentrant delivery | Resolved |
| F9 failed-open/error contract | **BLOCKER open**: failed-open exception contradicts A3-9’s blanket rule |
| F10 late observations / activation scoping | Resolved, subject to the same failed-open wording fix |
| F11 legacy RX path versus event capture | Resolved |
| F12 PTY chunk-count limitation | Resolved |
| F13 QtCore-only/Qt 6.3 verification | **BLOCKER open**: required compile target contradicts retained §3 |
| F14 documentation drift | Resolved, conditional on Part C application |
| F15 source-ID wording | Resolved |
| F16 no production consumer | Resolved |
| Round-1 X1 clock-domain schema | Resolved |
| X2 64-bit JSON precision | Resolved |
| X3 structural versus stream validation | Resolved |
| X4 destruction ordering | Resolved |
| X5 duplicate configuration application | Resolved |
| X6 capture-point limitation | Resolved |
| X7 RX buffer across activations | Resolved |
| X8 deterministic seams | Resolved |
| X9 compatibility exception | Resolved |
| Round-2 widths, clock schema, transaction boundary, replay, timestamp normalization, activation identity, validator split, derived framing, documentation, test seams | Resolved except the two open items above and the `StartBits` gap |
| Round-3 N1–N10 | N1/N2/N4/N5/N7/N8/N9 resolved; N3 and N10 are resolved except for `StartBits`; N6 is resolved except for the failed-open conformance contradiction |
| Round-4 P1 formula, P2 transaction table, P3 replay, P4 stale delivery, P5 widths, P6 capture-vs-alignment, P7 tests, P8 three-document count | Resolved, except P2 inherits the `StartBits` omission; no regression of the corrected timestamp formula, field widths, alignment-vs-capture distinction, “no result for redundant open,” or replay ownership |

The CMake conflict appears to be a retained, previously missed inconsistency rather than a regression in the corrected QtCore-target requirement itself.

## Correct application plan after the three fixes

Content changes belong in:

- ADR-002, ADR-003, ADR-005, ADR-006, ADR-007, and ADR-008.
- SPEC-M8.
- The three Part C architecture documents.
- Additionally, the SPEC §3 non-goal must be corrected for the test-only compile target, and S-3 must include `StartBits`.

ADR-004 and ADR-009 have no substantive v3 amendment, but their statuses must change with the ADR set once the gate passes.

Only after the fixed package is accepted:

- Each ADR-002 through ADR-009 status line becomes `Status: Accepted`.
- SPEC-M8’s status line should become `Status: Accepted` (the current “implementation blocked pending ADR review” wording must be removed).

No production code, test, CMake, or workflow file should change in this review-only application step.

Documented follow-ups outside M8 remain appropriate: M9 streaming writer/persistence, M10 replay-player interface, cross-domain synchronization and uncertainty UI, separately reviewed RX-buffer behavior, and actual Qt 6.3 CI/release-gate execution.

## Not verified

- No M8 `SessionEvent`, `ITransport`, `SessionController`, test double, or new tests exist yet, so proposed runtime behavior was not executable.
- I did not build or run tests; this read-only review environment must not create build artifacts.
- Real partial-write behavior, physical delivery, PTY read chunk boundaries, and actual Qt 6.3 compatibility remain unverified.