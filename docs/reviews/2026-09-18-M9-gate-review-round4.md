## Round-4 verdict

The prior **HIGH is closed**. The timer is now independent of subsequent events: ADR-010 requires it to be armed on the first unflushed write and fire within one second even when idle ([ADR-010:164](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:164)); SPEC-M9 mirrors this ([SPEC-M9:193](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:193)), provides the scheduler seam ([SPEC-M9:248](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:248)), and names idle, burst, and timer-failure tests ([SPEC-M9:305](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:305)).

This is implementable under ADR-009: `QTimer` is QtCore, needs no widget/app type, and can run on the recorder/session thread. ADR-009 explicitly permits non-UI Qt modules in these contracts ([ADR-009:29](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-009-multi-executable-product-architecture.md:29)). It is, naturally, an event-loop timer rather than a hard real-time guarantee.

## New finding

**HIGH — the successful header write is not explicitly covered by the periodic-flush rule.**

The start sequence writes the header and then enters `Live` ([SPEC-M9:191](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:191)), while the timer is specified for an unflushed write *of a live recording* ([ADR-010:164](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:164)). Thus a conforming implementation can write a complete header into the sink’s process buffer, enter `Live`, receive no event, and never arm a timer. That contradicts the stated post-header crash/loadability reasoning ([ADR-010:149](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-010-live-session-recording.md:149)) and is not covered by the idle-tail test, which begins by writing an event ([SPEC-M9:305](/home/max/Development/misc/komport-qt6/docs/specs/SPEC-M9-live-session-recording.md:305)).

Minimal fix: explicitly make a successful header write participate in the flush policy—either flush it synchronously before declaring the recording `Live`, or arm the same one-second timer immediately after it. Add an idle zero-record/header-tail test. If choosing the timed option, qualify the crash/loadability language for the pre-flush header window.

This is substantive, not cosmetic.

## Release decision

- ADR-010 and SPEC-M9: **do not mark Accepted yet**; a status-only change is insufficient.
- ADR-006’s normative writer-profile subsection: **may stand** as an amendment; its schema is now exhaustive ([ADR-006:155](/home/max/Development/misc/komport-qt6/docs/architecture-decisions/ADR-006-kpsession-file-format-v1.md:155)).
- SPEC-M8 §6.2’s D8 sentence: **may stand** as the bounded read-only M9 amendment.
- M9 implementation under SPEC-M9 §11: **do not start** until the header-tail flush/loss case is resolved.

No build or tests were run in this read-only review.