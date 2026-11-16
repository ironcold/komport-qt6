# M10 implementation self-review, slice 1: `SessionReader` (SPEC-M10 §11 steps 1-2)

Status: self-review of an implementation slice, before the independent review of the
same slice. Date 2026-09-19. Baseline: `v2/m10-session-reader-replay`, after the gate
("ADR-011 and SPEC-M10 accepted", commits `b9169f5` + `be2020a`).

## What the slice contains

SPEC-M10 §11 steps 1 and 2, in one reviewable unit because the value types are the
reader's return values:

| File | Change |
| --- | --- |
| `komport/sessionreader.h` | new: `SessionSourceDescriptor`, `SessionClockDomain`, `SessionFileInfo`, `SessionFile`, `SessionFilePtr`, `SessionByteSource` (§5.10 seam), `SessionLoadOutcome`, `SessionReader` |
| `komport/sessionreader.cpp` | new: the production `QFile` byte source and the loader (`load()`) |
| `tests/tst_sessionreader.cpp` | new: 36 test functions, the reader table of §7 |
| `tests/CMakeLists.txt` | `komport_add_test(tst_sessionreader)`; `sessionreader.cpp` added to the widget-free contract check |
| `tests/session_contract_compile.cpp` | includes `sessionreader.h`, `static_assert` on `SessionReader`, touches the value types (ADR-009) |
| `CMakeLists.txt` | `sessionreader.cpp/.h` in `komport_core` |

Not in this slice (by the plan of §11): the player, the seams of §5.10 for the player,
the replay rendering entry points, the offline state and the application wiring. The
reader is the only new unit and it is complete on its own.

## The loader contract, row by row (SPEC-M10 §5.3.1/§5.3.2/§5.3.3)

| Row | Where it is implemented | Reason string (as specified) |
| --- | --- | --- |
| 1 open / not a regular file | `FileByteSource::open()` refuses a path that does not exist or is not a regular file; `SessionReader::load()` reports it | "the file could not be opened" |
| 2 I/O error | `readUpTo()` maps a negative read to the refusal | "the file could not be read" |
| 3 fewer than 16 bytes | the start-block read | "shorter than a start block" |
| 4 magic | `startBlock.left(8) != magic()` | "bad magic" |
| 5 version, 6 encoding | the `u16` at offsets 8 and 10 | "unsupported format version N" / "unsupported header encoding N" |
| 7 header > 16 MiB | decided from the declared `u32` **before** the header is read (the test asserts 16 bytes consumed) | "the header exceeds ADR-006's 16 MiB limit" |
| 8 header longer than the file | `size() - 16 < headerLength`, plus the short-read path for a source without a size | "the header length exceeds the file" |
| 9 header JSON | `QJsonDocument::fromJson(...).isObject()` | "the header is not a JSON object" |
| 11 required members and kinds, 10 their values | `validateHeader()`, presence/kind first, then `format`/`version`/`created`/`application` | names the member |
| 29 source count | `validateHeader()`, before the descriptor is interpreted: `> 1` → `multiSourceRefusal()` verbatim, `0` → "the file declares no source" | `multi-source sessions not supported in M10` |
| 13 descriptor | `sourceId` integral `1..4294967295`, `clockDomainId` string resolving to an entry, `name`/`transport` strings, `configuration` an object | names the violation |
| 14 configuration schema | `configurationViolation()`: the four sections exactly, the six hardware members as strings, `rxQueue`/`flushRate` as JSON numbers, `startBits` a string | names the violation |
| 15 clock domains | every entry: object, string `id`, unique in the file, `kind` one of ADR-006's two | "two clock domains share the id '...'" |
| 16 reference pair | `isCanonicalDecimalString()` for both members, `sessionTimestampNs` exactly `"0"` | names the violation |
| 17 `wallClockCorrelation` | validated when present (ISO-8601 UTC `wallClock`, canonical `precisionNs`), never interpreted, never a refusal reason on its own | names the violation |
| 18 `recordLength < 40` | from the four declared bytes, before the prefix fields are parsed | "record N declares a length below its own prefix" |
| 19 `recordLength > 64 MiB` | from the four declared bytes, before the truncation rule | "record N exceeds ADR-006's 64 MiB limit" |
| 20 fewer than 4 bytes left | `readUpTo(4)` returning 0 < bytes < 4 | recovered |
| 21 `4 + recordLength` past EOF | the body read short-reading after rows 18/19 passed | recovered |
| 22 lengths inconsistent | subtractive checks (`metadataLength > contentLength`, `payloadLength > contentLength - metadataLength`, then equality) | "record N has inconsistent lengths" |
| 23 flags | `flags != 0` | "record N has non-zero flags" |
| 24 metadata | `metadataLength > 0` ⇒ the bytes must be a JSON object; `0` ⇒ the empty object | "record N has non-object metadata" |
| 25 event-local (ADR-002) | `isValidSessionEvent()` on the event built from the prefix | "record N is not a valid SessionEvent: ..." |
| 26 source resolution | `event.sourceId != info.source.sourceId` | "record N's source does not resolve to the session's source" |
| 27 sequence | `sequence == 0 \|\| sequence <= previous` | "record N's sequence does not continue the stream" |
| 28 session time | `timestampNs < previous` | "record N's session time decreases" |

Ordering and arithmetic: the tables' order is implemented, the two `recordLength` checks
are decided from the declared value alone before the fields are parsed and before the
truncation rule, every length is validated before the bytes it describes are read or
allocated, and all length arithmetic is `qint64` with subtractive bounds checks.

## Test mapping (§7 reader table → shipped test)

§7's reader table plans 35 tests. 34 of them are shipped in this slice;
`theReaderNeverTouchesTheLivePath` is deferred to the document slice (§11 step 5). The fix
round added two tests the plan does not name
(`combinedHeaderViolationsReportTheFirstFailingRow`, `bufferingMembersAreOnlyJsonNumbers`),
which §8's clause about unlisted tests covers: they are extra evidence, not renamed plan
items. The slice therefore ships **36 test methods**; QTest reports 38 pass entries, which
includes `initTestCase` and `cleanupTestCase` (review: application verification, same-pass
item 2 - the earlier wording of this section counted the QTest entries as methods).
Three items of the table are deliberately **split**, which §8 allows because
§7 says the plan is the design and §8 carries the shipped mapping:

| §7 item | Status in this slice |
| --- | --- |
| `zeroRecordFileIsAValidSessionWithNoEvents` | reader half implemented and passing; the second half ("`play()`/steps refuse with a reason") belongs to the player slice and is asserted there |
| `theReaderNeverTouchesTheLivePath` | **deferred to the document slice** (§11 step 5): the assertion needs a live document with a pty-backed port, and `tst_sessionreader` is specified as a pty-free target (§7). The reader itself holds no transport type at all (source audit of §5.8 covers the offline units) |
| `noSeekOperationExists` | player surface, player slice |

## Evidence

- `cmake -B build -DCMAKE_CXX_FLAGS="-Wall -Wextra" && cmake --build build`: **no warning,
  no error** (checked for the new units and the changed CMake/test files).
- `ctest`: **18/18 targets pass**, of which `tst_sessionreader` is 36 test methods (QTest
  reports 38 pass entries: the methods plus `initTestCase` and `cleanupTestCase`); the M9
  regression suite is included and unchanged.
- The widget-free contract check (`komport_session_contract_check`) now compiles
  `sessionreader.cpp` against `Qt6::Core` alone, so a widget or application type entering
  the reader breaks the build.

## Fix round after the independent slice review

`docs/reviews/2026-09-19-M10-step1-reader-implementation-review.md` blocked the slice with
four blocking findings and two cleanups. Disposition (the review is unchanged; this section
is the fix record):

| Review item | Disposition |
| --- | --- |
| 1 HIGH — `wallClockCorrelation: null` accepted as absent | fixed: only `isUndefined()` is "absent", an explicit `null` is refused; fixture added |
| 2 HIGH — `localBuffering` restricted to integral 32-bit (a second admission test) | fixed: the members must be JSON *numbers* and nothing more (ADR-006's wording); the writer profile's integral rule stays the writer's. New test `bufferingMembersAreOnlyJsonNumbers` with fractional, negative and above-`INT32_MAX` accepted fixtures and string/boolean/missing refusals |
| 3 MEDIUM — header checks not in the table's order, only "refused" asserted | fixed: row 13 (fields **and** its domain resolution) now precedes row 14, and rows 15/16/17 are applied as whole-array passes instead of per entry; new test `combinedHeaderViolationsReportTheFirstFailingRow` with four combined-invalid fixtures asserting the winning reason. The one irreducible dependency (presence/kind before values) is now **stated in the accepted SPEC** instead of being implicit: amendment of 2026-09-19 in §5.3 |
| 4 ARCHITECTURE QUESTION — row 1 promises an open-failure detail the §5.10 seam cannot supply | owner decision: **the text was amended, not the seam** (my recommendation, taken while the owner was unavailable and to be confirmed by him). SPEC-M10 §5.3.1 row 1 now says the loader reports its own rule's reason, that the seam reports success only, and that extending the seam was considered and not chosen. No API growth, no code change |
| 5 DOCUMENTATION — `bytesRead` misdescribed for a recovered load | fixed in `sessionreader.h` and in this document: `bytesRead` is every byte consumed, including the discarded truncated suffix |
| 6 TEST GAP — missing boundary fixtures | fixed: the exact-16-MiB header is asserted admissible (and the `+ 1` case refused), the canonical-decimal boundaries/spellings are covered (i64 min/max and `-1` accepted; `+1`, `-0`, leading/trailing space, leading zero, one past the upper bound refused), an out-of-range `precisionNs` and an explicit `null` correlation are covered |

Two numbered **amendments inside the accepted SPEC-M10** carry this round (both dated
2026-09-19, both documenting the review round that produced them): §5.3's ordering sentence,
and §5.3.1 row 1's parenthetical. No other accepted document changed.

## The owner's ratification of the row-1 amendment (resolved)

The application verification (`docs/reviews/2026-09-19-M10-step1-reader-application-verification.md`)
confirmed every code fix and every test addition and blocked the commit on one thing only:
SPEC-M10 §5.3.1 row 1's amendment was taken on the implementer's recommendation while the
owner was unavailable. **The owner ratified the text amendment on 2026-09-19** ("amendment
passt"): the loader reports the reason of its own rule, the §5.10 seam stays exactly as the
accepted specification fixes it, and no diagnostic method is added. The specification now
carries the ratified form instead of the provisional marker, so the trail is consistent: the
amendment is a settled decision, not an open one.

## Disclosures (six deviations, choices and gaps a reviewer should check)

1. **A failed open reports no operating-system detail** (resolved by amendment, not by
   code): SPEC-M10 §5.3.1 row 1 promised "the specific reason is reported" while the §5.10
   seam fixes `open()` to `bool`. The slice review raised this as an owner decision; the
   text was amended on 2026-09-19 (row 1 now states that the loader reports its own rule's
   reason and that the seam reports success only), so the code and the specification agree
   without growing the seam. The owner's confirmation of that choice is still outstanding.
2. **Record indices in the reasons are zero-based**, like the record list M9's test-side
   parser reports; the specification does not fix a base and the tests do not depend on it.
3. **Additional members in `application`, `reference` and `wallClockCorrelation` are
   ignored**, not refused: ADR-006 declares only the nested `configuration` sections
   exhaustive ("None of these objects carries an additional member"), and §5.3.1 row 12's
   principle (additional content is never a reason to refuse) was applied consistently. A
   reviewer may prefer a stricter reading for `application`; that would be a spec change.
4. **The reader's refusal reasons are the spec's text**, but two of them are richer than
   the table's example wording (`record N is not a valid SessionEvent: <why>`, and the
   configuration/domain violations name the member). The table asks for "the specific
   violation", so this is the intended reading.
5. **`bytesRead` is every byte the loader consumed from the source** - on success that
   includes the discarded truncated suffix of a recovered record, so it measures what was
   read, not the delivered session's size. The header states this after the review round.
6. **`grep`-able independence:** `sessionreader.cpp` includes neither `sessionrecordcodec.h`
   nor `tests/sessionrecordreader.h`; the writer parity is a test
   (`readerLimitsMatchTheWriterCodec`) and the cross-checks use M9's parser from the test
   side only.

## Corrections to my own work during this slice (kept for the record)

- `recordViolation()` first read `event.flags` - `SessionEvent` has no `flags` member; the
  flags check belongs to the record bytes and stays in the loader.
- The session value was first built through a `const_cast` on the `shared_ptr`; it is now
  built as `std::make_shared<SessionFile>` and handed out as the const view.
- Three fixtures were wrong, not the reader: a non-data record carrying a payload (row 25
  refuses it), a header-only boundary fixture whose record still had to resolve the
  descriptor's `sourceId` (row 26), and a "row 22" fixture that left the declared body
  absent, so the truncation rule applied before the field check - which is the specified
  order. The fixtures were corrected; the reader was not touched.
- The oversize fixture in `headerAboveSixteenMibIsRefusedBeforeItIsRead` writes its
  declared length through `qToLittleEndian` instead of hand-patching bytes on the host's
  endianness.
