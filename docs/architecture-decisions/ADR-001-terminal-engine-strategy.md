# ADR-001: Terminal Engine Strategy

Status: Proposed
Date: 2026-09-04

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
