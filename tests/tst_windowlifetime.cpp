/***************************************************************************
                          tst_windowlifetime.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    Regression test for the window-lifetime fix (Qt::WA_DeleteOnClose,
    closeEvent() closing the serial port, ~KomportApp() unregistering from
    the static pViewList, and slotFileClose() not touching `this` after an
    accepted close()) - see TODO.md's Codex-review section for the full
    incident writeup (two review rounds).
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
#include "komportview.h"
#include "komportserial.h"

#include <QTest>
#include <QPointer>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>

class TstWindowLifetime : public QObject
{
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();

  void windowManagerCloseDestroysWindow();
  void slotFileCloseDoesNotTouchFreedWindow();

private:
  QString mTestConfigFile;
};

void TstWindowLifetime::initTestCase()
{
  // KomportApp reads/writes real QSettings (organization "Komport-Qt6") on
  // construction (readOptions()/seedBuiltinProfiles()/initProfiles()) -
  // redirect to a separate, test-only organization/application name so
  // this test never touches a real user's saved profiles/devices/macros.
  QCoreApplication::setOrganizationName( QStringLiteral("Komport-Qt6-Test") );
  QCoreApplication::setApplicationName( QStringLiteral("Komport-Qt6-Test") );
  mTestConfigFile = QSettings().fileName();
}

void TstWindowLifetime::cleanupTestCase()
{
  const QString dirPath = QFileInfo(mTestConfigFile).absolutePath();
  QFile::remove(mTestConfigFile);
  QDir().rmdir(dirPath); // only succeeds if empty - fine, we create nothing else in it
}

void TstWindowLifetime::windowManagerCloseDestroysWindow()
{
  KomportApp *win1 = new KomportApp();
  win1->show();
  KomportApp *win2 = new KomportApp();
  win2->show();

  QPointer<KomportApp> guard1(win1);

  // Simulate the window manager's close button (not the File > Close
  // menu action / slotFileClose()).
  win1->close();
  // QTRY_VERIFY2 rather than a single processEvents() call: the
  // DeferredDelete event WA_DeleteOnClose's deleteLater() posts is only
  // guaranteed to be delivered once the event loop actually gets to it,
  // which one bare processEvents() call isn't guaranteed to do in every
  // Qt version/platform combination under a test runner (no outer
  // QApplication::exec() is running here). QTRY_VERIFY2 pumps the event
  // loop repeatedly (qWait() internally) until the condition holds or a
  // timeout is hit, which is the robust way to wait for it.
  QTRY_VERIFY2( guard1.isNull(),
            "Qt::WA_DeleteOnClose should have destroyed win1 after close()" );

  // The actual regression this guards: pViewList (static, shared across
  // all KomportApp windows) must have been pruned of win1's view in
  // ~KomportApp() - otherwise this broadcast touches freed memory.
  win2->getDocument()->slotUpdateAllViews(nullptr);

  win2->close();
  QCoreApplication::processEvents();
}

void TstWindowLifetime::slotFileCloseDoesNotTouchFreedWindow()
{
  KomportApp *win = new KomportApp();
  win->show();
  QPointer<KomportApp> guard(win);
  KomportSerial *serial = win->getDocument()->getSerial();

  // Invoke the exact same slot the "File > Close" QAction triggers.
  // slotFileClose() -> close() -> closeEvent() closes the port
  // *synchronously* here; Qt::WA_DeleteOnClose only schedules the window's
  // actual destruction via deleteLater(), which doesn't run until control
  // returns to the event loop. So: check the port state first, while `win`
  // (and the KomportDoc it owns `serial` through) is still alive, *then*
  // pump events and check that the window itself is gone - checking in the
  // other order would dereference `serial` after it was freed alongside
  // `win`, a use-after-free in the test itself rather than a real check.
  QVERIFY( QMetaObject::invokeMethod(win, "slotFileClose") );

  QVERIFY2( !serial->isOpen(), "closeEvent() should have closed the "
                                "serial port" );

  // QTRY_VERIFY2 rather than a single processEvents() call - see the
  // comment on the same pattern in windowManagerCloseDestroysWindow()
  // above.
  QTRY_VERIFY2( guard.isNull(), "slotFileClose() should result in the "
                            "window being destroyed for an accepted close" );
}

QTEST_MAIN(TstWindowLifetime)
#include "tst_windowlifetime.moc"
