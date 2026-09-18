I could not write the requested review file: this workspace is read-only, and no output filename was specified.

## Round-1 verification

| Round-1 item | Verdict | Evidence / correction assessment |
|---|---|---|
| BLOCKER: live endpoint change did not reopen | **Partially fixed** | Successful live endpoint changes now store the full request and call `open()` ([komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:597)); the test verifies close → open → one result ([tst_transport.cpp](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:520)). However, if reopening fails, the returned result incorrectly sets `storedOnly = true` ([komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:603)), although the operation started while live and consumed an open attempt. |
| HIGH: effective state/read-back and `endpoint` metadata | **Fixed** | Hardware fields are read back after application ([komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:294)); `mEffective` uses that state ([komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:690)); metadata uses `endpoint`, not `portName` ([komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:823)). Using the requested device path instead of `QSerialPort::portName()` is correct for the stated PTY reason. |
| HIGH: undeclared `SessionEventType` accepted | **Fixed** | Explicit exhaustive switch rejects the default ([sessionevent.h](/home/max/Development/misc/komport-qt6/komport/sessionevent.h:96)); test covers values `999` and `0` ([tst_sessioncontract.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessioncontract.cpp:124)). |
| MEDIUM: observations with activation ID 0 | **Fixed in code; test gap remains** | RX and runtime-error observations require a live activation ([komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:495), [komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:523)); configuration observations are likewise suppressed ([komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:770)). I found no post-close/direct-callback regression test. |
| TEST GAP: configuration observation order/cardinality | **Partially fixed** | The scripted partial case asserts `configurationChanged,transportError`, and failed-without-change produces no configuration observation ([tst_transport.cpp](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:403)). It still does not model `QSerialPort::errorOccurred()` during a failed setting application. |
| Step-2 compile probe inconsistent | **Fixed** | Probe tracks its open state and rejects writes while closed ([session_contract_compile.cpp](/home/max/Development/misc/komport-qt6/tests/session_contract_compile.cpp:41)). |
| D1: undocumented `storedOnly` public API addition | **Not fixed** | Header documents it ([transportconfiguration.h](/home/max/Development/misc/komport-qt6/komport/transportconfiguration.h:106)), but the accepted SPEC-M8 does not. C3 requires an accepted amendment first. |
| D2: open reports `hardware` despite equal prior snapshot | **Not fixed as documentation** | Code deliberately does this ([komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:720)); C2 should amend the normative text. |

## New findings

1. **HIGH — failed live endpoint change mislabels its result as stored-only**

   A live endpoint-change request closes the old activation and calls `open()`. If the new open fails, it returns `storedOnly = true` ([komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:603)). That contradicts the proposed C3 meaning and falsely describes a failed activation change as a closed-port store-only operation.

   Minimal fix: return a non-`storedOnly`, `Failed` result with the open-failure message; document that this path produces the single `Error(kind: "open")`, not a configuration observation.

2. **HIGH — transaction cardinality is not protected against `errorOccurred()` during configuration**

   `slotPortError()` emits a runtime transport error whenever an activation is live ([komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:523)). A failed apply subsequently emits the mandated apply error ([komportserial.cpp](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:789)). There is no “configuration transaction in progress” guard/coalescing rule, so an apply that causes `QSerialPort::errorOccurred()` can exceed the specification’s two-observation maximum.

   Minimal fix: coalesce/suppress port errors attributable to the active apply attempt, retaining exactly the transaction’s specified apply error. Add an injected-error seam test.

3. **TEST GAP — required 64 KiB byte-wise TX test is absent**

   SPEC-M8 §13 explicitly requires the 64 KiB `putChar()` volume/pathology test. The all-byte PTY test covers `00..FF` once ([tst_transport.cpp](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:702)), not this requirement.

4. **TEST GAP — core metadata test still constructs obsolete `portName` metadata**

   [tst_sessioncontract.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessioncontract.cpp:267) builds an `effective` object containing `portName` and does not assert its absence. The integration test is correct, but this core test should use/assert `endpoint`.

Steps 4–7 are absent as declared. Consequently this slice does not yet provide controller ownership, activation filtering, FIFO delivery, source/sequence/time mapping, or application-call-site migration; it would not satisfy those later steps unchanged.

## Clarifications

- **C1 — approve as written.** It accurately matches the pre-M8 signal sources and the retained code behavior.
- **C2 — approve with revised wording:**

  > The `open()` rows of the operation table always perform the open-and-configure transaction, and that transaction applies the hardware settings even when the requested values equal the last effective snapshot, because a freshly opened port carries no settings. Its result therefore includes `"hardware"` in `changedGroups`, even where a configure-level comparison would find no value difference. This is an explicit exception for an open-and-configure transaction; the documented no-op row applies only to `configure(request)` on a live port with an unchanged endpoint.

- **C3 — approve with revised wording, plus the code fix above:**

  > The value returned to the caller additionally carries `storedOnly`. It is true only when `configure(request)` is called while the port is closed and the request is stored without a transaction. It is a return-value flag, never event metadata, and such an operation emits no observation. It is false for a live endpoint-change request that fails to reopen; that path reports its open failure through the single `Error(kind: "open")` observation and returns a failed result.

## Verdict

**Not ready to commit.**

Minimum must-fix list:

1. Correct failed live endpoint-change return semantics.
2. Guarantee configuration transactions cannot emit more than two observations when `errorOccurred()` is raised during apply; test it through a seam.
3. Add the required 64 KiB byte-wise TX test.
4. Accept and apply the C2/C3 SPEC-M8 amendment before retaining the corresponding behavior/API.

I did not run the build, warnings check, `ctest`, or PTY tests; those claims cannot be verified in this read-only environment.