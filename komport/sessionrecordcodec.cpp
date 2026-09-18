/***************************************************************************
                      sessionrecordcodec.cpp  -  Komport Serial Port Communicator
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

#include "sessionrecordcodec.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

#include <QtEndian>

#include <cmath>

namespace {

/** Append an integer in the little-endian width ADR-006 fixes for it. */
template <typename T>
void appendLittleEndian(QByteArray &buffer, T value)
{
  const T little = qToLittleEndian(value);
  buffer.append(reinterpret_cast<const char *>(&little), sizeof(T));
}

/** Saturating addition: a size estimate must never wrap around. */
qint64 saturatedAdd(qint64 a, qint64 b)
{
  constexpr qint64 kCap = qint64(1) << 62;
  if (a >= kCap - b)
    return kCap;
  return a + b;
}

/** Exact compact-JSON size of a string, without serialising it.
  *
  * Mirrors what the compact JSON writer emits: two quotes, the short escapes
  *  for quote/backslash/bell/backspace/formfeed/newline/return/tab, "\u00XX" for
  *  the remaining control characters, "\uXXXX" for a surrogate that is not part
  *  of a valid pair, and otherwise the UTF-8 byte count of the character (one
  *  four-byte sequence per surrogate pair). Non-ASCII characters are *not*
  *  escaped by that writer, so they are counted, not inflated.
  */
qint64 jsonStringSize(const QString &text)
{
  qint64 size = 2;   // the enclosing quotes
  for (int i = 0; i < text.size(); ++i) {
    const ushort unit = text.at(i).unicode();
    switch (unit) {
      case u'"':
      case u'\\':
      case u'\b':
      case u'\f':
      case u'\n':
      case u'\r':
      case u'\t':
        size += 2;
        break;
      default:
        if (unit < 0x20) {
          size += 6;                                          // "\u00XX"
        } else if (unit < 0x80) {
          size += 1;                                          // plain ASCII
        } else if (QChar::isHighSurrogate(unit) && i + 1 < text.size()
                   && QChar::isLowSurrogate(text.at(i + 1).unicode())) {
          size += 4;                                          // one 4-byte sequence
          ++i;
        } else if (QChar::isSurrogate(unit)) {
          // A surrogate that is not part of a valid pair is written escaped, as
          // \uXXXX - not as a replacement character.
          size += 6;
        } else if (unit < 0x800) {
          size += 2;
        } else {
          size += 3;
        }
        break;
    }
  }
  return size;
}

/** Exact compact-JSON size of one number.
  *
  * Measured with the JSON writer itself - a one-element array around the value
  * costs two bytes of syntax - so the count is exact without this codec having to
  * replicate the writer's double formatting. The allocation is a handful of
  * bytes per number, never the size of the unit being sized.
  */
qint64 jsonNumberSize(double value)
{
  const QByteArray wrapped = QJsonDocument(QJsonArray{ QJsonValue(value) }).toJson(QJsonDocument::Compact);
  return qMax<qint64>(0, qint64(wrapped.size()) - 2);
}

/** Compact-JSON size of a value, computed without serialising it.
  *
  * Exact for every JSON value shape: commas are counted only *between* members
  * and elements, strings carry their real escape and UTF-8 lengths, and numbers
  * are measured through the writer (jsonNumberSize). This must stay exact - the
  * refusal decision depends on it, and SPEC-M9 section 5.3 requires one record
  * per accepted event, so an over-estimate would drop data that fits.
  */
qint64 compactJsonSizeOf(const QJsonValue &value)
{
  switch (value.type()) {
    case QJsonValue::Object: {
      qint64 total = 2;   // the braces
      bool first = true;
      const QJsonObject object = value.toObject();
      for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!first)
          total = saturatedAdd(total, 1);   // the comma *between* members
        first = false;
        total = saturatedAdd(total, jsonStringSize(it.key()));
        total = saturatedAdd(total, 1);     // the colon
        total = saturatedAdd(total, compactJsonSizeOf(it.value()));
      }
      return total;
    }
    case QJsonValue::Array: {
      qint64 total = 2;   // the brackets
      bool first = true;
      const QJsonArray array = value.toArray();
      for (const QJsonValue &item : array) {
        if (!first)
          total = saturatedAdd(total, 1);   // the comma *between* elements
        first = false;
        total = saturatedAdd(total, compactJsonSizeOf(item));
      }
      return total;
    }
    case QJsonValue::String:
      return jsonStringSize(value.toString());
    case QJsonValue::Double:
      return jsonNumberSize(value.toDouble());
    case QJsonValue::Bool:
      return value.toBool() ? 4 : 5;
    case QJsonValue::Null:
      return 4;
    case QJsonValue::Undefined:
      // An explicit undefined value - a missing object lookup inside a metadata
      // array, for instance - which the writer emits as `null`. (A default
      // constructed QJsonValue is Null, not Undefined.)
      return 4;
  }
  return 0;
}

/** Refuse a JSON tree whose size exceeds the budget, before it is serialised
  *  into an allocation of that size. */
bool jsonFitsBudget(const QJsonValue &value, qint64 budgetBytes, const QString &what,
                    const QString &limitLabel, QString *reason)
{
  if (compactJsonSizeOf(value) > budgetBytes) {
    if (reason != nullptr)
      *reason = QStringLiteral("the %1 JSON cannot fit the %2 limit").arg(what, limitLabel);
    return false;
  }
  return true;
}

/** The six hardware members of ADR-006's configuration schema, in sorted order
  *  because they are compared against QJsonObject::keys(), which is sorted. */
const QStringList &hardwareMembers()
{
  static const QStringList members({ QStringLiteral("baudRate"), QStringLiteral("dataBits"),
                                     QStringLiteral("endpoint"), QStringLiteral("flowControl"),
                                     QStringLiteral("parity"), QStringLiteral("stopBits") });
  return members;
}

/** Check one hardware section of the configuration snapshot: exactly the six
  *  ADR-006 members, every one a JSON string. */
bool hardwareSectionIsConformant(const QJsonValue &section, const QString &name, QString *reason)
{
  if (!section.isObject()) {
    if (reason != nullptr)
      *reason = QStringLiteral("the configuration %1 section is not a JSON object").arg(name);
    return false;
  }
  const QJsonObject object = section.toObject();
  QStringList actual = object.keys();
  actual.sort();
  for (const QString &member : hardwareMembers()) {
    if (!object.contains(member)) {
      if (reason != nullptr)
        *reason = QStringLiteral("the configuration %1 section is missing %2").arg(name, member);
      return false;
    }
    if (!object.value(member).isString()) {
      if (reason != nullptr)
        *reason = QStringLiteral("the configuration %1 section's %2 is not a string").arg(name, member);
      return false;
    }
  }
  if (actual != hardwareMembers()) {
    if (reason != nullptr)
      *reason = QStringLiteral("the configuration %1 section carries a member ADR-006's profile does not allow").arg(name);
    return false;
  }
  return true;
}

/** Validate the applied configuration snapshot against ADR-006's exhaustive
  *  nested schema: no missing member, no additional member, no wrong type.
  *
  * A snapshot is not a configuration *result*: per-transaction fields such as
  * `applyStatus`, `changedGroups` or a message are refused rather than written.
  */
bool configurationIsConformant(const QJsonObject &configuration, QString *reason)
{
  if (configuration.isEmpty()) {
    if (reason != nullptr)
      *reason = QStringLiteral("the configuration snapshot is empty");
    return false;
  }

  QStringList actual = configuration.keys();
  actual.sort();
  const QStringList expected({ QStringLiteral("compatibility"), QStringLiteral("effective"),
                               QStringLiteral("localBuffering"), QStringLiteral("requested") });
  if (actual != expected) {
    if (reason != nullptr) {
      const QStringList unexpected = [&actual, &expected]() {
        QStringList extra;
        for (const QString &key : actual) {
          if (!expected.contains(key))
            extra.append(key);
        }
        return extra;
      }();
      *reason = unexpected.isEmpty()
                    ? QStringLiteral("the configuration snapshot is missing a required section")
                    : QStringLiteral("the configuration snapshot carries an unsupported section: %1")
                          .arg(unexpected.join(QStringLiteral(", ")));
    }
    return false;
  }

  if (!hardwareSectionIsConformant(configuration.value(QStringLiteral("requested")),
                                   QStringLiteral("requested"), reason))
    return false;
  if (!hardwareSectionIsConformant(configuration.value(QStringLiteral("effective")),
                                   QStringLiteral("effective"), reason))
    return false;

  const QJsonValue bufferingValue = configuration.value(QStringLiteral("localBuffering"));
  if (!bufferingValue.isObject()) {
    if (reason != nullptr)
      *reason = QStringLiteral("the configuration localBuffering section is not a JSON object");
    return false;
  }
  const QJsonObject buffering = bufferingValue.toObject();
  QStringList bufferingKeys = buffering.keys();
  bufferingKeys.sort();
  if (bufferingKeys != QStringList({ QStringLiteral("flushRate"), QStringLiteral("rxQueue") })) {
    if (reason != nullptr)
      *reason = QStringLiteral("the configuration localBuffering section needs exactly rxQueue and flushRate");
    return false;
  }
  for (const QString &member : { QStringLiteral("rxQueue"), QStringLiteral("flushRate") }) {
    const QJsonValue value = buffering.value(member);
    // ADR-006 keeps 32-bit quantities as JSON numbers, so these two members are
    // only conformant as finite, integral numbers in the 32-bit range. The range
    // test comes first: converting an out-of-range double to an integer is
    // undefined and must never happen.
    const double asDouble = value.toDouble();
    constexpr double kMaxInt32 = 2147483647.0;
    if (!value.isDouble() || !std::isfinite(asDouble) || std::trunc(asDouble) != asDouble
        || asDouble < 0.0 || asDouble > kMaxInt32) {
      if (reason != nullptr) {
        *reason = QStringLiteral("the configuration buffering member %1 is not an integral JSON number in the 32-bit range")
                      .arg(member);
      }
      return false;
    }
  }

  const QJsonValue compatibilityValue = configuration.value(QStringLiteral("compatibility"));
  if (!compatibilityValue.isObject()) {
    if (reason != nullptr)
      *reason = QStringLiteral("the configuration compatibility section is not a JSON object");
    return false;
  }
  const QJsonObject compatibility = compatibilityValue.toObject();
  QStringList compatibilityKeys = compatibility.keys();
  compatibilityKeys.sort();
  if (compatibilityKeys != QStringList({ QStringLiteral("startBits") })
      || !compatibility.value(QStringLiteral("startBits")).isString()) {
    if (reason != nullptr)
      *reason = QStringLiteral("the configuration compatibility section needs exactly the string startBits");
    return false;
  }

  if (reason != nullptr)
    reason->clear();
  return true;
}

/** The JSON header object of ADR-006's v1 writer profile. Internal: the codec's
  *  contract is the byte block, and the profile is a property of it. */
QJsonObject headerObject(const SessionHeaderFacts &facts)
{
  QJsonObject application;
  application.insert(QStringLiteral("name"), facts.applicationName);
  application.insert(QStringLiteral("version"), facts.applicationVersion);

  QJsonObject source;
  source.insert(QStringLiteral("sourceId"), static_cast<qint64>(SessionRecordCodec::kSourceId));
  source.insert(QStringLiteral("clockDomainId"), SessionRecordCodec::clockDomainId());
  source.insert(QStringLiteral("name"), SessionRecordCodec::sourceName());
  source.insert(QStringLiteral("transport"), SessionRecordCodec::transport());
  source.insert(QStringLiteral("configuration"), facts.configuration);

  QJsonObject reference;
  reference.insert(QStringLiteral("sourceTimestampNs"), sessionJsonInteger(facts.referenceSourceTimestampNs));
  reference.insert(QStringLiteral("sessionTimestampNs"),
                   sessionJsonInteger(SessionRecordCodec::kReferenceSessionTimestampNs));

  QJsonObject clockDomain;
  clockDomain.insert(QStringLiteral("id"), SessionRecordCodec::clockDomainId());
  clockDomain.insert(QStringLiteral("kind"), SessionRecordCodec::clockDomainKind());
  clockDomain.insert(QStringLiteral("reference"), reference);

  QJsonObject header;
  header.insert(QStringLiteral("format"), QStringLiteral("komport-session"));
  header.insert(QStringLiteral("version"), 1);
  header.insert(QStringLiteral("created"), facts.createdUtc.toUTC().toString(Qt::ISODate));
  header.insert(QStringLiteral("application"), application);
  header.insert(QStringLiteral("sources"), QJsonArray{ source });
  header.insert(QStringLiteral("clockDomains"), QJsonArray{ clockDomain });
  return header;
}

} // namespace

qint64 SessionRecordCodec::compactJsonSize(const QJsonValue &value)
{
  return compactJsonSizeOf(value);
}

QString SessionRecordCodec::clockDomainId()
{
  return QStringLiteral("local-process-monotonic-v1");
}

QString SessionRecordCodec::sourceName()
{
  return QStringLiteral("local serial");
}

QString SessionRecordCodec::transport()
{
  return QStringLiteral("serial");
}

QString SessionRecordCodec::clockDomainKind()
{
  return QStringLiteral("process-monotonic");
}

QByteArray SessionRecordCodec::magic()
{
  // ADR-006: "KPSN" followed by 0x1A, CR, LF, NUL. Written as a fixed byte
  // array because the magic contains a NUL and must keep its exact length.
  static const char kMagic[8] = { 'K', 'P', 'S', 'N', 0x1a, '\r', '\n', '\0' };
  return QByteArray(kMagic, 8);
}

bool SessionRecordCodec::recordSizesFit(qint64 metadataBytes, qint64 payloadBytes, QString *reason)
{
  const auto fail = [reason](const QString &why) {
    if (reason != nullptr)
      *reason = why;
    return false;
  };

  if (metadataBytes < 0 || payloadBytes < 0)
    return fail(QStringLiteral("a record component length must not be negative"));

  // The lengths are written as u32 by ADR-006; a value that does not fit would
  // silently truncate in the file.
  constexpr qint64 kMaxU32 = static_cast<qint64>(0xFFFFFFFFu);
  if (metadataBytes > kMaxU32)
    return fail(QStringLiteral("the metadata JSON exceeds the u32 metadataLength field"));
  if (payloadBytes > kMaxU32)
    return fail(QStringLiteral("the payload exceeds the u32 payloadLength field"));

  // ADR-006's reader limit, applied by the writer: the record body is the 40
  // prefix bytes after recordLength plus the two content sections. Checked
  // before any allocation, so an oversized event cannot even be assembled.
  const qint64 recordBodyBytes = kRecordBodyPrefixSize + metadataBytes + payloadBytes;
  if (recordBodyBytes > kMaxRecordBodyBytes)
    return fail(QStringLiteral("the record body exceeds ADR-006's 64 MiB limit"));

  if (reason != nullptr)
    reason->clear();
  return true;
}

SessionRecordEncoding SessionRecordCodec::encodeHeader(const SessionHeaderFacts &facts)
{
  SessionRecordEncoding result;

  // ADR-006 requires a wall-clock creation time; an invalid QDateTime would
  // serialise as an empty string, which is not a conformant header.
  if (!facts.createdUtc.isValid()) {
    result.reason = QStringLiteral("the creation time is not a valid QDateTime");
    return result;
  }

  // 1. The snapshot must satisfy ADR-006's exhaustive schema. This runs first,
  //    so a non-conformant snapshot is refused before anything is serialised.
  QString configurationReason;
  if (!configurationIsConformant(facts.configuration, &configurationReason)) {
    result.reason = QStringLiteral("the configuration snapshot is not conformant: %1")
                        .arg(configurationReason);
    return result;
  }

  const QJsonObject header = headerObject(facts);

  // 2. Bound the JSON before serialising it: the upper bound is computed from
  //    the tree without building the byte array (SPEC-M9 section 5.3).
  QString budgetReason;
  if (!jsonFitsBudget(header, kMaxHeaderBytes, QStringLiteral("header"),
                      QStringLiteral("16 MiB"), &budgetReason)) {
    result.reason = budgetReason;
    return result;
  }

  const QByteArray headerJson = QJsonDocument(header).toJson(QJsonDocument::Compact);
  // 3. Belt and braces: the exact size must satisfy the same limit.
  if (headerJson.size() > kMaxHeaderBytes) {
    result.reason = QStringLiteral("the JSON header exceeds ADR-006's 16 MiB limit");
    return result;
  }

  QByteArray buffer;
  buffer.reserve(magic().size() + 4 + 4 + headerJson.size());
  buffer.append(magic());
  appendLittleEndian<quint16>(buffer, 1);  // format version
  appendLittleEndian<quint16>(buffer, 1);  // header encoding: UTF-8 JSON
  appendLittleEndian<quint32>(buffer, static_cast<quint32>(headerJson.size()));
  buffer.append(headerJson);

  result.bytes = buffer;
  result.ok = true;
  return result;
}

SessionRecordEncoding SessionRecordCodec::encodeRecord(const SessionEvent &event)
{
  SessionRecordEncoding result;

  // A malformed event is a producer defect, never data to persist
  // (SPEC-M9 section 5.3).
  QString invalidReason;
  if (!isValidSessionEvent(event, &invalidReason)) {
    result.reason = QStringLiteral("the event is not a valid SessionEvent: %1").arg(invalidReason);
    return result;
  }

  const qint64 payloadBytes = static_cast<qint64>(event.payload.size());

  // An empty metadata object is the zero-length encoding of ADR-006, never the
  // two bytes of "{}". A non-empty one is bounded before it is serialised: it
  // may not consume more than the record body has left.
  QByteArray metadataJson;
  if (!event.metadata.isEmpty()) {
    QString budgetReason;
    if (!jsonFitsBudget(event.metadata, kMaxRecordBodyBytes - kRecordBodyPrefixSize - payloadBytes,
                        QStringLiteral("metadata"), QStringLiteral("64 MiB record body"),
                        &budgetReason)) {
      result.reason = budgetReason;
      return result;
    }
    metadataJson = QJsonDocument(event.metadata).toJson(QJsonDocument::Compact);
  }

  const qint64 metadataBytes = static_cast<qint64>(metadataJson.size());

  QString sizeReason;
  if (!recordSizesFit(metadataBytes, payloadBytes, &sizeReason)) {
    result.reason = sizeReason;
    return result;
  }

  const quint32 recordLength = static_cast<quint32>(kRecordBodyPrefixSize + metadataBytes + payloadBytes);

  QByteArray buffer;
  buffer.reserve(kRecordPrefixSize + metadataBytes + payloadBytes);
  appendLittleEndian<quint32>(buffer, recordLength);
  appendLittleEndian<quint16>(buffer, static_cast<quint16>(event.type));
  appendLittleEndian<quint8>(buffer, static_cast<quint8>(event.direction));
  appendLittleEndian<quint8>(buffer, 0);  // flags: zero in v1 (ADR-006)
  appendLittleEndian<quint64>(buffer, event.sequence);
  appendLittleEndian<quint32>(buffer, event.sourceId);
  appendLittleEndian<qint64>(buffer, event.sourceTimestampNs);
  appendLittleEndian<qint64>(buffer, event.timestampNs);
  appendLittleEndian<quint32>(buffer, static_cast<quint32>(metadataBytes));
  appendLittleEndian<quint32>(buffer, static_cast<quint32>(payloadBytes));
  buffer.append(metadataJson);
  buffer.append(event.payload);

  result.bytes = buffer;
  result.ok = true;
  return result;
}
