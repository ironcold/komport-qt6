# Independent implementation review — M8 step 3

## Verdict

**Not ready to commit.** One BLOCKER and two HIGH findings must be fixed first. Step 4’s controller/FIFO work is correctly absent from this slice, but step 3 itself does not yet conform to the accepted configuration and event contracts.

The inspected working tree is limited to `KomportSerial`, `ConfigurationResult`, test CMake wiring, and `tst_transport`; step-2 contract files are committed in `8e3cdbd`.

## Compliance

| Requirement | Status | Evidence |
|---|---|---|
| ADR-003 activation IDs increment per `open()` attempt | Met | `komport/komportserial.cpp:179-180`; tests check IDs 1–3 at `tests/tst_transport.cpp:449-466`. |
| Failed open consumes ID, emits no `opened` | Met for the direct path | Failure emits only `transportError(attempt, …)` at `komport/komportserial.cpp:183-191`. |
| Single failed-open error | Partially met | The normal synchronous path is guarded at `komport/komportserial.cpp:510-516`, but the guard has no correlation for a delayed error after a subsequent open. No test covers that case. |
| `closed()` last for a live activation | Met on the ordinary close path | `close()` clears the live ID and emits `closed` last at `komport/komportserial.cpp:212-227`. See M1 for post-close callbacks. |
| Synchronous `open()` result | Met | `QSerialPort::open()` is called and its result returned in the same call at `komport/komportserial.cpp:182-208`. |
| Process-wide monotonic source clock | Met | Static `QElapsedTimer` at `komport/komportserial.cpp:28-40`. |
| One observed TX primitive for `putChar`, both `putStr`, `writeBytes` | Met | All route to `writeRaw()` at `komport/komportserial.cpp:388-466`; TX observation exists only there at lines 393-400. |
| RX observation before legacy buffer | Met | `bytesReceived` precedes `mRxBuffer += chunk` at `komport/komportserial.cpp:483-492`. Empty chunks are ignored. |
| TX accepted-prefix / legacy bool returns | Met | Prefix is observed and `sentChar` is emitted for accepted bytes at `komport/komportserial.cpp:393-400`; boolean adapters require full acceptance at lines 439-466. |
| Configuration: closed port stores only | Met | `applyConfigurationInternal()` stores, applies no hardware, emits no observation through `storedOnly` at `komport/komportserial.cpp:612-636`. |
| Configuration: open, unchanged endpoint / documented no-op | Nominally met | No-op check and no emission at `komport/komportserial.cpp:639-644`, `707-732`. Its reliability depends on the broken effective-state handling below. |
| Configuration: open, endpoint changed | **Not met** | Public `applyConfiguration()` never closes/reopens when `_requested.endpoint` differs; it only applies non-endpoint hardware setters at `komport/komportserial.cpp:580-586`, `663-680`. |
| Open/reopen emits close, open, then one result | Met on normal paths | `open()` closes first, emits `opened`, then configuration observations at `komport/komportserial.cpp:168-205`. |
| `startBits` compatibility-only | Met | It is stored but not passed to `QSerialPort` at `komport/komportserial.cpp:353-358`; compatibility-only observation mapping works at lines 681-700. |
| Effective read-back metadata | **Not met** | Metadata uses `portName`, not required `endpoint`, at `komport/komportserial.cpp:735-745`; internal effective state is overwritten by the request at line 679 rather than read-back values. |
| Duplicate self-application removed | Met | No `settingsChanged → slotSettingsChanged` connection remains; see `komport/komportserial.cpp:115-121`. |
| Legacy setters emit compatibility notification | Ambiguous / not met literally | Only `setBaudRate()` emits `settingsChanged` (`komport/komportserial.cpp:235-243`); device, framing, flow, RX queue, and flush setters do not. The accepted spec says each setter emits its compatibility signal. |
| `KomportDoc` owns controller and destroys it first | Not yet applicable | No `SessionController` exists; `KomportDoc` only contains `mSerial` at `komport/komportdoc.h:109-115`. |
| SPEC 7 controller state, failed-open event conversion, activation filter | Not yet applicable | Step 4 is absent. |
| SPEC 8 source ID 1, event sequence/time mapping, no decoded-data source | Not yet applicable except raw transport boundary | Raw RX/TX preservation is implemented; event creation is absent. |
| SPEC 9 error metadata | Mostly met at transport layer | `kind`, `code`, `message` are added at `komport/komportserial.cpp:86-95`; write context is included at lines 415-422. |
| SPEC 10 FIFO and non-reentrant event delivery | Not yet applicable | Requires the absent `SessionController`. |

Step-2’s structural event validator is also non-conforming: it accepts any undeclared `SessionEventType` value as a non-data event because it only distinguishes `Data` from “everything else” (`komport/sessionevent.h:96-107`), contrary to ADR-002’s declared-enumerator requirement.

## Findings

1. **BLOCKER — public configuration does not reopen on endpoint change**

   `applyConfiguration(request)` with an open port and a changed endpoint must emit old `TransportClosed`, new `TransportOpened`, then exactly one result. Instead, it changes stored endpoint state and applies only baud/framing/flow to the old open port (`komport/komportserial.cpp:647-680`). It can report a successful configuration for a port it never opened.

   Minimal fix: make the public entry point detect an endpoint change while live, close the old activation, store the complete request, reopen, and emit exactly the one open-and-configure result after `opened`. Add the operation-table test.

2. **HIGH — effective configuration is not the specified read-back state**

   `readBackHardwareJson()` uses `portName` rather than `endpoint` (`komport/komportserial.cpp:738`), and successful hardware application replaces `mEffective` with the requested values (`line 679`). This invalidates no-op decisions and `changedGroups` where Qt normalizes or rejects values. Invalid numeric baud values are especially wrong: `applyHardwareSettings()` silently retains the previous baud (`lines 289-299`) but returns success if other fields succeed, after which line 679 records the invalid request as effective.

   Minimal fix: define one canonical read-back-to-`TransportConfiguration` conversion using `endpoint`, update `mEffective` from that conversion, and treat invalid/unapplied requested hardware as partial/failed rather than full. Test normalization, invalid numeric baud, rejected field, and metadata keys.

3. **HIGH — structural validation accepts undeclared event types**

   `isValidSessionEvent()` accepts `static_cast<SessionEventType>(999)` if direction is `None` and payload empty (`komport/sessionevent.h:96-107`). ADR-002 requires `type` to be a declared enumerator.

   Minimal fix: explicitly switch over all eight allowed values and reject the default case; add a unit test for invalid enum values.

4. **MEDIUM — observations can carry activation ID zero outside a live activation**

   `slotDataAvailable()` emits `bytesReceived(observingActivationId(), …)` without checking a live activation (`komport/komportserial.cpp:483-490`). Likewise, a delayed runtime port error emits `transportError(0, …)` after close (`lines 500-522`). This conflicts with ADR-003’s activation identity rule and creates avoidable step-4 filter diagnostics.

   Minimal fix: preserve legacy RX buffering and `settingsFailed`, but suppress transport observations when `mCurrentActivationId == 0`; add a direct-callback/post-close regression test.

5. **ARCHITECTURE QUESTION — legacy setter notification text conflicts with retained behavior**

   SPEC 6.2 literally says each legacy setter applies once and “then emits its compatibility signal.” The implementation retains the old behavior for all except baud: no `settingsChanged()` from device/framing/flow/buffer setters. This may be intentional compatibility preservation, but it is not the accepted wording.

   Minimal fix: clarify the accepted spec whether “compatibility signal” means each setter’s pre-existing signal behavior, or make the notifications uniform and explicitly authorize that visible behavior change. Do not silently choose in code.

6. **TEST GAP — configuration-error cardinality is not tested against real `QSerialPort` errors**

   The scripted hardware seam returns a Boolean and never emits `QSerialPort::errorOccurred` (`tests/tst_transport.cpp:75-91`). It therefore cannot reveal an extra runtime error interleaved with the intended apply error. The specification caps a transaction at two observations.

   Minimal fix: add a seam for error reporting or an assertion around the complete observation sequence.

## Known deviations

- **D1 — `ConfigurationResult::storedOnly`: acceptable only with an accepted API documentation amendment.** It is not metadata and accurately exposes the closed-port outcome (`komport/transportconfiguration.h:106-111`). However it is an addition to a public QtCore contract created in step 2, outside the accepted written interface. Classify as **DOCUMENTATION**: amend the spec/API documentation before retaining it, or remove it.

- **D2 — open reports `changedGroups: ["hardware"]` despite equal prior snapshot: not compliant as written.** The implementation deliberately does this at `komport/komportserial.cpp:695-696`. SPEC 6.2 says `changedGroups` lists groups whose values actually changed, while its table requires an open transaction but has no unchanged-open mapping. This is an **ARCHITECTURE QUESTION** requiring an accepted clarification; the current implementation cannot be declared spec-compliant merely because the behavior is sensible.

- **D3 — effective `changedGroups`, request-versus-effective no-op decision: conceptually sound and matches the observable mapping.** The implementation calculates requested differences for no-op (`lines 604-644`) and effective differences for reporting (`lines 689-700`). It becomes correct only after HIGH finding 2 fixes `mEffective`.

- **D4 — `writeBytes()` also emits legacy `sentChar()`: acceptable.** It is a new entry point, and routing it through the same primitive gives the existing hex monitor a faithful accepted-byte view (`komport/komportserial.cpp:393-400`). Add a `sentChar` assertion for this new path.

- **D5 — split calculation from emission: sound in principle.** `applyConfigurationInternal()` calculates one result while the public entry point and `open()` emit it exactly once (`komport/komportserial.cpp:204-205`, `580-586`). Strictly, the public `applyConfiguration()` *does* emit; only the internal helper does not. The split is appropriate for the required `opened → configurationChanged` order, subject to fixing the endpoint-change path.

## Test sufficiency against SPEC 13

Present:

- Core-only contract compile target and QtCore-only value tests.
- Basic activation IDs and direct failed-open transport error.
- Scripted RX non-empty/empty behavior.
- Accepted-prefix TX behavior.
- Basic entry-point TX observations.
- No-op, local-buffering, `startBits`, partial/failed configuration cases.
- Closed-store then open and basic reopen tests.

Missing or weaker than required:

- No controller double, controller tests, event collector, source ID/sequence/timestamp mapping, activation filter, FIFO, two-subscriber non-reentrancy, or failed-open duplicate suppression. These belong to step 4/6 and are absent.
- No test for public `configure()` with endpoint changed — the BLOCKER.
- No test for each operation-table row or each legacy setter.
- No assertion that configuration metadata has `endpoint` and genuine read-back effective values.
- No zero-accepted write test.
- No real PTY RX concatenation test spanning `00..FF`; the RX test is a scripted protected-slot call, not end-to-end.
- No all-byte transport-observation TX test; `tst_serial` proves wire bytes but not `bytesWritten`.
- No 64 KiB byte-wise upload/pathology test.
- `tst_transport` does not assert `settingsFailed()` remains synchronous; `tst_profileerror` is the indirect regression but I could not execute it.
- `ContractProbeTransport` claims to be a contract probe while `open()` returns true and `isOpen()` false (`tests/session_contract_compile.cpp:41-44`); harmless for compilation, but not behavioral evidence.

## Minimum must-fix order

1. BLOCKER: endpoint-changed public configuration transaction.
2. HIGH: actual effective read-back state and invalid-baud/result handling.
3. HIGH: declared-enumerator validation in `SessionEvent`.
4. Add focused tests for 1–3 and transport observation cardinality.
5. Resolve/document D1, D2, and the legacy-signal ambiguity before acceptance.

FIFO, controller ownership/destruction, event source ID/sequence/time mapping, activation filtering, and non-reentrancy may be recorded as steps 4–6 follow-ups; they must not be presented as implemented.

To claim step 3 complete, the transport/configuration requirements above must be fixed and its required focused tests must build and pass. To claim any substantive part of step 4, `KomportDoc` must own a `SessionController` destroyed before `mSerial`, with ADR-005 mapping, source ID 1, failed-open handling, activation filtering, and FIFO non-reentrant delivery; none exists yet.

## Not verified

I did not build or run tests: the available build executable predates this working-tree change (`build/tests/tst_serial` is older than the modified sources), and the read-only environment cannot produce a current build. Therefore I could not verify compilation, warnings, `ctest`, PTY behavior, or the actual Qt runtime timing of `errorOccurred()`/`settingsFailed()`.