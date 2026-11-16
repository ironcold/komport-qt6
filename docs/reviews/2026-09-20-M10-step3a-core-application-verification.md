## Verdict

**No — step 3a should not be adopted or committed as it stands.** The two critical re-entrancy findings are resolved, but the required same-pass evidence remains incomplete and the self-review contains two material inconsistencies.

## Disposition of prior findings

| Finding | Disposition | Evidence |
|---|---|---|
| Delivery guard hid §5.11 completion/continuation semantics | Resolved | `onDeliveryDue()` has no live-state/session guard; it delivers, then unconditionally completes or arms under the stated valid-mutation-context precondition. [player implementation](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:357) |
| “Defence in depth” affected signal re-entry | Resolved | The guard and its disclosure are gone. `play()` no longer relies on a later callback guard to decide the outcome. |
| Comment claimed replay/schedule identities | Resolved | The restart comment now cites the §5.10 cancellation guarantee, without claiming tokens or replay IDs. [comment](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:180) |
| Stop test denied its own state transition | Resolved | The test now excludes only delivery/completion signals and asserts the `Paused` transition. [test](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:670) |
| Complete refusal/timing evidence | **Not resolved** | The new test’s stated coverage exceeds what it executes. It tries every timing value only in `Idle`; it exercises only `Scale2` in `Ready`, `Scale5` in `Paused`, and `Immediate` in `Finished`. [test](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:1012) It also does not exhaust all refusal entry points/conditions with an unchanged-observable-state assertion, and its cumulative delivery/completion counts do not prove that an individual `setTiming()` emitted neither signal. |

The fifth item was an adoption condition in the prior review, so it needs completion before approval.

## Re-entrancy and cancellation

There is no remaining *hidden authorization policy* in the core: no identity, snapshot, or state guard is deciding whether a continuation completes or schedules.

However, the core deliberately still depends on a receiver not mutating the player, as §5.11’s precondition says. For example, after `eventDelivered`, `onDeliveryDue()` reads `mSession`, completes, or arms; `play()` emits `Playing` before arming; and the synchronous step loop continues after each emitted event. [implementation](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:220) [implementation](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:357) [implementation](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:461)

That is acceptable for the deliberately cut 3a contract only because it is explicit, unsupported behavior pending 3b—not because cancellation makes re-entry safe. The self-review should correct its statement that “No operation assumes anything after an emission”; it plainly does, under the precondition. [self-review](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step3a-core-implementation-selfreview.md:31)

§5.10 is the right normative location and its strength is right: it promises only that a callback actually removed by `cancelPending()` will not later run. It does not, and cannot, cancel a callback already executing. Both implementations uphold that contract:

- Production: stops the timer and clears the stored callback; even a later timer dispatch finds no callable function. [implementation](/home/max/Development/misc/komport-qt6/komport/sessionreplayplayer.cpp:67)
- Test scheduler: clears both `mArmed` and the callback, so `fire()` returns false afterwards. [test scheduler](/home/max/Development/misc/komport-qt6/tests/tst_sessionreplayplayer.cpp:71)

A small documentation cleanup is still warranted: put the cancellation guarantee beside `cancelPending()` in [sessionreplayseams.h](/home/max/Development/misc/komport-qt6/komport/sessionreplayseams.h:59), not only in the specification.

## New same-pass items

- The self-review’s inventory still says **28 test methods / 30 entries**, while its later evidence and the current executable list show **29 / 31**. [self-review](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-20-M10-step3a-core-implementation-selfreview.md:16)
- Replace the new test with a genuine matrix: every declared timing in `Idle`, `Ready`, `Paused`, and `Finished`; every non-`Step` policy while `Playing`; every refusal condition/entry point; and before/after assertions for state, position, delivered count, event count, timing, pending schedule, and delivery/completion emissions.

## Verification limits

I could statically verify the implementation, spec, comments, test source, and scheduler behavior. The existing current build record shows `ctest` **19/19** and `tst_sessionreplayplayer` **31 passed**; the executable lists 29 test methods. I could not independently re-run the warning-enabled build or full suite in this read-only review, so warning-free status remains owner-reported.