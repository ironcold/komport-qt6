# QTermWidget bridge spike

Throwaway architecture-decision code for
[`ADR-001-terminal-engine-strategy.md`](../../docs/architecture-decisions/ADR-001-terminal-engine-strategy.md).
Not part of the shipped app or the normal build - only built with
`-DKOMPORT_BUILD_SPIKES=ON`.

## What it does

Bridges a `QSerialPort` to a `QTermWidget` started in "teletype" mode
(`startTerminalTeletype()` instead of `startShellProgram()`, i.e. no shell
child process). Both directions of the data flow `CLAUDE.md`'s
"Terminal-Engine-Leitlinie" requires are wired up, each through a stub hook
function that stands in for the real hex-monitor/charset-translation logic:

```text
QSerialPort -> [rx-hook] -> QTermWidget's internal pty (slave fd)
QTermWidget's Emulation -> sendData() signal -> [tx-hook] -> QSerialPort
```

## Building

```sh
cmake -B build-spike -DKOMPORT_BUILD_SPIKES=ON -DBUILD_TESTING=OFF
cmake --build build-spike --target qtermwidget-bridge-spike
```

Needs the `qtermwidget6` package (Qt6 build, not `qtermwidget5`) - on
openSUSE Tumbleweed: `zypper install qtermwidget-devel qtermwidget-data`.

## Running interactively (real display)

```sh
./build-spike/spike/qtermwidget-bridge/qtermwidget-bridge-spike /dev/ttyUSB0
```

Or against a fake device with no hardware attached, using two linked ptys:

```sh
socat -d -d pty,raw,echo=0,link=/tmp/komport-spike-a pty,raw,echo=0,link=/tmp/komport-spike-b &
./build-spike/spike/qtermwidget-bridge/qtermwidget-bridge-spike /tmp/komport-spike-a
# in another terminal, play the "remote device":
cat /tmp/komport-spike-b            # see what the spike window sends (TX)
echo 'hello from the device' > /tmp/komport-spike-b   # feed it fake RX data
```

This is the same `pty`-pair trick already used by several permanent
`tests/tst_*.cpp` regression tests in this repo (see `TODO-ARCHIVE.md`).

## Running headlessly (`--selftest`)

```sh
QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 \
  ./build-spike/spike/qtermwidget-bridge/qtermwidget-bridge-spike /tmp/komport-spike-a --selftest
```

(`QT_FORCE_STDERR_LOGGING=1` only matters under systemd, which routes
`qInfo()`/`qWarning()` to the journal instead of stderr by default - see
`TODO.md` section 0.1 nitpick.) Runs a scripted structural check (color
schemes, scrollback size round-trip, RX-to-display, TX-emission) and exits
0/1. Needs an external harness (or a human with the `socat` pair above) to
actually feed RX bytes into the other end while it runs - it doesn't spawn
`socat` itself.

## Findings (informing ADR-001)

Verified end-to-end against a real `socat`-created pty pair
(openSUSE Tumbleweed, `qtermwidget-devel` 2.4.0-1.4, Qt 6.2+):

- **RX bridging works and preserves the required hook point.** Bytes written
  to the "remote" end of the test pty went `QSerialPort::readyRead()` ->
  `[rx-hook]` (stand-in for hex-monitor/charset translation) ->
  `::write()` to `QTermWidget::getPtySlaveFd()` -> and showed up correctly
  in the widget's own screen image (verified via `selectedText()`). The
  insertion point CLAUDE.md requires (raw bytes fully available *before*
  the terminal backend ever sees them) exists cleanly with QTermWidget as
  backend, exactly as it does with the current hand-rolled emulation.
- **TX bridging works the same way in reverse.** Asking the widget to
  process text (`sendText()`, standing in for real keyboard input) produced
  a `sendData()` signal carrying the exact bytes, which reached `[tx-hook]`
  and then the real serial device end.
- **Color schemes, scrollback size are usable out of the box.** 14 color
  schemes ship with `qtermwidget-data` (`GreenOnBlack`, `Solarized`,
  `Nord`, ...) and are selectable via `setColorScheme()`;
  `setHistorySize()`/`historySize()` round-trip correctly. Milestone 5's
  color-scheme goal would come largely "for free" from adopting
  `QTermWidget` - the current hand-rolled emulation has neither
  configurable schemes nor a scrollback size independent from the
  `KomportScrollBuffer` implementation this project already has.
- **Copy/paste is inherited "for free".** `setSelectionStart/End()` +
  `selectedText()` and the built-in `copyClipboard()`/`pasteClipboard()`
  slots work without any extra code - contrast with `KomportView`'s
  several review-round history of selection/clipboard bugs (see `TODO.md`
  sections 0.4/0.7).
- **Gotcha: new-style `connect(sender, &Class::signal, ...)` fails against
  this distro's prebuilt `libqtermwidget6.so`** with a runtime
  `"signal not found"` warning, for `QTermWidget`'s own signals
  (`sendData`, `receivedData`) specifically - other Qt classes'
  signals (`QSerialPort::readyRead`) connect fine. The old-style
  `SIGNAL()/SLOT()` macro syntax works without issue and is what this
  spike uses for those two connections (see the comment in `main.cpp`).
  Root cause not fully diagnosed (suspected build/moc-version skew between
  however the distro package was built and the Qt6 version this project
  targets) - if `QTermWidget` is ever adopted for real, this needs to be
  either root-caused, or accepted as a permanent exception to
  `CLAUDE.md`'s "prefer new-style connect syntax" guidance for these two
  specific signals.
- **Could not be verified headlessly** (this sandbox has no real display):
  actual font rendering quality, cursor blink/shape visuals, and
  interactive mouse-drag selection. These would need a manual check with a
  real display before a final adoption decision.
- **Performance** was not rigorously benchmarked (no comparable
  high-throughput serial data source available in this environment) -
  the widget felt responsive for the small amount of test traffic used
  here, but this is not a real performance verdict either way.
