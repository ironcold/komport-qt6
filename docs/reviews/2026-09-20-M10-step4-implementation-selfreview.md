# M10 implementation self-review, step 4: the reply-free rendering path

Status: self-review, written before the adversarial gate round of this slice. Date 2026-09-20.
Scope: SPEC-M10 §11 step 4, implementing §5.7's reply-free rendering entry and §5.8 clause 3,
against the approved plan `docs/reviews/2026-09-20-M10-step4-reply-free-rendering-note.md`.

## What is implemented

`komport/komportemulation.{h,cpp}`

- The single private choke point `sendTerminalReply(const QByteArray &)`: it returns without
  writing while a replay render is in progress, and calls `serial()->putStr(reply.constData())`
  otherwise. The three reply sites now call it - `CSI 5 n` (was `putStr("\x1b[0n")`),
  `CSI 6 n` (the built cursor report) and `CSI c`/`CSI 0 c`/private `c` forms plus `ESC Z`
  (was `putStr("\x1b[?62c")`).
- `mTerminalRepliesAllowed`, explicitly initialised to `true`. The former body of
  `slotReceivedChar(char)` is now the private `renderReceivedChar(char)`; the two public receive
  entries differ only in the scope they install: the live one sets the mode to "allowed" and the
  new `slotReplayReceivedChar(char)` sets it to "suppressed", both through
  `KomportEmulationReplyModeScope` (a `QPointer`-guarded save-and-restore scope), so nesting and
  re-entry restore each caller's mode.

`komport/komportview.{h,cpp}`

- `slotReplayReceivedChar(char)`: forwards to `mEmulation->slotReplayReceivedChar()` first - the
  live order (emulation first, view bookkeeping second) - then performs exactly the live slot's
  scroll bookkeeping. A separate slot, so the live connection is untouched.
- `setKeyInputEnabled(bool)` / `keyInputEnabled()` with `mKeyInputEnabled` defaulting to `true`;
  `keyPressEvent()` consumes the event and emits no `keyPressed` while suppressed.

## Evidence

`tst_replayreply` (new target, 7 pass entries) and `tst_replayreplyui` (new app-based target, 4 pass
entries); full suite 21/21 with `-Wall -Wextra` and no warnings from the changed files:

1. `theLiveEntryPointsStillAnswer` - all five query forms answer through the live entry.
2. `everyReplayedQueryIsRenderedButNotAnswered` - the same five through the replay entry: zero
   `bytesWritten`, the PTY peer silent (non-blocking read gives only `EAGAIN`), and a printable
   sentinel still reaches `cellArray.cell(0,0)->character()`.
3. `suppressionNeverSurvivesTheReplayCall` - a live query after a replay render answers again.
4. `aNestedReplayStaysSuppressedAndAReentrantLiveCallAnswers` - synchronous re-entry through a
   `cellChanged` receiver (emitted from `drawChar()`) while the outer replay render is active:
   exactly one write leaves (the inner live query's), the nested replay query stays suppressed,
   and the outer mode survives for the next replay query.
5. `replayEntriesAndReplySitesPassTheSourceAudit` - the structural half: exactly three
   `sendTerminalReply(` calls in the emulation, exactly one direct `serial()->putStr/putChar` in
   the whole file (the one inside the choke point), both receive entries containing no write and
   calling the shared renderer, two scoped rollbacks, and the member defaulted to `true` in the
   header.

## Disclosures and the fix round after the adversarial review

The adversarial round (`docs/reviews/2026-09-20-M10-step4-adversarial-review.md`) refused the
slice with four findings. Two of them were test-side and are fixed here, and the remaining two
changed the slice itself:

1. **The view-level test exists now** (`tst_replayreplyui`, app-based offscreen, two tests): the
   replay stream goes through `KomportView::slotReplayReceivedChar()` and its grid, cursor and
   scroll bookkeeping are compared against the same stream delivered live over a pty, with the
   replay run's peer silent; and the key-input suppression is proven with a real key event through
   the view (no `keyPressed`, no `bytesWritten`, silent peer) against an enabled control that
   signals and writes.
2. **The reply-mode scope is no longer `QScopedValueRollback<bool>`.** The adversarial round
   found that a `cellChanged` receiver can destroy the emulation inside `drawChar()`, after which
   the scope's destructor wrote into freed memory. It is now
   `KomportEmulationReplyModeScope`, which holds a `QPointer<QObject>` and touches the emulation
   only while it still exists.
3. **The lifetime contract is written down** on `renderReceivedChar()`: rendering is synchronous,
   `cellChanged` is emitted from `drawChar()` inside it, and a receiver must not destroy the view
   or the emulation during a render - `renderReceivedChar()` continues afterwards
   (`advanceCursorWithWrap()`, the line-end work) and cannot be made deletion-safe there. The
   documented position is that synchronous destruction during a render callback is **not
   supported**; the QPointer scope protects its own restoration and is explicitly *not* claimed as
   a general lifetime guarantee. This hazard is pre-existing (the same synchronous emission exists
   in the live path), but it is now stated instead of implied.
2. **The source audit covers the two receive entries and the reply sites.** The plan stages the
   display adapter (the application-level consumer) into steps 6-7, when it exists; the audit is
   written so that adding the adapter to it is a two-line change.
3. **`grep`-based audit limits.** The audit counts call sites in the source text. A reply built
   through an indirection the audit does not understand would be missed - which is why the
   behavioural tests (1-4) exist beside it and why the plan calls the pair "the structural and the
   dynamic half".

6. `aReplayedStreamRendersLikeALiveOne` (view level) - the same stream through
   `KomportView::slotReplayReceivedChar()` renders an identical grid, cursor and scroll position to
   the live run over a pty, while the replay run leaves nothing on the peer.
7. `suppressedKeyInputNeitherSignalsNorWrites` (view level) - a real key event with
   `setKeyInputEnabled(false)` produces no `keyPressed`, no `bytesWritten` and no peer byte; the
   enabled control produces all three.
8. `replayEntriesAndReplySitesPassTheSourceAudit` was tightened after the adversarial round: it now
   collects every transmitting spelling (`->putStr(`, `->putChar(`, `->writeBytes(`) and requires
   the set of transmitting functions to be exactly the allow-list - the choke point plus the two
   established active-input functions - and it states its own limitation (an arbitrary indirect
   call is invisible to a source-text audit) rather than calling itself a proof.

## Fix round after the third adversarial round (2026-09-29)

The third round (`docs/reviews/2026-09-20-M10-step4-adversarial-review-3.md`) returned one high
blocking finding: the slice contained an unapproved change to `KomportSerial` - the RX overflow
branch flushed the buffer instead of dropping its oldest bytes - which SPEC-M10 §3 forbids ("No
change to ... `KomportSerial`, the legacy character signals, the legacy RX buffer"; "the existing RX
buffer keep[s] its present behaviour") and which also changed *when* the legacy display path emits
`receivedChar` (synchronously on overflow instead of at the flush timer). This self-review had never
documented that change: it was introduced during the round-2 fix round and recorded only in the
handover - which is itself part of the finding. The finding's scope claim was checked against the
spec before it was adopted (it holds); its statement that no changed test observes `receivedChar` is
imprecise - `tests/tst_replayreplyui.cpp:281` observes it and `tests/tst_transport.cpp:921` spies on
it; what no test pins is the *overflow delivery semantics* of the legacy display path.

Fixed in this round, and nothing else:

1. `komport/komportserial.cpp` restored to `HEAD`; the slice touches no `KomportSerial` code. The
   removed patch is preserved as `/tmp/m10-step4-serial-rx-overflow.diff` (sha256 `ee5bac7e...`) for
   its own spec-amended slice, as the round recommends.
2. `tests/tst_replayreplyui.cpp` no longer depends on it: `replayStream()` is now 831 bytes (150
   short lines, the queries at lines 10/75/120, ending mid-line), and the test asserts the bound
   (`< 1024`, the `rxQueue` default) with its reason on the assertion - the legacy display path drops
   the oldest bytes of an over-mark burst before its flush timer runs (SPEC-M8 §6.2: the display path
   is deliberately lossy), so comparing the two renders requires that the live path delivered every
   byte it was given. Measured on the fixed revision: stream 831 bytes, `receivedChar` count 831,
   history 176 rows = 144 scrollback + 32 visible - a deeper scrollback than the reviewed revision had.
3. Comments corrected: the earlier "1024 of 1971 bytes" divergence was attributed to a half-delivered
   live render alone; both causes are named now (incomplete synchronisation and the over-mark burst).

The handover's open selection item, resolved in the same round:

4. `suppressedKeyInputNeitherSignalsNorWrites` reporting the selection as lost is resolved test-side
   and documented in `docs/reviews/2026-09-20-M10-step4-selection-fix-note.md`. Both candidates the
   handover named - a synchronous clipboard `selectionChanged` reaching `slotSelectionChanged()`, and
   a suppression gap including the key release path - are refuted by the code
   (`komportview.cpp:748-751`, `:816-820`, `:268-270`), and the real cause is the test's own
   scroll-dependent cell resolution: it selected at the bottom of the history, scrolled to the top and
   then asked `getCell(0,0)`, which by then resolved into the scroll buffer. The assertion was not
   weakened: the state is fixed before the measurement, the "before" snapshot is taken immediately
   before the key event and asserted non-vacuous there, the scroll comparisons come first so each
   assertion answers for its own part of the promise, and the enabled control proves it takes the
   bookkeeping the suppressed branch skips.

Evidence after the fix round: `-Wall -Wextra` build with 0 warnings and 0 errors; `ctest` 21/21;
`tst_replayreply` 7/7; `tst_replayreplyui` 4/4 offscreen and 4/4 under `xcb`/`Xvfb`; the three
mutation probes on the suppressed `keyPressEvent()` branch red on exactly their own assertions with
the product file restored byte-identically afterwards.

Nothing is committed; no push.
