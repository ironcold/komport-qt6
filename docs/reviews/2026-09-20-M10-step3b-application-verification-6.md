Verdict: **No — do not adopt or commit step 3b yet.**

Disposition per item:

1. **Acting-stop token-bump pin: not resolved.** `pausedBefore` is declared but never used. The outer step still begins in `Ready`; `play()` changes its state to `Playing` before `stop()` returns it to `Paused`. The outer continuation is therefore refused by both state and token, not token alone. [test](tests/tst_sessionreplayplayer.cpp:1682)

2. **Finished-receiver no-op report: resolved.** The test captures `stop()` and asserts `delivered == 3`, `remaining == 0`, and `finished`. [test](tests/tst_sessionreplayplayer.cpp:1541)

3. **Transient-`Paused` assertions: resolved for the identified gaps.** It asserts the acting stop report, exact invalid-timing reason, nonempty null-start reason, and no scheduler arm. [test](tests/tst_sessionreplayplayer.cpp:1845)

4. **Stale counts: not resolved.** The design note still says “eleven plus six” twice, while its own test-plan list names seven new tests. The self-review says “17 tests” while also saying eleven plus seven. §5.11 still says “plus three.” [design note](docs/reviews/2026-09-20-M10-step3b-design-note.md:14) [self-review](docs/reviews/2026-09-20-M10-step3b-implementation-selfreview.md:23) [spec](docs/specs/SPEC-M10-session-reader-passive-replay.md:1100)

New findings:

- **Blocking documentation contradiction:** §5.11 says only `StepContinue` takes caller-derived position and that `Finish` checks no position, but the implementation’s `Finish` checks live position against the captured event count. The acceptance row also claims a moved position is tested for every phase, but the direct test does not exercise a moved-position `Finish` case. Resolve the normative rule, then align code and test. [spec](docs/specs/SPEC-M10-session-reader-passive-replay.md:988) [code](komport/sessionreplayplayer.cpp:252) [acceptance row](docs/specs/SPEC-M10-session-reader-passive-replay.md:1108)

- The opening snapshot prose still says every continuation supplies its position explicitly, contradicting the later rule that `Arm` and `Continue` derive it. The design note retains the same superseded proposal without marking it superseded. [spec](docs/specs/SPEC-M10-session-reader-passive-replay.md:968) [design note](docs/reviews/2026-09-20-M10-step3b-design-note.md:123)

- The non-interrupting matrix proves the outer step continues, but it discards the return values of unknown timing, null `start()`, and refused `play()`. Its row therefore overstates that those individual refusals are asserted. [test](tests/tst_sessionreplayplayer.cpp:1956)

Verified:

- The current focused player binary passes: **49 passed, 0 failed**.
- `git diff --check` is clean.

Could not verify:

- A warning-enabled rebuild and full **19/19 CTest** run: the read-only environment prevents CTest from creating `build/Testing/Temporary/LastTest.log`.