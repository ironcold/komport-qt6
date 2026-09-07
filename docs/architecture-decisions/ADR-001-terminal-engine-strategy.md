# ADR-001: Terminal Engine Strategy

Status: Rejected (QTermWidget as a backend) - keep the built-in emulation
Date: 2026-09-04, decided 2026-09-07

## Context

Komport-Qt6 is not meant to be another generic shell terminal. Its reason to
exist is the serial-console corner cases where common tools are often close, but
not satisfying enough: unusual line endings, raw control bytes, industrial
devices, network hardware, old computers, session diagnosis, macros, profiles
and retro or industry character sets.

This creates tension around terminal emulation. Reusing an existing terminal
engine could improve VT220/xterm behavior, colors, cursor handling and
scrollback quality. At the same time, Komport-Qt6 needs strict control over the
serial byte stream before terminal interpretation, because features like the
hex monitor, session logging, line-ending handling and planned charset
translation live there.

## Decision

Keep the current `QSerialPort`-first architecture as the reference path for now.
Before deeply extending the built-in VT100/VT102 emulation toward VT220 or
xterm behavior, run a focused spike for `QTermWidget` as an optional terminal
backend.

The required data flow is:

```text
QSerialPort -> RX/TX diagnosis/logging -> charset translation -> terminal backend
terminal backend -> charset translation/line endings/macros -> QSerialPort
```

`QTermWidget` is the preferred reuse candidate because it is an embeddable Qt
terminal widget and avoids much of the KDE Frameworks weight. KDE `KonsolePart`
is useful as a comparison point, but not the preferred direction because it
pulls in heavier KF6/KParts/XmlGui dependencies and is designed primarily around
PTY-backed shell sessions rather than direct serial I/O.

## Consequences

- The built-in emulation remains the stable backend until a spike proves that
  reuse helps more than it constrains.
- Charset translation must remain byte-stream level functionality before
  terminal interpretation, regardless of backend.
- Hex monitor, session logging, profiles, macros and line-ending handling remain
  first-class product features, not afterthoughts behind a generic terminal.
- If `QTermWidget` makes the special serial workflows easier and more robust, it
  can become an optional or replacement backend.
- If `QTermWidget` complicates the serial-specific features, Komport-Qt6 should
  continue improving its own terminal emulation.

## Follow-Up

- Run a small `QTermWidget` prototype with RX/TX bridging, keyboard input,
  copy/paste, scrollback, color schemes and cursor behavior.
- Explicitly verify that CP437, Amiga and PETSCII-style translation can happen
  before terminal parsing.
- Keep `KonsolePart` documented as a rejected or non-preferred option unless a
  future requirement justifies KDE Frameworks dependencies.

## Spike Results (2026-09-07)

Ran the prototype from the Follow-Up section - see
`spike/qtermwidget-bridge/` (`README.md` there has the full write-up and
exact repro steps; only the summary relevant to the decision is repeated
here). Built against `qtermwidget-devel` 2.4.0-1.4 (openSUSE Tumbleweed,
Qt6 build of `QTermWidget`), verified end-to-end with a real `socat`-created
pty pair standing in for a serial device.

**Confirmed working, structurally verified (not just read from docs):**

- RX/TX bridging via `QTermWidget::startTerminalTeletype()` +
  `getPtySlaveFd()` + the `sendData()` signal, with the required
  diagnosis/charset-translation hook point sitting cleanly *before* the
  terminal backend on RX and *after* it on TX, exactly as
  `CLAUDE.md`'s data-flow diagram requires. Verified: bytes written to the
  simulated remote device end showed up correctly in the widget's screen
  image (`selectedText()`), and text sent through the widget's own
  emulation reached the simulated device end.
- 14 ready-made color schemes ship with `qtermwidget-data` and are
  selectable via `setColorScheme()` - directly relevant to Milestone 5.
- Scrollback size is configurable via `setHistorySize()`/`historySize()`.
- Copy/paste (`setSelectionStart/End()`, `selectedText()`,
  `copyClipboard()`/`pasteClipboard()`) works out of the box, in contrast
  to `KomportView`'s own selection/clipboard code, which has needed fixes
  across several of this project's review rounds (`TODO.md` 0.4/0.7).

**New caveat found by the spike, not previously known:**

- New-style `connect(sender, &Class::signal, ...)` fails at runtime
  (`"signal not found"`) specifically for `QTermWidget`'s own signals
  against this distro's prebuilt library; the old-style `SIGNAL()/SLOT()`
  macro syntax works. Root cause not fully diagnosed (suspected
  build/moc-version skew in how the distro package was built). Would need
  to be root-caused, or accepted as a permanent, documented exception to
  `CLAUDE.md`'s "prefer new-style connect syntax for ported code"
  guidance, if `QTermWidget` is adopted.

**Not verifiable in this environment (no real display available):**

- Actual font rendering quality, cursor blink/shape visuals, interactive
  mouse-drag selection, and any real performance comparison against the
  current hand-rolled emulation under sustained high-throughput serial
  traffic. A manual check with a real display is needed before a final
  adoption decision, if the direction below is pursued further.

**Final decision (2026-09-07): Rejected.** Keep the built-in `KomportEmulation`
as the terminal backend; do not adopt `QTermWidget`. The spike confirmed the
required RX/TX bridging and hook points are technically achievable, and that
color schemes, scrollback sizing and copy/paste would come largely "for
free" - but weighed against that:

- The built-in emulation is, after six independent adversarial review rounds
  (`TODO.md` sections 0.1-0.7), already converged and specifically shaped
  around this project's actual reason to exist (serial special cases: raw
  control bytes, unusual line endings, hex diagnosis, macros, profiles,
  future charset translation) - not a generic terminal need.
- Swapping the rendering/emulation backend under all of that is a large,
  risky change for benefits (nicer default color schemes, free copy/paste)
  that are reachable more cheaply by extending the existing architecture
  directly (Milestone 5 already plans an Appearance tab; copy/paste bugs
  found during review are fixed, not open).
- The spike surfaced an unresolved, undiagnosed integration gotcha (new-style
  `connect()` failing at runtime for `QTermWidget`'s own signals against the
  distro package) and left performance and real-display rendering quality
  unverified - open risk with no corresponding open need driving adoption.
- Milestone 4 (VT220/xterm colors, scroll regions, insert mode, VT220 ID)
  is scoped as direct, incremental extensions to `komportemulation.cpp`
  and does not require a backend change to implement.

The spike code stays in the repository (`spike/qtermwidget-bridge/`, built
only via `-DKOMPORT_BUILD_SPIKES=ON`) as a documented, working reference in
case this decision is revisited later - e.g. if a future requirement
specifically needs something the built-in emulation structurally can't do.
`KonsolePart` remains rejected/non-preferred as stated in the Decision
section above; nothing in the spike changed that assessment (it was never
prototyped, being the explicitly non-preferred comparison point).
