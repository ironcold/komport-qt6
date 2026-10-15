## Verdict

**Yes — implementation of M10 step 4 may start on this plan.**

## Dispositions

- Re-entry test: resolved. It now creates synchronous re-entry from `cellChanged` during an outer replay render, covers nested replay and re-entrant live input, and asserts caller-mode restoration.
- Replay-query/view-path test: resolved. It explicitly drives `KomportView::slotReplayReceivedChar()`, uses sentinels and a private CSI-`c` variant, and states the required emulation-before-bookkeeping order.
- Earlier corrections: remain correctly incorporated: scoped save/restore for both entries, explicit default-allowed mode, no new transport seam, complete reply-site inventory, and staged source audit.

## New findings

None.

The specified choke point and `QScopedValueRollback<bool>` mode design are appropriate. The private CSI-`c` hostile variant is valid because the dispatcher selects on final `c` and the attributes handler ignores parameters. The stated view ordering matches the existing live signal connection order.

## What I could not verify

No step-4 production code or tests exist yet, so I could not compile the implementation or exercise the PTY, `bytesWritten`, source audit, or actual nested signal execution.