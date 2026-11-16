# M10 step 7 - adversarial review (2026-10-16)

Slice: the source/dependency audits (SPEC-M10 5.8) and the dynamic no-write evidence. Tests and CMake only; SPEC 5.8 amended. Runner: codex-companion adversarial-review, scope working-tree.

## Focus (round 1)

Review M10 step 7 (SPEC-M10 5.8 and 11 step 7): the source/dependency audits and the dynamic no-write evidence. Files: tests/tst_replayreply.cpp (helpers hoisted to file scope; new section 5 of replayEntriesAndReplySitesPassTheSourceAudit auditing KomportApp in komport/komport.cpp; new offlineUnitsNameNoWriteEntryPointOrTransport over sessionreader.* and sessionreplayplayer.*), tests/tst_sessionreplayui.cpp (noTransmitDuringReplay, replayingARecordedPtySessionTransmitsNothing, plus the earlier adapter pty test), tests/CMakeLists.txt (compile definitions). No production code changed in this slice. Focus: do the audits actually establish what SPEC 5.8 claims (could a write entry point or a transport include in the offline units / the replay-driven KomportApp functions slip past the regex/brace extraction - comments, strings, macros, multiple definitions, lambdas inside bodies, functions the extraction misses); are limits of the audit stated; do the pty tests prove silence (timing, non-blocking read, the recorded fixture really containing a live-answered query and both directions, seam use of opening the port while offline); would they fail under the obvious regressions. Translations deferred on purpose. Owner's foreign changes out of scope.

## Round 1

# Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No-ship: the new source audit is bypassable by valid preprocessor constructs, and the dynamic evidence does not reliably prove the claimed recorded-query and peer-silence cases.

Findings:
- [high] Transport dependency audit can miss valid preprocessor includes (tests/tst_replayreply.cpp:550-561)
  The include regex recognizes only literal `#include <...>` or `#include "..."`. A valid `#define transport_header "itransport.h"` followed by `#include transport_header` (or a line-spliced directive) is not matched; the remaining text also need not contain the case-sensitive `ITransport` token. The offline unit would compile with the forbidden transport dependency while this test remains green, defeating the SPEC 5.8 dependency claim.
  Recommendation: Make this audit compiler-aware: derive and inspect resolved compiler dependencies/preprocessed active source, rejecting forbidden transport headers there. Keep any text scan only as a supplementary diagnostic.
- [medium] Recorded PTY fixture does not prove the terminal query was answered and recorded (tests/tst_sessionreplayui.cpp:1048-1067)
  The test injects a query, but after reload it only requires at least three events and at least one TX/RX event. The explicit `typed\r` write alone satisfies TX; serial RX may legitimately arrive in multiple reads, so fragmented RX plus that typed TX satisfies these assertions even if the emulation never answered `CSI 5 n` or the recorder omitted that answer. The advertised end-to-end replay of a live-answered query can therefore pass without its critical fixture content.
  Recommendation: Before replay, assert the live peer received the exact status reply, then inspect the loaded event stream by concatenated direction (or exact ordered events) to require the injected RX payload, the expected reply payload, and `typed\r`.
- [medium] Peer-silence assertion is neither timeout-based nor error-safe (tests/tst_sessionreplayui.cpp:1016-1022)
  After a fixed delay, the test performs one non-blocking read and accepts every non-positive result. That treats `EINTR`, `EIO`, or another read failure as silence, and a write delivered after that single read passes. This contradicts the specified peer observation 'asserted with a timeout' and weakens the only boundary-level check for a path bypassing `ITransport::bytesWritten`.
  Recommendation: Poll the PTY through a bounded timeout; fail immediately on received bytes, accept only `EAGAIN`/`EWOULDBLOCK` while waiting, and fail on all other read errors. Apply the same helper to the recorded-PTY test.

Next steps:
- Harden the compiler/source audit, then make the recorded fixture and PTY silence checks assert their claimed evidence explicitly.

## Round 2

# Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No-ship: the new source audit can be bypassed by macro-expanded write calls or a differently named transport dependency, so it does not establish the claimed structural no-write property.

Findings:
- [medium] The source audit only recognizes literal spellings, so compiled write paths can evade it (tests/tst_replayreply.cpp:177-185)
  `reportsTransmit()` searches only literal `putStr`, `putChar`, and `writeBytes` tokens. A token-pasting macro can expand to one of these calls without any matching source token; `QMetaObject::invokeMethod` is also intentionally hidden by string stripping. Likewise, the compiler dependency pass rejects only three named transport headers/modules, so a new differently named QtCore-only transport/writer header is not excluded. The finite PTY fixtures would catch ordinary unconditional regressions, but cannot turn this lexical check into proof that no replay-driven path can transmit under all inputs. The stated limit covers indirect calls but not macro-expanded write calls or unknown transport dependencies.
  Recommendation: Preprocess the audited translation units with their real compile arguments and scan the main-file-expanded output (or use an AST-based check) for the forbidden calls. Make the dependency assertion an allow-list of permitted project headers/transitive paths, rather than a deny-list of current transport spellings, and document any remaining deliberately bounded cases in SPEC 5.8.

Next steps:
- Strengthen the audit before accepting its structural no-write claim; retain the PTY tests as runtime evidence.

## Round 3

# Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No-ship: the new structural audit can miss a transmitting replay handler, so it does not establish the claimed KomportApp invariant.

Findings:
- [medium] KomportApp audit is not macro-expanded and misses indirect dispatch (tests/tst_replayreply.cpp:469-525)
  The KomportApp portion scans raw `komport.cpp` only. The compiler `-E` scan is restricted to the two offline `.cpp` units (lines 596–639), while the driven-handler checks neither expand macros nor reject `invokeMethod`. A token-pasted write macro or inline helper used by, for example, `slotReplayStepTx` can evade the identifier scan; that handler is also not exercised by the pty full-play test. The documented claim that only `slotMacroTriggered` can transmit is therefore stronger than the test establishes.
  Recommendation: Preprocess KomportApp with its real compile definitions/includes and attribute expanded tokens to the original source line, rejecting write names and `invokeMethod` in every replay-driven handler while retaining the explicitly guarded macro-slot exception. Add a pty test that invokes each replay stepping path, or otherwise cover the remaining handlers dynamically.

Next steps:
- Extend the app-side audit and rerun the focused replay-reply and replay-UI tests.

## Round 4

# Codex Adversarial Review

Target: working tree diff
Verdict: needs-attention

No-ship: the source audit can miss reachable write calls in headers and is not preprocessing under the flags used to build the production units, so it does not support the claimed structural no-write guarantee.

Findings:
- [high] KomportApp audit drops project-header inline helpers (tests/tst_replayreply.cpp:493-503)
  The line-marker filter retains only lines attributed exactly to komport.cpp. An inline/project-header helper called by a replay handler is therefore omitted entirely, while its call site contains no listed write identifier. For example, a helper in komport.h that calls putStr would pass this source audit despite being reachable from slotReplayStepTx. The finite PTY fixture may not exercise a conditional helper branch, so it cannot repair this structural gap.
  Recommendation: Include project-header spans in the audited preprocessed source and audit reachable helper bodies (with explicit exclusions for transport declarations), or narrow the SPEC claim to exclude header-defined helpers.
- [high] Preprocessor audit does not use the production compile definitions (tests/tst_replayreply.cpp:481-486)
  The audit manually invokes the compiler with only KOMPORT_VERSION and QT_WIDGETS_LIB. The actual komport_core build has QT_CORE_LIB, QT_GUI_LIB, QT_PRINTSUPPORT_LIB, and QT_SERIALPORT_LIB; the offline-unit invocation similarly omits QT_CORE_LIB. A write or transport include under one of those real build guards is compiled into production but absent from both -E and -M audit results.
  Recommendation: Have CMake provide the effective compile definitions/options for komport_core and komport_session_contract_check to the audit, or generate and run an audit command from the actual compile command rather than reconstructing flags in the test.

Next steps:
- Fix both audit blind spots, then rerun the PTY and source-audit tests in a writable test environment.

## Round 5

# Codex Adversarial Review

Target: working tree diff
Verdict: approve

Approve: within its explicitly documented lexical/runtime bounds, the source audits and live-pty tests credibly cover the M10 step-7 no-write claims. No material bypass or evidence gap found.

No material findings.

## Resolution summary

- Round 1: include audit was a text scan of the directive (bypassable by a macro-spelled include); recorded-pty fixture did not prove the live-answered query; peer silence was one non-blocking read. Fixed: compiler `-M` include set, exact-reply and per-direction assertions, bounded `peerStaysSilent`.
- Round 2: deny-list and literal spelling. Fixed: allow-list of project headers + "Qt headers only from QtCore", macro-expanded scan (`-E`, line markers) of the offline units.
- Round 3: the KomportApp audit was raw-text and missed indirect dispatch. Fixed: preprocessed `komport.cpp`, `invokeMethod` and live entries rejected in every replay handler, new pty test `everyReplayControlReachesNoTransport`.
- Round 4: header-defined helpers and production compile definitions missed. Fixed: project-header active text searched for member calls of the write entry points; Qt module guards mirrored in tests/CMakeLists.txt (a generator expression over komport_core's definitions strips them from the test itself - observed as a crash of the first test).
- Round 5: approve, within the bounds SPEC 5.8 now states.

## Evidence

- -Wall -Wextra build clean; ctest 23/23; tst_replayreply 8 tests, tst_sessionreplayui 28 tests.
- Probes, each caught: a write call in the adapter; the macro slot's guard moved; a forbidden include (direct, via macro, an unlisted project header); a token-pasted write name in the offline unit and in a replay handler; `invokeMethod` in the adapter; an inline helper in a project header; TX written to the port; the live receive entry instead of the replay entry.
- Deferred on purpose: German/French translations of the new strings; `noSeekOperationExists` (step 9 static inspection).
