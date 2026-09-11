/***************************************************************************
                          tst_i18n.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    Regression test for Milestone 6 (TODO.md): internationalization.
    Loads the REAL, build-time-compiled German translation from its
    embedded Qt resource (":/translations/komport_de.qm" - komport_core's
    CMake target embeds it via qt6_add_resources(), and every test binary
    links komport_core, so this resource is genuinely present here too,
    not a synthetic stand-in) and verifies translated UI text actually
    appears where expected, and - the specific bug class a Codex review
    round found in this milestone - that ParityComboBox/FlowControlComboBox
    keep their internally compared/persisted value (Qt::UserRole data)
    fixed at the English identifier KomportSerial::applyPortSettings()
    expects, independent of which language is currently displayed.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "settingsdialog.h"

#include <QTest>
#include <QComboBox>
#include <QTranslator>
#include <QCoreApplication>

class TstI18n : public QObject
{
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();

  void germanTranslationLoadsFromEmbeddedResource();
  void parityComboBoxKeepsEnglishIdentifierAsDataWhenTranslated();
  void flowControlComboBoxKeepsEnglishIdentifierAsDataWhenTranslated();
  void colorSchemeNamesAreActuallyTranslated();
  void findDataStillLocatesItemsByIdentifierWhenTranslated();

private:
  QTranslator mTranslator;
  bool mTranslatorLoaded = false;
};

void TstI18n::initTestCase()
{
  // Same embedded resource path/basename/suffix main.cpp itself uses -
  // see its own QTranslator::load() call and CMakeLists.txt's
  // qt6_add_resources(komport_core "translations" PREFIX "/translations" ...).
  mTranslatorLoaded = mTranslator.load( QStringLiteral("komport_de"), QStringLiteral(":/translations") );
  QVERIFY2( mTranslatorLoaded, "the embedded komport_de.qm resource must be loadable - if this fails, the build's translation wiring itself is broken" );
  QCoreApplication::installTranslator( &mTranslator );
}

void TstI18n::cleanupTestCase()
{
  if ( mTranslatorLoaded ) QCoreApplication::removeTranslator( &mTranslator );
}

void TstI18n::germanTranslationLoadsFromEmbeddedResource()
{
  // Already proven by initTestCase() succeeding, but spelled out here too
  // as its own named, independently reportable test result.
  QVERIFY( mTranslatorLoaded );
}

void TstI18n::parityComboBoxKeepsEnglishIdentifierAsDataWhenTranslated()
{
  // Codex review finding (Milestone 6, High-severity-relevant): naively
  // wrapping "NONE"/"EVEN"/"ODD" in tr() without decoupling display from
  // stored value would have made the combo's *text* (and therefore
  // whatever reads it back) locale-dependent, silently breaking the
  // parity KomportSerial::applyPortSettings() actually applies, and
  // corrupting saved profiles' Parity value under a non-English locale.
  SettingsDialog dialog;
  QVERIFY( dialog.ParityComboBox != nullptr );
  QCOMPARE( dialog.ParityComboBox->count(), 3 );

  // the displayed text is genuinely translated...
  QCOMPARE( dialog.ParityComboBox->itemText(0), QStringLiteral("Keine") );
  QCOMPARE( dialog.ParityComboBox->itemText(1), QStringLiteral("Gerade") );
  QCOMPARE( dialog.ParityComboBox->itemText(2), QStringLiteral("Ungerade") );
  // ...but the stored/compared value never changes, regardless of language -
  // this is what KomportApp persists into strParity/QSettings and what
  // KomportSerial::applyPortSettings() string-compares against.
  QCOMPARE( dialog.ParityComboBox->itemData(0).toString(), QStringLiteral("NONE") );
  QCOMPARE( dialog.ParityComboBox->itemData(1).toString(), QStringLiteral("EVEN") );
  QCOMPARE( dialog.ParityComboBox->itemData(2).toString(), QStringLiteral("ODD") );
}

void TstI18n::flowControlComboBoxKeepsEnglishIdentifierAsDataWhenTranslated()
{
  SettingsDialog dialog;
  QVERIFY( dialog.FlowControlComboBox != nullptr );
  QCOMPARE( dialog.FlowControlComboBox->count(), 3 );

  // "XON/XOFF"/"RTS/CTS" are universal protocol abbreviations - never
  // translated, text and data are intentionally identical for those two.
  QCOMPARE( dialog.FlowControlComboBox->itemText(0), QStringLiteral("XON/XOFF") );
  QCOMPARE( dialog.FlowControlComboBox->itemData(0).toString(), QStringLiteral("XON/XOFF") );
  QCOMPARE( dialog.FlowControlComboBox->itemText(1), QStringLiteral("RTS/CTS") );
  QCOMPARE( dialog.FlowControlComboBox->itemData(1).toString(), QStringLiteral("RTS/CTS") );
  // "None" is the one genuinely translated entry here.
  QCOMPARE( dialog.FlowControlComboBox->itemText(2), QStringLiteral("Keine") );
  QCOMPARE( dialog.FlowControlComboBox->itemData(2).toString(), QStringLiteral("NONE") );
  // constructor default selection must still resolve correctly post-translation.
  QCOMPARE( dialog.FlowControlComboBox->currentData().toString(), QStringLiteral("NONE") );
}

void TstI18n::colorSchemeNamesAreActuallyTranslated()
{
  // Codex review finding: createAppearanceTab() calls tr(scheme.name) with
  // a runtime const char*, which lupdate cannot statically extract unless
  // the literal is separately marked (QT_TRANSLATE_NOOP("SettingsDialog", ...)
  // in the kColorSchemes table, komport/settingsdialog.cpp) with the exact
  // context tr(scheme.name) is actually called from - proves the whole
  // chain (extraction into the .ts, translation, and runtime lookup
  // finding it under the right context) genuinely works end-to-end, not
  // just that lupdate found the strings.
  SettingsDialog dialog;
  QVERIFY( dialog.ColorSchemeComboBox != nullptr );
  QStringList items;
  for ( int i = 0; i < dialog.ColorSchemeComboBox->count(); ++i ) items << dialog.ColorSchemeComboBox->itemText(i);
  QVERIFY2( items.contains(QStringLiteral("Grün auf Schwarz")), qPrintable(QStringLiteral("expected translated 'Green on Black', got: %1").arg(items.join(QStringLiteral(", ")))) );
  QVERIFY2( items.contains(QStringLiteral("Schwarz auf Hellgelb")), qPrintable(QStringLiteral("expected translated 'Black on Light Yellow', got: %1").arg(items.join(QStringLiteral(", ")))) );
  QVERIFY2( !items.contains(QStringLiteral("Green on Black")), "the untranslated English name must not still be showing" );
}

void TstI18n::findDataStillLocatesItemsByIdentifierWhenTranslated()
{
  // The exact operation KomportApp::slotShowPreferences() performs when
  // pre-filling the dialog from a loaded profile's strParity/strFlowControl -
  // must still work correctly (find the right row) even though the
  // displayed text no longer matches the identifier being searched for.
  SettingsDialog dialog;
  const int parityIdx = dialog.ParityComboBox->findData( QStringLiteral("EVEN") );
  QVERIFY( parityIdx >= 0 );
  QCOMPARE( dialog.ParityComboBox->itemText(parityIdx), QStringLiteral("Gerade") );

  const int flowIdx = dialog.FlowControlComboBox->findData( QStringLiteral("RTS/CTS") );
  QVERIFY( flowIdx >= 0 );
  QCOMPARE( dialog.FlowControlComboBox->itemText(flowIdx), QStringLiteral("RTS/CTS") );
}

QTEST_MAIN(TstI18n)
#include "tst_i18n.moc"
