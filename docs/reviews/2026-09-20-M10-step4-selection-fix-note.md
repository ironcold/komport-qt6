# M10 step 4 fix note: the open selection assertion

Date: 2026-09-29. Written after the handover
`docs/reviews/2026-09-20-M10-step4-handover.md` left one item open. Scope: the failing
assertion of `TstReplayReplyUi::suppressedKeyInputNeitherSignalsNorWrites` in
`tests/tst_replayreplyui.cpp`. No product behaviour was changed by this fix.

## The open item as handed over

The handover recorded: "after the suppressed key event the selection is **gone**
(`selectedText()` is `\"\"` where it was `\"x \\n\"`)", and named two candidates for the
cause, with the instruction to read `select()`/`slotSelectionChanged()`/`keyReleaseEvent`
before deciding and to weaken no assertion:

1. the test does not mirror the real caller (a synchronous `QClipboard::selectionChanged`
   into `slotSelectionChanged()` could call `deselect()` while `mInSelection` is unset -
   only the mouse path sets it), or
2. a real suppression gap, including the key *release* path.

## What the code says

- `KomportView::select()` already forces `mInSelection` around the clipboard write
  (`komport/komportview.cpp:748-751`, committed with the pre-existing selection work, not
  part of this slice), precisely so a synchronous `selectionChanged` cannot wipe the cell
  flags it just set. The synchronous half of candidate 1 is therefore closed in the code,
  for the mouse caller and for any other caller.
- `KomportView::slotSelectionChanged()` deselects only while `!mInSelection`
  (`komportview.cpp:816-820`); it is connected to the **clipboard** signal
  (`komportview.cpp:70`). It is the only path that clears a live selection without a user
  action - `deselect()` is otherwise called from `mousePressEvent()` (a new drag,
  `:648`) and from `select()` itself (`:690`).
- `KomportView::keyReleaseEvent()` is an empty body (`komportview.cpp:268-270`,
  `Q_UNUSED(_e)`): there is no release path that could deselect, scroll or focus-change.
  The silenced press path returns before every side effect (`:253-259`).
- So neither candidate explains the recorded symptom.

## What was measured

`tests/tst_replayreplyui.cpp` was instrumented temporarily (not committed) to print, at
four points - after `select()`, after the scroll to the top, immediately before the key
event, immediately after it - the selection text, the cell flags of `getCell(0,0)` *and*
of the live-grid `cellArray()->cell(0,0)`, the scroll value, the scroll maximum, and a
`QClipboard::selectionChanged` counter. The test was then run in two environments:
`QT_QPA_PLATFORM=offscreen` and `QT_QPA_PLATFORM=xcb` under a private `Xvfb :99` (where
the platform does implement `QClipboard::Selection` and the `select()` clipboard path
really executes).

Both runs, identical:

```
after-select   clipSignals=0 selectedText="x \n" flagGetCell00=1 flagCellArray00=1 scrollValue=57 scrollMax=57
after-scroll   clipSignals=0 selectedText="x \n" flagGetCell00=0 flagCellArray00=1 scrollValue= 0 scrollMax=57
before-key     clipSignals=0 selectedText="x \n" flagGetCell00=0 flagCellArray00=1 scrollValue= 0 scrollMax=57
after-key      clipSignals=0 selectedText="x \n" flagGetCell00=0 flagCellArray00=1 scrollValue= 0 scrollMax=57
```

Three facts follow, and the third is the real cause:

1. No `selectionChanged` is delivered in either environment, so no deselect happens
   (`selectedText()` and the live-grid flag are unchanged across the key event).
2. The row `after-key` is identical to `before-key`: the suppressed key event changed
   nothing.
3. The flag that the failing assertion read flipped `1 -> 0` at the **scroll** step, before
   any key event existed. `select()` resolves its cells through the scroll-position-aware
   `getCell()`, and the test selected while the view followed its output (bottom,
   `getCell(0,0)` = live grid) and then scrolled to the top of the history before checking
   `getCell(0,0)` - which from then on resolved into `mScrollBuffer`. The assertion
   compared a cell the selection had never flagged: it could not hold in any run, with or
   without key input.

## Correction of the handover (corrections list)

- The recorded symptom is **not reproducible** in this tree, on either platform. The
  recorded value `"x \n"` shows that the selection in those runs was made at the bottom
  and that the view had scrolled - i.e. the same mis-resolution as above; `selectedText()`
  is a member the key path never touches, so its loss cannot come from the key event.
- Candidate 1 (synchronous clipboard delivery) is refuted by `komportview.cpp:748-751`.
- Candidate 2 (a suppression gap including the release path) is refuted by
  `komportview.cpp:253-259` and `:268-270`.
- What the handover got right: the assertion had to be settled before the round, and the
  promise must not be weakened. It is not weakened; it is now measured comparably.

## The fix (test-side, `tests/tst_replayreplyui.cpp`)

1. **State first, then measure.** The view is placed at the top of its history *before*
   the selection is made (`bar->setValue(0)`, asserted `0`, asserted `< maximum`, so the
   non-bottom position is positive evidence rather than an assumption), and the scroll
   position does not move again until after the key event. The selection therefore lives in
   the scrollback and the cell-flag check resolves through `getCell()` at an unchanged
   position - the same resolution `select()` used.
2. **The "before" snapshot is taken immediately before the event** (`selectedText()`, the
   cell flags, `bar->value()`, `bar->maximum()`), and is asserted non-vacuous right there
   (the selection exists and carries its flags). A snapshot captured earlier would also
   cover focus acquisition and event processing, and a difference found there would be
   attributed to a key event it did not come from.
3. **The scroll comparisons come first** after the event, so that a mutation which moves
   the scroll is reported by the scroll comparison and not by the cell-flag check, which
   depends on the scroll position. Each assertion answers for its own part of the promise.
4. **The enabled control now proves the difference it is there for**: after re-enabling,
   the same key writes *and* takes the bookkeeping the suppressed branch returns before
   (`bar->value() == bar->maximum()`, `> scrollBefore`). Without that, "the scroll position
   did not move" could hold simply because no path moves it.

## Teeth: the mutation probes

Probes executed against the real product file, one mutation at a time
(`/tmp/mutate.py`, single-occurrence replacement with an occurrence check; `komport/komportview.cpp`
restored from a byte copy afterwards and verified equal with `cmp`):

| Mutation of `keyPressEvent()`'s suppressed branch | Result | Failing assertion |
| --- | --- | --- |
| guard removed (event falls through to the live path) | red | `QCOMPARE(pressed.count(), 0)` - actual 1 |
| guard consumes but calls `deselect()` | red | `QCOMPARE(view->selectedText(), selectedBefore)` - actual `""` |
| guard consumes but takes `resetScroll()`/`slotScroll()` | red | `QCOMPARE(bar->value(), scrollBefore)` - actual 53 |
| none (restored) | green | `tst_replayreplyui` 4/4 |

The second probe reproduces the symptom the handover described - which is the evidence that
the assertion now observes a real deselect of the key path if one ever appears, and that it
was the test, not the product, that was wrong here.

## Residual observation, outside this slice

The one path that clears a live selection without a user action remains the clipboard hop
`komportview.cpp:70` -> `:816-820`. It was silent in both environments above (`clipSignals=0`);
if a platform ever delivers `selectionChanged` while the view does not hold the selection,
that hop would deselect and the corrected test would go red for that reason - reported by
the scroll/selection comparisons, and not attributable to key input. That behaviour is
pre-existing, exists with key input enabled or suppressed, and changing it would alter
visible terminal behaviour (AGENTS.md: such a change needs its own reviewed slice). It is
named here as an entry criterion, not fixed here.

## Evidence

```
cmake -B build -DCMAKE_CXX_FLAGS="-Wall -Wextra" && cmake --build build -j$(nproc)   # 0 warnings, 0 errors
cd build && ctest --output-on-failure                                               # 100% tests passed, 0 failed out of 21
QT_QPA_PLATFORM=offscreen ./tests/tst_replayreplyui                                 # 4 passed, 0 failed
DISPLAY=:99 QT_QPA_PLATFORM=xcb ./tests/tst_replayreplyui                           # 4 passed, 0 failed (real Selection support)
```

## Round 3 update (2026-09-29)

The third adversarial round (`docs/reviews/2026-09-20-M10-step4-adversarial-review-3.md`) left the
correction above unchallenged and refused the slice for a different reason: the RX-overflow change in
`komport/komportserial.cpp` that the handover describes as a product fix is outside the accepted scope
(SPEC-M10 §3 keeps `KomportSerial` and the legacy RX buffer unchanged). It was removed from the slice
in the same fix round, and the view-level test's stream was shortened so it no longer depends on it -
see the fix-round section of `docs/reviews/2026-09-20-M10-step4-implementation-selfreview.md`.
