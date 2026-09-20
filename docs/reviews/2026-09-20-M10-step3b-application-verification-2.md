Verdict: **do not adopt or commit step 3b yet.**

- **Critical — rule 8 is still internally contradictory.** The new positive amendment is correct and matches code, but the preceding text still says a receiver that “stops” ends the step, and the acceptance table still says `stop` yields only one delivery. Both conflict with the new Ready/Paused no-op rule and the actual test. Update [rule 8](docs/specs/SPEC-M10-session-reader-passive-replay.md:1037) and its [acceptance-table row](docs/specs/SPEC-M10-session-reader-passive-replay.md:1104) to distinguish no-op `stop()` from a `stop()` after the step has been superseded into `Playing`.

- **High — `StepContinue` evidence: resolved.** The predicate is exercised in an actual `Paused` state after `stop()` bumps the token, with positive, wrong-position, and stale-token cases. The implementation also uses the caller-derived coordinate only for `StepContinue`. [Test](tests/tst_sessionreplayplayer.cpp:1778) [predicate](komport/sessionreplayplayer.cpp:225)

- **Medium — design-note fix: not fully resolved.** Its body and supersession marker are correct, but the title retains the obsolete two-argument signature, and its call-site table still describes the superseded per-delivery snapshot model. [Design note](docs/reviews/2026-09-20-M10-step3b-design-note.md:1) [stale table](docs/reviews/2026-09-20-M10-step3b-design-note.md:24)  
  §5.11 rule 8 also retains the obsolete two-argument helper spelling. [Spec](docs/specs/SPEC-M10-session-reader-passive-replay.md:1038)

New finding:

- **Medium — incomplete rule-8 test matrix.** The new Ready/Paused test covers valid timing, invalid timing, null start, and no-op stop, but omits `play()` refused in `Step` mode—the third refusal explicitly named by rule 8. Add it in both contexts. [Test matrix](tests/tst_sessionreplayplayer.cpp:1927)

No implementation operation behaves differently from the new positive list in a Ready or Paused step: valid declared timing changes continue; refusals continue; no-op stop continues; `close`, successful `start`, successful `play`, and nested steps stop the outer step. A `stop()` only denies after the state has already become `Playing`, via the bumped token.

Verification:

- `tst_sessionreplayplayer`: **49 passed, 0 failed**.
- `git diff --check`: passed.
- I could not independently verify the warning-free rebuild or CTest 19/19: CTest cannot write its log in this read-only sandbox, and direct `tst_serial` execution cannot create required serial lock files.