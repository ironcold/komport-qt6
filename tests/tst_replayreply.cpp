/***************************************************************************
                tst_replayreply.cpp  -  Komport M10 step 4
                             ------------------
    begin                : 2026-09-20
    copyright            : (C) 2026 by Mike Sharkey
    email                : mike@mrjdesigns.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// M10 step 4 (SPEC-M10 section 11, sections 5.7/5.8): the reply-free rendering
// path. A replayed "CSI 5 n", "CSI 6 n", "CSI c" or "ESC Z" is rendered, not
// answered - and the live entry points still answer, so the suppression belongs
// to the replay entry and not to the emulation as a whole.
//
// The evidence is the accepted set of section 5.8: the existing
// ITransport::bytesWritten signal (emitted by the sole accepted-write primitive),
// the silence of the pty peer, and the source audit in tst_replayreply_replysites
// (see the plan note) - no test-only transport seam, because the spec rejects
// substituting the transport.

#include "komportcellarray.h"
#include "komportemulation.h"
#include "komportserial.h"

#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>

#include <QByteArray>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QFile>
#include <QStringList>
#include <QList>
#include <QString>

#include <cerrno>
#include <fcntl.h>
#include <pty.h>
#include <unistd.h>

namespace {

/** Opens a pty and returns the slave name, or an empty string when unavailable. */
QString makePty(int *masterFd)
{
  int master = -1;
  int slave = -1;
  char name[256] = { 0 };
  if (::openpty(&master, &slave, name, nullptr, nullptr) != 0)
    return QString();
  ::close(slave);
  *masterFd = master;
  return QString::fromLocal8Bit(name);
}

/** A pty-backed serial: the peer whose silence is the end-to-end evidence. */
bool openPtySerial(KomportSerial &serial, int *masterFd)
{
  const QString name = makePty(masterFd);
  if (name.isEmpty())
    return false;
  ::fcntl(*masterFd, F_SETFL, ::fcntl(*masterFd, F_GETFL, 0) | O_NONBLOCK);
  serial.setDeviceName(name);
  serial.setBaudRate(qint32(9600));
  return serial.open();
}

/** The four queries the spec names, plus one private form of `c` the dispatcher
  *  accepts (it selects the handler on the final `c`, and the attributes handler
  *  reads no parameters at all). */
QList<QByteArray> terminalQueries()
{
  return { QByteArrayLiteral("\x1b[5n"), QByteArrayLiteral("\x1b[6n"),
           QByteArrayLiteral("\x1b[c"), QByteArrayLiteral("\x1bZ"),
           QByteArrayLiteral("\x1b[?1c") };
}

/** Reads up to @p bytes from the peer within @p timeoutMs; returns what arrived. */
QByteArray readPeerWithTimeout(int masterFd, int bytes, int timeoutMs)
{
  QByteArray received;
  QElapsedTimer clock;
  clock.start();
  while (received.size() < bytes && clock.elapsed() < timeoutMs) {
    char buffer[256] = { 0 };
    const ssize_t got = ::read(masterFd, buffer, sizeof(buffer));
    if (got > 0)
      received.append(buffer, int(got));
    else
      QTest::qWait(10);
  }
  return received;
}

/** Feeds one byte at a time through one of the emulation's two receive entries. */
template <typename Entry>
void feed(KomportEmulation &emulation, const QByteArray &bytes, Entry entry)
{
  for (char byte : bytes)
    (emulation.*entry)(byte);
}

} // namespace

class TstReplayReply : public QObject
{
  Q_OBJECT

private slots:
  void theLiveEntryPointsStillAnswer();
  void everyReplayedQueryIsRenderedButNotAnswered();
  void suppressionNeverSurvivesTheReplayCall();
  void aNestedReplayStaysSuppressedAndAReentrantLiveCallAnswers();
  void replayEntriesAndReplySitesPassTheSourceAudit();
};

void TstReplayReply::theLiveEntryPointsStillAnswer()
{
  KomportSerial serial;
  int masterFd = -1;
  if (!openPtySerial(serial, &masterFd))
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&serial, &masterFd]() {
    serial.close();
    if (masterFd >= 0)
      ::close(masterFd);
  });

  KomportCellArray cells;
  KomportEmulation emulation(&serial, &cells);
  QSignalSpy written(&serial, &KomportSerial::bytesWritten);

  // Each query's exact reply, so a wrong answer cannot pass (adversarial round,
  // finding 3). The cursor report is derived from the live cursor.
  for (const QByteArray &query : terminalQueries()) {
    written.clear();
    const QPoint cursor = cells.cursor();
    QByteArray expected;
    if (query == QByteArrayLiteral("\x1b[5n"))
      expected = QByteArrayLiteral("\x1b[0n");
    else if (query == QByteArrayLiteral("\x1b[6n"))
      expected = QByteArrayLiteral("\x1b[") + QByteArray::number(cursor.y() + 1) + ";"
                 + QByteArray::number(cursor.x() + 1) + "R";
    else
      expected = QByteArrayLiteral("\x1b[?62c");

    feed(emulation, query, &KomportEmulation::slotReceivedChar);
    QVERIFY2(written.count() >= 1, query.constData());
    QCOMPARE(readPeerWithTimeout(masterFd, expected.size(), 500), expected);
  }
}

void TstReplayReply::everyReplayedQueryIsRenderedButNotAnswered()
{
  KomportSerial serial;
  int masterFd = -1;
  if (!openPtySerial(serial, &masterFd))
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&serial, &masterFd]() {
    serial.close();
    if (masterFd >= 0)
      ::close(masterFd);
  });

  KomportCellArray cells;
  KomportEmulation emulation(&serial, &cells);
  QSignalSpy written(&serial, &KomportSerial::bytesWritten);

  for (const QByteArray &query : terminalQueries()) {
    written.clear();
    feed(emulation, query, &KomportEmulation::slotReplayReceivedChar);
    QCOMPARE(written.count(), 0);                                 // no accepted write
    // Silence with a bounded window: a delayed write would still be caught here,
    // which one non-blocking read that happens to see EAGAIN would not be
    // (adversarial round, finding 3).
    QCOMPARE(readPeerWithTimeout(masterFd, 1, 200), QByteArray());
  }

  // The replayed bytes are still rendered: a printable sentinel reaches the cell
  // array through the replay entry exactly as it would live.
  feed(emulation, QByteArrayLiteral("Z"), &KomportEmulation::slotReplayReceivedChar);
  KomportCell *cell = cells.cell(0, 0);
  QVERIFY(cell != nullptr);
  QCOMPARE(cell->character(), QChar('Z'));
}

void TstReplayReply::suppressionNeverSurvivesTheReplayCall()
{
  KomportSerial serial;
  int masterFd = -1;
  if (!openPtySerial(serial, &masterFd))
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&serial, &masterFd]() {
    serial.close();
    if (masterFd >= 0)
      ::close(masterFd);
  });

  KomportCellArray cells;
  KomportEmulation emulation(&serial, &cells);
  QSignalSpy written(&serial, &KomportSerial::bytesWritten);

  // A replay render that is not a query at all, then a live query: the reply must
  // still come, so the mode was restored on the replay call's exit rather than
  // left suppressed.
  feed(emulation, QByteArrayLiteral("A"), &KomportEmulation::slotReplayReceivedChar);
  written.clear();
  feed(emulation, QByteArrayLiteral("\x1b[5n"), &KomportEmulation::slotReceivedChar);
  QVERIFY(written.count() >= 1);
}

void TstReplayReply::aNestedReplayStaysSuppressedAndAReentrantLiveCallAnswers()
{
  KomportSerial serial;
  int masterFd = -1;
  if (!openPtySerial(serial, &masterFd))
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&serial, &masterFd]() {
    serial.close();
    if (masterFd >= 0)
      ::close(masterFd);
  });

  KomportCellArray cells;
  KomportEmulation emulation(&serial, &cells);
  QSignalSpy written(&serial, &KomportSerial::bytesWritten);

  // Synchronous re-entry, created by a `cellChanged` receiver - that signal is
  // emitted from drawChar() - while the *outer* replay render is active: a nested
  // replay query (must stay suppressed) and a live query (must answer, or section
  // 5.7's live requirement would be broken by an outer replay).
  bool done = false;
  QObject::connect(&cells, &KomportCellArray::cellChanged, &emulation,
                   [&emulation, &done]() {
                     if (done)
                       return;
                     done = true;
                     feed(emulation, QByteArrayLiteral("\x1b[5n"),
                          &KomportEmulation::slotReplayReceivedChar);
                     feed(emulation, QByteArrayLiteral("\x1b[5n"),
                          &KomportEmulation::slotReceivedChar);
                   });

  written.clear();
  feed(emulation, QByteArrayLiteral("B"), &KomportEmulation::slotReplayReceivedChar);
  // Exactly one write during that render: the nested live call's reply. The nested
  // replay query inside it stayed suppressed although its own scope had set the
  // mode, because the inner scope restored the outer replay's mode on exit.
  QCOMPARE(written.count(), 1);

  // And afterwards a replay query is still suppressed: the outer mode survived.
  written.clear();
  feed(emulation, QByteArrayLiteral("\x1b[5n"), &KomportEmulation::slotReplayReceivedChar);
  QCOMPARE(written.count(), 0);
}

/** The structural half of section 5.8: the emulation answers the terminal in
  *  exactly one place. This reads the source, so it fails on the next reply site
  *  somebody adds without routing it through the choke point - the property is
  *  then structural rather than remembered. Staged as the plan says: the two
  *  receive entries and the reply sites now, the display adapter when it exists. */
void TstReplayReply::replayEntriesAndReplySitesPassTheSourceAudit()
{
  // The audit reads source text, so two things make it honest: it looks at *every*
  // transmitting identifier regardless of how the member is reached (`.` and `->` are
  // both fine, comments and string literals are stripped first), and its limit is
  // written down instead of implied - an indirect call (QMetaObject::invokeMethod, a
  // signal into another object, a virtual override elsewhere) is invisible to it.
  const auto stripCommentsAndStrings = [](const QString &in) {
    QString out;
    out.reserve(in.size());
    for (int i = 0; i < in.size(); ++i) {
      const QChar c = in.at(i);
      if (c == QLatin1Char('/') && i + 1 < in.size() && in.at(i + 1) == QLatin1Char('/')) {
        while (i < in.size() && in.at(i) != QLatin1Char('\n'))
          out.append(QLatin1Char(' ')), ++i;
        out.append(QLatin1Char('\n'));
      } else if (c == QLatin1Char('/') && i + 1 < in.size() && in.at(i + 1) == QLatin1Char('*')) {
        i += 1;
        out.append(QLatin1String("  "));
        while (i + 1 < in.size() && !(in.at(i) == QLatin1Char('*') && in.at(i + 1) == QLatin1Char('/'))) {
          out.append(in.at(i) == QLatin1Char('\n') ? QLatin1Char('\n') : QLatin1Char(' '));
          ++i;
        }
        i += 1;
        out.append(QLatin1String("  "));
      } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
        const QChar quote = c;
        out.append(QLatin1Char(' '));
        ++i;
        while (i < in.size() && in.at(i) != quote) {
          if (in.at(i) == QLatin1Char('\\')) {
            out.append(QLatin1Char(' '));
            ++i;
          }
          out.append(in.at(i) == QLatin1Char('\n') ? QLatin1Char('\n') : QLatin1Char(' '));
          ++i;
        }
        out.append(QLatin1Char(' '));
      } else {
        out.append(c);
      }
    }
    return out;
  };

  // Brace-aware: a body runs from the first `{` after `Owner::name(` to its matching
  // `}` in the stripped text, so braces inside strings or comments cannot confuse it.
  const auto bodiesOf = [&stripCommentsAndStrings](const QString &raw, const QString &owner) {
    const QString text = stripCommentsAndStrings(raw);
    QList<QPair<QString, QPair<int, int>>> bodies;
    const QRegularExpression definition(QStringLiteral("\\b") + owner + QStringLiteral("::(\\w+)\\s*\\("));
    auto match = definition.globalMatch(text);
    while (match.hasNext()) {
      const QRegularExpressionMatch current = match.next();
      const int opening = text.indexOf(QLatin1Char('{'), current.capturedEnd());
      if (opening < 0)
        continue;
      int depth = 0;
      int closing = -1;
      for (int at = opening; at < text.size(); ++at) {
        if (text.at(at) == QLatin1Char('{'))
          ++depth;
        else if (text.at(at) == QLatin1Char('}')) {
          --depth;
          if (depth == 0) {
            closing = at;
            break;
          }
        }
      }
      if (closing > opening)
        bodies.append(qMakePair(current.captured(1), qMakePair(opening, closing)));
    }
    return bodies;
  };

  const QStringList transmitIdentifiers = { QStringLiteral("putStr"), QStringLiteral("putChar"),
                                            QStringLiteral("writeBytes") };
  const auto reportsTransmit = [&transmitIdentifiers](const QString &body) {
    for (const QString &identifier : transmitIdentifiers)
      if (QRegularExpression(QStringLiteral("\\b") + identifier + QStringLiteral("\\b")).match(body).hasMatch())
        return true;
    return false;
  };

  // 1. The emulation. Its transmitting functions must be exactly the allow-list: the
  //    choke point plus the two established active-input functions.
  QFile emulationFile(QStringLiteral(KOMPORT_EMULATION_SOURCE));
  QVERIFY2(emulationFile.open(QIODevice::ReadOnly | QIODevice::Text),
           qPrintable(emulationFile.fileName()));
  const QString emulationRaw = QString::fromUtf8(emulationFile.readAll());
  const QString emulationText = stripCommentsAndStrings(emulationRaw);
  const auto emulationBodies = bodiesOf(emulationRaw, QStringLiteral("KomportEmulation"));
  QStringList emulationTransmitting;
  for (const auto &body : emulationBodies)
    if (reportsTransmit(emulationText.mid(body.second.first, body.second.second - body.second.first)))
      emulationTransmitting.append(body.first);
  emulationTransmitting.sort();
  const QStringList emulationAllowed = { QStringLiteral("sendTerminalReply"),
                                         QStringLiteral("slotKeyPressed"),
                                         QStringLiteral("slotSimKeyPressed") };
  QVERIFY2(emulationTransmitting == emulationAllowed,
           qPrintable(emulationTransmitting.join(QStringLiteral(", "))));

  // Every transmitting identifier lies inside a body this audit parsed *and* that
  // body is allow-listed - otherwise a call in a function the extraction misses would
  // slip through while the set comparison above still passed.
  for (const QString &identifier : transmitIdentifiers) {
    auto occurrences = QRegularExpression(QStringLiteral("\\b") + identifier + QStringLiteral("\\b"))
                           .globalMatch(emulationText);
    while (occurrences.hasNext()) {
      const int at = occurrences.next().capturedStart();
      QString enclosing;
      for (const auto &body : emulationBodies)
        if (at > body.second.first && at < body.second.second)
          enclosing = body.first;
      QVERIFY2(!enclosing.isEmpty(),
               qPrintable(QStringLiteral("a %1 call outside every parsed body").arg(identifier)));
      QVERIFY2(emulationAllowed.contains(enclosing),
               qPrintable(QStringLiteral("%1 transmits (%2)").arg(enclosing, identifier)));
    }
  }

  // 2. The view replay path, which SPEC-M10 5.8 names as well: the replay entry and
  //    the live entry it mirrors transmit nothing at all, and the set of transmitting
  //    view functions must be the allow-list below.
  QFile viewFile(QStringLiteral(KOMPORT_VIEW_SOURCE));
  QVERIFY2(viewFile.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(viewFile.fileName()));
  const QString viewRaw = QString::fromUtf8(viewFile.readAll());
  const QString viewText = stripCommentsAndStrings(viewRaw);
  const auto viewBodies = bodiesOf(viewRaw, QStringLiteral("KomportView"));
  for (const QString &entry : { QStringLiteral("slotReplayReceivedChar"),
                                QStringLiteral("slotReceivedChar") }) {
    bool found = false;
    for (const auto &body : viewBodies) {
      if (body.first != entry)
        continue;
      found = true;
      const QString text = viewText.mid(body.second.first, body.second.second - body.second.first);
      QVERIFY2(!reportsTransmit(text), qPrintable(entry));
      QVERIFY2(!text.contains(QStringLiteral("getSerial"))
                   || entry == QStringLiteral("slotReceivedChar"),
               qPrintable(entry));
    }
    QVERIFY2(found, qPrintable(entry));
  }
  QStringList viewTransmitting;
  for (const auto &body : viewBodies)
    if (reportsTransmit(viewText.mid(body.second.first, body.second.second - body.second.first)))
      viewTransmitting.append(body.first);
  viewTransmitting.sort();
  const QStringList viewAllowed;   // the view owns no write path; its live path is a signal
  QVERIFY2(viewTransmitting == viewAllowed,
           qPrintable(viewTransmitting.join(QStringLiteral(", "))));

  // 3. The reply sites: the choke point is called, and the two reply functions reach
  //    the transport in no other way.
  QCOMPARE(emulationText.count(QStringLiteral("sendTerminalReply(")), 4);   // 1 definition + 3 sites
  for (const QString &replyFunction : { QStringLiteral("doDeviceStatusReport"),
                                        QStringLiteral("doDeviceAttributes") }) {
    const int at = emulationRaw.indexOf(QStringLiteral("void KomportEmulation::") + replyFunction);
    QVERIFY2(at >= 0, qPrintable(replyFunction));
    const QString body = emulationText.mid(
        emulationText.indexOf(QLatin1Char('{'), at),
        emulationText.indexOf(QStringLiteral("\n}\n"), at) - emulationText.indexOf(QLatin1Char('{'), at));
    QVERIFY2(body.contains(QStringLiteral("sendTerminalReply(")), qPrintable(replyFunction));
    QVERIFY2(!reportsTransmit(body), qPrintable(replyFunction));
  }

  // 4. The mechanism: both entries install the scoped mode, its restoration is guarded
  //    by a QPointer, the scope is private to the translation unit, and the live
  //    default sits in the header.
  for (const QString &entry : { QStringLiteral("void KomportEmulation::slotReceivedChar(char _ch)"),
                                QStringLiteral("void KomportEmulation::slotReplayReceivedChar(char _ch)") }) {
    const int at = emulationText.indexOf(entry);
    QVERIFY2(at >= 0, qPrintable(entry));
    const QString body = emulationText.mid(at, emulationText.indexOf(QStringLiteral("\n}\n"), at) - at);
    QVERIFY2(body.contains(QStringLiteral("KomportEmulationReplyModeScope")), qPrintable(entry));
    QVERIFY2(body.contains(QStringLiteral("renderReceivedChar")), qPrintable(entry));
  }
  QVERIFY(emulationText.contains(QStringLiteral("if (emulation)")));
  QVERIFY(emulationText.contains(QStringLiteral("QPointer<QObject> mEmulation;")));

  QFile headerFile(QStringLiteral(KOMPORT_EMULATION_HEADER));
  QVERIFY2(headerFile.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(headerFile.fileName()));
  const QString headerText = QString::fromUtf8(headerFile.readAll());
  QVERIFY(headerText.contains(QStringLiteral("bool mTerminalRepliesAllowed = true;")));
  QVERIFY(headerText.contains(QStringLiteral("class KomportEmulationReplyModeScope;")));
  QVERIFY(!headerText.contains(QStringLiteral("KomportEmulationReplyModeScope(KomportEmulation")));
  // The accepted, pre-existing limit is written down as a limit.
  QVERIFY(headerText.contains(QStringLiteral("not supported")));
  QVERIFY(headerText.contains(QStringLiteral("accepted, known limit")));
  QVERIFY(headerText.contains(QStringLiteral("cellChanged")));
}

QTEST_MAIN(TstReplayReply)
#include "tst_replayreply.moc"
