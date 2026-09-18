# M8 implementation self-review (2026-09-18)

Self-review of the M8 implementation against the accepted
`docs/specs/SPEC-M8-session-transport-foundation.md` (steps 2-6 of its §17), as
required by §17 item 7 and by the binding workflow
(`docs/komport-engineering-governance-spec-review-workflow.md`). It precedes the
independent final review of the same scope; the independent review is the gate,
this document is the claim it has to check.

## 1. What was implemented

| Step (§17) | Commit | Content |
| --- | --- | --- |
| 2 | `8e3cdbd` | Core value/type contracts: `SessionEvent` + validator, `ITransport`, `TransportConfiguration`/`ConfigurationResult`, the test-only `Qt6::Core` compile target |
| 3 | `634b396` | `KomportSerial` as the local transport: activation identity, one observed write primitive, RX/TX observations, the single configuration transaction, injectable seams |
| 4 | `dc8757e` | `SessionController` (state model, activation filter, FIFO delivery, ADR-005 mapping) and the document ownership/destruction order |
| 5 | `90928ee` | Both application call sites migrated to the configuration entry point |
| 6 | (this slice) | Test coverage completed against §13 and the full suite run |

Review records for each step: `docs/reviews/2026-09-18-M8-implementation-review-step2-3*.md`,
`…-step4*.md`, `…-step5*.md`.

## 2. §14 acceptance criteria

| Criterion (§14) | Evidence | Status |
| --- | --- | --- |
| Core types and the transport contract compile against the locally installed Qt 6 and are documented against the declared floor of Qt 6.3 | Build of 2026-09-18 with the local Qt **6.11.1**; `CMakeLists.txt` declares `find_package(Qt6 6.3 …)`; the floor is named in the contract headers | met locally |
| "Compiles with Qt 6.3" counts only once a Qt 6.3 build is actually run (CI job or release gate; the repository has no CI workflow) | **not run**: no Qt 6.3 toolchain and no CI in this repository | **not verifiable here** |
| A dedicated compile/object target with only the public session and transport headers, linking `Qt6::Core` (not the Widgets-linking test targets), builds | `tests/CMakeLists.txt`: target `komport_session_contract_check` compiles `session_contract_compile.cpp`, `itransport.cpp` and `sessioncontroller.cpp` against `Qt6::Core` only; no `komport_core`, no `Qt6::Test` | met |
| Current serial I/O exposes binary chunk observations without changing legacy public calls/signals | `KomportSerial` implements `ITransport` additively; `putChar`/`putStr`/`setFraming`/`setFlowControl`/`setBaudRate`/`setDeviceName`/`setRxQueue`/`setFlushRate`, `receivedChar`/`sentChar`/`settingsChanged`/`settingsFailed` and the legacy RX buffer are unchanged; the legacy failure notification is emitted synchronously by `open()` (see §4.1) and `tst_serial`/`tst_profileerror` pass unchanged | met |
| A document-owned controller emits ordered, byte-exact live events | `tests/tst_sessiondocument.cpp`: real pty, all 256 byte values arrive byte-exact as ordered events; document teardown emits nothing | met |
| Every M8 event carries source ID 1, including a failed-open `Error(kind: "open")`; zero is reserved and unused; every event preserves source-local timing separately from the derived session time | `kSourceId = 1` in `sessioncontroller.cpp`; `isValidSessionEvent()` rejects `sourceId == 0`; tests assert ID 1 on ordinary and on failed-open events and assert `sourceTimestampNs` is preserved next to `timestampNs` | met |
| New public session/transport value contracts have no `QWidget` or executable-specific type dependency | the Core-only object target above; `sessioncontroller.h` includes Qt Core types only | met |
| Open, close, error and effective serial configuration are observable as non-data events | `ITransport`'s seven observations → `SessionEvent` types `TransportOpened`, `TransportClosed`, `TransportConfigChanged`, `LineStateChanged`, `Error`; the operation table of §6.2 is tested per row in `tst_transport` and `tst_sessioncontroller` | met |
| Compatibility expectations of §3: settings vocabulary, persistence and dialog presentation unchanged; one user action produces exactly one configuration transaction and its result(s); the synchronous legacy `settingsFailed()` on a failed open is preserved | `strDevice`/`strBaudRate`/`strStartBits`/… and the `QSettings` keys are untouched; both call sites build one request and call the entry point once; `tst_transport::openAndConfigureProducesOneOpenedAndOneTransaction()` asserts exactly one hardware application per open; the failed-open notification is emitted synchronously by `open()` and asserted in `failedOpenKeepsTheSynchronousLegacyNotification()`; `tst_profileerror` passes | met |
| NUL/all-byte pty tests pass | `tests/tst_transport.cpp` and `tests/tst_sessiondocument.cpp`, all 256 values in both directions, every TX and RX event asserted non-empty, run (not skipped) in this sandbox | met |
| Existing full `ctest` suite passes | 13/13 targets, 9 of them pre-existing | met |
| No file format, replay UI, decoder or active transmission is introduced | no new file format, no replay/decoder code, no automatic open; the controller is passive and the document owns it | met |

## 3. §13 testing strategy

| Requirement | Test | Status |
| --- | --- | --- |
| Origin capture on the first accepted event per domain, `timestampNs == 0` for that anchor, identity mapping | `tst_sessioncontroller::adr005AnchorAndIdentityMapping`, `failedOpenErrorIsTheDomainAnchor` | met |
| Non-negativity and non-decreasing session time per domain, exactly one anomaly diagnostic per violation | `adr005NonDecreasingTimeAndOneAnomalyDiagnosticPerViolation` (counts the anomaly events) | met |
| Source-time preservation | `everyEmittedEventSatisfiesTheStructuralContract`, anchor tests | met |
| Structural validation of every event | `everyEmittedEventSatisfiesTheStructuralContract` runs `isValidSessionEvent()` over the whole stream; `tst_sessioncontract` covers the validator's rejection cases including undeclared enumerators | met |
| Source ID 1 on every event including `Error(kind: "open")` | the controller tests and `tst_sessiondocument` assert it per event | met |
| Controller double emitting scripted observations (empty reads, partial accepts, configuration observations, scripted errors) | `ScriptedTransport` in `tst_sessioncontroller` (all four kinds) and `ScriptedSerial` in `tst_transport` (write/read/apply seams) | met |
| One observation → one event; sequence continuity across activations; the documented no-op produces nothing | `oneObservationProducesExactlyOneEvent`, `sequenceContinuesAcrossActivationsWithoutReset`, `emptyReadsAndTheNoOpProduceNothing` | met |
| One test per operation-table row including read-back values in the metadata and `changedGroups` | `tst_transport`: no-op, local-buffering-only, compatibility-only, partial, failed, endpoint change, open-and-configure, closed-port store; `legacySettersFollowTheOperationTableForEveryField()` covers the legacy rows of `setDeviceName`/`setRxQueue`/`setFlushRate` on a closed and on a live port; `tst_sessioncontroller::operationTableRowsCrossTheControllerUnchanged` checks the metadata pass-through | met |
| `startBits`-only change: one result with `changedGroups: ["compatibility"]`, unchanged hardware and buffering sections, no hardware application, unchanged UI/config behaviour of that field | `bufferingAndCompatibilityChangesReportTheirOwnGroup` compares the `requested`, `effective` and `localBuffering` metadata sections against the preceding result, asserts the value survives in `requestedConfiguration()` and asserts that no hardware application happened; the profile path is covered end to end by `tst_profileerror::startBitsSurvivesTheProfileRoundTrip()` through a real `KomportApp`: a profile with `StartBits=2` is loaded, the value reaches the staged configuration request, and saving the profile writes the same value back | met; the modal dialog's own combo box remains inspection-only (§4.4) |
| Observable mapping for `full`, `partial`, `failed`-with-change, `failed`-without-change in the stated order | `partialAndFailedAppliesReportTheirApplyStatus` (`partial` with `apply_partial`, and `failed`-without-change with the failed status, empty `changedGroups` and one Error); `portErrorDuringApplyStaysWithinTheObservationBudget` generates `failed`-with-change in the real transport (a port error during the apply, status `failed`, changed effective state, emitted as `partial`: `configurationChanged` then `Error`); the controller test asserts the same rows as events | met |
| FIFO order and two-subscriber non-reentrancy (RX then TX for both subscribers) | `fifoDeliveryIsOrderedAndNeverReentrant` (two collectors, a reply written from inside delivery, delivery depth 1) | met |
| Activation filtering: an old id delivered after a new `opened` is dropped with exactly one diagnostic and no event; a second test models conforming order and asserts one event per observation | `activationFilterDropsForeignObservationsWithOneDiagnosticEach`, `lateClosedAndRepeatedOpenedAreFiltered`, `staleAndZeroActivationIdsAreRejectedByTheWatermark`, `oneObservationProducesExactlyOneEvent` | met |
| A failed `open()` whose error is reported twice by a faulty double yields exactly one `Error(kind: "open")` | `failedOpenIsReportedOnceAndKeepsTheSessionIdle` (the double scripts the duplicate) | met |
| `KomportSerial`: accepted-prefix behaviour and one transaction per operation via an injectable seam; the existing serial suite unchanged, including the synchronous failed-open path `tst_profileerror` depends on | `writeToPort`/`readFromPort`/`applyHardwareSettings` seams; `txObservationCoversTheAcceptedPrefixAndReportsTheRefusal`, `zeroAcceptedWriteReportsOneErrorAndNoDataEvent`, `unusableBaudRateIsReportedAndNotStored`; `tst_serial` and `tst_profileerror` green | met |
| PTY end-to-end binary preservation, no chunk-count claims, no dependency on the legacy flush timer | `ptyCarriesEveryByteValueInBothDirections` concatenates TX and RX payloads, asserts every TX and RX event is non-empty and carries a non-negative source time, and does not assert chunk counts | met |
| Volume/pathology: 64 KiB byte-wise through `putChar()` yields one TX event per accepted write with correct payload bytes; event count, byte count and ordering; no in-memory event vector in the consumer | `byteWiseSixtyFourKiBUploadKeepsOneObservationPerAcceptedWrite` (counter lambdas, not a spy vector) | met |
| Regression: existing tests still pass; hex monitor and text logger keep their character-signal behaviour | 13/13 targets green; the character-signal behaviour is asserted directly - `sentChar` per accepted byte (`sentChars == totalBytes` in the 64 KiB test, one per byte for `putChar`/`putStr`/`writeBytes`) and `receivedChar` still fed by the legacy buffer path even without a live activation (`observationsRequireALiveActivation`) | met at signal level; §4.4 explains why the widget consumers themselves are not instantiated |

## 4. Deliberate deviations, limits and open items

These are stated so the independent review can judge them rather than discover
them:

1. **A failed `open()` emits the legacy `settingsFailed()` synchronously in
   `open()` itself**, so this requirement of §7/§14 no longer depends on when
   `QSerialPort` delivers `errorOccurred()`. Pre-M8 the notification came *only*
   from that signal (see `06e36aa:komport/komportserial.cpp`), which is
   platform-dependent - so the review's premise that a synchronous guarantee
   existed and was lost is not correct, but its substance is: the application reads
   its `mSerialErrorPending` flag directly after the call returned, and an
   incidental guarantee is not a contract. `slotPortError()` now skips its own
   emission for an attempt the transport already reported, so exactly one
   notification results on every platform, and
   `tst_transport::failedOpenKeepsTheSynchronousLegacyNotification()` pins it by
   asserting the count at the moment `open()` returns.
2. **The failure classification was refined so that the required row is
   reachable.** `partial` now means "the port refused individual settings"; a port
   error raised while the settings were applied means the *attempt* failed, so the
   transaction is `failed` even when some settings went through. That is §6.2's
   "failed with a changed effective state" row, emitted like `partial` exactly as
   the table prescribes; before this change the row was unreachable (dead
   normative text) and the case was reported as `partial`. The real transport now
   produces it, tested in
   `tst_transport::portErrorDuringApplyStaysWithinTheObservationBudget()`.
3. **The modal preferences method is not driven by a test.** `slotShowPreferences()`
   runs `QDialog::exec()`, so `tst_sessiondocument::aFailedLiveEndpointChangeIsNotRetried()`
   replays its exact sequence and its result-aware guard against a real document,
   transport, pty and controller instead. The step-5 review accepted this and
   rejected a dialog-seam refactor as outside the accepted scope.
4. **The two legacy widget consumers and the dialog are not instantiated by a test.**
   §13 asks that the hex monitor and the text logger retain their character-signal
   behaviour, and that the compatibility field keeps its UI/config behaviour. M8
   changes only the signals the consumers read and the request the dialog fills;
   both are asserted directly - exactly one `sentChar` per accepted byte including
   the 64 KiB upload, `receivedChar` still fed by the legacy buffer path without a
   live activation, and the profile round trip of `startBits` through a real
   `KomportApp` (`tst_profileerror::startBitsSurvivesTheProfileRoundTrip()`).
   What is *not* driven is the modal settings dialog and the widgets themselves:
   `QDialog::exec()` needs a UI harness this repository does not have, and the
   widgets belong to Milestone 7 code that M8 does not touch. A test that
   instantiates them would test the harness, not this milestone.
5. **The effective endpoint is the configured device path, not
   `QSerialPort::portName()`**, which strips the `/dev/` prefix (`pts/6` for
   `/dev/pts/6`) and would therefore misreport the device. Documented in
   `readBackEffectiveConfiguration()` and `applyHardwareSettings()`.
6. **The legacy RX buffer is still not cleared by `close()`** and the flush timer
   keeps running, so legacy RX can cross an activation boundary. That is the
   pre-existing display behaviour M8 must not change; the event path is unaffected
   because observations carry the activation id. A change needs its own reviewed
   slice.
7. **Record cost.** A byte-wise upload produces one event per accepted write, and
   the v1 record prefix is 44 bytes, so the M8 event path is deliberately
   expensive for that pattern; M9's streaming policy owns it (§13 sizing note).
8. **Non-standard baud rates can read back rounded.** The transport reports what
   the port has, so a kernel-rounded rate is reported truthfully and a repeated
   identical request then counts as a hardware change. Honest by construction.
9. **Corrections to my own statements:** I reported "14 test methods" for
   `tst_sessioncontroller` in the step-4 round-1 briefing where 13 existed (18
   after the review cycles, 23 in `tst_transport` now); and my first version of the
   self-review said "seven commits" for `master..HEAD`, which holds 13 - seven of
   them are the M8 implementation/review sequence from `8e3cdbd` on.
10. **Historical review records are not retro-edited.** The wording that rounds 3-5
   corrected in code and specification still appears inside the round records
   under `docs/reviews/`; those files are the verbatim record of what was claimed
   in each round. The round-6 review accepted this scope explicitly.
11. **Not verifiable here:** the Qt 6.3 build criterion (no 6.3 toolchain, no CI),
   a device-level serial test, and platform-specific `QSerialPort` signal timing.

## 5. Process notes

The five implementation steps went through six independent review rounds for
steps 2-3, six for step 4 and two for step 5, all through the local Codex CLI with
records under `docs/reviews/`. Findings that changed behaviour: the missing live
endpoint reopen in the entry point (step 3, blocker), the discarded port error and
the non-reentrant transaction window (step 3), the activation high-water mark and
the unreachable failed-id set (step 4), and the double open attempt of the
preferences path (step 5). Five findings were contradictions between an accepted
document and the code: they were resolved in the documents (C1-C5) or in the code,
never by silently preferring one side.

This milestone ships **no production consumer**: the controller is owned by the
document and emits its events into a void until M9's recorder exists. That is the
specified state, not an oversight.

## 6. Verification evidence (2026-09-18)

The independent review runs in a read-only environment and cannot execute
anything, so the results behind the tables above are recorded here as produced:

- Configure: `cmake -S . -B /tmp/komport-m8-build -DCMAKE_CXX_FLAGS="-Wall -Wextra"`
  with the locally installed Qt 6.11.1 (the declared floor is 6.3).
- Build: `cmake --build /tmp/komport-m8-build -j4` → exit 0, **0 warnings**,
  0 errors (GCC).
- Tests: `ctest` in that build directory → **100% tests passed, 0 failed out of
  13**, real time 3.58 s. Targets in ctest order: `tst_cellarray`, `tst_emulation`,
  `tst_serial`, `tst_transport`, `tst_sessioncontroller`, `tst_sessiondocument`,
  `tst_windowlifetime`, `tst_profileerror`, `tst_selection`, `tst_appearance`,
  `tst_charset`, `tst_i18n`, `tst_sessioncontract`.
- M8 test targets and their methods, all executed without a skip:
  `tst_sessioncontract` 10, `tst_transport` 23, `tst_sessioncontroller` 18,
  `tst_sessiondocument` 3 - 54 methods, 0 skipped. The pre-existing
  `tst_profileerror` now holds 2 methods, the second one being the profile round
  trip of the compatibility field added for §13.
- The nine pre-existing targets above are unchanged and green, including
  `tst_serial` (legacy setter/monitor behaviour) and `tst_profileerror` (the
  synchronous failed-open path).
- M8 commits (`8e3cdbd` … `18be2ff`, seven commits: steps 2-5 with their review
  records); the step-6 test additions and this self-review are the uncommitted
  slice reviewed together with them.
- The tests force `QT_QPA_PLATFORM=offscreen`; the pty tests use `openpty()` and
  reported no skip in this environment.
