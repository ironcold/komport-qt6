## Verdict

**No — do not commit this slice as it stands.**

Blocking, in priority order:

1. `wallClockCorrelation: null` is silently accepted although a present optional member must have the specified object shape.
2. The reader adds an unsupported admission restriction to `localBuffering` numbers.
3. Header checks do not follow the specified row order, so prescribed refusal precedence/reasons are not reliable.
4. The accepted spec requires a specific open-failure reason, but its fixed seam cannot supply one; this needs an owner/spec decision.

## Findings

- **HIGH — blocking** — `komport/sessionreader.cpp:291`

  An explicitly present `wallClockCorrelation: null` is accepted as if absent. SPEC-M10 §5.3.1 row 17 requires a present correlation to contain an ISO-8601 UTC `wallClock` and canonical-i64 `precisionNs`; `null` is neither absent nor that object.

  Minimal fix: accept only `isUndefined()` as absent; reject `null`. Add a fixture for explicit JSON `null`.

- **HIGH — blocking** — `komport/sessionreader.cpp:265-271`

  `rxQueue` and `flushRate` are restricted to non-negative integral signed-32-bit values. ADR-006 and SPEC-M10 row 14 require these fields to be JSON numbers, not integer/range-constrained values. This is an unapproved second reader admission test, contrary to ADR-011 D2; it can reject an otherwise conformant file.

  Minimal fix: require `QJsonValue::isDouble()` only. Add accepted fixtures for fractional, negative, and above-`INT32_MAX` JSON numbers. If those values were intended to be invalid, amend ADR-006 first rather than tightening the reader locally.

- **MEDIUM — blocking** — `komport/sessionreader.cpp:328-359`, `392-463`

  The header table is not implemented in its listed order despite SPEC-M10 §5.3 stating that every check is ordered. In particular, row 11 runs before row 10, and the source-domain-resolution part of row 13 runs only after rows 14-17. Combined-invalid fixtures therefore return a later/different refusal than the contract prescribes. `tests/tst_sessionreader.cpp:572-580` only asserts refusal, not ordering or the required reason.

  Minimal fix: implement the table’s precedence explicitly and add combined-invalid tests that assert the winning reason. If the intended dependency order is the current one, amend the accepted table before committing.

- **ARCHITECTURE QUESTION — blocking** — `komport/sessionreader.h:98-101`, `komport/sessionreader.cpp:541-543`

  SPEC-M10 row 1 says the specific open-failure reason is reported. The specified seam exposes only `bool open()`, and the implementation returns only the base sentence. This is a real spec/implementation mismatch, correctly disclosed by the author.

  Minimal fix: owner chooses one: amend row 1 to make the base message sufficient, or amend the seam to expose a diagnostic and report `QFile::errorString()` in production.

- **DOCUMENTATION — same-pass cleanup** — `komport/sessionreader.h:121-126`, `docs/reviews/2026-09-19-M10-step1-reader-implementation-selfreview.md:102-104`

  The statement that success means `bytesRead` is “the complete session” is false for a recovered load: the implementation consumes the discarded truncated suffix too. The implementation’s count is reasonable; the wording is not.

  Minimal fix: describe `bytesRead` as all bytes consumed from the source, including a recovered truncated suffix.

- **TEST GAP — same-pass cleanup** — `tests/tst_sessionreader.cpp:466-480`, `585-615`, `676-721`

  The tests cover header length *above* 16 MiB but not exactly 16 MiB; a `>=` regression would pass. Canonical-decimal coverage also lacks signed boundary/spelling cases such as `INT64_MIN`, `-1`, `-0`, `+1`, and whitespace, and does not test out-of-range `wallClockCorrelation.precisionNs`.

  Minimal fix: add exact-limit and canonical-i64 boundary/spelling fixtures. The u32-maximum `sourceId`, 64 MiB record boundary, and “do not consume after invalid record length” are already meaningfully covered.

## Confirmed points

Rows 18/19 are correctly decided from `recordLength` alone before body parsing or recovery (`sessionreader.cpp:642-648`); row 20 is the only no-length recovery case; rows 21-24 then operate on a complete body. A malformed complete record is refused rather than recovered.

The reader is QtCore-only and has no transport/controller/widget dependency in its two units. The Core-only contract target includes `sessionreader.cpp`. The production reader is read-only, does not retain the file handle, and returns a shared pointer to const session data.

The M9 parser is independent of the production reader: it is test-only, has its own parsing implementation, and the reader itself includes neither it nor `SessionRecordCodec`.

The split items are honest for this slice: the zero-record player behavior belongs to the player slice, and live-path proof belongs to the document slice. They remain milestone obligations; they are not missing reader-slice code.

## Self-review disclosures

The document has six numbered disclosures, not seven.

1. Open-failure detail: **needs owner decision**; current code does not meet the accepted wording.
2. Zero-based record indices: **confirmed**; the spec fixes no base.
3. Extra members in `application`, `reference`, and `wallClockCorrelation`: **confirmed** under the accepted reader rules; only the configuration nested schema is explicitly exhaustive.
4. Richer specific refusal text: **confirmed**.
5. `bytesRead` description: **wrong** for recovered success; documentation correction needed.
6. Independence claim: **confirmed**.

## Verification limits

I could not run the claimed build or tests: the read-only sandbox prevented CTest from creating `build/Testing/Temporary/LastTest.log`. I therefore did not run the warning build or verify “18/18” dynamically.

Statically, `tst_sessionreader.cpp` contains 34 test slots; a reported total of 36 may include Qt Test lifecycle passes, but I could not verify that execution claim.