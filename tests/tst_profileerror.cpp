/***************************************************************************
                          tst_profileerror.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    Regression test for KomportApp::loadProfile(): a serial error reported
    synchronously while applying a profile (e.g. the profile's device
    doesn't exist) used to be immediately overwritten by loadProfile()'s
    own unconditional trailing "Loaded profile ..." status message, so the
    user never actually saw it (see TODO.md's Codex-review section, second
    review round).
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komport.h"
#include "komportdoc.h"
#include "komportserial.h"

#include <QTest>
#include <QPointer>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QLabel>

/** Test seam: KomportApp::saveProfile() is protected, and the profile round trip of
  * a field is exactly what SPEC-M8 13 asks to keep unchanged. No production change
  * is needed for this - the access is widened for the test only. */
class TestableKomportApp : public KomportApp
{
public:
  using KomportApp::saveProfile;
};

class TstProfileError : public QObject
{
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();

  void serialErrorSurvivesLoadProfileStatusMessage();
  void startBitsSurvivesTheProfileRoundTrip();

private:
  QString mTestConfigFile;
};

void TstProfileError::initTestCase()
{
  // Same reasoning as tst_windowlifetime.cpp: keep this off the real
  // user's "Komport-Qt6" profiles entirely.
  QCoreApplication::setOrganizationName( QStringLiteral("Komport-Qt6-Test-ProfileError") );
  QCoreApplication::setApplicationName( QStringLiteral("Komport-Qt6-Test-ProfileError") );
  mTestConfigFile = QSettings().fileName();
}

void TstProfileError::cleanupTestCase()
{
  const QString dirPath = QFileInfo(mTestConfigFile).absolutePath();
  QFile::remove(mTestConfigFile);
  QDir().rmdir(dirPath);
}

void TstProfileError::serialErrorSurvivesLoadProfileStatusMessage()
{
  // Seed a profile whose device cannot possibly exist, so KomportSerial::open()
  // fails and reports the legacy settingsFailed() synchronously from inside
  // loadProfile()'s call to applyConnectionSettings().
  {
    QSettings settings;
    settings.beginGroup( QStringLiteral("Profiles/BadDeviceTest") );
    settings.setValue( QStringLiteral("Device"), QStringLiteral("/dev/definitely-does-not-exist-komport-test") );
    settings.setValue( QStringLiteral("BaudRate"), QStringLiteral("9600") );
    settings.setValue( QStringLiteral("DataBits"), QStringLiteral("8") );
    settings.setValue( QStringLiteral("StartBits"), QStringLiteral("1") );
    settings.setValue( QStringLiteral("StopBits"), QStringLiteral("1") );
    settings.setValue( QStringLiteral("Parity"), QStringLiteral("NONE") );
    settings.setValue( QStringLiteral("FlowControl"), QStringLiteral("NONE") );
    settings.setValue( QStringLiteral("RXQueue"), QStringLiteral("1024") );
    settings.setValue( QStringLiteral("FlushRate"), QStringLiteral("256") );
    settings.setValue( QStringLiteral("ScrollBuffer"), QStringLiteral("1024") );
    settings.setValue( QStringLiteral("LineEnding"), QStringLiteral("CR") );
    settings.endGroup();
  }

  KomportApp *win = new KomportApp();
  win->show();
  QPointer<KomportApp> guard(win);

  win->loadProfile( QStringLiteral("BadDeviceTest") );

  QLabel *statusLabel = win->findChild<QLabel *>( QStringLiteral("hoverHintLabel") );
  QVERIFY( statusLabel != nullptr );
  QVERIFY2( statusLabel->text().startsWith( QStringLiteral("Serial port error") ),
            qPrintable( QStringLiteral("expected the serial error to survive as the status "
                                       "message, but it reads: \"%1\"").arg(statusLabel->text()) ) );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

void TstProfileError::startBitsSurvivesTheProfileRoundTrip()
{
  // SPEC-M8 13 requires the compatibility field startBits to keep its unchanged
  // configuration behaviour: it is loaded from the profile, staged into the
  // configuration request and saved back unchanged, and never applied to
  // hardware. This covers the profile path end to end through the real
  // application; the dialog's own combo box is modal and stays covered by
  // inspection plus the transport-level test.
  {
    QSettings settings;
    settings.beginGroup( QStringLiteral("Profiles/StartBitsTest") );
    settings.setValue( QStringLiteral("Device"), QStringLiteral("/dev/definitely-does-not-exist-komport-test") );
    settings.setValue( QStringLiteral("BaudRate"), QStringLiteral("9600") );
    settings.setValue( QStringLiteral("DataBits"), QStringLiteral("8") );
    settings.setValue( QStringLiteral("StartBits"), QStringLiteral("2") );
    settings.setValue( QStringLiteral("StopBits"), QStringLiteral("1") );
    settings.setValue( QStringLiteral("Parity"), QStringLiteral("NONE") );
    settings.setValue( QStringLiteral("FlowControl"), QStringLiteral("NONE") );
    settings.setValue( QStringLiteral("RXQueue"), QStringLiteral("1024") );
    settings.setValue( QStringLiteral("FlushRate"), QStringLiteral("256") );
    settings.setValue( QStringLiteral("ScrollBuffer"), QStringLiteral("1024") );
    settings.setValue( QStringLiteral("LineEnding"), QStringLiteral("CR") );
    settings.endGroup();
  }

  KomportApp *win = new TestableKomportApp();
  win->show();
  QPointer<KomportApp> guard(win);

  win->loadProfile( QStringLiteral("StartBitsTest") );

  // The profile's value reached the request the application staged through the
  // single configuration entry point (even though the device cannot be opened).
  KomportSerial *serial = win->getDocument()->getSerial();
  QCOMPARE( serial->requestedConfiguration().startBits, QStringLiteral("2") );

  // Saving the profile writes the same value back, so the field is neither lost
  // nor normalised on its way through the application.
  static_cast<TestableKomportApp *>(win)->saveProfile( QStringLiteral("StartBitsTest") );
  {
    QSettings check;
    check.beginGroup( QStringLiteral("Profiles/StartBitsTest") );
    QCOMPARE( check.value( QStringLiteral("StartBits") ).toString(), QStringLiteral("2") );
    check.endGroup();
  }

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

QTEST_MAIN(TstProfileError)
#include "tst_profileerror.moc"
