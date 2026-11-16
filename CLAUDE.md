# Komport-Qt6 — Modernization KDE3/Qt3 → Qt6

> **Project renamed:** The original project "Komport" (Mike Sharkey, KDE 2/3, last active in 2003) is no longer maintained upstream. This Qt6 fork continues independently as **Komport-Qt6** — directory (`~/Entwicklung/komport-qt6`), CMake project/binary name (`komport-qt6`), `.desktop` display name ("Komport Qt6") and version numbering (newly starting at `1.0.0`, instead of the inherited KDE3 `0.4.6`) have been adjusted accordingly. Older entries in `TODO-ARCHIVE.md` still refer to `./build/komport` — this was the actual binary name at the respective time and will not be rewritten retrospectively as a historical record. The source files in `komport/` retain their filenames and the file header title "Komport Serial Port Communicator" as a reference to their origin.

> **Status:** The porting has been completed and builds cleanly with CMake/Qt6 (`cmake -B build && cmake --build build`, also without warnings using `-Wall -Wextra`). Additionally, four admin tool features were added: a toggleable hex monitor (RX/TX, split-screen), a significantly more complete VT100/VT102 emulation (cursor counter, insert/delete line/char, DECCKM/DECTCEM, device status reports, extended SGR colors — plus two crash bugs found and fixed via code review during cursor clamping), programmable macro buttons, one-click session logging, and a line-ending selection (CR/LF/CRLF) for the Enter key and macros. What was implemented/decided is detailed in `TODO-ARCHIVE.md`; open points/known gaps are in `TODO.md`. This document remains as a target/architecture reference for future changes.

## What is Komport?

A serial terminal program (VT100/VT102-like emulation) from 2003, originally written for KDE 2/3 (Mike Sharkey). Author of the KDE porting: `komport/` contains Doc/View/App structure in classic KDevelop-1.2 style (KMainWindow + KAction menus + session management). Build system is pure Autotools (`configure.in`, `Makefile.am`, `admin/`) — no CMake, no `.pro` file.

Core functions that must be retained:
- Open/configure serial connection (device, baud rate, data bits, stop bits, parity, flow control) and terminal I/O in real time.
- Character grid display (`KomportCell`/`KomportCellArray`) with scrollback (`KomportScrollBuffer`, optionally file-based via `KomportFileScrollBuffer`).
- VT100/VT102 emulation (`komportemulation.cpp`, ~910 lines — by far the largest and most valuable part of the code; the escape sequence logic itself remains substantively unchanged, only the surrounding Qt/KDE dependencies are replaced).
- File transfer framework (`KomportTransfer`/`KomportUpload`/`KomportDownload`, `KomportScript`, `KomportQueue`) — currently rudimentary, not to be functionally extended during porting, only kept runnable.
- Settings dialog (`settingsdialog.ui`/`.cpp`/`.h`) for connection parameters.

## Product Vision / Reason for Existence

Komport-Qt6 is consciously not intended to become another generic terminal for normal shell work. Its reason for existence lies in special cases where one often tries several half-fitting tools today and still ends up dissatisfied: serial consoles of network and industrial devices, historical computers, unusual line endings, raw control characters, diagnostic captures, macros, device profiles, and charset or graphics character peculiarities.

The retro idea is not an end in itself but a good product anchor: a modern Qt6 tool that takes old and stubborn serial worlds seriously. Comfort features may be reminiscent of KDE Konsole, Kate, or classic terminal programs, but must always serve the serial special case. Reuse is welcome as long as it does not obscure these goals or pull the project into unnecessary framework dependencies.

## Terminal Engine Guideline

The proprietary `QSerialPort` pipeline remains the reference architecture for now, because hex monitor, logging, macros, profiles, line endings, and the planned charset translation attach directly to the raw byte stream. A foreign terminal engine may only sit behind a clear bridge:

`QSerialPort -> RX/TX-Diagnosis/Logging -> charset translation -> Terminal-Backend`

and in the opposite direction:

`Terminal-Backend -> charset translation/line ending/macros -> QSerialPort`

KDE `KonsolePart` is technically interesting, but likely too heavy for this project in the long run: KF6/KParts/XmlGui dependencies, PTY/shell focus, and additional bridge complexity fit poorly with a lean, pure Qt6 serial tool. `QTermWidget` is the more sensible candidate for a spike because it can be embedded as a Qt widget and brings less KDE ballast. Nonetheless: if special features such as charset tables, hex view, profiles, or raw data control are degraded by it, the proprietary emulation remains the better choice.

## Session/Replay Platform (after v1.0.0, not yet implemented)

The product-wide extension to lossless session recording, offline analysis, passive replay, decoders, simulation, and later network transport is defined in these documents:

- `docs/komport-decoder-session-concept-evolution.md` — derivation and product justification.
- `docs/komport-session-replay-simulation-architecture.md` — target image and recommended sequence.
- `docs/komport-engineering-governance-spec-review-workflow.md` — mandatory: reviewed spec before production code.
- `docs/architecture-decisions/ADR-002` to `ADR-009` — proposed foundation contracts for event, transport, direction, time, file format and replay security as well as source time and product boundaries.
- `docs/specs/SPEC-M8-session-transport-foundation.md` — first, consciously narrow implementation cut.
- `docs/komport-multi-executable-product-architecture.md` — target image for lean terminal, later analyzer and headless agent on shared libraries.

The multi-port sniffer is described as a separate later backlog in `docs/komport-multiport-sniffer-time-alignment.md`. Therefore, the session foundation must already separate source identity from local TX/RX direction and preserve the observed source time alongside a derived session time. M8 nonetheless implements only one local source; multi-source capture, sniffer views, and clock alignment remain explicitly later, specified milestones (ADR-008).

Current `komport-qt6` remains the terminal. `komport-analyzer` and `komport-agent` are not M8 artifacts and do not trigger an immediate restructuring. New session/transport contracts will however be kept widget- and frontend-neutral so that a later, demand-driven library extraction without dependency of one program on the internals of another remains possible (ADR-009).

The previous single-character signals and the text logger are not a lossless session architecture. New recorder/decoder/replay code must not use them as a source. Before implementation of M8, the ADRs and the spec must be independently reviewed and marked as accepted.

## Goal of this Migration

1. **Remove KDE classes, introduce standard Qt6 classes.** It should end up being a pure Qt6 program, no longer with KDE Frameworks/KF6 dependency.
2. **Switch serial backend to `QSerialPort` + `QSerialPortInfo`** (Qt SerialPort module), as a replacement for the current raw POSIX/`termios` implementation in `komportserial.cpp`.
3. Retain functional scope and operation as far as sensible — this is a **technology migration, not a rewrite of functionality**. UI may be modernized as part of this (e.g., Qt Designer `.ui` files instead of hand-written widget layout), but emulation logic/behavior should not change.

## Class Mapping (KDE/Qt3 → Qt6)

| Old (KDE/Qt3) | New (Qt6) | Affected Files |
| --- | --- | --- |
| `KApplication`, `kapp.h` | `QApplication` | `main.cpp`, `komport.h/.cpp` |
| `KCmdLineArgs`, `KCmdLineOptions` | `QCommandLineParser`/`QCommandLineOption` | `main.cpp` |
| `KAboutData`, `I18N_NOOP` | Direct in `QApplication` setup / omit if possible; `QCoreApplication::translate` for i18n | `main.cpp` |
| `KMainWindow` | `QMainWindow` | `komport.h/.cpp` |
| `KAction`, `KToggleAction`, `KActionCollection`, `KStdAction` | `QAction`, `QAction::setCheckable(true)` | `komport.h/.cpp` |
| `KRecentFilesAction` | proprietary recent-files logic via `QAction` list + `QSettings` (no direct Qt6 equivalent) | `komport.h/.cpp` |
| `KMenuBar` | `QMenuBar` (already included in `QMainWindow`) | `komport.cpp` |
| `KStatusBar` | `QStatusBar` (`QMainWindow::statusBar()`) | `komport.cpp` |
| `KConfig` | `QSettings` | `komport.h/.cpp` |
| `KMessageBox` | `QMessageBox` | `komport.cpp`, `komportdoc.cpp` |
| `KIconLoader`, KDE icon theme | `QIcon` (resource/theme icons via `QIcon::fromTheme`) | `komport.cpp` |
| `KProgress` | `QProgressBar`/`QProgressDialog` | Transfer dialogs |
| `KURL` | `QUrl` | `komportdoc.h/.cpp`, `komport.h/.cpp`, `komporttransfer.h/.cpp`, `settingsdialog.h/.cpp` |
| `KURLRequester` | `QLineEdit` + `QToolButton` with `QFileDialog::getOpenFileName`, or proprietary composite widget | `settingsdialog.ui/.h/.cpp` |
| `kfiledialog.h` (`KFileDialog`) | `QFileDialog` | `komport.cpp` |
| `komportui.rc` (KDE XML-UI framework) | build menus/toolbars programmatically in `QMainWindow` or via Qt Designer `.ui` | `komport.cpp`, `komportui.rc` omitted |
| `KAccel` | `QShortcut` / `QAction::setShortcut` | `komport.h/.cpp` |

## Class Mapping (Qt3 → Qt6, independent of KDE)

| Old (Qt3) | New (Qt6) | Note |
| --- | --- | --- |
| `QCString` | `QByteArray` | esp. `komportserial.h/.cpp` (device name), `komportemulation.h` |
| `QPtrList<T>` | `QList<T>` | `komportdoc.h/.cpp`, `komportcellarray.h` |
| `QSocketNotifier` for serial data | omitted — `QSerialPort::readyRead()` handles this | `komportserial.cpp` |
| `local8Bit()`/`QString::local8Bit()` | `toLocal8Bit()` | wherever used |
| Old `SIGNAL()/SLOT()` macro | continues to work in Qt6, but new function-pointer syntax (`connect(a, &A::sig, b, &B::slot)`) preferred for newly written/ported code | esp. `komport.cpp`, `komportserial.cpp` |
| `WFlags` | `Qt::WindowFlags` | `settingsdialog.h` |
| `QT_VERSION` constructor signature `(parent, name)` | Qt6 constructor `(parent, Qt::WindowFlags)`, `name` via `setObjectName()` | everywhere |

## Serial Backend — Core of the Migration

**Done:** `komportserial.h/.cpp` is completely switched to `QSerialPort` (no raw POSIX/`termios`, no `QSocketNotifier`, `KomportQueue` omitted) — details on original state and conversion in `TODO-ARCHIVE.md` section 2.

## Architecture Overview

- `KomportApp` (`komport.h/.cpp`) — main window, menus/toolbar/statusbar, file management, also wires the newer panels/bars at the bottom (hex monitor, macro bar, recording, see below). Central widget is a `QSplitter` with `KomportView` and `KomportHexView`. Also carries **device profile management** (`initProfiles()`/`loadProfile()`/`saveProfile()`, toolbar `profileCombo`): a profile bundles serial parameters, line ending and the macro bar assignment under `QSettings` group `Profiles/<Name>`; `readOptions()`/`saveOptions()` only handle window chrome (geometry, bar visibility, last used files), no longer connection settings. Details in `TODO-ARCHIVE.md` section 11.2.
- `KomportDoc` (`komportdoc.h/.cpp`) — holds `KomportSerial` instance, document-view pattern (from KDevelop boilerplate; actually too heavy for a terminal, but structurally adopted instead of redesigned).
- `KomportView` (`komportview.h/.cpp`) — character grid widget, drawing, mouse/keyboard events, selection/clipboard, holds `KomportCellArray`, `KomportScrollBuffer`, `KomportEmulation`. **Attention:** sits in the central `QSplitter`, not directly under `KomportApp` — `getDocument()` therefore runs via `window()`, not `parentWidget()` (see `TODO-ARCHIVE.md` 8.6). Scrollbar is a real child widget (`KomportMinimapScrollBar`, see below), no longer a manually positioned sibling — see `TODO-ARCHIVE.md` section 15 for the bug history.
- `KomportMinimapScrollBar` (`komportminimap.h/.cpp`, **new**) — Kate-like minimap instead of a normal `QScrollBar`: text density silhouette of entire history (scrollback + live), hover text preview, `QPalette`-based (automatically follows system theme). API compatible with old `QScrollBar` (`value`/`setValue`/`maximum`/`setMaximum`/`valueChanged`), therefore integrated as drop-in in `KomportView`.
- `KomportEmulation` (`komportemulation.h/.cpp`) — VT100/VT102 escape sequence interpreter. **Largest and most important module.** Meanwhile quite complete (cursor movement with counter, insert/delete line/char, DECCKM/DECTCEM, device status reports, extended SGR colors incl. 256-colors, scroll regions/DECSTBM, tab, non-CSI escapes) — details in `TODO-ARCHIVE.md` section 8.2 and `TODO-ARCHIVE.md` section 20 (milestone 4), known gaps (DECOM/Origin mode, VT52) in `TODO.md` section 6. Also carries the `LineEnding` setting (CR/LF/CRLF) for the Enter key and the macro bar.
- `KomportCharset` (`komportcharset.h/.cpp`, **new**, milestone 7) — byte-based charset translation between raw serial byte stream and `KomportEmulation` (RX/TX, see `CLAUDE.md`'s data flow specification above). Built-in: Standard (identity), IBM CP437 (0x80-0xFF), PETSCII (ASCII-compatible range + £/↑/←). Additionally, `reloadCustomCharsets()` loads any number of custom `*.charset` files from `customCharsetsDirectory()` (`~/.config/Komport-Qt6/charsets/`) — new charsets can thus be added without code change/rebuild, see `TODO.md` section 1 for the file format.
- `KomportCell`/`KomportCellArray` — character cell model of visible screen; `KomportCellArray` since feature extension also carries the DECTCEM visibility flag for the cursor (`cursorVisible()`/`setCursorVisible()`/signal `cursorVisibilityChanged`).
- `KomportScrollBuffer`/`KomportFileScrollBuffer` — scrollback (memory or file).
- `KomportSerial` (`komportserial.h/.cpp`) — switched to `QSerialPort` (see above); in addition to `receivedChar(char)` now also carries `sentChar(char)` (for the hex monitor).
- `KomportTransfer`/`KomportUpload`/`KomportDownload`/`KomportScript` — file transfer framework.
- `SettingsDialog` (`settingsdialog.h/.cpp`) — connection settings, hand-written with Qt6 layouts (the old Qt3 `.ui` is removed, Qt6 `uic` cannot read it).
- `KomportHexView` (`komporthexview.h/.cpp`, **new**) — toggleable split-screen hexdump panel (RX/TX separate, 16 bytes/line, raw before any emulation interpretation), with two RX/TX filter checkboxes in the panel header, whose state is persisted per profile.
- `KomportMacroBar` (`komportmacrobar.h/.cpp`, **new**) — 8 programmable quick-command buttons, docked at bottom, editable via click/right-click, persisted.
- `KomportSessionLogger` (`komportsessionlogger.h/.cpp`, **new**) — one-click capture of received byte stream into a timestamped text file.

## Build System

**Done:** the old Autotools setup (`configure.in`, `acinclude.m4`, `admin/*`, KDevelop-1.2 generated, KDE-2/3 typical) was completely replaced by a top-level `CMakeLists.txt` (`find_package(Qt6 COMPONENTS Widgets PrintSupport SerialPort REQUIRED)`, `CMAKE_AUTOMOC`/`CMAKE_AUTORCC`, C++17). All Autotools files and KDevelop-1.x project files are removed (see `TODO-ARCHIVE.md` section 3 for the full list). Build: `cmake -B build && cmake --build build`. The main code resides in an object library `komport_core` (everything except `main.cpp`), against which both the `komport-qt6` executable and the `Qt6::Test`-based regression tests in `tests/` link (`ctest` in build directory after build; `BUILD_TESTING` via `include(CTest)`, default on). See `TODO.md` section 0.1 for the reason.

## What is NOT in Scope (unless otherwise requested by user)

- No uncontrolled switch to a foreign terminal engine. Terminal engine work must be decided as separate spike/ADR; `QTermWidget` is the preferred candidate for reuse, `KonsolePart` only as a consciously documented counter-sample. Until then, the proprietary VT100/VT102-near emulation remains the stable path.
  **Explicitly requested exception by user:** the bright ANSI color codes `90–97`/`100–107` (`komportemulation.cpp`, `doGraphics()`) are of aixterm/xterm origin, not part of original VT102 documentation in the header comment of this file — they were consciously included because the user explicitly required "correct handling of color codes (ANSI)" and real target devices (Cisco/Juniper CLIs, colored `ls` output etc.) send them routinely. Conscious, documented exception from VT100/VT102 restriction. **Update milestone 4 (see `TODO.md` section 0.8):** the 256-color protocol (`CSI 38;5;N`/`48;5;N`) was subsequently also consciously included (explicitly specified as milestone 4 goal, not just aixterm compatibility) — remains however the only further exception: no True-Color (`CSI 38;2;r;g;b` is parsed/consumed but not applied), no other xterm functional scope.
- No functional extension of file transfer (upload/download/script) beyond previous rudimentary framework.
- **Obsolete since milestone 6 (Internationalization):** this line originally estimated i18n as "not core goal" — meanwhile it is a full, implemented milestone (see `TODO.md` section 0, "Milestones (Roadmap)"): all visible string literals use `tr()`, Qt6 `LinguistTools` are integrated (`CMakeLists.txt`), a German translation (`komport/translations/komport_de.ts`) is compiled to `.qm` at build time and embedded via Qt resource system, `main.cpp` loads them automatically based on system language (`QLocale::system()`), including Qt's own base translations (standard dialog texts like OK/Cancel). The old KDE `I18N_NOOP`/.po chain (inherited from KDE3 porting) is completely replaced by the modern Qt6 .ts/.qm chain.

## Other

- License: GPL (see `COPYING`), original author Mike Sharkey — for renames/new files, maintain copyright header convention of existing files or supplement meaningfully (do not remove original author).
- `komport.desktop` remains usable; adjust paths/categories in the `.desktop` file for non-KDE environments if necessary. The app icon is `komport/komport-qt6.svg` (terminal screen + DB9 connector; replaces the old KDE3 PNGs `lo16`/`lo32-app-komport.png`), embedded via `komport.qrc` and installed to `share/icons/hicolor/scalable/apps`.
