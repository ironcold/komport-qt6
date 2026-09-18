## Round-3 verification verdict

1. **Nested configuration schema — Closed.**  
   ADR-006 now fixes every nested member, type, and the no-extra-members rule: [`ADR-006:115`](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:115), [`ADR-006:161`](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:161). SPEC-M9 mirrors it and names a completeness/no-extras test: [`SPEC-M9:148`](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:148), [`SPEC-M9:279`](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:279). The M8 producer corroborates the exact keys: [`transportconfiguration.cpp:56`](/home/max/Development/misc/komport-qt6/komport/transportconfiguration.cpp:56), [`transportconfiguration.cpp:81`](/home/max/Development/misc/komport-qt6/komport/transportconfiguration.cpp:81), [`komportserial.cpp:896`](/home/max/Development/misc/komport-qt6/komport/komportserial.cpp:896).

2. **Durability wording — Closed as the round-2 wording correction.**  
   ADR-010 now separately and accurately states process-crash, host/power-failure, and incomplete-header cases, and restricts the first two bounds to after a successful complete header write: [`ADR-010:146`](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:146), [`ADR-010:156`](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:156). The lifecycle row matches: [`SPEC-M9:201`](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:201).

3. **D1 wording correction — Closed.**  
   The assertion is now correctly conditional on the complete header having been written, with failed/short header write expressly called out as unloadable: [`ADR-010:108`](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:108), [`ADR-010:193`](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:193).

## New finding

**HIGH — the specified flush mechanism does not provide the claimed one-second process-buffer bound.**

The documents say flushing is checked only “on each event and on stop” and describe it as “at most every second”: [`ADR-010:163`](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:163), [`SPEC-M9:193`](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:193). After an event writes buffered data, an otherwise idle recording can receive no later event and no stop for an arbitrary duration; no check occurs, so its process-buffer tail is not time-bounded. The named test also asserts only “at most once per second,” which tests a maximum flush frequency, not a maximum retention time: [`SPEC-M9:304`](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:304).

Minimal fix: specify a session-thread periodic flush mechanism, active while a successful-header `Live` recording has pending data, that flushes no later than one second after the first pending write. Keep final flush on stop; define timer flush failure as `Damaged`; add a deterministic idle-period test proving the pending tail is flushed within one second without another event. A test scheduler/tick seam may supplement the existing clock seam.

## Release decision

- ADR-006’s writer-profile amendment: **may stand**.
- SPEC-M8 §6.2 D8 amendment: **may stand**.
- ADR-010 and SPEC-M9: **do not mark Accepted yet**; a status-line-only change is insufficient.
- M9 implementation under §11: **do not start yet**.

The remaining objection is substantive, not cosmetic: the required crash-loss bound cannot be implemented from the current event/stop-only flush rule. No repository changes or tests were performed in this read-only review.