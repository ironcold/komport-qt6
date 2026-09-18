**M9 may not yet be declared closed.** The code remedies are sound, and I do not require the proposed recorder volume test for closure. The remaining issues are small but prevent a verified closing approval:

- SPEC-M9 still says an unqualified “flush failure” is a damaged transition, conflicting with ADR-010’s accepted pre-`Live` start-flush exception. Update [SPEC-M9 §5.6](docs/specs/SPEC-M9-live-session-recording.md:192) to state that a failed initial header flush refuses the start and remains `Stopped`.

- The new shipped test is omitted from both the §8 failure-path mapping and the self-review mapping: `aFailedStartFlushRefusesTheStartAndLeavesNoLoadableFile`. Add it, and have it assert the required “may be incomplete” reason as well as the existing state/file distinction. [Test](tests/tst_sessionrecorder.cpp:760)

- The self-review still says `tst_sessionrecorder.cpp` has **20** methods, but it now has **21**. [Self-review](docs/reviews/2026-09-18-M9-implementation-selfreview.md:54)

- The 64 KiB gap is acceptable without a new test, but §8 still claims that the upload “keeps memory flat.” That is not measured evidence. Reword it to attribute the no-session-sized-state conclusion to static inspection/append-only design, matching the self-review’s gap 4. [SPEC §8](docs/specs/SPEC-M9-live-session-recording.md:362)

On the requested name audit: §8 now uses the three old flush names only as explicitly labelled planned names, not as shipped tests; I found no other design-time name presented there as shipped.

The initial-flush implementation and its test are otherwise correct: it refuses the start, closes the sink, remains `Stopped`, and retains a syntactically loadable accepted header because `flush()` failure is a durability failure, not truncation. The `quint64` narrowing is now an explicit, documented status-message bound; no further production issue there.

I could not independently rerun CTest: this read-only environment cannot create CTest’s `LastTest.log`, and its existing `build/` is stale (it lacks `tst_sessionrecorder`).