**Commit decision: changes requested; do not commit round 3 yet.**

(a) **Close path — mostly sound, one documentation correction required.**  
Amending the accepted spec/ADR is the right instrument: this is a deliberate cross-module, user-visible reporting rule, not an implementation exception to hide in code. SPEC-M9 now accurately matches the code: a clean application close is silent; a damaged one emits an untranslated warning with path, records, accepted bytes, duration, and reason ([SPEC §5.6–5.7](docs/specs/SPEC-M9-live-session-recording.md:197), [code](komport/komport.cpp:966)).

However ADR-010 §9 still says application close “reports through the log,” which reads as applying to clean closes too. The code logs only damaged reports. Make the ADR equally explicit about clean close being silent.

(b) **Assertions — improved, but not yet sufficient as semantic evidence.**  
The idle test now proves both refusal and its `live` reason. Good.

The clean-stop test proves the full message *shape*, but not that its reported count/bytes are correct:

- It hard-codes `1` in the regex, while the parsed file is only asserted to have `>= 1` records.
- The byte assertion is only `status->text().contains(fileSize)`, so it does not establish that the captured *byte-count field* equals the final file size.

Capture regex groups for record count and byte count, then compare them respectively to `recorded.records.size()` and `QFileInfo(path).size()`. The path and duration-format assertions are already adequate. A separate forced-damage GUI test remains unnecessary for this slice; the recorder’s seam tests own reproducible damage.

(c) **`%n` / `.arg()` — correct for normal-sized counts, but not fully correct for the declared type.**  
Your normal-case reasoning is right:

- no translator loaded: `%n` renders the English source form, including `record(s)`;
- German TS has singular and plural numerus forms;
- `tr(..., nullptr, n)` selects/replaces `%n` first, then `.arg()` replaces the lowest remaining numbered placeholder—here `%2`, then `%3`, etc. [Qt plural handling](https://doc.qt.io/qt-6/i18n-source-translation.html), [QString::arg semantics](https://doc.qt.io/qt-6/qstring.html#arg-2)

But `records` is `quint64`, while `tr()` accepts only `int`; `int(_report.records)` makes the displayed `%n` and plural selection wrong above `INT_MAX` ([implementation](komport/komport.cpp:1522), [report type](komport/sessionrecorder.h:80)). M9 specifies no count cap, so this must be resolved or bounded explicitly.

The German runtime-selection gap is small but worth closing. The existing German i18n test already loads the real embedded catalog ([tst_i18n.cpp](tests/tst_i18n.cpp:55)); add direct runtime assertions for both `n == 1` and `n == 2`, including the remaining `%2…%4` substitutions. The compiled-QM byte check is useful author evidence, but it does not prove runtime lookup and plural selection.