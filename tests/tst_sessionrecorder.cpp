/***************************************************************************
                       tst_sessionrecorder.cpp  -  Komport Session Recorder
                             -------------------
    begin                : 2026
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    M9 recorder tests against SPEC-M9 (sections 5.3-5.9 and 7): one record per
    accepted event, the header written and flushed at start with the domain's
    true anchor, the periodic flush that bounds the process buffer, and the
    failure paths that end a recording as damaged while keeping the complete
    prefix valid.

    The transport is a deterministic double, the sink is scripted (short writes,
    failed flushes) and the flush scheduler is manual, so nothing depends on a
    device, a disk or wall-clock time. The file is parsed by the test-side reader
    of sessionrecordreader.h, which shares no code with the production codec.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "sessionrecorder.h"
#include "sessionrecorderseams.h"
#include "sessioncontroller.h"

#include "sessionrecordreader.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <functional>
#include <memory>

/** The one permitted way to construct a recorder with the SPEC-M9 5.9 seams: the
  *  injection constructor is private, because the seams are not public API. */
struct SessionRecorderSeamsForTest
{
  static std::unique_ptr<SessionRecorder> create(SessionController *controller,
                                                 SessionRecordSink *sink,
                                                 SessionFlushScheduler *scheduler,
                                                 SessionMonotonicClock *clock)
  {
    return std::unique_ptr<SessionRecorder>(new SessionRecorder(controller, sink, scheduler, clock));
  }
};

namespace {

/** A deterministic ITransport double: every observation is scripted by the test,
  *  and every call the recorder is not allowed to make is counted. */
class ScriptedTransport : public ITransport
{
public:
  using ITransport::ITransport;

  void scriptOpened(quint64 activationId, qint64 sourceTimestampNs)
  {
    emit opened(activationId, sourceTimestampNs, QJsonObject());
  }

  void scriptClosed(quint64 activationId, qint64 sourceTimestampNs)
  {
    emit closed(activationId, sourceTimestampNs, QJsonObject());
  }

  void scriptRx(quint64 activationId, const QByteArray &bytes, qint64 sourceTimestampNs)
  {
    emit bytesReceived(activationId, bytes, sourceTimestampNs);
  }

  void scriptConfigurationChanged(quint64 activationId, qint64 sourceTimestampNs,
                                  const QJsonObject &metadata)
  {
    emit configurationChanged(activationId, sourceTimestampNs, metadata);
  }

  /** Calls the recorder must never make (ADR-007: no output path). */
  int callsIntoTheTransport() const { return mOpenCalls + mCloseCalls + mWriteCalls; }

  bool open() override
  {
    ++mOpenCalls;
    return true;
  }
  void close() override { ++mCloseCalls; }
  bool isOpen() const override { return true; }
  qint64 writeBytes(const QByteArray &bytes) override
  {
    ++mWriteCalls;
    return bytes.size();
  }

private:
  mutable int mOpenCalls = 0;
  mutable int mCloseCalls = 0;
  mutable int mWriteCalls = 0;
};

/** A sink that keeps exactly the bytes it accepted, and can be told to fail.
  *
  * A short write accepts one byte less than offered and appends only that prefix,
  * which is what a real disk does when it runs out of space mid-write. */
class ScriptedSink : public SessionRecordSink
{
public:
  bool failOpen = false;         ///< open() fails
  int failWriteCall = -1;        ///< the nth write (1-based) fails entirely
  int shortWriteCall = -1;       ///< the nth write accepts one byte less
  int failFlushCall = -1;        ///< the nth flush (1-based) fails

  bool open(const QString &path) override
  {
    mPath = path;
    ++mOpenCalls;
    mOpen = !failOpen;
    return mOpen;
  }

  qint64 write(const QByteArray &bytes) override
  {
    ++mWriteCalls;
    if (mWriteCalls == failWriteCall)
      return -1;
    qint64 accepted = bytes.size();
    if (mWriteCalls == shortWriteCall)
      accepted = qMax<qint64>(0, accepted - 1);
    mBytes.append(bytes.left(int(accepted)));
    return accepted;
  }

  bool flush() override
  {
    ++mFlushCalls;
    return mFlushCalls != failFlushCall;
  }

  void close() override
  {
    ++mCloseCalls;
    mOpen = false;
  }

  QString path() const { return mPath; }
  bool isOpen() const { return mOpen; }
  qint64 writtenBytes() const { return mBytes.size(); }
  const QByteArray &acceptedBytes() const { return mBytes; }
  int writeCalls() const { return mWriteCalls; }
  int flushCalls() const { return mFlushCalls; }
  int closeCalls() const { return mCloseCalls; }

private:
  QString mPath;
  QByteArray mBytes;
  bool mOpen = false;
  int mOpenCalls = 0;
  int mWriteCalls = 0;
  int mFlushCalls = 0;
  int mCloseCalls = 0;
};

/** A manual flush scheduler: the test decides when the timer fires. */
class ManualScheduler : public SessionFlushScheduler
{
public:
  void schedule(int milliseconds, std::function<void()> callback) override
  {
    mDelayMs = milliseconds;
    mCallback = std::move(callback);
    ++mScheduleCalls;
  }

  void cancel() override
  {
    mCallback = nullptr;
    ++mCancelCalls;
  }

  bool pending() const { return static_cast<bool>(mCallback); }
  int delayMs() const { return mDelayMs; }
  int scheduleCalls() const { return mScheduleCalls; }
  int cancelCalls() const { return mCancelCalls; }

  /** Fire the scheduled flush, if one is pending. */
  void fire()
  {
    std::function<void()> callback = std::move(mCallback);
    mCallback = nullptr;
    if (callback)
      callback();
  }

private:
  std::function<void()> mCallback;
  int mDelayMs = 0;
  int mScheduleCalls = 0;
  int mCancelCalls = 0;
};

/** The applied configuration snapshot of ADR-006: exactly four sections. */
QJsonObject configurationSnapshot()
{
  const auto hardware = [](const QString &endpoint) {
    QJsonObject object;
    object.insert(QStringLiteral("baudRate"), QStringLiteral("9600"));
    object.insert(QStringLiteral("dataBits"), QStringLiteral("8"));
    object.insert(QStringLiteral("endpoint"), endpoint);
    object.insert(QStringLiteral("flowControl"), QStringLiteral("none"));
    object.insert(QStringLiteral("parity"), QStringLiteral("none"));
    object.insert(QStringLiteral("stopBits"), QStringLiteral("1"));
    return object;
  };

  QJsonObject buffering;
  buffering.insert(QStringLiteral("flushRate"), 256);
  buffering.insert(QStringLiteral("rxQueue"), 1024);

  QJsonObject compatibility;
  compatibility.insert(QStringLiteral("startBits"), QStringLiteral("1"));

  QJsonObject configuration;
  configuration.insert(QStringLiteral("compatibility"), compatibility);
  configuration.insert(QStringLiteral("effective"), hardware(QStringLiteral("/dev/ttyUSB0")));
  configuration.insert(QStringLiteral("localBuffering"), buffering);
  configuration.insert(QStringLiteral("requested"), hardware(QStringLiteral("/dev/ttyUSB0")));
  return configuration;
}

SessionRecordingRequest recordingRequest(const QString &path)
{
  SessionRecordingRequest request;
  request.path = path;
  request.applicationName = QStringLiteral("Komport");
  request.applicationVersion = QStringLiteral("2.0.0");
  request.configuration = configurationSnapshot();
  return request;
}

/** A monotonic clock the test advances by hand, so the reported duration is
  *  verified without waiting for wall-clock time. */
class ManualClock : public SessionMonotonicClock
{
public:
  /** @param startNs the first reading; 0 is a legal value and must be handled. */
  explicit ManualClock(qint64 startNs = 1000000) : mNow(startNs) {}

  qint64 nowNs() override { return mNow; }
  void advance(qint64 deltaNs) { mNow += deltaNs; }
  void set(qint64 value) { mNow = value; }

private:
  qint64 mNow;
};

/** The pieces every test needs, in construction order. */
struct Fixture {
  ScriptedTransport transport;
  SessionController controller{ &transport };
  ScriptedSink sink;
  ManualScheduler scheduler;
  ManualClock clock;
  std::unique_ptr<SessionRecorder> owned{
      SessionRecorderSeamsForTest::create(&controller, &sink, &scheduler, &clock) };
  SessionRecorder &recorder = *owned;

  /** Bring the session live and accept its first event as the anchor. */
  void goLive(qint64 anchorSourceTimestampNs = 1000)
  {
    transport.scriptOpened(1, anchorSourceTimestampNs);
  }
};

} // namespace

class TstSessionRecorder : public QObject
{
Q_OBJECT

private slots:
  void startWritesACompleteHeaderAndNeedsALiveSession();
  void oneRecordPerAcceptedEventWithByteExactPayloads();
  void midSessionStartWritesTheTrueAnchorReference();
  void metadataIsWrittenVerbatim();
  void aShortWriteEndsTheRecordingAsDamagedAndKeepsTheCompletePrefix();
  void aFailedFlushEndsTheRecordingAsDamaged();
  void theProcessBufferIsBoundedByOneTimerPerBurst();
  void theRecorderNeverCallsIntoTheTransport();
  void stopReportsAndAllowsASecondRecording();
  void destroyingARunningRecorderFinalisesWithoutASignal();
  void theReportedDurationComesFromTheMonotonicClock();
  void startAfterADamagedRecordingFinalisesItAndStartsANewFile();
  void aShortHeaderWriteRefusesTheStartAndLeavesNoLoadableFile();
  void anEventThatCannotBeEncodedReportsItsSequenceAndSizes();
  void aShortWriteReportsEveryAcceptedByte();
  void anExplicitStopWhoseFinalFlushFailsReportsItWithoutASignal();
  void theReaderRejectsMalformedWriterOutput();
  void theReaderRejectsNonConformantProfileVariants();
  void aClockThatStartsAtZeroReportsTheRealDuration();
  void theStartFlushMakesTheHeaderVisibleThroughASecondHandle();
};

/** The header is written and flushed at start, with no waiting for a first event;
  *  a start without a live session is refused and touches nothing. */
void TstSessionRecorder::startWritesACompleteHeaderAndNeedsALiveSession()
{
  Fixture fixture;

  // Idle session: refused, and nothing is written or even opened.
  const SessionRecordingStart refused = fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/a.kpsession")));
  QCOMPARE(refused.ok, false);
  QVERIFY(!refused.reason.isEmpty());
  QCOMPARE(fixture.sink.writeCalls(), 0);
  QCOMPARE(fixture.sink.path(), QString());
  QCOMPARE(static_cast<int>(fixture.recorder.state()), static_cast<int>(SessionRecorder::State::Stopped));

  // Live session: the file exists with a complete header and no records.
  fixture.goLive();
  const SessionRecordingStart started = fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/a.kpsession")));
  QVERIFY2(started.ok, qPrintable(started.reason));
  QCOMPARE(static_cast<int>(fixture.recorder.state()), static_cast<int>(SessionRecorder::State::Live));
  QCOMPARE(fixture.sink.writeCalls(), 1);        // the header block, in one call
  QCOMPARE(fixture.sink.flushCalls(), 1);        // the start flush of ADR-010 4
  QCOMPARE(fixture.sink.isOpen(), true);

  const SessionRecordFile file = readSessionRecordFile(fixture.sink.acceptedBytes());
  QVERIFY2(file.loadable, qPrintable(file.error));
  QVERIFY(file.records.isEmpty());
  QCOMPARE(file.bytesConsumed, fixture.sink.writtenBytes());

  // A recording without a single event is a valid file, honestly reported.
  const SessionRecordingReport report = fixture.recorder.stop();
  QCOMPARE(report.records, quint64(0));
  QCOMPARE(report.bytes, fixture.sink.writtenBytes());
  QCOMPARE(report.damaged, false);
  QCOMPARE(report.path, QStringLiteral("/tmp/a.kpsession"));
  QCOMPARE(report.reason, QStringLiteral("no session data recorded"));
  QCOMPARE(fixture.sink.closeCalls(), 1);
}

/** One record per accepted event, in order, with byte-exact payloads. */
void TstSessionRecorder::oneRecordPerAcceptedEventWithByteExactPayloads()
{
  Fixture fixture;
  fixture.goLive();
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/b.kpsession"))).ok);

  // Every byte value, in two observations, so NUL and 0xFF must survive.
  QByteArray allBytes;
  for (int i = 0; i < 256; ++i)
    allBytes.append(char(i));
  fixture.transport.scriptRx(1, QByteArrayLiteral("first"), 2000);
  fixture.transport.scriptRx(1, allBytes, 3000);

  const SessionRecordingReport report = fixture.recorder.stop();
  QCOMPARE(report.records, quint64(2));

  const SessionRecordFile file = readSessionRecordFile(fixture.sink.acceptedBytes());
  QVERIFY2(file.loadable, qPrintable(file.error));
  QCOMPARE(file.records.size(), 2);
  QCOMPARE(file.records.at(0).payload, QByteArrayLiteral("first"));
  QCOMPARE(file.records.at(1).payload, allBytes);
  QCOMPARE(file.records.at(0).timestampNs, qint64(1000));   // 2000 - 1000
  QCOMPARE(file.records.at(1).timestampNs, qint64(2000));   // 3000 - 1000
  // The recording starts mid-session, so the sequence continues from the events the
  // session already emitted: the anchor (the `opened` observation) was sequence 1
  // and is *not* in the file - the file begins where the recording began.
  QCOMPARE(file.records.at(0).sequence, quint64(2));
  QCOMPARE(file.records.at(1).sequence, quint64(3));
  QCOMPARE(file.records.at(0).sourceId, quint32(1));
  QCOMPARE(file.records.at(0).flags, quint8(0));
  // A data event carries no metadata, so the record holds exactly its prefix and
  // its payload: the empty object is the zero-length encoding, never "{}".
  QCOMPARE(file.records.at(0).metadataJson.size(), 0);
  QCOMPARE(file.records.at(0).metadata.isEmpty(), true);
  QCOMPARE(file.records.at(0).recordLength, quint32(40 + file.records.at(0).payload.size()));
  QCOMPARE(int(file.records.at(1).eventType), int(SessionEventType::Data));
  QCOMPARE(int(file.records.at(1).direction), int(SessionDirection::Rx));
}

/** A recording started while the session is already running carries the domain's
  *  true anchor, not the first event it happens to see (ADR-010 D8). */
void TstSessionRecorder::midSessionStartWritesTheTrueAnchorReference()
{
  Fixture fixture;
  fixture.goLive(1000);                          // the anchor, session time 0
  fixture.transport.scriptRx(1, QByteArrayLiteral("before"), 4500);

  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/c.kpsession"))).ok);
  fixture.transport.scriptRx(1, QByteArrayLiteral("after"), 5000);
  const SessionRecordingReport report = fixture.recorder.stop();
  QCOMPARE(report.records, quint64(1));

  const SessionRecordFile file = readSessionRecordFile(fixture.sink.acceptedBytes());
  QVERIFY2(file.loadable, qPrintable(file.error));

  // The header's reference is the anchor, whose session time is exactly 0 ...
  const QJsonArray domains = file.header.value(QStringLiteral("clockDomains")).toArray();
  QCOMPARE(domains.size(), 1);
  const QJsonObject reference = domains.at(0).toObject().value(QStringLiteral("reference")).toObject();
  qint64 referenceSourceNs = -1;
  qint64 referenceSessionNs = -1;
  QVERIFY(sessionJsonIntegerToQInt64(reference.value(QStringLiteral("sourceTimestampNs")), &referenceSourceNs));
  QVERIFY(sessionJsonIntegerToQInt64(reference.value(QStringLiteral("sessionTimestampNs")), &referenceSessionNs));
  QCOMPARE(referenceSourceNs, qint64(1000));
  QCOMPARE(referenceSessionNs, qint64(0));

  // ... while the recorded stream begins well after that anchor.
  QCOMPARE(file.records.at(0).timestampNs, qint64(4000));   // 5000 - 1000
  QVERIFY(file.records.at(0).timestampNs > 0);
  QCOMPARE(report.durationNs, qint64(0));   // the recording's own duration is the clock's

  // The source descriptor points at the same clock domain as the domain entry.
  const QJsonObject source = file.header.value(QStringLiteral("sources")).toArray().at(0).toObject();
  QCOMPARE(source.value(QStringLiteral("clockDomainId")).toString(),
           domains.at(0).toObject().value(QStringLiteral("id")).toString());
  QCOMPARE(source.value(QStringLiteral("sourceId")).toInt(), 1);
}

/** The transport's metadata is stored verbatim, including 64-bit values as
 *  canonical decimal strings. */
void TstSessionRecorder::metadataIsWrittenVerbatim()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/d.kpsession"))).ok);

  // The controller attaches metadata of its own to structural observations; the
  // recorder must not re-encode or drop it. A configuration change carries the
  // largest metadata the session produces.
  QJsonObject metadata;
  metadata.insert(QStringLiteral("applyStatus"), QStringLiteral("full"));
  metadata.insert(QStringLiteral("changedGroups"), QJsonArray({ QStringLiteral("hardware") }));
  metadata.insert(QStringLiteral("futureNs"), sessionJsonInteger(9007199254740993LL));
  fixture.transport.scriptConfigurationChanged(1, 1500, metadata);

  const SessionRecordingReport report = fixture.recorder.stop();
  QCOMPARE(report.records, quint64(1));

  const SessionRecordFile file = readSessionRecordFile(fixture.sink.acceptedBytes());
  QVERIFY2(file.loadable, qPrintable(file.error));
  QCOMPARE(file.records.size(), 1);

  const QByteArray expected = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
  QCOMPARE(file.records.at(0).metadataJson, expected);
  QCOMPARE(file.records.at(0).metadata.value(QStringLiteral("futureNs")).toString(),
           QStringLiteral("9007199254740993"));
}

/** A short write ends the recording as damaged, and the complete prefix stays a
  *  loadable file (ADR-006 ignores the truncated final record). */
void TstSessionRecorder::aShortWriteEndsTheRecordingAsDamagedAndKeepsTheCompletePrefix()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/e.kpsession"))).ok);

  fixture.sink.shortWriteCall = 3;              // the header, one record, then a short one
  QList<SessionRecordingReport> ended;
  QObject::connect(&fixture.recorder, &SessionRecorder::recordingEnded,
                   [&ended](const SessionRecordingReport &report) { ended.append(report); });

  fixture.transport.scriptRx(1, QByteArrayLiteral("one"), 1100);
  fixture.transport.scriptRx(1, QByteArrayLiteral("two"), 1200);
  fixture.transport.scriptRx(1, QByteArrayLiteral("three"), 1300);

  QCOMPARE(ended.size(), 1);
  const SessionRecordingReport damaged = ended.at(0);
  QCOMPARE(damaged.damaged, true);
  QCOMPARE(damaged.records, quint64(1));        // the short record was not accepted
  QVERIFY(damaged.reason.contains(QStringLiteral("incompletely")));
  QCOMPARE(static_cast<int>(fixture.recorder.state()), static_cast<int>(SessionRecorder::State::Damaged));

  // Further events are ignored while the recording is damaged.
  fixture.transport.scriptRx(1, QByteArrayLiteral("four"), 1400);
  QCOMPARE(ended.size(), 1);

  const SessionRecordingReport stopped = fixture.recorder.stop();
  QCOMPARE(stopped.damaged, true);
  QCOMPARE(stopped.records, quint64(1));
  QCOMPARE(static_cast<int>(fixture.recorder.state()), static_cast<int>(SessionRecorder::State::Stopped));
  QCOMPARE(fixture.sink.closeCalls(), 1);

  // The complete prefix is still a valid file; the partial record at the end is
  // exactly the case ADR-006 recovers from by ignoring it.
  const SessionRecordFile file = readSessionRecordFile(fixture.sink.acceptedBytes());
  QVERIFY2(file.loadable, qPrintable(file.error));
  QCOMPARE(file.records.size(), 1);
  QCOMPARE(file.records.at(0).payload, QByteArrayLiteral("one"));
  QCOMPARE(file.truncatedFinalRecord, true);
  QVERIFY(file.bytesConsumed <= fixture.sink.writtenBytes());
}

/** A failed flush is damage: the notification must not be lost. */
void TstSessionRecorder::aFailedFlushEndsTheRecordingAsDamaged()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/f.kpsession"))).ok);

  QList<SessionRecordingReport> ended;
  QObject::connect(&fixture.recorder, &SessionRecorder::recordingEnded,
                   [&ended](const SessionRecordingReport &report) { ended.append(report); });
  fixture.sink.failFlushCall = 2;               // the start flush succeeds, the timer flush fails

  fixture.transport.scriptRx(1, QByteArrayLiteral("data"), 1100);
  QCOMPARE(fixture.scheduler.scheduleCalls(), 1);
  QVERIFY(fixture.scheduler.pending());
  fixture.scheduler.fire();

  QCOMPARE(ended.size(), 1);
  QCOMPARE(ended.at(0).damaged, true);
  QVERIFY(ended.at(0).reason.contains(QStringLiteral("flushed")));
  QCOMPARE(static_cast<int>(fixture.recorder.state()), static_cast<int>(SessionRecorder::State::Damaged));

  // The final stop of a damaged recording does not flush again.
  const int flushesBefore = fixture.sink.flushCalls();
  const SessionRecordingReport stopped = fixture.recorder.stop();
  QCOMPARE(stopped.damaged, true);
  QCOMPARE(fixture.sink.flushCalls(), flushesBefore);
  QCOMPARE(stopped.records, quint64(1));
}

/** The process buffer is bounded: one timer per burst, never re-armed while it
  *  is pending, and the interval is the one the specification fixes. */
void TstSessionRecorder::theProcessBufferIsBoundedByOneTimerPerBurst()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/g.kpsession"))).ok);
  QCOMPARE(fixture.scheduler.scheduleCalls(), 0);   // the header was flushed synchronously

  fixture.transport.scriptRx(1, QByteArrayLiteral("a"), 1100);
  QCOMPARE(fixture.scheduler.scheduleCalls(), 1);
  QCOMPARE(fixture.scheduler.delayMs(), int(SessionRecorder::kFlushIntervalMs));

  // More events in the same burst do not extend the deadline.
  fixture.transport.scriptRx(1, QByteArrayLiteral("b"), 1200);
  fixture.transport.scriptRx(1, QByteArrayLiteral("c"), 1300);
  QCOMPARE(fixture.scheduler.scheduleCalls(), 1);

  // Firing the timer flushes; the next write arms a new one.
  const int flushesBefore = fixture.sink.flushCalls();
  fixture.scheduler.fire();
  QCOMPARE(fixture.sink.flushCalls(), flushesBefore + 1);
  QVERIFY(!fixture.scheduler.pending());
  fixture.transport.scriptRx(1, QByteArrayLiteral("d"), 1400);
  QCOMPARE(fixture.scheduler.scheduleCalls(), 2);
}

/** The recorder is a passive consumer: it never opens, closes or writes the
  *  transport (ADR-007), and it observes only the session event stream. */
void TstSessionRecorder::theRecorderNeverCallsIntoTheTransport()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/h.kpsession"))).ok);
  fixture.transport.scriptRx(1, QByteArrayLiteral("payload"), 1100);
  fixture.recorder.stop();

  QCOMPARE(fixture.transport.callsIntoTheTransport(), 0);
}

/** The report says what was written, and a second recording starts cleanly. */
void TstSessionRecorder::stopReportsAndAllowsASecondRecording()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/i.kpsession"))).ok);
  fixture.transport.scriptRx(1, QByteArrayLiteral("xyz"), 1500);

  const SessionRecordingReport first = fixture.recorder.stop();
  QCOMPARE(first.path, QStringLiteral("/tmp/i.kpsession"));
  QCOMPARE(first.records, quint64(1));
  QCOMPARE(first.bytes, fixture.sink.writtenBytes());
  QCOMPARE(first.damaged, false);
  QVERIFY(first.reason.isEmpty());

  // The reported duration is wall-clock time on the monotonic clock (SPEC-M9 5.6);
  // the session timestamp of the last event is *not* the recording's duration.
  QCOMPARE(first.durationNs, qint64(0));       // the test clock has not advanced

  // A second recording writes a fresh header to a new target.
  const int writesBefore = fixture.sink.writeCalls();
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/j.kpsession"))).ok);
  QCOMPARE(fixture.sink.path(), QStringLiteral("/tmp/j.kpsession"));
  QCOMPARE(fixture.sink.writeCalls(), writesBefore + 1);
  fixture.transport.scriptRx(1, QByteArrayLiteral("later"), 2000);
  const SessionRecordingReport second = fixture.recorder.stop();
  QCOMPARE(second.path, QStringLiteral("/tmp/j.kpsession"));
  QCOMPARE(second.records, quint64(1));

  // Starting a second recording while one runs is refused.
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/k.kpsession"))).ok);
  const SessionRecordingStart again = fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/l.kpsession")));
  QCOMPARE(again.ok, false);
  QVERIFY(again.reason.contains(QStringLiteral("already running")));
}

/** Destruction finalises a running recording silently: flush, close, no signal
  *  and no callback into a half-destroyed object (ADR-010 8). */
void TstSessionRecorder::destroyingARunningRecorderFinalisesWithoutASignal()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  ScriptedSink sink;
  ManualScheduler scheduler;
  ManualClock clock;
  int endedCount = 0;

  {
    std::unique_ptr<SessionRecorder> recorder =
        SessionRecorderSeamsForTest::create(&controller, &sink, &scheduler, &clock);
    transport.scriptOpened(1, 1000);
    QVERIFY(recorder->start(recordingRequest(QStringLiteral("/tmp/m.kpsession"))).ok);
    transport.scriptRx(1, QByteArrayLiteral("tail"), 1100);
    QObject::connect(recorder.get(), &SessionRecorder::recordingEnded,
                     [&endedCount](const SessionRecordingReport &) { ++endedCount; });
    QVERIFY(scheduler.pending());
  }

  QCOMPARE(endedCount, 0);                 // no signal from the destructor
  QCOMPARE(sink.closeCalls(), 1);          // but the file is closed
  QVERIFY(sink.flushCalls() >= 2);         // the start flush and the final one
  QVERIFY(!scheduler.pending());           // and the timer is not left running

  const SessionRecordFile file = readSessionRecordFile(sink.acceptedBytes());
  QVERIFY2(file.loadable, qPrintable(file.error));
  QCOMPARE(file.records.size(), 1);
}

/** The reported duration is the monotonic clock's, from the start flush to the
  *  finalisation - never a session timestamp (SPEC-M9 5.6). */
void TstSessionRecorder::theReportedDurationComesFromTheMonotonicClock()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/n.kpsession"))).ok);

  // Three seconds of recording without a single event: the session time stays 0,
  // the duration does not.
  fixture.clock.advance(3'000'000'000LL);
  const SessionRecordingReport clean = fixture.recorder.stop();
  QCOMPARE(clean.durationNs, qint64(3'000'000'000LL));
  QCOMPARE(clean.records, quint64(0));

  // A damaged recording's duration ends when the failure happened, not when the
  // user finally stops it.
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/o.kpsession"))).ok);
  // The sink counts writes across recordings, so anchor the failure dynamically:
  // this recording's header is already written, the next write is its first record.
  fixture.sink.shortWriteCall = fixture.sink.writeCalls() + 1;
  fixture.clock.advance(1'000'000'000LL);
  fixture.transport.scriptRx(1, QByteArrayLiteral("x"), 2000);
  QCOMPARE(static_cast<int>(fixture.recorder.state()), static_cast<int>(SessionRecorder::State::Damaged));
  QCOMPARE(fixture.recorder.lastReport().durationNs, qint64(1'000'000'000LL));

  fixture.clock.advance(5'000'000'000LL);        // time passes after the damage
  const SessionRecordingReport damaged = fixture.recorder.stop();
  QCOMPARE(damaged.damaged, true);
  QCOMPARE(damaged.durationNs, qint64(1'000'000'000LL));
}

/** A damaged recording is finalised by the next start, so recording can resume
  *  after a failure (SPEC-M9 5.5). */
void TstSessionRecorder::startAfterADamagedRecordingFinalisesItAndStartsANewFile()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/p.kpsession"))).ok);
  fixture.sink.shortWriteCall = 2;
  fixture.transport.scriptRx(1, QByteArrayLiteral("doomed"), 1100);
  QCOMPARE(static_cast<int>(fixture.recorder.state()), static_cast<int>(SessionRecorder::State::Damaged));

  // The next start finalises the damaged recording first, then begins a new one.
  const SessionRecordingStart resumed = fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/q.kpsession")));
  QVERIFY2(resumed.ok, qPrintable(resumed.reason));
  QCOMPARE(static_cast<int>(fixture.recorder.state()), static_cast<int>(SessionRecorder::State::Live));
  QCOMPARE(fixture.recorder.lastReport().damaged, true);   // the damaged one is the record of it
  QCOMPARE(fixture.recorder.lastReport().path, QStringLiteral("/tmp/p.kpsession"));

  fixture.transport.scriptRx(1, QByteArrayLiteral("fine"), 1200);
  const SessionRecordingReport report = fixture.recorder.stop();
  QCOMPARE(report.path, QStringLiteral("/tmp/q.kpsession"));
  QCOMPARE(report.records, quint64(1));
  QCOMPARE(report.damaged, false);

  const SessionRecordFile file = readSessionRecordFile(fixture.sink.acceptedBytes());
  QVERIFY2(file.loadable, qPrintable(file.error));
  QVERIFY(file.truncatedFinalRecord || file.records.size() >= 1);
}

/** A short header write refuses the start and leaves a file that does not load -
  *  the one case ADR-010 section 8 names explicitly. */
void TstSessionRecorder::aShortHeaderWriteRefusesTheStartAndLeavesNoLoadableFile()
{
  Fixture fixture;
  fixture.goLive(1000);
  fixture.sink.shortWriteCall = 1;              // the header itself

  const SessionRecordingStart start = fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/r.kpsession")));
  QCOMPARE(start.ok, false);
  QVERIFY(start.reason.contains(QStringLiteral("incompletely")));
  QVERIFY(start.reason.contains(QStringLiteral("not loadable")));
  QCOMPARE(static_cast<int>(fixture.recorder.state()), static_cast<int>(SessionRecorder::State::Stopped));
  QCOMPARE(fixture.sink.closeCalls(), 1);
  QCOMPARE(fixture.sink.isOpen(), false);

  // No records and no recording: the file that remains is exactly the unloadable
  // artefact the reason warned about.
  const SessionRecordFile file = readSessionRecordFile(fixture.sink.acceptedBytes());
  QCOMPARE(file.loadable, false);
}

/** An event the codec refuses is reported with its sequence number and sizes, and
  *  ends the recording as damaged (SPEC-M9 5.3). The controller never emits such an
  *  event, so the defensive path is driven directly through its signal. */
void TstSessionRecorder::anEventThatCannotBeEncodedReportsItsSequenceAndSizes()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/s.kpsession"))).ok);

  QList<SessionRecordingReport> ended;
  QObject::connect(&fixture.recorder, &SessionRecorder::recordingEnded,
                   [&ended](const SessionRecordingReport &report) { ended.append(report); });

  SessionEvent invalid;
  invalid.sequence = 7;
  invalid.sourceId = 1;
  invalid.type = SessionEventType::Data;
  invalid.direction = SessionDirection::Rx;
  invalid.payload = QByteArray();               // a Data event without payload is invalid
  invalid.sourceTimestampNs = 1500;
  invalid.timestampNs = 500;
  fixture.controller.eventObserved(invalid);    // test-only: a conformant controller never emits this

  QCOMPARE(ended.size(), 1);
  QVERIFY(ended.at(0).damaged);
  QVERIFY2(ended.at(0).reason.contains(QStringLiteral("event 7")), qPrintable(ended.at(0).reason));
  QVERIFY(ended.at(0).reason.contains(QStringLiteral("0 payload bytes")));
  QVERIFY(ended.at(0).reason.contains(QStringLiteral("metadata bytes")));
  QCOMPARE(static_cast<int>(fixture.recorder.state()), static_cast<int>(SessionRecorder::State::Damaged));
  QCOMPARE(fixture.recorder.stop().records, quint64(0));
}

/** The report counts every byte the sink accepted, including a partial tail, so
  *  that `bytes` describes the file on disk (ADR-010, SPEC-M9 5.6). */
void TstSessionRecorder::aShortWriteReportsEveryAcceptedByte()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/t.kpsession"))).ok);
  fixture.transport.scriptRx(1, QByteArrayLiteral("complete"), 1100);

  fixture.sink.shortWriteCall = 3;              // the header, one record, then a partial one
  fixture.transport.scriptRx(1, QByteArrayLiteral("partial"), 1200);

  const SessionRecordingReport damaged = fixture.recorder.lastReport();
  QCOMPARE(damaged.damaged, true);
  QCOMPARE(damaged.records, quint64(1));
  QCOMPARE(damaged.bytes, fixture.sink.writtenBytes());   // the partial byte counts too
  QVERIFY(damaged.reason.contains(QStringLiteral("record 3")));

  const SessionRecordingReport stopped = fixture.recorder.stop();
  QCOMPARE(stopped.bytes, fixture.sink.writtenBytes());
  QCOMPARE(stopped.records, quint64(1));
}

/** An explicit stop that fails its final flush reports the failure in its return
  *  value and emits nothing: the application asked, so it is not interrupted. */
void TstSessionRecorder::anExplicitStopWhoseFinalFlushFailsReportsItWithoutASignal()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/u.kpsession"))).ok);
  fixture.transport.scriptRx(1, QByteArrayLiteral("data"), 1100);

  QList<SessionRecordingReport> ended;
  QObject::connect(&fixture.recorder, &SessionRecorder::recordingEnded,
                   [&ended](const SessionRecordingReport &report) { ended.append(report); });

  fixture.sink.failFlushCall = 2;               // the start flush succeeded, the final one fails
  const SessionRecordingReport stopped = fixture.recorder.stop();

  QCOMPARE(ended.size(), 0);                    // no signal for an explicit stop
  QCOMPARE(stopped.damaged, true);
  QVERIFY(stopped.reason.contains(QStringLiteral("flushed")));
  QCOMPARE(static_cast<int>(fixture.recorder.state()), static_cast<int>(SessionRecorder::State::Stopped));
  QCOMPARE(fixture.sink.closeCalls(), 1);
}

/** The test-side reader must reject malformed writer output, not merely parse it
  *  (SPEC-M9 section 7 lists what it has to validate). */
void TstSessionRecorder::theReaderRejectsMalformedWriterOutput()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/v.kpsession"))).ok);
  fixture.transport.scriptRx(1, QByteArrayLiteral("payload"), 1100);
  fixture.recorder.stop();

  const QByteArray good = fixture.sink.acceptedBytes();
  QVERIFY2(readSessionRecordFile(good).loadable, qPrintable(readSessionRecordFile(good).error));

  const quint32 headerLength = qFromLittleEndian<quint32>(good.constData() + 12);
  const int recordOffset = 16 + int(headerLength);
  QVERIFY(recordOffset + 44 < good.size());

  // Non-zero flags are not v1 (ADR-006).
  QByteArray withFlags = good;
  withFlags[recordOffset + 7] = char(1);
  const SessionRecordFile flagsFile = readSessionRecordFile(withFlags);
  QCOMPARE(flagsFile.loadable, false);
  QVERIFY(flagsFile.error.contains(QStringLiteral("flags")));

  // A record length that does not match its parts.
  QByteArray badLength = good;
  badLength[recordOffset] = char(badLength[recordOffset] - 1);
  const SessionRecordFile lengthFile = readSessionRecordFile(badLength);
  QCOMPARE(lengthFile.loadable, false);
  QVERIFY(lengthFile.error.contains(QStringLiteral("inconsistent")));

  // A reference session time other than zero is not a v1 reference.
  QByteArray wrongReference = good;
  const int referenceIndex = wrongReference.indexOf(QByteArrayLiteral("\"sessionTimestampNs\":\""));
  QVERIFY(referenceIndex > 0);
  const int valueIndex = referenceIndex + int(qstrlen("\"sessionTimestampNs\":\""));
  QCOMPARE(wrongReference.at(valueIndex), '0');
  wrongReference[valueIndex] = '1';
  const SessionRecordFile referenceFile = readSessionRecordFile(wrongReference);
  QCOMPARE(referenceFile.loadable, false);
  QVERIFY(referenceFile.error.contains(QStringLiteral("reference session time")));

  // A truncated final record is the one thing the reader recovers from.
  QByteArray truncated = good;
  truncated.chop(3);
  const SessionRecordFile truncatedFile = readSessionRecordFile(truncated);
  QVERIFY2(truncatedFile.loadable, qPrintable(truncatedFile.error));
  QCOMPARE(truncatedFile.truncatedFinalRecord, true);
  QCOMPARE(truncatedFile.records.size(), 0);
}

/** A monotonic clock may read 0 at the start instant: the duration must still be
  *  measured, and a damage at that instant must freeze it (0 is a value, not an
  *  "unset" marker). */
void TstSessionRecorder::aClockThatStartsAtZeroReportsTheRealDuration()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  ScriptedSink sink;
  ManualScheduler scheduler;
  ManualClock clock(0);
  std::unique_ptr<SessionRecorder> recorder =
      SessionRecorderSeamsForTest::create(&controller, &sink, &scheduler, &clock);

  transport.scriptOpened(1, 1000);
  QVERIFY2(recorder->start(recordingRequest(QStringLiteral("/tmp/w.kpsession"))).ok, "start refused");
  clock.advance(2'000'000'000LL);
  QCOMPARE(recorder->stop().durationNs, qint64(2'000'000'000LL));

  clock.set(0);
  QVERIFY(recorder->start(recordingRequest(QStringLiteral("/tmp/x.kpsession"))).ok);
  sink.shortWriteCall = sink.writeCalls() + 1;      // the first record of this recording
  transport.scriptRx(1, QByteArrayLiteral("x"), 2000);
  QCOMPARE(static_cast<int>(recorder->state()), static_cast<int>(SessionRecorder::State::Damaged));
  QCOMPARE(recorder->lastReport().durationNs, qint64(0));

  clock.advance(9'000'000'000LL);                   // long after the damage
  const SessionRecordingReport damaged = recorder->stop();
  QCOMPARE(damaged.durationNs, qint64(0));          // frozen at the damage, not 9 s
  QCOMPARE(damaged.damaged, true);
}

/** The start flush must reach the operating system: a second handle sees a
  *  complete header while the recording is still running (ADR-010 4). This is the
  *  one recorder test that uses the production sink and a real file. */
void TstSessionRecorder::theStartFlushMakesTheHeaderVisibleThroughASecondHandle()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("live.kpsession"));

  ScriptedTransport transport;
  SessionController controller(&transport);
  SessionRecorder recorder(&controller);            // production sink, scheduler and clock
  transport.scriptOpened(1, 1000);
  const SessionRecordingStart started = recorder.start(recordingRequest(path));
  QVERIFY2(started.ok, qPrintable(started.reason));

  QFile whileRecording(path);
  QVERIFY(whileRecording.open(QIODevice::ReadOnly));
  const QByteArray onDisk = whileRecording.readAll();
  whileRecording.close();
  if (onDisk.isEmpty()) {
    // A one-line diagnostic instead of a bare failure: the reader would only say
    // "shorter than a start block".
    QFAIL("the start flush left nothing in the file");
  }
  const SessionRecordFile early = readSessionRecordFile(onDisk);
  QVERIFY2(early.loadable, qPrintable(early.error));
  QVERIFY(early.records.isEmpty());
  QCOMPARE(early.bytesConsumed, qint64(onDisk.size()));

  transport.scriptRx(1, QByteArrayLiteral("live bytes"), 1100);
  const SessionRecordingReport report = recorder.stop();
  QCOMPARE(report.records, quint64(1));

  QFile afterwards(path);
  QVERIFY(afterwards.open(QIODevice::ReadOnly));
  const SessionRecordFile after = readSessionRecordFile(afterwards.readAll());
  QVERIFY2(after.loadable, qPrintable(after.error));
  QCOMPARE(after.records.size(), 1);
  QCOMPARE(after.records.at(0).payload, QByteArrayLiteral("live bytes"));
}

/** The reader's profile validation rejects the variants a sloppy writer would
  *  produce: numbers where the profile demands decimal strings, non-canonical
  *  spellings, a creation time that is not ISO-8601 UTC, a wrong source ID. */
void TstSessionRecorder::theReaderRejectsNonConformantProfileVariants()
{
  Fixture fixture;
  fixture.goLive(1000);
  QVERIFY(fixture.recorder.start(recordingRequest(QStringLiteral("/tmp/y.kpsession"))).ok);
  fixture.transport.scriptRx(1, QByteArrayLiteral("p"), 1100);
  fixture.recorder.stop();

  const QJsonObject good = readSessionRecordFile(fixture.sink.acceptedBytes()).header;
  QVERIFY2(profileViolation(good).isEmpty(), qPrintable(profileViolation(good)));

  const auto withReference = [](const QJsonObject &header, const QJsonValue &value) {
    QJsonArray domains = header.value(QStringLiteral("clockDomains")).toArray();
    QJsonObject domain = domains.at(0).toObject();
    QJsonObject reference = domain.value(QStringLiteral("reference")).toObject();
    reference.insert(QStringLiteral("sessionTimestampNs"), value);
    domain.insert(QStringLiteral("reference"), reference);
    domains.replace(0, domain);
    QJsonObject result = header;
    result.insert(QStringLiteral("clockDomains"), domains);
    return result;
  };

  // A JSON number where the profile requires the decimal string "0".
  QVERIFY(profileViolation(withReference(good, 0)).contains(QStringLiteral("canonical")));
  // A non-canonical spelling of a number.
  QVERIFY(profileViolation(withReference(good, QStringLiteral("00"))).contains(QStringLiteral("canonical")));
  QVERIFY(profileViolation(withReference(good, QStringLiteral("+5"))).contains(QStringLiteral("canonical")));
  QVERIFY(profileViolation(withReference(good, QStringLiteral(" 5"))).contains(QStringLiteral("canonical")));
  // A reference session time that is not zero.
  QVERIFY(profileViolation(withReference(good, QStringLiteral("5"))).contains(QStringLiteral("reference session time")));

  // The creation time must be a valid ISO-8601 UTC timestamp.
  QJsonObject withoutZone = good;
  withoutZone.insert(QStringLiteral("created"), QStringLiteral("2026-09-18T21:00:00"));
  QVERIFY(profileViolation(withoutZone).contains(QStringLiteral("ISO-8601")));
  QJsonObject nonsense = good;
  nonsense.insert(QStringLiteral("created"), QStringLiteral("not a timestamp"));
  QVERIFY(profileViolation(nonsense).contains(QStringLiteral("ISO-8601")));

  // A source ID other than 1 belongs to no v1 profile.
  QJsonArray sources = good.value(QStringLiteral("sources")).toArray();
  QJsonObject source = sources.at(0).toObject();
  source.insert(QStringLiteral("sourceId"), 2);
  sources.replace(0, source);
  QJsonObject wrongSource = good;
  wrongSource.insert(QStringLiteral("sources"), sources);
  QVERIFY(profileViolation(wrongSource).contains(QStringLiteral("source ID")));

  // Fixed numeric members hold exactly the fixed number: a fractional value is not
  // accepted just because a truncating conversion would make it fit.
  QJsonObject fractionalSource = good;
  QJsonArray fractionalSources = fractionalSource.value(QStringLiteral("sources")).toArray();
  QJsonObject fractionalEntry = fractionalSources.at(0).toObject();
  fractionalEntry.insert(QStringLiteral("sourceId"), 1.5);
  fractionalSources.replace(0, fractionalEntry);
  fractionalSource.insert(QStringLiteral("sources"), fractionalSources);
  QVERIFY(profileViolation(fractionalSource).contains(QStringLiteral("source ID")));

  QJsonObject fractionalVersion = good;
  fractionalVersion.insert(QStringLiteral("version"), 1.5);
  QVERIFY(profileViolation(fractionalVersion).contains(QStringLiteral("version")));

  // A version written as a string is not a number either.
  QJsonObject stringVersion = good;
  stringVersion.insert(QStringLiteral("version"), QStringLiteral("1"));
  QVERIFY(profileViolation(stringVersion).contains(QStringLiteral("version")));
}

QTEST_MAIN(TstSessionRecorder)

#include "tst_sessionrecorder.moc"
