/***************************************************************************
                       sessionrecorder.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de
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

#include "sessionrecordcodec.h"
#include "sessionrecorderseams.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QTimer>

namespace {

/** The production sink: a plain write-only `QFile` (SPEC-M9 section 5.9). */
class FileRecordSink : public SessionRecordSink
{
public:
  bool open(const QString &path) override
  {
    mFile.setFileName(path);
    return mFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
  }

  qint64 write(const QByteArray &bytes) override { return mFile.write(bytes); }
  bool flush() override { return mFile.flush(); }
  void close() override
  {
    if (mFile.isOpen())
      mFile.close();
  }

private:
  QFile mFile;
};

/** The production scheduler: a single-shot `QTimer` on the recorder's thread.
  *
  * A pending schedule is replaced rather than duplicated, which is what keeps the
  * flush rate at one per burst (ADR-010 D2). */
class TimerFlushScheduler : public SessionFlushScheduler
{
public:
  explicit TimerFlushScheduler(QObject *context)
  : mTimer(new QTimer(context))
  {
    mTimer->setSingleShot(true);
    QObject::connect(mTimer, &QTimer::timeout, mTimer, [this]() {
      std::function<void()> callback = std::move(mCallback);
      mCallback = nullptr;
      if (callback)
        callback();
    });
  }

  void schedule(int milliseconds, std::function<void()> callback) override
  {
    mCallback = std::move(callback);
    mTimer->start(milliseconds);
  }

  void cancel() override
  {
    mTimer->stop();
    mCallback = nullptr;
  }

private:
  QTimer *mTimer;
  std::function<void()> mCallback;
};

/** The production monotonic clock: a steady timer, nanosecond resolution. */
class ElapsedMonotonicClock : public SessionMonotonicClock
{
public:
  ElapsedMonotonicClock() { mTimer.start(); }
  qint64 nowNs() override { return mTimer.nsecsElapsed(); }

private:
  QElapsedTimer mTimer;
};

} // namespace

SessionRecorder::SessionRecorder(SessionController *_controller, QObject *parent)
: QObject(parent)
, mController(_controller)
, mSink(nullptr)
, mScheduler(nullptr)
, mClock(nullptr)
{
  mOwnedSink = std::make_unique<FileRecordSink>();
  mOwnedScheduler = std::make_unique<TimerFlushScheduler>(this);
  mOwnedClock = std::make_unique<ElapsedMonotonicClock>();
  mSink = mOwnedSink.get();
  mScheduler = mOwnedScheduler.get();
  mClock = mOwnedClock.get();

  QObject::connect(mController, &SessionController::eventObserved, this,
                   &SessionRecorder::onEventObserved);
}

SessionRecorder::SessionRecorder(SessionController *_controller, SessionRecordSink *_sink,
                                 SessionFlushScheduler *_scheduler, SessionMonotonicClock *_clock)
: QObject(nullptr)
, mController(_controller)
, mSink(_sink)
, mScheduler(_scheduler)
, mClock(_clock)
{
  Q_ASSERT(mSink != nullptr);
  Q_ASSERT(mScheduler != nullptr);
  Q_ASSERT(mClock != nullptr);

  QObject::connect(mController, &SessionController::eventObserved, this,
                   &SessionRecorder::onEventObserved);
}

SessionRecorder::~SessionRecorder()
{
  // Finalise what is open, and nothing else: no signal, no report (ADR-010 8).
  mScheduler->cancel();
  if (mFileOpen) {
    mSink->flush();
    mSink->close();
    mFileOpen = false;
  }
}

SessionRecordingStart SessionRecorder::start(const SessionRecordingRequest &request)
{
  SessionRecordingStart result;

  // SPEC-M9 5.5: a damaged recording is finalised first, so recording can be
  // resumed after a failure without restarting the application.
  if (mState == State::Damaged)
    stop();

  if (mState != State::Stopped) {
    result.reason = QStringLiteral("a recording is already running");
    return result;
  }

  // ADR-010 4: recording starts only while the session is live. That is what makes
  // the header writable at start - there is no waiting state before a first event,
  // and no state in which recording is on but nothing has been decided.
  if (mController->state() != SessionController::State::Live) {
    result.reason = QStringLiteral("the session is not live; recording needs an open session");
    return result;
  }

  const SessionClockDomainReference reference = mController->clockDomainReference();
  if (!reference.valid) {
    result.reason = QStringLiteral("the clock domain has no anchor yet");
    return result;
  }

  // Assemble and validate the header before touching the target: a refused start
  // must not leave a truncated or unreadable file behind (SPEC-M9 5.4/5.6).
  SessionHeaderFacts facts;
  facts.applicationName = request.applicationName;
  facts.applicationVersion = request.applicationVersion;
  facts.createdUtc = QDateTime::currentDateTimeUtc();
  facts.configuration = request.configuration;
  facts.referenceSourceTimestampNs = reference.sourceTimestampNs;

  const SessionRecordEncoding header = SessionRecordCodec::encodeHeader(facts);
  if (!header.ok) {
    result.reason = header.reason;
    return result;
  }

  if (!mSink->open(request.path)) {
    result.reason = QStringLiteral("the target file cannot be opened");
    return result;
  }
  mFileOpen = true;

  const WriteOutcome written = writeUnit(header.bytes);
  if (!written.complete) {
    // A failed or short header write is the one case that may leave an unloadable
    // file: ADR-006 fails loading on an invalid header rather than recovering it.
    mSink->close();
    mFileOpen = false;
    result.reason = written.accepted <= 0
                        ? QStringLiteral("the header could not be written")
                        : QStringLiteral("the header was written incompletely (%1 of %2 bytes); "
                                         "the file is not loadable")
                              .arg(QString::number(written.accepted),
                                   QString::number(header.bytes.size()));
    return result;
  }

  // The start flush of ADR-010 4: the header is outside this process's buffer from
  // the first moment, so an otherwise idle recording still has its complete header.
  if (!mSink->flush()) {
    mSink->close();
    mFileOpen = false;
    result.reason = QStringLiteral("the header could not be flushed; the file may be incomplete");
    return result;
  }

  mState = State::Live;
  mPath = request.path;
  mRecords = 0;
  mBytes = qint64(header.bytes.size());
  mLastSessionTimestampNs = 0;
  mFlushPending = false;
  mEndNs = 0;
  mStartNs = mClock->nowNs();   // the duration starts after the successful start flush
  mRecordingStarted = true;     // 0 is a valid instant, so this flag, not the value,
                                // decides whether a recording is being measured

  result.ok = true;
  return result;
}

void SessionRecorder::onEventObserved(const SessionEvent &_event)
{
  if (mState != State::Live)
    return;

  const SessionRecordEncoding record = SessionRecordCodec::encodeRecord(_event);
  if (!record.ok) {
    // An event that cannot be written conformantly is not data to persist, and the
    // report names it unambiguously: its sequence number and its sizes (SPEC-M9 5.3).
    // The codec's own reason carries the violated limit.
    const QString reason =
        QStringLiteral("event %1 could not be encoded (%2 payload bytes, %3 metadata bytes): %4")
            .arg(QString::number(_event.sequence), QString::number(_event.payload.size()),
                 QString::number(SessionRecordCodec::compactJsonSize(_event.metadata)),
                 record.reason);
    endDamaged(reason);
    return;
  }

  const WriteOutcome written = writeUnit(record.bytes);
  // Every byte the sink accepted counts, including a partial tail: the report must
  // describe what is on disk, not what was intended (ADR-010, SPEC-M9 5.6).
  mBytes += written.accepted;
  if (!written.complete) {
    endDamaged(QStringLiteral("record %1 was written incompletely (%2 of %3 bytes)")
                   .arg(QString::number(_event.sequence), QString::number(written.accepted),
                        QString::number(record.bytes.size())));
    return;
  }

  ++mRecords;
  mLastSessionTimestampNs = _event.timestampNs;

  // Bound the process buffer: the first unflushed write arms one timer, so a burst
  // flushes at most once per second and a quiet line is still flushed in time.
  armFlush();
}

SessionRecorder::WriteOutcome SessionRecorder::writeUnit(const QByteArray &_bytes)
{
  const qint64 accepted = mSink->write(_bytes);

  WriteOutcome outcome;
  outcome.accepted = qMax<qint64>(0, accepted);
  outcome.complete = (accepted == qint64(_bytes.size()));
  return outcome;
}

void SessionRecorder::armFlush()
{
  if (mFlushPending)
    return;

  mFlushPending = true;
  mScheduler->schedule(kFlushIntervalMs, [this]() { onFlushTimer(); });
}

void SessionRecorder::onFlushTimer()
{
  mFlushPending = false;
  if (mState != State::Live)
    return;

  if (!mSink->flush())
    endDamaged(QStringLiteral("the recorded data could not be flushed"));
}

qint64 SessionRecorder::durationNs() const
{
  if (!mRecordingStarted)
    return 0;
  // A damaged recording's duration ends when the failure happened, not when the
  // application finally stops it; `mState` decides which instant applies, because
  // 0 is a valid reading of the clock.
  const qint64 end = (mState == State::Damaged) ? mEndNs : mClock->nowNs();
  return qMax<qint64>(0, end - mStartNs);
}

void SessionRecorder::clearRecordingState()
{
  mPath.clear();
  mRecords = 0;
  mBytes = 0;
  mLastSessionTimestampNs = 0;
  mStartNs = 0;
  mEndNs = 0;
  mRecordingStarted = false;
  mFlushPending = false;
}

void SessionRecorder::endDamaged(const QString &_reason)
{
  // The recording ends here, but the file is finalised by the next stop(): the
  // damaged state of SPEC-M9 5.5 is observable, and its transition to stopped is
  // the report finalisation. Nothing is flushed again - the sink already failed
  // once, and the complete prefix stays valid data.
  mScheduler->cancel();
  mFlushPending = false;
  mEndNs = mClock->nowNs();
  mState = State::Damaged;

  SessionRecordingReport report;
  report.path = mPath;
  report.records = mRecords;
  report.bytes = mBytes;
  report.durationNs = durationNs();
  report.damaged = true;
  report.reason = _reason;

  mLastReport = report;
  emit recordingEnded(report);
}

SessionRecordingReport SessionRecorder::stop()
{
  if (mState == State::Stopped)
    return SessionRecordingReport();

  const bool damaged = (mState == State::Damaged);
  mScheduler->cancel();
  mFlushPending = false;

  bool finalFlushFailed = false;
  if (mFileOpen) {
    if (!damaged)
      finalFlushFailed = !mSink->flush();
    mSink->close();
    mFileOpen = false;
  }

  SessionRecordingReport report;
  report.path = mPath;
  report.records = mRecords;
  report.bytes = mBytes;
  report.durationNs = durationNs();
  report.damaged = damaged || finalFlushFailed;
  report.reason = damaged ? mLastReport.reason
                          : (finalFlushFailed
                                 ? QStringLiteral("the recorded data could not be flushed")
                                 : (mRecords == 0
                                        // SPEC-M9 5.6: a recording without a single event
                                        // says so rather than looking like a silent success.
                                        ? QStringLiteral("no session data recorded")
                                        : QString()));

  mState = State::Stopped;
  mLastReport = report;
  clearRecordingState();
  return report;
}
