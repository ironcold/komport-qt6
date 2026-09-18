/***************************************************************************
                         sessioncontroller.cpp  -  Komport Session Controller
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

#include "sessioncontroller.h"

#include <QCoreApplication>
#include <QDebug>

namespace {

/** The metadata of the one anomaly diagnostic ADR-005 asks for: the observation's
  * source time contradicted the monotonic clock of its domain, so the event kept
  * the last emitted session time and its raw time and payload stay untouched. */
QJsonObject anomalyMetadata()
{
  QJsonObject metadata;
  metadata.insert(QStringLiteral("kind"), QStringLiteral("runtime"));
  metadata.insert(QStringLiteral("code"), QStringLiteral("non_monotonic_observation"));
  metadata.insert(QStringLiteral("message"),
                  QCoreApplication::translate("SessionController",
                      "The source time of an observation was lower than the last emitted "
                      "session time; the event kept the last session time."));
  return metadata;
}

} // namespace

/** Bind a transport to a new session (SPEC-M8 6.1). */
SessionController::SessionController(ITransport *_transport, QObject *parent)
: QObject(parent)
, mDelivering(false)
, mState(State::Idle)
, mCurrentActivationId(0)
, mHighestAttemptId(0)
, mLastSequence(0)
, mReferenceSet(false)
, mReferenceSourceTimestampNs(0)
, mLastEmittedSessionTimestampNs(0)
{
  if (_transport == nullptr)
    return;

  // Direct, same-thread connections (SPEC-M8 10): M8 is main-Qt-thread only and
  // introduces no queue, lock or worker. Every observation the transport reports
  // reaches the controller in the order the transport emitted it.
  QObject::connect(_transport, &ITransport::bytesReceived,
                   this, &SessionController::onBytesReceived);
  QObject::connect(_transport, &ITransport::bytesWritten,
                   this, &SessionController::onBytesWritten);
  QObject::connect(_transport, &ITransport::opened,
                   this, &SessionController::onOpened);
  QObject::connect(_transport, &ITransport::closed,
                   this, &SessionController::onClosed);
  QObject::connect(_transport, &ITransport::configurationChanged,
                   this, &SessionController::onConfigurationChanged);
  QObject::connect(_transport, &ITransport::lineStateChanged,
                   this, &SessionController::onLineStateChanged);
  QObject::connect(_transport, &ITransport::transportError,
                   this, &SessionController::onTransportError);
}

/** Destruction emits nothing and calls nothing on the transport (SPEC-M8 7). */
SessionController::~SessionController()
{
  // The transport is not touched here - not even disconnected - because this
  // controller is destroyed before the transport (SPEC-M8 6.2) and Qt removes the
  // connection by itself. Discarding the queue is enough: a queued observation of
  // a session that is ending is not an event.
  mDelivering = false;
  mFifo.clear();
}

SessionClockDomainReference SessionController::clockDomainReference() const
{
  SessionClockDomainReference reference;
  reference.valid = mReferenceSet;
  if (mReferenceSet) {
    // The anchor's raw source time is the reference ADR-005 measures against, and
    // its session time is 0 by construction: the mapping subtracts exactly this
    // value. Both stay fixed for the lifetime of the domain, so a consumer that
    // starts observing mid-session reads the same pair as one that saw the anchor.
    reference.sourceTimestampNs = mReferenceSourceTimestampNs;
    reference.sessionTimestampNs = 0;
  }
  return reference;
}

void SessionController::onBytesReceived(quint64 _activationId, const QByteArray &_bytes,
                                        qint64 _sourceTimestampNs)
{
  // An empty read is not an observation and never produces a data event (§9).
  if (_bytes.isEmpty())
    return;
  if (!acceptsObservation(SessionEventType::Data, _activationId))
    return;
  enqueue(SessionEventType::Data, SessionDirection::Rx, _bytes, _sourceTimestampNs, QJsonObject());
}

void SessionController::onBytesWritten(quint64 _activationId, const QByteArray &_bytes,
                                       qint64 _sourceTimestampNs)
{
  if (_bytes.isEmpty())
    return;
  if (!acceptsObservation(SessionEventType::Data, _activationId))
    return;
  enqueue(SessionEventType::Data, SessionDirection::Tx, _bytes, _sourceTimestampNs, QJsonObject());
}

void SessionController::onOpened(quint64 _activationId, qint64 _sourceTimestampNs,
                                 const QJsonObject &_metadata)
{
  // A successful open is the activation boundary of SPEC-M8 7. Arrival order is
  // never trusted - the id decides - so an attempt id is admitted exactly once and
  // only above the watermark of every attempt seen so far: a repeated `opened`, a
  // delayed `opened` of an activation that already failed, and the id 0 are all
  // rejected (ADR-003: ids strictly increase and start at 1).
  if (_activationId == 0) {
    reportDrop(_activationId, QStringLiteral("0 is not an activation id"));
    return;
  }
  if (_activationId <= mHighestAttemptId) {
    reportDrop(_activationId, QStringLiteral("this activation id was already used"));
    return;
  }

  mCurrentActivationId = _activationId;
  mHighestAttemptId = _activationId;
  mState = State::Live;
  enqueue(SessionEventType::TransportOpened, SessionDirection::None, QByteArray(),
          _sourceTimestampNs, _metadata);
}

void SessionController::onClosed(quint64 _activationId, qint64 _sourceTimestampNs,
                                 const QJsonObject &_metadata)
{
  if (_activationId == 0 || _activationId != mCurrentActivationId) {
    reportDrop(_activationId, QStringLiteral("no live activation carries this id"));
    return;
  }

  mCurrentActivationId = 0;
  mState = State::Idle;
  enqueue(SessionEventType::TransportClosed, SessionDirection::None, QByteArray(),
          _sourceTimestampNs, _metadata);
}

void SessionController::onConfigurationChanged(quint64 _activationId, qint64 _sourceTimestampNs,
                                               const QJsonObject &_metadata)
{
  if (!acceptsObservation(SessionEventType::TransportConfigChanged, _activationId))
    return;
  enqueue(SessionEventType::TransportConfigChanged, SessionDirection::None, QByteArray(),
          _sourceTimestampNs, _metadata);
}

void SessionController::onLineStateChanged(quint64 _activationId, qint64 _sourceTimestampNs,
                                           const QJsonObject &_metadata)
{
  if (!acceptsObservation(SessionEventType::LineStateChanged, _activationId))
    return;
  enqueue(SessionEventType::LineStateChanged, SessionDirection::None, QByteArray(),
          _sourceTimestampNs, _metadata);
}

void SessionController::onTransportError(quint64 _activationId, qint64 _sourceTimestampNs,
                                         const QJsonObject &_metadata)
{
  if (!acceptsObservation(SessionEventType::Error, _activationId))
    return;
  enqueue(SessionEventType::Error, SessionDirection::None, QByteArray(),
          _sourceTimestampNs, _metadata);
}

/** The activation filter of SPEC-M8 7, decided by the id alone. */
bool SessionController::acceptsObservation(SessionEventType _type, quint64 _activationId)
{
  if (_activationId != 0 && _activationId == mCurrentActivationId)
    return true;

  // The failed open of SPEC-M8 7/9: the transport reports its own attempt exactly
  // once, and that error is the only observation accepted for an attempt id the
  // controller has not admitted yet. The session creates no live activation, but
  // the attempt id is consumed and must never be reused (ADR-003). Admission
  // raises the watermark, so a repeated error of that attempt and every delayed
  // observation of it are dropped below.
  if (_type == SessionEventType::Error && _activationId > mHighestAttemptId) {
    mHighestAttemptId = _activationId;
    return true;
  }

  if (_activationId == 0)
    reportDrop(_activationId, QStringLiteral("the observation carries no activation id"));
  else if (_type == SessionEventType::Error)
    reportDrop(_activationId, QStringLiteral("this error repeats an attempt that is already known"));
  else
    reportDrop(_activationId, QStringLiteral("the observation does not belong to the live activation"));
  return false;
}

/** Exactly one diagnostic per dropped observation, never an event (SPEC-M8 7). */
void SessionController::reportDrop(quint64 _activationId, const QString &_reason)
{
  // The diagnostic is a Qt warning, so it reaches the application's existing
  // message handler. A public signal or counter for it would be API that the
  // accepted documents do not define (SPEC-M8 6.2 exposes only the event signal).
  qWarning() << QStringLiteral("SessionController: dropped an observation of activation %1 - %2")
                    .arg(_activationId)
                    .arg(_reason);
}

void SessionController::enqueue(SessionEventType _type, SessionDirection _direction,
                                const QByteArray &_payload, qint64 _sourceTimestampNs,
                                const QJsonObject &_metadata)
{
  PendingObservation pending;
  pending.type = _type;
  pending.direction = _direction;
  pending.payload = _payload;   // copied byte for byte, no normalization (§8)
  pending.metadata = _metadata;
  pending.sourceTimestampNs = _sourceTimestampNs;

  mFifo.enqueue(pending);
  drainFifo();
}

/** FIFO delivery of SPEC-M8 10: in order, one event at a time, never nested. */
void SessionController::drainFifo()
{
  if (mDelivering)
    return;   // a nested call returns at once; the queued observation is emitted
              // by the loop below, after the current delivery has finished

  mDelivering = true;
  while (!mFifo.isEmpty()) {
    const PendingObservation pending = mFifo.dequeue();

    // An observation of the transport is mapped here. An event the controller
    // derived itself (the anomaly diagnostic) already carries its session time and
    // is never mapped a second time - that would rewrite its time and derive
    // another diagnostic from it.
    bool anomaly = false;
    const qint64 sessionTimestampNs = pending.sessionTimeMapped
        ? pending.sessionTimestampNs
        : mapSourceTime(pending.sourceTimestampNs, &anomaly);

    SessionEvent event;
    event.sequence = ++mLastSequence;
    event.sourceId = kSourceId;
    event.sourceTimestampNs = pending.sourceTimestampNs;   // raw, never rewritten
    event.timestampNs = sessionTimestampNs;
    event.type = pending.type;
    event.direction = pending.direction;
    event.payload = pending.payload;
    event.metadata = pending.metadata;
    emit eventObserved(event);

    if (anomaly) {
      // ADR-005: exactly one anomaly diagnostic per violating observation. It is a
      // complete event like every other one, so it goes through the same FIFO and
      // is delivered by this loop, in order, with the session time the triggering
      // event had to keep.
      PendingObservation diagnostic;
      diagnostic.type = SessionEventType::Error;
      diagnostic.direction = SessionDirection::None;
      diagnostic.metadata = anomalyMetadata();
      diagnostic.sourceTimestampNs = pending.sourceTimestampNs;
      diagnostic.sessionTimeMapped = true;
      diagnostic.sessionTimestampNs = sessionTimestampNs;
      mFifo.enqueue(diagnostic);
    }
  }
  mDelivering = false;
}

/** The ADR-005 mapping for the single clock domain of M8. */
qint64 SessionController::mapSourceTime(qint64 _sourceTimestampNs, bool *_anomaly)
{
  if (_anomaly != nullptr)
    *_anomaly = false;

  if (!mReferenceSet) {
    // The anchor event of the domain: the reference is taken from it, so it
    // carries session time 0. A failed-open Error is a legitimate anchor, and a
    // later activation or reopen never moves the reference.
    mReferenceSet = true;
    mReferenceSourceTimestampNs = _sourceTimestampNs;
    mLastEmittedSessionTimestampNs = 0;
    return 0;
  }

  const qint64 rawSessionTimestampNs = _sourceTimestampNs - mReferenceSourceTimestampNs;
  if (rawSessionTimestampNs < mLastEmittedSessionTimestampNs) {
    // Non-monotonic observation: emitted with the last emitted session time, its
    // payload and raw time untouched, plus one anomaly diagnostic.
    if (_anomaly != nullptr)
      *_anomaly = true;
    return mLastEmittedSessionTimestampNs;
  }

  mLastEmittedSessionTimestampNs = rawSessionTimestampNs;
  return rawSessionTimestampNs;
}
