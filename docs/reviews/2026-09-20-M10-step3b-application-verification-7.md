Verdict: **No — do not adopt or commit step 3b as it stands.**

Disposition:

- The acting-stop case is now valid. `pausedBefore` is used; the outer step enters from `Paused`, and the receiver’s `play(); stop()` restores `Paused` with position 1. The schedule-token bump is therefore the only remaining authorization failure. [test](tests/tst_sessionreplayplayer.cpp:1711)
- The stale-count fix is incomplete: the design note still says “eleven plus six,” despite listing seven added tests later. The self-review’s 18-test / 29+18 totals are correct; §5.11 no longer states a stale number. [design note](docs/reviews/2026-09-20-M10-step3b-design-note.md:14)

New findings:

- §5.11 says `Finish` checks no position, but the implementation consults live `mPosition >= snapshot.eventCount()`. The direct-helper test does not exercise a valid `Finish` authorization. Resolve the normative/code mismatch and test it. [spec](docs/specs/SPEC-M10-session-reader-passive-replay.md:995) [code](komport/sessionreplayplayer.cpp:251)
- The non-interrupting acceptance row says the invalid timing, null `start()`, and Step-mode `play()` are refusals, but the test discards those results. It proves the outer step continues, not those individual refusal results. [spec](docs/specs/SPEC-M10-session-reader-passive-replay.md:1107) [test](tests/tst_sessionreplayplayer.cpp:1956)

All other §5.11 acceptance rows match their tests, including the final-stop, transient-`Paused`, completion-supersession, timing-linearization, and final-step cases.

Verified:

- `git diff --check` is clean.
- The focused player test passes: **49 passed, 0 failed**.

Could not verify:

- A fresh `-Wall -Wextra` rebuild or full `ctest 19/19`: this read-only environment prevents CTest from creating its log files.