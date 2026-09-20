# M10 slice 2 (the replay player): re-cut proposal after six review rounds

Status: **open, awaiting the owner's decision.** Written 2026-09-20 by the implementer. This is
the pause the binding process asks for after five review-fix cycles with unresolved critical
findings (`docs/komport-engineering-governance-spec-review-workflow.md`, "count your cycles"),
and the reviewer asked for it explicitly in its own cycle judgement.

## Where the slice stands

`komport/sessionreplayplayer.{h,cpp}`, `komport/sessionreplayseams.h`,
`tests/tst_sessionreplayplayer.cpp` are complete, **warning-free** (`-Wall -Wextra`),
`ctest` **19/19** green, **39 test methods** (41 QTest pass entries), the unit compiles against
`Qt6::Core` alone (ADR-009 contract check). Nothing of it is committed.

The review record of the slice (all in `docs/reviews/`):

| Round | Outcome |
| --- | --- |
| implementation review | blocked: re-entrancy from a delivery receiver (dangling session, re-arm after a stop), `INT_MAX` delay cap, three test gaps |
| application verification | two further re-entrancy cases (dangling event reference, `completion` computed after the state change) |
| application verification 2 | two further cases (`play()` arming after its own state signal, a no-op `stop()` swallowing a completion) |
| application verification 3 | three further cases (a refusal after `Playing` was entered, a step running past a takeover, a stop report computed after its signal) - all resolved |
| application verification 4 | **re-cut recommended**: identity model; plus a step snapshot and a `stop()` wording contradiction |
| application verification 5 | snapshot re-cut implemented (two identities + `OperationSnapshot`); two items fixed |
| application verification 6 | those two resolved; **one new HIGH** of the family (a final step calls `setState(Finished)` without checking that its replay still owns the player), and the reviewer's judgement: **re-cut/split for the owner, do not patch locally again** |

Six rounds, four fix cycles, two structural re-cuts, and the family still yields one more
instance. The instances are getting smaller (the current one is a single missing guard), but the
trend is the point: local guards are not closing this class.

## Why the family keeps yielding instances

The player is a *synchronous signal emitter*: every operation can be re-entered by a receiver
of its own signal, and each operation has three separable concerns that the current design
interleaves - (a) the *identity* of the replay it acts on, (b) the *facts* it must report, and
(c) the *permission* to mutate live state afterwards. The identity and the facts are now
snapshot-based; the permission check is still written per operation, in five places, and each
round the reviewers found the one place where it was missing or incomplete.

## Options for the owner

**A. Split the slice (recommended).** Two slices with two contracts, each reviewable on its own:
- **A1 - the replay state machine without re-entrancy:** the five states, the seven operations,
  the timing ladder, the step semantics, the value-returned refusals, with one normative
  precondition - *no player operation is called from a receiver of a player signal* - pinned by
  tests. Everything the rounds proved about the state machine and the arithmetic stays.
- **A2 - the re-entrancy contract as its own slice:** one documented permission helper
  (`mayMutateReplay(snapshot)`), applied at every mutation site as the *only* way to change
  state, with the existing re-entrancy tests (they are already written) as its acceptance
  evidence and one review per helper, not per operation.
- Consequence: two smaller spec sections instead of §5.5's accreting amendments, a bounded
  review surface, and the re-entrancy tests finally point at a single artefact. Cost: one more
  review round per slice, and §11 step 3 is re-cut.

**B. Finish A1+A2 in one slice anyway.** Add the missing guard plus the permission helper, run
one more round. Cheapest in rounds; the reviewer has advised against exactly this twice, and the
process rule is against it.

**C. Ship the player with the re-entrancy corner documented as a known limitation.** Fix the
current guard, commit, and record "calling a player operation from a receiver of a player signal
is unsupported and may leave inconsistent state" as a limitation in §5.5. Honest and
inexpensive, but it trades a contract for a caveat in an accepted specification - the owner
should weigh whether the M10 player ever has a second consumer (M11's decoders will add one).

My recommendation is **A**: it is the reviewer's advice, it is what the process prescribes at
this point, and it turns the accumulated test evidence into a contract instead of a lesson.

## What is not in question

The slice's substance is verified: timing arithmetic (including the clamp and the chunked
long-delay wait), one-delivery-per-callback, the refusal values, step semantics, the clock
non-use, the ADR-009 boundary, and 25 of §7's 27 planned player tests. Only the re-entrancy
permission layer is unresolved. Slice 1 (the reader) is committed (`006a607`, `18a3943`) and
pushed; `master` is untouched.
