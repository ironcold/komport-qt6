/***************************************************************************
                          itransport.h  -  Komport Serial Port Communicator
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

#ifndef ITRANSPORT_H
#define ITRANSPORT_H

// ADR-003 (accepted 2026-09-18): ITransport v1 owns byte movement only. It
// neither stores sessions nor decodes bytes and does not assign session time -
// that is the SessionController's mapping (ADR-005).
//
// This header is part of the public session/transport contract: it uses QtCore
// only and must never gain a dependency on QWidget, KomportApp or any other
// executable-specific type (ADR-009).
//
// Everything below except the base class itself is frozen for M8: an
// implementation that adds a byte-transmitting entry point which does not route
// through the single observed write primitive (ADR-003) would be the bypass the
// ADR forbids.

#include <QByteArray>
#include <QJsonObject>
#include <QObject>

/** A source of byte movement: local serial today, later TCP or a remote agent.
  *
  * Subclasses must satisfy the ADR-003 conformance rules:
  *
  * - `activationId` starts at 1 for a transport instance and strictly increases
  *   on every `open()` attempt; a failed attempt consumes an id and never
  *   produces an `opened` for it.
  * - Every observation of an activation carries that activation's id.
  * - `closed()` is the last signal a transport *emits* for an activation, and a
  *   non-error observation is never emitted for an activation id that never
  *   produced an `opened`; the single `transportError` of a failed `open()` is
  *   the only exception.
  * - An attempt's failure is reported exactly once, using an internal
  *   in-progress/result guard, never by comparing human-readable text.
  * - `open()` reports its own result synchronously (a transport whose open
  *   completes asynchronously is not part of v1).
  * - Timestamps are non-negative monotonic nanoseconds in this source's clock
  *   domain, taken from one clock for the lifetime of that domain - never a
  *   per-activation zero.
  */
class ITransport : public QObject {
Q_OBJECT
public:
  explicit ITransport(QObject *parent = nullptr);
  ~ITransport() override;

  /** Open the transport. Reports its own result synchronously. */
  virtual bool open() = 0;
  /** Close the transport. Emits exactly one `closed` for the current activation. */
  virtual void close() = 0;
  /** Is the transport open? */
  virtual bool isOpen() const = 0;
  /** Send bytes.
    *
    * @return the number of bytes this transport accepted (0..bytes.size());
    *         a negative value means the write was refused entirely. A partial
    *         result must not be presented to a caller as success.
    */
  virtual qint64 writeBytes(const QByteArray &bytes) = 0;

signals:
  /** Bytes were observed from the peer. One signal per non-empty read. */
  void bytesReceived(quint64 activationId, const QByteArray &bytes, qint64 sourceTimestampNs);
  /** Bytes were accepted by the transport API. "Accepted" never means
    * "physically delivered"; a partial write reports exactly the accepted
    * prefix. */
  void bytesWritten(quint64 activationId, const QByteArray &bytes, qint64 sourceTimestampNs);
  /** An activation was opened successfully. */
  void opened(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
  /** The current activation ended. The last signal emitted for it. */
  void closed(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
  /** A configuration transaction result. See SPEC-M8 section 6.2 for exactly
    * which observations a transaction produces for which outcome. */
  void configurationChanged(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
  /** A modem line changed state. Part of the frozen surface, unimplemented
    * until a transport actually exposes modem lines. */
  void lineStateChanged(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
  /** An error was observed. Metadata carries at least `kind` (open, apply,
    * write, runtime), a stable machine-readable `code`, a human-readable
    * `message` and non-secret numeric context - never credentials. */
  void transportError(quint64 activationId, qint64 sourceTimestampNs, const QJsonObject &metadata);
};

#endif // ITRANSPORT_H
