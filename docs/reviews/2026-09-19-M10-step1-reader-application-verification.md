## Verdict

**No — do not commit yet.** The code fixes are sound, but row 1’s amendment is still awaiting the owner’s ratification while the accepted spec says no decision remains open. That is an unresolved architecture/process decision.

## Disposition

| Item | Status | Evidence |
| --- | --- | --- |
| 1. Explicit `null` correlation | Resolved | Only `isUndefined()` is absent; `null` is refused at [sessionreader.cpp](/home/max/Development/misc/komport-qt6/komport/sessionreader.cpp:294). Test covers both a valid absence and explicit null. |
| 2. `localBuffering` restrictions | Resolved | Only `isDouble()` is required at [sessionreader.cpp](/home/max/Development/misc/komport-qt6/komport/sessionreader.cpp:265). The only remaining integral/range helper call is for `sourceId` at line 389, as ADR-006 requires. |
| 3. Header ordering | Resolved in code/spec | Row 13 resolution precedes row 14; rows 15–17 are whole-array passes at [sessionreader.cpp](/home/max/Development/misc/komport-qt6/komport/sessionreader.cpp:404). The amended §5.3 matches this precedence. |
| 4. Open-failure detail | Partially resolved — blocking | Code and amended row 1 agree on the base reason. But the self-review says owner confirmation is outstanding at [self-review](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-19-M10-step1-reader-implementation-selfreview.md:113), conflicting with SPEC-M10’s “No decision remains open.” |
| 5. `bytesRead` wording | Resolved | [sessionreader.h](/home/max/Development/misc/komport-qt6/komport/sessionreader.h:121) now correctly includes discarded recovered bytes. |
| 6. Boundary fixtures | Resolved | Exact 16 MiB is constructed against `SessionReader::kMaxHeaderBytes`; the `+1` rejection, i64 spelling/range cases, null correlation, and out-of-range precision are present. |
| §5.3 ordering amendment | Resolved | It accurately states the implemented passes and first-failing-row rule. |
| §5.3.1 row 1 amendment | Partially resolved — blocking | Semantically unambiguous and code-aligned, but not owner-ratified. |

## New findings

- **ARCHITECTURE QUESTION — blocking** — [SPEC-M10 §5.3.1 row 1](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:294), [self-review](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-19-M10-step1-reader-implementation-selfreview.md:98), [SPEC-M10](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1233)  
  The implementation author records the owner decision as still outstanding, but the accepted spec says no decision remains open.  
  Minimal fix: owner formally ratifies or rejects the row-1 amendment, and the review/spec trail is made consistent.

- **DOCUMENTATION — same-pass** — [sessionreader.cpp](/home/max/Development/misc/komport-qt6/komport/sessionreader.cpp:134), [self-review](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-19-M10-step1-reader-implementation-selfreview.md:40)  
  Both still describe integral/ranged JSON-number validation as applying to row 14. That is now false.  
  Minimal fix: limit the helper comment to `sourceId`/row 13 and correct self-review row 14 to “JSON numbers.”

- **DOCUMENTATION — same-pass** — [self-review](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-19-M10-step1-reader-implementation-selfreview.md:63), [self-review](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-19-M10-step1-reader-implementation-selfreview.md:81)  
  There are 36 test methods in the source, not 38. QTest’s “38 passed” includes `initTestCase` and `cleanupTestCase`; the two added tests also mean there are 34 planned methods, not 36.  
  Minimal fix: state 36 test methods / 38 QTest pass entries, including setup and cleanup.

- **TEST GAP — same-pass** — [tst_sessionreader.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessionreader.cpp:574), [tst_sessionreader.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessionreader.cpp:598)  
  `does not resolve` also occurs in row 26, and `session time` also occurs in row 28; the fixtures make the intended header row evident, but the substrings are not globally unique.  
  Minimal fix: assert the distinguishing full reason or a unique phrase such as `clockDomains[] entry` and `does not reference session time exactly`.

The domain restructuring itself is safe: malformed unrelated domain entries still refuse under row 15, and facts for a source resolving to the second domain are copied correctly.

## Verification limits

The direct `tst_sessionreader` binary ran the fixed tests successfully, including the new ordering/buffering/boundary cases. It ended **35 passed, 3 failed** only because this read-only sandbox cannot create `QTemporaryDir`; all three failures occur at `directory.isValid()` before reader assertions. `ctest` likewise cannot write `build/Testing/Temporary/LastTest.log`.