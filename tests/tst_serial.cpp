/***************************************************************************
                          tst_serial.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    Regression tests for KomportSerial::setRxQueue()/setFlushRate()/
    putChar()/putStr(): a value <= 0 for RXQueue used to make
    slotDataAvailable()'s overflow trim discard every byte that arrives,
    and a FlushRate of 0 (previously reachable from the settings dialog's
    spinbox directly, not just a hand-edited profile) turned the RX flush
    timer into an idle busy-poll (QTimer::start(0) re-fires on every
    single event-loop iteration) - see TODO.md's Codex-review section.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komportserial.h"

#include <QTest>
#include <QScopeGuard>
#include <pty.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>

class TstSerial : public QObject
{
  Q_OBJECT
private slots:
  void rxQueueClampsToPositive();
  void flushRateClampsToPositive();
  void putCharAndPutStrReportFailureWhenClosed();
  void lengthAwarePutStrRejectsNegativeLength();
  void lengthAwarePutStrPreservesEmbeddedNulAndFollowingControlChar();
};

void TstSerial::rxQueueClampsToPositive()
{
  KomportSerial serial;
  QCOMPARE( serial.setRxQueue(-1), 1 );
  QCOMPARE( serial.setRxQueue(0), 1 );
  // A normal, already-valid value must pass through unchanged.
  QCOMPARE( serial.setRxQueue(4096), 4096 );
}

void TstSerial::flushRateClampsToPositive()
{
  KomportSerial serial;
  // 0 used to be accepted (both here and by the settings dialog's
  // spinbox) - QTimer::start(0) re-fires on every single event-loop
  // iteration for as long as the timer is running, an idle busy-poll
  // rather than a "flush immediately" setting.
  serial.setFlushRate(-1);
  QCOMPARE( serial.flushRate(), 1 );
  serial.setFlushRate(0);
  QCOMPARE( serial.flushRate(), 1 );
  // A normal, already-valid value must pass through unchanged.
  serial.setFlushRate(100);
  QCOMPARE( serial.flushRate(), 100 );
}

void TstSerial::putCharAndPutStrReportFailureWhenClosed()
{
  // Regression test for putStr(): it used to be void, silently dropping a
  // partial/failed write with no way for a caller to notice (see TODO.md's
  // Codex-review section, third full review). This sandbox has no real
  // serial device to actually exercise a genuine partial hardware write,
  // but the closed-port early-return path is the one piece of that
  // contract that's reliably testable without one - both putChar() and
  // putStr() must report failure (not silently claim success) when there
  // is nowhere for the bytes to go.
  KomportSerial serial; // never opened
  QVERIFY( !serial.isOpen() );
  QCOMPARE( serial.putChar('x'), false );
  QCOMPARE( serial.putStr("hello"), false );
  // Must not crash on a null string either.
  QCOMPARE( serial.putStr(nullptr), false );
}

void TstSerial::lengthAwarePutStrPreservesEmbeddedNulAndFollowingControlChar()
{
  // Milestone 7 (Codex review finding, round 3): KomportApp::
  // slotMacroTriggered() can produce charset-translated macro text
  // containing a genuine embedded NUL byte (CP437's byte 0x00 is real
  // Unicode NUL - see KomportCharset). The null-terminated putStr(const
  // char*) overload would silently *truncate* everything after such a
  // NUL (strlen()-based length); a first fix attempt instead silently
  // *skipped* the NUL byte, which avoided the truncation but still
  // dropped/altered data without telling anyone. The new
  // putStr(const char*, qsizetype) overload must send every byte
  // verbatim, embedded NUL included - this test writes a payload with a
  // NUL followed immediately by a control character (CR) followed by
  // more data, and reads the raw bytes back from the other end of a real
  // local pty pair to prove the exact byte sequence made it onto the
  // wire unmodified. A payload that only *changed* (rather than got
  // shorter) would still be a bug this specific pattern is chosen to
  // catch: truncation would stop the read at "A" (1 byte total),
  // NUL-skipping would deliver "A\rB" (3 bytes, missing the NUL), and
  // only a genuinely correct implementation delivers all 4 bytes intact.
  int masterFd = -1, slaveFd = -1;
  char slaveName[256];
  if ( ::openpty(&masterFd, &slaveFd, slaveName, nullptr, nullptr) != 0 ) {
    QSKIP( "openpty() unavailable in this sandbox - cannot verify real wire bytes without a pty pair" );
  }
  ::close(slaveFd); // KomportSerial/QSerialPort opens its own fd on the slave path below
  // Codex review finding (round 4): masterFd used to have no
  // failure-safe cleanup - an assertion failure below (open()/putStr()/
  // the final QCOMPARE) returned early without closing it. qScopeGuard()
  // runs on every exit path (normal return, an early QVERIFY2/QCOMPARE
  // failure, even an exception), so this now always closes masterFd
  // exactly once, however the function actually exits.
  auto masterFdGuard = qScopeGuard( [&masterFd]() { if ( masterFd >= 0 ) ::close(masterFd); } );

  // masterFd is blocking by default - without O_NONBLOCK, the read loop
  // below's very first ::read() call would block indefinitely if
  // putStr()'s write hasn't actually reached the kernel pty buffer yet
  // (no explicit waitForBytesWritten() anywhere in this codebase's
  // putStr(), so that's a real race, not a hypothetical one) - non-
  // blocking mode plus the polling loop's own QDeadlineTimer/QTest::qWait()
  // is what actually gives Qt's event loop a chance to flush the pending
  // write between read attempts.
  //
  // Codex review finding (round 4): fcntl()'s own return value used to be
  // ignored - if it failed, masterFd would stay blocking and the first
  // ::read() below could hang forever regardless of the QDeadlineTimer,
  // since a blocking read() never even reaches the loop condition check.
  QVERIFY2( ::fcntl(masterFd, F_SETFL, O_NONBLOCK) == 0,
            qPrintable(QStringLiteral("fcntl(O_NONBLOCK) failed: %1").arg(QString::fromLocal8Bit(strerror(errno)))) );

  KomportSerial serial;
  serial.setDeviceName( QString::fromLocal8Bit(slaveName) );
  QVERIFY2( serial.open(), qPrintable(QStringLiteral("failed to open pty slave %1 via KomportSerial").arg(slaveName)) );

  const char payload[] = { 'A', '\0', 0x0D, 'B' }; // embedded NUL, then a control char (CR), then more data
  QVERIFY( serial.putStr( payload, static_cast<qsizetype>(sizeof(payload)) ) );

  QByteArray received;
  QDeadlineTimer deadline(2000);
  while ( received.size() < static_cast<int>(sizeof(payload)) && !deadline.hasExpired() ) {
    char buf[64];
    const ssize_t n = ::read( masterFd, buf, sizeof(buf) );
    if ( n > 0 ) received.append( buf, static_cast<int>(n) );
    else if ( n < 0 && errno != EAGAIN && errno != EWOULDBLOCK ) break;
    QTest::qWait(10);
  }

  QCOMPARE( received, QByteArray(payload, sizeof(payload)) );

  serial.close(); // masterFd itself is closed by masterFdGuard above
}

void TstSerial::lengthAwarePutStrRejectsNegativeLength()
{
  // Codex review finding (round 4): qsizetype is signed, but a negative
  // _len wasn't rejected before reaching QIODevice::write(). For
  // _len == -1 specifically, write() returns its own -1 error sentinel,
  // and the old "written != len" check (-1 != -1, false) then reported
  // *success* despite transmitting nothing - the opposite of what a
  // caller checking the return value would expect. isOpen() alone
  // already returns false on an unopened KomportSerial regardless of
  // this fix (see putCharAndPutStrReportFailureWhenClosed() above), so a
  // real open port (the same pty-pair technique as the test above) is
  // needed here to actually exercise the negative-length check in
  // isolation, rather than accidentally passing for the wrong reason.
  int masterFd = -1, slaveFd = -1;
  char slaveName[256];
  if ( ::openpty(&masterFd, &slaveFd, slaveName, nullptr, nullptr) != 0 ) {
    QSKIP( "openpty() unavailable in this sandbox - cannot verify against a real open port" );
  }
  ::close(slaveFd);
  auto masterFdGuard = qScopeGuard( [&masterFd]() { if ( masterFd >= 0 ) ::close(masterFd); } );
  QVERIFY2( ::fcntl(masterFd, F_SETFL, O_NONBLOCK) == 0,
            qPrintable(QStringLiteral("fcntl(O_NONBLOCK) failed: %1").arg(QString::fromLocal8Bit(strerror(errno)))) );

  KomportSerial serial;
  serial.setDeviceName( QString::fromLocal8Bit(slaveName) );
  QVERIFY2( serial.open(), qPrintable(QStringLiteral("failed to open pty slave %1 via KomportSerial").arg(slaveName)) );

  QCOMPARE( serial.putStr("x", -1), false );

  // Confirm nothing was actually written to the wire either - a false
  // return that still silently sent bytes would be its own bug.
  char buf[8];
  QTest::qWait(50);
  QCOMPARE( ::read(masterFd, buf, sizeof(buf)), -1 );
  QVERIFY( errno == EAGAIN || errno == EWOULDBLOCK );

  serial.close();
}

QTEST_MAIN(TstSerial)
#include "tst_serial.moc"
