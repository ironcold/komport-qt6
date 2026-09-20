/***************************************************************************
                   sessionreplayplayer.cpp  -  Komport Serial Port Communicator
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

#include "sessionreplayplayer.h"

#include <QElapsedTimer>
#include <QTimer>

#include <limits>

// SPEC-M10 sections 5.5 and 5.6, decision by decision:
//
//  - exactly one delivery per scheduled callback, always through the scheduler
//    seam, including `Immediate` with a zero delay, so the event loop turns
//    between two events and `stop()` is honoured at a delivery boundary;
//  - every delay comes from the stored session timeline (`timestampNs`), never
//    from wall-clock time, and stored timing is never rewritten;
//  - the first event after a `start()`/restart has no predecessor: delay 0;
//  - scaling up uses checked multiplication and clamps, scaling down truncates;
//  - a step is synchronous, delivers the inclusive prefix and arms no timer;
//  - every refusal is a returned value carrying a reason and changes nothing.

namespace {

/** The production scheduler: one single-shot `QTimer` on the player's thread.
  *
  * Qt's timer is millisecond-based while the stored timeline is nanoseconds, so
  * the interval rounds *up*: a positive delay never collapses into a zero-delay
  * storm, and `Immediate` stays zero so the event loop still turns between two
  * deliveries (ADR-005: real-world precision is best-effort).
  *
  * A delay longer than a single `QTimer` can express is waited out in
  * consecutive chunks rather than shortened: Qt's millisecond interval is an
  * `int`, and scheduling a 30-day delay as 24.8 days would fire the replay far
  * too early - the specified delay is the stored one (review: slice-2
  * implementation review, finding 2).
  */
class TimerScheduler : public SessionReplayScheduler
{
public:
  explicit TimerScheduler(QObject *context) : mContext(context)
  {
    mTimer.setSingleShot(true);
    QObject::connect(&mTimer, &QTimer::timeout, mContext, [this]() { onChunkElapsed(); });
  }

  void scheduleAfter(qint64 delayNs, std::function<void()> callback) override
  {
    mCallback = std::move(callback);
    startChunk(delayNs);
  }

  void cancelPending() override
  {
    mTimer.stop();
    mCallback = nullptr;
    mRemainingNs = 0;
    mChunkMs = 0;
  }

private:
  /** The largest millisecond interval a QTimer can be asked for. */
  static constexpr qint64 kMaxChunkMs = qint64(std::numeric_limits<int>::max());

  void startChunk(qint64 remainingNs)
  {
    mRemainingNs = remainingNs;
    qint64 milliseconds = 0;
    if (remainingNs > 0) {
      milliseconds = remainingNs / 1000000;
      if (remainingNs % 1000000 != 0)
        ++milliseconds;   // round up
      if (milliseconds > kMaxChunkMs)
        milliseconds = kMaxChunkMs;
    }
    mChunkMs = milliseconds;
    mTimer.start(int(milliseconds));
  }

  void onChunkElapsed()
  {
    mRemainingNs -= mChunkMs * Q_INT64_C(1000000);
    if (mRemainingNs > 0) {
      startChunk(mRemainingNs);   // wait out the rest of the same delay
      return;
    }
    const std::function<void()> callback = mCallback;
    mCallback = nullptr;
    if (callback)
      callback();
  }

  QObject *mContext;
  QTimer mTimer;
  std::function<void()> mCallback;
  qint64 mRemainingNs = 0;
  qint64 mChunkMs = 0;
};

/** The production monotonic clock (SPEC-M10 5.10), M9's pattern: a monotonic
  * `QElapsedTimer`, never wall-clock time - a wall clock can jump.
  *
  * The player never consults it: every delay is derived from the stored session
  * timeline. It exists so that the seam is complete and a test can inject a
  * counting clock and prove that no time source is consulted (ADR-005).
  */
class ElapsedMonotonicClock : public SessionMonotonicClock
{
public:
  ElapsedMonotonicClock() { mTimer.start(); }
  qint64 nowNs() override { return mTimer.nsecsElapsed(); }

private:
  QElapsedTimer mTimer;
};

/** A scaling-up factor applied with checked arithmetic, clamped instead of
  * wrapping (SPEC-M10 5.6). */
qint64 scaledUp(qint64 value, qint64 factor)
{
  if (value <= 0)
    return 0;
  constexpr qint64 kMax = std::numeric_limits<qint64>::max();
  if (value > kMax / factor)
    return kMax;
  return value * factor;
}

} // namespace

SessionReplayPlayer::SessionReplayPlayer(QObject *parent)
    : SessionReplayPlayer(nullptr, nullptr, parent)
{
}

SessionReplayPlayer::SessionReplayPlayer(SessionReplayScheduler *scheduler,
                                        SessionMonotonicClock *clock, QObject *parent)
    : QObject(parent), mScheduler(scheduler), mClock(clock)
{
  if (mScheduler == nullptr) {
    // The timer's context is this object, so the connection dies with the
    // player and a pending callback can never outlive it.
    mOwnedScheduler = std::make_unique<TimerScheduler>(this);
    mScheduler = mOwnedScheduler.get();
  }
  if (mClock == nullptr) {
    mOwnedClock = std::make_unique<ElapsedMonotonicClock>();
    mClock = mOwnedClock.get();
  }
}

SessionReplayPlayer::~SessionReplayPlayer()
{
  // Stop the timer and drop a pending callback; emit nothing (SPEC-M10 5.5).
  mScheduler->cancelPending();
}

SessionReplayStart SessionReplayPlayer::start(const SessionFilePtr &session)
{
  SessionReplayStart result;
  if (!session) {
    result.reason = QStringLiteral("no session was given");
    return result;
  }

  // The explicit restart: a running replay stops first, the delivered count and
  // the position go back to the beginning, and nothing is re-armed. SPEC-M10
  // 5.10 guarantees that the cancelled callback is never invoked.
  mScheduler->cancelPending();
  mSession = session;
  mPosition = 0;
  mDelivered = 0;
  setState(SessionReplayState::Ready);
  result.ok = true;
  return result;
}

SessionReplayStart SessionReplayPlayer::play()
{
  SessionReplayStart result;
  if (!mSession) {
    result.reason = QStringLiteral("no session is loaded");
    return result;
  }
  if (mSession->events.isEmpty()) {
    result.reason = QStringLiteral("the session has no events");
    return result;
  }
  if (mPosition >= quint64(mSession->events.size())) {
    result.reason = QStringLiteral("the replay is finished - restart it to replay again");
    return result;
  }
  if (mTiming == SessionReplayTiming::Step) {
    result.reason = QStringLiteral("step mode advances only on a step command");
    return result;
  }
  if (mState == SessionReplayState::Playing) {
    // Not an operation of a running replay (SPEC-M10 5.5's state table lists
    // stop(), start(), close() and setTiming() for `Playing`). Refusing it keeps
    // the rule "a refusal changes nothing" literal: accepting it would re-arm
    // the pending delivery with a fresh delay and so stretch the replay.
    result.reason = QStringLiteral("the replay is already running");
    return result;
  }

  setState(SessionReplayState::Playing);
  scheduleNextDelivery();
  result.ok = true;
  return result;
}

SessionReplayReport SessionReplayPlayer::stop()
{
  if (mState == SessionReplayState::Playing) {
    mScheduler->cancelPending();
    setState(SessionReplayState::Paused);
    return report();
  }
  // A stop in Idle, Ready, Paused or Finished is a no-op report, not an error,
  // and it emits nothing (SPEC-M10 5.5).
  return report();
}

SessionReplayStep SessionReplayPlayer::stepNextEvent()
{
  const QString refusal = stepRefusal();
  SessionReplayStep result;
  if (!refusal.isEmpty()) {
    result.reason = refusal;
    return result;
  }
  // The next event, unconditionally: the match is the event at the position.
  return performStep(qint64(mPosition), true);
}

SessionReplayStep SessionReplayPlayer::stepNextRx()
{
  return stepToMatch(SessionDirection::Rx);
}

SessionReplayStep SessionReplayPlayer::stepNextTx()
{
  return stepToMatch(SessionDirection::Tx);
}

void SessionReplayPlayer::close()
{
  mScheduler->cancelPending();
  mSession.reset();
  mPosition = 0;
  mDelivered = 0;
  setState(SessionReplayState::Idle);
}

SessionReplayState SessionReplayPlayer::state() const
{
  return mState;
}

SessionReplayTiming SessionReplayPlayer::timing() const
{
  return mTiming;
}

SessionReplayTimingChange SessionReplayPlayer::setTiming(SessionReplayTiming timing)
{
  SessionReplayTimingChange result;
  result.timing = mTiming;

  if (!isDeclaredTiming(timing)) {
    // Only reachable through a cast; a refusal is still a value, never silence.
    result.reason = QStringLiteral("unknown timing policy");
    return result;
  }
  if (mState == SessionReplayState::Playing && timing == SessionReplayTiming::Step) {
    // Automatic advancement and step advancement must not both own the position
    // (ADR-011 D7): the state, the policy and a pending delivery are untouched.
    result.reason = QStringLiteral("pause the replay before selecting step mode");
    return result;
  }

  mTiming = timing;
  result.ok = true;
  result.timing = timing;
  return result;
}

quint64 SessionReplayPlayer::position() const
{
  return mPosition;
}

quint64 SessionReplayPlayer::deliveredCount() const
{
  return mDelivered;
}

quint64 SessionReplayPlayer::eventCount() const
{
  return mSession ? quint64(mSession->events.size()) : 0;
}

qint64 SessionReplayPlayer::delayNsFor(quint64 index) const
{
  if (!mSession || index == 0 || index >= quint64(mSession->events.size()))
    return 0;

  const qint64 gap = mSession->events.at(qsizetype(index)).timestampNs
      - mSession->events.at(qsizetype(index - 1)).timestampNs;
  // The loader guarantees a non-decreasing stream, but a hand-built session in a
  // test may not: a zero or negative gap delivers with delay 0, in stored order.
  const qint64 stored = qMax<qint64>(0, gap);

  switch (mTiming) {
    case SessionReplayTiming::Original:
    case SessionReplayTiming::Scale1:
      return stored;
    case SessionReplayTiming::Immediate:
      return 0;
    case SessionReplayTiming::Scale0_1:
      return scaledUp(stored, 10);
    case SessionReplayTiming::Scale0_25:
      return scaledUp(stored, 4);
    case SessionReplayTiming::Scale0_5:
      return scaledUp(stored, 2);
    case SessionReplayTiming::Scale2:
      return stored / 2;
    case SessionReplayTiming::Scale5:
      return stored / 5;
    case SessionReplayTiming::Scale10:
      return stored / 10;
    case SessionReplayTiming::Step:
      break;   // never scheduled: play() and selecting Step while Playing refuse
  }
  return 0;
}

void SessionReplayPlayer::scheduleNextDelivery()
{
  mScheduler->scheduleAfter(delayNsFor(mPosition), [this]() { onDeliveryDue(); });
}

void SessionReplayPlayer::onDeliveryDue()
{
  // No state, session or identity check here on purpose: SPEC-M10 5.10 guarantees
  // that a callback cancelled by stop(), close() or start() is never invoked, so
  // under a valid mutation context this callback always belongs to a replay that
  // is still playing. A guard would be mutation-authorization logic in the core
  // (step 3a's review gate, finding 1).
  deliverAtCurrentPosition();
  if (mPosition >= quint64(mSession->events.size())) {
    setState(SessionReplayState::Finished);
    emit replayFinished(report());
    return;
  }
  scheduleNextDelivery();
}

void SessionReplayPlayer::deliverAtCurrentPosition()
{
  const SessionEvent &event = mSession->events.at(qsizetype(mPosition));
  ++mPosition;
  ++mDelivered;
  emit eventDelivered(event);
}

SessionReplayReport SessionReplayPlayer::report() const
{
  SessionReplayReport result;
  result.delivered = mDelivered;
  result.position = mPosition;
  const quint64 total = eventCount();
  result.remaining = total > mPosition ? total - mPosition : 0;
  // A session without events is never "finished": nothing was replayable, which
  // is why play() and the steps refuse it instead.
  result.finished = (mState == SessionReplayState::Finished)
      || (total > 0 && mPosition >= total);
  result.timing = mTiming;
  return result;
}

void SessionReplayPlayer::setState(SessionReplayState state)
{
  if (mState == state)
    return;
  mState = state;
  emit stateChanged(state);
}

bool SessionReplayPlayer::isDeclaredTiming(SessionReplayTiming timing)
{
  switch (timing) {
    case SessionReplayTiming::Original:
    case SessionReplayTiming::Immediate:
    case SessionReplayTiming::Scale0_1:
    case SessionReplayTiming::Scale0_25:
    case SessionReplayTiming::Scale0_5:
    case SessionReplayTiming::Scale1:
    case SessionReplayTiming::Scale2:
    case SessionReplayTiming::Scale5:
    case SessionReplayTiming::Scale10:
    case SessionReplayTiming::Step:
      return true;
  }
  return false;
}

QString SessionReplayPlayer::stepRefusal() const
{
  if (!mSession)
    return QStringLiteral("no session is loaded");
  if (mSession->events.isEmpty())
    return QStringLiteral("the session has no events");
  if (mState == SessionReplayState::Playing)
    return QStringLiteral("stop the replay before stepping");
  if (mPosition >= quint64(mSession->events.size()))
    return QStringLiteral("the replay is finished - restart it to replay again");
  return QString();
}

SessionReplayStep SessionReplayPlayer::stepToMatch(SessionDirection direction)
{
  const QString refusal = stepRefusal();
  SessionReplayStep result;
  if (!refusal.isEmpty()) {
    result.reason = refusal;
    return result;
  }

  qint64 match = -1;
  for (quint64 index = mPosition; index < quint64(mSession->events.size()); ++index) {
    const SessionEvent &event = mSession->events.at(qsizetype(index));
    if (event.type == SessionEventType::Data && event.direction == direction) {
      match = qint64(index);
      break;
    }
  }

  const bool matched = match >= 0;
  if (!matched) {
    // No match: advance to the end, deliver the remaining events and report it.
    match = qint64(mSession->events.size()) - 1;
  }
  return performStep(match, matched);
}

SessionReplayStep SessionReplayPlayer::performStep(qint64 matchIndex, bool matched)
{
  SessionReplayStep result;
  const quint64 startPosition = mPosition;
  while (qint64(mPosition) <= matchIndex)
    deliverAtCurrentPosition();

  const quint64 total = mSession ? quint64(mSession->events.size()) : 0;
  result.ok = true;
  result.advanced = mPosition - startPosition;
  result.matched = matched;
  result.reachedEnd = total > 0 && mPosition >= total;
  if (result.reachedEnd)
    setState(SessionReplayState::Finished);
  return result;
}
