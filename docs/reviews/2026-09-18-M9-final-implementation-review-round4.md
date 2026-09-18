M9 may now be declared closed. No unresolved closing-review findings remain.

Verified at `6f78579`:

- SPEC §5.6 and the recorder contract correctly distinguish a failed start flush from a flush failure during `Live`.
- The test is consistently renamed, asserts the exact refusal reason, checks the sink closes, and preserves the valid-header/durability distinction.
- The worktree is clean and HEAD matches the stated commit.

I could not independently rerun `ctest`: this read-only environment prevents CTest from writing its log, and the available build directory is stale (it lacks the M9 test targets). That is an environment limitation, not a new M9 finding. No fifth review cycle is necessary.