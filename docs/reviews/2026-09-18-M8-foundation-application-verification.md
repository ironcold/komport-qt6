# M8 application-verification review

## Verdict

**Approved.** ADR-002 through ADR-009 and SPEC-M8 may now have their status lines changed to `Status: Accepted`.

I found no remaining application-fidelity defect, retained contradiction, or factual/technical error that blocks acceptance.

## 1. Application fidelity

All intended items are present in the applied documents and retain their intended meaning:

- A2-1…A2-4: sequence continuity, validation split, 64-bit JSON encoding, empty `Data` metadata: [ADR-002](</home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-002-session-event-v1.md:40>).
- A3-1…A3-9: source clock, single write primitive, accepted-count API, live/replay separation, capture boundary, lifetime rule, activation IDs, failed-open exception, and synchronous open: [ADR-003](</home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-003-transport-abstraction-v1.md:25>).
- A5-1/A5-2: executable per-domain timestamp mapping and capture-origin versus cross-domain-mapping distinction: [ADR-005](</home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-005-session-timestamp-semantics.md:21>).
- A6-1…A6-3: mandatory source/domain reference pair, fixed widths/44-byte prefix, and empty-metadata encoding: [ADR-006](</home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:44>).
- A7-1 and A8-1/A8-2: M10 owns the replay-player interface; per-domain origin metadata is not cross-domain alignment: [ADR-007](</home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-007-replay-safety-model.md:13>), [ADR-008](</home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-008-multi-source-session-time-alignment.md:24>).
- S-1…S-11 are applied, including the §3 exceptions, complete configuration request, normative operation/observable tables, `startBits`, ownership/lifetime, lifecycle filtering, FIFO delivery, tests, §14 criteria, and the test-only QtCore target exception: [SPEC-M8 §3](</home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:33>), [§6.2](</home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:109>), [§13–14](</home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:366>).
- All seven Part C notes are present: two in the replay architecture, four in multiport, and one in multi-executable. [Occurrences](</home/max/Development/misc/komport-qt6/docs/komport-session-replay-simulation-architecture.md:230>), [multiport](</home/max/Development/misc/komport-qt6/docs/komport-multiport-sniffer-time-alignment.md:125>), [multi-executable](</home/max/Development/misc/komport-qt6/docs/komport-multi-executable-product-architecture.md:574>).

The two §6.2 `KomportDoc` paragraphs are complementary, not duplicated: the first establishes ownership and exposure boundaries; the second specifies destruction order and rejects unsafe QObject-child reliance. [SPEC-M8](</home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:210>).

The round-5 corrections are correctly integrated:

- Failed opens are the explicit sole `transportError` exception to the no-pre-open non-error rule.
- §3 explicitly permits the test-only Core compile/object target required by §14.
- `startBits` is in the request, metadata, observable mapping, and test plan, while explicitly remaining non-hardware compatibility state.

No intended item was missing, duplicated improperly, mangled, or placed in a semantically wrong location.

## 2. Residual contradiction check

No retained contradiction remains.

- ADR-002’s timestamp sentence now correctly delegates to ADR-005’s session-origin mapping; its structural and stream validation rules agree with ADR-005/006.
- ADR-003’s activation identity, delivery distinction, live/replay boundary, write semantics, capture boundary, and lifetime rule agree with SPEC §§7/10 and ADR-007.
- SPEC §3 authorizes exactly the orchestration changes later required by §§6.2 and 14.
- SPEC §8 uses per-domain non-decreasing time and sequence/arrival order globally, matching ADR-005.
- The three supersession-note documents no longer present their old `QVariantMap`, source-zero, or magic examples as current v1 rules.
- ADR-008’s deferral applies to cross-domain mappings; ADR-006’s mandatory per-domain reference pair is correctly described as capture metadata.

“Does not consume an activation” in SPEC §7 is coherent when read as “does not create a successful/live activation”; the failed attempt still consumes an `activationId`, as ADR-003 expressly requires.

## 3. Factual and technical checks

The applied code claims are accurate:

- The current duplicate settings application is real: `open()` applies settings then emits the self-connected signal. [serial implementation](</home/max/Development/misc/komport-qt6/komport/komportserial.cpp:64>).
- `startBits` is stored but intentionally not applied to `QSerialPort`. [header](</home/max/Development/misc/komport-qt6/komport/komportserial.h:60>), [implementation](</home/max/Development/misc/komport-qt6/komport/komportserial.cpp:158>).
- Profile loading closes, stages six settings, and opens unconditionally; preferences stages them and opens only if closed. [profile path](</home/max/Development/misc/komport-qt6/komport/komport.cpp:625>), [preferences path](</home/max/Development/misc/komport-qt6/komport/komport.cpp:1250>).
- RX buffering remains lossy/delayed and survives `close()`; byte-wise upload uses `putChar()`. [serial RX path](</home/max/Development/misc/komport-qt6/komport/komportserial.cpp:237>), [transfer](</home/max/Development/misc/komport-qt6/komport/komporttransfer.cpp:53>).
- The existing tests link `komport_core`, which publicly links Widgets, and `.github/` has no workflow. The dedicated Core-only target is therefore a valid new acceptance requirement. [tests CMake](</home/max/Development/misc/komport-qt6/tests/CMakeLists.txt:10>), [top-level CMake](</home/max/Development/misc/komport-qt6/CMakeLists.txt:89>).

No obligation is intrinsically impossible to implement.

## 4. Conditions and follow-ups outside M8

Acceptance of the ADR/spec set does not mean M8 implementation is complete. The remaining explicitly deferred work is:

- M9 streaming writer and persistence.
- M10’s exact replay-player interface.
- Cross-domain synchronization, alignment mappings, and uncertainty UI.
- Any behavior change to the retained legacy RX buffer.
- A real Qt 6.3 CI job or release-gate build.

## 5. Open findings and §13 taxonomy

**None.** There are no open findings requiring a Section 13 taxonomy category.

## 6. Not verified

I did not execute a build, `ctest`, PTY tests, or a Qt 6.3 build. Those are implementation/acceptance activities not yet possible for the unimplemented M8 foundation, and the documents correctly retain them as future acceptance criteria.