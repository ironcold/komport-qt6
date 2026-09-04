# Komport-Qt6

A lightweight, native serial port communicator and VT100/VT102 terminal emulator for the Linux desktop, built on **Qt6** and **QSerialPort**.

*Komport-Qt6* is a modernized, standalone continuation of the classic **"Komport"** application by Mike Sharkey (2003/2004). The original project was deeply tied to KDE 2/3 and Qt3 and has been abandoned for over 15 years.

This project strips away all legacy KDE Frameworks and Qt3/4/5 dependencies, migrating the application to **pure, native Qt6**. While the robust terminal emulation, character-grid rendering, and general lightweight structure are carried forward, the entire under-the-hood architecture has been rewritten for modern Linux systems.

---

## 🛠 Features

### Core Enhancements (Beyond the Original)
* **Modern Serial Backend:** Powered entirely by `QSerialPort` and `QSerialPortInfo`. Features automatic hardware device detection (no more guessing if it's `/dev/ttyUSB0` or `/dev/ttyS1`).
* **Hardware-Level Framing:** Proper application of data bits, stop bits, parity, and flow control (XON/XOFF, RTS/CTS) directly to the hardware—fixing long-standing bugs in the 2004 upstream codebase.

### Admin & Diagnosis Power-Tools
* **Toggleable Hex Monitor:** View raw RX/TX streams in a toggleable split-screen panel featuring the classic `[Offset] [Hex-Bytes] [ASCII]` layout—ideal for debugging invisible control characters.
* **One-Click Session Logging:** A dedicated "Record" button in the toolbar to live-log the received terminal output to a timestamped text file.
* **Programmable Macro Bar:** A customizable row of quick-command buttons at the bottom of the window for firing off repetitive admin commands (e.g., `show running-config`, `wr mem`) with a single click.
* **Line-Ending Selection:** Quick-switch options for the Return key and macros to send `CR`, `LF`, or `CR+LF`, for compatibility with varied enterprise network switches and embedded devices.
* **Expanded Terminal Emulation:** Broader VT100/VT102 coverage, including full support for ANSI SGR colors (including bright variants), multi-parameter cursor movement, insert/delete line/character, DECCKM/DECTCEM mode handling, and device status reports.

> Not yet implemented: per-connection settings profiles (save/switch between named device configs), VT100 scroll regions, and VT52 mode. See `TODO.md` for the current, honest list of open items and known gaps.

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
