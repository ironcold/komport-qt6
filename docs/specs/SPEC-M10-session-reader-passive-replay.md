# Implementation Spec: M10 Session Reader and Passive Replay

Status: Accepted (independent review rounds 1–5 closed on 2026-09-19, with the owner's
ratification of the process-wide offline state; the review records are in `docs/reviews/`)

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
event / next RX / next TX), forward-only navigation, a rendering path that answers no
terminal query, and no transmission path, no decoder, no simulation and no multi-source
support.

## 2. Scope

Included:

- `SessionReader`: a QtCore-only, read-only loader for ADR-006's v1 container that
  applies the rules ADR-006 fixes - the start block, the header and the required header
  content, every record's structure, ADR-002 event validity as ADR-011 D3 requires, and
  ADR-006's recovery rule for a truncated final record - and produces one immutable offline
  session (typed header facts plus the ordered stored events) or a refusal with a reason.
  It adds exactly one loader rule of its own: the source-count rule of §5.4 (a session has
  exactly one source descriptor).
- The typed header facts (`SessionFileInfo`, `SessionSourceDescriptor`,
  `SessionClockDomain`) and the offline session value type (`SessionFile`) - QtCore
  only, no widget or application type.
- `SessionReplayPlayer`: the read-only replay-player interface of ADR-007 with its five
  states, its operations (start/restart, play, stop, step next event / next RX / next
  TX, close), the timing ladder, the stored-timing-preserving scale arithmetic, the
  delivery signal and value-returned refusals.
- The replay rendering entry points and the reply suppression that make a replayed byte
  unable to answer the terminal: `KomportEmulation::slotReplayReceivedChar(char)`,
  `KomportView::slotReplayReceivedChar(char)` and the emulation's single terminal-reply
  choke point `KomportEmulation::sendTerminalReply()` (§5.7, §5.8).
- The offline state in the application: the `Load Session for Replay...` action, the
  explicit loading preconditions and their refusal, the permanent indicator
  `Offline Session — Passive Replay`, the complete enumeration of disabled
  live/connection/transmit controls and the offline refusal of their entry points
  (§5.7.1), the replay controls (timing ladder and step actions), and the read-only
  offline display adapter that renders delivered events into the existing terminal and
  hex views through the replay rendering entry point.
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
  refused (§5.3.3 row 29, §5.4); multi-source and the sniffer views are M14 (ADR-008).
- No second normative text of the v1 format. ADR-006 owns the format, its limits, its
  reader rules and its required header content, and this specification applies them rather
  than restating them. M10's only own **format** rule is the source count of §5.4 (its two
  I/O preconditions in §5.3.1 are loader rules); it adds no
  rule about the number of clock domains and no admission test built from ADR-006's fixed
  local profile values (review round 1, findings 2 and 6).
- No arbitrary seek, no position setter, no range selection and no timeline slider in
  M10: forward-only navigation only (ADR-011 D9); a later seek is additive because the
  session is stored randomly accessible.
- No change to the `.kpsession` v1 format, no new version, no writer change: the reader
  must accept what the M9 writer produces (§5.3) and the format stays as ADR-006 fixed it.
- No change to `SessionRecordCodec`, `SessionRecorder`, `SessionController`,
  `KomportSerial`, the legacy character signals, the legacy RX buffer, the text logger,
  the `.charset`/profile formats, or `tests/sessionrecordreader.h` (which stays M9's
  test-side parser and is not this milestone's reader - ADR-011 D1). The emulation and the
  view change **additively** only: they gain the replay rendering entry points and the
  single terminal-reply choke point of §5.7, while the live entry points, their replies,
  the live character-signal wiring and the existing RX buffer keep their present behaviour,
  which §7's regression tests pin.
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
- The terminal, the hex monitor (`KomportHexView::appendRx(char)`/`appendTx(char)`) and
  the text logger consume `KomportSerial`'s character signals; they are display adapters
  and not session consumers.
- The live RX path reaches the emulation **directly**, and the emulation can transmit:
  `KomportEmulation`'s constructor connects `KomportSerial::receivedChar(char)` to its own
  `slotReceivedChar(char)` (`komport/komportemulation.cpp`), while
  `KomportView::slotReceivedChar(char)` performs only scroll bookkeeping and renders
  nothing. A terminal query is answered by writing to the port:
  `doDeviceStatusReport()` (`CSI 5 n`, `CSI 6 n`) and `doDeviceAttributes()` (`CSI c`,
  `ESC Z`) call `serial()->putStr(...)`. Rendering stored bytes through that live entry
  point would therefore have an output path, which is why §5.7 adds a reply-suppressing
  replay rendering entry point on the emulation and on the view (review round 1,
  finding 1).
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
                                                        +-- KomportView::slotReplayReceivedChar
                                                        |     (reply-free replay rendering
                                                        |      entry, §5.7; it drives
                                                        |      KomportEmulation::slotReplayReceivedChar)
                                                        +-- KomportHexView::appendRx/appendTx
                                                              (display only, no port access)
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
  QString kind;                 ///< ADR-006's kind of the resolved domain
                                ///< ("process-monotonic" local, "agent-monotonic"
                                ///< future remote); read as a fact, never an
                                ///< admission test
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

/** Reads `.kpsession` v1 files. ADR-006 is the sole normative source of the format, its
  * limits, its reader rules and its required header content; this reader applies them
  * (ADR-011 D1/D2/D3/D10) and adds exactly one restriction of its own (the single-source
  * rule of §5.4). QtCore only. */
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
  static constexpr qsizetype kMaxHeaderBytes = 16 * 1024 * 1024;   ///< ADR-006's limit
  static constexpr qsizetype kMaxRecordBodyBytes = 64 * 1024 * 1024; ///< ADR-006's limit
  /** The 8-byte magic "KPSN" 0x1A CR LF NUL. */
  static QByteArray magic();
  /** The verbatim refusal of ADR-011 D2 for a file with more than one source - the only
    * loader restriction M10 adds to ADR-006. */
  static QString multiSourceRefusal();       // "multi-source sessions not supported in M10"

  /** Load @p path into one offline session. Read-only; no state is kept between calls. */
  SessionLoadOutcome load(const QString &path) const;
};
```

The loader contract is the three tables below, **exhaustively** - and it is the list of
ADR-006 rules this reader *applies*, plus the single restriction of §5.3.3. **ADR-006 is
the sole normative source of the format**, its limits, its reader rules and its required
header content; this section does not restate those rules as a second contract, it names
each one, says where ADR-006 fixes it and fixes the outcome (and, where ADR-006 leaves the
wording free, the reason string) so that the implementation and its tests are unambiguous.
Where this specification and ADR-006 could ever be read differently, **ADR-006 wins**, and
a change to a format rule is a change to ADR-006 (ADR-011 D2).

"Refused" means: no offline session is produced, nothing is rendered, and the load result
carries the reason (and, where the file was fully read, the bytes read). "Recovered" means
ADR-006's rule: the complete prefix becomes the session, the truncated final record is
ignored, never delivered and never treated as a valid event, and the flag reports it.
Every check is applied in the order listed, and every declared length is validated against
its limit **before** the bytes it describes are read or any buffer is sized from it
(ADR-006: "The reader validates every length before allocating").

#### 5.3.1 The ADR-006 rules the reader applies

| # | What is checked | Where ADR-006 fixes it | Outcome and reason |
| --- | --- | --- | --- |
| 1 | file open: the path cannot be opened read-only or is not a regular file | this loader's own I/O rule, not a format rule | refused: "the file could not be opened" (the specific reason is reported) |
| 2 | file read: an I/O error while reading the declared bytes | this loader's own I/O rule | refused: "the file could not be read" |
| 3 | start block: fewer than 16 bytes in total | ADR-006's start-block layout (8-byte magic, `u16` version, `u16` encoding, `u32` header length) | refused: "shorter than a start block" |
| 4 | magic: the first 8 bytes are not `KPSN` 0x1A CR LF NUL | ADR-006, "Invalid magic/version/header ... fails loading" | refused: "bad magic" |
| 5 | format version: the `u16` version is not 1 | ADR-006, the same reader rule | refused: "unsupported format version N" |
| 6 | header encoding: the `u16` encoding is not 1 (UTF-8 JSON) | ADR-006, the same reader rule | refused: "unsupported header encoding N" |
| 7 | header length limit: the `u32` header length exceeds 16 MiB | ADR-006, "header at most 16 MiB" and "validates every length before allocating" | refused before the header is read: "the header exceeds ADR-006's 16 MiB limit" |
| 8 | header length vs. file: 16 + headerLength exceeds the bytes available | ADR-006, an invalid or truncated header fails loading | refused: "the header length exceeds the file" |
| 9 | header JSON: the header bytes are not a UTF-8 JSON object | ADR-006, the header is UTF-8 JSON | refused: "the header is not a JSON object" |
| 10 | required root values: `format` is the string `komport-session`, `version` the JSON number 1, `created` an ISO-8601 UTC string, `application` the `{name, version}` string pair | ADR-006, its required header content and writer profile | refused: the misshaped member is named |
| 11 | required root members present and of their required kind: `format`, `version`, `created`, `application`, `sources` (an array), `clockDomains` (an array) | ADR-006, its required header content | refused: "the header is missing a required member (…)" |
| 12 | additional root members | ADR-006 permits optional decoder hints and notes without naming their members, so M10 cannot and does not enumerate them | accepted and ignored - not interpreted, not exposed, and never a reason to refuse (ADR-011 D2) |
| 13 | source descriptor: `sourceId` is a JSON number with no fractional part in `1 .. 4294967295` (ADR-006's non-zero 32-bit id), `clockDomainId` is a string that resolves to an entry of `clockDomains[]`, `name`/`transport` are strings, `configuration` is present | ADR-006's normative source part ("`sourceId` 32-bit, non-zero, unique in the file"; "A source without a `clockDomainId` is not v1-conformant") | refused: the specific violation (`sourceId` `0`, `1.5`, negative and above `UINT32_MAX` each have their own case). ADR-006's *fixed local profile values* are **not** an admission test: another in-range `sourceId`, another domain `id` or the other `kind` ADR-006 defines are read, not refused (ADR-011 D2) |
| 14 | configuration schema: `requested`/`effective` carry exactly `endpoint`, `baudRate`, `dataBits`, `stopBits`, `parity`, `flowControl` as strings; `localBuffering` exactly `rxQueue`/`flushRate` as numbers; `compatibility` exactly `startBits` as a string; no section carries an additional member | ADR-006's exhaustive nested `configuration` schema ("None of these objects carries an additional member") | refused: the specific violation |
| 15 | clock domain entries: each entry carries `id` (a string), `kind` (one of ADR-006's two kinds) and a `reference`; the `id`s are **unique in the file** | ADR-006's normative clock-domain part ("`id` opaque string, unique in the file") | refused: the specific violation, e.g. "two clock domains share the id …" |
| 16 | reference pair: `sourceTimestampNs`/`sessionTimestampNs` are canonical decimal strings that represent an `i64` (ADR-006's signed 64-bit field), and `sessionTimestampNs` is exactly `"0"` | ADR-006 | refused: the specific violation, including a canonical-looking decimal outside the `i64` range |
| 17 | optional domain member `wallClockCorrelation`: when present, `wallClock` is an ISO-8601 UTC string and `precisionNs` a canonical decimal string representing an `i64` | ADR-006's optional wall-clock correlation and its shapes | refused: the specific violation. The member's *presence* is never a reason to refuse, the correlation is not interpreted and not part of the typed facts (ADR-011 D2), but its shapes are validated because ADR-006 fixes them (review round 3, finding 3) |
| 18 | record length present and sane: the leading `u32` `recordLength` is below 40, the size of the prefix that follows it | ADR-006's record layout: the declared length covers the 40-byte post-length prefix plus both content sections | refused before any field of the record is parsed (so no byte outside the declared record is consumed): "record N declares a length below its own prefix" |
| 19 | record length limit: the leading `u32` `recordLength` exceeds 64 MiB | ADR-006, "record at most 64 MiB" and "validates every length before allocating" | refused *before* the truncation rule of row 21 can apply: "record N exceeds ADR-006's 64 MiB limit" |
| 20 | incomplete final record: fewer than **4** bytes remain after the header or after a complete record, so the leading `recordLength` field itself is incomplete | ADR-006's recovery rule, for the case in which no length can be read at all | recovered: the incomplete final record is dropped and reported (`recoveredTruncatedFinalRecord`) |
| 21 | record length vs. file: a length that passed rows 18 and 19, but whose `4 + recordLength` runs past the end of the file | ADR-006's recovery rule | recovered: the record was cut short mid-write; as above |
| 22 | record length consistency: `40 + metadataLength + payloadLength != recordLength` | ADR-006, "`metadataLength + payloadLength` must exactly match the remaining record body"; a malformed complete record fails loading | refused: "record N has inconsistent lengths". Together with row 19 this bounds every *consistent* record body at 64 MiB, which is the same limit `SessionRecordCodec` enforces over the fields (the form §7's parity test pins) |
| 23 | flags: `flags != 0` | ADR-006's record prefix, "flags (must be zero in v1)" | refused: "record N has non-zero flags" |
| 24 | metadata: `metadataLength > 0` and the metadata bytes are not a JSON object | ADR-006, the metadata is JSON and "`metadataLength == 0` is the encoding of an empty metadata object" | refused: "record N has non-object metadata" |

The record prefix itself is read field by field with ADR-006's fixed widths and
little-endian order (`u32` `recordLength`, `u16` `eventType`, `u8` `direction`, `u8`
`flags`, `u64` `sequence`, `u32` `sourceId`, `i64` `sourceTimestampNs`, `i64`
`timestampNs`, `u32` `metadataLength`, `u32` `payloadLength`); §7's
`recordPrefixIsReadFieldByField` pins that layout against a hand-built byte fixture.

#### 5.3.2 ADR-011 D3's event and stream validation

ADR-011 D3 applies ADR-002's two-part validation contract to a loaded file: a loaded
session is presented to views as an ordered event stream and must satisfy what a live
stream satisfies. These checks are ADR-011's, not local additions of this specification,
and they are refusals, never silent repairs - the loader does not reorder, renumber, drop,
translate or re-timestamp anything.

| # | What is checked | Where the rule comes from | Outcome and reason |
| --- | --- | --- | --- |
| 25 | event-local validity: a declared `eventType`; a declared `direction`; `Data` with a non-empty payload and direction `Tx` or `Rx`; every non-data event with an empty payload and direction `None`; a non-zero `sourceId`; non-negative `sourceTimestampNs`/`timestampNs` | ADR-002's `isValidSessionEvent()`, applied by ADR-011 D3 | refused: the specific event-local violation |
| 26 | source resolution: the record's `sourceId` equals the file's single source descriptor's `sourceId` | ADR-011 D3 | refused: "record N's source does not resolve to the session's source" |
| 27 | sequence: a record's `sequence` is zero or does not strictly increase from the previous record | ADR-002's producer property, applied by ADR-011 D3 | refused: "record N's sequence does not continue the stream" |
| 28 | session time: `timestampNs` decreases within the resolved domain | ADR-002's producer property, applied by ADR-011 D3 | refused: "record N's session time decreases" |

ADR-006 imposes no minimum record count: a file that ends exactly after its header (or
after a recovery of its only record) is a valid v1 file with zero events, it loads, and
replay and the step operations refuse with a reason (§5.5).

#### 5.3.3 The single restriction this milestone adds

| # | What is checked | Where the rule comes from | Outcome and reason |
| --- | --- | --- | --- |
| 29 | source count: `sources[]` does not hold exactly one entry | the owner's scope decision, recorded in ADR-011 D2: M10 presents a single-source session (multi-source is M14, ADR-008) | refused: with more than one entry exactly `multi-source sessions not supported in M10`; with none, "the file declares no source" (§5.4) |

M10 adds no rule of its own about the number of clock domains (review round 1,
finding 6): the single source's `clockDomainId` resolves to exactly one domain per
ADR-006, that domain is the session's timeline, and ADR-002's monotonicity rule (check 28)
applies to it (ADR-011 D2).

Normative notes on the tables:

- The "Where ADR-006 fixes it" column is the citation that keeps the format's normative
  text single; the outcome column fixes what this milestone's tests assert, and the reason
  strings are this milestone's wording rather than a format rule.
- Checks 10-17 deliberately do **not** require ADR-006's fixed local profile values. A
  v1-conformant file whose descriptor carries another non-zero `sourceId`, whose clock
  domain carries the `agent-monotonic` kind or another domain `id` loads, and so does a
  file carrying ADR-006's permitted optional content (decoder hints and notes,
  `wallClockCorrelation`); every record's `sourceId` is resolved against the file's own
  descriptor. Requiring the local values was review round 1's finding 2 - it refused
  valid v1 files.
- Checks 27 and 28 deliberately do **not** require the file to begin at `sequence` 1 or at
  session time 0. A recording may start mid-session (ADR-010 §4), so its first record
  legitimately carries a higher sequence and a later session time while the header carries
  the domain's true anchor; requiring a start at 1 would refuse a valid M9 file. What is
  required is a strictly increasing sequence and non-decreasing session time from record to
  record.
- The recovered outcomes (20, 21) are the *only* partial results, and they are reported as
  such. A recovered session with zero events is a session with zero events, not an error.
- Rows 18-24 are applied in the listed order, and the order matters (review rounds 2 and 3,
  finding 4): rows 18 and 19 are decided from the declared `recordLength` alone, before the
  record's own fields are parsed, and only a length that passes both can be recovered by
  row 21 - a malformed or impossible length is therefore never absorbed as a merely
  truncated final record. Row 20 is the one case in which no length exists to validate.
- All length arithmetic is carried out in 64-bit integers with checked operations, or
  expressed as subtractive bounds checks against the bytes that remain (`4 + recordLength`,
  `40 + metadataLength + payloadLength`), so a declared length can neither wrap nor be
  widened into a plausible one; §7's `lengthArithmeticDoesNotOverflow` pins this with
  u32-maximum fixtures (review round 3, finding 2).
- The loader accepts both a `.kpsession` name and any other name, and identifies the format
  by magic and version only (ADR-011 D2).
- The header is interpreted once, here; no consumer receives the raw header JSON (ADR-011
  D2). `SessionFileInfo` is the single shape of the header in the program.
- The loader's own constants (checks 4-7, 19, 23) are independent of
  `SessionRecordCodec`; §7's `readerLimitsMatchTheWriterCodec` pins them equal (ADR-011
  D2).

### 5.4 The source-count rule - the only format restriction M10 adds to ADR-006

Multi-source files are refused, not partially rendered (ADR-008 and M14 own the support;
ADR-011 D2). This is the single format rule that belongs to M10 rather than to ADR-006:
the refusal is decided during header validation at the source count (row 29 of §5.3.3),
before any record of the file is interpreted, so an unsupported file produces no session,
no implicitly selected source, no placeholder "unsupported" session and no rendering at
all. In particular:

- the refusal carries the verbatim string `multi-source sessions not supported in M10`, so
  a user and a test can tell this refusal from a corrupt file.
- the header must declare **exactly one** source descriptor, because that descriptor is
  what every record resolves against (check 25) and what the offline summary presents: a
  file that declares none is refused with its own reason, "the file declares no source".
  Neither half is a format rule - ADR-006 leaves the source count to the writer and the
  reader's job is to present the session M10 supports.
- the number of clock domains is **not** checked: M10 has no rule of its own about it
  (§5.3.3); the single source's `clockDomainId` is resolved per ADR-006 and that domain is
  the session's timeline.
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
/** The result of a timing change: a refusal is a value, and `timing` is the policy in
  * effect after the call (unchanged when `ok` is false). */
struct SessionReplayTimingChange { bool ok = false; QString reason;
                                   SessionReplayTiming timing = SessionReplayTiming::Original; };
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
  SessionReplayTimingChange setTiming(SessionReplayTiming timing);
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
Playing --setTiming(Step)--------------------------> refused (state unchanged)
Playing --close()----------------------------------> Idle (cancels the pending callback)
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
| `Playing` | automatic replay running through the scheduler | `stop()`, `start()` (restart, which stops first), `close()` (cancels the pending callback), `setTiming()` for every ladder value except `Step`, which is refused with a reason (§5.6); accepted values apply to the next delivery |
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
- `close()` detaches the session, releases the shared pointer, cancels any pending
  scheduled delivery and returns to `Idle`; it emits nothing but the state change. It is
  defined in every state, including `Playing`, and cannot refuse - it returns no value,
  because there is nothing to refuse: the pending callback is cancelled, `deliveredCount()`
  stops, and nothing is delivered afterwards (review round 3, cleanup item 2).
- `eventDelivered` emits exactly one stored event per delivery, in stored order, by
  `const` reference to a value that is never modified afterwards; the delivered
  sequence is always a complete prefix of the stored stream - no gaps, no reordering,
  no duplication, no derived event (ADR-011 D5).
- Every refusal is a returned value carrying a reason; no refusal is silent and no
  refusal changes the state.
- `setTiming()` returns a `SessionReplayTimingChange` and changes nothing but the timing
  policy. It accepts every declared ladder value in `Idle`, `Ready`, `Paused` and
  `Finished` (`ok == true`, `timing` the new policy) and takes effect as described in
  §5.6. In `Playing` it accepts every value **except `Step`**: selecting `Step` while a
  replay is running returns `ok == false` with the reason "pause the replay before
  selecting step mode" and changes nothing - the state stays `Playing`, `timing` reports
  the unchanged policy and a pending delivery is untouched - because automatic advancement
  and step advancement must not both own the position (ADR-011 D7). The user pauses
  explicitly with `stop()` and then selects `Step`; there is no implicit pause and no state
  in which the player is `Playing` with nothing scheduled and no permitted step. A value
  outside the declared enumerators (only reachable through a cast) returns `ok == false`
  with a reason ("unknown timing policy") and keeps the current policy, so no refusal is
  silent. It never rewrites stored timing.

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
| `Step` | not scheduled | `play()` is refused, and selecting `Step` while `Playing` is refused (§5.5); only steps advance |

Normative details:

- The first event after a `start()`/restart has no predecessor: its delay is 0, whatever
  the policy (a restart does not wait for the session's own origin).
- A change of `setTiming()` while `Playing` - for any value except `Step` - takes effect
  for the next scheduled delivery; a delay already being waited out is not re-computed.
  Stored timing is never touched. `Step` is never reached from `Playing` in either
  direction: `play()` while `Step` is selected is refused, and selecting `Step` while
  `Playing` returns `ok == false` (§5.5). A running automatic replay and step advancement
  therefore never coexist, and there is no window in which the player is `Playing` with
  neither a timer nor a permitted step (ADR-011 D7).
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
- **Preconditions (normative, enforced in `loadOfflineSession()` itself so they are
  testable without a window):** loading is permitted only while the whole application is
  idle - every open `KomportApp` window
  1. has `SessionController::state()` `Idle` (no live session; the transport, if any, is
     closed), **and**
  2. has `SessionRecorder::state()` `Stopped` (no recording is running), **and**
  3. has no offline session loaded.

  The check walks the application's top-level windows (`QApplication::topLevelWidgets()`,
  the pattern `slotFileQuit()` and `reconcileCharsetSelectionAfterReloadForAllWindows()`
  already use, `komport/komport.h`), because the offline state is process-wide (ADR-011
  D11): a replay and a live connection must not coexist, neither in one window nor across
  two. Otherwise the load is refused **explicitly** with a reason reported through the
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
  - every control that can transmit, capture the live path or change the connection is
    **disabled** and its entry point refuses as well; §5.7.1 enumerates them exhaustively
    - including `Record Live Session...`, the text logger's `Record Session...`,
    `Edit → Paste` and `New &Window`, which the first draft of this section omitted - and
    names the path each one can reach;
  - the update is applied to **every** open window, not only to the one that loaded the
    session (review round 4, finding 2): all windows' transmit, capture and connection
    controls are disabled and all windows' guarded entry points refuse through the shared
    process-wide predicate of §5.7.1, while the indicator and the replay controls appear in
    the window that loaded the session. Closing the offline session restores the live
    controls in every window;
  - the replay controls are enabled: `Play`, `Stop`, `Step Event`, `Step RX`, `Step TX`
    and the timing ladder in the order Original, Immediate, 0.1x, 0.25x, 0.5x, 1x, 2x,
    5x, 10x, Step. Each handler consumes the player's result value instead of assuming
    success (review round 3, finding 4): a refused timing change (selecting `Step` while
    `Playing`, §5.5) restores the selector to the policy the player reports in
    `SessionReplayTimingChange::timing` and reports `SessionReplayTimingChange::reason`
    through the translated status mechanism, so the user interface never shows a policy the
    player did not adopt; a refused `play()` or step is reported the same way and leaves the
    controls as they were. The timing selector is written only from a player result - it is
    not an independent copy of the policy;
  - the load outcome is reported through the status mechanism: path, event count, and -
    when ADR-006's recovery rule applied - that a truncated final record was dropped and
    the complete prefix was loaded. A load failure reports its reason and enters no
    offline state;
  - the read-only display adapter renders what the player delivers into the existing
    views through a **replay rendering entry point that cannot answer the terminal**:
    - `KomportEmulation::slotReplayReceivedChar(char)` (new) handles one replayed byte
      exactly like `slotReceivedChar(char)` - same escape-sequence state machine, same cell
      writes, same charset translation - but suppresses the terminal's own replies.
      Today's live path reaches the emulation directly (`KomportEmulation`'s constructor
      connects `KomportSerial::receivedChar` to its own `slotReceivedChar(char)`,
      `komport/komportemulation.cpp`), and the emulation answers a stored `CSI 5 n`,
      `CSI 6 n`, `CSI c` or `ESC Z` by writing to the port
      (`doDeviceStatusReport()`/`doDeviceAttributes()`, `serial()->putStr(...)`); replay
      therefore needs its own entry point, not the live one;
    - the emulation's terminal-generated replies have exactly **one private choke point**,
      `KomportEmulation::sendTerminalReply(const QByteArray &)`, and the replay entry
      suppresses that choke point for the duration of the call only (a flag that is set on
      entry and cleared on every exit, so it can never persist into the live path). The
      live entry points keep their replies, which a test pins
      (`liveTerminalQueriesStillAnswer`);
    - a replayed query is therefore still rendered - it changes the screen or the cursor
      as it would live - without a byte leaving the process, and the choke point is also
      the single place future active-output work would have to pass through;
    - `KomportView::slotReplayReceivedChar(char)` (new) is the view-side entry the display
      adapter calls: it forwards the byte to the emulation's replay entry point and
      performs the same display bookkeeping as the live `slotReceivedChar(char)`. It is a
      separate slot, so the live connection is untouched and the replay path is
      unreachable from the live signal;
    - every `Data` event's bytes are additionally rendered in the hex monitor through
      `KomportHexView::appendRx(char)`/`appendTx(char)` according to the event's stored
      direction (ADR-004); the hex monitor is a display-only widget with no port access.
      Non-data events are delivered to the consumer but rendered by no pane in M10 - they
      are counted in the status summary instead - because M10 has no event-list view (the
      timeline and decoder views are later milestones). The adapter is application-level:
      it is the only consumer the M10 player has, and its subscription is what M11's
      decoders will add a second one to;
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

#### 5.7.1 Every output-relevant control, and the offline guard

The table below is the enumeration - the answer to "which user path could still reach a
transport write while a session is loaded". It is complete rather than representative, and
every row names the path the control can reach, so that a path nobody listed is visible as
a missing row instead of as an unexamined risk.

| Control / entry point | Path it can reach | State while offline |
| --- | --- | --- |
| `Upload...` (`fileOpen`), `Download...` (`fileSave`), `Download As...` (`fileSaveAs`), the `Upload recent file` submenu | `KomportTransfer` -> `KomportSerial::putChar()` | disabled; `slotFileOpen()`/`slotFileSave()`/`slotFileSaveAs()`/`slotFileOpenRecent()` refuse |
| `Edit → Paste` (`editPaste`) | `slotEditPaste()` -> `KomportView::slotSimKeyPressed()` -> `KomportEmulation::slotSimKeyPressed()` -> `serial()->putChar()` | disabled; `slotEditPaste()` refuses |
| `New &Window` (`fileNewWindow`) | `slotFileNewWindow()` constructs a second `KomportApp`, whose construction runs `initProfiles()` -> `loadProfile()` -> `applyConnectionSettings()` (close, configure, open): a real live port reached from the offline user interface | disabled; `slotFileNewWindow()` refuses, so no second window and no port activation happens while a replay is loaded (review round 2, finding 2) |
| profile combo (`profileCombo`), `Settings` (`showPreferences`) | `loadProfile()`/`applyConnectionSettings()` (close, configure, open) and the settings dialog | disabled; both entry points refuse before touching the port or opening a dialog |
| `Save Profile` (`profileSave`), `Delete Profile` (`profileDelete`) | `QSettings` only - no port access; disabled because a profile describes the live connection and the offline state does not change connection configuration | disabled; both slots refuse |
| terminal key input | `KomportView::keyPressed` -> `KomportEmulation::slotKeyPressed()` -> `putChar()`/`putStr()` | input suppressed: `KomportView::setKeyInputEnabled(false)` makes `keyPressEvent()` consume the event without emitting `keyPressed`, so the view keeps its focus, its selection and its scrolling but produces no bytes |
| macro bar (`KomportMacroBar`) | `slotMacroTriggered()` -> `serial->putStr()` | disabled; `slotMacroTriggered()` refuses |
| `Record Live Session...` (`recordLiveSession`) | `SessionRecorder` on the live path (`startSessionRecording()`) | disabled; `slotToggleSessionRecording()` and `startSessionRecording()` refuse |
| `Record Session...` (the text logger, `recordSession`) | the legacy RX character signal -> a text file | disabled; `slotToggleRecording()` refuses (the logger is a live-path consumer and a replay cannot feed it) |
| `Edit → Cut`/`Copy`, `File → Print`, the toolbar and status-bar toggles, the hex monitor and its filters, the minimap, scrolling and selection, and the slot `slotFileNew()` (which has no menu entry: it goes through the unimplemented document stubs and touches no port) | the clipboard, the view's own state, a printer, widget visibility - no transport and no live path | enabled; listed so that the enumeration is demonstrably complete rather than a summary |
| `File → Close` (`fileClose`), `File → Quit` (`fileQuit`) | teardown: `closeEvent()` calls `KomportDoc::closeSession()`, which closes the transport (`mSerial.close()`), and `slotFileQuit()` does that for every top-level window (`komport/komport.cpp`) | enabled, deliberately: teardown closes the offline session and destroys the document - it is how the offline state is left - it never writes a byte, and refusing it would trap the user in the offline state. It is not an output path, and once it completes the process-wide preconditions of §5.7 allow a load again (review round 3, cleanup item 1) |
| the line-ending combo | writes the line-ending setting and the emulation's line ending; it transmits nothing by itself | enabled; with key input, paste and macros suppressed there is no transmitter whose output it could shape |
| `Load Session for Replay...` while a session is loaded | the loader | enabled, and refused by the loading precondition of §5.7 |
| `Close Session`, `Play`, `Stop`, `Step Event`, `Step RX`, `Step TX`, the timing ladder | the replay player only | enabled |

Two rules make that table hold instead of merely describing the user interface:

- **The guard lives in the entry points, not in the `QAction`.** Every slot in a
  "disabled" row begins with the offline refusal - the reason
  "a loaded offline session is being replayed - close it first" - and returns without a
  side effect, with the reason reported through the translated status mechanism like any
  other refusal. A disabled `QAction` is a user-interface affordance only: the same slots
  are reachable from a shortcut, from the macro bar and from a direct call, and the M10
  tests call them directly while a session is loaded. The predicate behind the guard is
  **process-wide and shared** (review round 4, finding 2): one helper answers "is a window
  offline" for every guarded slot in every window, so a paste, macro, profile, settings,
  upload, download or recording slot invoked in a *second*, idle window refuses as well,
  and no window can open, configure or write while any window replays.
- **The replay path has its own entry point.** Rendering never goes through a live entry
  point (§5.7), so even a defect in the guard above cannot make a replayed byte answer the
  terminal.
- **The offline state is process-wide.** The loading preconditions of §5.7 walk every open
  window's document, `New &Window` is refused while any window is offline (row above), and
  no M10 operation leaves a live session or a running recording beside a replay - neither
  in the same window nor across two (ADR-011 D11). The walk uses the pattern `slotFileQuit()`
  and `reconcileCharsetSelectionAfterReloadForAllWindows()` already establish.

### 5.8 Safety: no code path can transmit

ADR-011 D14 records the argument; this section is its checkable form. The properties and
the evidence are stated separately, because a boundary that does not exclude the forbidden
type proves nothing.

What holds by construction, with the mechanism named in each clause:

1. `SessionReader` and `SessionReplayPlayer` include no transport header, hold no
   `ITransport` member or parameter, and call no write entry point (`writeBytes()`,
   `putChar()`, `putStr()`); their only outputs are read bytes and a signal carrying
   `const SessionEvent &`.
2. Neither object is connected to `KomportSerial` or `SessionController`: the offline
   session is not an `ITransport` and is never registered with the controller, and the
   controller never consumes the file. The live path is unchanged and unaware of replay.
3. Rendering a replayed byte cannot answer the terminal: the emulation's terminal-generated
   replies have the single private choke point `KomportEmulation::sendTerminalReply()`,
   and the replay rendering entry point of §5.7 suppresses it for the duration of the call
   (the live entry points keep their replies). A stored `CSI 5 n`, `CSI 6 n`, `CSI c` or
   `ESC Z` is displayed, not answered.
4. The user-facing path is closed twice: the offline state disables every control of
   §5.7.1 *and* every one of those entry points refuses while offline, so a key press, a
   paste, a macro, an upload, a download, a profile load, a settings dialog or a recording
   start while a session is loaded reaches nothing - including when a slot is called
   directly, which is how the tests drive them.
5. There is nothing to arm: no target selection, no confirmation, no checkbox and no
   persisted state can enable an output; active TX replay and simulation are not
   implemented.

How that is checked, and what each check does and does not prove:

- **Dependency audit.** `sessionreader.h`/`.cpp` and `sessionreplayplayer.h`/`.cpp` join
  `komport_session_contract_check` (`tests/CMakeLists.txt`), which is built against
  `Qt6::Core` alone. Its claim is bounded and stated as such: it keeps widget and
  application types out of the offline units (ADR-009). It does **not** prove the absence
  of a transport, because `ITransport` is itself QtCore-only and would compile inside that
  target - the contract check cannot see it.
- **Source audit, over the offline units and over the objects the replay path drives.**
  Two tests read the sources from the source tree (their directory is injected by CMake as
  a compile definition) and assert what the dependency audit cannot:
  - `offlineUnitsNameNoWriteEntryPointOrTransport`: the four offline unit sources include
    neither `itransport.h` nor `komportserial.h` nor a widget header and contain no write
    entry point name (`writeBytes`, `putChar`, `putStr`) outside comments;
  - `replayEntriesAndReplySitesPassTheSourceAudit`: the functions the replay path drives -
    `KomportEmulation::slotReplayReceivedChar()`, `KomportView::slotReplayReceivedChar()`
    and the display adapter - contain no direct write call, and every terminal-generated
    reply site in the emulation reaches the port only through the single choke point
    `sendTerminalReply()` (the live reply functions call it directly, the replay entry
    through the suppression).
  Being tests, both run with the rest of `ctest` and are repeatable in one command.
- **Dynamic no-write evidence, in-process and at the peer.** `KomportDoc` holds its
  transport by value and M10 adds no substitution seam for it, so the M10 tests do not
  pretend to inject a transport double. They use the two observations the existing code
  already provides: the transport's own write observation `ITransport::bytesWritten`
  (emitted by the single write primitive every write entry point routes through, ADR-003,
  and already consumed by `SessionController`) and the real pty the document's port is
  attached to. `bytesWritten` is never emitted during a load or a replay, and the peer
  reads nothing (asserted with a timeout). The two observations say exactly that much and
  no more (review round 3, cleanup item 3): the transport's write observation is emitted
  only after a write has been *accepted* (`komport/komportserial.cpp`), so its absence
  proves that the transport accepted no byte - not that a write entry point was never
  called. That structural property is what the two source audits above establish; the
  peer's silence then shows that nothing arrived at the device boundary. The tests that
  carry this evidence are:
  - `replayedTerminalQueriesProduceNoReply` replays a session containing `CSI 5 n`,
    `CSI 6 n`, `CSI c` and `ESC Z` (each delivered through the display adapter into the
    terminal view) and asserts that neither `bytesWritten` nor the peer shows anything,
    while the replay did render the stored bytes;
  - `liveTerminalQueriesStillAnswer` sends the same four sequences over the live path and
    asserts that the four replies do reach the peer, so the suppression is proved specific
    to the replay entry point rather than a globally disabled reply path;
  - `noTransmitDuringReplay` replays a recorded TX/RX session completely and asserts that
    neither `bytesWritten` nor the peer shows anything while the TX payloads are delivered
    through `eventDelivered`;
  - `replayingARecordedPtySessionTransmitsNothing` does the same for a file the M9
    recorder wrote end to end.

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
- The replay rendering entry points (§5.7) add no ownership: `KomportEmulation` stays
  owned by the view and the view by the application, exactly as today. The entry points
  hold no reference to the replay player, the player holds no reference to a view, and the
  player is still destroyed before the reader/session and before the transport.

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
- There is deliberately **no transport seam**: `KomportDoc` owns its `KomportSerial` by
  value (M8/M9) and M10 adds no substitution point for it, so the application-level tests
  assert through a real pty and the port's observable state (§5.8) instead of injecting a
  transport double. Introducing such a seam would change M8's ownership contract and is
  not part of this milestone.

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
  (§5.8): the offline objects hold no transport, a replayed byte cannot answer the
  terminal, the offline state disables every output-relevant control *and* the entry
  points behind them refuse while it is active, and nothing can arm an output.
- **Reply-free replay rendering:** a stored terminal query is displayed and never
  answered - the emulation's terminal-generated replies have one choke point, the replay
  rendering entry point suppresses it for the duration of the call, and the live entry
  points keep their replies.
- **One offline state:** at most one window has a loaded session, and only while no window
  has a live session and no recording runs anywhere (§5.7). The indicator lives in the
  window the session was loaded into, but the exclusivity is process-wide (ADR-011 D11).
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
| `headerViolationsRefuseTheFile` | checks 10-17, one case per rule: a missing required root member, `format`, `version` as the number 1, `created` as an ISO-8601 UTC string, `application`, the source descriptor (a zero `sourceId`, a `clockDomainId` resolving to no `clockDomains[]` entry, a non-string `name`), the exhaustive `configuration` schema (an extra member in `requested`), the clock domain entry (`kind` outside ADR-006's two kinds), and the reference pair (a non-canonical decimal, and `sessionTimestampNs` other than `"0"`) |
| `numericHeaderShapesRefuseOutOfRangeValues` | check 13 and 16/17 boundary fixtures: `sourceId` `0`, `1.5`, a negative value and `4294967296` each refuse the file, while `1` and `4294967295` load; a canonical-looking decimal outside the `i64` range in the reference pair or in `precisionNs` refuses it (review round 3, finding 3) |
| `lengthArithmeticDoesNotOverflow` | checks 18-22 with u32-maximum fixtures: a record whose declared `recordLength`, `metadataLength` or `payloadLength` is at or above `UINT32_MAX` is refused through the length checks (row 19 or row 22), never wrapped into a plausible sum and never recovered as a truncation (review round 3, finding 2) |
| `duplicateClockDomainIdIsRefused` | check 15: two clock-domain entries sharing an `id` violate ADR-006's uniqueness rule and refuse the file |
| `malformedWallClockCorrelationIsRefused` | check 17: a `wallClockCorrelation` whose `wallClock` is not an ISO-8601 UTC string or whose `precisionNs` is not a canonical decimal string violates ADR-006's shapes and refuses the file; a *well-formed* correlation (and its absence) never refuses one |
| `permittedOptionalMembersAreAccepted` | a v1-conformant file that carries ADR-006's permitted optional content (decoder hints and notes at the root, a well-formed `wallClockCorrelation`) loads; none of it is interpreted, exposed or a reason to refuse |
| `localProfileValuesAreNotAnAdmissionTest` | a v1-conformant file whose descriptor carries another non-zero `sourceId`, another domain `id` and the `agent-monotonic` kind ADR-006 defines loads, and every record resolves against the file's own descriptor (review round 1, finding 2: the previous draft refused this valid file) |
| `multiSourceFileIsRefusedWithTheM10Message` | row 29: the reason string is exactly `multi-source sessions not supported in M10`, no session, no partial rendering, no implicit source selection |
| `fileWithoutASourceIsRefused` | row 29's other half: a header whose `sources[]` is empty is refused with its own reason ("the file declares no source") and no session |
| `clockDomainCountIsNotAnM10Rule` | the reader adds no clock-domain-count rule of its own (ADR-011 D2): `multiClockDomainRefusal()` does not exist in its surface, and a file naming a second (unreferenced) domain entry is judged by ADR-006's rules alone - the session's clock domain is the one the source's `clockDomainId` resolves to, and no multi-clock-domain refusal exists anywhere (review round 1, finding 6) |
| `recordPrefixIsReadFieldByField` | the header facts and every record field (eventType, direction, flags, sequence, sourceId, both timestamps, both lengths) come back with ADR-006's layout and little-endian order, checked against a hand-built byte fixture |
| `recoveredTruncatedFinalRecordIsReportedNotDelivered` | checks 20/21: the complete prefix is the session, `recoveredTruncatedFinalRecord` is set, the truncated record is absent and is never delivered |
| `truncationInsideTheStartBlockOrHeaderRefusesTheFile` | checks 3/8 after the header start: a file cut before its header is complete is refused, not recovered |
| `recordLengthBelowThePrefixIsRefused` | check 18: a final record declaring a `recordLength` below 40 is refused (not recovered), and no byte beyond the declared record is consumed |
| `recordLengthAboveTheLimitIsRefusedBeforeRecovery` | check 19: a final record declaring a `recordLength` above 64 MiB is refused even though the file ends inside the declared length - the impossible length is not accepted as a truncated final record |
| `inconsistentRecordLengthsRefuseTheWholeFile` | check 22: `40 + metadataLength + payloadLength != recordLength` |
| `nonZeroFlagsRefuseTheFile` | check 23 |
| `nonObjectMetadataRefusesTheFile` | check 24 |
| `invalidEventFieldsRefuseTheFile` | check 25: unknown type, `Data` with direction `None`, `Data` with an empty payload, a non-data event with a payload or a direction, `sourceId` 0, a negative timestamp |
| `recordSourceMustResolveToTheFileSource` | check 26: a record with a different `sourceId` refuses the file |
| `sequenceMustStrictlyIncrease` | check 27: a zero sequence, a repeated sequence and a decreasing sequence each refuse the file |
| `aMidSessionRecordingLoadsWithItsTrueAnchor` | a file recorded from an already-live session (first record's `sequence` > 1, first `timestampNs` > 0, header reference pair the domain's true anchor with `sessionTimestampNs` `"0"`) loads without a refusal and delivers the stored values unchanged |
| `sessionTimeMustNotDecreaseInTheDomain` | check 28 |
| `payloadsSurviveByteExactly` | all 256 byte values, embedded NUL, an empty-ish metadata object (`metadataLength == 0`) and metadata with 64-bit values above 2^53 as canonical decimal strings round-trip through the reader |
| `sessionWithEveryEventTypeLoadsFieldByField` | a fixture with data TX/RX plus opened, closed, configuration-change, line-state, error, annotation and bookmark events loads with every field, type and direction unchanged |
| `zeroRecordFileIsAValidSessionWithNoEvents` | ADR-006 imposes no minimum record count: the file loads successfully, `eventCount() == 0`, and `play()`/steps refuse with a reason |
| `readErrorRefusesTheFileWithoutASession` | check 2: an injected read failure, and a source that short-reads without error, produce a refusal and no session |
| `theFileIsUnchangedByLoading` | the file's bytes and size are identical before and after a successful load and after a refused load |
| `theReaderHoldsNoStateBetweenLoads` | load A, then B, then A again: identical outcomes; a refused load does not affect the next successful one |
| `theReaderNeverTouchesTheLivePath` | the reader is constructed and used while a live document with a real (pty-backed) port exists: no port call, no controller event, no character signal |

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
| `selectingStepWhilePlayingIsRefused` | `setTiming(Step)` while `Playing` returns `ok == false` with the reason "pause the replay before selecting step mode" and changes nothing - the state stays `Playing`, `timing()` still reports the previous policy and a pending delivery still happens - while `stop()` followed by `setTiming(Step)` returns `ok == true`; there is no implicit pause (review round 1 finding 4; the value-returned refusal is review round 2 finding 1) |
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
| `closeWhilePlayingCancelsAndReturnsToIdle` | `close()` in `Playing` cancels the pending delivery (the scheduler seam shows a cancelled callback), transitions to `Idle`, delivers nothing afterwards and emits only the state change - it cannot refuse (review round 3, cleanup item 2) |
| `zeroEventSessionRefusesPlayAndStep` | a session with no events refuses `play()` and every step with a reason |
| `refusalsAreValuesThatChangeNothing` | every refusal path leaves state, position and count untouched and reports a reason |
| `deliveredEventsAreTheStoredEvents` | payload bytes, metadata JSON, sequence, sourceId and both timestamps of every delivered event compare equal to the stored value |
| `offlineUnitsNameNoWriteEntryPointOrTransport` | the source/dependency audit of §5.8: the four offline unit sources include neither `itransport.h` nor `komportserial.h` nor a widget header and contain no write entry point name (`writeBytes`, `putChar`, `putStr`) outside comments, and the units are compiled into the widget-free contract check - whose claim is stated as covering widget and application types only, **not** `ITransport`, which is itself QtCore-only and would compile there (ADR-011 D14, review round 1, finding 5) |
| `noSeekOperationExists` | static inspection of the public surface: no position setter, no seek, no range operation (recorded as a static-inspection gap in the self-review, as SPEC-M9 did for the recorder's memory claim) |

UI/offline-state tests (`tst_sessionreplayui`, real `KomportApp` offscreen):

| Test | Asserts |
| --- | --- |
| `loadingIsRefusedWhileTheSessionIsLive` | with an open pty, the load is refused with a reason, no offline state, indicator hidden, live session untouched |
| `loadingIsRefusedWhileAnotherWindowsSessionIsLive` | with window A live (its own document and transport) and window B idle, B's load is refused with the same reason, no offline state is entered, and A's session is untouched (review round 3, finding 1 - the process-wide walk) |
| `loadingIsRefusedWhileAnotherWindowHasASessionLoaded` | with window A offline and window B idle, B's load is refused ("an offline session is already loaded - close it first") |
| `newWindowIsRefusedWhileAnyWindowIsOffline` | with window A offline, `slotFileNewWindow()` refuses with the offline reason: no second window is constructed, so no profile is loaded and no port is opened (review round 3, finding 2) |
| `offlineStateDisablesControlsInEveryOpenWindow` | with window A offline and window B - opened before the load - idle: B's profile combo, `Save Profile`, `Delete Profile`, `Settings`, `Upload`/`Download`/`Upload recent file`, `Edit → Paste`, both recording actions, the macro bar and `New &Window` are disabled as well, while the indicator and the replay controls appear only in A; closing the offline session re-enables both windows (review round 4, finding 2) |
| `offlineGuardRefusesInEveryWindow` | with window A offline, window B's slots are invoked **directly** - `loadProfile()`, `slotShowPreferences()`, `slotEditPaste()`, `slotMacroTriggered()`, `slotFileOpen()`, `slotFileSave()`, `slotToggleSessionRecording()`, `startSessionRecording()`: each reports the process-wide offline refusal, changes nothing, opens no dialog, starts no recording, and neither B's pty peer receives a byte nor is `ITransport::bytesWritten` emitted (review round 4, finding 2) |
| `loadingIsRefusedWhileARecordingRuns` | with a running `.kpsession` recording, the load is refused with a reason and the recording is not stopped |
| `loadingIsRefusedWhileAnOfflineSessionIsLoaded` | a second load is refused explicitly; the loaded session, its position and its indicator are unchanged |
| `theOfflineIndicatorAppearsExactlyWhileOffline` | `offlineReplayStatusLabel` is visible exactly in the offline state, with the source text `Offline Session — Passive Replay` |
| `everyOutputRelevantControlIsDisabledWhileOffline` | §5.7.1 row by row: `New &Window`, the profile combo, `Save Profile`, `Delete Profile`, `Settings`, `Upload`/`Download`/`Download As`/`Upload recent file`, `Edit → Paste`, `Record Live Session...`, `Record Session...` and the macro bar are disabled while the offline state is active and re-enabled on close, and terminal key input is suppressed (`setKeyInputEnabled(false)`) |
| `theTimingLadderIsPresentInOrder` | the replay controls offer Original, Immediate, 0.1x, 0.25x, 0.5x, 1x, 2x, 5x, 10x, Step in that order, with Play, Stop, Step Event, Step RX, Step TX |
| `refusedTimingSelectionRestoresTheSelectorAndReportsIt` | a `Step` selection while a replay is `Playing` leaves the player's policy unchanged **and** the selector showing the policy the player reports, with the refusal reason in the status mechanism - the user interface never shows a policy the player did not adopt (review round 3, finding 4) |
| `invokingEachOfflineGuardedEntryPointReachesNoTransport` | every guarded entry point of §5.7.1 is called **directly** while a session is loaded - `slotFileNewWindow()`, `slotEditPaste()`, `slotFileOpen()`, `slotFileSave()`, `slotFileSaveAs()`, `slotFileOpenRecent()`, `slotMacroTriggered()`, `slotToggleSessionRecording()`, `slotToggleRecording()`, `startSessionRecording()`, `loadProfile()`, `slotSaveProfile()`, `slotDeleteProfile()`, `slotShowPreferences()` and a synthetic key event on the view: each reports its offline refusal through the status mechanism, changes nothing, opens no window and no dialog, starts no recording and leaves the pty peer without a byte |
| `keyInputPasteAndMacrosWhileOfflineReachNoTransport` | a real `QKeyEvent` on the view, a paste of non-empty clipboard text and a macro trigger while offline: the pty peer receives nothing and `ITransport::bytesWritten` is never emitted |
| `replayedTerminalQueriesProduceNoReply` | a replay of a session containing `CSI 5 n`, `CSI 6 n`, `CSI c` and `ESC Z` renders those bytes into the terminal view while the pty peer receives nothing and `ITransport::bytesWritten` is never emitted (review round 1, finding 1: these are exactly the sequences that used to reach `serial()->putStr()`) |
| `liveTerminalQueriesStillAnswer` | the same four sequences sent over the live path do reach the pty peer (status report, cursor-position report, `ESC [ ? 62 c` for both identify requests), so the suppression is proved specific to the replay entry point and not a globally disabled reply path |
| `replayEntriesAndReplySitesPassTheSourceAudit` | the source audit of §5.8 for the objects the replay path drives: no write call in the replay entry functions, and every terminal-generated reply site of the emulation reaches the port only through `sendTerminalReply()` (review round 2, finding 5) |
| `noTransmitDuringReplay` | a full replay of a recorded TX/RX session over a pty: the peer reads nothing, `ITransport::bytesWritten` is never emitted, and the TX payloads are delivered through `eventDelivered` |
| `replayRendersIntoTheSameViewAndHexMonitor` | RX payload bytes appear as characters in the terminal view and every delivered event's bytes appear in the hex monitor tagged `RX`/`TX`, byte-exactly (embedded NUL and 0xFF included) |
| `aRefusedLoadReportsItsReasonAndEntersNoOfflineState` | bad magic, a profile violation and a multi-source file: status reason, indicator hidden, controls unchanged |
| `theOfflineStateNeverOpensConfiguresOrWritesTheTransport` | over a real pty: no `open`, no configuration change, no `close` and no write is observable at the port during loading, replaying or closing, and a configured-but-closed port keeps its settings |
| `recordedSettingsAreNeverApplied` | after loading a file whose snapshot differs from the current settings, the port is still closed and its configuration is unchanged |
| `closingTheOfflineSessionRestoresTheLiveControls` | indicator hidden, the live controls restored in every window, no port opened |
| `closingTheOfflineSessionStopsARunningReplay` | close during `Playing` stops and destroys the player without a report to the user |
| `loadingIsNotPersistedAcrossRuns` | no offline state, path or timing choice is written to `QSettings`; a new window starts live |

End-to-end (`tst_sessionreplayendtoend`, real pty):

| Test | Asserts |
| --- | --- |
| `recordPtySessionThenLoadAndReplayByteExactly` | record a pty session with M9's recorder (TX and RX, including binary payloads), load it with the M10 reader, replay it with `Immediate`: the concatenated TX payloads equal the bytes sent, the concatenated RX payloads equal the bytes received, and the event count/sequences/type/direction match; the file also parses with M9's test-side reader |
| `replayingARecordedPtySessionTransmitsNothing` | the pty peer reads nothing during load and replay, including when the recorded session contains terminal queries |
| `theRecoveryRuleSurvivesARealRecording` | truncating a recorded file inside its final record yields a session whose complete prefix replays, with the recovery reported |

Regression: the existing full `ctest` suite (17/17 targets at M9) passes; M9's tests,
`tests/sessionrecordreader.h`, `SessionRecordCodec`, `SessionRecorder` and the text logger
are unchanged; the emulation's and the view's live entry points, their replies and the live
character-signal wiring keep their present behaviour (M10's replay entry points and the
reply choke point are additive, and the live-reply test above pins the difference); the
build stays warning-free under `-Wall -Wextra`.

## 8. Acceptance criteria

Test names are the planned ones of §7; where a planned name and the shipped name differ,
this list carries the shipped one and the M10 implementation self-review holds the full
mapping (the SPEC-M9 §8 rule). The milestone is complete when all of them pass. This list
is criteria-based rather than a test inventory: §7 is the exhaustive plan, so a test that
no criterion below names (the zero-event and zero-record readings, the reader's
statelessness, the lifetime and report assertions) still runs as part of the suite.

- [ ] A `.kpsession` v1 file written by M9's recorder loads into one offline session with
  every header fact typed, validated and every event verbatim -
  `sessionWithEveryEventTypeLoadsFieldByField`, `payloadsSurviveByteExactly`,
  `recordPrefixIsReadFieldByField`, `readerLimitsMatchTheWriterCodec`,
  `recordPtySessionThenLoadAndReplayByteExactly`, `zeroRecordFileIsAValidSessionWithNoEvents`,
  `zeroEventSessionRefusesPlayAndStep`, `theReaderHoldsNoStateBetweenLoads`,
  `theReaderNeverTouchesTheLivePath`.
- [ ] Every refusal of §5.3 is implemented, reported with its reason, and leaves no
  session and no rendering; the header is read by applying ADR-006's required members and
  shapes (ADR-006 remaining the sole normative source of the format), and ADR-006's
  permitted optional content never refuses a file - `badMagicVersionOrEncodingRefusesTheFile`,
  `headerAboveSixteenMibIsRefusedBeforeItIsRead`, `headerLongerThanTheFileIsRefused`,
  `headerThatIsNotAJsonObjectRefusesTheFile`, `headerViolationsRefuseTheFile`,
  `duplicateClockDomainIdIsRefused`, `malformedWallClockCorrelationIsRefused`,
  `numericHeaderShapesRefuseOutOfRangeValues`, `lengthArithmeticDoesNotOverflow`,
  `permittedOptionalMembersAreAccepted`, `localProfileValuesAreNotAnAdmissionTest`,
  `recordLengthBelowThePrefixIsRefused`, `recordLengthAboveTheLimitIsRefusedBeforeRecovery`,
  `inconsistentRecordLengthsRefuseTheWholeFile`,
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
- [ ] The source count is the one rule M10 adds to ADR-006: a file with more than one
  source is refused with the fixed message and no partial rendering, a file with none is
  refused with its own reason, and M10 adds no rule of its own about the number of clock
  domains (ADR-011 D2) - `multiSourceFileIsRefusedWithTheM10Message`,
  `fileWithoutASourceIsRefused`, `clockDomainCountIsNotAnM10Rule`.
- [ ] Passive replay distributes the stored, immutable events in stored order to its
  consumer, with no gaps, duplication or modification, without mutating the session and
  with a report at the end and from `stop()` -
  `playDeliversEveryEventInStoredOrder`, `deliveredEventsAreTheStoredEvents`,
  `immediateTimingSchedulesOneDeliveryPerCallback`,
  `recordPtySessionThenLoadAndReplayByteExactly`,
  `startPositionsAtTheFirstEventAndNeverMutatesTheSession`,
  `replayFinishedReportsDeliveredRemainingAndTiming`,
  `closeReleasesTheSessionAndReturnsToIdle`, `closeWhilePlayingCancelsAndReturnsToIdle`,
  `refusalsAreValuesThatChangeNothing`.
- [ ] The timing ladder is complete and exact: original timing and 1x are the stored
  gaps, immediate is scheduled with delay 0, each scale multiplies or divides the stored
  gap with truncation, and no policy ever rewrites stored timing -
  `originalTimingUsesTheStoredSessionTimeGaps`, `scaleOneEqualsOriginalTiming`,
  `eachScaleMultipliesOrDividesTheStoredGap`, `equalStoredTimestampsDeliverWithoutDelayInOrder`,
  `changingTimingAppliesToTheNextEventOnly`, `theTimingLadderIsPresentInOrder`,
  `refusedTimingSelectionRestoresTheSelectorAndReportsIt`.
- [ ] Step mode offers next event, next RX and next TX, delivers the inclusive prefix,
  never advances on its own, refuses `play()` while selected and refuses to be selected
  while `Playing` (no implicit pause, no mixed state); "next decoded frame" is absent
  (M11) - `playingIsRefusedInStepMode`, `selectingStepWhilePlayingIsRefused`,
  `stepDeliversTheInclusivePrefixAndTheMatch`,
  `stepNextRxDeliversEveryEventUpToAndIncludingTheNextRx`,
  `stepNextTxDeliversEveryEventUpToAndIncludingTheNextTx`,
  `stepPastTheLastMatchAdvancesToTheEndAndReportsNoMatch`, `stepIsRefusedWhilePlaying`,
  `stepAfterFinishIsRefused`.
- [ ] Navigation is forward-only: start/restart, play/stop and the three steps exist, and
  no seek, position setter or range operation does -
  `stopPreservesPositionAndCount`, `playResumesFromThePreservedPosition`,
  `restartReplaysTheWholeStreamAgain`, `noSeekOperationExists` (static inspection,
  recorded as such in the self-review).
- [ ] The offline state is explicit, exclusive and **process-wide**: the indicator reads
  exactly `Offline Session — Passive Replay`, every output-relevant control of §5.7.1 -
  including `New &Window` and `Edit → Paste` - is disabled and every one of those entry
  points refuses when it is called directly, and loading is refused while any window has a
  live session, a recording runs in any window or any window has a session loaded, without
  disturbing what it refuses to replace -
  `theOfflineIndicatorAppearsExactlyWhileOffline`,
  `everyOutputRelevantControlIsDisabledWhileOffline`,
  `invokingEachOfflineGuardedEntryPointReachesNoTransport`,
  `keyInputPasteAndMacrosWhileOfflineReachNoTransport`,
  `loadingIsRefusedWhileTheSessionIsLive`, `loadingIsRefusedWhileARecordingRuns`,
  `loadingIsRefusedWhileAnOfflineSessionIsLoaded`,
  `loadingIsRefusedWhileAnotherWindowsSessionIsLive`,
  `loadingIsRefusedWhileAnotherWindowHasASessionLoaded`,
  `newWindowIsRefusedWhileAnyWindowIsOffline`,
  `offlineStateDisablesControlsInEveryOpenWindow`, `offlineGuardRefusesInEveryWindow`,
  `closingTheOfflineSessionRestoresTheLiveControls`, `loadingIsNotPersistedAcrossRuns`.
- [ ] No code path can transmit: the player and reader hold no transport and no output
  target, a replayed byte cannot answer the terminal, the offline state closes the
  user-facing transmit path, replay starts without a confirmation, and no bytes leave the
  process or reach a peer during loading or replay. The claim rests on the
  source/dependency audit over the offline units *and* the replay entry points/reply sites,
  on `ITransport::bytesWritten` never being emitted, and on the peer's silence; the
  widget-free compile check is stated as covering widget/application types only -
  `offlineUnitsNameNoWriteEntryPointOrTransport`,
  `replayEntriesAndReplySitesPassTheSourceAudit`,
  `replayedTerminalQueriesProduceNoReply`, `liveTerminalQueriesStillAnswer`,
  `noTransmitDuringReplay`, `invokingEachOfflineGuardedEntryPointReachesNoTransport`,
  `keyInputPasteAndMacrosWhileOfflineReachNoTransport`,
  `theOfflineStateNeverOpensConfiguresOrWritesTheTransport`,
  `recordedSettingsAreNeverApplied`, `replayingARecordedPtySessionTransmitsNothing`, and
  the widget-free contract check.
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
| The offline state leaks a transmit path (key input, paste, macro bar, upload, download, recording start, settings, profile load, or a second window's connection) | Every output-relevant control and entry point is enumerated in §5.7.1 - including `New &Window`, whose slot constructs a window that would load a profile and open a port - disabled *and* guarded in the slot itself, and each one is invoked directly in a test; the offline contracts hold no transport at all. The disabled state and the guard are process-wide, so an already-open second window is covered as well (`offlineStateDisablesControlsInEveryOpenWindow`, `offlineGuardRefusesInEveryWindow`) |
| A replayed terminal query is answered on the wire, because the emulation writes its replies to the port | The replay path has its own rendering entry point and the emulation's terminal-generated replies have a single choke point that this entry point suppresses for the duration of the call; `replayedTerminalQueriesProduceNoReply` replays `CSI 5 n`, `CSI 6 n`, `CSI c` and `ESC Z` over a real pty, and `liveTerminalQueriesStillAnswer` pins that the live path still answers (ADR-011 D14) |
| A window that was already open keeps a live connection while another window replays | The offline state is process-wide (ADR-011 D11): loading requires every window idle, `New &Window` is refused while any window is offline, and the cross-window walk is tested (`loadingIsRefusedWhileAnotherWindowsSessionIsLive`, `newWindowIsRefusedWhileAnyWindowIsOffline`) |
| A safety claim is accepted because a boundary looks reassuring instead of excluding the forbidden type (`ITransport` is itself QtCore-only, so the widget-free check cannot see it) | §5.8 separates the bounded compile claim from an explicit source/dependency audit and from dynamic no-write evidence over a real pty, and states which property each check does and does not prove |
| The loader drifts into a second, M10-local format contract and starts refusing v1-conformant files | ADR-006 is cited as the sole normative source, every loader-contract row names the ADR-006 rule it applies, and M10's own rule is exactly one row (the source count); `permittedOptionalMembersAreAccepted` and `localProfileValuesAreNotAnAdmissionTest` pin that ADR-006's optional content and a different-but-conformant profile are not refusals |
| A file from a future writer version is misread | The reader accepts exactly version 1 and encoding 1 and refuses anything else with a reason; there is no guessing, no heuristic parse and no partial interpretation of an unknown version |
| Multi-source files arrive before M14 and are half-rendered | Refused as early as the header validation (row 29 of §5.3.3) with a fixed, tested message, and refused by the only loader, so no other entry point can bypass it |
| Users expect a seek/slider and find restart only | Explicit non-goal, documented in the ADR (D9) and here, exposed in the user-visible refusal wording where a seek-like operation would be expected; the internal store stays seek-ready so the later feature is additive |
| The recovered-prefix rule is mistaken for "the file is fine" | The recovery is reported in the load outcome, in the user-visible summary and in the acceptance criteria, and the truncated record is proven absent from the delivered stream |
| The stream checks of §5.3 refuse a file a laxer future writer might produce | Deliberate (ADR-011 D3): presenting an inconsistent stream as an ordered session is worse than refusing it; the reason names the record and the property, and a future format version can relax it explicitly |

## 10. Decisions

All decisions this specification depends on are recorded in ADR-011 as D1-D14 and are
implemented here without reinterpretation:

| ADR decision | Where this specification implements it |
| --- | --- |
| D1 reader, not transport | §2, §5.1, §5.2, §5.8 |
| D2 ADR-006 applied as the sole normative format source, header interpreted once, exactly one format restriction of M10's own (the source count, row 29) | §5.3, §5.4 |
| D3 record, event and stream validation; recovery rule | §5.3.1 (rows 20-22), §5.3.2 (rows 25-28) |
| D4 all-or-nothing | §5.3, §6 |
| D5 immutable, verbatim | §5.5, §6 |
| D6 the player interface | §5.5 |
| D7 timing ladder and arithmetic, `Step` refused into and out of `Playing` | §5.5, §5.6 |
| D8 step semantics | §5.6 |
| D9 forward-only | §3, §5.5, §9 |
| D10 one materialised session, stateless reader | §5.9, §5.10, §9 |
| D11 the offline state (explicit, process-wide, reversible), the loading preconditions, the complete control enumeration and the entry-point guards | §5.7, §5.7.1 |
| D12 recorded settings are informational | §5.7, §9 |
| D13 document ownership and its operations | §5.9 |
| D14 no code path can transmit: the mechanisms, the reply choke point, and the audit plus dynamic no-write evidence that check them | §5.7, §5.7.1, §5.8 |

No decision remains open in this specification. The refusal string of §5.3.3 row 29 (and
its "the file declares no source" companion reason), the three loading-refusal reasons of
§5.7, the offline refusal of the guarded entry points (§5.7.1), the indicator text and the
operation and state names of §5.5 are the normative strings and names this milestone's
tests assert; changing any of them is a specification change, reviewed like any other.

## 11. Implementation steps

1. `SessionReader` with its byte-source seam, its constants, the header validation that
   applies ADR-006's required members and shapes while accepting its permitted optional
   content, the record/event/stream validation, ADR-006's recovery rule and the load
   outcome type, as its own reviewed slice with `tst_sessionreader` (including the
   writer-parity test, the byte fixture, `permittedOptionalMembersAreAccepted`,
   `localProfileValuesAreNotAnAdmissionTest`, `duplicateClockDomainIdIsRefused`,
   `malformedWallClockCorrelationIsRefused`, `numericHeaderShapesRefuseOutOfRangeValues`,
   `lengthArithmeticDoesNotOverflow`, `recordLengthBelowThePrefixIsRefused` and
   `recordLengthAboveTheLimitIsRefusedBeforeRecovery`).
2. `SessionFile`/`SessionFileInfo`/`SessionSourceDescriptor`/`SessionClockDomain` and
   the widget-free contract check entry for the new units (ADR-009), so the boundary is
   compiled from the first commit.
3. `SessionReplayPlayer` with its five states, the seven operations, the timing
   arithmetic (including the refusal of `Step` while `Playing`, returned as
   `SessionReplayTimingChange`), the scheduler and clock seams, the delivery signal and
   value-returned refusals, with `tst_sessionreplayplayer`.
4. The reply-free rendering path: `KomportEmulation::sendTerminalReply()` as the single
   choke point, `KomportEmulation::slotReplayReceivedChar()`,
   `KomportView::slotReplayReceivedChar()` and `KomportView::setKeyInputEnabled()`, with
   the tests that replay `CSI 5 n`, `CSI 6 n`, `CSI c` and `ESC Z` without a reply and pin
   that the live entry points still answer - the slice that makes review finding 1
   structural rather than procedural.
5. `KomportDoc` ownership, `loadOfflineSession()` with the process-wide preconditions (the
   top-level-window walk) and `closeOfflineSession()`, the destruction order and the
   document-level tests (`tst_sessiondocument`-style additions or a new document test
   target).
6. Application wiring: the `Load Session for Replay...` action and its dialog, the
   `offlineReplayStatusLabel` indicator, the complete control enumeration of §5.7.1 (the
   disabled controls - including `New &Window`, paste and the recording actions - *and* the
   offline refusal inside every guarded entry slot), the process-wide UI update that applies
   the disabled state to **every** open window and the one shared offline predicate behind
   the guards, the replay
   controls with the timing ladder and the step actions (each handler consuming the
   player's result value, §5.7), the read-only offline display
   adapter into `KomportView` and `KomportHexView` through the replay entry point, the
   outcome reporting through the status mechanism, with `tst_sessionreplayui`.
7. The source/dependency audit tests (`offlineUnitsNameNoWriteEntryPointOrTransport`,
   `replayEntriesAndReplySitesPassTheSourceAudit`) and the dynamic no-write evidence -
   `ITransport::bytesWritten` never emitted plus the silent peer - over a real pty:
   `replayedTerminalQueriesProduceNoReply`, `liveTerminalQueriesStillAnswer`,
   `noTransmitDuringReplay`, `replayingARecordedPtySessionTransmitsNothing`.
8. `tst_sessionreplayendtoend` over a real pty (record with M9's recorder, load, replay,
   byte-exact comparison, no transmission), the regression run of the whole suite and the
   warning-free build under `-Wall -Wextra`.
9. Self-review against this specification (including the static-inspection items:
   `noSeekOperationExists`, `offlineUnitsNameNoWriteEntryPointOrTransport`), then the
   independent review.
