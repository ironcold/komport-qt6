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

SessionReplayPlayer::OperationSnapshot SessionReplayPlayer::captureSnapshot() const
{
  OperationSnapshot snapshot;
  snapshot.scheduleToken = mScheduleToken;
  snapshot.replayId = mReplayId;
  snapshot.session = mSession;
  snapshot.state = mState;
  snapshot.position = mPosition;
  snapshot.delivered = mDelivered;
  snapshot.timing = mTiming;
  return snapshot;
}

bool SessionReplayPlayer::OperationSnapshot::sameReplay(const SessionReplayPlayer &player) const
{
  return replayId == player.mReplayId && session == player.mSession;
}

quint64 SessionReplayPlayer::OperationSnapshot::eventCount() const
{
  return session ? quint64(session->events.size()) : 0;
}

bool SessionReplayPlayer::OperationSnapshot::deliveryIsTheLastOne() const
{
  const quint64 total = eventCount();
  return total > 0 && position + 1 >= total;
}

SessionReplayReport SessionReplayPlayer::OperationSnapshot::report() const
{
  SessionReplayReport result;
  result.delivered = delivered;
  result.position = position;
  const quint64 total = eventCount();
  result.remaining = total > position ? total - position : 0;
  result.finished = (state == SessionReplayState::Finished) || (total > 0 && position >= total);
  result.timing = timing;
  return result;
}

SessionReplayReport SessionReplayPlayer::OperationSnapshot::completionReport() const
{
  SessionReplayReport result;
  result.delivered = delivered + 1;
  result.position = position + 1;
  const quint64 total = eventCount();
  result.remaining = total > position + 1 ? total - (position + 1) : 0;
  result.finished = true;
  result.timing = timing;
  return result;
}

bool SessionReplayPlayer::mayMutateReplay(const OperationSnapshot &snapshot, MutationPhase phase,
                                          quint64 expectedPosition) const
{
  // SPEC-M10 5.11's predicate, and the only authorization in this class. `snapshot`
  // is always the operation's *entry* snapshot: a snapshot taken after a receiver
  // acted would compare equal to the live player in every respect and make this a
  // tautology (step 3b's design note, finding 3).
  if (!snapshot.sameReplay(*this))
    return false;                       // the replay the operation started on is released or replaced
  if (snapshot.scheduleToken != mScheduleToken && phase != MutationPhase::Complete)
    return false;                       // its pending schedule was cancelled (rule 10)
  // `expectedPosition` is consulted by `StepContinue` only, where the progress is
  // inherently the caller's (the step's own delivered count). `Arm` and `Continue`
  // have a fixed relation to the entry snapshot, and the helper derives it rather
  // than trusting a caller (3b's review, finding 2).
  switch (phase) {
  case MutationPhase::Arm:
    return mState == SessionReplayState::Playing && mPosition == snapshot.position;
  case MutationPhase::Continue:
    return mState == SessionReplayState::Playing && mPosition == snapshot.position + 1;
  case MutationPhase::StepContinue:
    return mState == snapshot.state && mPosition == expectedPosition;
  case MutationPhase::Complete:
    // The completion of a replay that delivered its last event outranks a `stop()`
    // in that delivery (rule 1), so the token is exempt - but never the identity.
    return mState == SessionReplayState::Playing || mState == SessionReplayState::Paused;
  case MutationPhase::Finish:
    return mState == snapshot.state && snapshot.eventCount() > 0
        && mPosition >= snapshot.eventCount();
  }
  return false;
}

SessionReplayStart SessionReplayPlayer::start(const SessionFilePtr &session)
{
  SessionReplayStart result;
  if (!session) {
    result.reason = QStringLiteral("no session was given");
    return result;
  }
  // The explicit restart: a running replay stops first, the delivered count and
  // the position go back to the beginning, and nothing is re-armed. Both
  // identities change, so every continuation armed for the old replay is refused
  // from here on (SPEC-M10 5.11 rule 2).
  ++mScheduleToken;
  ++mReplayId;
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

  const OperationSnapshot snapshot = captureSnapshot();   // before the state signal
  setState(SessionReplayState::Playing);

  // `stateChanged` is synchronous: a receiver may have closed, restarted or
  // stopped the player. `play()` has succeeded either way - it entered `Playing` -
  // but it must not arm a delivery the receiver just cancelled (rule 4).
  if (!mayMutateReplay(snapshot, MutationPhase::Arm, 0)) {
    result.ok = true;
    return result;
  }
  scheduleNextDelivery();
  result.ok = true;
  return result;
}

SessionReplayReport SessionReplayPlayer::stop()
{
  if (mState == SessionReplayState::Playing) {
    // A canceller does not authorize its own cancellation (rule 10): cancelling
    // is not a mutation, and it must work in any state. It bumps the schedule
    // token and cancels before it changes the state, and the report it returns is
    // the one of the replay it stopped, captured before that state signal.
    const OperationSnapshot stopped = captureSnapshot();
    ++mScheduleToken;
    mScheduler->cancelPending();
    setState(SessionReplayState::Paused);
    return stopped.report();
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
  ++mScheduleToken;
  ++mReplayId;
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
  const quint64 scheduleToken = mScheduleToken;
  const quint64 replayId = mReplayId;
  mScheduler->scheduleAfter(delayNsFor(mPosition),
                            [this, scheduleToken, replayId]() { onDeliveryDue(scheduleToken, replayId); });
}

void SessionReplayPlayer::onDeliveryDue(quint64 scheduleToken, quint64 replayId)
{
  const OperationSnapshot snapshot = captureSnapshot();   // before the first emission
  if (snapshot.replayId != replayId || snapshot.scheduleToken != scheduleToken)
    return;                       // a stale continuation: this callback's schedule is gone (rule 4)

  // Whether this delivery is the last of *this* replay, and the completion report
  // itself, are snapshot facts: a receiver of the delivery can neither report a
  // completion that belongs to another replay nor suppress the one this one owes.
  const bool lastEventOfThisReplay = snapshot.deliveryIsTheLastOne();
  const SessionReplayReport finishedReport = snapshot.completionReport();

  deliverAtCurrentPosition(snapshot);

  if (lastEventOfThisReplay && mayMutateReplay(snapshot, MutationPhase::Complete, 0)) {
    setState(SessionReplayState::Finished);
    emit replayFinished(finishedReport);
    return;
  }
  if (!mayMutateReplay(snapshot, MutationPhase::Continue, 0))
    return;
  scheduleNextDelivery();
}

void SessionReplayPlayer::deliverAtCurrentPosition(const OperationSnapshot &snapshot)
{
  const SessionEvent &event = snapshot.session->events.at(qsizetype(mPosition));
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
  const OperationSnapshot entry = captureSnapshot();   // fixes the result and the reports
  const quint64 startPosition = entry.position;
  quint64 advanced = 0;

  while (qint64(startPosition + advanced) <= matchIndex) {
    // The step's own entry snapshot authorizes every delivery, with the position
    // this continuation owns spelled out: after `advanced` deliveries that is
    // `startPosition + advanced` (SPEC-M10 5.11, `StepContinue`).
    if (!mayMutateReplay(entry, MutationPhase::StepContinue, startPosition + advanced))
      break;
    deliverAtCurrentPosition(entry);
    ++advanced;
  }

  const quint64 streamSize = entry.eventCount();
  const bool completed = advanced == quint64(matchIndex) - startPosition + 1;
  result.ok = true;
  result.advanced = advanced;
  result.matched = matched && completed;
  result.reachedEnd = completed && streamSize > 0 && startPosition + advanced >= streamSize;
  if (result.reachedEnd && mayMutateReplay(entry, MutationPhase::Finish, 0))
    setState(SessionReplayState::Finished);
  return result;
}
