# Komport-Qt6 — TODO

Aktive Liste offener Punkte. Die komplette, abgeschlossene Portierungs-/
Feature-/Review-Historie (Abschnitte 0–5, 8–10) ist nach **`TODO-ARCHIVE.md`**
ausgelagert, damit diese Datei übersichtlich bleibt — dort stehen alle Details
zu bereits erledigter Arbeit. Architektur/Ziele stehen in `CLAUDE.md`.

Stand: alle bisherigen Aufträge (Basis-Portierung, Admin-Tool-Features,
Code-Review-Fixes, Umbenennung) sind abgearbeitet — siehe `TODO-ARCHIVE.md`.
Unten stehen nur die bewusst offen gelassenen/verschobenen Punkte.

## 6. Bekannte, bewusst nicht behobene Altlasten (vom Original übernommen)

- `KomportDoc::openDocument/saveDocument` waren im Original bereits reine
  TODO-Stubs ohne echte Dateiverarbeitung — bleiben es auch nach der Portierung.
- `KomportFileScrollBuffer::cell()` liefert immer `NULL` (Datei-Scrollback war im
  Original nie fertig implementiert) — unverändert übernommen.
- VT100/VT102-Emulation: Scroll-Regionen (`DECSTBM`/`CSI r`) werden nur
  konsumiert, nicht angewendet; `ESC M` (Reverse Index) scrollt am oberen Rand
  nicht rückwärts; VT52-Modus, echte Zeichensatz-Umschaltung
  (Linien-Grafikzeichen) und Insert-Mode (`CSI 4h`) sind nicht implementiert.
  Details siehe `TODO-ARCHIVE.md` Abschnitt 8.2.

## 7. Wunschliste / mögliche nächste Schritte

Ursprünglich aus der alten `TODO`-Datei des Original-Autors (2003) übernommen,
seither ergänzt:

- VT100-Emulation weiter finalisieren, insbesondere die oben unter Abschnitt 6
  genannten Lücken (Scroll-Regionen wären der aufwändigste Brocken — bräuchte
  eine scroll-region-fähige `scrollUp()`/neue `scrollDown()` in
  `KomportCellArray`).
- Mögliche Erweiterungen der Profilverwaltung (Abschnitt 11 in
  `TODO-ARCHIVE.md`), falls gewünscht: Umbenennen bestehender Profile,
  Import/Export als Datei, ein "Profil wechseln" ohne den Kombinationsfeld-Text
  erst manuell zu tippen bei sehr vielen Profilen (z.B. Sortierung/Filter).
- Ideen aus der `README.md`, die (noch) nicht umgesetzt sind, bei Bedarf hier
  eintragen, bevor sie als Feature versprochen werden.
