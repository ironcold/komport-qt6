/***************************************************************************
                    sessionreplayplayer.h  -  Komport Serial Port Communicator
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

#ifndef SESSIONREPLAYPLAYER_H
#define SESSIONREPLAYPLAYER_H

// ADR-011 D6-D10 (accepted 2026-09-19) and SPEC-M10 sections 5.5, 5.6 and 5.10:
// the read-only passive replay of one loaded session.
//
// The player has exactly one output - `eventDelivered` - and holds no transport,
// no output target, no file and no widget. It distributes the stored, immutable
// events of a session in stored order, at a timing the user chooses from the
// ladder, forward-only, and reports every refusal as a value carrying a reason.
// Automatic advancement always goes through the scheduler seam, one event per
// callback, so `stop()` is honoured at a delivery boundary and no operation
// blocks or iterates over the session.
//
// It never consults wall-clock time: every delay is computed from the stored
// session timeline (`timestampNs`) and handed to the scheduler; the clock seam is
// injected so that a test can prove that (ADR-005).
//
// QtCore only (ADR-009): the compile proof is in tests/session_contract_compile.cpp.

#include "sessionreader.h"        // SessionFilePtr, SessionEvent
#include "sessionreplayseams.h"   // SessionReplayScheduler, SessionMonotonicClock

#include <QObject>
#include <QString>

#include <memory>

/** The M10 timing ladder (ADR-011 D7; architecture section 9.1). */
enum class SessionReplayTiming {
  Original, Immediate,
  Scale0_1, Scale0_25, Scale0_5, Scale1, Scale2, Scale5, Scale10,
  Step
};

/** The player's states (ADR-011 D6). */
enum class SessionReplayState { Idle, Ready, Playing, Paused, Finished };

/** The result of `start()`: whether a session is attached. */
struct SessionReplayStart {
  bool ok = false;
  QString reason;
};

/** The result of a step: a refusal is a value, and the command's effect is
  * reported rather than assumed. */
struct SessionReplayStep {
  bool ok = false;
  QString reason;
  quint64 advanced = 0;      ///< events this call delivered
  bool matched = false;      ///< the requested event/direction was found
  bool reachedEnd = false;   ///< the position is at the end after the call
};

/** The result of a timing change: a refusal is a value, and `timing` is the
  * policy in effect after the call (unchanged when `ok` is false). */
struct SessionReplayTimingChange {
  bool ok = false;
  QString reason;
  SessionReplayTiming timing = SessionReplayTiming::Original;
};

/** What a replay reports at its end and at an explicit stop. */
struct SessionReplayReport {
  quint64 delivered = 0;
  quint64 remaining = 0;
  quint64 position = 0;
  bool finished = false;
  SessionReplayTiming timing = SessionReplayTiming::Original;
  QString reason;
};

struct SessionReplayPlayerSeamsForTest;

/** Read-only passive replay of one loaded session (ADR-007, ADR-011 D6). QtCore only. */
class SessionReplayPlayer : public QObject
{
  Q_OBJECT
public:
  explicit SessionReplayPlayer(QObject *parent = nullptr);
  ~SessionReplayPlayer() override;    ///< cancels a pending delivery; emits nothing

  /** Attach @p session and position at its first event; calling it again is the
    * explicit restart (it stops a running replay first and resets the delivered
    * count). Refuses only a null session. */
  SessionReplayStart start(const SessionFilePtr &session);
  /** Begin or resume automatic replay from the current position. */
  SessionReplayStart play();
  /** Stop; keep the position and the delivered count. Never an error. */
  SessionReplayReport stop();
  SessionReplayStep stepNextEvent();
  SessionReplayStep stepNextRx();
  SessionReplayStep stepNextTx();
  /** Detach the session and return to `Idle`; cannot refuse. */
  void close();

  SessionReplayState state() const;
  SessionReplayTiming timing() const;
  /** Change the timing policy; `Step` is refused while `Playing` (§5.6). */
  SessionReplayTimingChange setTiming(SessionReplayTiming timing);
  quint64 position() const;        ///< index of the next event to deliver
  quint64 deliveredCount() const;
  quint64 eventCount() const;      ///< 0 while `Idle`

signals:
  /** Exactly one stored event per delivery, in stored order, by `const`
    * reference to a value that is never modified afterwards (ADR-011 D5). */
  void eventDelivered(const SessionEvent &event);
  void stateChanged(SessionReplayState state);
  /** Emitted when an automatic replay delivers its last event; a step that
    * reaches the end reports through its result value instead (§5.5/§5.6). */
  void replayFinished(const SessionReplayReport &report);

private:
  /** Injection constructor for the SPEC-M10 5.10 seams; reachable only through
    * `SessionReplayPlayerSeamsForTest`, because the seams are not public API. The
    * injected objects are not owned and must outlive the player. */
  SessionReplayPlayer(SessionReplayScheduler *scheduler, SessionMonotonicClock *clock,
                      QObject *parent);
  friend struct SessionReplayPlayerSeamsForTest;

  /** The delay before the event at @p index, from the stored session timeline. */
  qint64 delayNsFor(quint64 index) const;
  /** Ask the scheduler for the next delivery of the event at the current position. */
  void scheduleNextDelivery();
  /** The scheduled callback: deliver one event, then schedule the next or finish. */
  void onDeliveryDue();
  /** Deliver the event at the current position (emitting `eventDelivered`). */
  void deliverAtCurrentPosition();
  /** The report of the current state (delivered, remaining, position, finished). */
  SessionReplayReport report() const;
  /** Enter @p state, emitting `stateChanged` only when it actually changes. */
  void setState(SessionReplayState state);
  /** Whether @p timing is one of the declared ladder entries. */
  static bool isDeclaredTiming(SessionReplayTiming timing);
  /** The refusal of a step in the current state, or an empty string. */
  QString stepRefusal() const;
  /** A step that looks for the next `Data` event of @p direction. */
  SessionReplayStep stepToMatch(SessionDirection direction);
  /** Perform a step, delivering everything up to and including @p matchIndex. */
  SessionReplayStep performStep(qint64 matchIndex, bool matched);

  SessionFilePtr mSession;             ///< the attached session, or null while Idle
  SessionReplayState mState = SessionReplayState::Idle;
  SessionReplayTiming mTiming = SessionReplayTiming::Original;
  quint64 mPosition = 0;               ///< index of the next event to deliver
  quint64 mDelivered = 0;
  SessionReplayScheduler *mScheduler;  ///< injected or owned; see below
  SessionMonotonicClock *mClock;       ///< injected or owned, and never consulted
  std::unique_ptr<SessionReplayScheduler> mOwnedScheduler;  ///< the production default
  std::unique_ptr<SessionMonotonicClock> mOwnedClock;       ///< the production default
};

#endif // SESSIONREPLAYPLAYER_H
