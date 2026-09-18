## Round-2 verdict

Do **not** mark ADR-010 or SPEC-M9 Accepted, and do **not** start M9 implementation yet.

| # | Verdict | Evidence |
|---|---|---|
| 1. Header writer profile | **Partially closed — BLOCKER remains.** The root profile and local constants are fixed, but `requested`/`effective` still use `"<hardware field>": "<value>"`, so their exhaustive member names and JSON types are not normative. “Use the field names” does not supply those names in the accepted spec. [ADR-006:115](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:115), [ADR-006:143](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:143), [SPEC-M8:137](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:137). |
| 2. D8 / frozen M8 surface | **Closed.** ADR-010 gives the exact QtCore-only struct and signature; SPEC-M8 records it as the M9 addition; SPEC-M9 lists D8 as resolved. [ADR-010:218](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:218), [SPEC-M8:243](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M8-session-transport-foundation.md:243), [SPEC-M9:365](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:365). |
| 3. Applied configuration while idle | **Closed.** Start is refused unless the controller is `Live`, without creating a file; the header snapshot is therefore taken only when values can be honestly described as applied. [ADR-010:87](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:87), [SPEC-M9:180](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:180). |
| 4. Final `TransportClosed` recording | **Closed.** The documents correctly distinguish destruction lifetime from application-close behaviour and make “close transport, then finalise recorder” normative. [ADR-010:51](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:51), [SPEC-M9:219](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:219). |
| 5. Crash / short-write claims | **Partially closed — HIGH remains.** The host/power, short-header-write, and flush-failure cases are now separated. However, ADR-010 still says a process crash “can truncate only the final record,” while its own header-write rule permits an incomplete initial header; it also calls the loss window bounded “plus the OS page cache” after correctly saying that `fsync` is absent. Those statements overstate a durability bound. [ADR-010:145](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:145), [ADR-010:151](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:151), [ADR-010:158](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:158), [ADR-010:173](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:173). |
| 6. Limits and overflow | **Closed.** Checked arithmetic, 16 MiB header, `u32` component lengths, 64 MiB body limit, damaged transition, reporting, and named boundary tests are specified. [SPEC-M9:126](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:126), [SPEC-M9:283](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:283). |
| 7. Text logger conflict | **Closed.** The old `Record Session...` path remains untouched; the new checkable `Record Live Session...` action is separately specified. [SPEC-M9:43](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:43), [SPEC-M9:205](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:205). |
| 8. State machine, flush, naming, overwrite | **Closed.** States/transitions, one-second policy, current-directory timestamped suggestion, and normal overwrite confirmation are specified. [SPEC-M9:160](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:160), [SPEC-M9:191](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:191), [SPEC-M9:209](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:209). |
| 9. Test mapping and seams | **Closed.** The sink and clock seams are defined, named tests are provided, and acceptance criteria map to test names. [SPEC-M9:227](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:227), [SPEC-M9:275](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:275), [SPEC-M9:320](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:320). |
| 10. Citations / naming claim | **Closed.** The erroneous ADR-006 §7.5 reference is replaced with an explicit note distinguishing the older architecture text from ADR-006’s normative Decision section; the unsupported claim about an existing timestamp convention is absent. [ADR-010:242](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:242), [SPEC-M9:209](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:209). |

### D1 deviation

**Confirmed in principle, with one wording correction required.** ADR-006 imposes no minimum record count: a complete, valid header followed by zero records is loadable v1 content. [ADR-006:14](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:14), [ADR-006:57](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:57).

The live-only precondition makes the anchor and applied snapshot available at start. [ADR-010:92](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:92). Thus a successfully written header with zero records is valid and is better than deferring file creation.

But “no invalid empty `.kpsession` can exist” is too broad: a failed/zero-byte or short initial header write can leave an empty or incomplete, unloadable artifact, as ADR-010 itself acknowledges. [ADR-010:108](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:108), [ADR-010:151](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:151). Qualify the claim as applying after a successful complete header write.

### Required fixes

**BLOCKER — complete the persisted configuration schema.** In ADR-006, replace the placeholders with the exact nested members and types: `endpoint`, `baudRate`, `dataBits`, `stopBits`, `parity`, `flowControl` as strings in both `requested` and `effective`; `rxQueue` and `flushRate` as JSON numbers; `startBits` as a string. State that these nested objects have no additional members. Mirror that fixed shape in SPEC-M9 §5.4 and its header-profile test.

**HIGH — correct durability wording.** State that the “only final record” bound applies only after the complete header write succeeded; an interrupted/short initial header write can leave an unloadable file. Remove the claim that the loss window is bounded by the OS page cache; without `fsync`, only the process-buffer flushing cadence is bounded, not durable storage loss.

No separate new finding beyond the two residual findings above. No cosmetic items.

### Release decision

- ADR-010 and SPEC-M9: **No**, do not mark Accepted yet; a status-line-only change is insufficient.
- ADR-006 writer-profile amendment: **No**, not until the nested configuration schema is made fully normative.
- SPEC-M8 §6.2 D8 amendment: **Yes**, it can stand, paired with ADR-010’s exact declaration.
- M9 implementation: **No**.

The single blocking item is the incomplete fixed v1 header schema for `sources[0].configuration`; as written, it still permits incompatible persisted dialects.

### Not verified

I made no changes and did not build or run tests in this read-only environment. No M9 code exists to inspect, so the named tests, write/flush seams, UI wiring, and runtime close ordering remain prospective. I statically checked the M8 interfaces/close path and consulted the recorded M8 verification evidence in [M8 self-review §6](/home/max/Development/misc/komport-qt6/docs/reviews/2026-09-18-M8-implementation-selfreview.md:148).