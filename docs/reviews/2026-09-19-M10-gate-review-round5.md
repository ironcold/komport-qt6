## Verdict: rejected pending owner ratification

The two applied text fixes are accepted. No further documentation edit is required.

- The offline rule now has the required process-wide strength: every open window’s relevant controls are disabled, every guarded entry point uses the shared predicate, and closing restores controls everywhere. The new §7 tests are defined and cited consistently in §8, §9, and §11.
- ADR-011 D11/D14 and SPEC-M10 §5.7/§5.7.1 agree; I found no residual normative per-window/per-view alternative. The loading-window-only indicator/replay controls are correctly distinguished from process-wide live-control disabling.
- §8 now correctly says a report is available “at the end and from `stop()`”; it no longer claims one from `close()`.
- §8 test identifiers are all declared in §7; references and row/section numbering checked here resolve.

I agree that owner ratification is a process event, not a needed prose change. The documents adequately record the selected process-wide decision and rejection of the per-window alternative, but they cannot themselves prove that the owner has ratified it.

After explicit owner ratification, change only these two lines in the same step:

- [ADR-011 status](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:3) → `Status: Accepted`
- [SPEC-M10 status](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:3) → `Status: Accepted`

Static checks also confirm the cited existing-code basis: top-level window traversal exists, the named action/slot surface exists, `closeEvent()` calls `closeSession()` which closes the serial transport, and `writeRaw()` emits `bytesWritten` only after a positive accepted write. See [traversal](/home/max/Development/misc/komport-qt6/komport/komport.cpp:736), [close path](/home/max/Development/misc/komport-qt6/komport/komport.cpp:953), [serial close](/home/max/Development/misc/komport-qt6/komport/komportdoc.cpp:235), and [write primitive](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:401).

I could not verify M10 implementation, its source audits, GUI/PTY behavior, or tests because no M10 code or test targets exist yet. No files were modified.