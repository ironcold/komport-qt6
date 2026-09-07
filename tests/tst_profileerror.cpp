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

#include <QTest>
#include <QPointer>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QLabel>

class TstProfileError : public QObject
{
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();

  void serialErrorSurvivesLoadProfileStatusMessage();

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
  // Seed a profile whose device cannot possibly exist, so
  // KomportSerial::open() fails and QSerialPort::errorOccurred() ->
  // slotPortError() -> settingsFailed() fires synchronously from inside
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

QTEST_MAIN(TstProfileError)
#include "tst_profileerror.moc"
