## Verdict table

| Item | Verdict | Evidence |
|---|---|---|
| Must-fix: failed live endpoint change result | Closed | The live endpoint branch returns a non-`storedOnly`, `Failed` result with a message after `open()` has reported the single open error: [komportserial.cpp:605](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:605). The test checks the old close, one `kind: "open"` error, no configuration result, and failed return: [tst_transport.cpp:576](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:576). |
| Must-fix: transaction-cardinality guard | Partially closed | The direct, non-reentrant hardware-apply path is guarded at [komportserial.cpp:702](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:702), and `slotPortError()` suppresses an extra event at [komportserial.cpp:524](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:524). The seam test covers partial and successful scripted applies: [tst_transport.cpp:794](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:794). However, it loses genuine port errors and the boolean guard is not reentrancy-safe; see findings below. |
| Must-fix: 64 KiB byte-wise TX test | Closed | The test sends 64 KiB through `putChar()`, counts observations and legacy chars through lambdas, checks one-byte payload/order, then verifies exact PTY delivery: [tst_transport.cpp:860](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:860). It does not retain an in-memory *event* vector. |
| Must-fix: C2/C3 specification amendment | Closed | The approved C2 text is present verbatim at [SPEC-M8:186](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:186); C3 is present verbatim at [SPEC-M8:194](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:194). |
| Test gap: observations require a live activation | Partially closed | The new test verifies RX and runtime-error suppression with initial activation ID zero while preserving `receivedChar()` and `settingsFailed()`: [tst_transport.cpp:837](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:837). It does **not** perform `open()` → `close()` → direct RX/error callback, so it cannot regress the reset in [komportserial.cpp:217](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:217). |
| Test gap: obsolete `portName` core metadata | Closed | The core metadata test now constructs `endpoint` and string `baudRate`, and asserts `portName` is absent: [tst_sessioncontract.cpp:256](/home/max/Development/misc/komport-qt6/tests/tst_sessioncontract.cpp:256). |

## New findings

1. **HIGH — the cardinality guard drops a real port error instead of recording it.**

   `slotPortError()` emits the legacy notification, then returns without retaining the error while the guard is set: [komportserial.cpp:511](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:511). `applyConfigurationInternal()` has no state through which that error can affect `ConfigurationResult`; a successful setter return therefore produces a `full` configuration observation and no error.

   The new test explicitly codifies this loss: it injects `ResourceError` during an otherwise successful apply and asserts no `transportError`: [tst_transport.cpp:825](/home/max/Development/misc/komport-qt6/tests/tst_transport.cpp:825). This conflicts with SPEC-M8 §9: a transport error while live becomes an `Error`: [SPEC-M8:332](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:332).

   Minimal fix: retain a transaction-scoped port-error flag/message. Consume it in the transaction result, turning the outcome into a recorded apply failure/partial result as appropriate, while keeping the two-observation maximum. Update the successful injected-error test to require that single recorded transaction error.

2. **HIGH — `mConfigurationTransactionInProgress` is not safe against direct-signal reentrancy.**

   The boolean is set/reset unconditionally around the virtual seam: [komportserial.cpp:708](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:708). But `slotPortError()` emits `settingsFailed()` before consulting the flag: [komportserial.cpp:511](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:511). A direct legacy receiver may call a setter or `applyConfiguration()`; the nested operation resets the boolean to false while the outer hardware application is still active. A later outer error can then escape as a runtime observation, violating the stated transaction budget.

   Minimal fix: make the apply state nesting-safe (scope restoration/depth rather than an unqualified boolean), and add a seam test with a reentrant `settingsFailed()` receiver.

## C1/C2/C3 alignment

C1’s legacy-signal rule matches the implementation: `open()` and `setBaudRate()` emit `settingsChanged()`, while framing, flow-control, queue, and flush setters do not; the former self-connection is absent. See [SPEC-M8:211](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:211), [komportserial.cpp:208](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:208), and [komportserial.cpp:236](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:236).

C2 matches: successful `open()` forces hardware application and reports `"hardware"` even with an unchanged snapshot: [komportserial.cpp:201](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:201), [komportserial.cpp:739](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:739).

C3 matches: `storedOnly` is metadata-free ([transportconfiguration.cpp:81](/home/max/Development/misc/komport-qt6/komport/transportconfiguration.cpp:81)), true only for the closed store-only branch ([komportserial.cpp:666](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:666)), and false for a failed live endpoint change ([komportserial.cpp:617](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:617)). No residual C1/C2/C3 wording mismatch found.

## Final verdict

**Not ready to commit.**

Minimum preceding fixes:

1. Preserve a synchronous port error raised during hardware application in the transaction result/error observation.
2. Make the transaction guard reentrancy-safe and test that path.
3. Extend the activation-zero test through a real close cycle.

I could not run the build, warning check, `ctest`, or PTY tests in this read-only environment; the claimed green results and warning count remain unverified.