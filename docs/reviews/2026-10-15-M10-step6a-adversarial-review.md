# M10 step 6a - adversarial review (2026-10-15)

Slice: the offline state in the application (SPEC-M10 5.7/5.7.1): load action, indicator, Close Session, controls disabled in every window, offline guard in every entry point, window teardown. Runner: codex-companion adversarial-review, scope working-tree.

## Focus given to the reviewer

Review M10 step 6a (SPEC-M10 sections 5.7, 5.7.1, 11 step 6a): the application wiring of the offline state in komport/komport.cpp/.h, komport/komportdoc.cpp/.h (offlinePath only) and tests/tst_sessionreplayui.cpp. Focus: (1) does ANY user path still reach a transport write, a port open/configure/close, or a recording start while a session is loaded in ANY window - check every row of the 5.7.1 table against the code, plus paths the table may have missed (shortcuts, toolbar, profile combo returnPressed, macro bar, eventFilter, drag/drop, QAction triggered vs toggled, the recent-files menu actions already built before the load); (2) the process-wide update: windows created/destroyed while offline, closeEvent vs destructor, re-entrancy of closeOfflineSession from destructor, controls that re-enable themselves (editPaste via slotViewModified, recent menu); (3) correctness of the tests: do they actually fail when the guard or the update is removed; (4) the spec amendment in 5.7 ('Window teardown') and the 6a/6b cut. KomportSerial and the legacy RX buffer must be untouched. Owner's foreign changes (AGENTS.md, CLAUDE.md, docs/project-context.md, docs/example-charsets/README.md, docs/translation-glossary.md) are out of scope. German translations of the new strings are deliberately deferred to one translation step after 6b - do not flag that.

## Round 1 - verdict: approve

# Codex Adversarial Review

Target: working tree diff
Verdict: approve

Ship: no material offline-state escape was found. The 5.7.1 rows, pre-existing recent actions, profile return path, macro/key paths, teardown, and process-wide restoration are guarded or intentionally exempted by the accepted teardown rule.

No material findings.

## Evidence

- -Wall -Wextra build clean; ctest 23/23 (tst_sessionreplayui: 16 tests).
- Mutation probes, each caught: macro guard removed; setKeyInputEnabled(true) always; update applied only to the loading window; paste re-enable ignoring the offline flag; destructor safety net removed; New Window guard removed.
- Deferred on purpose: German/French translations of the new strings (one translation step after 6b).
