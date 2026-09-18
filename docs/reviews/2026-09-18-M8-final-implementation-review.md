## Verdict

**M8 must not be declared complete.** The stated seven implementation/review commits plus the step-6 test slice do not meet the accepted specification.

### BLOCKER — failed-open compatibility is not preserved

SPEC-M8 §7 and §14 require synchronous legacy `settingsFailed()` on a failed `open()`. On failure, [`KomportSerial::open()`](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:185) emits `transportError` and `settingsChanged`, but never `settingsFailed()`. The latter is emitted only by [`slotPortError()`](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:508), so its timing depends on `QSerialPort::errorOccurred()`.

The code and self-review explicitly acknowledge it “may therefore arrive synchronously or asynchronously.” That contradicts the accepted requirement; it is not acceptable as an M8 “pre-existing” limit. The application’s error-status suppression can consequently be overtaken by a late notification.

`tst_profileerror` assumes the local failure is synchronous, but does not assert that `settingsFailed()` occurs before `open()` returns. `tst_transport::failedOpenReportsExactlyOneOpenErrorAndConsumesAnActivationId()` does not observe `settingsFailed()` at all.

### HIGH — required failed-with-change outcome is absent in the real transport

§6.2 and §13 require the observable mapping for `full`, `partial`, `failed` with change, and `failed` without change.

The production classification makes a failed apply with a changed effective hardware state become `partial`, not `failed` ([`komportserial.cpp`](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:731)). A port-error path with changes does the same ([`komportserial.cpp`](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:769)). Thus real `failed`-with-change is not generated.

The controller test only scripts such metadata; it proves controller pass-through, not the local transport mapping. This is both an implementation deviation and a test gap.

## §14 self-review verification

| Criterion claim | Review result |
|---|---|
| Local Qt compilation and declared Qt 6.3 floor | **Cannot be verified here.** CMake declares 6.3, but the read-only sandbox prevented regeneration/build. |
| Genuine Qt 6.3 build not run | **Claim confirmed.** No 6.3 toolchain or CI configuration is present. |
| Core-only contract object target builds | **Cannot be verified here.** The target is correctly configured with only `Qt6::Core`, but could not be built in this environment. |
| Binary chunks without legacy API/signal change | **Claim wrong.** The failed-open legacy signal timing is no longer guaranteed as required. |
| Document-owned controller emits ordered byte-exact live events | **Claim confirmed by code inspection**, but its PTY execution was not independently rerun. |
| Source ID 1, failed-open ID, source/session timing | **Claim confirmed.** Controller assignment/filtering and deterministic tests match the requirement. |
| No QWidget/executable-specific dependency | **Claim confirmed.** Public contract headers use QtCore only; the Core-only target is correctly isolated, though not rebuilt here. |
| Non-data observations and all compatibility expectations | **Claim wrong.** Settings vocabulary/persistence/dialog source paths appear unchanged and each migrated call site has one transaction, but synchronous failed-open `settingsFailed()` is not preserved. |
| NUL/all-byte PTY tests pass | **Cannot be verified here.** Tests exist; execution could not be rerun. |
| Full 13/13 `ctest` passes | **Cannot be verified here.** Build-system regeneration attempted to write generated Qt/CMake files and failed under the read-only sandbox. |
| No format/replay/decoder/new active-transmission feature | **Claim confirmed.** The diff introduces none. |

## §13 test verification

The following self-review claims are not adequately covered.

- **Failed-with-change mapping:** `tst_sessioncontroller::operationTableRowsCrossTheControllerUnchanged()` injects a fabricated event. It does not prove that `KomportSerial` produces it; in fact, current production logic cannot.

- **Failed-without-change assertion is incomplete:** `tst_transport::partialAndFailedAppliesReportTheirApplyStatus()` checks `apply_failed` code, but does not assert `applyStatus == "failed"` or empty `changedGroups` in the error metadata.

- **All operation-table rows:** `legacySettersKeepTheirNotificationBehaviour()` covers baud, framing, and flow control only. It does not assert the open-port and closed-port operation-table behavior for `setDeviceName`, `setRxQueue`, and `setFlushRate`.

- **`startBits` requirement remains incomplete:** the uncommitted strengthening compares `effective` and `localBuffering`, but not the `requested` hardware metadata section. It also does not test the required unchanged UI/configuration behaviour or persistence of the Start Bits field.

- **PTY “no event is empty”:** the new assertions check RX payloads only. `tst_transport::ptyCarriesEveryByteValueInBothDirections()` does not assert that every TX observation payload is non-empty; `tst_sessiondocument` has no TX case. The claim that both PTY tests assert no observed payload is empty is therefore overstated.

- **Hex monitor/text logger regression:** no test instantiates either consumer or verifies its character-signal behaviour. Current wiring is unchanged, and transport tests observe `receivedChar`/`sentChar`, but that is indirect coverage only.

The remaining controller requirements—source-time mapping, one-event-per-observation, activation filtering, duplicate failed-open suppression, FIFO/non-reentrancy, structural-event checks, and byte preservation—are substantively covered by the deterministic tests.

## Undisclosed or incorrectly judged limits

- The failed-open signal timing is disclosed, but misjudged. It is an explicit accepted requirement and must be fixed before closure.
- The real transport’s inability to produce `failed` with changed effective state is not disclosed.
- The §13 gaps above are not disclosed.
- `git log master..HEAD` contains **12** commits, not seven. Seven are the post-acceptance implementation/review sequence; the range also includes five preceding foundation/spec commits. The uncommitted slice is limited to the stated test files plus the untracked self-review.

## Must-fix before closure

1. Guarantee exactly one synchronous legacy `settingsFailed()` for a failed `open()` while retaining exactly one `Error(kind: "open")`.
2. Implement or formally re-specify the required `failed`-with-changed-effective-state mapping; the current code/spec combination is inconsistent.
3. Add the missing §13 assertions/tests listed above, especially failed status metadata, all legacy setter rows, Start Bits UI/persistence/metadata, and TX non-empty PTY observations.
4. Run and record the warning-enabled build and complete test suite.
5. Leave the Qt 6.3 checkbox open until a genuine Qt 6.3 build runs.

Accordingly, no final §14 acceptance checkbox should be closed as a milestone-completion action yet. The Qt 6.3 criterion must remain open in all cases.

I could not independently run the build or tests because the environment is filesystem read-only; CMake’s required generated-file writes were blocked.