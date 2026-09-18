/***************************************************************************
                      sessionrecordreader.h  -  Komport Serial Port Communicator
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

#ifndef SESSIONRECORDREADER_H
#define SESSIONRECORDREADER_H

// M9 test-side reader (SPEC-M9 section 5.9 and 7): parses a `.kpsession` v1 byte
// stream by hand from ADR-006's layout, deliberately *without* the production
// codec, so that a defect in the writer cannot be hidden by the reader sharing
// its code. It is test infrastructure only; M10 brings the real reader, and this
// one is neither its interface nor its implementation.

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <QtEndian>

#include "sessionevent.h"   // sessionJsonIntegerToQInt64, for the reference pair

/** ADR-006's reader limits, applied by this reader as well. */
static constexpr qint64 kReaderMaxHeaderBytes = 16 * 1024 * 1024;
static constexpr qint64 kReaderMaxRecordBodyBytes = 64 * 1024 * 1024;

/** One parsed record, with the metadata kept both raw and parsed. */
struct SessionRecordFileRecord {
  quint32 recordLength = 0;
  quint16 eventType = 0;
  quint8 direction = 0;
  quint8 flags = 0;
  quint64 sequence = 0;
  quint32 sourceId = 0;
  qint64 sourceTimestampNs = 0;
  qint64 timestampNs = 0;
  QByteArray metadataJson;      ///< empty when metadataLength was 0
  QJsonObject metadata;         ///< parsed from metadataJson
  QByteArray payload;
};

/** The result of parsing a `.kpsession` v1 byte stream. */
struct SessionRecordFile {
  bool loadable = false;              ///< magic, version, encoding and header are valid
  QString error;                      ///< why the file is not loadable
  QJsonObject header;
  QList<SessionRecordFileRecord> records;
  /** The file ends inside a record: ADR-006 ignores that record and keeps the
    *  complete prefix, which is why this is not an error. */
  bool truncatedFinalRecord = false;
  qint64 bytesConsumed = 0;
};

/** ADR-006 keeps 64-bit values as *canonical decimal strings*, so this reader
  *  accepts nothing else: no JSON numbers, no sign, no leading zero, no
  *  whitespace. (The production helper sessionJsonIntegerToQInt64() is more
  *  permissive; a reader that shares its tolerance would not catch a writer that
  *  emits a number where the profile requires a string.) */
inline bool isCanonicalDecimalString(const QJsonValue &value, qint64 *out)
{
  if (!value.isString())
    return false;
  const QString text = value.toString();
  if (text.isEmpty() || text.startsWith(QLatin1Char('-')) || text.startsWith(QLatin1Char('+'))
      || (text.size() > 1 && text.startsWith(QLatin1Char('0')))   // "0" itself is canonical
      || text != text.trimmed())
    return false;
  for (const QChar character : text) {
    if (!character.isDigit())
      return false;
  }
  bool ok = false;
  const qint64 parsed = text.toLongLong(&ok);
  if (!ok || QString::number(parsed) != text)
    return false;
  if (out != nullptr)
    *out = parsed;
  return true;
}

/** ADR-006's fixed v1 writer profile, validated independently of the production
  *  codec (SPEC-M9 section 7 requires the reader to check the profile's member
  *  names and the reference pair, not merely to parse them). */
inline QString profileViolation(const QJsonObject &header)
{
  const auto keysAre = [](const QJsonObject &object, const QStringList &expected) {
    QStringList actual = object.keys();
    actual.sort();
    QStringList wanted = expected;
    wanted.sort();
    return actual == wanted;
  };

  if (!keysAre(header, { QStringLiteral("application"), QStringLiteral("clockDomains"),
                         QStringLiteral("created"), QStringLiteral("format"),
                         QStringLiteral("sources"), QStringLiteral("version") }))
    return QStringLiteral("the header carries unknown or missing root members");
  if (header.value(QStringLiteral("format")).toString() != QStringLiteral("komport-session"))
    return QStringLiteral("wrong format member");
  // Fixed numeric members must hold exactly that number: a fractional value is not
  // the v1 profile, and a truncating comparison would hide it.
  const QJsonValue versionValue = header.value(QStringLiteral("version"));
  if (!versionValue.isDouble() || versionValue.toDouble() != 1.0)
    return QStringLiteral("wrong version member");
  if (!header.value(QStringLiteral("created")).isString())
    return QStringLiteral("created is not a string");
  const QDateTime created = QDateTime::fromString(header.value(QStringLiteral("created")).toString(),
                                                 Qt::ISODate);
  if (!created.isValid() || created.timeSpec() != Qt::UTC)
    return QStringLiteral("created is not an ISO-8601 UTC timestamp");

  const QJsonObject application = header.value(QStringLiteral("application")).toObject();
  if (!keysAre(application, { QStringLiteral("name"), QStringLiteral("version") })
      || !application.value(QStringLiteral("name")).isString()
      || !application.value(QStringLiteral("version")).isString())
    return QStringLiteral("the application object is not the fixed {name, version}");

  const QJsonArray sources = header.value(QStringLiteral("sources")).toArray();
  if (sources.size() != 1)
    return QStringLiteral("the sources array must hold exactly one entry");
  const QJsonObject source = sources.at(0).toObject();
  if (!keysAre(source, { QStringLiteral("clockDomainId"), QStringLiteral("configuration"),
                         QStringLiteral("name"), QStringLiteral("sourceId"),
                         QStringLiteral("transport") }))
    return QStringLiteral("the source descriptor carries unknown or missing members");
  const QJsonValue sourceIdValue = source.value(QStringLiteral("sourceId"));
  if (!sourceIdValue.isDouble() || sourceIdValue.toDouble() != 1.0)
    return QStringLiteral("the source ID is not 1");
  if (source.value(QStringLiteral("clockDomainId")).toString() != QStringLiteral("local-process-monotonic-v1"))
    return QStringLiteral("wrong clock domain id");
  if (source.value(QStringLiteral("name")).toString() != QStringLiteral("local serial"))
    return QStringLiteral("wrong source name");
  if (source.value(QStringLiteral("transport")).toString() != QStringLiteral("serial"))
    return QStringLiteral("wrong transport");

  const QJsonObject configuration = source.value(QStringLiteral("configuration")).toObject();
  if (!keysAre(configuration, { QStringLiteral("compatibility"), QStringLiteral("effective"),
                                QStringLiteral("localBuffering"), QStringLiteral("requested") }))
    return QStringLiteral("the configuration snapshot has the wrong sections");
  for (const QString &section : { QStringLiteral("requested"), QStringLiteral("effective") }) {
    const QJsonObject hardware = configuration.value(section).toObject();
    if (!keysAre(hardware, { QStringLiteral("baudRate"), QStringLiteral("dataBits"),
                             QStringLiteral("endpoint"), QStringLiteral("flowControl"),
                             QStringLiteral("parity"), QStringLiteral("stopBits") }))
      return QStringLiteral("the %1 section has the wrong members").arg(section);
    for (const QString &member : hardware.keys()) {
      if (!hardware.value(member).isString())
        return QStringLiteral("the %1 section's %2 is not a string").arg(section, member);
    }
  }
  const QJsonObject buffering = configuration.value(QStringLiteral("localBuffering")).toObject();
  if (!keysAre(buffering, { QStringLiteral("flushRate"), QStringLiteral("rxQueue") })
      || !buffering.value(QStringLiteral("flushRate")).isDouble()
      || !buffering.value(QStringLiteral("rxQueue")).isDouble())
    return QStringLiteral("the localBuffering section is not the fixed pair of numbers");
  const QJsonObject compatibility = configuration.value(QStringLiteral("compatibility")).toObject();
  if (!keysAre(compatibility, { QStringLiteral("startBits") })
      || !compatibility.value(QStringLiteral("startBits")).isString())
    return QStringLiteral("the compatibility section is not the fixed startBits string");

  const QJsonArray domains = header.value(QStringLiteral("clockDomains")).toArray();
  if (domains.size() != 1)
    return QStringLiteral("the clockDomains array must hold exactly one entry");
  const QJsonObject domain = domains.at(0).toObject();
  if (!keysAre(domain, { QStringLiteral("id"), QStringLiteral("kind"), QStringLiteral("reference") }))
    return QStringLiteral("the clock domain entry carries unknown or missing members");
  if (domain.value(QStringLiteral("id")).toString() != source.value(QStringLiteral("clockDomainId")).toString())
    return QStringLiteral("the source's clock domain does not exist in clockDomains");
  if (domain.value(QStringLiteral("kind")).toString() != QStringLiteral("process-monotonic"))
    return QStringLiteral("wrong clock domain kind");

  const QJsonObject reference = domain.value(QStringLiteral("reference")).toObject();
  if (!keysAre(reference, { QStringLiteral("sessionTimestampNs"), QStringLiteral("sourceTimestampNs") }))
    return QStringLiteral("the reference pair has the wrong members");
  qint64 anchorSourceNs = 0;
  qint64 anchorSessionNs = 1;
  if (!isCanonicalDecimalString(reference.value(QStringLiteral("sourceTimestampNs")), &anchorSourceNs)
      || !isCanonicalDecimalString(reference.value(QStringLiteral("sessionTimestampNs")), &anchorSessionNs))
    return QStringLiteral("the reference pair is not made of canonical decimal strings");
  if (anchorSessionNs != 0)
    return QStringLiteral("the reference session time is not 0");

  return QString();
}

/** Parse @p bytes as a `.kpsession` v1 file. */
inline SessionRecordFile readSessionRecordFile(const QByteArray &bytes)
{
  SessionRecordFile file;

  const auto fail = [&file](const QString &reason) {
    file.loadable = false;
    file.error = reason;
    return file;
  };

  if (bytes.size() < 16)
    return fail(QStringLiteral("shorter than a start block"));

  static const char kMagic[8] = { 'K', 'P', 'S', 'N', 0x1a, '\r', '\n', '\0' };
  if (bytes.left(8) != QByteArray(kMagic, 8))
    return fail(QStringLiteral("bad magic"));

  const quint16 version = qFromLittleEndian<quint16>(bytes.constData() + 8);
  if (version != 1)
    return fail(QStringLiteral("unsupported format version %1").arg(version));

  const quint16 encoding = qFromLittleEndian<quint16>(bytes.constData() + 10);
  if (encoding != 1)
    return fail(QStringLiteral("unsupported header encoding %1").arg(encoding));

  const quint32 headerLength = qFromLittleEndian<quint32>(bytes.constData() + 12);
  if (qint64(16) + qint64(headerLength) > bytes.size())
    return fail(QStringLiteral("the header length exceeds the file"));
  if (qint64(headerLength) > kReaderMaxHeaderBytes)
    return fail(QStringLiteral("the header exceeds ADR-006's 16 MiB limit"));

  const QByteArray headerBytes = bytes.mid(16, int(headerLength));
  const QJsonDocument headerDocument = QJsonDocument::fromJson(headerBytes);
  if (!headerDocument.isObject())
    return fail(QStringLiteral("the header is not a JSON object"));
  file.header = headerDocument.object();

  // The profile is part of the format: a writer that deviates is as broken as one
  // that writes bad lengths, so the reader refuses rather than merely parsing.
  const QString violation = profileViolation(file.header);
  if (!violation.isEmpty())
    return fail(QStringLiteral("the header violates the v1 writer profile: %1").arg(violation));

  qint64 offset = 16 + qint64(headerLength);
  while (offset < bytes.size()) {
    const qint64 remaining = bytes.size() - offset;
    if (remaining < 44) {
      file.truncatedFinalRecord = true;
      break;
    }

    const char *prefix = bytes.constData() + offset;
    const quint32 recordLength = qFromLittleEndian<quint32>(prefix);
    if (qint64(4) + qint64(recordLength) > remaining) {
      file.truncatedFinalRecord = true;
      break;
    }

    SessionRecordFileRecord record;
    record.recordLength = recordLength;
    record.eventType = qFromLittleEndian<quint16>(prefix + 4);
    record.direction = quint8(prefix[6]);
    record.flags = quint8(prefix[7]);
    record.sequence = qFromLittleEndian<quint64>(prefix + 8);
    record.sourceId = qFromLittleEndian<quint32>(prefix + 16);
    record.sourceTimestampNs = qFromLittleEndian<qint64>(prefix + 20);
    record.timestampNs = qFromLittleEndian<qint64>(prefix + 28);
    const quint32 metadataLength = qFromLittleEndian<quint32>(prefix + 36);
    const quint32 payloadLength = qFromLittleEndian<quint32>(prefix + 40);

    if (qint64(40) + qint64(metadataLength) + qint64(payloadLength) != qint64(recordLength))
      return fail(QStringLiteral("record %1 has inconsistent lengths").arg(file.records.size()));
    if (qint64(40) + qint64(metadataLength) + qint64(payloadLength) > kReaderMaxRecordBodyBytes)
      return fail(QStringLiteral("record %1 exceeds ADR-006's 64 MiB limit").arg(file.records.size()));
    if (record.flags != 0)
      return fail(QStringLiteral("record %1 has non-zero flags").arg(file.records.size()));

    record.metadataJson = bytes.mid(int(offset) + 44, int(metadataLength));
    if (!record.metadataJson.isEmpty()) {
      const QJsonDocument metadataDocument = QJsonDocument::fromJson(record.metadataJson);
      if (!metadataDocument.isObject())
        return fail(QStringLiteral("record %1 has non-object metadata").arg(file.records.size()));
      record.metadata = metadataDocument.object();
    }
    record.payload = bytes.mid(int(offset) + 44 + int(metadataLength), int(payloadLength));

    file.records.append(record);
    offset += 44 + qint64(metadataLength) + qint64(payloadLength);
  }

  file.bytesConsumed = offset;
  file.loadable = true;
  return file;
}

#endif // SESSIONRECORDREADER_H
