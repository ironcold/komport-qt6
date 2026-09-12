# Komport-Qt6

A lightweight, native serial port communicator and VT100/VT102 terminal emulator for the Linux desktop, built on **Qt6** and **QSerialPort**.

Komport-Qt6 exists for the awkward real-world serial-console cases where the usual tools are always almost right, but never quite satisfying: odd line endings, invisible control bytes, legacy character sets, industrial devices, old computers, network gear and ad-hoc diagnosis sessions. The goal is not to become a generic shell terminal. It is a deliberate special-purpose serial workbench with a practical retro-computing streak.

*Komport-Qt6* is a modernized, standalone continuation of the classic **"Komport"** application by Mike Sharkey (2003/2004). The original project was deeply tied to KDE 2/3 and Qt3 and has been abandoned for over 15 years.

This project strips away all legacy KDE Frameworks and Qt3/4/5 dependencies, migrating the application to **pure, native Qt6**. While the robust terminal emulation, character-grid rendering, and general lightweight structure are carried forward, the entire under-the-hood architecture has been rewritten for modern Linux systems.

---

## 🛠 Features

### Core Enhancements (Beyond the Original)
* **Modern Serial Backend:** Powered entirely by `QSerialPort` and `QSerialPortInfo`. Features automatic hardware device detection (no more guessing if it's `/dev/ttyUSB0` or `/dev/ttyS1`).
* **Hardware-Level Framing:** Proper application of data bits, stop bits, parity, and flow control (XON/XOFF, RTS/CTS) directly to the hardware—fixing long-standing bugs in the 2004 upstream codebase.
* **Device Profiles:** Save, load, and delete named connection profiles from a dropdown in the toolbar. Each profile bundles the full session: serial parameters, the line-ending choice, and the macro bar's quick commands—switching profiles cleanly disconnects, re-applies the new hardware settings, and reloads the macro bar in one step. Ships with ready-made presets for **Cisco**, **HP 1920** and **Aruba CX** consoles (baud rate/framing plus vendor-appropriate quick commands)—edit or delete them like any other profile.

### Admin & Diagnosis Power-Tools
* **Toggleable Hex Monitor:** View raw RX/TX streams in a toggleable split-screen panel featuring the classic `[Offset] [Hex-Bytes] [ASCII]` layout—ideal for debugging invisible control characters.
* **One-Click Session Logging:** A dedicated "Record" button in the toolbar to live-log the received terminal output to a timestamped text file.
* **Programmable Macro Bar:** A customizable row of quick-command buttons at the bottom of the window for firing off repetitive admin commands (e.g., `show running-config`, `wr mem`) with a single click.
* **Line-Ending Selection:** Quick-switch options for the Return key and macros to send `CR`, `LF`, or `CR+LF`, for compatibility with varied enterprise network switches and embedded devices.
* **Special-Case Serial Workbench:** The long-term niche is the stuff where generic terminals get annoying: raw byte inspection, profile-bound quirks, retro/industry character-set translation, device-specific macros, and repeatable logging.
* **Expanded Terminal Emulation:** Broader VT100/VT102 coverage, including full support for ANSI SGR colors (256-color and bright variants), scroll regions (DECSTBM), insert mode, multi-parameter cursor movement, insert/delete line/character, DECCKM/DECTCEM mode handling, and device status reports.
* **Retro/Industrial Character-Set Translation:** A Character Set dropdown in the Terminal settings tab (built in: **IBM CP437** and **PETSCII**) translates the raw byte stream between the serial device and the display, so box-drawing art, accented letters and other special glyphs a plain-ASCII/Latin-1 assumption would otherwise mangle come through correctly — in both directions, typed/pasted/macro text included. **Fully extensible without touching any code:** drop a plain-text `*.charset` file into the folder the Settings dialog's "Custom Charsets Folder..." button opens for you (`~/.config/Komport-Qt6/charsets/`, which ships a short, bilingual usage guide the moment it's created) to add your own — see below.
* **Internationalized UI:** the interface automatically follows your system language (Qt6 Linguist/`QLocale`). Ships with all 24 EU official languages plus Russian — 23 of the 24 fully translated (Irish is intentionally left untranslated for now; see `TODO.md`). No system-language match, or an incomplete translation? It falls back cleanly to the English original, string by string.

> Not yet implemented: VT52 mode and DECOM/origin mode. See `TODO.md` for the current, honest list of open items and known gaps.

---

## 🔤 Adding Your Own Character Set

Komport-Qt6 comes with **IBM CP437** and **PETSCII** built in, selectable from the Terminal tab of the Settings dialog. You can add more of your own — no compiling, no code, just a text file.

1. Click **"Custom Charsets Folder..."** in the Settings dialog (Terminal tab), or open `~/.config/Komport-Qt6/charsets/` yourself. A `README.txt` with the same instructions as below (in English and German) is already waiting for you there.
2. Drop in a plain-text file named `something.charset`. Each line maps one byte to the Unicode character it should display as:
   ```
   # Name: My Retro Charset
   DB=2588
   41=03B1
   ```
   - The file must be plain UTF-8 text (plain ASCII, as in the example above, is valid UTF-8 too) — not UTF-16/UTF-32.
   - The optional `# Name: ...` line sets what shows up in the dropdown; without it, the filename itself is used.
   - Every byte you *don't* list keeps its default meaning (byte value == the same Unicode code point) — this is exactly what keeps control codes (Ctrl characters, Escape, ...) working normally without you having to think about terminal protocol details at all.
   - The reverse direction (what gets sent back over the wire when you type or paste a character shown by your custom table) is worked out automatically from the very same list — there's nothing extra to write for that.
3. Re-open the Settings dialog (or just restart Komport-Qt6) and your new charset appears in the dropdown, right alongside CP437 and PETSCII.

That's the whole mechanism — anyone can extend the character-set list on their own, for whatever oddball encoding their hardware happens to speak.

**Ready-made retro-computer examples:** [`docs/example-charsets/`](docs/example-charsets/) ships verified `*.charset` files for Atari ST, Atari 8-bit (ATASCII), Sinclair ZX Spectrum, Amstrad CPC, Acorn Archimedes/RISC OS, and MSX (International) — just copy the one you want into your custom-charsets folder. Its own README also explains which well-known retro platforms (Amiga, Apple II MouseText, BBC Micro, TRS-80) were deliberately left out, and why.

---

## 📦 Installation (Ubuntu / Debian-based)

To build *Komport-Qt6* from source, open a terminal and follow these steps:

### 1. Install Dependencies
```bash
sudo apt update
sudo apt install -y cmake g++ qt6-base-dev qt6-serialport-dev qt6-tools-dev qt6-l10n-tools
```

### 2. Clone and Build
```bash
git clone <this-repository-url> komport-qt6
cd komport-qt6
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 3. Run
You can launch the compiled application directly from the build folder:
```bash
./build/komport-qt6
```

*Note: To access serial ports (`/dev/ttyUSB*` or `/dev/ttyS*`) without root permissions, make sure your user is part of the `dialout` group:*
```bash
sudo usermod -aG dialout $USER
# Log out and log back in for changes to take effect
```

See `INSTALL` for the plain-text version of these steps, including `cmake --install`.

---

## 🗺 Project Architecture & History

For those interested in the dirty details of the migration, the repository includes:
* **`CLAUDE.md`** — the architecture blueprint, class mappings (KDE3 → Qt6), and design philosophy.
* **`TODO.md`** — currently open items and known gaps.
* **`TODO-ARCHIVE.md`** — the full, detailed migration/feature/code-review log (what changed, when, and why).

---

## 📜 License & Authors

This project is licensed under the **GNU General Public License v2 (or later)** - see the `COPYING` file for details.

* **Original Author:** Mike Sharkey `<michael@sharkey.servebeer.com>` (2003-2004)
* **Qt6 Port & Feature Engineering:** Harald Stürmer `<ironcold@ironcold.de>` (2026), with *Claude (Anthropic)* serving as an AI pair-programmer.
