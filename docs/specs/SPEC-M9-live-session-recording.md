# Implementation Spec: M9 Live Session Recording

Status: Accepted (independent review rounds 1–5 closed on 2026-09-18; review
record in `docs/reviews/2026-09-18-M9-gate-review*.md`)

Date: 2026-09-18

## 1. Objective

Persist a live session's event stream to a `.kpsession` v1 file, byte-exactly and
ordered, without changing visible terminal behaviour, without buffering the
session in memory, and without introducing a reader, replay, decoder or any
output path to hardware.

## 2. Scope

Included:

- A `SessionRecorder` that consumes only `SessionController::eventObserved` and
  writes ADR-006 v1 records for every accepted event.
- The `.kpsession` v1 header, written per ADR-006's normative v1 writer profile
  from the live session's own facts (source descriptor, clock domain with the
  anchor reference pair, applied configuration snapshot).
- The file lifecycle: explicit start while the session is live with the header
  written at once, one record appended per event, stop and finalise, damaged-state
  handling.
- The controller's read-only clock-domain reference accessor (ADR-010 D8) and its
  recording in SPEC-M8 §6.2.
- Ownership in `KomportDoc` (destruction order controller → recorder → transport)
  and the QtCore-only constraint on the recorder.
- Application wiring: a new checkable action `Record Live Session...`, the
  recording indicator, and the finalisation and error reporting through the
  existing translated status mechanism.
- Tests that verify the written bytes structurally and byte-exactly, the
  streaming property, the failure paths and the recovery rule.

## 3. Non-goals

- No reader/loader (`SessionReader`), no load/save UI for offline sessions, no
  passive replay: M10.
- No decoder framework, no decoded output in the file: M11.
- No active TX replay or simulation: M12 and later (ADR-007).
- No change to the existing text logger or to its `Record Session...` action,
  which stays wired to the character signals exactly as it is; the new action of
  §5.7 is additive and separately named.
- No change to the legacy RX buffer, the character signals, or the existing
  `.charset`/profile formats.
- No CRC, index, footer or compression (ADR-006 defers these to later versions).
- No multi-source recording, no alignment mapping: ADR-008 and M14.
- No new format and no format amendment beyond ADR-006's normative v1 writer
  profile; changing a binary width would be a format-version change.

## 4. Current state (M8, verified)

- `SessionController` emits ordered `SessionEvent` values through `eventObserved`
  with source ID 1, `sequence` assigned at emission time, `sourceTimestampNs`
  preserved and `timestampNs` mapped per ADR-005; the anchor event of the clock
  domain carries session time 0.
- Record-level events exist for open, close, configuration change, line state and
  errors; the same stream carries RX and TX data with byte-exact payloads.
- The controller's state is observable (`State::Idle` / `State::Live`).
- The controller is owned by `KomportDoc`; the transport is a by-value member
  destroyed after the controller; `~KomportSerial` emits no event.
- `KomportApp::closeEvent()` initiates closure by calling `KomportDoc::closeSession()`
  before the document is torn down, which is what makes a terminal `TransportClosed`
  event recordable.
- While the transport is closed, a configuration request is stored without a
  hardware transaction (`storedOnly`), so "requested" and "applied" differ.
- Nothing consumes the stream yet; the recorder is its first consumer.

## 5. Design

### 5.1 Components

```text
KomportDoc
  +-- KomportSerial          (transport, by value, destroyed last)
  +-- SessionController      (owned, destroyed first)
  +-- SessionRecorder        (owned, destroyed after the controller)

SessionRecorder (QtCore only)
  |-- SessionRecordCodec     (header and record assembly, byte level)
  +-- SessionFileSink        (internal seam over the file, §5.9)
```

The recorder receives the controller as its event source and, when recording
starts, the target path and the applied configuration snapshot as values. It never
owns the transport and never calls into it.

### 5.2 Data flow

```text
transport observation -> SessionController -> eventObserved -> SessionRecorder
                                                                  |
                                                    SessionRecordCodec
                                                                  |
                                                         SessionFileSink
                                                                  |
                                                            .kpsession file
```

Recording never feeds back: nothing the recorder does can produce an event,
because it writes to a file and nowhere else (ADR-007).

### 5.3 Record assembly (per event, one record)

Field order and width are ADR-006's:

| Record field | Source | Encoding rule |
| --- | --- | --- |
| `recordLength` | computed | bytes after this field: `40 + metadataLength + payloadLength` |
| `eventType` | `event.type` | ADR-002's numeric value, `u16` |
| `direction` | `event.direction` | ADR-004's numeric value, `u8` |
| `flags` | constant | `0` in v1 |
| `sequence` | `event.sequence` | `u64` |
| `sourceId` | `event.sourceId` | `u32`, non-zero |
| `sourceTimestampNs` | `event.sourceTimestampNs` | `i64`, preserved, never normalised |
| `timestampNs` | `event.timestampNs` | `i64`, the ADR-005 session time |
| `metadataLength`, `payloadLength` | computed | `u32` |
| metadata JSON | `event.metadata` | UTF-8; an empty object is encoded as length 0 (ADR-006); 64-bit JSON values as canonical decimal strings |
| payload | `event.payload` | verbatim bytes, unchanged |

Normative rules:

- One accepted event produces exactly one record; records are never split and
  never coalesced, so chunk preservation survives recording.
- The record is assembled in one buffer with **checked arithmetic** and written
  with one write call. A header above 16 MiB, a `metadataLength` or
  `payloadLength` above `u32`, or a record body above 64 MiB is a write refusal:
  the event is not written, the recording enters the damaged state, and the report
  names the event's sequence number and the offending sizes. (These are the limits
  ADR-006 requires a reader to enforce; the writer applies them so it cannot
  produce a file a conforming reader must reject.)
- No field is added, dropped or rewritten; the payload bytes and
  `sourceTimestampNs` are written as observed.
- An event whose `isValidSessionEvent()` is false is a producer defect, not data
  to persist: the recorder reports it, stops as damaged, and writes no record for
  it.

### 5.4 Header assembly

Written once, at start, immediately after the file is created, exactly per
ADR-006's **normative v1 writer profile** (member names, types and the fixed local
profile values are defined there and are not restated as negotiable here):

| Header part | Value in M9 |
| --- | --- |
| `format`, `version`, `created`, `application` | ADR-006's profile: the format name, the number 1, an ISO-8601 UTC timestamp, and the writing application's name and version |
| `sources[]` | exactly one entry with `sourceId` 1, `clockDomainId` `local-process-monotonic-v1`, `name` `local serial`, `transport` `serial`, and `configuration` = the applied configuration snapshot the application supplies at start. Its nested members are ADR-006's fixed schema, exhaustively: `requested` and `effective` with exactly `endpoint`, `baudRate`, `dataBits`, `stopBits`, `parity`, `flowControl` as strings (endpoint unredacted), `localBuffering` with exactly `rxQueue` and `flushRate` as numbers, `compatibility` with exactly `startBits` as a string, and no additional member anywhere. A snapshot carries no per-transaction fields (`applyStatus`, `changedGroups`, message) |
| `clockDomains[]` | exactly one entry with `id` `local-process-monotonic-v1`, `kind` `"process-monotonic"`, and `reference` = the pair read from the controller accessor of §5.6: `sourceTimestampNs` as a canonical decimal string and `sessionTimestampNs` as the decimal string `"0"` |
| decoder hints, notes | none in M9 |

Normative consequences: the reference pair comes from the controller, never from
the event the recorder happens to see first, so a recording started mid-session
begins at session time greater than zero while its header carries the domain's
true anchor (session time exactly 0); the recorder writes the header at start,
where the accessor is valid because the live precondition guarantees that the
domain has an anchor; no alignment mapping and no decoded output are written; a
clock domain is never written without its reference pair (ADR-006).

### 5.5 States and transitions

```text
Stopped --start (only while the session is Live)--> Live
Live    --stop or application close--------------> Stopped
Live    --write/flush failure, invalid or oversized event--> Damaged
Damaged --report finalisation--------------------> Stopped
```

| State | Meaning |
| --- | --- |
| `Stopped` | no recording; no file; the recorder holds no path |
| `Live` | the file exists with its complete header, and every event is appended as it arrives. A recording may legitimately have zero records |
| `Damaged` | a write, flush, validation or size failure occurred; nothing further is written until recording is stopped |

There is deliberately no intermediate state between "recording off" and "recording
on with a file": starting outside the live state is refused, and a start in the
live state writes the header immediately, so the recorder is never switched on
without a decision having been made.

A start request while not `Live` is refused without creating a file, with the
reason reported through the status mechanism. Start after a damaged recording is
allowed: the previous recording is stopped and finalised, and a new file is
started. Stopping a recording that received no event reports "no session data
recorded"; the file it created stays, with its complete header, as a valid v1 file
with zero records (ADR-010 D1).

### 5.6 Lifecycle

| Step | Behaviour |
| --- | --- |
| start (user action, session live) | the recorder records the target path and the configuration snapshot the application supplies, creates the file, writes the magic and the complete header in one call and flushes it before declaring the recording `Live`; state `Live` |
| each event | one record appended immediately |
| flush | a one-second timer on the session thread, armed by the first unflushed write of a live recording and firing at most once per second per burst; the final flush happens on stop (ADR-010 D2/§8). Retention is therefore bounded even when the line goes idle and no further event arrives. A flush that fails while the start block is written - before the recording is `Live` - refuses the start and leaves the recorder `Stopped`: there is no recording to damage yet (ADR-010 §8) |
| stop (user action) | final flush, close, report path, record count, bytes, duration, state |
| stop with no event recorded | report "no session data recorded"; the file with its complete header remains a valid v1 file with zero records |
| application close | the application's close path calls `KomportDoc::closeSession()`, which closes the transport first (while controller and recorder are live) and then finalises a running recorder, returning its report; that path reports through the log instead of the status bar, because the window is closing (§5.7) |
| short or failed write | stop as damaged, report with the byte counts; the complete prefix stays valid |
| flush failure (during a `Live` recording) | damaged transition, reported like a write failure; a flush that fails while the start block is written refuses the start instead (§5.8, ADR-010 §8) |
| invalid event, oversized event | damaged transition; the event is not written, its sequence number and sizes are reported |
| the transport closes or reopens during recording | no special case: `closed`, `opened` and the resulting configuration event are ordinary records; one recording covers as many activations as the session has |
| the process or host dies | a process crash loses the not-yet-flushed records (bounded by the flush cadence) and can leave the final record truncated; a host or power failure can additionally lose already-flushed records; a failure inside the header write at start can leave a file with an incomplete header, which does not load - all bounds are stated in ADR-010 §8 and covered by §7's recovery tests |

### 5.7 Application wiring

- A new checkable action `Record Live Session...` starts and stops `.kpsession`
  recording; its checked state is the recording indicator.
- The existing text logger action `Record Session...` and the logger behind it are
  unchanged and keep their own dialog, wiring and behaviour.
- Start uses a save dialog with `QDir::currentPath()` as the directory and a
  suggested name carrying date and time, extension `.kpsession`; overwriting an
  existing file requires the dialog's normal confirmation.
- Start/stop/failure outcomes are reported through the existing translated status
  mechanism (`tr()` and the status label pattern); no new notification mechanism.
  One exemption, because the surface would not be seen: on the application-close
  path the recording is finalised silently. A damaged close is logged as an
  untranslated diagnostic warning that carries the same finalisation facts (path,
  complete record count, accepted bytes, duration, reason); a clean close logs
  nothing, and in both cases the facts are also available as the recorder's last
  report while the recorder lives, so no fact is lost - only the visible message
  that no one would still be reading.
- Recording is never started automatically and no recording state is persisted
  across runs.

### 5.8 Ownership and destruction

`KomportDoc` owns the recorder beside the controller and destroys the controller
first, then the recorder, then the transport. `~KomportSerial` emits no event, so
a direct document destruction records no terminal session event.

On the normal application-close path `KomportApp::closeEvent()` initiates closure
by calling the document's `closeSession()` operation. That operation closes the
transport first, while controller and recorder are still alive - which is when the
terminal `TransportClosed` event is recorded - and only then finalises a running
recorder, returning its report (an empty report when nothing was being recorded).
The sequence is a document operation so that the ordering itself is testable
without a window; the application keeps the timing decision and the presentation of
the outcome. The close ordering is normative for this milestone.

### 5.9 File-sink seam (internal, testable)

The file I/O is behind an internal seam so the failure paths are testable without
a faulty filesystem:

```cpp
/** Internal seam: the file operations the recorder needs. Not public API. */
class SessionFileSink {
public:
  virtual ~SessionFileSink();
  virtual bool open(const QString &path) = 0;
  virtual qint64 write(const QByteArray &bytes) = 0;   // bytes accepted, < 0 on error
  virtual bool flush() = 0;
  virtual void close() = 0;
};
```

`SessionRecorder` takes an optional sink in its constructor; the default
implementation wraps `QFile`. Tests inject a scripted sink that can accept a short
write, fail a flush and fail a write.

Two further seams make the flush and the report deterministic in tests: a clock
seam (an injectable monotonic time source, default the production clock) for the
reported duration, and a scheduler seam (an injectable "run this callback after
this delay" mechanism, default `QTimer` on the recorder's thread) so a test can
fire the periodic flush explicitly instead of waiting for wall-clock time. Neither
seam is part of the class's documented public surface, and neither is used by
production code other than as its default.

## 6. Invariants

- Byte-exactness: the payload bytes in the file are the bytes the transport
  observed/accepted; the recorder never normalises, translates (no charset
  translation) or trims.
- Stream purity: the recorder consumes `eventObserved` only, never the character
  signals and never the legacy RX buffer.
- Ordering: records appear in the order the controller emitted them.
- Memory: bounded by one record, independent of session length.
- Bounds: no record or header the writer produces can exceed ADR-006's limits.
- No output path other than the file: the recorder cannot make the transport
  transmit anything (ADR-007).
- Single thread: the write happens on the session's thread; the recorder
  introduces no thread and no queued connection.

## 7. Testing strategy

Test names in this section are the design-time plan. Where the shipped suite uses a
different name, §8 carries the shipped name, and the M9 implementation self-review
holds the full mapping between plan, criterion and shipped test.

Test-only structural reader (ADR-010 D3): a minimal parser under `tests/` that
validates magic, version, header encoding and length, record length consistency,
`flags == 0`, the header profile's member names and the reference pair, and that
extracts payloads. It links no production codec code, is not public API and is not
M10's reader.

Unit tests of the codec (no file I/O):

| Test | Asserts |
| --- | --- |
| `headerProfileMatchesAdr006` | the fixed member names, types and local profile values of the normative v1 writer profile, including the exhaustive nested `configuration` schema (`requested`/`effective` with exactly the six string members, `localBuffering` with exactly `rxQueue`/`flushRate` as numbers, `compatibility` with exactly `startBits`) and the absence of any additional member |
| `recordPrefixIsFortyFourBytesInTheAgreedOrder` | field order, widths and the `recordLength` arithmetic |
| `emptyMetadataIsEncodedAsLengthZero` | `{}` becomes `metadataLength == 0` |
| `sixtyFourBitJsonValuesAreCanonicalDecimalStrings` | `sequence` and both timestamps above 2^53 survive as decimal strings |
| `oversizedHeaderIsRejected` | a header above 16 MiB is refused, not written |
| `oversizedRecordIsRejected` | metadata or payload above `u32`, and a record body above 64 MiB, are refused |
| `lengthArithmeticIsChecked` | the `recordLength` computation cannot overflow silently |

Recorder tests (scripted event source, injected sink, injected clock):

| Test | Asserts |
| --- | --- |
| `oneRecordPerAcceptedEventInOrder` | record count equals event count, sequences in order |
| `payloadsSurviveByteExactly` | embedded NUL and all 256 values round-trip via the test reader |
| `metadataIsWrittenVerbatim` | metadata JSON is unchanged, including an empty object |
| `startIsRefusedUnlessTheSessionIsLive` | idle start refused, no file created, reason reported |
| `headerIsWrittenAtStart` | after start the file exists and begins with magic, version and the complete header of ADR-006's profile, with the reference pair from the controller accessor |
| `headerIsOnDiskWithoutAnyEvent` | start, receive no event, and read the file back from disk through a separate handle: the complete header is already there because the start flush happened before the recording was declared live |
| `recordingWithZeroRecordsIsAValidFile` | start, stop without any event: the file parses as a valid v1 file with zero records, and the report says no session data was recorded |
| `midSessionStartCarriesTheTrueAnchorReference` | header reference equals the controller's accessor, `sessionTimestampNs` `"0"`, first record's `timestampNs` greater than zero |
| `shortWriteDuringTheHeaderWriteLeavesNoReadableFile` | the test reader reports an invalid header, not a recovered session |
| `shortWriteAfterAValidPrefixIsReportedAsDamaged` | prefix records are complete and recovered, the tail is ignored, the state is damaged |
| `flushFailureEntersTheDamagedState` | a flush failure during a record is reported and stops the recording |
| `pendingTailIsFlushedWithinOneSecondWithoutFurtherEvents` | write one event, then let the line go idle: the injected scheduler fires the armed flush and the sink receives it, so the retained tail is bounded without another event |
| `burstFlushesAtMostOncePerSecond` | several writes inside one interval produce exactly one flush, not one per write |
| `timerFlushFailureEntersTheDamagedState` | a flush that fails when the timer fires is a damaged transition like a failing write |
| `oversizedEventStopsAsDamaged` | the event is not written and its sequence and sizes are reported |
| `invalidEventStopsAsDamaged` | a non-conformant event is not written |
| `stopReportsRecordCountBytesAndDuration` | the finalisation report is exact, with the injected clock |
| `flushIntervalBoundsTheBufferedTail` | with the injected clock and scheduler, flushes happen at most once per second and never later than one second after the first unflushed write |
| `stopThenStartWritesASecondFile` | a second recording after a stop is a new file |
| `theRecorderNeverTouchesTheTransportOrTheCharacterSignals` | the transport double records no call; a character-signal observer sees no change; no automatic start |
| `truncatedFileIsRecoveredToItsCompletePrefix` | the test reader reports the complete prefix as recovered (ADR-006's rule) |

Document-level tests: `theDocumentOwnsTheRecorderInTheSpecifiedOrder`,
`applicationCloseRecordsTheTerminalClosedEvent` (close the transport, then
finalise) and `destroyingTheDocumentWithoutCloseRecordsNoTerminalEvent`.

Pty end-to-end: `livePtySessionIsRecordedByteExactly` - a real pty session records
live events; the concatenated payloads equal the sent and received byte streams,
and the file parses with the test reader.

Regression: the existing full suite passes, and the text logger's behaviour and
wiring are unchanged.

## 8. Acceptance criteria

Test names are those of the shipped suite (`tests/`); where a planned name and the
shipped name differ, this list carries the shipped one, and the M9 implementation
self-review holds the full mapping. All of them pass.

- [x] A live session produces a `.kpsession` v1 file that satisfies ADR-006's
  normative v1 writer profile and record layout - verified by
  `headerProfileMatchesAdr006`, `startBlockPrefixIsExactlyTheAdr006Fixture`,
  `recordBytesAreExactlyTheAdr006LittleEndianFixture`, `headerIsWrittenAtStart`,
  `theStartFlushMakesTheHeaderVisibleThroughASecondHandle`,
  `recordPrefixIsFortyFourBytesInTheAgreedOrder`.
- [x] Every accepted event is one record; payloads and both timestamps are preserved
  exactly, including values above 2^53 in the JSON parts -
  `oneRecordPerAcceptedEventWithByteExactPayloads`, `metadataIsWrittenVerbatim`,
  `sixtyFourBitJsonValuesAreCanonicalDecimalStrings`,
  `jsonSizeCounterIsExactForEveryValueShape`.
- [x] The recorder holds no session-sized state; a 64 KiB byte-wise upload produces
  one observation per accepted write. The no-session-sized-state half rests on
  static inspection - the class has no container member and appends each record - not
  on a memory measurement (self-review gap 4) -
  `theProcessBufferIsBoundedByOneTimerPerBurst`,
  `byteWiseSixtyFourKiBUploadKeepsOneObservationPerAcceptedWrite`, and the pty volume
  proof `aRecordedPtySessionIsByteExactAndChunked`.
- [x] Recording starts only while the session is live; the file carries its complete
  header from the start, with the domain's true anchor and an applied configuration
  snapshot - `startWritesACompleteHeaderAndNeedsALiveSession`,
  `midSessionStartWritesTheTrueAnchorReference`,
  `clockDomainReferenceStartsInvalidAndCarriesTheAnchor`,
  `clockDomainReferenceUsesAFailedOpenAsTheAnchor`.
- [x] The writer's own buffer is bounded: retention never exceeds one second even
  when the line goes idle, and a burst flushes at most once per second -
  `theProcessBufferIsBoundedByOneTimerPerBurst` (the planned names
  `pendingTailIsFlushedWithinOneSecondWithoutFurtherEvents`,
  `burstFlushesAtMostOncePerSecond` and `flushIntervalBoundsTheBufferedTail` were
  consolidated into it: its assertions cover no re-arm inside a burst, the interval
  as the flush deadline, and a flush on the armed timer without any further event),
  `aClockThatStartsAtZeroReportsTheRealDuration`.
- [x] Failure paths stop the recording and report; a damaged file keeps its complete
  prefix; the crash bounds of ADR-010 §8 are stated and tested -
  `aShortHeaderWriteRefusesTheStartAndLeavesNoLoadableFile`,
  `aFailedStartFlushRefusesTheStartAndClosesTheSink`,
  `aShortWriteEndsTheRecordingAsDamagedAndKeepsTheCompletePrefix`,
  `aShortWriteReportsEveryAcceptedByte`, `aFailedFlushEndsTheRecordingAsDamaged`,
  `anExplicitStopWhoseFinalFlushFailsReportsItWithoutASignal`, and the reader's
  recovery of a truncated final record in `theReaderRejectsMalformedWriterOutput`.
- [x] No record or header the writer produces can exceed ADR-006's limits, and an
  offending event is refused with its sequence and sizes reported -
  `oversizedHeaderIsRejected`, `oversizedRecordIsRejected`, `lengthArithmeticIsChecked`,
  `headerLimitBoundaryIsExact`, `largeAsciiMetadataIsAcceptedWhenItFits`,
  `anEventThatCannotBeEncodedReportsItsSequenceAndSizes`. An event above 64 MiB is not
  driven through the recorder, because that needs a test allocating 64 MiB; the limit
  itself is covered at the codec level, and the gap is stated in the self-review.
- [x] Recording is an explicit user action in a separate action while the text logger
  stays unchanged; the recorder never calls the transport and never transmits -
  `anIdleSessionRefusesTheRecordingStart`,
  `aRecordingShowsItsIndicatorAndStopsWithAReport`,
  `theRecorderNeverCallsIntoTheTransport`, and the chunking property of
  `aRecordedPtySessionIsByteExactAndChunked`, which fails if the legacy character
  signals were the source.
- [x] The existing full `ctest` suite passes, warning-free under `-Wall -Wextra`
  (17/17 targets, 0 warnings, 2026-09-18; the Qt 6.3 line stays open, see the
  self-review §4).

## 9. Risks

| Risk | Mitigation |
| --- | --- |
| Recording costs one record per accepted write for byte-wise uploads | Accepted deliberately (SPEC-M8 §13 sizing note); the streaming design keeps memory flat, and the cost is documented for M10's loader |
| A crash or power failure loses or truncates data | The three bounds are stated normatively (ADR-010 §8) and each has a test; no "recovered" claim is made that the design cannot keep |
| The recorder drifts into doing the reader's job | The reader is M10; the test-side parser is constrained by D3 and lives in `tests/` |
| The D8 accessor is mistaken for a general controller API | It is specified, read-only, recorded in SPEC-M8 §6.2 as an M9 addition, and asserted to return `valid == false` before the first accepted event |
| The new action collides with the text logger | The logger action and its wiring are untouched; the new action has its own name, its own dialog filter and its own status messages, and a regression test covers the logger |
| Metadata grows unbounded (a configuration snapshot per change) | The metadata is the producer's JSON, unchanged, and the writer enforces ADR-006's per-record limits |
| The flush window hides data loss from the user | The window, its periodic timer and the fact that it bounds only the process buffer (not durable storage) are documented in ADR-010 §8, in the code comment at the flush policy, and asserted by the idle-retention and burst tests |

## 10. Decisions

All decisions this specification depended on were resolved in ADR-010 by the gate
review round 1: D1 the empty-recording question is closed by the live-only start
(the header is written at start, so a zero-record recording is a valid file); D2 a
one-second flush interval
with stated durability bounds; D3 a test-only structural reader; D4 the fixed
local profile values; D5 `QDir::currentPath()` with a timestamped suggested name
and normal overwrite confirmation; D6 a separate `Record Live Session...` action;
D7 the configuration snapshot supplied by value at start; D8 the read-only
controller accessor. No decision remains open.

## 11. Implementation steps

1. `SessionRecordCodec`: header and record assembly with checked arithmetic and
   byte-level unit tests against ADR-006 (no file I/O).
2. `SessionController::clockDomainReference()` (D8) and the SPEC-M8 §6.2 wording,
   as its own small reviewed slice, because it touches M8's frozen controller
   surface.
3. `SessionRecorder` with the file-sink and clock seams, the state machine and the
   error handling, plus the test-side structural reader and the recorder tests
   (including the mid-session start, the failure paths and the recovery rule).
4. `KomportDoc` ownership, destruction order, the `closeSession()` operation (the
   normative close ordering, as a testable document operation) and the
   document-level tests.
5. Application wiring: the `Record Live Session...` action, the indicator, the
   delegation of the close path to `KomportDoc::closeSession()` and the outcome
   reporting.
6. Full pty end-to-end recording test and the complete suite; warning-free build.
7. Self-review against this specification, then the independent review.
