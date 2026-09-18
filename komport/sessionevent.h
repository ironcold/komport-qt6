/***************************************************************************
                          sessionevent.h  -  Komport Serial Port Communicator
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

#ifndef SESSIONEVENT_H
#define SESSIONEVENT_H

// ADR-002 (accepted 2026-09-18): SessionEvent v1 is the only model crossing
// session, recorder, decoder, replay and future remote boundaries.
//
// This header is part of the public session/transport contract: it uses QtCore
// only and must never gain a dependency on QWidget, KomportApp or any other
// executable-specific type (ADR-009). The compile proof for that rule is
// tests/session_contract_compile.cpp, built against Qt6::Core alone.

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cmath>

/** Which way Komport moved bytes towards its transport peer (ADR-004). */
enum class SessionDirection : quint8 {
  None = 0, ///< non-data event
  Tx = 1,   ///< Komport sent bytes towards its transport peer
  Rx = 2    ///< Komport received bytes from its transport peer
};

/** The kinds of event a session stream can carry (ADR-002). */
enum class SessionEventType : quint16 {
  Data = 1,
  TransportOpened = 2,
  TransportClosed = 3,
  TransportConfigChanged = 4,
  LineStateChanged = 5,
  Error = 6,
  Annotation = 7,
  Bookmark = 8
};

/** One event of a session stream (ADR-002).
  *
  * The struct is deliberately a plain aggregate: M8 requires the *producer* to
  * satisfy the contract (see isValidSessionEvent()), not a type-level encoding.
  * A consumer receives events as const references or copies and must not modify
  * them or replace stored events with derived data.
  *
  * `sourceTimestampNs` is the raw monotonic observation time of the source's
  * clock domain and is never rewritten; `timestampNs` is the derived session
  * time from ADR-005's mapping (see SessionController).
  */
struct SessionEvent {
  quint64 sequence = 0;                 ///< starts at 1, strictly increasing within a session
  quint32 sourceId = 0;                 ///< non-zero for physical/event sources; 0 is reserved
  qint64 sourceTimestampNs = 0;         ///< raw source time, non-negative, never rewritten
  qint64 timestampNs = 0;               ///< derived session time, non-negative
  SessionEventType type = SessionEventType::Data;
  SessionDirection direction = SessionDirection::None;
  QByteArray payload;                   ///< non-empty for Data, empty otherwise
  QJsonObject metadata;                 ///< empty for Data in M8
};

/** Structural (event-local, stateless) validation of an event.
  *
  * This is the first of the two validation parts ADR-002 defines. The second,
  * stream validation (sequence starts at one and strictly increases, session
  * time is non-decreasing within a clock domain, sources resolve to their clock
  * domain), is stateful and belongs to the producing SessionController.
  *
  * @param event the event to check
  * @param reason optional; receives a human-readable reason on failure
  * @return true if the event satisfies every event-local rule
  */
inline bool isValidSessionEvent(const SessionEvent &event, QString *reason = nullptr)
{
  const auto fail = [reason](const QString &why) {
    if (reason != nullptr)
      *reason = why;
    return false;
  };

  // ADR-002: `type` must be one of the declared enumerators. A cast from an
  // arbitrary integer is not a valid event, so every declared type is listed
  // explicitly and the default case is a rejection.
  bool isData = false;
  switch (event.type) {
    case SessionEventType::Data:
      isData = true;
      break;
    case SessionEventType::TransportOpened:
    case SessionEventType::TransportClosed:
    case SessionEventType::TransportConfigChanged:
    case SessionEventType::LineStateChanged:
    case SessionEventType::Error:
    case SessionEventType::Annotation:
    case SessionEventType::Bookmark:
      break;
    default:
      return fail(QStringLiteral("type is not a declared SessionEventType enumerator"));
  }

  if (isData) {
    if (event.direction != SessionDirection::Tx && event.direction != SessionDirection::Rx)
      return fail(QStringLiteral("a Data event needs direction Tx or Rx"));
    if (event.payload.isEmpty())
      return fail(QStringLiteral("a Data event needs a non-empty payload"));
  } else {
    if (event.direction != SessionDirection::None)
      return fail(QStringLiteral("a non-data event needs direction None"));
    if (!event.payload.isEmpty())
      return fail(QStringLiteral("a non-data event must not carry a payload"));
  }
  if (event.sourceId == 0)
    return fail(QStringLiteral("sourceId 0 is reserved for future session-wide non-data events"));
  if (event.sourceTimestampNs < 0)
    return fail(QStringLiteral("sourceTimestampNs must not be negative"));
  if (event.timestampNs < 0)
    return fail(QStringLiteral("timestampNs must not be negative"));

  if (reason != nullptr)
    reason->clear();
  return true;
}

// ---------------------------------------------------------------------------
// Lossless 64-bit values in JSON (ADR-002 / ADR-006).
//
// JSON numbers are IEEE-754 doubles and lose integer precision beyond 2^53
// (about 104 days of nanoseconds), so every 64-bit quantity that appears in
// JSON - the sequence number, both timestamps and any future 64-bit mapping
// value - is written as a canonical decimal string. 32-bit quantities keep
// their widths and stay JSON numbers. Readers accept both forms and never
// truncate silently.
// ---------------------------------------------------------------------------

/** Encode a 64-bit value for JSON as a canonical decimal string. */
inline QJsonValue sessionJsonInteger(qint64 value)
{
  return QJsonValue(QString::number(value));
}

/** Decode a 64-bit value from JSON.
  *
  * Accepts the canonical decimal string and, for compatibility with
  * hand-written headers, an integral JSON number. An integral JSON number is
  * accepted only while its magnitude stays inside the range where doubles
  * represent *every* integer (|v| <= 2^53); beyond that a JSON number cannot be
  * trusted to carry the intended 64-bit value, so the canonical string is
  * required. Fractional values, non-numeric strings and non-numeric types are
  * rejected.
  *
  * @param value the JSON value to decode
  * @param out receives the decoded value on success
  * @return true if the value was decoded exactly
  */
inline bool sessionJsonIntegerToQInt64(const QJsonValue &value, qint64 *out)
{
  if (out == nullptr)
    return false;
  if (value.isString()) {
    bool ok = false;
    const qint64 parsed = value.toString().toLongLong(&ok);
    if (!ok)
      return false;
    *out = parsed;
    return true;
  }
  if (value.isDouble()) {
    const double asDouble = value.toDouble();
    const double rounded = std::nearbyint(asDouble);
    if (asDouble != rounded)
      return false;
    // An exact comparison is only meaningful inside the range where doubles
    // represent integers without gaps.
    constexpr double kExactIntegerLimit = 9007199254740992.0; // 2^53
    if (rounded > kExactIntegerLimit || rounded < -kExactIntegerLimit)
      return false;
    *out = static_cast<qint64>(rounded);
    return true;
  }
  return false;
}

#endif // SESSIONEVENT_H
