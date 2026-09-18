# M9 implementation self-review (2026-09-18)

M9 is "live session recording" (`.kpsession`): the recorder that turns the ordered
live event stream of an M8 session into ADR-006's v1 container file, plus the
application wiring that starts and stops it. This document is my own review of the
milestone before the independent closing review; it states what shipped, maps the
accepted acceptance criteria to real tests, and lists every deviation and open item
I know of, including the ones that are unflattering.

## 1. What was implemented

| Slice | Commit | Content |
|---|---|---|
| Gate | `249f906` | ADR-010 and SPEC-M9 accepted, plus the ADR-006 v1 writer profile and the SPEC-M8 controller accessor |
| 1 | `db7a7c3` | `SessionRecordCodec`: the start block (magic, version, encoding, u32 length, ADR-006 profile header) and one record per event, with checked arithmetic and an exact allocation-free JSON size counter |
| 2 | `18564d1` | `SessionController::clockDomainReference()`: the read-only anchor pair a mid-session consumer needs |
| 3 | `e82728f` | `SessionRecorder`: streaming, start flush, one-second periodic flush, damage transitions, reports, sink/scheduler/clock seams |
| 4 | `ddd3b71` | `KomportDoc` owns the recorder (controller destroyed first, then recorder, then transport) and `closeSession()` performs the normative close ordering |
| 5a | `73af458` | `KomportSerial::appliedConfigurationSnapshot()` plus the shared section builder, so the snapshot and the live metadata cannot become two dialects |
| 5b | `fd5daef` | the `Record Live Session...` action, the permanent status indicator, the reporting, the German numerus forms |
| 6 | this slice | the pty end-to-end proof: byte-exact RX/TX and the chunking property |

Review records are committed separately from code, one directory entry per slice
under `docs/reviews/2026-09-18-M9-*`.

## 2. §8 acceptance criteria

The names in §8 of the spec were the design-time plan. My first alignment missed three
of them - §8 still named `pendingTailIsFlushedWithinOneSecondWithoutFurtherEvents`,
`burstFlushesAtMostOncePerSecond` and `flushIntervalBoundsTheBufferedTail`, none of
which exists under that name - and the closing review caught the false *claim* rather
than the tests. Those three planned checks are consolidated into
`theProcessBufferIsBoundedByOneTimerPerBurst` (one schedule per burst, no re-arm, the
interval as the deadline, and a flush on the armed timer without any further event),
and §8 now says so explicitly. The mapping, criterion by criterion:

| §8 criterion | Shipped tests (all green) |
|---|---|
| v1 profile and record layout | `headerProfileMatchesAdr006`, `startBlockPrefixIsExactlyTheAdr006Fixture`, `recordPrefixIsFortyFourBytesInTheAgreedOrder`, `recordBytesAreExactlyTheAdr006LittleEndianFixture`, `oneRecordPerAcceptedEventWithByteExactPayloads`, `headerIsWrittenAtStart`, `theStartFlushMakesTheHeaderVisibleThroughASecondHandle` |
| one record per event, exact payloads and timestamps | `oneRecordPerAcceptedEventWithByteExactPayloads`, `metadataIsWrittenVerbatim`, `sixtyFourBitJsonValuesAreCanonicalDecimalStrings`, `jsonSizeCounterIsExactForEveryValueShape` |
| no session-sized state in the recorder | `theProcessBufferIsBoundedByOneTimerPerBurst`, `aRecordedPtySessionIsByteExactAndChunked`. **Weakest row in this table**: the 64 KiB test exercises `KomportSerial` with a counter, not the recorder, and nothing measures the recorder's memory. The property rests on the class having no container member and on the append-only design (static inspection), not on a measurement - see gap 4. |
| start only while live; header from the start with the true anchor | `startWritesACompleteHeaderAndNeedsALiveSession`, `midSessionStartWritesTheTrueAnchorReference`, `clockDomainReferenceStartsInvalidAndCarriesTheAnchor`, `clockDomainReferenceUsesAFailedOpenAsTheAnchor` |
| bounded writer buffer, one flush per second at most | `theProcessBufferIsBoundedByOneTimerPerBurst` (the three planned flush checks are consolidated into it), `aClockThatStartsAtZeroReportsTheRealDuration` |
| failure paths stop and report; damaged prefix stays valid; crash bounds | `aShortHeaderWriteRefusesTheStartAndLeavesNoLoadableFile`, `aFailedStartFlushRefusesTheStartAndClosesTheSink`, `aShortWriteEndsTheRecordingAsDamagedAndKeepsTheCompletePrefix`, `aShortWriteReportsEveryAcceptedByte`, `aFailedFlushEndsTheRecordingAsDamaged`, `anExplicitStopWhoseFinalFlushFailsReportsItWithoutASignal`, `theReaderRejectsMalformedWriterOutput` (truncated final record recovered) |
| no unit can exceed the limits; offending event refused with sequence and sizes | `oversizedHeaderIsRejected`, `oversizedRecordIsRejected`, `lengthArithmeticIsChecked`, `headerLimitBoundaryIsExact`, `largeAsciiMetadataIsAcceptedWhenItFits`, `anEventThatCannotBeEncodedReportsItsSequenceAndSizes` |
| explicit separate action, logger unchanged, recorder never calls the transport | `anIdleSessionRefusesTheRecordingStart`, `aRecordingShowsItsIndicatorAndStopsWithAReport`, `theRecorderNeverCallsIntoTheTransport`, `aRecordedPtySessionIsByteExactAndChunked` (records per chunk, not per character) |
| full suite passes, warning-free under `-Wall -Wextra` | see §6 |

## 3. §7 testing strategy

| File | Methods | Covers |
|---|---|---|
| `tst_sessionrecordcodec.cpp` | 18 | the container bytes and the profile, without file I/O |
| `tst_sessionrecorder.cpp` | 21 | lifecycle, flush policy, damage paths, reporting, the seams |
| `tst_sessiondocument.cpp` | 5 (+3 M8) | ownership, close ordering over a pty |
| `tst_sessionrecordingui.cpp` | 2 | the action, the indicator and the reporting, offscreen |
| `tst_sessionrecordingendtoend.cpp` | 1 | the pty end-to-end proof |
| `tst_i18n.cpp` | +1 | the German numerus forms at runtime |
| `tst_transport.cpp`, `tst_sessioncontroller.cpp` | +2 each | the 5a accessor, the D8 accessor |

Four deliberate gaps, each reviewed and accepted:

1. **An oversized (>64 MiB) event through the recorder** is not tested: it cannot be
   driven through a conformant controller without a test that allocates 64 MiB, and
   the limit arithmetic itself is covered at the codec level
   (`oversizedRecordIsRejected`, `lengthArithmeticIsChecked`). The recorder's
   encode-refusal path is therefore defensive code; the *reporting* half of it is
   driven directly through the controller's signal.
2. **No application seam forces a filesystem failure**, so the app-level damage
   transition is covered by the recorder's injected-sink tests plus inspection of the
   two-line wiring rather than by an app test. Adding a seam only to force I/O
   failure would widen the production boundary for test convenience.
3. **The pty observation count is not asserted**: chunking belongs to OS/pty
   scheduling, not to the recorder's contract. The asserted property is that
   thousands of bytes become few records and that their concatenation is byte-exact.
4. **The recorder's memory is not measured.** No portable Qt test asserts memory
   behaviour, and the recorder holds no container member by design, so the property
   is established by static inspection. A behavioural volume test - a large burst
   arriving byte-exactly in the file - would strengthen it without measuring memory;
   it is not shipped, which makes this table's memory row the weakest of the nine.

## 4. Deliberate deviations, limits and open items

- Three deviations from the accepted text were made **with an amendment**, never
  silently: the document operation `KomportDoc::closeSession()` (SPEC-M9 §5.6/§5.8,
  ADR-010 §1), the reporting exemption for the closing window (SPEC-M9 §5.7,
  ADR-010 §9) and the snapshot source `KomportSerial::appliedConfigurationSnapshot()`
  (ADR-010 D7, SPEC-M8 §6.2). Each was raised by the review, specified first, and
  verified afterwards.
- Two **limits were stated rather than engineered away**: a flush that fails *before*
  the recording became live is a refused start, not a damage - there is no recording
  to damage (ADR-010 §8, clarified after the closing review) - and the pluralizable
  record count passes through Qt's numerus API, which takes an `int` (ADR-010 §9).
  Both are documented bounds of a status message, not of a recording.
- The **test-side reader is stricter than ADR-006's letter** (flags must be zero, the
  reference pair must be canonical decimal strings, `created` must be ISO-8601 UTC).
  That is intentional: it is test infrastructure and explicitly not M10's reader.
- The **`%n` numerus** change in the summary is new relative to the reviewed 5b text;
  it exists because the first version read "1 complete records" in the user interface.
- Open, none of them blocking M9: the Qt 6.3 acceptance line (local Qt 6.11.1, no CI
  in the repository); the legacy RX-buffer flush item carried over from M8; M10's
  reader and replay player, whose interface is deliberately not frozen here.

## 5. Process notes

Cycle counts: gate 5, codec 5, D8 accessor 1, recorder 4, document 3, 5a 2, 5b 4,
step 6 1. Across the whole milestone the independent review raised 4 blockers,
8 high, 8 medium and 6 test/documentation gaps, and every one of them held when
checked against the code.

Two observations worth recording:

1. **The findings moved, but not wholly, from the code to my verification.** The
   gate, the codec and the recorder rounds found design defects; the late rounds of
   5a and 5b found *checks* that were too weak - a reader that parsed without
   validating, assertions
   that tested wording rather than values, and a numerus "proof" that showed the
   German forms were present but not that they were selected. A tolerant check
   produces green evidence for a false claim, which is the same failure class as an
   over-cautious size estimate in production code.
2. **A technique I adopted mid-milestone:** for every test that guards an ordering, I
   now deliberately reverse the order in the production path, rebuild, and confirm
   that exactly that test fails, before restoring. `closeSession()`'s ordering is
   guarded that way, and the experiment is recorded in the review trail.

Corrections to my own earlier statements, listed rather than buried: the conservative
JSON size factor as a *refusal* threshold would have dropped valid recordings; the
reported "duration" was a session timestamp until the review caught it; the size
counter was inexact for four value shapes in a row; a zero clock reading was treated
as "no recording"; `updateRecordingUi()` claimed an active recording in the damaged
state; "1 complete records" shipped into the German user interface for one round. In
addition, two long edits of mine were truncated by my tooling and briefly left a
production file broken; I detected it at the linker, restored the swallowed function
verbatim from `HEAD`, and had the reviewer verify the repair by hash rather than
taking my word for it.

## 6. Verification evidence (2026-09-18)

- `cmake -B /tmp/komport-m8-build -DCMAKE_CXX_FLAGS="-Wall -Wextra"`, then
  `cmake --build`: **0 warnings**.
- `ctest`: **17/17** targets pass; the M9 targets are
  `tst_sessionrecordcodec`, `tst_sessionrecorder`, `tst_sessionrecordingui`,
  `tst_sessionrecordingendtoend`.
- German catalogue: the new strings, including both numerus forms, were verified byte
  for byte inside the compiled `komport_de.qm` (UTF-16BE), and the runtime selection
  of both forms is asserted by `tst_i18n`.
- The Qt 6.3 line remains unverified (local Qt 6.11.1; no CI in the repository).
