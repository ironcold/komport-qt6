## Verdict

**Rejected for round 4.** Leave both `Status: Proposed` lines unchanged.

The round-3 fixes are substantively present, but acceptance still needs:

1. **ARCHITECTURE QUESTION — owner ratification required.**  
   The process-wide choice is now coherent: ADR-011 D11/D14/register and SPEC §5.7/§5.7.1, §6–§11 consistently require every window idle, reject `New Window`, and use the existing top-level-widget traversal pattern. The current topology can implement it: `KomportDoc` already includes `komport.h`, and `KomportApp` already walks all `QApplication::topLevelWidgets()`. [ADR-011 D11](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:454), [SPEC §5.7](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:608), [existing traversal](/home/max/Development/misc/komport-qt6/komport/komport.cpp:732).

   However, this is a stricter cross-window behavior selected by the implementation agent while the owner was unavailable. It reverses no accepted decision, but it should be explicitly ratified by the owner before acceptance.

2. **HIGH — process-wide offline controls are not specified/tested across pre-existing windows.**  
   D11 requires connection/transmit/live-capture controls to be disabled and guarded while offline is active “in any window.” [ADR-011](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:477) SPEC lists a process-wide *load* walk, but does not require applying the disabled state and global entry-point guard to every already-open idle `KomportApp`; its cross-window tests cover live-load refusal, existing offline-load refusal, and refusal to create a new window, not invoking controls in existing window B while A is offline. [SPEC tests](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1023)

   This matters because those existing controls can open/configure or write: profile application closes/configures/opens the port, paste reaches simulated TX, and macros write directly. [profile application](/home/max/Development/misc/komport-qt6/komport/komport.cpp:651), [paste](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1172), [macro TX](/home/max/Development/misc/komport-qt6/komport/komport.cpp:1427)

   Minimal fix: specify a process-wide UI-state update and a single global offline predicate used by every guarded entry point in every window; restore all windows when the offline session closes. Add a two-window UI/pty test: load in A, then assert B’s controls are disabled and directly invoke B’s profile/settings/paste/macro/upload/recording paths without opening, configuring, recording, or writing.

## Round-3 dispositions

| Item | Disposition | Evidence |
|---|---|---|
| 1. Offline scope | **Partially resolved** | Technically coherent and implementable, but needs owner ratification and lacks the existing-window control propagation/guard test above. |
| 2. Recovery vs. limit/overflow | **Resolved** | ADR D3 distinguishes fewer-than-four bytes from a present length, validates 40…64 MiB before recovery, and requires 64-bit checked/subtractive arithmetic. SPEC rows 18–22 and named overflow tests agree. |
| 3. Header numeric shapes | **Resolved** | SPEC row 13 states integral `1..4294967295`; rows 16/17 require canonical decimal values representable as `i64`; boundary fixtures are named. |
| 4. Timing-selector refusals | **Resolved** | §5.7 consumes the result, restores the selector from `SessionReplayTimingChange::timing`, reports its reason, and treats play/step refusals consistently. [SPEC](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:636) |
| Cleanup: Close/Quit | **Resolved** | The table now names `closeEvent()` → `closeSession()` → serial close, explains why teardown remains enabled, and is compatible with destruction. |
| Cleanup: `close()` while Playing | **Resolved** | Transition, state table, operation text, and test agree: it cancels the callback and returns to Idle without refusal. |
| Cleanup: dynamic evidence | **Resolved** | §5.8 correctly limits absent `bytesWritten` to “no accepted byte”; source audits carry the structural claim. This matches `writeRaw()`, which emits only after a positive accepted write. [implementation](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:401) |

Nothing from round 3 was silently dropped.

## Same-pass editorial cleanup

- **DOCUMENTATION:** SPEC §8 says replay has “a report at the end and at a close,” but §5.5 specifies that `close()` returns no report and emits only a state change. Change §8 to say “at the end and on `stop()`,” or remove the report claim. [conflict](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:1103)

After owner ratification plus the global-window control fix and this cleanup, change `Status: Proposed` to `Status: Accepted` in [ADR-011](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-011-session-reader-passive-replay.md:3) and [SPEC-M10](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M10-session-reader-passive-replay.md:3).

I could not verify proposed M10 implementation behavior, tests, source audits, GUI/offscreen behavior, or pty evidence: M10 has no code/tests yet. I performed only static document/code review and made no files or patches.