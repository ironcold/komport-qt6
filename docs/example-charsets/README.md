# Example retro-computer character sets / Beispiel-Retrocomputer-Zeichensätze

**EN:** This folder holds ready-made `*.charset` files for several retro
computers, built on top of Komport-Qt6's user-extensible charset
mechanism (see the Settings dialog's "Custom Charsets Folder..." button,
or `TODO.md` section 1 for the full file format). These are **not**
compiled into the application — they're plain data files for you to
copy into your own custom-charsets folder (`~/.config/Komport-Qt6/
charsets/`) if you want them.

**DE:** Dieser Ordner enthält fertige `*.charset`-Dateien für mehrere
Retrocomputer, aufbauend auf Komport-Qt6s erweiterbarem Zeichensatz-
Mechanismus (siehe den "Custom Charsets Folder..."-Button im
Settings-Dialog, oder `TODO.md` Abschnitt 1 für das vollständige
Dateiformat). Diese sind **nicht** in die Anwendung eingebaut — es
sind reine Datendateien zum Kopieren in den eigenen
Custom-Charsets-Ordner (`~/.config/Komport-Qt6/charsets/`), falls
gewünscht.

## How to use them / Verwendung

```
cp docs/example-charsets/atari_st.charset ~/.config/Komport-Qt6/charsets/
```

Then re-open the Settings dialog (Terminal tab) or restart Komport-Qt6
— the new entry appears in the Character Set dropdown.

Dann den Settings-Dialog (Terminal-Tab) neu öffnen oder Komport-Qt6 neu
starten — der neue Eintrag erscheint im Zeichensatz-Dropdown.

## What's here / Was hier liegt

| File | Platform |
|---|---|
| `atari_st.charset` | Atari ST / STE / TT / Falcon |
| `atascii.charset` | Atari 8-bit (400/800/XL/XE) |
| `zx_spectrum.charset` | Sinclair ZX Spectrum |
| `amstrad_cpc.charset` | Amstrad CPC (464/664/6128/...) |
| `acorn_risc_os.charset` | Acorn Archimedes / RISC OS |
| `msx_international.charset` | MSX (International/European variant) |

Every file's header comment names its source and explains exactly which
bytes couldn't be shown as their real character and why (a format
limitation, or a genuinely ambiguous/undocumented source value) —
**none of these tables were guessed**, following the same "research it
properly, or leave it out" rule used for this project's UI
translations. Bytes with a real, documented glyph this format simply
cannot encode (see the astral-code-point note below) are explicitly
mapped to `U+FFFD` (the standard Unicode "replacement character")
rather than left unlisted — an unlisted byte falls back to plain
identity, which would display as a plausible-looking but simply wrong
Latin-1 letter instead of visibly signalling "something real belongs
here". You'll see this file format's own duplicate-mapping warning when
loading one of these files as a result (several bytes intentionally
share the `U+FFFD` target) — expected and harmless. The one exception:
bytes deliberately left as plain identity because a *different* real
serial device might genuinely mean a standard control code by that same
byte value (documented per-file, same reasoning as the built-in CP437
charset's own scope reduction, see `TODO.md` section 6.3) — for those,
identity is the intentionally safe choice, and `U+FFFD` would be
exactly as wrong as the original glyph would have been.

Jede Datei benennt in ihrem Kopfkommentar die Quelle und erklärt genau,
welche Bytes nicht als ihr echtes Zeichen dargestellt werden können und
warum (eine Formatgrenze, oder ein wirklich unklarer/unbelegter
Quellwert) — **keine dieser Tabellen wurde geraten**, nach derselben
"richtig recherchieren, sonst auslassen"-Regel wie bei den
UI-Übersetzungen dieses Projekts. Bytes mit einem echten, dokumentierten
Zeichen, das dieses Format schlicht nicht kodieren kann (siehe den
Astral-Codepoint-Hinweis unten), werden explizit auf `U+FFFD` (das
Standard-Unicode-"Replacement Character") gemappt statt unaufgeführt zu
bleiben — ein unaufgeführtes Byte fällt auf reine Identität zurück, was
als plausibel aussehender, aber schlicht falscher Latin-1-Buchstabe
angezeigt würde, statt sichtbar zu signalisieren "hier gehört etwas
Echtes hin". Beim Laden einer dieser Dateien erscheint deshalb die
eigene Doppel-Mapping-Warnung dieses Dateiformats (mehrere Bytes teilen
sich absichtlich das Ziel `U+FFFD`) — erwartet und harmlos. Die eine
Ausnahme: Bytes, die bewusst als reine Identität stehen bleiben, weil
ein *anderes* reales seriell angeschlossenes Gerät mit demselben
Bytewert tatsächlich einen Standard-Steuercode meinen könnte
(pro Datei dokumentiert, dieselbe Begründung wie die Scope-Reduktion
des eingebauten CP437-Zeichensatzes selbst, siehe `TODO.md` Abschnitt
6.3) — dort ist Identität die absichtlich sichere Wahl, und `U+FFFD`
wäre dort genauso falsch wie das ursprüngliche Zeichen gewesen wäre.

## Platforms deliberately NOT included / bewusst nicht enthaltene Plattformen

**EN:**
- **Commodore Amiga:** its default character set is ISO-8859-1/Latin-1
  byte-for-byte — the only "difference" (byte 0x7F's DEL glyph drawn as
  diagonal stripes instead of blank) is purely a font-rendering choice
  on real Amiga hardware, not a different byte-to-character mapping —
  nothing for a charset-translation table to express. Already covered
  entirely by the built-in "Standard" charset.
- **Apple II / IIc / IIgs (MouseText):** on real hardware this is a
  mode *toggle* that *replaces* the `@`, `A`-`Z`, `[`, `\`, `]`, `^`,
  `_` byte range with icons, not an additive extension like CP437 —
  using it here would make ordinary uppercase letters permanently
  unreadable while selected.
- **BBC Micro:** its default high-byte range is user-definable by
  design (no fixed mapping exists at all); its one genuinely fixed
  extended set, Teletext/MODE 7, encodes color/flash/double-height as
  inline control bytes mixed into the text stream — a stateful
  attribute system, not a simple byte-to-character table.
- **TRS-80 (Model I/III/4):** its printable range is plain ASCII with
  no substitutions; its distinctive "squot" block graphics (bytes
  0x80-0xBF) are entirely encoded in Unicode's newer astral "Symbols
  for Legacy Computing" block, which this file format cannot represent
  at all (see below); bytes 0xC0-0xFF are a space run-length-encoding
  scheme, not characters.

**DE:**
- **Commodore Amiga:** Der Standard-Zeichensatz entspricht Byte für
  Byte ISO-8859-1/Latin-1 — der einzige "Unterschied" (Byte 0x7F wird
  auf echter Amiga-Hardware als Diagonalstreifen statt leer
  dargestellt) ist eine reine Schriftart-Entscheidung, kein
  unterschiedliches Byte-zu-Zeichen-Mapping — für eine
  Zeichensatz-Übersetzungstabelle gibt es hier nichts abzubilden.
  Bereits vollständig durch den eingebauten Zeichensatz "Standard"
  abgedeckt.
- **Apple II / IIc / IIgs (MouseText):** Auf echter Hardware ein
  Moduswechsel, der den Bytebereich `@`, `A`-`Z`, `[`, `\`, `]`, `^`,
  `_` durch Icons *ersetzt*, keine additive Erweiterung wie CP437 —
  würde normale Großbuchstaben dauerhaft unlesbar machen, solange
  aktiv.
- **BBC Micro:** Der Standard-Oberbyte-Bereich ist konstruktionsbedingt
  frei belegbar (kein fester Mapping existiert); der einzige wirklich
  feste erweiterte Zeichensatz, Teletext/MODE 7, kodiert Farbe/
  Blinken/doppelte Höhe als inline-Steuerbytes im Textstrom selbst —
  ein zustandsbehaftetes Attributsystem, keine einfache
  Byte-zu-Zeichen-Tabelle.
- **TRS-80 (Model I/III/4):** Der druckbare Bereich ist reines ASCII
  ohne Abweichungen; die charakteristischen "Squot"-Blockgrafiken
  (Bytes 0x80-0xBF) liegen komplett im neueren astralen
  Unicode-Block "Symbols for Legacy Computing", den dieses
  Dateiformat gar nicht darstellen kann (siehe unten); Bytes
  0xC0-0xFF sind ein Leerzeichen-Lauflängen-Kodierungsschema, keine
  Zeichen.

## A note on "astral" Unicode code points / Hinweis zu "astralen" Unicode-Codepoints

**EN:** Several real, well-documented glyphs on some of these
platforms (especially Amstrad CPC and MSX) fall into Unicode's newer
"Symbols for Legacy Computing" block (U+1FB00 and above — added in
Unicode 13.0+). The `*.charset` file format only supports code points
up to U+FFFF (see `komportcharset.cpp`'s `loadCustomCharsetFile()`) —
those specific bytes are mapped to `U+FFFD` instead (see above) and
documented as such in each file's header, not silently dropped.

**DE:** Mehrere echte, gut dokumentierte Zeichen auf einigen dieser
Plattformen (besonders Amstrad CPC und MSX) liegen im neueren
Unicode-Block "Symbols for Legacy Computing" (U+1FB00 und höher — seit
Unicode 13.0). Das `*.charset`-Dateiformat unterstützt nur Codepoints
bis U+FFFF (siehe `loadCustomCharsetFile()` in `komportcharset.cpp`) —
genau diese Bytes werden stattdessen auf `U+FFFD` gemappt (siehe oben)
und im Kopfkommentar jeder Datei dokumentiert, nicht stillschweigend
fallen gelassen.
