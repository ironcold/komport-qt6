# Independent M8 Foundation Review

**Verdict: not accepted.** Neither ADR-002…ADR-009 nor SPEC-M8 should be marked Accepted as written, nor merely by applying A1.1…A16.1 unchanged. Several amendments identify real defects, but A1, A6, A7, A11, and A16 are materially incomplete or incorrect.

No M8 implementation exists in the reviewed codebase; this is therefore a contract/specification review, validated against the current serial implementation and its callers.

## F1–F16 assessment

| Finding | Assessment and evidence | Resolution | Category |
|---|---|---|---|
| **F1 – clock origin/domain** | **Valid.** ADR-003 says timestamps are activation-relative (`ADR-003:37-45`), while ADR-002/SPEC-M8 use a common local clock (`ADR-002:43-47`, `SPEC-M8:69-72,136-137`). The architecture document also expects relative session time (`komport-multiport-sniffer-time-alignment.md:365-401,1174-1178`). No current serial timestamp source exists yet, so the conflict is entirely contractual. | **Reject A1.1–A1.4 as written.** A process/agent-start zero cannot reliably be converted to exact wall time, and requiring that conversion is unnecessary and misleading. Use a process-wide monotonic **source** clock, but make session time relative to a declared session origin. Correct text: “`sourceTimestampNs` is measured by one monotonic clock for the lifetime of its clock domain. For M8, on the first event accepted by a controller, the controller records `sessionOriginSourceTimestampNs`; `timestampNs = sourceTimestampNs - sessionOriginSourceTimestampNs`. The raw source time is retained. The session header records a stable `clockDomainId`, the source/session reference pair, and an optional wall-clock correlation with stated precision; wall-clock correlation is informational, not an exact conversion of monotonic zero.” | **BLOCKER** |
| **F2 – all TX entry points** | **Valid.** TX currently enters through `putChar()` (`komportserial.cpp:172-178`), both `putStr()` overloads (`184-223`), keyboard/emulation paths (`komportemulation.cpp:705-736,1271-1292,1581-1585`), macros (`komport.cpp:1355-1386`), and byte-wise upload (`komporttransfer.cpp:53-65`). A new `writeBytes()` alone would miss the existing product’s TX. | **Accept intent, revise wording.** One internal write primitive must observe every legacy and new write entry point. However, “one chunk on the wire” and “replay/decoder must not recombine” are wrong: `QSerialPort::write()` acceptance is not electrical-wire delivery, and decoders must be able to reassemble events (`komport-session-replay-simulation-architecture.md:266-290`). Replace the final part with: “Each successful/partially accepted API write creates one transport-observation chunk. This records API acceptance boundaries, not physical wire framing. Decoders may assemble byte streams across events; event storage preserves the original boundaries.” | **HIGH** |
| **F3 – reset on transport activation** | **Valid.** `open()` always closes first (`komportserial.cpp:64-73`), and `setDeviceName()` closes/reopens an already-open port (`48-55`). A reset on each open contradicts ADR-002’s strictly increasing per-session sequence (`ADR-002:40`) and gives one document/controller repeated sequence ranges. | **Accept core resolution, revise state model.** Sequence must continue across successful close/reopen activations. `Closed` must not mean ordinary transport close if reopening is legal. Use `Idle` and `Live`; reserve terminal finalization for explicit controller/session finalization, if one is ever required. `KomportDoc::newDocument()` does not replace or reset the serial object (`komportdoc.cpp:137-158`), so the session boundary must explicitly be controller/document lifetime, not the File → New action. | **BLOCKER** |
| **F4 – partial-write omission** | **Valid.** `putStr()` emits `sentChar()` for exactly the accepted prefix before returning false (`komportserial.cpp:205-220`). Omitting that prefix from the event stream would contradict the existing diagnostic path. | **Accept A4.1–A4.4 with precision fixes.** `qint64 writeBytes()` is appropriate: `0..size()` means accepted byte count, `-1` means refusal/error. Rename/document `bytesWritten` as “accepted by the transport API,” not physically written. A partial prefix is a `Data/Tx` event; emit a separate `Error` with `kind: "write"` and `acceptedBytes`/`refusedBytes`. Do not call it generic `runtime`. | **HIGH** |
| **F5 – byte-wise transfer volume** | **Valid in substance.** Upload invokes `putChar()` per input byte (`komporttransfer.cpp:53-65`), so one accepted byte produces one TX observation under F2. The stated fixed record-prefix size is inaccurate: ADR-006’s prefix is 44 bytes, not about 30 (`ADR-006:29-41`). | **Accept A5.1 and A5.3 in principle.** Record that M8 preserves API write boundaries and that upload can create one-byte events. Do not make M9’s design contingent on an in-memory vector; the M9 writer must be streaming. A 64-KiB regression is useful, but it should assert byte count, ordering, and bounded consumer memory rather than dictate a future storage implementation. A5.2 belongs in ADR-006 but is redundant: it should simply say zero-length JSON is the encoding of `{}`. | **MEDIUM** |
| **F6 – effective configuration cannot be derived** | **Valid, and more serious than stated.** `open()` applies settings, then unconditionally emits `settingsChanged()` (`komportserial.cpp:64-72`), whose self-connection applies them **again** (`36-39,107-112`). `applyPortSettings()` is `void` and applies properties sequentially (`115-169`); `setFraming()`/`setFlowControl()` bypass `settingsChanged()` (`289-302`). A failed apply can leave an open port partly changed. | **Reject A6.1 as written.** “No configuration event on unsuccessful change” can leave the recording with stale configuration after a partial hardware change. Correct text: “Every logical configuration transaction produces exactly one result after all requested settings have been attempted. Its metadata contains requested settings, read-back effective settings, and `applyStatus` of `full`, `partial`, or `failed`. If the effective settings changed, emit `TransportConfigChanged` even for `partial`, followed by an `Error` where applicable. Do not emit duplicate transaction results. The legacy `settingsChanged()` signal may retain its compatibility behavior, but it must not drive a second hardware application.” | **BLOCKER** |
| **F7 – passive replay versus ITransport** | **Valid.** ADR-003 calls the controller the sole transport-to-event adapter (`ADR-003:42-45`), while ADR-007 says passive replay emits stored events through it without an output transport (`ADR-007:13-15`). An `ITransport` signal only carries byte chunks and transport metadata, not stored sequence, source ID, event type, or aligned timestamp. | **Reject A7.1.** A read-only `ITransport` would recreate events and lose or reinterpret persisted event identity; it cannot faithfully replay annotations/bookmarks or original sequence numbers. Correct text for ADR-003: “`SessionController` is the sole adapter for **live transport observations**. A future passive replay source emits immutable stored `SessionEvent` values through a separately specified read-only event-source/player interface; it is not an `ITransport` unless a later ADR defines exact preservation semantics.” ADR-007 should refer to that future source, not imply a transport adapter. | **HIGH / ARCHITECTURE QUESTION** |
| **F8 – reentrant delivery** | **Valid.** Direct Qt signal delivery is specified for M8 (`SPEC-M8:154-158`). Existing legacy delivery can transmit while processing received characters: `receivedChar` connects to emulation (`komportemulation.cpp:643-655`), which can issue replies (`1271-1292`). A consumer writing from `eventObserved` could cause nested TX event delivery and reorder what later subscribers observe. | **Accept A8.1 with one addition.** The controller needs a FIFO of complete observations and must assign sequence at dequeue/emission time. This rule applies to all signal types, including close/error/configuration, not merely data. Add a test with two subscribers to prove that both see `RX, TX`, rather than one seeing nested `TX` before its `RX` callback returns. | **HIGH** |
| **F9 – error distinction/open failure** | **Valid.** Current failures converge at `settingsFailed(QString)` (`komportserial.cpp:164-168,247-252`); existing tests even rely on an open error arriving synchronously in this path (`tests/tst_profileerror.cpp:64-97`). | **A9.1/A9.2 are incomplete.** SPEC-M8 currently says errors become events only when live (`SPEC-M8:144-150`) but also says failed open produces an error observation while not entering `Live` (`116-126`). Correct text: “A controller session exists from construction. An open attempt that fails emits one `Error(kind:"open")` while `Idle`; it does not emit `TransportOpened` or enter `Live`. `open()` is authoritative for its own result. Suppress the corresponding asynchronous port error by an explicit in-progress/error-generation guard, not by matching human-readable text.” Define metadata at least as `kind`, stable machine-readable `code`, `message`, and relevant non-secret numeric context. | **HIGH** |
| **F10 – observations outside Live** | **Valid.** The state model does not define late transport signals after close or before a successful open. | **Accept A10.1, with activation scoping.** Drop such observations and report a diagnostic without creating session events. Associate signal handling with an activation generation so a queued late signal from an earlier activation cannot be attributed to a later reopen. Do not assert that `TransportClosed` is necessarily the last callback absent this generation guard. | **MEDIUM** |
| **F11 – recorder/display divergence** | **Partly valid; one factual assertion is false.** RX queue trimming is real (`komportserial.cpp:237-244`) and character delivery is timer-delayed (`255-285`). But close does **not** clear `mRxBuffer` (`75-80`), so buffered legacy bytes are retained and can be emitted after close/reopen; they are not “dropped on close.” | **Revise A11.1.** Correct text: “The event stream preserves every non-empty `readAll()` result observed by the transport before legacy buffering and is independent of `RxQueue` and `FlushRate`. The legacy character path is lossy under RX overflow and delayed by its flush timer. Its buffer is currently retained across close/reopen; M8 must either preserve that behavior explicitly or clear it as a separately reviewed visible-behavior change. Event capture describes application-observed transport bytes, not an electrical-wire guarantee.” Do not require universal event/legacy equality. | **MEDIUM** |
| **F12 – PTY chunk assertions** | **Valid.** The existing serial PTY tests cover TX embedded NUL, not RX chunk boundaries (`tests/tst_serial.cpp:90-160`), and PTY/kernel buffering cannot guarantee a split. | **Accept A12.1 with a test-double requirement.** PTY tests should assert concatenated byte equality, no empty event, and order. “One event per transport read” requires a deterministic fake transport or injectable read-observation seam; it cannot be established solely by a PTY test. | **TEST GAP** |
| **F13 – unverifiable acceptance criteria** | **Valid.** The declared floor is Qt 6.3 (`CMakeLists.txt:21-34`), and `.github/` contains issue/PR templates but no workflow. The existing tests link `komport_core`, which publicly links Widgets (`tests/CMakeLists.txt:10-18`; `CMakeLists.txt:83-93`), so they cannot prove QtCore-only headers. | **Revise A13.1.** “Compile with Qt 6.3” is only an acceptance criterion if a Qt 6.3 build is actually run. Add a small object-library/compile target containing only public session headers and linking `Qt6::Core`; make Qt 6.3 CI or a release-gate prerequisite before claiming compatibility. | **TEST GAP** |
| **F14 – architecture-document drift** | **Valid.** The older architecture template still uses `QVariantMap` (`komport-session-replay-simulation-architecture.md:218-228`), the old magic (`327-331`), and source zero examples (`komport-multiport-sniffer-time-alignment.md:125-147,311-325`). | **Accept, broaden A14.1.** Mark each conceptual template/example as superseded by the ADR, including the later source-zero examples, not only the first occurrence. | **DOCUMENTATION** |
| **F15 – M8 source-ID wording** | **Valid.** ADR-002 reserves zero and requires physical sources non-zero (`41-43`). | **Accept A15.1.** Also ensure failed-open `Error` events use configured source ID 1, rather than silently using reserved zero. | **LOW** |
| **F16 – no production consumer** | **Valid.** M8 has no recorder or production event consumer, and current serial tests cover only legacy behavior (`tests/tst_serial.cpp:36-45`). | **Reject the legacy cross-check as universal proof.** It conflicts with the intended RX overflow/timer divergence in F11. Correct text: “Add a test-only event collector. Verify event behavior directly using a deterministic transport double and use legacy-signal comparison only in a no-overflow, fully-flushed fixture. The test collector is not production code and no production component depends on it.” | **TEST GAP** |

## Additional findings missed by the pre-review

### A1 — The proposed F1 amendment conflicts with the architecture documents and lacks a clock-domain schema

**Evidence:** The multi-source architecture says same-clock session time is source time minus a common epoch (`komport-multiport-sniffer-time-alignment.md:1151-1188`), while A1 proposes numeric identity. ADR-006 requires “capture clock domains” but does not define a stable identifier or a source-to-session reference pair (`ADR-006:50-58`).

**Impact:** Future same-host merging cannot distinguish clock domains or reproduce the mapping that made `timestampNs` meaningful.

**Minimal fix:** Apply the corrected F1 text; add mandatory `clockDomainId` and the exact source/session reference pair to ADR-006’s header schema.

**Category:** **BLOCKER**

### A2 — `QJsonObject` is unsafe for arbitrary 64-bit timing metadata

**Evidence:** ADR-002 freezes `QJsonObject metadata` (`ADR-002:28-37`), while ADR-008 expects mappings with nanosecond reference points, offset, scale, and uncertainty (`ADR-008:24-28`). JSON numeric values are IEEE-754 doubles and cannot represent every `qint64` exactly.

**Impact:** A future clock mapping stored in generic JSON metadata can silently lose nanosecond precision after `2^53` ns (about 104 days).

**Minimal fix:** Specify that integer nanosecond values in JSON/header metadata are canonical decimal strings, or define a binary/versioned mapping record rather than placing arbitrary mappings in `QJsonObject`.

**Category:** **HIGH**

### A3 — The SessionEvent invariants are not enforceable or testable as stated

**Evidence:** `SessionEvent` is a fully mutable public struct (`ADR-002:21-37`) while the ADR says it is immutable after emission (`53-54`). It defines no constructor, validator, or invalid-enum policy. SPEC-M8 nevertheless requires “value semantics for every event type/direction invariant” (`SPEC-M8:185-190`).

**Impact:** “Immutable,” non-empty-data, empty-nondata, valid direction/type, source-ID, and non-negative time cannot be mechanically verified as a contract.

**Minimal fix:** Define a `SessionEvent` validation/factory contract: valid enum values; Data requires non-empty payload and Tx/Rx; non-data requires empty payload and None; source ID rules; non-negative timestamps. State that consumers receive immutable copies/`const SessionEvent`, and that the controller is the only M8 producer.

**Category:** **HIGH**

### A4 — Controller/transport QObject ownership and destruction order are unspecified

**Evidence:** `KomportDoc` owns the current serial transport by value (`komportdoc.h:109-115`), while the spec only says it “owns the transport and SessionController” (`SPEC-M8:92-98`). Qt child destruction occurs in the QObject base destructor, after C++ members are destroyed.

**Impact:** If the controller is merely a `KomportDoc` QObject child and its destructor touches a member `KomportSerial`, it can dereference a destroyed transport. Close-event destruction is real (`komport.cpp:918-932`; `tests/tst_windowlifetime.cpp:113-139`).

**Minimal fix:** Specify one safe layout: e.g. `KomportDoc` stores controller in a `std::unique_ptr` destroyed in its destructor body before `mSerial`, or declares controller as a member constructed after `mSerial` so it is destroyed first. Controller destruction must not emit events or invoke its transport.

**Category:** **HIGH**

### A5 — Existing open/configuration behavior creates duplicate applications and ambiguous event ordering

**Evidence:** `open()` calls `applyPortSettings()` then emits the self-connected `settingsChanged()` (`komportserial.cpp:36-39,64-72`), resulting in a second apply. Settings can fail while the port remains open because `applyPortSettings()` does not close it (`115-169`).

**Impact:** “Exactly one configuration observation” cannot be achieved without changing the actual orchestration. It is also undefined whether `TransportOpened`, configuration, and error occur in that situation.

**Minimal fix:** Define one explicit `open-and-configure` transaction: port open success emits `TransportOpened`; one configuration-result event follows; a partial/failed application emits an error after that result. Remove internal reliance on `settingsChanged()` as the application trigger.

**Category:** **BLOCKER**

### A6 — “Byte-exact” needs an explicit capture-point limitation

**Evidence:** RX is only what `readAll()` returns after a `readyRead` signal (`komportserial.cpp:237-245`). TX is what `QSerialPort::write()` accepted (`205-220`), with no `waitForBytesWritten()` or physical-delivery acknowledgment. SPEC-M8 itself notes this risk but only in the risks table (`223-229`).

**Impact:** Calling this “wire truth” or implying electrically exact TX is false; port closure can discard bytes buffered below the application capture point.

**Minimal fix:** Put this in ADR-003/SPEC-M8 invariants: “byte exact at the application transport-observation boundary: RX `readAll()` chunks and bytes accepted by `write()`. It is not a logic-analyzer/electrical delivery record.”

**Category:** **HIGH**

### A7 — The legacy RX buffer crosses transport activations

**Evidence:** `close()` does not clear `mRxBuffer` (`komportserial.cpp:75-80`), while the timer later emits its contents (`255-269`).

**Impact:** Existing UI/emulation can render pre-close bytes after a reopen. Any resulting emulation reply could become TX in a later activation, while the corresponding RX event belongs to the former activation.

**Minimal fix:** Explicitly choose one behavior in M8: preserve and document this legacy compatibility behavior, or clear the buffer on close as a reviewed terminal-behavior change. In either case, event activation generation must remain correct.

**Category:** **MEDIUM**

### A8 — Partial-write and chunk-boundary acceptance tests need deterministic seams

**Evidence:** Existing tests explicitly state that a genuine partial hardware write is not reliably testable in the environment (`tests/tst_serial.cpp:72-81`). PTYs do not deterministically select read/write chunk boundaries.

**Impact:** SPEC-M8’s requested partial-write and exact-observation tests cannot be proven only through its proposed PTY integration suite.

**Minimal fix:** Add a deterministic fake `ITransport` for controller tests and extract/inject the write-result handling needed to test the `KomportSerial` accepted-prefix behavior. Keep PTYs for end-to-end binary preservation.

**Category:** **TEST GAP**

### A9 — ADR-003’s “no bypass” rule needs a compatibility exception scoped to M8

**Evidence:** Existing widgets and transfer code directly obtain `KomportSerial` and invoke legacy methods (`komportview.cpp:88-92,506-508`; `komport.cpp:1250-1276`; `komporttransfer.cpp:26-65`). SPEC-M8 deliberately preserves them (`22-24,82-84`).

**Impact:** The ADR’s absolute “No new feature may bypass `ITransport` or `SessionController`” (`ADR-003:51-53`) is ambiguous when the same object is both `KomportSerial` and the local transport implementation.

**Minimal fix:** State: “M8 legacy callers may continue to call compatibility methods on the local transport, provided every such call routes through the single observed write primitive. M8 adds no new UI-side transport API and no new consumer may derive session data from character signals.”

**Category:** **DOCUMENTATION**

## Blocking items, in priority order

1. **F1/A1 and A1:** establish one coherent source-clock, session-origin, clock-domain, and header-reference contract.
2. **F3, F9, F10:** define controller-session versus transport-activation lifecycle, failed-open event semantics, late-signal handling, and sequence behavior.
3. **F2, F4, F8, A6:** define the complete observation boundary: all TX entry points, accepted-prefix semantics, non-reentrant FIFO delivery, and the explicit non-electrical capture point.
4. **F6 and A5:** define one configuration transaction with read-back effective values, partial-apply behavior, error order, and no duplicate apply.
5. **A4:** specify safe controller/transport lifetime and destruction ordering.
6. **F7:** resolve the replay event-source boundary before accepting ADR-003 and ADR-007 together.
7. **A2 and A3:** repair the frozen public-event/header contract so timing metadata remains precise and invariants are objectively testable.

The remaining F5, F11–F16 and A7–A9 should be resolved in the same amendment pass, but are not independently fundamental contract blockers.

## What I could not verify

I did not modify files or run a build/test suite. I could not verify Qt 6.3 compatibility, PTY behavior on this host, physical serial partial-write behavior, or any M8 runtime behavior because no M8 `SessionEvent`, `ITransport`, or `SessionController` implementation exists yet.