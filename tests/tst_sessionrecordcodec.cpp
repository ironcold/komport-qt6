/***************************************************************************
                      tst_sessionrecordcodec.cpp  -  Komport Serial Port Communicator
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

// Byte-level tests of the `.kpsession` v1 assembly (SPEC-M9 section 7, unit
// tests of the codec): no file I/O, no session, no GUI. Every assertion here is
// against ADR-006's fixed layout and its normative v1 writer profile.

#include "sessionrecordcodec.h"

#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>

namespace {

/** A structurally valid Data event, Tx direction (isValidSessionEvent holds). */
SessionEvent dataEvent(quint64 sequence = 1, const QByteArray &payload = QByteArrayLiteral("AB"))
{
  SessionEvent event;
  event.sequence = sequence;
  event.sourceId = 1;
  event.sourceTimestampNs = 1000;
  event.timestampNs = 250;
  event.type = SessionEventType::Data;
  event.direction = SessionDirection::Tx;
  event.payload = payload;
  return event;
}

/** Header facts with a complete, conformant applied-configuration snapshot. */
SessionHeaderFacts headerFacts()
{
  QJsonObject requested;
  requested.insert(QStringLiteral("endpoint"), QStringLiteral("/dev/pts/6"));
  requested.insert(QStringLiteral("baudRate"), QStringLiteral("9600"));
  requested.insert(QStringLiteral("dataBits"), QStringLiteral("8"));
  requested.insert(QStringLiteral("stopBits"), QStringLiteral("1"));
  requested.insert(QStringLiteral("parity"), QStringLiteral("NONE"));
  requested.insert(QStringLiteral("flowControl"), QStringLiteral("NONE"));

  QJsonObject effective = requested;
  effective.insert(QStringLiteral("baudRate"), QStringLiteral("9600"));

  QJsonObject buffering;
  buffering.insert(QStringLiteral("rxQueue"), 1024);
  buffering.insert(QStringLiteral("flushRate"), 256);

  QJsonObject compatibility;
  compatibility.insert(QStringLiteral("startBits"), QStringLiteral("1"));

  QJsonObject configuration;
  configuration.insert(QStringLiteral("requested"), requested);
  configuration.insert(QStringLiteral("effective"), effective);
  configuration.insert(QStringLiteral("localBuffering"), buffering);
  configuration.insert(QStringLiteral("compatibility"), compatibility);

  SessionHeaderFacts facts;
  facts.applicationName = QStringLiteral("Komport");
  facts.applicationVersion = QStringLiteral("2.0-test");
  facts.createdUtc = QDateTime::fromString(QStringLiteral("2026-09-18T12:00:00Z"), Qt::ISODate);
  facts.configuration = configuration;
  return facts;
}

/** Sorted member names of a JSON object, for exact set comparisons. */
QStringList memberNames(const QJsonObject &object)
{
  QStringList names = object.keys();
  names.sort();
  return names;
}

} // namespace

class TstSessionRecordCodec : public QObject
{
  Q_OBJECT

private slots:
  void headerProfileMatchesAdr006();
  void recordPrefixIsFortyFourBytesInTheAgreedOrder();
  void emptyMetadataIsEncodedAsLengthZero();
  void sixtyFourBitJsonValuesAreCanonicalDecimalStrings();
  void oversizedHeaderIsRejected();
  void oversizedRecordIsRejected();
  void lengthArithmeticIsChecked();
  void recordBytesAreExactlyTheAdr006LittleEndianFixture();
  void startBlockPrefixIsExactlyTheAdr006Fixture();
  void nonConformantConfigurationIsRejected();
  void invalidEventIsRejected();
  void invalidCreationTimeIsRejected();
  void bufferingMembersOutsideThe32BitRangeAreRejected();
  void largeAsciiMetadataIsAcceptedWhenItFits();
  void stringMetadataEscapesExactlyAsTheFormatExpects();
  void jsonSizeCounterIsExactForEveryValueShape();
  void numericAndNestedMetadataAreSizedAndAcceptedExactly();
  void headerLimitBoundaryIsExact();
};

/** ADR-006's normative v1 writer profile: member names, types, the fixed local
  *  profile values and the exhaustive nested configuration schema. */
void TstSessionRecordCodec::headerProfileMatchesAdr006()
{
  const SessionRecordEncoding encoded = SessionRecordCodec::encodeHeader(headerFacts());
  QVERIFY2(encoded.ok, qPrintable(encoded.reason));
  const QByteArray block = encoded.bytes;

  // magic "KPSN" 0x1A CR LF NUL, then u16 version, u16 encoding, u32 length.
  QCOMPARE(SessionRecordCodec::magic(), QByteArray("KPSN\x1a\r\n\0", 8));
  QCOMPARE(block.left(8), SessionRecordCodec::magic());
  QCOMPARE(qFromLittleEndian<quint16>(block.constData() + 8), quint16(1));   // format version
  QCOMPARE(qFromLittleEndian<quint16>(block.constData() + 10), quint16(1));  // UTF-8 JSON
  const quint32 headerLength = qFromLittleEndian<quint32>(block.constData() + 12);
  QCOMPARE(qint64(headerLength), qint64(block.size() - 16));

  const QJsonObject header = QJsonDocument::fromJson(block.mid(16)).object();
  QCOMPARE(memberNames(header),
           QStringList({ QStringLiteral("application"), QStringLiteral("clockDomains"),
                         QStringLiteral("created"), QStringLiteral("format"),
                         QStringLiteral("sources"), QStringLiteral("version") }));
  QCOMPARE(header.value(QStringLiteral("format")).toString(), QStringLiteral("komport-session"));
  QCOMPARE(header.value(QStringLiteral("version")).toInt(), 1);
  QCOMPARE(header.value(QStringLiteral("created")).toString(), QStringLiteral("2026-09-18T12:00:00Z"));

  const QJsonObject application = header.value(QStringLiteral("application")).toObject();
  QCOMPARE(memberNames(application),
           QStringList({ QStringLiteral("name"), QStringLiteral("version") }));
  QCOMPARE(application.value(QStringLiteral("name")).toString(), QStringLiteral("Komport"));
  QCOMPARE(application.value(QStringLiteral("version")).toString(), QStringLiteral("2.0-test"));

  const QJsonArray sources = header.value(QStringLiteral("sources")).toArray();
  QCOMPARE(sources.size(), 1);
  const QJsonObject source = sources.at(0).toObject();
  QCOMPARE(memberNames(source),
           QStringList({ QStringLiteral("clockDomainId"), QStringLiteral("configuration"),
                         QStringLiteral("name"), QStringLiteral("sourceId"),
                         QStringLiteral("transport") }));
  QCOMPARE(source.value(QStringLiteral("sourceId")).toInt(), 1);  // 32-bit: a JSON number
  QCOMPARE(source.value(QStringLiteral("clockDomainId")).toString(),
           QStringLiteral("local-process-monotonic-v1"));
  QCOMPARE(source.value(QStringLiteral("name")).toString(), QStringLiteral("local serial"));
  QCOMPARE(source.value(QStringLiteral("transport")).toString(), QStringLiteral("serial"));

  // The nested configuration schema is exhaustive, and only these members are
  // written (ADR-006's v1 writer profile).
  const QJsonObject configuration = source.value(QStringLiteral("configuration")).toObject();
  QCOMPARE(memberNames(configuration),
           QStringList({ QStringLiteral("compatibility"), QStringLiteral("effective"),
                         QStringLiteral("localBuffering"), QStringLiteral("requested") }));
  const QStringList hardwareMembers({ QStringLiteral("baudRate"), QStringLiteral("dataBits"),
                                      QStringLiteral("endpoint"), QStringLiteral("flowControl"),
                                      QStringLiteral("parity"), QStringLiteral("stopBits") });
  for (const QString &section : { QStringLiteral("requested"), QStringLiteral("effective") }) {
    const QJsonObject object = configuration.value(section).toObject();
    QCOMPARE(memberNames(object), hardwareMembers);
    for (const QString &member : hardwareMembers)
      QCOMPARE(object.value(member).isString(), true);   // six strings, as ADR-006 fixes them
  }
  const QJsonObject buffering = configuration.value(QStringLiteral("localBuffering")).toObject();
  QCOMPARE(memberNames(buffering),
           QStringList({ QStringLiteral("flushRate"), QStringLiteral("rxQueue") }));
  QCOMPARE(buffering.value(QStringLiteral("rxQueue")).isDouble(), true);
  QCOMPARE(buffering.value(QStringLiteral("flushRate")).isDouble(), true);
  const QJsonObject compatibility = configuration.value(QStringLiteral("compatibility")).toObject();
  QCOMPARE(memberNames(compatibility), QStringList({ QStringLiteral("startBits") }));
  QCOMPARE(compatibility.value(QStringLiteral("startBits")).isString(), true);

  const QJsonArray clockDomains = header.value(QStringLiteral("clockDomains")).toArray();
  QCOMPARE(clockDomains.size(), 1);
  const QJsonObject clockDomain = clockDomains.at(0).toObject();
  QCOMPARE(memberNames(clockDomain),
           QStringList({ QStringLiteral("id"), QStringLiteral("kind"),
                         QStringLiteral("reference") }));
  QCOMPARE(clockDomain.value(QStringLiteral("id")).toString(),
           QStringLiteral("local-process-monotonic-v1"));
  QCOMPARE(clockDomain.value(QStringLiteral("kind")).toString(),
           QStringLiteral("process-monotonic"));
  const QJsonObject reference = clockDomain.value(QStringLiteral("reference")).toObject();
  QCOMPARE(memberNames(reference),
           QStringList({ QStringLiteral("sessionTimestampNs"), QStringLiteral("sourceTimestampNs") }));
  QCOMPARE(reference.value(QStringLiteral("sourceTimestampNs")).isString(), true);
  QCOMPARE(reference.value(QStringLiteral("sessionTimestampNs")).toString(), QStringLiteral("0"));
}

/** ADR-006's fixed 44-byte prefix, field by field, in the agreed order. */
void TstSessionRecordCodec::recordPrefixIsFortyFourBytesInTheAgreedOrder()
{
  SessionEvent event = dataEvent(7, QByteArrayLiteral("payload-bytes"));
  event.metadata.insert(QStringLiteral("kind"), QStringLiteral("open"));
  event.sourceTimestampNs = 4242;
  event.timestampNs = 99;

  const SessionRecordEncoding encoded = SessionRecordCodec::encodeRecord(event);
  QVERIFY2(encoded.ok, qPrintable(encoded.reason));
  const QByteArray record = encoded.bytes;

  const QByteArray metadataJson = QJsonDocument(event.metadata).toJson(QJsonDocument::Compact);
  QCOMPARE(qint64(SessionRecordCodec::kRecordPrefixSize), qint64(44));
  QCOMPARE(qint64(record.size()),
           qint64(44 + metadataJson.size() + event.payload.size()));

  const char *cursor = record.constData();
  QCOMPARE(qFromLittleEndian<quint32>(cursor), quint32(record.size() - 4));  // recordLength
  cursor += 4;
  QCOMPARE(qFromLittleEndian<quint16>(cursor), quint16(SessionEventType::Data));  // eventType
  cursor += 2;
  QCOMPARE(qFromLittleEndian<quint8>(cursor), quint8(SessionDirection::Tx));      // direction
  cursor += 1;
  QCOMPARE(qFromLittleEndian<quint8>(cursor), quint8(0));                         // flags
  cursor += 1;
  QCOMPARE(qFromLittleEndian<quint64>(cursor), quint64(7));                       // sequence
  cursor += 8;
  QCOMPARE(qFromLittleEndian<quint32>(cursor), quint32(1));                       // sourceId
  cursor += 4;
  QCOMPARE(qFromLittleEndian<qint64>(cursor), qint64(4242));                      // sourceTimestampNs
  cursor += 8;
  QCOMPARE(qFromLittleEndian<qint64>(cursor), qint64(99));                        // timestampNs
  cursor += 8;
  QCOMPARE(qFromLittleEndian<quint32>(cursor), quint32(metadataJson.size()));     // metadataLength
  cursor += 4;
  QCOMPARE(qFromLittleEndian<quint32>(cursor), quint32(event.payload.size()));    // payloadLength
  cursor += 4;
  QCOMPARE(record.mid(44, metadataJson.size()), metadataJson);
  QCOMPARE(record.mid(44 + metadataJson.size()), event.payload);
}

/** An empty metadata object is the zero-length encoding, never "{}". */
void TstSessionRecordCodec::emptyMetadataIsEncodedAsLengthZero()
{
  SessionEvent event = dataEvent(1, QByteArrayLiteral("\x00\x01\x02"));
  QVERIFY(event.metadata.isEmpty());

  const SessionRecordEncoding encoded = SessionRecordCodec::encodeRecord(event);
  QVERIFY2(encoded.ok, qPrintable(encoded.reason));
  QCOMPARE(qint64(encoded.bytes.size()), qint64(44 + event.payload.size()));
  QVERIFY(!encoded.bytes.contains("{}"));
  QCOMPARE(qFromLittleEndian<quint32>(encoded.bytes.constData() + 36), quint32(0));  // metadataLength
}

/** 64-bit values live in the header as canonical decimal strings, and the
  *  producer's metadata passes through verbatim. */
void TstSessionRecordCodec::sixtyFourBitJsonValuesAreCanonicalDecimalStrings()
{
  const qint64 beyondTwoToTheFiftyThree = 9007199254740993LL;   // 2^53 + 1
  SessionHeaderFacts facts = headerFacts();
  facts.referenceSourceTimestampNs = beyondTwoToTheFiftyThree;
  const SessionRecordEncoding encoded = SessionRecordCodec::encodeHeader(facts);
  QVERIFY2(encoded.ok, qPrintable(encoded.reason));

  const QJsonObject header = QJsonDocument::fromJson(encoded.bytes.mid(16)).object();
  const QJsonValue reference = header.value(QStringLiteral("clockDomains")).toArray().at(0)
                                 .toObject().value(QStringLiteral("reference"))
                                 .toObject().value(QStringLiteral("sourceTimestampNs"));
  QCOMPARE(reference.isString(), true);
  QCOMPARE(reference.toString(), QStringLiteral("9007199254740993"));
  qint64 decoded = 0;
  QCOMPARE(sessionJsonIntegerToQInt64(reference, &decoded), true);
  QCOMPARE(decoded, beyondTwoToTheFiftyThree);

  // Metadata belongs to the producer: the codec writes it unchanged, including
  // decimal strings above 2^53, and adds nothing of its own.
  SessionEvent event = dataEvent(2, QByteArrayLiteral("x"));
  event.type = SessionEventType::Annotation;
  event.direction = SessionDirection::None;
  event.payload.clear();
  event.metadata.insert(QStringLiteral("futureNs"), QStringLiteral("9007199254740993"));
  const SessionRecordEncoding record = SessionRecordCodec::encodeRecord(event);
  QVERIFY2(record.ok, qPrintable(record.reason));
  const QJsonObject metadata = QJsonDocument::fromJson(record.bytes.mid(44)).object();
  QCOMPARE(metadata, event.metadata);
}

/** A header beyond ADR-006's 16 MiB limit is refused, not truncated. */
void TstSessionRecordCodec::oversizedHeaderIsRejected()
{
  SessionHeaderFacts facts = headerFacts();
  facts.applicationVersion = QString(SessionRecordCodec::kMaxHeaderBytes + 1024, QLatin1Char('v'));

  const SessionRecordEncoding encoded = SessionRecordCodec::encodeHeader(facts);
  QCOMPARE(encoded.ok, false);
  QVERIFY(encoded.bytes.isEmpty());
  QVERIFY(encoded.reason.contains(QStringLiteral("16 MiB")));
}

/** A record body beyond ADR-006's 64 MiB limit is refused before assembly. */
void TstSessionRecordCodec::oversizedRecordIsRejected()
{
  SessionEvent event = dataEvent(3, QByteArray(int(SessionRecordCodec::kMaxRecordBodyBytes), char(0x5A)));
  const SessionRecordEncoding encoded = SessionRecordCodec::encodeRecord(event);
  QCOMPARE(encoded.ok, false);
  QVERIFY(encoded.bytes.isEmpty());
  QVERIFY(encoded.reason.contains(QStringLiteral("64 MiB")));
}

/** The sizing arithmetic is checked: boundaries, the u32 fields and negatives. */
void TstSessionRecordCodec::lengthArithmeticIsChecked()
{
  QString reason;
  constexpr qint64 kMaxBody = SessionRecordCodec::kMaxRecordBodyBytes;
  constexpr qint64 kPrefixTail = SessionRecordCodec::kRecordBodyPrefixSize;

  // Exactly at the limit, and one byte beyond it.
  QCOMPARE(SessionRecordCodec::recordSizesFit(0, kMaxBody - kPrefixTail, &reason), true);
  QCOMPARE(SessionRecordCodec::recordSizesFit(1, kMaxBody - kPrefixTail, &reason), false);
  QVERIFY(reason.contains(QStringLiteral("64 MiB")));

  // The two u32 length fields cannot hold these values.
  QCOMPARE(SessionRecordCodec::recordSizesFit(0x100000000LL, 0, &reason), false);
  QVERIFY(reason.contains(QStringLiteral("u32")));
  QCOMPARE(SessionRecordCodec::recordSizesFit(0, 0x100000000LL, &reason), false);
  QVERIFY(reason.contains(QStringLiteral("u32")));

  // Negative components are refusals, not wraparound.
  QCOMPARE(SessionRecordCodec::recordSizesFit(-1, 0, &reason), false);
  QCOMPARE(SessionRecordCodec::recordSizesFit(0, -1, &reason), false);

  // The largest representable payload below the limit is accepted.
  QCOMPARE(SessionRecordCodec::recordSizesFit(0, kMaxBody - kPrefixTail - 1, &reason), true);
}

/** Byte-exact fixture: the record as ADR-006 lays it out, read off the
  *  documented field order rather than from the implementation's endianness
  *  helpers, with non-symmetric multi-byte values so that a writer using the
  *  host byte order cannot pass on a little-endian machine. */
void TstSessionRecordCodec::recordBytesAreExactlyTheAdr006LittleEndianFixture()
{
  SessionEvent event;
  event.sequence = 0x0102030405060708ULL;
  event.sourceId = 1;
  event.sourceTimestampNs = 0x1122334455667788LL;
  event.timestampNs = 0x0000000102030405LL;
  event.type = SessionEventType::Data;      // 1
  event.direction = SessionDirection::Tx;   // 1
  event.payload = QByteArrayLiteral("AZ");  // 0x41 0x5A

  const SessionRecordEncoding encoded = SessionRecordCodec::encodeRecord(event);
  QVERIFY2(encoded.ok, qPrintable(encoded.reason));

  const QByteArray expected = QByteArray::fromHex(
      "2a000000"              // recordLength = 42 = 40 + 0 + 2
      "0100"                  // eventType: Data
      "01"                    // direction: Tx
      "00"                    // flags
      "0807060504030201"      // sequence, little-endian
      "01000000"              // sourceId
      "8877665544332211"      // sourceTimestampNs
      "0504030201000000"      // timestampNs
      "00000000"              // metadataLength: an empty object
      "02000000"              // payloadLength
      "415a");                // payload
  QCOMPARE(encoded.bytes, expected);
}

/** The start block's fixed prefix, byte for byte: magic, version, header
  *  encoding - and the u32 header length in little-endian order. */
void TstSessionRecordCodec::startBlockPrefixIsExactlyTheAdr006Fixture()
{
  const SessionRecordEncoding encoded = SessionRecordCodec::encodeHeader(headerFacts());
  QVERIFY2(encoded.ok, qPrintable(encoded.reason));

  const QByteArray expectedPrefix = QByteArray::fromHex("4b50534e1a0d0a00"  // "KPSN", 0x1A, CR, LF, NUL
                                                        "0100"            // format version 1
                                                        "0100");          // header encoding: UTF-8 JSON
  QCOMPARE(encoded.bytes.left(12), expectedPrefix);

  // The u32 after the prefix is the length of exactly the JSON that follows it.
  const quint32 headerLength = qFromLittleEndian<quint32>(encoded.bytes.constData() + 12);
  QCOMPARE(qint64(headerLength), qint64(encoded.bytes.size() - 16));
  QVERIFY(QJsonDocument::fromJson(encoded.bytes.mid(16)).isObject());
}

/** The codec refuses a configuration snapshot outside ADR-006's exhaustive
  *  schema: extra sections, missing members, wrong types, extra members. */
void TstSessionRecordCodec::nonConformantConfigurationIsRejected()
{
  // A per-transaction field is not part of a configuration snapshot.
  SessionHeaderFacts facts = headerFacts();
  QJsonObject configuration = facts.configuration;
  configuration.insert(QStringLiteral("applyStatus"), QStringLiteral("full"));
  facts.configuration = configuration;
  SessionRecordEncoding encoded = SessionRecordCodec::encodeHeader(facts);
  QCOMPARE(encoded.ok, false);
  QVERIFY(encoded.bytes.isEmpty());
  QVERIFY(encoded.reason.contains(QStringLiteral("unsupported section")));

  // A missing hardware member.
  facts = headerFacts();
  QJsonObject requested = facts.configuration.value(QStringLiteral("requested")).toObject();
  requested.remove(QStringLiteral("parity"));
  configuration = facts.configuration;
  configuration.insert(QStringLiteral("requested"), requested);
  facts.configuration = configuration;
  encoded = SessionRecordCodec::encodeHeader(facts);
  QCOMPARE(encoded.ok, false);
  QVERIFY(encoded.reason.contains(QStringLiteral("missing parity")));

  // Wrong types: the buffering members are numbers, not strings.
  facts = headerFacts();
  QJsonObject buffering;
  buffering.insert(QStringLiteral("rxQueue"), QStringLiteral("1024"));
  buffering.insert(QStringLiteral("flushRate"), 256);
  configuration = facts.configuration;
  configuration.insert(QStringLiteral("localBuffering"), buffering);
  facts.configuration = configuration;
  encoded = SessionRecordCodec::encodeHeader(facts);
  QCOMPARE(encoded.ok, false);
  QVERIFY(encoded.reason.contains(QStringLiteral("integral JSON number")));

  // An extra member inside a hardware section.
  facts = headerFacts();
  QJsonObject effective = facts.configuration.value(QStringLiteral("effective")).toObject();
  effective.insert(QStringLiteral("portName"), QStringLiteral("pts/6"));
  configuration = facts.configuration;
  configuration.insert(QStringLiteral("effective"), effective);
  facts.configuration = configuration;
  encoded = SessionRecordCodec::encodeHeader(facts);
  QCOMPARE(encoded.ok, false);
  QVERIFY(encoded.reason.contains(QStringLiteral("does not allow")));

  // A conformant snapshot still passes: the checks reject only what they must.
  QVERIFY(SessionRecordCodec::encodeHeader(headerFacts()).ok);
}

/** A structurally invalid event is a producer defect and is never written. */
void TstSessionRecordCodec::invalidEventIsRejected()
{
  // A Data event without a payload.
  SessionEvent event = dataEvent(1, QByteArray());
  SessionRecordEncoding encoded = SessionRecordCodec::encodeRecord(event);
  QCOMPARE(encoded.ok, false);
  QVERIFY(encoded.bytes.isEmpty());
  QVERIFY(encoded.reason.contains(QStringLiteral("not a valid SessionEvent")));

  // A non-data event that carries a payload.
  event = dataEvent(2, QByteArrayLiteral("x"));
  event.type = SessionEventType::Annotation;
  event.direction = SessionDirection::None;
  encoded = SessionRecordCodec::encodeRecord(event);
  QCOMPARE(encoded.ok, false);
  QVERIFY(encoded.reason.contains(QStringLiteral("not a valid SessionEvent")));

  // The reserved source ID.
  event = dataEvent(3, QByteArrayLiteral("x"));
  event.sourceId = 0;
  encoded = SessionRecordCodec::encodeRecord(event);
  QCOMPARE(encoded.ok, false);
  QVERIFY(encoded.reason.contains(QStringLiteral("sourceId 0")));
}

/** ADR-006 requires an ISO-8601 UTC creation time: an invalid QDateTime is
  *  refused rather than serialised as an empty string. */
void TstSessionRecordCodec::invalidCreationTimeIsRejected()
{
  SessionHeaderFacts facts = headerFacts();
  facts.createdUtc = QDateTime();
  const SessionRecordEncoding encoded = SessionRecordCodec::encodeHeader(facts);
  QCOMPARE(encoded.ok, false);
  QVERIFY(encoded.bytes.isEmpty());
  QVERIFY(encoded.reason.contains(QStringLiteral("creation time")));
}

/** The buffering members are 32-bit quantities (ADR-006): an integral check
  *  must never convert an out-of-range double. */
void TstSessionRecordCodec::bufferingMembersOutsideThe32BitRangeAreRejected()
{
  const QList<double> rejected({ 1.0e30, -1.0, 1.5, 2147483648.0 });
  for (double value : rejected) {
    SessionHeaderFacts facts = headerFacts();
    QJsonObject buffering;
    buffering.insert(QStringLiteral("rxQueue"), value);
    buffering.insert(QStringLiteral("flushRate"), 256);
    QJsonObject configuration = facts.configuration;
    configuration.insert(QStringLiteral("localBuffering"), buffering);
    facts.configuration = configuration;

    const SessionRecordEncoding encoded = SessionRecordCodec::encodeHeader(facts);
    QCOMPARE(encoded.ok, false);
    QVERIFY2(encoded.reason.contains(QStringLiteral("integral JSON number")),
             qPrintable(encoded.reason));
  }

  // The 32-bit maximum itself is still accepted.
  SessionHeaderFacts facts = headerFacts();
  QJsonObject buffering;
  buffering.insert(QStringLiteral("rxQueue"), 2147483647.0);
  buffering.insert(QStringLiteral("flushRate"), 0);
  QJsonObject configuration = facts.configuration;
  configuration.insert(QStringLiteral("localBuffering"), buffering);
  facts.configuration = configuration;
  QVERIFY(SessionRecordCodec::encodeHeader(facts).ok);
}

/** A large but in-limit ASCII metadata string must be accepted: the size
  *  preflight is exact for strings, so it may not refuse a record that fits.
  *  (The earlier conservative bound estimated six bytes per character and
  *  refused this case, which would have dropped an accepted event.) */
void TstSessionRecordCodec::largeAsciiMetadataIsAcceptedWhenItFits()
{
  const int textLength = 16 * 1024 * 1024;   // far below the 64 MiB record body
  SessionEvent event = dataEvent(5, QByteArrayLiteral("payload"));
  event.metadata.insert(QStringLiteral("k"), QString(textLength, QLatin1Char('x')));

  const SessionRecordEncoding encoded = SessionRecordCodec::encodeRecord(event);
  QVERIFY2(encoded.ok, qPrintable(encoded.reason));

  // The metadata JSON is {"k":"<textLength x's>"}: exactly textLength + 8 bytes,
  // and the record is its prefix plus metadata plus payload.
  const QByteArray metadataJson = QJsonDocument(event.metadata).toJson(QJsonDocument::Compact);
  QCOMPARE(qint64(metadataJson.size()), qint64(textLength) + 8);
  QCOMPARE(qint64(encoded.bytes.size()),
           qint64(44) + qint64(metadataJson.size()) + qint64(event.payload.size()));
}

/** String escaping in the metadata section is exactly what the format expects,
  *  checked against hand-written JSON bytes rather than against the writer. */
void TstSessionRecordCodec::stringMetadataEscapesExactlyAsTheFormatExpects()
{
  // A quote, a newline and a NUL: the short escapes and the \u00XX form.
  SessionEvent event = dataEvent(6, QByteArrayLiteral("p"));
  event.metadata.insert(QStringLiteral("s"), QString::fromLatin1("A\"B\nC\0D", 7));

  const SessionRecordEncoding encoded = SessionRecordCodec::encodeRecord(event);
  QVERIFY2(encoded.ok, qPrintable(encoded.reason));

  const QByteArray expectedJson = QByteArrayLiteral("{\"s\":\"A\\\"B\\nC\\u0000D\"}");
  const quint32 metadataLength = qFromLittleEndian<quint32>(encoded.bytes.constData() + 36);
  QCOMPARE(qint64(metadataLength), qint64(expectedJson.size()));
  QCOMPARE(encoded.bytes.mid(44, expectedJson.size()), expectedJson);
}

/** The size counter must agree with the JSON writer for every value shape: the
  *  refusal decision depends on it, and an over-estimate would refuse a unit
  *  that fits, dropping an accepted event (SPEC-M9 section 5.3). */
void TstSessionRecordCodec::jsonSizeCounterIsExactForEveryValueShape()
{
  QString loneSurrogate;
  loneSurrogate.append(QChar(0xD800));

  QJsonObject nested;
  nested.insert(QStringLiteral("emptyObject"), QJsonObject());
  nested.insert(QStringLiteral("emptyArray"), QJsonArray());
  nested.insert(QStringLiteral("mixed"), QJsonArray({ 1.0, QStringLiteral("two"), true,
                                                      QJsonValue(QJsonValue::Null),
                                                      QJsonObject({ { QStringLiteral("deep"),
                                                                      QJsonArray({ 3.5, -0.0 }) } }) }));
  // A metadata array can carry an undefined value and a NaN; the writer emits
  // both as `null`, and the counter must agree.
  nested.insert(QStringLiteral("undefinedHere"), QJsonArray({ QJsonValue(),
                                                              QJsonValue(QJsonValue::Undefined),
                                                              QJsonValue(std::numeric_limits<double>::quiet_NaN()) }));

  const QList<QJsonValue> corpus({
      QJsonValue(QString()),
      QJsonValue(QStringLiteral("plain ascii")),
      QJsonValue(QStringLiteral("quote\" backslash\\ slash/")),
      QJsonValue(QString::fromLatin1("bell\b formfeed\f nl\n cr\r tab\t nul\0 soh\x01", 30)),
      QJsonValue(QString::fromUtf8("naive \xC3\xA9 \xE4\xB8\xAD \xF0\x9F\x90\xA7")),
      QJsonValue(loneSurrogate),
      QJsonValue(0.0), QJsonValue(1.0), QJsonValue(1024.0), QJsonValue(-1.5),
      QJsonValue(0.1), QJsonValue(1.0e-5), QJsonValue(1.0e30),
      QJsonValue(2147483647.0), QJsonValue(9007199254740992.0),
      QJsonValue(std::numeric_limits<double>::max()),
      QJsonValue(true), QJsonValue(false), QJsonValue(QJsonValue::Null),
      QJsonValue(QJsonValue::Undefined),
      QJsonValue(QJsonObject()), QJsonValue(QJsonArray()),
      QJsonValue(nested),
  });

  for (const QJsonValue &value : corpus) {
    // The writer itself is the reference: a one-element array costs two bytes of
    // syntax, so the value's own size is the remaining bytes.
    const QByteArray wrapped = QJsonDocument(QJsonArray{ value }).toJson(QJsonDocument::Compact);
    const qint64 expected = qint64(wrapped.size()) - 2;
    QCOMPARE(SessionRecordCodec::compactJsonSize(value), expected);
    QVERIFY(expected > 0);
  }
}

/** Numeric-heavy and nested metadata must be sized exactly and accepted: the
  *  old estimate (32 bytes per number, one byte of comma per member) would have
  *  over-counted both shapes. */
void TstSessionRecordCodec::numericAndNestedMetadataAreSizedAndAcceptedExactly()
{
  QJsonObject numbers;
  for (int i = 0; i < 500; ++i)
    numbers.insert(QStringLiteral("n%1").arg(i), double(i) / 8.0);

  QJsonArray nested;
  nested.append(QJsonObject({ { QStringLiteral("a"), QJsonArray({ 1.0, 2.0, 3.0 }) } }));
  nested.append(QJsonObject({ { QStringLiteral("b"), QStringLiteral("text") } }));

  SessionEvent event = dataEvent(9, QByteArrayLiteral("xy"));
  event.metadata.insert(QStringLiteral("numbers"), numbers);
  event.metadata.insert(QStringLiteral("nested"), nested);

  const SessionRecordEncoding encoded = SessionRecordCodec::encodeRecord(event);
  QVERIFY2(encoded.ok, qPrintable(encoded.reason));

  const QByteArray metadataJson = QJsonDocument(event.metadata).toJson(QJsonDocument::Compact);
  QCOMPARE(SessionRecordCodec::compactJsonSize(event.metadata), qint64(metadataJson.size()));
  QCOMPARE(qint64(encoded.bytes.size()),
           qint64(44) + qint64(metadataJson.size()) + qint64(event.payload.size()));
}

/** The 16 MiB header limit is a real boundary: the exact size is accepted, one
  *  byte more is refused. Measured through the public API only. */
void TstSessionRecordCodec::headerLimitBoundaryIsExact()
{
  // The fixed overhead of the header JSON, measured with a small version string.
  SessionHeaderFacts probe = headerFacts();
  probe.applicationVersion = QString(16, QLatin1Char('v'));
  const SessionRecordEncoding probeEncoded = SessionRecordCodec::encodeHeader(probe);
  QVERIFY2(probeEncoded.ok, qPrintable(probeEncoded.reason));
  const qint64 overhead = qint64(qFromLittleEndian<quint32>(probeEncoded.bytes.constData() + 12)) - 16;
  QVERIFY(overhead > 0);

  // A version sized so that the header JSON is exactly at the limit.
  SessionHeaderFacts facts = headerFacts();
  facts.applicationVersion = QString(int(SessionRecordCodec::kMaxHeaderBytes - overhead), QLatin1Char('v'));
  const SessionRecordEncoding atLimit = SessionRecordCodec::encodeHeader(facts);
  QVERIFY2(atLimit.ok, qPrintable(atLimit.reason));
  QCOMPARE(qint64(qFromLittleEndian<quint32>(atLimit.bytes.constData() + 12)),
           qint64(SessionRecordCodec::kMaxHeaderBytes));

  // One byte more does not fit and is refused, with nothing produced.
  facts.applicationVersion.append(QLatin1Char('v'));
  const SessionRecordEncoding overLimit = SessionRecordCodec::encodeHeader(facts);
  QCOMPARE(overLimit.ok, false);
  QVERIFY(overLimit.bytes.isEmpty());
}

QTEST_MAIN(TstSessionRecordCodec)
#include "tst_sessionrecordcodec.moc"
