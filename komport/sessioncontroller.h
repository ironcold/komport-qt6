/***************************************************************************
                         sessioncontroller.h  -  Komport Session Controller
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

#ifndef SESSIONCONTROLLER_H
#define SESSIONCONTROLLER_H

// SPEC-M8 (accepted 2026-09-18), sections 6.1, 7, 8, 10: one SessionController
// binds exactly one ITransport to one live session. It owns no transport and
// never writes to one; it converts the transport's observations into ordered
// SessionEvents with source ID 1 and applies the ADR-005 session-time mapping.
//
// This header is part of the public session/transport contract: it uses QtCore
// only and must never gain a dependency on QWidget, QSerialPort, KomportApp or
// any other executable-specific type (ADR-003, ADR-009). The widget-free
// compile proof for the contract headers is tests/session_contract_compile.cpp;
// this controller is compiled into komport_core like the rest of the module.

#include "itransport.h"
#include "sessionevent.h"

#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QString>

/** Binds one transport to one session (SPEC-M8 6.1).
  *
  * The controller is the only place that assigns a session `sequence` and a
  * session time: an event's `sequence` is assigned when it is dequeued, and its
  * `timestampNs` follows ADR-005's mapping per clock domain. M8 has exactly one
  * active local transport and therefore one clock domain and one reference.
  *
  * Delivery is FIFO and non-reentrant (SPEC-M8 10): an observation produced while
  * an event is being delivered - for example by an emulation consumer that writes
  * a reply from its handler - is appended to the FIFO and emitted after the
  * current delivery returns. Emissions never nest.
  *
  * The controller is passive: it never opens, closes or writes a transport, and
  * it is destroyed before the transport it observes (SPEC-M8 6.2).
  */
class SessionController : public QObject {
Q_OBJECT
public:
  /** The session state of SPEC-M8 7. A session exists as long as the controller
    * does; `Idle` is not a closed session, only an unopened transport. */
  enum class State {
    Idle,   ///< constructed, or the current activation ended
    Live    ///< the transport is open
  };

  /** Bind @p transport to a new session.
    *
    * @param transport the observed transport; it must outlive this controller
    * @param parent the QObject parent, may be nullptr (the owner is the document)
    */
  explicit SessionController(ITransport *transport, QObject *parent = nullptr);
  /** Destruction emits no event and calls nothing on the transport (ADR-003). */
  ~SessionController() override;

  /** The current session state (SPEC-M8 7). */
  State state() const { return mState; }
  /** The physical source ID every event of this session carries (SPEC-M8 8). */
  quint32 sourceId() const { return kSourceId; }
  /** The activation id currently live, or 0 while `Idle` (SPEC-M8 7). */
  quint64 currentActivationId() const { return mCurrentActivationId; }
  /** Sequence of the last emitted event; 0 before the first one (SPEC-M8 8). */
  quint64 lastEmittedSequence() const { return mLastSequence; }

signals:
  /** One ordered session event: the read-only surface of the session, named by
    * SPEC-M8 section 2. */
  void eventObserved(const SessionEvent &event);

private slots:
  /** One non-empty read: one RX observation (SPEC-M8 6.2). */
  void onBytesReceived(quint64 activationId, const QByteArray &bytes, qint64 sourceTimestampNs);
  /** One accepted write: one TX observation covering exactly the accepted prefix. */
  void onBytesWritten(quint64 activationId, const QByteArray &bytes, qint64 sourceTimestampNs);
  /** A successful open: the activation boundary of the session (SPEC-M8 7). */
  void onOpened(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
  /** The end of an activation; the session stays alive (SPEC-M8 7). */
  void onClosed(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
  /** A configuration transaction result (one or two observations per transaction). */
  void onConfigurationChanged(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
  /** A modem-line change; part of the frozen surface, unimplemented in M8. */
  void onLineStateChanged(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
  /** An error observation, including the transport's failed-open error. */
  void onTransportError(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);

private:
  /** An accepted observation, before it has a sequence number or a session time.
    * SPEC-M8 10 requires both to be assigned at dequeue/emission time. */
  struct PendingObservation {
    SessionEventType type = SessionEventType::Data;
    SessionDirection direction = SessionDirection::None;
    QByteArray payload;
    QJsonObject metadata;
    qint64 sourceTimestampNs = 0;
    /** Set for an event the controller itself derives (the ADR-005 anomaly
      * diagnostic): its session time is already mapped, so the drain loop must
      * neither map it a second time nor derive another diagnostic from it. */
    bool sessionTimeMapped = false;
    qint64 sessionTimestampNs = 0;
  };

  /** The activation filter of SPEC-M8 7: an observation belongs to the session
    * only if it carries the live activation id - or if it is the failed-open
    * error of an attempt that never produced an `opened`. */
  bool acceptsObservation(SessionEventType type, quint64 activationId);
  /** Emit exactly one diagnostic for a rejected observation (SPEC-M8 7). It is
    * a Qt warning and never an event; nothing else observes a drop. */
  void reportDrop(quint64 activationId, const QString &reason);
  /** Append an accepted observation and drain the FIFO (non-reentrant). */
  void enqueue(SessionEventType type, SessionDirection direction, const QByteArray &payload,
               qint64 sourceTimestampNs, const QJsonObject &metadata);
  /** Deliver every queued observation in order; never nests (SPEC-M8 10). */
  void drainFifo();
  /** ADR-005 mapping for the single clock domain of M8; sets @p anomaly when the
    * observation's source time contradicts the domain's monotonic clock. */
  qint64 mapSourceTime(qint64 sourceTimestampNs, bool *anomaly);

  /** FIFO of accepted observations awaiting delivery (SPEC-M8 10). The
    * controller holds no pointer to the transport: it observes it through the
    * signals and never calls into it (SPEC-M8 7, ADR-003). */
  QQueue<PendingObservation> mFifo;
  /** true while an event is being delivered; a nested drain returns at once */
  bool mDelivering;
  /** session state (SPEC-M8 7) */
  State mState;
  /** the live activation id, 0 while `Idle` (SPEC-M8 7) */
  quint64 mCurrentActivationId;
  /** the highest activation id this controller ever admitted - either through an
    * `opened` or through the accepted failed-open error of an attempt. Activation
    * ids strictly increase (ADR-003) and delivery order is never trusted, so every
    * later admission has to be above this watermark; it also rejects the id 0,
    * which is not an activation id at all, and it is what suppresses a repeated
    * error of an attempt and every delayed observation of it (SPEC-M8 7). */
  quint64 mHighestAttemptId;
  /** sequence of the last emitted event (SPEC-M8 8) */
  quint64 mLastSequence;
  /** the physical source ID of every event of this session (SPEC-M8 8) */
  static constexpr quint32 kSourceId = 1;
  /** ADR-005 reference of this session's clock domain: set by the first accepted
    * event and never moved afterwards, not even by a later activation. */
  bool mReferenceSet;
  qint64 mReferenceSourceTimestampNs;
  qint64 mLastEmittedSessionTimestampNs;
};

#endif
