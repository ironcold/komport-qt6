# Contributing to Komport-Qt6

Thank you for helping improve Komport-Qt6. Bug reports, focused feature
proposals, documentation improvements, and code contributions are welcome.

By participating, you agree to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Before You Start

- Search existing issues before opening a new one.
- Keep proposals aligned with the project's focus: a lightweight Qt6 serial
  workbench for unusual, legacy, industrial, and network-device workflows.
- Discuss broad UI changes, terminal-engine replacements, new dependencies,
  or substantial architectural work in an issue before implementation.
- Never include passwords, private device output, serial numbers, access
  tokens, or other sensitive data in reports or captures.

## Reporting Bugs

Use the bug-report issue form and include:

- Komport-Qt6 version or commit
- Linux distribution and Qt version
- Serial device type and relevant connection settings
- Clear reproduction steps and expected/actual behavior
- A minimal, sanitized byte sequence or log when terminal emulation is involved

For security vulnerabilities, follow [SECURITY.md](SECURITY.md) instead of
opening a public issue.

## Building and Testing

Install a C++17 compiler, CMake 3.16 or newer, and the Qt6 Widgets,
PrintSupport, SerialPort, and Test development packages. Then run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

## Pull Requests

- Base changes on the current default branch and keep each pull request focused.
- Follow the existing C++ and Qt style in the surrounding code.
- Preserve original attribution and licensing headers in inherited source files.
- Add or update regression tests for behavior changes and bug fixes.
- Update `README.md`, `TODO.md`, or architecture decisions when applicable.
- Explain how the change was tested. Hardware-dependent tests should name the
  device class and configuration without exposing sensitive information.

Contributions are accepted under the repository's GNU General Public License,
version 2 or later.
