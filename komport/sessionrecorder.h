/***************************************************************************
                        sessionrecorder.h  -  Komport Serial Port Communicator
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

#ifndef SESSIONRECORDER_H
#define SESSIONRECORDER_H

// ADR-010 (live session recording) and SPEC-M9: records a live session to a
// `.kpsession` v1 file (ADR-006) as the events arrive.
//
// The recorder is a *consumer*: it observes `SessionController::eventObserved`
// and nothing else. Session data never comes from the legacy `receivedChar` /
// `sentChar` signals or from the legacy RX buffer (AGENTS.md), it never reads
// or writes the transport (ADR-007), and it changes no terminal behaviour.
//
// Properties that the specification fixes and this class must keep:
//   - One record per accepted event, appended immediately; no event is buffered
//     in a container, so memory does not grow with the session (SPEC-M9 5.3).
//   - Recording starts only while the session is live, and the complete header
//     is written and flushed at start - the live state guarantees the clock
//     domain's anchor, so nothing has to wait for a first event (ADR-010 4).
//   - The process buffer is bounded: a one-second timer armed by the first
//     unflushed write flushes no later than one second after it, even if the
//     line then goes quiet and no further event arrives (ADR-010 8, D2).
//   - A write, flush or encoding failure ends the recording as *damaged*: the
//     complete prefix stays valid data, the file is not deleted, the event that
//     could not be written is reported with its sequence number and sizes, and
//     the outcome reaches the application (SPEC-M9 5.5/5.6).
//   - The reported duration is wall-clock time on the monotonic clock from the
//     start flush to the finalisation, never a session timestamp (SPEC-M9 5.6).
//   - No `fsync`: the flush bounds this process's buffer only, never durable
//     storage (ADR-010 8).
//
// This unit is part of M9's QtCore-only surface (ADR-009, ADR-010 section 7).
// The sink, scheduler and clock seams of SPEC-M9 5.9 are *not* part of this
// class's public contract: they live in sessionrecorderseams.h, and only the
// test access struct may construct a recorder with them.

#include "sessioncontroller.h"
#include "sessionevent.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <memory>

class SessionRecordSink;
class SessionFlushScheduler;
class SessionMonotonicClock;

/** Everything a recording needs that only the application knows (SPEC-M9 5.4). */
struct SessionRecordingRequest {
  QString path;             ///< the target file, chosen by the user
  QJsonObject configuration;///< the applied configuration snapshot of ADR-006
  QString applicationName;  ///< the writing application, for the header
  QString applicationVersion;///< its version, for the header
};

/** The outcome of a start request. */
struct SessionRecordingStart {
  bool ok = false;
  QString reason;           ///< why the start was refused, empty when accepted
};

/** What a finished recording did. */
struct SessionRecordingReport {
  QString path;             ///< the file that was written
  quint64 records = 0;      ///< records written completely
  qint64 bytes = 0;         ///< every byte the sink accepted, including a partial tail
  qint64 durationNs = 0;    ///< monotonic time from the start flush to the finalisation
  bool damaged = false;     ///< the recording ended in the damaged state
  QString reason;           ///< the damage reason, or "no session data recorded" for
                            ///< a clean stop that wrote no record; otherwise empty
};

/** Records a live session to a `.kpsession` v1 file (ADR-010, SPEC-M9). */
class SessionRecorder : public QObject
{
Q_OBJECT
public:
  /** The recording states of SPEC-M9 section 5.5. There is no state between
    * stopped and recording: the header is written at start, so a recording
    * exists from the moment it begins. */
  enum class State {
    Stopped,   ///< not recording
    Live,      ///< recording; the file carries a complete header
    Damaged    ///< a failure ended the writing; finalising awaits `stop()`
  };

  /** The flush interval of ADR-010 decision D2. */
  static constexpr int kFlushIntervalMs = 1000;

  /** Records @p controller's session, with the production sink, scheduler and
    * clock.
    *
    * @param controller the observed session; it must outlive the recorder
    * @param parent the QObject parent, may be nullptr (the document owns it)
    */
  explicit SessionRecorder(SessionController *controller, QObject *parent = nullptr);
  /** Finalises a running recording: flush, close, nothing else. No signal is
    * emitted and no report is delivered from here (ADR-010 section 8). */
  ~SessionRecorder() override;

  /** The current recording state. */
  State state() const { return mState; }
  /** The report of the last finished recording; empty before the first one. */
  SessionRecordingReport lastReport() const { return mLastReport; }

  /** Begin recording to the requested path.
    *
    * Refused - without touching the target - unless the session is live, the
    * clock domain has an anchor, and the requested configuration snapshot
    * satisfies ADR-006's schema. On success the file exists with a complete,
    * flushed header and no records yet. A recording that ended as damaged is
    * finalised first, so a new recording can start afterwards (SPEC-M9 5.5).
    *
    * @param request the path, the configuration snapshot and the application facts
    * @return whether recording started, and why not if it did not
    */
  SessionRecordingStart start(const SessionRecordingRequest &request);

  /** Stop recording and finalise the file: a last flush, a close, and the report
    * of what was written. A recording that stopped because of a failure reports
    * the same way, with `damaged` set. An explicit stop never emits.
    *
    * @return the report of the recording that just ended; an empty report if none
    *         was running
    */
  SessionRecordingReport stop();

signals:
  /** A recording that ended without an explicit `stop()` - a write, flush or
    * encoding failure - so that the application can tell the user. An explicit
    * `stop()` reports through its return value instead and emits nothing. */
  void recordingEnded(const SessionRecordingReport &report);

private slots:
  /** One accepted session event: one record, appended immediately. */
  void onEventObserved(const SessionEvent &event);

private:
  /** The outcome of one sink write: how much was accepted, and whether it was all. */
  struct WriteOutcome {
    qint64 accepted = 0;    ///< bytes the sink accepted (0 on failure)
    bool complete = false;  ///< the sink accepted every byte offered
  };

  /** Injection constructor for the SPEC-M9 5.9 seams; reachable only through
    * `SessionRecorderSeamsForTest`, because the seams are not public API. The
    * injected objects are not owned and must outlive the recorder. */
  SessionRecorder(SessionController *controller, SessionRecordSink *sink,
                  SessionFlushScheduler *scheduler, SessionMonotonicClock *clock);
  friend struct SessionRecorderSeamsForTest;

  /** Write one complete unit and report what the sink accepted. */
  WriteOutcome writeUnit(const QByteArray &bytes);
  /** Arm the periodic flush if it is not already pending (ADR-010 D2). */
  void armFlush();
  /** The scheduled flush; a failure here is damage, not a lost notification. */
  void onFlushTimer();
  /** End the recording because of a failure: report, notify, await `stop()`. */
  void endDamaged(const QString &reason);
  /** The duration so far: the monotonic clock from the start flush to `mEndNs`
    * (or to now while the recording is still running). */
  qint64 durationNs() const;
  /** Forget the running recording's state, keeping `lastReport()`. */
  void clearRecordingState();

  SessionController *mController;
  SessionRecordSink *mSink;
  SessionFlushScheduler *mScheduler;
  SessionMonotonicClock *mClock;
  std::unique_ptr<SessionRecordSink> mOwnedSink;
  std::unique_ptr<SessionFlushScheduler> mOwnedScheduler;
  std::unique_ptr<SessionMonotonicClock> mOwnedClock;
  State mState = State::Stopped;
  QString mPath;              ///< the running recording's target
  bool mFileOpen = false;
  bool mFlushPending = false;
  quint64 mRecords = 0;
  qint64 mBytes = 0;
  qint64 mLastSessionTimestampNs = 0;
  qint64 mStartNs = 0;        ///< the monotonic instant after the start flush
  qint64 mEndNs = 0;          ///< the monotonic instant of a damage instant; see `mRecordingStarted`
  bool mRecordingStarted = false; ///< a start instant exists (0 is a *valid* clock value)
  SessionRecordingReport mLastReport;
};

#endif // SESSIONRECORDER_H
