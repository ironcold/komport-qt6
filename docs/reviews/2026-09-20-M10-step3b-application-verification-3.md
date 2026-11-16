Verdict: **do not adopt or commit step 3b yet.**

Disposition:

- Rule 8 prose: resolved. Its interrupting/non-interrupting lists are now internally consistent and match the implementation’s behavior.
- Acceptance row for rule 8: **not resolved.** It says `anInterruptedMultiEventStepStopsAtTheInterruption` tests a `stop()` that acts in `Playing` and “carries” all non-interrupting cases. It does neither: the test exercises `play`, no-op `stop` in `Ready`, `close`, restart, and nested step; the Ready/Paused positive cases are in the separate `aStepContinuesThroughEverythingThatDoesNotSupersedeIt` test. [Acceptance row](docs/specs/SPEC-M10-session-reader-passive-replay.md:1106), [interruption test](tests/tst_sessionreplayplayer.cpp:1665), [positive matrix](tests/tst_sessionreplayplayer.cpp:1927).
- Design-note title and call-site table: resolved. New low-severity correction: it still calls the table “the four phases,” although it now has five (`Arm`, `Continue`, `Complete`, `StepContinue`, `Finish`). [Design note](docs/reviews/2026-09-20-M10-step3b-design-note.md:14).
- Missing `play()`-refused-in-`Step` matrix case: resolved. It runs in both `Ready` and `Paused`, establishes `Step` beforehand, and asserts the policy remains `Step`. [Test](tests/tst_sessionreplayplayer.cpp:1942).

Exhaustiveness: confirmed. In a `Ready` or `Paused` step, all public mutators are classified: successful `play`, `close`, successful `start`, and nested steps interrupt; valid timing changes, all applicable refusals, and no-op `stop()` continue. No unclassified operation remains.

Verification:

- Direct `tst_sessionreplayplayer`: **49 passed, 0 failed**.
- `git diff --check`: passed.
- I could not independently rebuild with `-Wall -Wextra` or run full CTest 19/19 because this read-only sandbox prevents CTest from creating its required log file.