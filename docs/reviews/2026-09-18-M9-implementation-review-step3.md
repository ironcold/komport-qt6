## Round 1 review — not ready to commit

This slice has **four HIGH findings**. Once fixed, it should remain one cohesive step-3 commit; do not split the recorder from its internal test seams and tests.

### HIGH — damaged recordings cannot be restarted as specified

[SPEC-M9 §5.5](docs/specs/SPEC-M9-live-session-recording.md:180) requires `start()` after damage to finalise the prior file and begin a new one. [sessionrecorder.cpp](komport/sessionrecorder.cpp:132) refuses every state except `Stopped`, including `Damaged`.

`Damaged` is otherwise observable and unreachable from `Stopped`: [endDamaged](komport/sessionrecorder.cpp:264) is reached only from the `Live` event/timer paths, and [state()](komport/sessionrecorder.h:156) exposes it. But the required recovery transition is missing. Also, `Stopped` retains `mPath` after [stop()](komport/sessionrecorder.cpp:314), contrary to the specified stopped-state meaning.

Minimal fix: in `start()`, if state is `Damaged`, call `stop()` first, then continue normal start validation; clear the active path and per-recording state after finalising while retaining `mLastReport`.

### HIGH — reported “duration” is not a recording duration, and the required clock seam is absent

[SessionRecordingReport](komport/sessionrecorder.h:110) defines `sessionSpanNs` as the last event’s session timestamp. It is set from the event at [sessionrecorder.cpp](komport/sessionrecorder.cpp:221) and returned at [lines 303–312](komport/sessionrecorder.cpp:303). The test explicitly cements this incorrect behaviour at [tst_sessionrecorder.cpp](tests/tst_sessionrecorder.cpp:529).

That value is time since the controller’s anchor, not duration of the recording; a mid-session recording therefore reports unrelated pre-recording time. SPEC-M9 requires a report containing a duration, and §5.9 expressly requires an injectable monotonic clock for it ([SPEC](docs/specs/SPEC-M9-live-session-recording.md:248)); ADR-010 likewise says “session duration” ([ADR](docs/architecture-decisions/ADR-010-live-session-recording.md:180)). No clock seam exists in the public or private recorder API.

Minimal fix: add an internal injectable monotonic-clock seam, capture its value after the successful start flush, and compute `durationNs = finalisationNow - recordingStart`. Exercise clean stop and damage reporting with a manual clock. Do not use event timestamps for this field.

### HIGH — damaged reports undercount bytes actually accepted by the sink

`writeUnit()` discards the short-write byte count after forming text ([sessionrecorder.cpp](komport/sessionrecorder.cpp:230)); `mBytes` increases only after a whole record succeeds ([lines 215–223](komport/sessionrecorder.cpp:215)). Thus a short record write leaves physical bytes in the file but reports only the preceding complete prefix. ADR-010 requires the report to name “the bytes written” ([ADR-010](docs/architecture-decisions/ADR-010-live-session-recording.md:80)); SPEC-M9 likewise requires byte counts for short writes ([SPEC](docs/specs/SPEC-M9-live-session-recording.md:197)).

The short-write test proves the sink has the partial tail but never compares either damage report’s `bytes` to `writtenBytes` ([test](tests/tst_sessionrecorder.cpp:415)).

Minimal fix: return a write outcome containing accepted bytes; increment the file-byte counter by every non-negative accepted count, while incrementing `records` only after a full record. Assert the damage signal and subsequent `stop()` report the sink’s accepted-byte count.

### HIGH — invalid/oversized-event reports omit required diagnostics

When encoding refuses an event, the recorder forwards only `record.reason` ([sessionrecorder.cpp](komport/sessionrecorder.cpp:207)). Codec refusal messages contain no event sequence or actual metadata/payload sizes, e.g. [codec](komport/sessionrecordcodec.cpp:465). This fails SPEC-M9’s explicit requirement that an oversized-event report include its sequence and offending sizes ([SPEC](docs/specs/SPEC-M9-live-session-recording.md:126)).

The disclosed inability to produce a >64 MiB controller event within the test budget explains the absence of a direct test; it does not remove the reporting requirement.

Minimal fix: enrich the recorder’s encoding-failure reason with `event.sequence`, payload size, and computed metadata size before `endDamaged()`, preserving the codec’s specific limit reason. Add a deterministic test seam or a narrowly scoped test hook for an encoder refusal; an invalid event can also be emitted directly through the controller’s public signal in the test.

### MEDIUM — supposedly internal seams are exposed as public API

`SessionRecordSink`, `SessionFlushScheduler`, and the injected public constructor are in [sessionrecorder.h](komport/sessionrecorder.h:56), despite SPEC-M9 calling the sink explicitly “Not public API” and stating the clock/scheduler seams are not part of the documented public surface ([SPEC](docs/specs/SPEC-M9-live-session-recording.md:227)). The comments say they are not extension points, but C++ consumers can subclass and depend on them.

Ownership itself is sound: default seams are owned through `unique_ptr` ([header](komport/sessionrecorder.h:202)); injected seams are not deleted. However, the injected seam documentation never says they must outlive the recorder.

Minimal fix: move seam interfaces and the injection constructor into an internal/private test header, keeping only the production constructor public. Document the injected-object lifetime requirement there. The owned production timer/sink do not show a leak or double-close path.

### MEDIUM — structural reader does not perform the validation required of it

The independent reader does not share production codec code, which is good. However, it merely parses `flags` ([sessionrecordreader.h](tests/sessionrecordreader.h:113)) and never rejects non-zero flags; it accepts any JSON-object header without checking the fixed profile/reference pair ([lines 92–96](tests/sessionrecordreader.h:92)); and it does not enforce the 16 MiB header / 64 MiB record limits.

SPEC-M9 requires this reader to validate flags, header profile member names, reference pair, and structure ([SPEC](docs/specs/SPEC-M9-live-session-recording.md:271)). As written, recorder tests can pass with several malformed writer outputs.

Minimal fix: make the reader reject all of those violations and add negative assertions to recorder tests.

### TEST GAP — key recorder requirements are absent or only simulated weakly

The ten tests would pass with several wrong implementations:

- No restart-after-damage test; the current defect passes.
- No short-header-write unreadability test.
- No invalid-event or oversized-event damage/report test.
- No assertion that a partial-write report includes partial bytes.
- No manual-clock duration test; the current test asserts the wrong semantics.
- No assertion that explicit `stop()` emits no signal when its final flush fails.
- `theProcessBufferIsBoundedByOneTimerPerBurst` verifies scheduling and a manually fired callback, but without the required clock it cannot establish the “no later than one second” bound. It does, however, correctly cover a quiet period after the last event and no re-arm during a burst.
- The transport test checks no transport calls, but not the required character-signal half of `theRecorderNeverTouchesTheTransportOrTheCharacterSignals`.
- Metadata is tested only for a non-empty object, not the required empty-object record path.
- Header visibility is not checked through a separate real file handle; this is deferred appropriately only if a later disk-backed test is planned, but it is named as a recorder requirement in §7.

The disclosed oversized-event limitation is understandable but remains a **TEST GAP** until an internal deterministic refusal seam or equivalent test strategy is agreed.

## Verified conformant aspects

- Start checks state and anchor, validates/encodes the full header before opening the sink, writes and flushes before setting `Live` ([sessionrecorder.cpp](komport/sessionrecorder.cpp:132)). Idle starts do not touch the target.
- The source anchor comes from `clockDomainReference`; the emitted session anchor is fixed zero, which matches both the accessor’s M8 guarantee and ADR-006. It does not derive the anchor from the first recorded event.
- One event is assembled once and written in one call; no event container exists. A short write stops further records, leaving only a complete prefix plus at most the partial record.
- The timer policy is correctly encoded: first unflushed write arms one 1000 ms callback, bursts do not re-arm, timer-flush failure damages, explicit clean stop flushes, and damaged stop does not re-flush.
- Damage signals occur without explicit `stop()`, explicit stop returns a report without emitting, and destruction cancels/flushed/closes silently.
- The recorder is QtCore-only: the contract target compiles `sessionrecorder.cpp` with `Qt6::Core` alone ([tests/CMakeLists.txt](tests/CMakeLists.txt:59)). It consumes only `SessionController::eventObserved` and has no transport output path.

## Could not independently verify

I did not build or run tests in this read-only environment, so the reported warning-free build and 15/15 `ctest` result remain author evidence, not independent evidence. I also could not observe real `QFile::flush()` visibility through a second handle or actual event-loop timer dispatch; the static timer wiring is consistent with the policy, subject to the normal session event loop running.