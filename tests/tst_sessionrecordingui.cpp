/***************************************************************************
                     tst_sessionrecordingui.cpp  -  Komport Recording UI
                             ----------------------
    begin                : 2026
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    M9 tests for the application wiring of the live session recording
    (SPEC-M9 5.6/5.7, implementation step 5b): the `Record Live Session...`
    action and the persistent status-bar indicator follow the recorder's state,
    an idle session refuses a start without creating a file, and a stopped
    recording leaves a readable `.kpsession` file behind.

    The file dialog itself is modal and interactive, so the tests drive
    `KomportApp::startSessionRecording()`, the non-interactive part the dialog
    delegates to; the dialog's own behaviour is Qt's.
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
#include "sessionrecorder.h"

#include "sessionrecordreader.h"

#include <QAction>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPointer>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <fcntl.h>
#include <pty.h>
#include <unistd.h>

namespace {

/** Create a pty pair and return the slave path (as tst_sessiondocument does). */
QString makePty(int *masterFd)
{
  int slaveFd = -1;
  char slaveName[256];
  if (::openpty(masterFd, &slaveFd, slaveName, nullptr, nullptr) != 0)
    return QString();
  ::close(slaveFd);
  return QString::fromLocal8Bit(slaveName);
}

} // namespace

class TstSessionRecordingUi : public QObject
{
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();

  void anIdleSessionRefusesTheRecordingStart();
  void aRecordingShowsItsIndicatorAndStopsWithAReport();

private:
  QString mTestConfigFile;
};

void TstSessionRecordingUi::initTestCase()
{
  // KomportApp reads and writes real QSettings on construction; redirect it to a
  // test-only organization so this test cannot touch a real user's profiles,
  // devices or macros.
  QCoreApplication::setOrganizationName(QStringLiteral("Komport-Qt6-Test"));
  QCoreApplication::setApplicationName(QStringLiteral("Komport-Qt6-Test"));
  mTestConfigFile = QSettings().fileName();
}

void TstSessionRecordingUi::cleanupTestCase()
{
  const QString dirPath = QFileInfo(mTestConfigFile).absolutePath();
  QFile::remove(mTestConfigFile);
  QDir().rmdir(dirPath);   // only succeeds if empty - fine, we create nothing else in it
}

/** Without a live session there is no clock-domain anchor and no applied
  *  configuration, so the start is refused - without touching the target. */
void TstSessionRecordingUi::anIdleSessionRefusesTheRecordingStart()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("idle.kpsession"));

  KomportApp *window = new KomportApp();
  QAction *action = window->findChild<QAction *>(QStringLiteral("recordLiveSession"));
  QLabel *indicator = window->findChild<QLabel *>(QStringLiteral("recordingStatusLabel"));
  QVERIFY(action != nullptr);
  QVERIFY(indicator != nullptr);
  QCOMPARE(action->isChecked(), false);
  QVERIFY(indicator->isHidden());

  QCOMPARE(window->startSessionRecording(path), false);

  QCOMPARE(QFile::exists(path), false);
  QCOMPARE(action->isChecked(), false);
  QVERIFY(indicator->isHidden());
  // The refusal must be explained, not merely returned.
  QLabel *status = window->findChild<QLabel *>(QStringLiteral("hoverHintLabel"));
  QVERIFY(status != nullptr);
  QVERIFY2(status->text().contains(QStringLiteral("Recording not started")),
           qPrintable(status->text()));
  QVERIFY2(status->text().contains(QStringLiteral("live")), qPrintable(status->text()));

  delete window;
}

/** The user-visible half of a recording: the indicator appears while it runs,
  *  both the action and the indicator return to rest when it stops, and the file
  *  it leaves behind is a loadable session. */
void TstSessionRecordingUi::aRecordingShowsItsIndicatorAndStopsWithAReport()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  ::fcntl(masterFd, F_SETFL, ::fcntl(masterFd, F_GETFL, 0) | O_NONBLOCK);

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("live.kpsession"));

  KomportApp *window = new KomportApp();
  QAction *action = window->findChild<QAction *>(QStringLiteral("recordLiveSession"));
  QLabel *indicator = window->findChild<QLabel *>(QStringLiteral("recordingStatusLabel"));
  QVERIFY(action != nullptr);
  QVERIFY(indicator != nullptr);

  KomportSerial *serial = window->getDocument()->getSerial();
  serial->setDeviceName(slaveName);
  serial->setBaudRate(qint32(9600));
  QVERIFY(serial->open());

  // The action's own entry point. `isHidden()` rather than `isVisible()`: the test
  // never shows the window, and an explicitly hidden widget reports isHidden().
  QVERIFY2(window->startSessionRecording(path), "the start was refused");
  QCOMPARE(action->isChecked(), true);
  QVERIFY(!indicator->isHidden());
  const qint64 headerOnlySize = QFileInfo(path).size();
  QVERIFY(headerOnlySize > 0);   // the header is flushed at start (ADR-010 4)

  // Traffic, so the recording has something to report. The transport read is
  // event-driven and the file grows when the flush policy flushes, hence QTRY.
  const QByteArray payload("recording ui");
  QCOMPARE(::write(masterFd, payload.constData(), payload.size()), qint64(payload.size()));
  QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(path).size() > headerOnlySize, 5000);

  // The user clicks the action off again: the report goes to the status
  // mechanism and the user interface returns to rest.
  window->slotToggleSessionRecording(false);
  QCOMPARE(action->isChecked(), false);
  QVERIFY(indicator->isHidden());
  // The full structured summary, not merely its wording: the record count, the
  // accepted byte count, the duration and the path all have to be there.
  QLabel *status = window->findChild<QLabel *>(QStringLiteral("hoverHintLabel"));
  QVERIFY(status != nullptr);
  // The English source form carries the "(s)" numerus convention, since English
  // needs no separate translation file; the German catalog has both forms.
  const QRegularExpression pattern(
      QStringLiteral("^Recording stopped: 1 complete record\\(s\\), \\d+ bytes in \\d+\\.\\d s, into ")
      + QRegularExpression::escape(path) + QStringLiteral("$"));
  QVERIFY2(pattern.match(status->text()).hasMatch(), qPrintable(status->text()));
  QFile counted(path);
  QVERIFY(counted.open(QIODevice::ReadOnly));
  const qint64 fileSize = counted.size();
  counted.close();
  QVERIFY2(status->text().contains(QString::number(fileSize)), qPrintable(status->text()));

  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const SessionRecordFile recorded = readSessionRecordFile(file.readAll());
  QVERIFY2(recorded.loadable, qPrintable(recorded.error));
  QVERIFY(recorded.records.size() >= 1);
  bool sawRx = false;
  for (const SessionRecordFileRecord &record : recorded.records) {
    if (record.eventType == quint16(SessionEventType::Data)
        && record.direction == quint8(SessionDirection::Rx))
      sawRx = true;
  }
  QVERIFY2(sawRx, "the traffic that arrived while recording was not written");

  delete window;
}

QTEST_MAIN(TstSessionRecordingUi)

#include "tst_sessionrecordingui.moc"
