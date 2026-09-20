/***************************************************************************
                    tst_sessionreplayplayer.cpp  -  Komport Serial Port Communicator
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

// The M10 replay player (ADR-011 D6-D10, SPEC-M10 5.5/5.6/5.10 and the player
// table of SPEC-M10 section 7).
//
// The player is driven through its two seams: the scripted scheduler records
// every requested delay and fires the callbacks explicitly, so the delay
// arithmetic is asserted exactly instead of being waited for, and the counting
// clock proves that the player never consults wall-clock time (ADR-005).
// Fixtures are session values built here, so the player's slice does not depend
// on the reader.

#include "sessionreplayplayer.h"

#include "sessionreader.h"   // SessionFile, SessionFilePtr, SessionEvent
#include "sessionevent.h"

#include <QList>
#include <QTest>

#include <functional>
#include <limits>
#include <memory>

/** The one permitted way to construct a player with the SPEC-M10 5.10 seams: the
  *  injection constructor is private, because the seams are not public API. */
struct SessionReplayPlayerSeamsForTest
{
  static std::unique_ptr<SessionReplayPlayer> create(SessionReplayScheduler *scheduler,
                                                     SessionMonotonicClock *clock)
  {
    return std::unique_ptr<SessionReplayPlayer>(new SessionReplayPlayer(scheduler, clock, nullptr));
  }
};

namespace {

/** A scripted scheduler: it records every delay it is asked for, replaces its
  *  pending callback like the production timer does, and is fired explicitly by
  *  the test - so no wall-clock time is involved anywhere. */
class ScriptedScheduler : public SessionReplayScheduler
{
public:
  QList<qint64> delays;        ///< every requested delay, in order
  qint64 pendingDelayNs = 0;   ///< the delay currently armed
  int cancelCalls = 0;

  void scheduleAfter(qint64 delayNs, std::function<void()> callback) override
  {
    delays.append(delayNs);
    pendingDelayNs = delayNs;
    mCallback = std::move(callback);
    mArmed = true;
  }

  void cancelPending() override
  {
    ++cancelCalls;
    mCallback = nullptr;
    mArmed = false;
    pendingDelayNs = 0;
  }

  bool armed() const { return mArmed; }

  /** Run the armed callback; false when nothing is armed. */
  bool fire()
  {
    if (!mArmed)
      return false;
    const std::function<void()> callback = mCallback;
    mCallback = nullptr;
    mArmed = false;
    pendingDelayNs = 0;
    callback();
    return true;
  }

private:
  std::function<void()> mCallback;
  bool mArmed = false;
};

/** A clock that counts: the player must never consult it (ADR-005). */
class CountingClock : public SessionMonotonicClock
{
public:
  int calls = 0;
  qint64 nowNs() override
  {
    ++calls;
    return 0;
  }
};

/** A Data event. */
SessionEvent dataEvent(quint64 sequence, SessionDirection direction, const QByteArray &payload,
                       qint64 timestampNs, qint64 sourceTimestampNs = 1000)
{
  SessionEvent event;
  event.sequence = sequence;
  event.sourceId = 1;
  event.sourceTimestampNs = sourceTimestampNs;
  event.timestampNs = timestampNs;
  event.type = SessionEventType::Data;
  event.direction = direction;
  event.payload = payload;
  return event;
}

/** A non-data event. */
SessionEvent nonDataEvent(SessionEventType type, quint64 sequence, qint64 timestampNs)
{
  SessionEvent event;
  event.sequence = sequence;
  event.sourceId = 1;
  event.sourceTimestampNs = 1000;
  event.timestampNs = timestampNs;
  event.type = type;
  event.direction = SessionDirection::None;
  return event;
}

/** One immutable session value of the shape the reader hands out. */
SessionFilePtr makeSession(const QList<SessionEvent> &events)
{
  auto session = std::make_shared<SessionFile>();
  session->info.version = 1;
  session->info.source.sourceId = 1;
  session->info.source.clockDomainId = QStringLiteral("local-process-monotonic-v1");
  session->events = events;
  return session;
}

/** Tx A, non-data, Rx B, Tx C - the Rx sits in the middle, so a step to it is not
  *  also the end of the stream. */
SessionFilePtr mixedSession()
{
  return makeSession(QList<SessionEvent>{
    dataEvent(1, SessionDirection::Tx, QByteArrayLiteral("A"), 0),
    nonDataEvent(SessionEventType::Annotation, 2, 250),
    dataEvent(3, SessionDirection::Rx, QByteArrayLiteral("B"), 1000),
    dataEvent(4, SessionDirection::Tx, QByteArrayLiteral("C"), 1500),
  });
}

/** Three Data events with strictly increasing session time. */
SessionFilePtr threeEventSession()
{
  return makeSession(QList<SessionEvent>{
    dataEvent(1, SessionDirection::Tx, QByteArrayLiteral("A"), 0),
    dataEvent(2, SessionDirection::Rx, QByteArrayLiteral("B"), 1000),
    dataEvent(3, SessionDirection::Tx, QByteArrayLiteral("C"), 2000),
  });
}

/** The player with its scripted seams, plus every observation a test needs. */
struct PlayerFixture {
  ScriptedScheduler scheduler;
  CountingClock clock;
  std::unique_ptr<SessionReplayPlayer> owned{
      SessionReplayPlayerSeamsForTest::create(&scheduler, &clock) };
  SessionReplayPlayer &player = *owned;

  QList<SessionEvent> delivered;          ///< every delivered event, in order
  QList<SessionReplayState> states;       ///< every emitted state
  /** The emission order, for the ordering claims of §5.5/§5.6. These receivers
    * only record; none of them calls back into the player (that is step 3b). */
  QStringList signalLog;
  int finishedCount = 0;
  SessionReplayReport lastReport;

  PlayerFixture()
  {
    QObject::connect(&player, &SessionReplayPlayer::eventDelivered,
                     [this](const SessionEvent &event) {
                       signalLog.append(QStringLiteral("event:%1").arg(event.sequence));
                       delivered.append(event);
                     });
    QObject::connect(&player, &SessionReplayPlayer::stateChanged,
                     [this](SessionReplayState state) {
                       signalLog.append(QStringLiteral("state:%1").arg(int(state)));
                       states.append(state);
                     });
    QObject::connect(&player, &SessionReplayPlayer::replayFinished,
                     [this](const SessionReplayReport &report) {
                       signalLog.append(QStringLiteral("finished"));
                       ++finishedCount;
                       lastReport = report;
                     });
  }

  /** Fire the pending callback until nothing is armed. */
  void runToEnd()
  {
    while (scheduler.fire()) {
    }
  }
};

} // namespace

class TstSessionReplayPlayer : public QObject
{
  Q_OBJECT

private slots:
  void startPositionsAtTheFirstEventAndNeverMutatesTheSession();
  void playDeliversEveryEventInStoredOrder();
  void deliveredEventsAreTheStoredEvents();
  void immediateTimingSchedulesOneDeliveryPerCallback();
  void originalTimingUsesTheStoredSessionTimeGaps();
  void scaleOneEqualsOriginalTiming();
  void eachScaleMultipliesOrDividesTheStoredGap();
  void equalStoredTimestampsDeliverWithoutDelayInOrder();
  void changingTimingAppliesToTheNextEventOnly();
  void playingIsRefusedInStepMode();
  void selectingStepWhilePlayingIsRefused();
  void stepDeliversTheInclusivePrefixAndTheMatch();
  void stepNextRxDeliversEveryEventUpToAndIncludingTheNextRx();
  void stepNextTxDeliversEveryEventUpToAndIncludingTheNextTx();
  void stepPastTheLastMatchAdvancesToTheEndAndReportsNoMatch();
  void stepIsRefusedWhilePlaying();
  void stepAfterFinishIsRefused();
  void stopPreservesPositionAndCount();
  void playResumesFromThePreservedPosition();
  void replayFinishedReportsDeliveredRemainingAndTiming();
  void restartReplaysTheWholeStreamAgain();
  void closeReleasesTheSessionAndReturnsToIdle();
  void closeWhilePlayingCancelsAndReturnsToIdle();
  void zeroEventSessionRefusesPlayAndStep();
  void refusalsAreValuesThatChangeNothing();
  void thePlayerNeverConsultsWallClockTime();
  void theTimingMatrixAndEveryRefusalLeaveThePlayerUnchanged();
  void stepNextEventDeliversExactlyTheNextEvent();
  void scalingUpClampsInsteadOfWrapping();
};

/** `start()` attaches the session, positions at the first event, resets the count
  *  and never mutates the stored session - and it is the explicit restart. */
void TstSessionReplayPlayer::startPositionsAtTheFirstEventAndNeverMutatesTheSession()
{
  PlayerFixture f;
  const SessionFilePtr session = threeEventSession();

  const SessionReplayStart started = f.player.start(session);
  QVERIFY2(started.ok, qPrintable(started.reason));
  QCOMPARE(f.player.state(), SessionReplayState::Ready);
  QCOMPARE(f.player.position(), quint64(0));
  QCOMPARE(f.player.deliveredCount(), quint64(0));
  QCOMPARE(f.player.eventCount(), quint64(3));
  QVERIFY(!f.scheduler.armed());   // start() arms nothing

  QVERIFY(f.player.play().ok);
  f.runToEnd();
  QCOMPARE(f.player.state(), SessionReplayState::Finished);

  // The stored session is exactly what it was.
  QCOMPARE(session->events.size(), 3);
  QCOMPARE(session->events.at(0).payload, QByteArrayLiteral("A"));
  QCOMPARE(session->events.at(1).sequence, quint64(2));
  QCOMPARE(session->events.at(2).timestampNs, qint64(2000));

  // The explicit restart resets the position and the count.
  QVERIFY(f.player.start(session).ok);
  QCOMPARE(f.player.state(), SessionReplayState::Ready);
  QCOMPARE(f.player.position(), quint64(0));
  QCOMPARE(f.player.deliveredCount(), quint64(0));

  // A null session is the one thing start() refuses.
  const SessionReplayStart refused = f.player.start(SessionFilePtr());
  QVERIFY(!refused.ok);
  QVERIFY(!refused.reason.isEmpty());
}

/** Every stored event is delivered exactly once, in stored order. */
void TstSessionReplayPlayer::playDeliversEveryEventInStoredOrder()
{
  PlayerFixture f;
  const SessionFilePtr session = makeSession(QList<SessionEvent>{
    dataEvent(1, SessionDirection::Tx, QByteArrayLiteral("A"), 0),
    dataEvent(2, SessionDirection::Rx, QByteArrayLiteral("B"), 100),
    dataEvent(3, SessionDirection::Tx, QByteArrayLiteral("C"), 200),
    nonDataEvent(SessionEventType::Bookmark, 4, 300),
    dataEvent(5, SessionDirection::Rx, QByteArrayLiteral("D"), 400),
  });
  QVERIFY(f.player.start(session).ok);
  QVERIFY(f.player.play().ok);
  f.runToEnd();

  QCOMPARE(f.delivered.size(), 5);
  for (int index = 0; index < f.delivered.size(); ++index)
    QCOMPARE(f.delivered.at(index).sequence, quint64(index + 1));
  QCOMPARE(f.finishedCount, 1);
}

/** The delivered events are the stored values, field by field. */
void TstSessionReplayPlayer::deliveredEventsAreTheStoredEvents()
{
  PlayerFixture f;
  SessionEvent withMetadata = dataEvent(1, SessionDirection::Tx, QByteArrayLiteral("A"), 0);
  withMetadata.metadata.insert(QStringLiteral("note"), QStringLiteral("kept"));
  const SessionFilePtr session = makeSession(QList<SessionEvent>{
    withMetadata,
    dataEvent(2, SessionDirection::Rx, QByteArray("\x00\xff\x01", 3), 100, 4242),
  });
  QVERIFY(f.player.start(session).ok);
  QVERIFY(f.player.play().ok);
  f.runToEnd();

  QCOMPARE(f.delivered.size(), 2);
  for (int index = 0; index < f.delivered.size(); ++index) {
    const SessionEvent &stored = session->events.at(index);
    const SessionEvent &delivered = f.delivered.at(index);
    QCOMPARE(delivered.sequence, stored.sequence);
    QCOMPARE(delivered.sourceId, stored.sourceId);
    QCOMPARE(delivered.sourceTimestampNs, stored.sourceTimestampNs);
    QCOMPARE(delivered.timestampNs, stored.timestampNs);
    QCOMPARE(delivered.type, stored.type);
    QCOMPARE(delivered.direction, stored.direction);
    QCOMPARE(delivered.payload, stored.payload);
    QCOMPARE(delivered.metadata, stored.metadata);
  }
  QCOMPARE(f.delivered.at(0).metadata.value(QStringLiteral("note")).toString(),
           QStringLiteral("kept"));
  QCOMPARE(f.delivered.at(1).payload.size(), 3);
}

/** `Immediate` still goes through the scheduler: one event per callback, never a
  *  synchronous loop, and the delay is zero. */
void TstSessionReplayPlayer::immediateTimingSchedulesOneDeliveryPerCallback()
{
  PlayerFixture f;
  QVERIFY(f.player.start(threeEventSession()).ok);
  QVERIFY(f.player.setTiming(SessionReplayTiming::Immediate).ok);
  QVERIFY(f.player.play().ok);

  QVERIFY(f.scheduler.armed());
  QCOMPARE(f.delivered.size(), 0);   // nothing is delivered synchronously
  QVERIFY(f.scheduler.fire());
  QCOMPARE(f.delivered.size(), 1);
  QVERIFY(f.scheduler.armed());
  QVERIFY(f.scheduler.fire());
  QCOMPARE(f.delivered.size(), 2);
  QVERIFY(f.scheduler.fire());
  QCOMPARE(f.delivered.size(), 3);
  QCOMPARE(f.scheduler.delays, QList<qint64>({ 0, 0, 0 }));
}

/** `Original` schedules exactly the stored session-time gaps. */
void TstSessionReplayPlayer::originalTimingUsesTheStoredSessionTimeGaps()
{
  PlayerFixture f;
  const SessionFilePtr session = makeSession(QList<SessionEvent>{
    dataEvent(1, SessionDirection::Tx, QByteArrayLiteral("A"), 100),
    dataEvent(2, SessionDirection::Rx, QByteArrayLiteral("B"), 250),
    dataEvent(3, SessionDirection::Tx, QByteArrayLiteral("C"), 1000),
  });
  QVERIFY(f.player.start(session).ok);
  QCOMPARE(f.player.timing(), SessionReplayTiming::Original);
  QVERIFY(f.player.play().ok);
  f.runToEnd();

  // The first event after a start has no predecessor: delay 0, then the stored
  // gaps 150 ns and 750 ns.
  QCOMPARE(f.scheduler.delays, QList<qint64>({ 0, 150, 750 }));
}

/** `Scale1` is `Original` by definition (ADR-011 D7); a test pins it. */
void TstSessionReplayPlayer::scaleOneEqualsOriginalTiming()
{
  PlayerFixture f;
  const SessionFilePtr session = makeSession(QList<SessionEvent>{
    dataEvent(1, SessionDirection::Tx, QByteArrayLiteral("A"), 10),
    dataEvent(2, SessionDirection::Rx, QByteArrayLiteral("B"), 1010),
    dataEvent(3, SessionDirection::Tx, QByteArrayLiteral("C"), 5010),
  });

  QVERIFY(f.player.start(session).ok);
  QVERIFY(f.player.play().ok);
  f.runToEnd();
  const QList<qint64> originalDelays = f.scheduler.delays;

  QVERIFY(f.player.start(session).ok);
  QVERIFY(f.player.setTiming(SessionReplayTiming::Scale1).ok);
  QVERIFY(f.player.play().ok);
  f.runToEnd();

  QCOMPARE(f.scheduler.delays.mid(originalDelays.size()), originalDelays);
}

/** Every ladder entry scales the stored gap as the table fixes it - up with
  *  checked multiplication, down with truncation. */
void TstSessionReplayPlayer::eachScaleMultipliesOrDividesTheStoredGap()
{
  const struct {
    SessionReplayTiming timing;
    qint64 expectedFor1000;
    qint64 expectedFor7;
  } cases[] = {
    { SessionReplayTiming::Original, 1000, 7 },
    { SessionReplayTiming::Immediate, 0, 0 },
    { SessionReplayTiming::Scale0_1, 10000, 70 },
    { SessionReplayTiming::Scale0_25, 4000, 28 },
    { SessionReplayTiming::Scale0_5, 2000, 14 },
    { SessionReplayTiming::Scale1, 1000, 7 },
    { SessionReplayTiming::Scale2, 500, 3 },
    { SessionReplayTiming::Scale5, 200, 1 },
    { SessionReplayTiming::Scale10, 100, 0 },
  };

  for (const auto &oneCase : cases) {
    for (const qint64 gap : { qint64(1000), qint64(7) }) {
      PlayerFixture f;
      const SessionFilePtr session = makeSession(QList<SessionEvent>{
        dataEvent(1, SessionDirection::Tx, QByteArrayLiteral("A"), 0),
        dataEvent(2, SessionDirection::Rx, QByteArrayLiteral("B"), gap),
      });
      QVERIFY(f.player.start(session).ok);
      QVERIFY(f.player.setTiming(oneCase.timing).ok);
      QVERIFY(f.player.play().ok);
      f.runToEnd();

      QCOMPARE(f.scheduler.delays.size(), 2);
      QCOMPARE(f.scheduler.delays.at(0), qint64(0));
      QCOMPARE(f.scheduler.delays.at(1),
               gap == 1000 ? oneCase.expectedFor1000 : oneCase.expectedFor7);
      QVERIFY(f.scheduler.delays.at(1) >= 0);
    }
  }
}

/** Equal stored timestamps deliver with delay 0 in stored order, one event per
  *  callback (no busy loop, no reordering). */
void TstSessionReplayPlayer::equalStoredTimestampsDeliverWithoutDelayInOrder()
{
  PlayerFixture f;
  const SessionFilePtr session = makeSession(QList<SessionEvent>{
    dataEvent(1, SessionDirection::Tx, QByteArrayLiteral("A"), 500),
    dataEvent(2, SessionDirection::Rx, QByteArrayLiteral("B"), 500),
    dataEvent(3, SessionDirection::Tx, QByteArrayLiteral("C"), 500),
  });
  QVERIFY(f.player.start(session).ok);
  QVERIFY(f.player.play().ok);

  QCOMPARE(f.delivered.size(), 0);
  for (int expected = 1; expected <= 3; ++expected) {
    QVERIFY(f.scheduler.armed());
    QVERIFY(f.scheduler.fire());
    QCOMPARE(f.delivered.size(), expected);
  }
  QCOMPARE(f.scheduler.delays, QList<qint64>({ 0, 0, 0 }));
  QCOMPARE(f.delivered.at(0).sequence, quint64(1));
  QCOMPARE(f.delivered.at(1).sequence, quint64(2));
  QCOMPARE(f.delivered.at(2).sequence, quint64(3));
}

/** A timing change while `Playing` applies to the next scheduled delivery: the
  *  delay already being waited out is not re-computed, and stored timing is never
  *  touched. */
void TstSessionReplayPlayer::changingTimingAppliesToTheNextEventOnly()
{
  PlayerFixture f;
  QVERIFY(f.player.start(threeEventSession()).ok);
  QVERIFY(f.player.play().ok);
  QCOMPARE(f.scheduler.delays, QList<qint64>({ 0 }));

  QVERIFY(f.scheduler.fire());   // event 1 delivered; the next delay is armed
  QCOMPARE(f.scheduler.delays, QList<qint64>({ 0, 1000 }));
  QCOMPARE(f.scheduler.pendingDelayNs, qint64(1000));

  const int schedulesBefore = f.scheduler.delays.size();
  QVERIFY(f.player.setTiming(SessionReplayTiming::Scale2).ok);
  QCOMPARE(f.scheduler.delays.size(), schedulesBefore);   // not re-computed
  QCOMPARE(f.scheduler.pendingDelayNs, qint64(1000));

  QVERIFY(f.scheduler.fire());   // event 2; the change applies from here on
  QCOMPARE(f.scheduler.delays, QList<qint64>({ 0, 1000, 500 }));
  QCOMPARE(f.player.timing(), SessionReplayTiming::Scale2);
}

/** `play()` while `Step` is the policy is refused with a reason and arms
  *  nothing (ADR-011 D7). */
void TstSessionReplayPlayer::playingIsRefusedInStepMode()
{
  PlayerFixture f;
  QVERIFY(f.player.start(threeEventSession()).ok);
  QVERIFY(f.player.setTiming(SessionReplayTiming::Step).ok);

  const SessionReplayStart refused = f.player.play();
  QVERIFY(!refused.ok);
  QVERIFY(refused.reason.contains(QStringLiteral("step mode")));
  QCOMPARE(f.player.state(), SessionReplayState::Ready);
  QVERIFY(!f.scheduler.armed());
  QCOMPARE(f.delivered.size(), 0);
}

/** Selecting `Step` while a replay runs is refused as a value, changes nothing -
  *  and there is no implicit pause (review round 1 finding 4; the value-returned
  *  refusal is review round 2 finding 1). */
void TstSessionReplayPlayer::selectingStepWhilePlayingIsRefused()
{
  PlayerFixture f;
  QVERIFY(f.player.start(threeEventSession()).ok);
  QVERIFY(f.player.play().ok);
  QCOMPARE(f.player.state(), SessionReplayState::Playing);

  const qint64 armedDelay = f.scheduler.pendingDelayNs;
  const int schedulesBefore = f.scheduler.delays.size();
  const SessionReplayTimingChange refused = f.player.setTiming(SessionReplayTiming::Step);
  QVERIFY(!refused.ok);
  QCOMPARE(refused.reason, QStringLiteral("pause the replay before selecting step mode"));
  QCOMPARE(refused.timing, SessionReplayTiming::Original);   // the policy in effect
  QCOMPARE(f.player.timing(), SessionReplayTiming::Original);
  QCOMPARE(f.player.state(), SessionReplayState::Playing);
  QCOMPARE(f.scheduler.pendingDelayNs, armedDelay);           // untouched
  QCOMPARE(f.scheduler.delays.size(), schedulesBefore);

  // The pending delivery still happens, so the refusal left no hole.
  QVERIFY(f.scheduler.fire());
  QCOMPARE(f.delivered.size(), 1);

  // Pausing first makes the same selection succeed.
  f.player.stop();
  QCOMPARE(f.player.state(), SessionReplayState::Paused);
  const SessionReplayTimingChange accepted = f.player.setTiming(SessionReplayTiming::Step);
  QVERIFY(accepted.ok);
  QCOMPARE(accepted.timing, SessionReplayTiming::Step);
}

/** A step delivers every event up to and including the match, and the match is
  *  the last one delivered. */
void TstSessionReplayPlayer::stepDeliversTheInclusivePrefixAndTheMatch()
{
  PlayerFixture f;
  QVERIFY(f.player.start(mixedSession()).ok);

  // The next Rx is the third event (index 2): everything before it is delivered.
  const SessionReplayStep step = f.player.stepNextRx();
  QVERIFY2(step.ok, qPrintable(step.reason));
  QCOMPARE(step.advanced, quint64(3));
  QVERIFY(step.matched);
  QVERIFY(!step.reachedEnd);
  QCOMPARE(f.player.position(), quint64(3));
  QCOMPARE(f.delivered.size(), 3);
  QCOMPARE(f.delivered.at(0).sequence, quint64(1));
  QCOMPARE(f.delivered.at(1).sequence, quint64(2));
  QCOMPARE(f.delivered.at(2).sequence, quint64(3));
  QCOMPARE(f.delivered.at(2).direction, SessionDirection::Rx);
  QVERIFY(!f.scheduler.armed());   // a step arms no timer
}

/** `stepNextRx()` skips over the intervening events without dropping them. */
void TstSessionReplayPlayer::stepNextRxDeliversEveryEventUpToAndIncludingTheNextRx()
{
  PlayerFixture f;
  const SessionFilePtr session = makeSession(QList<SessionEvent>{
    dataEvent(1, SessionDirection::Tx, QByteArrayLiteral("A"), 0),
    dataEvent(2, SessionDirection::Tx, QByteArrayLiteral("B"), 10),
    dataEvent(3, SessionDirection::Rx, QByteArrayLiteral("C"), 20),
    dataEvent(4, SessionDirection::Rx, QByteArrayLiteral("D"), 30),
  });
  QVERIFY(f.player.start(session).ok);

  const SessionReplayStep step = f.player.stepNextRx();
  QVERIFY(step.ok);
  QCOMPARE(step.advanced, quint64(3));
  QVERIFY(step.matched);
  QCOMPARE(f.delivered.size(), 3);
  QCOMPARE(f.delivered.at(2).payload, QByteArrayLiteral("C"));

  // The next call matches the following Rx.
  const SessionReplayStep second = f.player.stepNextRx();
  QCOMPARE(second.advanced, quint64(1));
  QVERIFY(second.matched);
  QVERIFY(second.reachedEnd);
  QCOMPARE(f.delivered.at(3).payload, QByteArrayLiteral("D"));
  QCOMPARE(f.player.state(), SessionReplayState::Finished);
}

/** `stepNextTx()` behaves the same way in the other direction. */
void TstSessionReplayPlayer::stepNextTxDeliversEveryEventUpToAndIncludingTheNextTx()
{
  PlayerFixture f;
  QVERIFY(f.player.start(mixedSession()).ok);

  const SessionReplayStep step = f.player.stepNextTx();
  QVERIFY(step.ok);
  QCOMPARE(step.advanced, quint64(1));   // the Tx is the first event
  QVERIFY(step.matched);
  QCOMPARE(f.delivered.size(), 1);
  QCOMPARE(f.delivered.at(0).direction, SessionDirection::Tx);
  QCOMPARE(f.player.position(), quint64(1));
}

/** A step that finds no match advances to the end, delivers the rest and reports
  *  it - and the end of a step is reported by its value, not by `replayFinished`. */
void TstSessionReplayPlayer::stepPastTheLastMatchAdvancesToTheEndAndReportsNoMatch()
{
  PlayerFixture f;
  const SessionFilePtr session = makeSession(QList<SessionEvent>{
    dataEvent(1, SessionDirection::Tx, QByteArrayLiteral("A"), 0),
    dataEvent(2, SessionDirection::Tx, QByteArrayLiteral("B"), 10),
    dataEvent(3, SessionDirection::Tx, QByteArrayLiteral("C"), 20),
  });
  QVERIFY(f.player.start(session).ok);

  const SessionReplayStep step = f.player.stepNextRx();
  QVERIFY(step.ok);
  QVERIFY(!step.matched);
  QVERIFY(step.reachedEnd);
  QCOMPARE(step.advanced, quint64(3));
  QCOMPARE(f.delivered.size(), 3);
  QCOMPARE(f.player.state(), SessionReplayState::Finished);
  QCOMPARE(f.finishedCount, 0);
}

/** A step while `Playing` is refused, delivers nothing and leaves the pending
  *  delivery in place. */
void TstSessionReplayPlayer::stepIsRefusedWhilePlaying()
{
  PlayerFixture f;
  QVERIFY(f.player.start(threeEventSession()).ok);
  QVERIFY(f.player.play().ok);

  const SessionReplayStep refused = f.player.stepNextEvent();
  QVERIFY(!refused.ok);
  QVERIFY(refused.reason.contains(QStringLiteral("stop the replay")));
  QCOMPARE(refused.advanced, quint64(0));
  QCOMPARE(f.delivered.size(), 0);
  QCOMPARE(f.player.position(), quint64(0));
  QVERIFY(f.scheduler.armed());
}

/** `Finished` refuses a step, and only an explicit restart recovers it. */
void TstSessionReplayPlayer::stepAfterFinishIsRefused()
{
  PlayerFixture f;
  const SessionFilePtr session = threeEventSession();
  QVERIFY(f.player.start(session).ok);
  QVERIFY(f.player.play().ok);
  f.runToEnd();
  QCOMPARE(f.player.state(), SessionReplayState::Finished);

  const SessionReplayStep refused = f.player.stepNextEvent();
  QVERIFY(!refused.ok);
  QVERIFY(refused.reason.contains(QStringLiteral("restart")));
  QCOMPARE(f.delivered.size(), 3);

  QVERIFY(f.player.start(session).ok);
  QCOMPARE(f.player.state(), SessionReplayState::Ready);
  QVERIFY(f.player.stepNextEvent().ok);
}

/** `stop()` cancels the pending delivery, keeps position and count, returns a
  *  report, emits no `eventDelivered` and no `replayFinished`, and announces the
  *  `Playing -> Paused` transition through `stateChanged` (§5.5). */
void TstSessionReplayPlayer::stopPreservesPositionAndCount()
{
  PlayerFixture f;
  QVERIFY(f.player.start(threeEventSession()).ok);
  QVERIFY(f.player.play().ok);
  QVERIFY(f.scheduler.fire());
  QCOMPARE(f.delivered.size(), 1);

  const SessionReplayReport report = f.player.stop();
  QCOMPARE(f.player.state(), SessionReplayState::Paused);
  QCOMPARE(report.delivered, quint64(1));
  QCOMPARE(report.remaining, quint64(2));
  QCOMPARE(report.position, quint64(1));
  QVERIFY(!report.finished);
  QCOMPARE(report.timing, SessionReplayTiming::Original);
  QCOMPARE(f.player.position(), quint64(1));
  QCOMPARE(f.player.deliveredCount(), quint64(1));
  QVERIFY(!f.scheduler.armed());
  QCOMPARE(f.delivered.size(), 1);   // nothing was delivered by the stop
  QCOMPARE(f.finishedCount, 0);      // and no completion was reported
  // The transition itself is announced - §5.5's table makes Playing -> Paused a
  // state change like any other, and only event deliveries and completions are
  // excluded.
  QVERIFY(f.states.contains(SessionReplayState::Paused));
  QVERIFY(!f.signalLog.isEmpty());
  QCOMPARE(f.signalLog.last(), QStringLiteral("state:%1").arg(int(SessionReplayState::Paused)));

  // A stop while Paused is a no-op report, not an error.
  const SessionReplayReport again = f.player.stop();
  QCOMPARE(again.position, quint64(1));
  QCOMPARE(f.player.state(), SessionReplayState::Paused);
}

/** Resuming continues at the preserved position; no event is delivered twice. */
void TstSessionReplayPlayer::playResumesFromThePreservedPosition()
{
  PlayerFixture f;
  QVERIFY(f.player.start(threeEventSession()).ok);
  QVERIFY(f.player.play().ok);
  QVERIFY(f.scheduler.fire());
  f.player.stop();

  QVERIFY(f.player.play().ok);
  f.runToEnd();
  QCOMPARE(f.delivered.size(), 3);
  QCOMPARE(f.delivered.at(0).sequence, quint64(1));
  QCOMPARE(f.delivered.at(1).sequence, quint64(2));
  QCOMPARE(f.delivered.at(2).sequence, quint64(3));
  QCOMPARE(f.finishedCount, 1);
  QCOMPARE(f.lastReport.delivered, quint64(3));
  QCOMPARE(f.lastReport.remaining, quint64(0));
}

/** A replay that ends by itself reports it through `replayFinished`. */
void TstSessionReplayPlayer::replayFinishedReportsDeliveredRemainingAndTiming()
{
  PlayerFixture f;
  QVERIFY(f.player.start(threeEventSession()).ok);
  QVERIFY(f.player.setTiming(SessionReplayTiming::Scale2).ok);
  QVERIFY(f.player.play().ok);
  f.runToEnd();

  QCOMPARE(f.finishedCount, 1);
  QCOMPARE(f.lastReport.delivered, quint64(3));
  QCOMPARE(f.lastReport.remaining, quint64(0));
  QCOMPARE(f.lastReport.position, quint64(3));
  QVERIFY(f.lastReport.finished);
  QCOMPARE(f.lastReport.timing, SessionReplayTiming::Scale2);
  QCOMPARE(f.player.state(), SessionReplayState::Finished);
  QVERIFY(!f.scheduler.armed());

  // The completion report follows the state change (the order §5.5 implies).
  QVERIFY(f.signalLog.size() >= 2);
  QCOMPARE(f.signalLog.at(f.signalLog.size() - 2), QStringLiteral("state:%1").arg(int(SessionReplayState::Finished)));
  QCOMPARE(f.signalLog.last(), QStringLiteral("finished"));
}

/** `start()` again replays the whole stream, in the same order. */
void TstSessionReplayPlayer::restartReplaysTheWholeStreamAgain()
{
  PlayerFixture f;
  const SessionFilePtr session = threeEventSession();
  QVERIFY(f.player.start(session).ok);
  QVERIFY(f.player.play().ok);
  f.runToEnd();
  QCOMPARE(f.delivered.size(), 3);

  QVERIFY(f.player.start(session).ok);
  QCOMPARE(f.player.deliveredCount(), quint64(0));
  QVERIFY(f.player.play().ok);
  f.runToEnd();

  QCOMPARE(f.delivered.size(), 6);
  QCOMPARE(f.delivered.at(3).sequence, quint64(1));
  QCOMPARE(f.delivered.at(5).sequence, quint64(3));
  QCOMPARE(f.finishedCount, 2);
}

/** `close()` detaches the session, releases the shared pointer and returns to
  *  `Idle`. */
void TstSessionReplayPlayer::closeReleasesTheSessionAndReturnsToIdle()
{
  PlayerFixture f;
  std::weak_ptr<const SessionFile> weak;
  {
    const SessionFilePtr session = threeEventSession();
    weak = session;
    QVERIFY(f.player.start(session).ok);
    QCOMPARE(f.player.eventCount(), quint64(3));
  }
  // The local reference is gone; the player still holds the session.
  QVERIFY(!weak.expired());

  f.player.close();
  QCOMPARE(f.player.state(), SessionReplayState::Idle);
  QCOMPARE(f.player.eventCount(), quint64(0));
  QCOMPARE(f.player.position(), quint64(0));
  QVERIFY(weak.expired());

  // close() in Idle is a no-op that cannot refuse.
  const int statesBefore = f.states.size();
  f.player.close();
  QCOMPARE(f.player.state(), SessionReplayState::Idle);
  QCOMPARE(f.states.size(), statesBefore);
}

/** `close()` while `Playing` cancels the pending callback and delivers nothing
  *  afterwards (review round 3, cleanup item 2). */
void TstSessionReplayPlayer::closeWhilePlayingCancelsAndReturnsToIdle()
{
  PlayerFixture f;
  QVERIFY(f.player.start(threeEventSession()).ok);
  QVERIFY(f.player.play().ok);
  QVERIFY(f.scheduler.armed());

  f.player.close();
  QVERIFY(!f.scheduler.armed());
  QCOMPARE(f.player.state(), SessionReplayState::Idle);
  QVERIFY(!f.scheduler.fire());
  QCOMPARE(f.delivered.size(), 0);
  QCOMPARE(f.finishedCount, 0);
  // Ready, Playing, Idle - the state change is the only signal close() emits.
  QCOMPARE(f.states.size(), 3);
  QCOMPARE(f.states.at(0), SessionReplayState::Ready);
  QCOMPARE(f.states.at(1), SessionReplayState::Playing);
  QCOMPARE(f.states.at(2), SessionReplayState::Idle);
}

/** A session without events is valid but has nothing to replay: `play()` and
  *  every step refuse with a reason. */
void TstSessionReplayPlayer::zeroEventSessionRefusesPlayAndStep()
{
  PlayerFixture f;
  QVERIFY(f.player.start(makeSession({})).ok);
  QCOMPARE(f.player.state(), SessionReplayState::Ready);
  QCOMPARE(f.player.eventCount(), quint64(0));

  const SessionReplayStart play = f.player.play();
  QVERIFY(!play.ok);
  QVERIFY(!play.reason.isEmpty());
  QVERIFY(!f.player.stepNextEvent().ok);
  QVERIFY(!f.player.stepNextRx().ok);
  QVERIFY(!f.player.stepNextTx().ok);
  QVERIFY(!f.scheduler.armed());
  QCOMPARE(f.player.state(), SessionReplayState::Ready);

  // The report of a session without events is never "finished".
  const SessionReplayReport report = f.player.stop();
  QCOMPARE(report.delivered, quint64(0));
  QCOMPARE(report.remaining, quint64(0));
  QVERIFY(!report.finished);
}

/** Every refusal is a returned value with a reason, and it changes nothing. */
void TstSessionReplayPlayer::refusalsAreValuesThatChangeNothing()
{
  PlayerFixture f;

  // Idle: play() and every step refuse.
  QCOMPARE(f.player.state(), SessionReplayState::Idle);
  QVERIFY(!f.player.play().ok);
  QVERIFY(!f.player.play().reason.isEmpty());
  QVERIFY(!f.player.stepNextEvent().ok);
  QVERIFY(!f.player.stepNextRx().ok);
  QVERIFY(!f.player.stepNextTx().ok);

  // A value outside the declared enumerators is a refusal, not silence.
  const SessionReplayTimingChange unknown =
      f.player.setTiming(static_cast<SessionReplayTiming>(99));
  QVERIFY(!unknown.ok);
  QVERIFY(unknown.reason.contains(QStringLiteral("unknown timing policy")));
  QCOMPARE(unknown.timing, SessionReplayTiming::Original);

  // In `Playing`, every refusal leaves state, position, count and policy alone.
  const SessionFilePtr session = threeEventSession();
  QVERIFY(f.player.start(session).ok);
  QVERIFY(f.player.play().ok);
  const SessionReplayState stateBefore = f.player.state();
  const quint64 positionBefore = f.player.position();
  const quint64 deliveredBefore = f.player.deliveredCount();
  const SessionReplayTiming timingBefore = f.player.timing();

  const SessionReplayStart playRefusal = f.player.play();
  QVERIFY(!playRefusal.ok);
  QVERIFY(!playRefusal.reason.isEmpty());
  const SessionReplayStep stepRefusal = f.player.stepNextRx();
  QVERIFY(!stepRefusal.ok);
  QVERIFY(!stepRefusal.reason.isEmpty());
  const SessionReplayTimingChange timingRefusal =
      f.player.setTiming(SessionReplayTiming::Step);
  QVERIFY(!timingRefusal.ok);
  QVERIFY(!timingRefusal.reason.isEmpty());

  QCOMPARE(f.player.state(), stateBefore);
  QCOMPARE(f.player.position(), positionBefore);
  QCOMPARE(f.player.deliveredCount(), deliveredBefore);
  QCOMPARE(f.player.timing(), timingBefore);
  QVERIFY(f.scheduler.armed());
}

/** The player derives every delay from the stored timeline: the injected clock is
  *  never consulted (ADR-005). */
void TstSessionReplayPlayer::thePlayerNeverConsultsWallClockTime()
{
  PlayerFixture f;
  const SessionFilePtr session = mixedSession();
  QVERIFY(f.player.start(session).ok);
  QVERIFY(f.player.play().ok);
  f.runToEnd();
  // A step after the finished replay needs the explicit restart.
  QVERIFY(f.player.start(session).ok);
  QVERIFY(f.player.stepNextRx().ok);
  QVERIFY(f.player.start(session).ok);
  QVERIFY(f.player.play().ok);
  f.player.stop();
  f.player.close();

  QCOMPARE(f.clock.calls, 0);
}

/** `stepNextEvent()` delivers exactly the event at the position - one event, the
  *  match, no more (the positive half of the step table). */
void TstSessionReplayPlayer::stepNextEventDeliversExactlyTheNextEvent()
{
  PlayerFixture f;
  QVERIFY(f.player.start(mixedSession()).ok);

  const SessionReplayStep step = f.player.stepNextEvent();
  QVERIFY2(step.ok, qPrintable(step.reason));
  QCOMPARE(step.advanced, quint64(1));
  QVERIFY(step.matched);
  QVERIFY(!step.reachedEnd);
  QCOMPARE(f.player.position(), quint64(1));
  QCOMPARE(f.player.deliveredCount(), quint64(1));
  QCOMPARE(f.delivered.size(), 1);
  QCOMPARE(f.delivered.at(0).sequence, quint64(1));
  QVERIFY(!f.scheduler.armed());   // a step arms no timer

  // Repeated stepping walks the stream one event at a time.
  const SessionReplayStep second = f.player.stepNextEvent();
  QCOMPARE(second.advanced, quint64(1));
  QCOMPARE(f.delivered.at(1).sequence, quint64(2));
  QCOMPARE(f.player.position(), quint64(2));
}

/** Scaling up uses checked arithmetic and clamps to the largest representable
  *  delay instead of wrapping (SPEC-M10 5.6). */
void TstSessionReplayPlayer::scalingUpClampsInsteadOfWrapping()
{
  const qint64 largest = std::numeric_limits<qint64>::max();
  PlayerFixture f;
  const SessionFilePtr session = makeSession(QList<SessionEvent>{
    dataEvent(1, SessionDirection::Tx, QByteArrayLiteral("A"), 0),
    dataEvent(2, SessionDirection::Rx, QByteArrayLiteral("B"), largest),
  });
  QVERIFY(f.player.start(session).ok);
  QVERIFY(f.player.setTiming(SessionReplayTiming::Scale0_1).ok);
  QVERIFY(f.player.play().ok);
  f.runToEnd();

  QCOMPARE(f.scheduler.delays.size(), 2);
  QCOMPARE(f.scheduler.delays.at(1), largest);   // clamped, not wrapped
  QVERIFY(f.scheduler.delays.at(1) > 0);

  // The same gap scaled down stays finite and sane.
  PlayerFixture g;
  QVERIFY(g.player.start(session).ok);
  QVERIFY(g.player.setTiming(SessionReplayTiming::Scale10).ok);
  QVERIFY(g.player.play().ok);
  g.runToEnd();
  QCOMPARE(g.scheduler.delays.at(1), largest / 10);
}















/** The timing ladder and the refusal surface as a matrix (§5.5's state table).
  *
  *  - every declared ladder value is accepted in `Idle`, `Ready`, `Paused` and
  *    `Finished`, reported back by `timing()`, and emits neither a delivery nor a
  *    completion on its own;
  *  - while `Playing` every value except `Step` is accepted (it applies to the
  *    next delivery) and `Step` is refused with the policy still in effect;
  *  - every refusal entry point returns a reason and leaves state, position,
  *    delivered count, event count, timing policy and the pending schedule
  *    exactly as they were: `play()` and the three steps on a zero-event session,
  *    `play()` and the three steps in `Idle`, `play()` in `Step` mode, `play()`
  *    and the three steps while `Playing`, and `play()`, the three steps and an
  *    unknown policy in `Finished`.
  */
void TstSessionReplayPlayer::theTimingMatrixAndEveryRefusalLeaveThePlayerUnchanged()
{
  const QList<SessionReplayTiming> ladder{SessionReplayTiming::Original,
                                          SessionReplayTiming::Immediate,
                                          SessionReplayTiming::Scale0_1,
                                          SessionReplayTiming::Scale0_25,
                                          SessionReplayTiming::Scale0_5,
                                          SessionReplayTiming::Scale1,
                                          SessionReplayTiming::Scale2,
                                          SessionReplayTiming::Scale5,
                                          SessionReplayTiming::Scale10,
                                          SessionReplayTiming::Step};
  const auto observe = [](const PlayerFixture &f) {
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(int(f.player.state()))
        .arg(f.player.position())
        .arg(f.player.deliveredCount())
        .arg(f.player.eventCount())
        .arg(int(f.player.timing()))
        .arg(f.scheduler.armed() ? 1 : 0);
  };
  const auto refusedUnchanged = [&observe](PlayerFixture &f, const QString &before,
                                           const QString &reason, const char *what) {
    QVERIFY2(!reason.isEmpty(), what);
    QCOMPARE(observe(f), before);
  };

  // (1) The ladder in the four states that accept every declared value.
  for (int stateIndex = 0; stateIndex < 4; ++stateIndex) {
    for (const SessionReplayTiming timing : ladder) {
      PlayerFixture f;
      QVERIFY(f.player.start(threeEventSession()).ok);
      if (stateIndex == 0) {
        f.player.close();                                    // Idle
      } else if (stateIndex == 2) {                           // Paused, mid-stream
        QVERIFY(f.player.setTiming(SessionReplayTiming::Original).ok);
        QVERIFY(f.player.play().ok);
        QVERIFY(f.scheduler.fire());
        f.player.stop();
        QCOMPARE(f.player.state(), SessionReplayState::Paused);
      } else if (stateIndex == 3) {                           // Finished
        QVERIFY(f.player.setTiming(SessionReplayTiming::Original).ok);
        QVERIFY(f.player.play().ok);
        f.runToEnd();
        QCOMPARE(f.player.state(), SessionReplayState::Finished);
      }
      const int deliveries = f.delivered.size();
      const int completions = f.finishedCount;

      const SessionReplayTimingChange change = f.player.setTiming(timing);
      QVERIFY(change.ok);
      QCOMPARE(change.timing, timing);
      QCOMPARE(f.player.timing(), timing);
      // no declared value emits a delivery or a completion by itself
      QCOMPARE(f.delivered.size(), deliveries);
      QCOMPARE(f.finishedCount, completions);
      QVERIFY(!f.scheduler.armed());   // and none of these states has a schedule
    }
  }

  // (2) While Playing: every declared value except Step is accepted; Step refuses.
  for (const SessionReplayTiming timing : ladder) {
    PlayerFixture f;
    QVERIFY(f.player.start(threeEventSession()).ok);
    QVERIFY(f.player.play().ok);
    QVERIFY(f.scheduler.armed());
    const QString before = observe(f);
    const int deliveries = f.delivered.size();
    const SessionReplayTiming policyBefore = f.player.timing();

    const SessionReplayTimingChange change = f.player.setTiming(timing);
    QCOMPARE(f.delivered.size(), deliveries);               // the change itself emits nothing
    if (timing == SessionReplayTiming::Step) {
      QVERIFY(!change.ok);
      QVERIFY(!change.reason.isEmpty());
      QCOMPARE(change.timing, policyBefore);               // the policy in effect
      QCOMPARE(f.player.timing(), policyBefore);
    } else {
      QVERIFY(change.ok);
      QCOMPARE(change.timing, timing);
      QCOMPARE(f.player.timing(), timing);
      // The accepted policy applies to the *next* delivery (§5.6): fire the
      // pending callback and read the delay it armed with. `threeEventSession()`
      // puts its first two events 1000 ns apart, so every value of the ladder has
      // a checkable counterpart here.
      QVERIFY(f.scheduler.fire());
      QCOMPARE(f.delivered.size(), deliveries + 1);
      qint64 expected = 1000;                              // Original and Scale1
      switch (timing) {
      case SessionReplayTiming::Immediate: expected = 0; break;
      case SessionReplayTiming::Scale0_1:  expected = 10000; break;
      case SessionReplayTiming::Scale0_25: expected = 4000; break;
      case SessionReplayTiming::Scale0_5:  expected = 2000; break;
      case SessionReplayTiming::Scale2:    expected = 500; break;
      case SessionReplayTiming::Scale5:    expected = 200; break;
      case SessionReplayTiming::Scale10:   expected = 100; break;
      default: break;
      }
      QCOMPARE(f.scheduler.delays.last(), expected);
    }
    QCOMPARE(f.finishedCount, 0);
    QVERIFY(f.scheduler.armed());                           // the next delivery is armed
    QCOMPARE(f.player.state(), SessionReplayState::Playing);
    if (timing == SessionReplayTiming::Step)
      QCOMPARE(f.player.position(), quint64(before.section('|', 1, 1).toUInt()));
    else
      QCOMPARE(f.player.position(), quint64(1));            // the successor went out
  }

  // (3) Every refusal entry point, with the observable state unchanged.
  {
    PlayerFixture f;                                        // Idle
    const QString before = observe(f);
    refusedUnchanged(f, before, f.player.play().reason, "play in Idle refuses with a reason");
    refusedUnchanged(f, before, f.player.stepNextEvent().reason, "step in Idle refuses with a reason");
    refusedUnchanged(f, before, f.player.stepNextRx().reason, "Rx step in Idle refuses with a reason");
    refusedUnchanged(f, before, f.player.stepNextTx().reason, "Tx step in Idle refuses with a reason");
    const SessionReplayStart nullStart = f.player.start(SessionFilePtr());
    QVERIFY(!nullStart.ok);
    refusedUnchanged(f, before, nullStart.reason, "a null start refuses with a reason");
    const SessionReplayTimingChange unknown =
        f.player.setTiming(static_cast<SessionReplayTiming>(99));
    QVERIFY(!unknown.ok);
    refusedUnchanged(f, before, unknown.reason, "an unknown policy refuses with a reason");
  }
  {
    PlayerFixture f;                                        // Step mode: play refuses
    QVERIFY(f.player.start(threeEventSession()).ok);
    QVERIFY(f.player.setTiming(SessionReplayTiming::Step).ok);
    const QString before = observe(f);
    refusedUnchanged(f, before, f.player.play().reason, "play in Step mode refuses with a reason");
    QVERIFY(f.player.stepNextEvent().ok);                   // while a step is accepted
  }
  {
    PlayerFixture f;                                        // Playing: play and the steps refuse
    QVERIFY(f.player.start(threeEventSession()).ok);
    QVERIFY(f.player.play().ok);
    const QString before = observe(f);
    refusedUnchanged(f, before, f.player.play().reason, "play while Playing refuses");
    refusedUnchanged(f, before, f.player.stepNextEvent().reason, "step while Playing refuses");
    refusedUnchanged(f, before, f.player.stepNextRx().reason, "Rx while Playing refuses");
    refusedUnchanged(f, before, f.player.stepNextTx().reason, "Tx while Playing refuses");
    QVERIFY(f.scheduler.armed());
  }
  {
    PlayerFixture f;                                        // a session with no events
    QVERIFY(f.player.start(makeSession(QList<SessionEvent>{})).ok);
    const QString before = observe(f);
    refusedUnchanged(f, before, f.player.play().reason, "play on a zero-event session refuses");
    refusedUnchanged(f, before, f.player.stepNextEvent().reason, "step on a zero-event session refuses");
    refusedUnchanged(f, before, f.player.stepNextRx().reason, "Rx on a zero-event session refuses");
    refusedUnchanged(f, before, f.player.stepNextTx().reason, "Tx on a zero-event session refuses");
    QVERIFY(!f.scheduler.armed());
  }
  {
    PlayerFixture f;                                        // Finished: play and steps refuse
    QVERIFY(f.player.start(threeEventSession()).ok);
    QVERIFY(f.player.play().ok);
    f.runToEnd();
    const QString before = observe(f);
    refusedUnchanged(f, before, f.player.play().reason, "play at the end refuses");
    refusedUnchanged(f, before, f.player.stepNextEvent().reason, "step at the end refuses");
    refusedUnchanged(f, before, f.player.stepNextRx().reason, "Rx at the end refuses");
    refusedUnchanged(f, before, f.player.stepNextTx().reason, "Tx at the end refuses");
    const SessionReplayTimingChange unknown =
        f.player.setTiming(static_cast<SessionReplayTiming>(99));
    QVERIFY(!unknown.ok);
    refusedUnchanged(f, before, unknown.reason, "an unknown policy at the end refuses");
  }
}

QTEST_MAIN(TstSessionReplayPlayer)
#include "tst_sessionreplayplayer.moc"