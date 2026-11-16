# M10 step 5, independent adversarial review, rounds 1-7

Record of the adversarial rounds of the M10 step-5 slice (SPEC-M10 §11 step 5: `KomportDoc` ownership,
`loadOfflineSession()` / `closeOfflineSession()` with the process-wide preconditions, destruction order).
Each reviewer text is reproduced verbatim; nothing is edited. Runner: codex-companion
`adversarial-review --wait --scope working-tree`, 2026-10-15. Foreign owner changes in the tree
(`AGENTS.md`, `CLAUDE.md`, `docs/project-context.md`, `docs/example-charsets/README.md`,
`docs/translation-glossary.md`) are not part of the slice.

| Round | Verdict | Finding | Fix |
| --- | --- | --- | --- |
| 1 | needs-attention | `closeOfflineSession()` not re-entrancy-safe (player's synchronous `stateChanged(Idle)`) | `mClosingOfflineSession` guard, `offlineSessionChanged(false)` emitted last, 2 tests |
| 2 | needs-attention | a receiver can delete the `KomportDoc` from `stateChanged(Idle)` (use-after-free) | `QPointer` guard, 2 tests, ASan |
| 3 | needs-attention | player destroyed under an outer player call (`play()`/delivery) | player retired instead of destroyed in place |
| 4 | needs-attention | (a) nested event loop can run `deleteLater` early; (b) Idle receiver could restart the retired player | spec change: `SessionReplayPlayer` call depth + terminal retired state (SPEC-M10 §5.11 rule 11, §5.9) |
| 5 | needs-attention | public self-deleting `retire()` on a generally constructible QObject; deferred deletion needs an event loop | `static retire(std::unique_ptr<...>)`, synchronous destruction when no own call is on the stack |
| 6 | needs-attention | Qt-parented player deleted with its parent beneath `play()` | `retire()` detaches the Qt parent first |
| 7 | **approve** | none | - |

Evidence at the approving round: `-Wall -Wextra` build 0 warnings; `ctest` 22/22; ASan build (separate build
directory): `tst_sessionreplayplayer` 52/52, `tst_sessionofflinedocument` 24/24. Mutation probes (each caught):
window walk removed (2 window tests red); re-entrancy guard removed (1 red); synchronous `delete` of the player
(heap-use-after-free); retired flag set after `close()` (Idle-receiver restart test red); deletion posted
regardless of call depth (heap-use-after-free); `setParent(nullptr)` removed (heap-use-after-free).
Accepted residual, documented in SPEC-M10 §5.11 rule 11: a player retired from inside one of its own
synchronous signals is reclaimed by `deleteLater()` and so needs an event loop to run afterwards; it is inert
until then.

## Round 1 (verbatim)

### Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No-ship: closeOfflineSession() is unsafe under the required player-signal re-entrancy case.

Findings:
- [high] Re-entrant close can destroy the active player and discard a newly loaded session (komport/komportdoc.cpp:347-357)
  mReplayPlayer->close() synchronously emits stateChanged(Idle). A connected handler can call KomportDoc::closeOfflineSession() while the outer call is still inside SessionReplayPlayer::close(). The inner call resets and destroys mReplayPlayer, clears mOfflineSession, and emits offlineSessionChanged(false); the outer invocation then resumes and resets/emits again. If an offlineSessionChanged(false) handler loads another session, that load can succeed, but the outer invocation subsequently destroys its new player/session. This violates the successful-load state contract and deletes the player while its close() stack frame is active. The new test only observes a non-re-entrant close, so it cannot catch this path.
  Recommendation: Make closeOfflineSession() re-entrancy-safe (for example, use a closing guard and defer/reset ownership only after the player close signal has returned), ensuring exactly one false transition per actual close. Add a direct stateChanged handler that calls closeOfflineSession(), plus a nested offlineSessionChanged(false) handler that attempts a reload, and assert no duplicate transition, no dangling player, and preservation of the successful nested load.

Next steps:
- Fix the re-entrant close path and add the signal-handler regression test before shipping.

## Round 2 (verbatim)

### Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No-ship: the re-entrancy guard does not protect destruction of the owning document during the player's synchronous close signal, leaving a use-after-free path.

Findings:
- [high] Player close signal can destroy its owning document mid-close (komport/komportdoc.cpp:360-366)
  `SessionReplayPlayer::close()` synchronously emits `stateChanged(Idle)`. A receiver may delete the `KomportDoc` (or its owner) from that signal. Its destructor resets `mReplayPlayer`, destroying the player while its `close()` call is active; control then returns here and accesses members of the freed document. The new boolean guard only handles nested `closeOfflineSession()` calls, not document lifetime. Current tests cover nested close/reload but not deletion from this signal handler.
  Recommendation: Move `mReplayPlayer` into a local `unique_ptr` before calling `player->close()` and use a `QPointer<KomportDoc>` lifetime guard before any subsequent document-member access or notification. Add a regression test whose `stateChanged(Idle)` receiver deletes the document (or owning window), ideally exercised under ASan.

Next steps:
- Fix the synchronous destruction path and rerun the offline-document test suite under an address sanitizer.

## Round 3 (verbatim)

### Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No ship: the round-3 workaround still permits a replay player to be destroyed from within an active player signal stack, causing use-after-free.

Findings:
- [high] Reentrant document close can destroy a player still executing (komport/komportdoc.cpp:363-371)
  Moving the player to a local pointer only protects the inner `player->close()` frame. A receiver of `stateChanged(Playing)` or `eventDelivered` can call `document.closeOfflineSession()`: this code moves the player out, calls its `close()`, then destroys it at `player.reset()`. Control then returns to the original `SessionReplayPlayer::play()` or `onDeliveryDue()` call, which continues to access `this` (for example `mayMutateReplay()` after delivery). That is a use-after-free. The existing deletion test only covers `stateChanged(Idle)` emitted by the inner close, where `SessionReplayPlayer::close()` happens not to access members after its signal. Additionally, if that receiver deletes the document, the local player survives until after the document's transport has been destroyed, violating the required player/session-before-transport order.
  Recommendation: Make teardown lifetime-safe for every synchronous player signal: do not synchronously destroy the player while a player call/signal stack can still resume. Retain/defer destruction through a dedicated teardown owner until signal unwinding is complete, including document deletion, while preserving player -> session -> transport destruction order. Add ASan tests that call `closeOfflineSession()` from `stateChanged(Playing)`, `eventDelivered`, and final-state signals, plus a reentrant document-deletion order test.

Next steps:
- Fix the player/document lifetime boundary and rerun the focused test under ASan before another review round.

## Round 4 (verbatim)

### Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No-ship: deferred retirement remains unsafe under re-entrant event loops, and a close signal receiver can revive the detached player.

Findings:
- [high] Nested event loop can delete the player while its outer call resumes (komport/komportdoc.cpp:270-271)
  `deleteLater()` is posted while an outer `play()`/delivery call can still be suspended in a synchronous player signal. A receiver can enter an event loop after `closeOfflineSession()` returns; Qt may process the deferred deletion before that outer frame resumes, after which `SessionReplayPlayer` continues accessing `this` (for example its post-signal mutation check). The current ASan tests exercise immediate unwinding only, not this nested-loop path.
  Recommendation: Make retirement lifetime-safe independently of event-loop nesting: retain ownership for every in-flight player operation and destroy only once its call depth reaches zero. Add an ASan test that closes from `stateChanged(Playing)` and enters a nested `QEventLoop` before the slot returns.
- [high] The retired player can be restarted from its Idle notification (komport/komportdoc.cpp:270-271)
  Normal close leaves all player receivers connected. `player->close()` clears its state and emits `Idle` before line 271 schedules deletion; an Idle receiver holding the public player pointer can call `start(document.offlineSession())` and `play()`. `close()` has no post-signal cleanup, so the detached player retains the session and arms its timer. If the event loop is stopped or never runs, `deleteLater()` never destroys it, directly violating the close contract and the new spec claim that a retired player holds no file/session. The tests do not attempt this mutation.
  Recommendation: Introduce a non-resurrectable retirement state before emitting Idle, with all replay-mutating operations refusing thereafter; ensure retired ownership is reclaimed deterministically when no event loop can process deferred deletion. Add a test whose Idle receiver attempts start/play and asserts no session, timer, or delivery survives close.

Next steps:
- Fix and ASan-test both re-entrant paths before accepting the §5.9 clarification; it currently does not uphold the stated destruction guarantee.

## Round 5 (verbatim)

### Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No ship: retirement makes a public QObject self-delete without a valid ownership contract, and the documented no-event-loop behavior violates the required deterministic player destruction.

Findings:
- [high] Public retire() can delete stack- or externally-owned players (komport/sessionreplayplayer.h:97-119)
  `SessionReplayPlayer` has a public constructor with no heap-only or ownership restriction, but public `retire()` arranges `deleteLater()` on `this`. A valid-looking `SessionReplayPlayer player; player.retire(); QCoreApplication::sendPostedEvents(...DeferredDelete)` deletes stack storage; a caller retaining a `unique_ptr` has the corresponding later double-delete. The existing test deliberately releases its `unique_ptr`, which masks this API hazard.
  Recommendation: Do not expose a self-deleting operation on a generally constructible QObject. Restrict retirement to a lifecycle mechanism with an unambiguous heap-owned handle, or redesign ownership so the owner/reclaimer—not the player—performs deferred destruction. Add regression coverage for stack and externally-owned instances.
- [medium] Close/destructor relinquishes ownership even when deferred deletion can never run (komport/komportdoc.cpp:69-73)
  Both close and the document destructor release the sole `unique_ptr` and depend on `deleteLater()`. If called after the event loop has stopped, or on a thread without one, the player remains allocated indefinitely and has no remaining owner. The new rule explicitly permits this, but that contradicts §5.9's required destruction order and the step requirement that KomportDoc owns/destroys the player; repeated synchronous load/close operations accumulate leaked players.
  Recommendation: Define a deterministic retirement/reclamation owner for the no-event-loop path, or explicitly narrow and approve the ownership/destruction contract before shipping. Test close and document destruction without processing deferred-delete events.

Next steps:
- Resolve the player lifetime/ownership model, then rerun the re-entrancy and no-event-loop lifetime tests under ASan.

## Round 6 (verbatim)

### Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No-ship: the new retirement API remains unsafe for a normal QObject-parented player and can reintroduce use-after-free during re-entrant teardown.

Findings:
- [high] QObject parent ownership can delete a deferred-retirement player beneath its outer call (komport/sessionreplayplayer.cpp:377-389)
  `retire()` relies on the incoming `unique_ptr` being the only owner, releases it, and defers destruction while a player call is active. But the public constructor accepts a `QObject *parent`, making Qt parent ownership a supported construction mode. If a `stateChanged(Playing)` receiver retires a player that has a QObject parent and then deletes that parent, QObject immediately destroys its child while `play()` is still on the stack. When the signal returns, `play()` continues and dereferences the freed player. The prose-only unique_ptr contract does not enforce this against the class's public QObject ownership model.
  Recommendation: Make retirement reject or detach QObject parenting before it can emit/close (and document/test that transfer), or remove parented construction for players that may use `retire()`. Add an ASan regression that retires from `stateChanged(Playing)` and destroys the former QObject parent before the outer call resumes.

Next steps:
- Resolve the QObject-parent lifetime conflict, then rerun the re-entrant ASan suite.

## Round 7 (verbatim)

### Codex Adversarial Review

Target: working tree diff
Verdict: approve

Ship: no grounded material defect found in the step-5 diff. Preconditions are process-wide and ordered, refusal paths are side-effect-free, and the retirement protocol covers synchronous re-entrancy without exposing a live player/session through the document after close.

No material findings.

