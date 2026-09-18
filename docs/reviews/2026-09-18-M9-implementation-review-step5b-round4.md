**Remaining numerus item: approved.** The new test uses the real embedded German QM, the exact `KomportApp` context and production source strings, supplies `%n` before chaining `.arg()`, and asserts singular, plural, damaged, and remaining-placeholder resolution. If it passes as reported, it closes the runtime-selection gap.

**Formatting claim: verified.** `tests/tst_i18n.cpp` is 28 insertions and 0 deletions; the diff contains only the slot declaration and new method. The final 44 bytes, including `QTEST_MAIN` and moc include, are byte-identical to `HEAD`.

**Commit decision: approve for the stated round-4 scope.** I did not execute the build or tests in this read-only environment; approval relies on the supplied warning-free build and 16/16 CTest result.