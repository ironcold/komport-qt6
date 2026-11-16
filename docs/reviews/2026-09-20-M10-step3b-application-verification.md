Verdict: **do not adopt or commit step 3b yet.**

Disposition:

- **Critical — rule 8 / no-op `stop()`: not resolved.** The implementation and test correctly let `stop()` from a `Ready` step continue, but §5.11 rule 8 still says a receiver that “stops” ends the step, and its acceptance-test row still requires one delivery only. The purported amendment is misplaced after implementation step 9, not in rule 8, and contradicts both rule 8 and the test table. [SPEC-M10-session-reader-passive-replay.md](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1037), [SPEC-M10-session-reader-passive-replay.md](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1094), [SPEC-M10-session-reader-passive-replay.md](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1532)

- **High — fixed `Arm`/`Continue` coordinates: implementation resolved; evidence incomplete.** The helper genuinely ignores `expectedPosition` for `Arm` and `Continue`, deriving their positions from the entry snapshot. The direct test proves this. [sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:236)  
  However, `StepContinue`’s positive and wrong-coordinate assertions run with a live `Playing` player. They do not establish the required caller-derived coordinate in an actual `Ready` or `Paused` step context. [tst_sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:1751)

- **High — final-stop sequence evidence: resolved.** The test clears the log and pins exactly `event:3`, `Paused`, `Finished`, `finished`; the transient-`Paused` test also checks the first stop’s captured timing. [tst_sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:1595), [tst_sessionreplayplayer.cpp](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:1834)

- **Medium — stale signature/design-note proposal: not resolved.** The main specification’s normative/checklist signatures are updated, but the design note still opens with the obsolete two-argument signature and retains an unmarked proposal that trusts an explicit position for `Arm` and `Continue`. [2026-09-20-M10-step3b-design-note.md](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step3b-design-note.md:1), [2026-09-20-M10-step3b-design-note.md](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step3b-design-note.md:120)

New finding:

- **High — rule-8 amendment remains incomplete about operations a step continues through.** A synchronous step also continues after a valid `setTiming()` call: it changes persistent timing, but does not alter replay identity, schedule token, state, or position. Likewise, refusals such as `start(nullptr)`, an invalid timing, or `play()` in Step timing leave it continuing. The amendment’s “not by an operation that has no effect” rationale therefore does not describe actual behavior. Decide and norm which operations interrupt a step, then pin at least valid `setTiming()` and a refusal from both `Ready` and `Paused` step contexts.

Verification:

- Current player binary: **48/48 QtTest entries passed** with `QT_QPA_PLATFORM=offscreen`.
- `git diff --check` passed.
- I could not independently rebuild warning-free or run full CTest: the read-only sandbox prevents CTest from writing `build/Testing/Temporary/LastTest.log`.