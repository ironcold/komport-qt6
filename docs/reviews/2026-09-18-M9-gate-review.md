## Round 1 verdict

**Do not mark either draft Accepted or start M9 implementation.** The documents contain unresolved BLOCKER/HIGH issues in the file contract, frozen controller surface, configuration provenance, lifecycle, and failure semantics.

### Findings

1. **BLOCKER — Header writer profile is not specified enough to produce one interoperable v1 format.**

   Evidence: [ADR-010](../home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:173) leaves all identity strings to D4; [SPEC-M9](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:130) says only “as ADR-006 requires” for root fields. ADR-006 requires format/version, creation time, application version, source descriptors, and clock domains, but the proposed writer has no canonical JSON shape or values.

   This leaves implementation to invent persisted names/structures, contrary to the fixed-format requirement.

   Minimal fix: add a normative header example/schema, including root member names and types, and resolve D4. For example:

   ```text
   M9 writes this fixed v1 header profile:
   {
     "format": "komport-session",
     "version": 1,
     "created": "<ISO-8601 UTC string>",
     "application": { "name": "Komport", "version": "<application version>" },
     "sources": [{
       "sourceId": 1,
       "clockDomainId": "local-process-monotonic-v1",
       "name": "local serial",
       "transport": "serial",
       "configuration": { "requested": ..., "effective": ...,
                          "localBuffering": ..., "compatibility": ... }
     }],
     "clockDomains": [{
       "id": "local-process-monotonic-v1",
       "kind": "process-monotonic",
       "reference": {
         "sourceTimestampNs": "<canonical decimal i64>",
         "sessionTimestampNs": "0"
       }
     }]
   }
   ```

   State that endpoint is retained in the configuration snapshot and is not redacted as a credential.

2. **BLOCKER — D8 changes the frozen M8 public controller contract, but its API is neither specified nor reconciled with M8.**

   Evidence: ADR-010 proposes an accessor at [lines 194–207](../home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:194). M8 instead says `KomportDoc` exposes only the controller’s event signal to new consumers at [SPEC-M8:231–237](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:231). The current public controller has no such accessor at [sessioncontroller.h:74–86](../home/max/Development/misc/komport-qt6/komport/sessioncontroller.h:74).

   SPEC-M9 also contradicts itself: it depends on D8 at [lines 137–145](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:137) and implements it at [lines 267–270](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:267), yet says no decision exists beyond D1–D7 at [line 259](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:259).

   Minimal fix: resolve D8, specify the QtCore-only public type/signature, and amend the M8 wording that says the event signal is the only new-consumer surface. For example:

   ```cpp
   struct SessionClockDomainReference {
     bool valid = false;
     qint64 sourceTimestampNs = 0;
     qint64 sessionTimestampNs = 0; // exactly 0 when valid in v1
   };

   SessionClockDomainReference clockDomainReference() const;
   ```

   The accessor must be read-only and return `valid == false` before the first accepted event.

3. **HIGH — “Applied configuration as of recording start” is false when recording is armed while idle.**

   Evidence: ADR-010 permits recording without an open transport at [lines 105–109](../home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:105); SPEC-M9 writes the supplied applied snapshot at start and remains armed at [lines 132 and 152–155](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:132). But M8 explicitly says a closed-port configuration request is stored only and no hardware transaction occurs at [komportserial.cpp:686–699](../home/max/Development/misc/komport-qt6/komport/komportserial.cpp:686). The later open performs the actual transaction.

   A header written from this snapshot would claim applied hardware values that were not applied.

   Minimal fix: restrict M9 start to `SessionController::Live`. This preserves D7(a) and makes the snapshot honest:

   ```text
   Start is accepted only while the controller is Live. The application passes
   requestedConfiguration() and effectiveConfiguration() by value at start; the
   header calls this the configuration applied as of recording start. If Idle,
   start is refused without creating a file and explains that live recording
   starts after the transport is open.
   ```

4. **HIGH — The stated destruction order does not make the final `closed` event recordable.**

   Evidence: ADR-010 makes that claim at [lines 42–46](../home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:42). In reality, `KomportDoc` destroys the controller before the transport at [komportdoc.cpp:51–58](../home/max/Development/misc/komport-qt6/komport/komportdoc.cpp:51), and `KomportSerial` destruction deliberately emits no `closed` event at [komportserial.cpp:141–147](../home/max/Development/misc/komport-qt6/komport/komportserial.cpp:141). M8 has an explicit regression test proving document destruction emits no event at [tst_sessiondocument.cpp:151–158](../home/max/Development/misc/komport-qt6/tests/tst_sessiondocument.cpp:151).

   The normal application close path can record `closed`, because it calls `serial->close()` before document destruction at [komport.cpp:927–937](../home/max/Development/misc/komport-qt6/komport/komport.cpp:927). SPEC-M9 does not specify that recorder finalisation must occur after that call.

   Minimal fix:

   ```text
   On accepted application close: first call KomportSerial::close() while the
   controller and recorder are alive; then stop and finalise the recorder; only
   then continue destruction. Direct KomportDoc destruction emits no terminal
   session event and finalises only the already-written recording.
   ```

5. **HIGH — Crash and short-write claims overstate recovery and durability.**

   Evidence: ADR-010 claims a crash can truncate only the final record at [lines 141–142](../home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:141); SPEC-M9 repeats this at [line 162](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:162). With no `fsync`, a host/power failure can lose a non-durable suffix, not merely leave a truncated last record. Also, a short first combined header-plus-record write may leave an incomplete header, which ADR-006 says fails loading rather than recovers.

   Minimal fix:

   ```text
   A successful flush bounds only the writer's buffered tail; without fsync M9
   gives no durable-storage bound for a host or power failure. A recovered file
   may omit a non-durable suffix and may end in one truncated final record.
   A short initial header-plus-first-record write can leave an unparseable file;
   only a failure after a complete header and record prefix is recoverable under
   the truncated-final-record rule.
   ```

   Define flush failure as a damaged-state transition and report it.

6. **HIGH — The writer has no limits/overflow policy for ADR-006’s 16 MiB header and 64 MiB record limits.**

   Evidence: ADR-006 requires reader limits at [lines 57–61](../home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:57). SPEC-M9 promises every accepted event becomes a record at [lines 111–120](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:111), but `isValidSessionEvent()` has no size validation at [sessionevent.h:88–136](../home/max/Development/misc/komport-qt6/komport/sessionevent.h:88). It could therefore write a record a conforming reader must reject.

   Minimal fix:

   ```text
   Before allocation or serialization, use checked arithmetic. Reject and stop
   damaged if headerLength exceeds 16 MiB, if metadataLength or payloadLength
   exceeds u32, or if recordLength = 40 + metadataLength + payloadLength exceeds
   64 MiB. The offending event is not written; report its sequence and sizes.
   ```

   Add boundary and overflow tests.

7. **HIGH — D6 conflicts with the declared non-goal of not changing the text logger.**

   Evidence: SPEC-M9 says the text logger is unchanged at [lines 40–41](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:40), but D6 leaves the recording action unresolved at [ADR-010:180–183](../home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:180). The current `Record Session…` action is the legacy text logger, wired to character signals at [komport.cpp:208–211](../home/max/Development/misc/komport-qt6/komport/komport.cpp:208) and [komport.cpp:1401–1420](../home/max/Development/misc/komport-qt6/komport/komport.cpp:1401).

   Minimal fix: retain that action and logger unchanged. Add a separate, clearly named checkable action such as `Record Live Session…` for `.kpsession`; its checked state is the indicator, and finalisation/errors go through the existing translated status-label mechanism.

8. **MEDIUM — The recorder state machine, D2 interval, file naming, and overwrite behavior are still unresolved.**

   Evidence: SPEC-M9 uses armed/live/damaged terminology at [lines 152–160](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:152) but has no transitions; D2, D5, and D6 remain open at [ADR-010:160–183](../home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:160).

   Minimal fix: specify `Stopped → Armed → Live → Stopped` and `Armed|Live → Damaged → Stopped`, including start-after-damage and close behavior. State a concrete flush interval and whether it is checked on each event/timer tick. Define overwrite confirmation and a deterministic default name.

9. **TEST GAP — Tests are prose-only, have no acceptance-to-test mapping, and lack required write/flush seams.**

   Evidence: SPEC-M9 lists categories rather than named tests at [lines 190–227](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:190); acceptance criteria at [lines 229–244](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:229) do not map to named tests. “Short write via a seam” is required at [line 215](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:215), but the seam is undefined.

   Minimal fix: name test methods and add a mapping table. Define an internal testable file-sink seam and a controllable clock/flush seam. Cover:
   - first combined header/record short write;
   - later record short write after a valid prefix;
   - flush failure;
   - actual damaged file parsed by the structural reader;
   - oversized header/record rejection;
   - close ordering with final `TransportClosed`;
   - no automatic start/no persisted recording state/no transport invocation.

10. **DOCUMENTATION — Several citations and factual claims are wrong.**

    Evidence: ADR-010 cites “ADR-006 §7.5” at [lines 61 and 142](../home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:61); SPEC-M9 repeats it at [lines 162 and 218](../home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:162). ADR-006 has no §7.5; its recovery rule is [ADR-006:57–61](../home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:57). Section 7.5 is in the older architecture document.

    D5 also claims a date-and-time naming practice at [ADR-010:177–179](../home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:177), but the existing logger opens an unnamed file in `QDir::currentPath()` at [komport.cpp:1405–1406](../home/max/Development/misc/komport-qt6/komport/komport.cpp:1405).

    Fix the citations and remove the unsupported naming claim.

## D1–D8 decisions

- **D1 — Overturn. Choose (c): create no file until the first record.** It avoids an invalid empty `.kpsession` surviving a crash or failed deletion. The delayed write-open error is preferable to leaving a misleading artifact.

- **D2 — Confirm (c), with a concrete interval.** Use a documented interval, e.g. one second, plus flush on stop; define the non-`fsync` durability limits precisely and test it through a clock/flush seam.

- **D3 — Confirm (a).** A minimal independent test-only structural parser is sound. It must not link production codec code, become a public API, or be presented as M10’s reader.

- **D4 — Confirm a fixed local profile.** Use non-empty `clockDomainId`/`id` `local-process-monotonic-v1`, source name `local serial`, transport `serial`, and the non-redacted endpoint in configuration. Device paths are not credentials under ADR-006.

- **D5 — Confirm explicit behavior.** Use `QDir::currentPath()` to match current dialogs, propose a timestamped `.kpsession` name, and require normal save-dialog overwrite confirmation. The existing logger supplies no timestamped naming convention.

- **D6 — Better option: add a separate live-session action.** Do not repurpose the text logger’s existing `Record Session…` action. Add `Record Live Session…`; checked state is the indicator; status label carries start/stop/failure outcome.

- **D7 — Confirm (a), but only after applying Finding 3.** The application supplies an immutable requested/effective snapshot by value at start. It is correct only when recording starts while the session is live.

- **D8 — Confirm (b).** The accessor is the right M9 addition: the controller owns `mReferenceSet` and `mReferenceSourceTimestampNs` at [sessioncontroller.h:157–161](../home/max/Development/misc/komport-qt6/komport/sessioncontroller.h:157), and the anchor is set before emission at [sessioncontroller.cpp:280–287](../home/max/Development/misc/komport-qt6/komport/sessioncontroller.cpp:280). Option (a) is more faithful to the original frozen M8 surface, but it rejects the normal “start recording after connection” workflow. B is preferable only with the explicit API and M8-document amendment above.

## Scope assessment

No production reader, replay, decoder, export, CRC/index/compression, multi-source, or format-version work has been pulled into M9. The test-only structural parser remains within M9 if constrained as D3 states.

M9 is nevertheless incomplete as specified: it needs the resolved header profile, valid applied-configuration precondition, D8 contract amendment, lifecycle ordering, bounds, and test plan before it can reliably write conformant v1 files or leave M10 an unambiguous input.

## Could not verify

I made no changes and ran no build/tests in this read-only environment. I verified the cited M8 behavior statically and consulted the recorded M8 build/test evidence in [the M8 self-review section 6](../home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-implementation-selfreview.md:148). No M9 production code exists to inspect.