# Implementation Spec: M10 Session Reader and Passive Replay

Status: Proposed

Date: 2026-09-18

Decisions: `docs/architecture-decisions/ADR-011-session-reader-passive-replay.md`
(D1-D14). This specification implements those decisions; where it and the ADR would
differ, the ADR is authoritative and this specification must be amended first.

## 1. Objective

Read a `.kpsession` v1 file back and replay it passively in the same view: load a
recorded session into one immutable offline session, show it in the existing terminal
and hex views through an explicit offline state, and distribute its stored events to
views through a read-only replay player that has no output target of any kind - with a
timing ladder (original, immediate, 0.1x/0.25x/0.5x/1x/2x/5x/10x, step mode with next
event / next RX / next TX), forward-only navigation, and no transmission path, no
decoder, no simulation and no multi-source support.

## 2. Scope

Included:

- `SessionReader`: a QtCore-only, read-only loader for ADR-006's v1 container that
  validates the start block, the header (ADR-006's normative v1 writer profile), every
  record's structure and ADR-002 event validity, applies ADR-006's recovery rule to a
  truncated final record, and produces one immutable offline session (typed header
  facts plus the ordered stored events) or a refusal with a reason.
- The typed header facts (`SessionFileInfo`, `SessionSourceDescriptor`,
  `SessionClockDomain`) and the offline session value type (`SessionFile`) - QtCore
  only, no widget or application type.
- `SessionReplayPlayer`: the read-only replay-player interface of ADR-007 with its five
  states, its operations (start/restart, play, stop, step next event / next RX / next
  TX, close), the timing ladder, the stored-timing-preserving scale arithmetic, the
  delivery signal and value-returned refusals.
- The offline state in the application: the `Load Session for Replay...` action, the
  explicit loading preconditions and their refusal, the permanent indicator
  `Offline Session — Passive Replay`, the disabled live/connection and transmit
  controls, the replay controls (timing ladder and step actions), and the read-only
  offline display adapter that renders delivered events into the existing terminal and
  hex views.
- Document ownership and the document operations that make the offline state testable
  without a window (`loadOfflineSession()`, `closeOfflineSession()`).
- The internal seams that make loading and replay deterministic in tests: the byte
  source, the scheduler and the monotonic clock.
- Tests for every property of §6, the planned names of §7, the acceptance criteria of
  §8 and the record→load→replay end-to-end proof over a real pty.

## 3. Non-goals

- No decoder framework and no decoded output: M11. "Next decoded frame" is deliberately
  **not** a step target in M10, though the delivery signal of §5.5 is the seam M11
  attaches to.
- No active TX replay, no output target, no `ITransport` selected by replay, no
  confirmation dialog for starting a replay: ADR-007 and architecture section 10; M12
  and later.
- No simulation, no fault injection, no scenario files: ADR-007 and later milestones.
- No multi-source support and no alignment mapping: a file with more than one source is
  refused (§5.3, §5.4); multi-source and the sniffer views are M14 (ADR-008).
- No arbitrary seek, no position setter, no range selection and no timeline slider in
  M10: forward-only navigation only (ADR-011 D9); a later seek is additive because the
  session is stored randomly accessible.
- No change to the `.kpsession` v1 format, no new version, no writer change: the reader
  must accept what the M9 writer produces (§5.3) and the format stays as ADR-006 fixed it.
- No change to `SessionRecordCodec`, `SessionRecorder`, `SessionController`,
  `KomportSerial`, the legacy character signals, the legacy RX buffer, the text logger,
  the `.charset`/profile formats, or `tests/sessionrecordreader.h` (which stays M9's
  test-side parser and is not this milestone's reader - ADR-011 D1).
- No export features (raw/hex/decoded export, architecture section 17): later work.
- No persistence of the offline state, of a replay position, of a timing choice or of a
  recently loaded session across runs: loading is always an explicit user action.

## 4. Current state (M9, verified)

- `SessionRecorder` writes `.kpsession` v1 through `SessionRecordCodec`: a complete
  header (flushed at start) and one record per accepted event, with the 44-byte
  little-endian prefix of ADR-006; the writer applies ADR-006's limits so it cannot
  produce a file a conforming reader must reject (SPEC-M9 §5.3/§5.4).
- `SessionRecordCodec` carries the format facts a reader needs: `magic()`,
  `kRecordPrefixSize == 44`, `kMaxHeaderBytes == 16 MiB`, `kMaxRecordBodyBytes ==
  64 MiB`, the fixed local profile values and the exhaustive `configuration` schema.
- `SessionController` exposes `state()` (`Idle`/`Live`), `sourceId()`,
  `lastEmittedSequence()`, the read-only `clockDomainReference()` (ADR-010 D8) and the
  one event signal `eventObserved`. It is the live observation path and is not a file
  consumer.
- `KomportDoc` owns the transport (by value, destroyed last), the controller (destroyed
  first) and the recorder, and exposes `closeSession()` - the normative close ordering as
  a testable document operation (SPEC-M9 §5.8).
- The application (`KomportApp`) has the `Record Live Session...` action, the permanent
  `recordingStatusLabel` indicator, the translated status mechanism (`slotStatusMsg()`,
  `tr()`) and a non-interactive `startSessionRecording(path)` entry point whose refusals
  are reported through it. `Record Session...` (the text logger) is unchanged and stays
  separate.
- The terminal (`KomportView::slotReceivedChar(char)`), the hex monitor
  (`KomportHexView::appendRx(char)`/`appendTx(char)`) and the text logger consume
  `KomportSerial`'s character signals; they are display adapters and not session
  consumers.
- `tests/sessionrecordreader.h` parses `.kpsession` v1 by hand for M9's tests: it
  validates magic, version, encoding, the header profile, record structure and `flags`,
  treats a truncated final record as recovered, and extracts payloads. SPEC-M9 D3
  constrains it to test infrastructure; its rules are the reference for §5.3's expected
  outcomes, but the production reader is a separate, independent implementation.
- `tests/CMakeLists.txt` registers tests through `komport_add_test(name)` (QTest,
  `QT_QPA_PLATFORM=offscreen`, linking `komport_core`), links the `util` system library
  for the tests that use a real pty, and builds the widget-free contract check
  (`komport_session_contract_check`, `Qt6::Core` alone) from
  `tests/session_contract_compile.cpp` plus the implementation units of the public
  session surface.
- Nothing reads `.kpsession` files today: there is no loader, no replay player, no
  offline state, no session view and no second consumer of the format. M10 is the first
  and only one.

## 5. Design

### 5.1 Components

```text
KomportDoc
  +-- KomportSerial            (transport, by value, destroyed last)
  +-- SessionController        (live observation path, unchanged)
  +-- SessionRecorder          (M9 writer, unchanged)
  +-- SessionReader            (M10 loader, QtCore only, stateless between loads)
  +-- SessionFilePtr           (M10 offline session, shared const value: typed header
  |                             facts + stored events)
  +-- SessionReplayPlayer      (M10 replay player, QtCore only)

SessionReplayPlayer -> eventDelivered(SessionEvent) -> KomportApp display adapter
                                                        +-- KomportView::slotReceivedChar
                                                        +-- KomportHexView::appendRx/appendTx
```

`SessionReader` and `SessionReplayPlayer` are the two new QtCore-only contracts
(ADR-009); the display adapter and everything with a widget in it lives in the
application, outside them. The reader never touches the transport; the player never
touches the reader, the transport or the file.

### 5.2 Data flow

```text
.kpsession file
      |  (read once, read-only, then closed)
      v
SessionReader ---- validate start block / header / records; recover a truncated
      |           final record; refuse anything else with a reason
      v
SessionFilePtr  (immutable: typed header facts + ordered stored events)
      |  \
      |   \--> offline summary (path, event count, recovered flag, recorded span)
      v
SessionReplayPlayer --eventDelivered(SessionEvent)--> views / (M11 decoders)
```

The loaded file is never written to, never kept open and never re-read; the replay
player has no output other than its signal. Nothing in this path can produce a byte on a
wire (ADR-007, §5.8).

### 5.3 Loading contract and its error/recovery semantics

```cpp
/** The typed header facts of a loaded file (ADR-011 D2). */
struct SessionSourceDescriptor {
  quint32 sourceId = 0;         ///< non-zero, unique in the file
  QString clockDomainId;        ///< resolves to SessionFileInfo::clockDomain
  QString name;                 ///< e.g. "local serial"
  QString transport;            ///< e.g. "serial"
  QJsonObject configuration;    ///< the applied snapshot, verbatim (ADR-006 schema)
};
struct SessionClockDomain {
  QString id;
  QString kind;                 ///< "process-monotonic" for the local profile
  qint64 referenceSourceTimestampNs = 0;   ///< the anchor event's raw source time
  qint64 referenceSessionTimestampNs = 0;  ///< exactly 0 in v1
};
struct SessionFileInfo {
  int version = 0;              ///< 1
  QDateTime createdUtc;         ///< ISO-8601 UTC, informational
  QString applicationName;      ///< informational
  QString applicationVersion;   ///< informational
  SessionSourceDescriptor source;
  SessionClockDomain clockDomain;
};

/** The one offline session of a successful load (ADR-011 D5: immutable, verbatim). */
struct SessionFile {
  SessionFileInfo info;
  QList<SessionEvent> events;   ///< stored order, stored values, never modified
};
using SessionFilePtr = std::shared_ptr<const SessionFile>;
class SessionByteSource;   ///< the read seam of §5.10 (declared here, defined there)

/** Loading outcome (ADR-011 D4: all-or-nothing). */
struct SessionLoadOutcome {
  bool ok = false;
  QString path;
  QString reason;                        ///< why the file was refused, empty when ok
  SessionFilePtr session;                ///< null unless ok
  bool recoveredTruncatedFinalRecord = false; ///< ADR-006's recovery rule applied
  quint64 eventCount = 0;
  qint64 bytesRead = 0;
};

/** Reads `.kpsession` v1 files (ADR-006, ADR-011 D1/D2/D3/D10). QtCore only. */
class SessionReader
{
public:
  /** The production byte source: `QFile`, opened read-only (§5.10). */
  SessionReader();
  /** Injection constructor for the byte-source seam of §5.10; reachable only through
    * the test access struct, never part of the documented public surface. */
  explicit SessionReader(SessionByteSource *source);

  static constexpr int kFormatVersion = 1;              ///< ADR-006's binary version
  static constexpr int kHeaderEncodingUtf8Json = 1;     ///< ADR-006's encoding id
  static constexpr qsizetype kRecordPrefixSize = 44;    ///< ADR-006's fixed prefix
  static constexpr qsizetype kMaxHeaderBytes = 16 * 1024 * 1024;
  static constexpr qsizetype kMaxRecordBodyBytes = 64 * 1024 * 1024;
  /** The 8-byte magic "KPSN" 0x1A CR LF NUL. */
  static QByteArray magic();
  /** The verbatim refusal of ADR-011 D2 for a file with more than one source. */
  static QString multiSourceRefusal();       // "multi-source sessions not supported in M10"
  /** The same for more than one clock domain. */
  static QString multiClockDomainRefusal();  // "multi-clock-domain sessions not supported in M10"

  /** Load @p path into one offline session. Read-only; no state is kept between calls. */
  SessionLoadOutcome load(const QString &path) const;
};
```

The table below is the loader contract, exhaustively. "Refused" means: no offline
session is produced, nothing is rendered, and the load result carries the reason (and,
where the file was fully read, the bytes read). "Recovered" means ADR-006's rule: the
complete prefix becomes the session, the truncated final record is ignored, never
delivered and never treated as a valid event, and the flag reports it. Every check is
applied in the order listed for its group, and every declared length is validated
against its limit **before** the bytes it describes are read or any buffer is sized
from it (ADR-006: "the reader validates every length before allocating").

| # | Check | Condition observed in the file | Outcome | Reason / effect |
| --- | --- | --- | --- | --- |
| 1 | file open | the path cannot be opened read-only, or is not a regular file | refused | "the file could not be opened" (the specific reason is reported) |
| 2 | file read | an I/O error while reading the declared bytes | refused | "the file could not be read" |
| 3 | start block | fewer than 16 bytes in total | refused | "shorter than a start block" |
| 4 | magic | the first 8 bytes are not `KPSN` 0x1A CR LF NUL | refused | "bad magic" |
| 5 | format version | the `u16` version is not 1 | refused | "unsupported format version N" |
| 6 | header encoding | the `u16` encoding is not 1 (UTF-8 JSON) | refused | "unsupported header encoding N" |
| 7 | header length limit | the `u32` header length exceeds 16 MiB | refused, before the header is read | "the header exceeds ADR-006's 16 MiB limit" |
| 8 | header length vs. file | 16 + headerLength exceeds the bytes available | refused | "the header length exceeds the file" |
| 9 | header JSON | the header bytes are not a UTF-8 JSON object | refused | "the header is not a JSON object" |
| 10 | header profile: root | the root members are not exactly `format`, `version`, `created`, `application`, `sources`, `clockDomains` | refused | "the header carries unknown or missing root members" |
| 11 | header profile: values | `format` is not `komport-session`; `version` is not the JSON number 1; `created` is not an ISO-8601 UTC string; `application` is not exactly the `{name, version}` string pair | refused | the specific profile violation |
| 12 | source descriptor | not exactly the members `sourceId`, `clockDomainId`, `name`, `transport`, `configuration`, or `sourceId` is not the JSON number 1, or `name`/`transport` are not strings | refused | the specific profile violation |
| 13 | configuration schema | `requested`/`effective` are not exactly the six string members, or `localBuffering` is not exactly `rxQueue`/`flushRate` as numbers, or `compatibility` is not exactly `startBits` as a string, or any section carries an additional member | refused | the specific profile violation (ADR-006's exhaustive nested schema) |
| 14 | clock domain entry | not exactly `id`, `kind`, `reference`, or `kind` is not `process-monotonic`, or the id differs from the source's `clockDomainId` | refused | the specific profile violation |
| 15 | reference pair | `sourceTimestampNs`/`sessionTimestampNs` are not canonical decimal strings, or `sessionTimestampNs` is not `"0"` | refused | the specific profile violation |
| 16 | source count | `sources[]` holds more than one entry | refused | exactly `multi-source sessions not supported in M10` (§5.4) |
| 17 | clock domain count | `clockDomains[]` holds more than one entry | refused | exactly `multi-clock-domain sessions not supported in M10` |
| 18 | record boundary | fewer than 44 bytes remain after the header or after a complete record | recovered | the incomplete final record is dropped; `recoveredTruncatedFinalRecord` is set |
| 19 | record length vs. file | `4 + recordLength` runs past the end of the file | recovered | as above |
| 20 | record length consistency | `40 + metadataLength + payloadLength != recordLength` | refused | "record N has inconsistent lengths" |
| 21 | record body limit | `40 + metadataLength + payloadLength` exceeds 64 MiB | refused | "record N exceeds ADR-006's 64 MiB limit" |
| 22 | flags | `flags != 0` | refused | "record N has non-zero flags" |
| 23 | metadata | `metadataLength > 0` and the metadata bytes are not a JSON object | refused | "record N has non-object metadata" |
| 24 | event type | `eventType` is not a declared `SessionEventType` enumerator | refused | event-local validity (`isValidSessionEvent()`'s type rule) |
| 25 | direction and payload | `Data` with direction `None` or an empty payload; a non-data event with a payload or a direction other than `None` | refused | the specific event-local violation |
| 26 | source resolution | the record's `sourceId` is 0, or differs from the file's single source descriptor | refused | "record N's source does not resolve to the session's source" |
| 27 | timestamps | a negative `sourceTimestampNs` or `timestampNs` | refused | the specific event-local violation |
| 28 | sequence | a record's `sequence` is zero, or does not strictly increase from the previous record | refused | "record N's sequence does not continue the stream" (ADR-002 stream property) |
| 29 | session time | `timestampNs` decreases within the (single) clock domain | refused | "record N's session time decreases" (ADR-002 stream property) |
| 30 | zero records | the file ends exactly after the header (or after a recovered truncation of its only record) | accepted | a valid v1 file with zero events; the session loads, and replay/step refuse with a reason (§5.5) |

Normative notes on the table:

- Checks 10-17 are the profile rule that makes ADR-006's writer profile part of the
  format: a writer that deviates is as broken as one that writes bad lengths, so the
  loader refuses rather than merely parsing (the same stance M9's test-side reader
  takes, which is why §7's tests can use it as an independent cross-check of the same
  fixtures - not as the reader).
- The profile fixes the single source's `sourceId` to 1 (check 12), while the file's own
  source descriptor is the authority against which every record is resolved (check 26).
  A file whose descriptor violates the profile is refused at check 12, so check 26 is
  reached only for a conforming descriptor and can compare against it directly rather
  than re-hard-coding the profile value.
- Checks 24-29 are the extension ADR-011 D3 records: a loaded session is presented as an
  ordered event stream, so it must satisfy what ADR-002 requires of a live stream. They
  are refusals, never silent repairs: the loader does not reorder, renumber, drop,
  translate or re-timestamp anything.
- Checks 28 and 29 deliberately do **not** require the file to begin at `sequence` 1 or
  at session time 0. A recording may start mid-session (ADR-010 §4), so its first record
  legitimately carries a higher sequence and a later session time while the header
  carries the domain's true anchor; requiring a start at 1 would refuse a valid M9 file.
  What is required is a strictly increasing sequence and non-decreasing session time from
  record to record.
- The recovered outcomes (18, 19) are the *only* partial results, and they are reported
  as such. A recovered session with zero events is a session with zero events, not an
  error.
- The loader accepts both a `.kpsession` name and any other name, and identifies the
  format by magic and version only (ADR-011 D2).
- The header is interpreted once, here; no consumer receives the raw header JSON
  (ADR-011 D2). `SessionFileInfo` is the single shape of the header in the program.
- The loader's own constants (checks 4-7, 21, 22) are independent of
  `SessionRecordCodec`; §7's `readerLimitsMatchTheWriterCodec` pins them equal
  (ADR-011 D2).

### 5.4 The multi-source refusal

Multi-source files are refused, not partially rendered (ADR-008 and M14 own the
support; ADR-011 D2). The refusal is decided during header validation, at the source
and clock-domain counts (rows 16 and 17 of §5.3), before any record of the file is
interpreted: a multi-source file produces no session, no implicitly selected source, no
placeholder "unsupported" session and no rendering at all. In particular:

- the refusal carries the verbatim string `multi-source sessions not supported in M10`
  (and `multi-clock-domain sessions not supported in M10` for check 17), so a user and a
  test can tell this refusal from a corrupt file.
- the application reports the reason through the translated status mechanism, exactly
  like any other load refusal, and enters no offline state.
- the requirement is a loader refusal, not a UI filter: a multi-source file cannot be
  opened by any other entry point either, because there is exactly one loader.

### 5.5 The replay-player contract

```cpp
/** The M10 timing ladder (ADR-011 D7; architecture section 9.1). */
enum class SessionReplayTiming {
  Original, Immediate,
  Scale0_1, Scale0_25, Scale0_5, Scale1, Scale2, Scale5, Scale10,
  Step
};
/** The player's states (ADR-011 D6). */
enum class SessionReplayState { Idle, Ready, Playing, Paused, Finished };

struct SessionReplayStart  { bool ok = false; QString reason; };
struct SessionReplayStep   { bool ok = false; QString reason; quint64 advanced = 0;
                             bool matched = false; bool reachedEnd = false; };
struct SessionReplayReport { quint64 delivered = 0; quint64 remaining = 0;
                             quint64 position = 0; bool finished = false;
                             SessionReplayTiming timing = SessionReplayTiming::Original;
                             QString reason; };

/** Read-only passive replay of one loaded session (ADR-007, ADR-011 D6). QtCore only. */
class SessionReplayPlayer : public QObject
{
  Q_OBJECT
public:
  explicit SessionReplayPlayer(QObject *parent = nullptr);
  ~SessionReplayPlayer() override;    ///< stops the timer; emits nothing

  SessionReplayStart  start(const SessionFilePtr &session);  ///< attach; restart if called again
  SessionReplayStart  play();                                ///< begin or resume automatic replay
  SessionReplayReport stop();                                ///< stop; keep the position
  SessionReplayStep   stepNextEvent();
  SessionReplayStep   stepNextRx();
  SessionReplayStep   stepNextTx();
  void                close();                               ///< detach; back to Idle

  SessionReplayState  state() const;
  SessionReplayTiming timing() const;
  void                setTiming(SessionReplayTiming timing);
  quint64             position() const;        ///< index of the next event to deliver
  quint64             deliveredCount() const;
  quint64             eventCount() const;      ///< 0 while Idle

signals:
  void eventDelivered(const SessionEvent &event);
  void stateChanged(SessionReplayState state);
  void replayFinished(const SessionReplayReport &report);
};
```

States and transitions:

```text
Idle    --start(session)---------------------------> Ready
Idle    --close()----------------------------------> Idle    (no-op)
Ready   --play()-----------------------------------> Playing
Ready   --stepNext*()------------------------------> Ready or Finished
Playing --stop()-----------------------------------> Paused
Playing --last event delivered---------------------> Finished (with replayFinished)
Paused  --play()-----------------------------------> Playing
Paused  --stepNext*()------------------------------> Paused or Finished
any of Ready/Paused/Finished --start(session)------> Ready  (explicit restart)
any of Ready/Paused/Finished --close()-------------> Idle
```

| State | Meaning | What is accepted |
| --- | --- | --- |
| `Idle` | no session attached; no position exists | `start()`, `setTiming()`; `stop()` and `close()` are no-ops; `play()` and the steps refuse with a reason |
| `Ready` | session attached, position at the first event, nothing delivered | `play()`, `stepNext*()`, `start()` (restart), `close()`, `setTiming()` |
| `Playing` | automatic replay running through the scheduler | `stop()`, `start()` (restart, which stops first), `setTiming()` (applies to the next delivery) |
| `Paused` | stopped at a preserved position with events remaining | `play()` (resume), `stepNext*()`, `start()`, `close()`, `setTiming()` |
| `Finished` | every stored event was delivered | `start()` (restart), `close()`, `setTiming()`; `play()` and `stepNext*()` refuse with a reason |

Operation semantics (normative):

- `start()` attaches the session and positions at the first event; calling it again is
  the explicit **restart** (it stops any running replay first, resets `deliveredCount()`
  to 0 and re-arms nothing). It refuses only a null session and never mutates the stored
  session. Starting a replay is safe without a confirmation (ADR-007) and performs no
  user interaction.
- `play()` begins automatic replay from the current position. It is refused when there
  is no session (`Idle`), when the session has zero events, when the position is at the
  end (`Finished`), and when the timing policy is `Step` (the reason says that step mode
  advances only on a step command - ADR-011 D7).
- `stop()` cancels a pending scheduled delivery, preserves the position and the
  delivered count, emits nothing, and returns a report. `stop()` in `Ready`/`Idle` is a
  no-op report, not an error.
- A `Playing` replay that delivers its last event transitions to `Finished` and emits
  `replayFinished` with the report (delivered, remaining 0, finished `true`), so an end
  without an explicit user action is still reported - the same rule ADR-010 fixes for a
  recording that ends by itself.
- `close()` detaches the session, releases the shared pointer, returns to `Idle`, and
  emits nothing but the state change.
- `eventDelivered` emits exactly one stored event per delivery, in stored order, by
  `const` reference to a value that is never modified afterwards; the delivered
  sequence is always a complete prefix of the stored stream - no gaps, no reordering,
  no duplication, no derived event (ADR-011 D5).
- Every refusal is a returned value carrying a reason; no refusal is silent and no
  refusal changes the state.
- `setTiming()` accepts every declared ladder value in every state and takes effect as
  described in §5.6; a value outside the declared enumerators (only reachable through a
  cast) is ignored and the current policy is kept. It never rewrites stored timing.

Threading: the player lives on the thread that created it (the session/application
thread), introduces no thread and no queued connection, and all operations are called
from that thread. The scheduler seam fires on that thread.

### 5.6 Timing and step semantics

Timing (ADR-011 D7), all arithmetic in integer nanoseconds on the stored session
timeline (`timestampNs`), never on `sourceTimestampNs` and never rewriting either:

| Timing | Delay before the next event | Notes |
| --- | --- | --- |
| `Original` | `timestampNs[i] - timestampNs[i-1]` | the stored gap, verbatim |
| `Immediate` | 0 | still scheduled, so the event loop turns between events |
| `Scale0_1` | gap × 10 | |
| `Scale0_25` | gap × 4 | |
| `Scale0_5` | gap × 2 | |
| `Scale1` | gap × 1 | identical to `Original` by definition; a test pins it |
| `Scale2` | gap ÷ 2 | truncating |
| `Scale5` | gap ÷ 5 | truncating |
| `Scale10` | gap ÷ 10 | truncating |
| `Step` | not scheduled | `play()` is refused; only steps advance |

Normative details:

- The first event after a `start()`/restart has no predecessor: its delay is 0, whatever
  the policy (a restart does not wait for the session's own origin).
- A change of `setTiming()` while `Playing` takes effect for the next scheduled delivery;
  a delay already being waited out is not re-computed. Stored timing is never touched.
- Division truncates (a sub-nanosecond remainder is dropped) and scaling down therefore
  never produces a delay longer than the stored gap; scaling up uses checked
  multiplication and clamps to the largest representable delay instead of wrapping.
- A zero or negative gap (two events at the same session time, or a stored stream the
  loader accepted as non-decreasing) delivers with delay 0 in stored order; equal
  timestamps never reorder events and never trigger a busy loop, because exactly one
  event is delivered per scheduled callback.
- Automatic advancement is **always** through the scheduler seam, including
  `Immediate` with a zero delay: one event per callback, so the user interface stays
  responsive, `stop()` is honoured at a delivery boundary, and no operation blocks or
  iterates over the session.
- The delay is computed from the gap between the event last delivered and the event to
  be delivered; it is not an absolute wall-clock schedule, and its real-world precision
  is best-effort (ADR-005: scheduling may be less precise than the stored timestamp -
  which stays the authoritative evidence).

Steps (ADR-011 D8):

| Operation | Matches | Delivered by the call |
| --- | --- | --- |
| `stepNextEvent()` | the next event, unconditionally | every event up to and including it |
| `stepNextRx()` | the next `Data` event with direction `Rx` | every event up to and including it (non-matching events included) |
| `stepNextTx()` | the next `Data` event with direction `Tx` | every event up to and including it (non-matching events included) |

- The matching event is the **last** one delivered by the call, so the delivered stream
  stays a complete prefix and a view's rendered state stays consistent with the stored
  stream (a terminal would otherwise silently lose the skipped characters).
- The step result reports `advanced` (how many events were delivered), `matched`
  (whether the requested direction/event was found) and `reachedEnd`.
- A step that finds no match advances to the end, delivers the remaining events, reports
  `matched == false` and `reachedEnd == true`, and leaves the player `Finished`.
- Steps are refused while `Playing` (the reason says to stop first) and in `Idle` and
  `Finished`; a refused step changes nothing.
- Step delivery is synchronous (it is a user-driven, single-turn advance, not a
  schedule): no timer is armed, and if a replay was paused, the position is simply
  advanced. Steps never move the position backwards.

### 5.7 The offline state and the loading preconditions (application wiring)

- A new action `Load Session for Replay...` (object name `loadSessionForReplay`) opens a
  file dialog with `QDir::currentPath()` as the directory and the filter
  `Komport session files (*.kpsession);;All files (*)`, and delegates to
  `KomportDoc::loadOfflineSession(path)`.
- **Preconditions (normative, all enforced in `loadOfflineSession()` so they are
  testable without a window):** loading is permitted only while
  1. `SessionController::state()` is `Idle` (no live session; the transport, if any, is
     closed), **and**
  2. `SessionRecorder::state()` is `Stopped` (no recording is running), **and**
  3. no offline session is loaded.
  Otherwise the load is refused **explicitly** with a reason reported through the
  existing translated status mechanism: "a live session is active - close it before
  loading a session for replay", "a recording is running - stop it before loading a
  session for replay", "an offline session is already loaded - close it first". A
  refusal changes nothing: it does not close, open, configure or write the transport, it
  does not stop a recording, and it does not discard the loaded session. A refusal
  enters no offline state.
- On success the application enters the offline state:
  - the permanent status-bar label `offlineReplayStatusLabel` becomes visible with the
    source text exactly `Offline Session — Passive Replay` (translated via `tr()`; its
    tooltip carries the loaded path, read-only);
  - the live and transmit controls are **disabled**: the profile combo, `Save Profile`,
    `Delete Profile`, `Settings` (connection controls) and terminal key input, the
    macro bar, `Upload`, `Download` and `Upload recent file` (transmit controls);
  - the replay controls are enabled: `Play`, `Stop`, `Step Event`, `Step RX`, `Step TX`
    and the timing ladder in the order Original, Immediate, 0.1x, 0.25x, 0.5x, 1x, 2x,
    5x, 10x, Step;
  - the load outcome is reported through the status mechanism: path, event count, and -
    when ADR-006's recovery rule applied - that a truncated final record was dropped and
    the complete prefix was loaded. A load failure reports its reason and enters no
    offline state;
  - the read-only display adapter renders what the player delivers into the existing
    views: each `Data`/`Rx` payload byte through `KomportView::slotReceivedChar(char)`
    (exactly the entry point the live character path uses), and every `Data` event's
    bytes through `KomportHexView::appendRx(char)`/`appendTx(char)` according to the
    event's stored direction (ADR-004). Non-data events are delivered to the consumer but
    rendered by no pane in M10 - they are counted in the status summary instead -
    because M10 has no event-list view (the timeline and decoder views are later
    milestones). The adapter is application-level: it is the only consumer the M10 player
    has, and its subscription is what M11's decoders will add a second one to;
  - the recorded facts (source name, transport, endpoint, created timestamp) may be
    shown read-only as **recorded** information, and are never applied to a port
    (ADR-011 D12).
- `KomportDoc::closeOfflineSession()` (an explicit `Close Session` action) leaves the
  offline state: the player is closed and destroyed, the session released, the indicator
  hidden, the live controls re-enabled. It opens no port, applies no recorded setting
  and resumes nothing. It is permitted in any replay state and stops a running replay
  first.
- Requesting a replay start is never gated behind a confirmation dialog (ADR-007: a
  passive replay is safe to start without one). No replay-related state is persisted
  across runs and no replay starts automatically on load.

### 5.8 Safety: no code path can transmit

The argument ADR-011 D14 records, restated as the properties the implementation and the
tests must hold:

1. `SessionReader` and `SessionReplayPlayer` include no transport header, hold no
   `ITransport` member or parameter, and call no write entry point (`writeBytes()`,
   `putChar()`, `putStr()`); their only outputs are read bytes and a signal carrying
   `const SessionEvent &`.
2. Neither object is connected to `KomportSerial` or `SessionController`: the offline
   session is not an `ITransport` and is never registered with the controller, and the
   controller never consumes the file. The live path is unchanged and unaware of replay.
3. The offline state disables every transmit entry point in the application (§5.7), so
   the user-facing path cannot reach the transport either; a key press, a macro, an
   upload or a download while a session is loaded reaches nothing.
4. There is nothing to arm: no target selection, no confirmation, no checkbox and no
   persisted state can enable an output; active TX replay and simulation are not
   implemented.
5. The boundary is compiled: `sessionreader.h`/`sessionreader.cpp` and
   `sessionreplayplayer.h`/`sessionreplayplayer.cpp` join
   `komport_session_contract_check` (`tests/CMakeLists.txt`), which is built against
   `Qt6::Core` alone, so a widget, application or transport type entering them breaks
   the build (ADR-009).
6. The application-level test `noTransmitDuringReplay` uses a transport double and a
   real pty peer and asserts that a full replay of a recorded TX/RX session produces no
   write and no bytes at the peer.

### 5.9 Ownership, lifetime and destruction

- `KomportDoc` owns `SessionReader`, `SessionFilePtr` and `SessionReplayPlayer` beside
  the controller and the recorder; the destruction order is controller → recorder →
  replay player → reader and session → transport, and the player's destructor stops its
  timer, emits nothing and releases the session.
- The session is shared, not copied: `SessionFilePtr` is one `shared_ptr<const
  SessionFile>` held by the document and by the player, so a loaded session exists once
  in memory and no consumer can mutate it (ADR-011 D6).
- The reader holds no file handle after `load()` returns (ADR-011 D10): the file is read
  once, completely, through the seam of §5.10 and closed before the session is handed
  out, so a later change to the file cannot affect a loaded session and no error can
  surface mid-replay from the file.
- Live and offline are mutually exclusive (§5.7), so the document never has to order a
  live session against an offline one.
- The application-close path is unchanged: `KomportApp::closeEvent()` still calls
  `KomportDoc::closeSession()` (M9's normative ordering). A loaded offline session is
  discarded with the document; the recording path is unaffected because offline and
  recording cannot be active at the same time.

### 5.10 Internal seams (testable, not public API)

Mirroring SPEC-M9 §5.9, three seams make loading and replay deterministic without a
faulty filesystem, wall-clock time or a real timer:

```cpp
/** Internal seam: the read-only file access the reader needs. Not public API. */
class SessionByteSource {
public:
  virtual ~SessionByteSource();
  virtual bool open(const QString &path) = 0;   ///< read-only
  virtual qint64 size() const = 0;              ///< bytes the source promises
  virtual qint64 read(QByteArray &out, qint64 maxBytes) = 0; ///< bytes read, or < 0
  virtual void close() = 0;
};

/** Internal seam: "run this callback after this delay", on the player's thread. */
class SessionReplayScheduler {
public:
  virtual ~SessionReplayScheduler();
  virtual void scheduleAfter(qint64 delayNs, std::function<void()> callback) = 0;
  virtual void cancelPending() = 0;
};
/** Internal seam: the monotonic time source used for the scheduler and the report. */
class SessionMonotonicClock {
public:
  virtual ~SessionMonotonicClock();
  virtual qint64 nowNs() const = 0;
};
```

- `SessionReader` takes an optional byte source; the default wraps `QFile` opened
  read-only. Tests inject a scripted source to produce a bad magic, an oversized
  declared header length (refused without reading it), a header longer than the file, a
  truncated final record, a short read and an I/O error, all without allocating a 16 MiB
  or 64 MiB buffer where the check is about the *declared* size.
- `SessionReplayPlayer` takes an optional scheduler and clock; the default is `QTimer`
  and the production clock. Tests fire scheduled callbacks explicitly and assert the
  computed delays exactly, instead of waiting for wall-clock time; ADR-005's lower
  real-world precision is therefore never asserted against.
- Neither seam is part of the documented public surface of its class, and production
  code uses them only as defaults; the injection constructors are reachable in tests
  through the same friend-for-testability pattern M9 uses for
  `SessionRecorderSeamsForTest`.

## 6. Invariants

- **Read-only:** the reader opens files read-only and never creates, modifies,
  truncates, renames or deletes one; a file is byte-identical after loading (§7's
  `theFileIsUnchangedByLoading`).
- **Immutability:** loaded events are never modified; the player delivers `const`
  references to stored values and never replaces them with derived data (ADR-002).
- **Verbatim:** payloads, metadata JSON, `sequence`, `sourceId`, `sourceTimestampNs` and
  `timestampNs` are delivered exactly as stored; no charset translation, no trimming, no
  coalescing or splitting, no re-timestamping, no re-anchoring (ADR-004, ADR-005,
  ADR-008).
- **Complete prefix:** the delivered stream is always a prefix of the stored stream, in
  stored order, with no gaps, duplicates or reordering - including during steps and at
  the end of a replay.
- **All-or-nothing:** a refused file yields no session, no events and no rendering; the
  only partial result is ADR-006's recovered prefix, which is flagged.
- **No transmission:** no code path in the offline path can reach a transport write
  (§5.8); the offline state disables every transmit entry point.
- **QtCore-only contracts:** `SessionReader`, its value types and `SessionReplayPlayer`
  carry no widget, application or transport type, and are compiled into the widget-free
  contract check (ADR-009).
- **Bounded units:** every declared length is validated against ADR-006's limit before
  the bytes are read or a buffer is sized from it; no allocation is driven by an
  unvalidated length.
- **Stateless reader:** a load is unaffected by any previous load; the reader holds no
  file handle and no cached state.
- **Single-threaded:** no new thread, no queued connection, no blocking operation; one
  event is delivered per scheduled callback, and `stop()` takes effect at a delivery
  boundary.
- **Prompt-free replay:** starting or running a replay performs no user interaction and
  needs no confirmation (ADR-007); no replay state is persisted across runs.

## 7. Testing strategy

Test names in this section are the design-time plan. Where the shipped suite uses a
different name, §8 carries the shipped name, and the M10 implementation self-review
holds the full mapping between plan, criterion and shipped test (the rule SPEC-M9 §7
established).

New test targets: `tst_sessionreader` (reader, mostly with the injected byte source, no
pty), `tst_sessionreplayplayer` (player, with the scripted scheduler and clock),
`tst_sessionreplayui` (a real `KomportApp` offscreen, with a pty - links `util`), and
`tst_sessionreplayendtoend` (M9 recorder → loader → player over a real pty - links
`util`). All are registered through `komport_add_test()`.

Independence: the reader tests build their fixtures with M9's `SessionRecordCodec` (or
as raw bytes) and cross-check them with the M9 test-side reader
(`tests/sessionrecordreader.h`), which stays M9's parser; the M10 reader links neither.

Reader tests:

| Test | Asserts |
| --- | --- |
| `readerLimitsMatchTheWriterCodec` | the magic, the 44-byte prefix size, the 16 MiB header limit and the 64 MiB record-body limit the reader enforces equal the ones `SessionRecordCodec` exposes; the format version and the header-encoding numbers are pinned by the hand-built byte fixture of `recordPrefixIsReadFieldByField`, which encodes ADR-006's start block literally |
| `badMagicVersionOrEncodingRefusesTheFile` | checks 3-6: a refusal with a reason, no session, nothing rendered |
| `headerAboveSixteenMibIsRefusedBeforeItIsRead` | check 7: the declared length is refused without the header being read or allocated (the injected source reports no read beyond the limit) |
| `headerLongerThanTheFileIsRefused` | check 8 |
| `headerThatIsNotAJsonObjectRefusesTheFile` | check 9 |
| `headerProfileViolationsRefuseTheFile` | checks 10-15, one case per rule: root members, `format`, `version` as the number 1, `created` UTC, `application`, the source descriptor, the exhaustive `configuration` schema, the clock domain entry, the canonical reference pair with `sessionTimestampNs` `"0"` |
| `multiSourceFileIsRefusedWithTheM10Message` | check 16: the reason string is exactly `multi-source sessions not supported in M10`, no session, no partial rendering, no implicit source selection |
| `multiClockDomainFileIsRefused` | check 17: the reason string is exactly `multi-clock-domain sessions not supported in M10` |
| `recordPrefixIsReadFieldByField` | the header facts and every record field (eventType, direction, flags, sequence, sourceId, both timestamps, both lengths) come back with ADR-006's layout and little-endian order, checked against a hand-built byte fixture |
| `recoveredTruncatedFinalRecordIsReportedNotDelivered` | checks 18/19: the complete prefix is the session, `recoveredTruncatedFinalRecord` is set, the truncated record is absent and is never delivered |
| `truncationInsideTheStartBlockOrHeaderRefusesTheFile` | checks 3/8 after the header start: a file cut before its header is complete is refused, not recovered |
| `inconsistentRecordLengthsRefuseTheWholeFile` | check 20: `40 + metadataLength + payloadLength != recordLength` |
| `oversizedRecordBodyIsRefused` | check 21: a declared body above 64 MiB is refused without allocating it |
| `nonZeroFlagsRefuseTheFile` | check 22 |
| `nonObjectMetadataRefusesTheFile` | check 23 |
| `invalidEventFieldsRefuseTheFile` | checks 24-27: unknown type, `Data` with direction `None`, `Data` with an empty payload, a non-data event with a payload or a direction, `sourceId` 0, a negative timestamp |
| `recordSourceMustResolveToTheFileSource` | check 26: a record with a different `sourceId` refuses the file |
| `sequenceMustStrictlyIncrease` | check 28: a zero sequence, a repeated sequence and a decreasing sequence each refuse the file |
| `aMidSessionRecordingLoadsWithItsTrueAnchor` | a file recorded from an already-live session (first record's `sequence` > 1, first `timestampNs` > 0, header reference pair the domain's true anchor with `sessionTimestampNs` `"0"`) loads without a refusal and delivers the stored values unchanged |
| `sessionTimeMustNotDecreaseInTheDomain` | check 29 |
| `payloadsSurviveByteExactly` | all 256 byte values, embedded NUL, an empty-ish metadata object (`metadataLength == 0`) and metadata with 64-bit values above 2^53 as canonical decimal strings round-trip through the reader |
| `sessionWithEveryEventTypeLoadsFieldByField` | a fixture with data TX/RX plus opened, closed, configuration-change, line-state, error, annotation and bookmark events loads with every field, type and direction unchanged |
| `zeroRecordFileIsAValidSessionWithNoEvents` | check 30: loads successfully, `eventCount() == 0`, and `play()`/steps refuse with a reason |
| `readErrorRefusesTheFileWithoutASession` | check 2: an injected read failure, and a source that short-reads without error, produce a refusal and no session |
| `theFileIsUnchangedByLoading` | the file's bytes and size are identical before and after a successful load and after a refused load |
| `theReaderHoldsNoStateBetweenLoads` | load A, then B, then A again: identical outcomes; a refused load does not affect the next successful one |
| `theReaderNeverTouchesTheLivePath` | the reader is constructed and used with a transport double present: no transport call, no controller event, no character signal |

Player tests (scripted scheduler, injected clock, fixture sessions):

| Test | Asserts |
| --- | --- |
| `startPositionsAtTheFirstEventAndNeverMutatesTheSession` | state `Ready`, position 0, delivered 0, and the stored events are identical after a full replay |
| `playDeliversEveryEventInStoredOrder` | TX A, RX B, TX C, RX D and non-data events are delivered in stored order (architecture section 39.2), each exactly once |
| `immediateTimingSchedulesOneDeliveryPerCallback` | `Immediate` delivers one event per scheduled callback, never a synchronous loop, and the UI thread has a turn between events |
| `originalTimingUsesTheStoredSessionTimeGaps` | with the injected clock, the scheduled delays equal the stored `timestampNs` gaps exactly |
| `scaleOneEqualsOriginalTiming` | the two ladder entries produce identical delays (ADR-011 D7) |
| `eachScaleMultipliesOrDividesTheStoredGap` | 0.1x/x10, 0.25x/x4, 0.5x/x2, 2x/÷2, 5x/÷5, 10x/÷10 with truncation, and no negative delay |
| `equalStoredTimestampsDeliverWithoutDelayInOrder` | delay 0, order preserved, no busy loop |
| `changingTimingAppliesToTheNextEventOnly` | the pending delay is not re-computed and stored timing is untouched |
| `playingIsRefusedInStepMode` | `play()` in `Step` returns a refusal with a reason and arms nothing |
| `stepDeliversTheInclusivePrefixAndTheMatch` | `stepNextEvent()` from position *i* delivers events *i..m* with *m* the match, reports `advanced` and `matched`, and leaves the position after *m* |
| `stepNextRxDeliversEveryEventUpToAndIncludingTheNextRx` | the same for the next `Rx` `Data` event; the skipped `Tx`/non-data events are delivered, not dropped |
| `stepNextTxDeliversEveryEventUpToAndIncludingTheNextTx` | the same for the next `Tx` `Data` event |
| `stepPastTheLastMatchAdvancesToTheEndAndReportsNoMatch` | `matched == false`, `reachedEnd == true`, state `Finished` |
| `stepIsRefusedWhilePlaying` | the reason says to stop first, nothing is delivered and the position is unchanged |
| `stepAfterFinishIsRefused` | `Finished` refuses a step and is recovered only by `start()` |
| `stopPreservesPositionAndCount` | `stop()` cancels the pending delivery, keeps position/delivered, returns the report, emits nothing |
| `playResumesFromThePreservedPosition` | no event is delivered twice across stop/play |
| `replayFinishedReportsDeliveredRemainingAndTiming` | at the end of a `Playing` replay: `replayFinished` with `delivered == eventCount`, `remaining == 0`, `finished == true`, state `Finished` |
| `restartReplaysTheWholeStreamAgain` | `start()` again resets the position and the count and delivers everything again |
| `closeReleasesTheSessionAndReturnsToIdle` | state `Idle`, `eventCount() == 0`, the shared session is released (its use count drops) |
| `zeroEventSessionRefusesPlayAndStep` | a session with no events refuses `play()` and every step with a reason |
| `refusalsAreValuesThatChangeNothing` | every refusal path leaves state, position and count untouched and reports a reason |
| `deliveredEventsAreTheStoredEvents` | payload bytes, metadata JSON, sequence, sourceId and both timestamps of every delivered event compare equal to the stored value |
| `thePlayerHoldsNoOutputTarget` | compile-level and static: no transport header, member or parameter; no write call; the unit is in the widget-free contract check (ADR-011 D14) |
| `noSeekOperationExists` | static inspection of the public surface: no position setter, no seek, no range operation (recorded as a static-inspection gap in the self-review, as SPEC-M9 did for the recorder's memory claim) |

UI/offline-state tests (`tst_sessionreplayui`, real `KomportApp` offscreen):

| Test | Asserts |
| --- | --- |
| `loadingIsRefusedWhileTheSessionIsLive` | with an open pty, the load is refused with a reason, no offline state, indicator hidden, live session untouched |
| `loadingIsRefusedWhileARecordingRuns` | with a running `.kpsession` recording, the load is refused with a reason and the recording is not stopped |
| `loadingIsRefusedWhileAnOfflineSessionIsLoaded` | a second load is refused explicitly; the loaded session, its position and its indicator are unchanged |
| `theOfflineIndicatorAppearsExactlyWhileOffline` | `offlineReplayStatusLabel` is visible exactly in the offline state, with the source text `Offline Session — Passive Replay` |
| `liveAndTransmitControlsAreDisabledWhileOffline` | profile combo, Save/Delete Profile, Settings, `Upload`, `Download`, `Upload recent file`, the macro bar and terminal key input are disabled (and re-enabled on close) |
| `theTimingLadderIsPresentInOrder` | the replay controls offer Original, Immediate, 0.1x, 0.25x, 0.5x, 1x, 2x, 5x, 10x, Step in that order, with Play, Stop, Step Event, Step RX, Step TX |
| `keyInputAndMacrosWhileOfflineReachNoTransport` | the transport double records no call; the pty peer receives nothing |
| `noTransmitDuringReplay` | a full replay of a recorded TX/RX session on a pty: no write call, no bytes at the peer, and the TX payloads are delivered through `eventDelivered` |
| `replayRendersIntoTheSameViewAndHexMonitor` | RX payload bytes appear as characters in the terminal view and every delivered event's bytes appear in the hex monitor tagged `RX`/`TX`, byte-exactly (embedded NUL and 0xFF included) |
| `aRefusedLoadReportsItsReasonAndEntersNoOfflineState` | bad magic, a profile violation and a multi-source file: status reason, indicator hidden, controls unchanged |
| `theOfflineStateNeverOpensConfiguresOrWritesTheTransport` | the transport double sees no `open`, `configure`, `close` or write; a configured-but-closed port keeps its settings |
| `recordedSettingsAreNeverApplied` | after loading a file whose snapshot differs from the current settings, the port is still closed and its configuration is unchanged |
| `closingTheOfflineSessionRestoresTheLiveControls` | indicator hidden, controls restored, no port opened |
| `closingTheOfflineSessionStopsARunningReplay` | close during `Playing` stops and destroys the player without a report to the user |
| `loadingIsNotPersistedAcrossRuns` | no offline state, path or timing choice is written to `QSettings`; a new window starts live |

End-to-end (`tst_sessionreplayendtoend`, real pty):

| Test | Asserts |
| --- | --- |
| `recordPtySessionThenLoadAndReplayByteExactly` | record a pty session with M9's recorder (TX and RX, including binary payloads), load it with the M10 reader, replay it with `Immediate`: the concatenated TX payloads equal the bytes sent, the concatenated RX payloads equal the bytes received, and the event count/sequences/type/direction match; the file also parses with M9's test-side reader |
| `replayingARecordedPtySessionTransmitsNothing` | the pty peer reads nothing and the transport double records no write during load and replay |
| `theRecoveryRuleSurvivesARealRecording` | truncating a recorded file inside its final record yields a session whose complete prefix replays, with the recovery reported |

Regression: the existing full `ctest` suite (17/17 targets at M9) passes; M9's tests,
`tests/sessionrecordreader.h`, `SessionRecordCodec`, `SessionRecorder`, the text logger
and the legacy character-signal wiring are unchanged; the build stays warning-free under
`-Wall -Wextra`.

## 8. Acceptance criteria

Test names are the planned ones of §7; where a planned name and the shipped name differ,
this list carries the shipped one and the M10 implementation self-review holds the full
mapping (the SPEC-M9 §8 rule). The milestone is complete when all of them pass.

- [ ] A `.kpsession` v1 file written by M9's recorder loads into one offline session with
  every header fact typed, validated and every event verbatim -
  `sessionWithEveryEventTypeLoadsFieldByField`, `payloadsSurviveByteExactly`,
  `recordPrefixIsReadFieldByField`, `readerLimitsMatchTheWriterCodec`,
  `recordPtySessionThenLoadAndReplayByteExactly`.
- [ ] Every refusal of §5.3 is implemented, reported with its reason, and leaves no
  session and no rendering; the header is validated against ADR-006's normative v1
  writer profile - `badMagicVersionOrEncodingRefusesTheFile`,
  `headerAboveSixteenMibIsRefusedBeforeItIsRead`, `headerLongerThanTheFileIsRefused`,
  `headerThatIsNotAJsonObjectRefusesTheFile`, `headerProfileViolationsRefuseTheFile`,
  `inconsistentRecordLengthsRefuseTheWholeFile`, `oversizedRecordBodyIsRefused`,
  `nonZeroFlagsRefuseTheFile`, `nonObjectMetadataRefusesTheFile`,
  `invalidEventFieldsRefuseTheFile`, `recordSourceMustResolveToTheFileSource`,
  `sequenceMustStrictlyIncrease`, `aMidSessionRecordingLoadsWithItsTrueAnchor`,
  `sessionTimeMustNotDecreaseInTheDomain`,
  `readErrorRefusesTheFileWithoutASession`,
  `aRefusedLoadReportsItsReasonAndEntersNoOfflineState`.
- [ ] ADR-006's recovery rule is implemented as the only partial result and is reported -
  `recoveredTruncatedFinalRecordIsReportedNotDelivered`,
  `truncationInsideTheStartBlockOrHeaderRefusesTheFile`,
  `theRecoveryRuleSurvivesARealRecording`.
- [ ] Multi-source and multi-clock-domain files are refused with the fixed messages and
  no partial rendering - `multiSourceFileIsRefusedWithTheM10Message`,
  `multiClockDomainFileIsRefused`.
- [ ] Passive replay distributes the stored, immutable events in stored order to its
  consumer, with no gaps, duplication or modification -
  `playDeliversEveryEventInStoredOrder`, `deliveredEventsAreTheStoredEvents`,
  `immediateTimingSchedulesOneDeliveryPerCallback`,
  `recordPtySessionThenLoadAndReplayByteExactly`.
- [ ] The timing ladder is complete and exact: original timing and 1x are the stored
  gaps, immediate is scheduled with delay 0, each scale multiplies or divides the stored
  gap with truncation, and no policy ever rewrites stored timing -
  `originalTimingUsesTheStoredSessionTimeGaps`, `scaleOneEqualsOriginalTiming`,
  `eachScaleMultipliesOrDividesTheStoredGap`, `equalStoredTimestampsDeliverWithoutDelayInOrder`,
  `changingTimingAppliesToTheNextEventOnly`.
- [ ] Step mode offers next event, next RX and next TX, delivers the inclusive prefix,
  never advances on its own and is refused while playing; "next decoded frame" is absent
  (M11) - `playingIsRefusedInStepMode`, `stepDeliversTheInclusivePrefixAndTheMatch`,
  `stepNextRxDeliversEveryEventUpToAndIncludingTheNextRx`,
  `stepNextTxDeliversEveryEventUpToAndIncludingTheNextTx`,
  `stepPastTheLastMatchAdvancesToTheEndAndReportsNoMatch`, `stepIsRefusedWhilePlaying`,
  `stepAfterFinishIsRefused`.
- [ ] Navigation is forward-only: start/restart, play/stop and the three steps exist, and
  no seek, position setter or range operation does -
  `stopPreservesPositionAndCount`, `playResumesFromThePreservedPosition`,
  `restartReplaysTheWholeStreamAgain`, `noSeekOperationExists` (static inspection,
  recorded as such in the self-review).
- [ ] The offline state is explicit and exclusive: the indicator reads exactly
  `Offline Session — Passive Replay`, live/connection and transmit controls are disabled
  while it is active, and loading is refused while a live session is active, while a
  recording runs or while a session is loaded, without disturbing what it refuses to
  replace - `theOfflineIndicatorAppearsExactlyWhileOffline`,
  `liveAndTransmitControlsAreDisabledWhileOffline`, `loadingIsRefusedWhileTheSessionIsLive`,
  `loadingIsRefusedWhileARecordingRuns`, `loadingIsRefusedWhileAnOfflineSessionIsLoaded`,
  `closingTheOfflineSessionRestoresTheLiveControls`.
- [ ] No code path can transmit: the player and reader hold no transport and no output
  target, the offline state closes the user-facing transmit path, replay starts without a
  confirmation, and no bytes reach a peer or a transport during loading or replay -
  `thePlayerHoldsNoOutputTarget`, `noTransmitDuringReplay`,
  `keyInputAndMacrosWhileOfflineReachNoTransport`,
  `theOfflineStateNeverOpensConfiguresOrWritesTheTransport`,
  `recordedSettingsAreNeverApplied`, `replayingARecordedPtySessionTransmitsNothing`, and
  the compile-level widget-free contract check.
- [ ] Loading and replaying render into the existing view and hex monitor byte-exactly,
  and the file is never modified - `replayRendersIntoTheSameViewAndHexMonitor`,
  `theFileIsUnchangedByLoading`, `closingTheOfflineSessionStopsARunningReplay`.
- [ ] The existing full `ctest` suite passes warning-free under `-Wall -Wextra`, with
  M9's reader, writer and tests unchanged - the regression statement of §7.

## 9. Risks

| Risk | Mitigation |
| --- | --- |
| A loaded session is held in memory in full, proportional to the file, and M9 writes one record per accepted write, so a byte-wise upload produces a large file | Stated as a cost, not hidden (ADR-011 D10); the store is randomly accessible and the position is an index, so a bounded-memory cursor is additive later; the reader never duplicates the session (one `shared_ptr`, shared with the player); the recorded per-record overhead is known from M9 (44 bytes plus metadata) |
| Replay timing cannot be exact on a desktop OS | ADR-005 already states that stored timing is evidence and replay timing is a policy; tests assert the computed delays with an injected clock and scheduler and never assert wall-clock precision (architecture section 39.3); the real-world precision is documented as best-effort |
| A large session could block the user interface during a replay | Automatic advancement is always scheduled and delivers exactly one event per callback, including at `Immediate`; `stop()` is honoured at a delivery boundary; the display adapter renders only what was delivered |
| The reader drifts into writer territory, or the two implementations diverge until one is a mirror of the other | The reader is its own unit, calls no writer code, refuses to import M9's test-side reader, and one test pins the shared format facts equal while the profile rules are asserted from both sides |
| A user cannot tell a refused file from a recovered one | The two outcomes are distinct in the load result, in the reason text and in the status report, and both have dedicated tests and a dedicated acceptance criterion |
| The offline state leaks a transmit path (key input, macro bar, upload, download, settings, profile load) | Every transmit entry point is enumerated in §5.7, disabled and tested; the contracts hold no transport; the boundary is compiled into the widget-free contract check |
| A file from a future writer version is misread | The reader accepts exactly version 1 and encoding 1 and refuses anything else with a reason; there is no guessing, no heuristic parse and no partial interpretation of an unknown version |
| Multi-source files arrive before M14 and are half-rendered | Refused as early as the header (checks 16/17) with a fixed, tested message, and refused by the only loader, so no other entry point can bypass it |
| Users expect a seek/slider and find restart only | Explicit non-goal, documented in the ADR (D9) and here, exposed in the user-visible refusal wording where a seek-like operation would be expected; the internal store stays seek-ready so the later feature is additive |
| The recovered-prefix rule is mistaken for "the file is fine" | The recovery is reported in the load outcome, in the user-visible summary and in the acceptance criteria, and the truncated record is proven absent from the delivered stream |
| The stream checks of §5.3 refuse a file a laxer future writer might produce | Deliberate (ADR-011 D3): presenting an inconsistent stream as an ordered session is worse than refusing it; the reason names the record and the property, and a future format version can relax it explicitly |

## 10. Decisions

All decisions this specification depends on are recorded in ADR-011 as D1-D14 and are
implemented here without reinterpretation:

| ADR decision | Where this specification implements it |
| --- | --- |
| D1 reader, not transport | §2, §5.1, §5.2, §5.8 |
| D2 own format enforcement, header interpreted once, multi-source refusal | §5.3 (checks 4-17), §5.4 |
| D3 record, event and stream validation; recovery rule | §5.3 (checks 18-29) |
| D4 all-or-nothing | §5.3, §6 |
| D5 immutable, verbatim | §5.5, §6 |
| D6 the player interface | §5.5 |
| D7 timing ladder and arithmetic | §5.6 |
| D8 step semantics | §5.6 |
| D9 forward-only | §3, §5.5, §9 |
| D10 one materialised session, stateless reader | §5.9, §5.10, §9 |
| D11 the offline state and the loading preconditions | §5.7 |
| D12 recorded settings are informational | §5.7, §9 |
| D13 document ownership and its operations | §5.9 |
| D14 no code path can transmit | §5.8 |

No decision remains open in this specification. The two refusal strings of §5.3 rows 16
and 17, the three loading-refusal reasons of §5.7, the indicator text and the operation
and state names of §5.5 are the normative strings and names this milestone's tests
assert; changing any of them is a specification change, reviewed like any other.

## 11. Implementation steps

1. `SessionReader` with its byte-source seam, its constants, the header profile
   validation, the record/event/stream validation, ADR-006's recovery rule and the load
   outcome type, as its own reviewed slice with `tst_sessionreader` (including the
   writer-parity test and the byte fixture).
2. `SessionFile`/`SessionFileInfo`/`SessionSourceDescriptor`/`SessionClockDomain` and
   the widget-free contract check entry for the new units (ADR-009), so the boundary is
   compiled from the first commit.
3. `SessionReplayPlayer` with its five states, the seven operations, the timing
   arithmetic, the scheduler and clock seams, the delivery signal and value-returned
   refusals, with `tst_sessionreplayplayer`.
4. `KomportDoc` ownership, `loadOfflineSession()` with the three preconditions,
   `closeOfflineSession()`, the destruction order and the document-level tests
   (`tst_sessiondocument`-style additions or a new document test target).
5. Application wiring: the `Load Session for Replay...` action and its dialog, the
   `offlineReplayStatusLabel` indicator, the disabled live/transmit controls, the replay
   controls with the timing ladder and the step actions, the read-only offline display
   adapter into `KomportView` and `KomportHexView`, the outcome reporting through the
   status mechanism, with `tst_sessionreplayui`.
6. `tst_sessionreplayendtoend` over a real pty (record with M9's recorder, load, replay,
   byte-exact comparison, no transmission), the regression run of the whole suite and the
   warning-free build under `-Wall -Wextra`.
7. Self-review against this specification (including the static-inspection items:
   `noSeekOperationExists`, `thePlayerHoldsNoOutputTarget`), then the independent review.
