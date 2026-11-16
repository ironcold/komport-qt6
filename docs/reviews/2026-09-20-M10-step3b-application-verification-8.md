## Verdict

**No — do not adopt or commit step 3b yet.**

## Disposition

- The design-note count is fixed: both occurrences now say “eleven plus seven.” [design note](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step3b-design-note.md:14)
- `Finish` implementation and its direct-helper test are now correct: it denies mid-stream and authorizes at the captured end. [code](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:251) [test](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:1809)

## New findings

1. **Blocking: the normative `Finish` table row remains contradictory.** The predicate correctly requires `eventCount() > 0 && mPosition >= eventCount()`, but the table still says “no position is checked.” [spec predicate](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:988) [table](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1001)  
   Update the row to state the exact predicate.

2. **Previously recorded acceptance gap remains unresolved.** The non-interrupting-step test invokes invalid timing, null `start()`, and Step-mode `play()`, but discards their result values; it proves the outer step continues, not that each operation refused. [acceptance row](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1107) [test actions](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:1978) [invocation](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:2006)

3. **Acceptance-row evidence is incomplete for the no-op stop completion report.** The row says the test asserts the completion report’s `delivered` and `position` at end, but the test never checks `lastReport.position`. [row](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1113) [test](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:1548)

4. **Related documentation cleanup:** §5.11 says every continuation passes an explicit position argument, while its predicate correctly says only `StepContinue` supplies one; `Arm`/`Continue` derive theirs. [conflicting text](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:968) [predicate](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:988) The design note also retains “no position for … `Finish`” in an unmarked proposed refinement. [design note](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step3b-design-note.md:123)

All other §5.11 predicate clauses and acceptance rows align with the implementation and exercised assertions.

## Could not verify

- `git diff --check` is clean.
- Existing build evidence records `ctest` 19/19 and player test 49 passed, with `-Wall -Wextra` configured, but I could not run a fresh rebuild or tests in the read-only environment.