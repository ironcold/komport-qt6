/***************************************************************************
                      tst_sessionreader.cpp  -  Komport Serial Port Communicator
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

// The M10 reader (ADR-011 D1-D4, SPEC-M10 5.3/5.4/5.10 and the reader table of
// SPEC-M10 section 7).
//
// Fixtures are built the way SPEC-M10 section 7 requires: with M9's writer codec
// (SessionRecordCodec) or as hand-written bytes, and cross-checked with M9's
// test-side parser (`tests/sessionrecordreader.h`), which stays M9's. The reader
// under test links neither - it is an independent implementation of ADR-006.
//
// No pty here: the byte-source seam covers the malformed and unreadable cases,
// and the real-file cases use a temporary directory.

#include "sessionreader.h"

#include "sessionrecordcodec.h"
#include "sessionevent.h"

#include "sessionrecordreader.h"   // M9's test-side parser, for the cross-checks

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QTemporaryDir>
#include <QTest>

#include <QtEndian>

#include <memory>

/** The one permitted way to construct a reader with the SPEC-M10 5.10 byte-source
  *  seam: the injection constructor is private, because the seam is not public
  *  API (the pattern M9 uses for SessionRecorderSeamsForTest). */
struct SessionReaderSeamsForTest
{
  static std::unique_ptr<SessionReader> create(SessionByteSource *source)
  {
    return std::unique_ptr<SessionReader>(new SessionReader(source));
  }
};

namespace {

/** The anchor's raw source time used by the fixtures (ADR-006's reference pair). */
constexpr qint64 kAnchorSourceTimestampNs = 123456789;

/** A scripted byte source: the whole file as bytes, plus the failure modes the
  *  checks of SPEC-M10 5.3.1 need (a failed open, an I/O error, a short read, an
  *  unknown size). */
class ScriptedSource : public SessionByteSource
{
public:
  QByteArray bytes;         ///< the file's bytes
  qint64 maxChunk = -1;     ///< cap on one read() call, -1 = hand out the whole request
  qint64 stopAfter = -1;    ///< stop handing out bytes after this many, without an error
  qint64 failAfter = -1;    ///< report an I/O error once this many bytes were read
  bool openFails = false;   ///< the path cannot be opened
  bool hideSize = false;    ///< the source promises no size at all
  qint64 bytesRead = 0;     ///< how much the loader consumed
  int openCalls = 0;
  int closeCalls = 0;

  bool open(const QString &) override
  {
    ++openCalls;
    return !openFails;
  }
  qint64 size() const override { return hideSize ? -1 : qint64(bytes.size()); }
  qint64 read(QByteArray &out, qint64 maxBytes) override
  {
    if (failAfter >= 0 && bytesRead >= failAfter)
      return -1;
    qint64 wanted = maxBytes;
    if (maxChunk > 0)
      wanted = qMin(wanted, maxChunk);
    if (stopAfter >= 0)
      wanted = qMin(wanted, stopAfter - bytesRead);
    if (wanted <= 0)
      return 0;
    const QByteArray chunk = bytes.mid(qsizetype(bytesRead), qsizetype(wanted));
    bytesRead += chunk.size();
    out.append(chunk);
    return chunk.size();
  }
  void close() override { ++closeCalls; }
};

/** Header facts for the writer codec: a complete, conformant local snapshot. */
SessionHeaderFacts headerFacts()
{
  QJsonObject requested;
  requested.insert(QStringLiteral("endpoint"), QStringLiteral("/dev/pts/6"));
  requested.insert(QStringLiteral("baudRate"), QStringLiteral("9600"));
  requested.insert(QStringLiteral("dataBits"), QStringLiteral("8"));
  requested.insert(QStringLiteral("stopBits"), QStringLiteral("1"));
  requested.insert(QStringLiteral("parity"), QStringLiteral("NONE"));
  requested.insert(QStringLiteral("flowControl"), QStringLiteral("NONE"));

  QJsonObject buffering;
  buffering.insert(QStringLiteral("rxQueue"), 1024);
  buffering.insert(QStringLiteral("flushRate"), 256);

  QJsonObject compatibility;
  compatibility.insert(QStringLiteral("startBits"), QStringLiteral("1"));

  QJsonObject configuration;
  configuration.insert(QStringLiteral("requested"), requested);
  configuration.insert(QStringLiteral("effective"), requested);
  configuration.insert(QStringLiteral("localBuffering"), buffering);
  configuration.insert(QStringLiteral("compatibility"), compatibility);

  SessionHeaderFacts facts;
  facts.applicationName = QStringLiteral("Komport");
  facts.applicationVersion = QStringLiteral("2.0-test");
  facts.createdUtc = QDateTime::fromString(QStringLiteral("2026-09-18T12:00:00Z"), Qt::ISODate);
  facts.configuration = configuration;
  facts.referenceSourceTimestampNs = kAnchorSourceTimestampNs;
  return facts;
}

/** The v1 header the writer codec produces, as a JSON object to mutate. */
QJsonObject standardHeader()
{
  const SessionRecordEncoding encoded = SessionRecordCodec::encodeHeader(headerFacts());
  Q_ASSERT(encoded.ok);
  const quint32 headerLength = qFromLittleEndian<quint32>(encoded.bytes.constData() + 12);
  return QJsonDocument::fromJson(encoded.bytes.mid(16, int(headerLength))).object();
}

template <typename T>
void appendLittleEndian(QByteArray &buffer, T value)
{
  const T little = qToLittleEndian(value);
  buffer.append(reinterpret_cast<const char *>(&little), sizeof(T));
}

/** A complete v1 file: the start block, @p header, then @p records. */
QByteArray buildFile(const QJsonObject &header, const QByteArray &records = QByteArray())
{
  const QByteArray headerJson = QJsonDocument(header).toJson(QJsonDocument::Compact);
  QByteArray file = SessionRecordCodec::magic();
  appendLittleEndian<quint16>(file, 1);   // format version
  appendLittleEndian<quint16>(file, 1);   // header encoding: UTF-8 JSON
  appendLittleEndian<quint32>(file, quint32(headerJson.size()));
  file.append(headerJson);
  file.append(records);
  return file;
}

/** A well-formed Data record, encoded by the writer codec. */
QByteArray recordBytes(const SessionEvent &event)
{
  const SessionRecordEncoding encoded = SessionRecordCodec::encodeRecord(event);
  Q_ASSERT(encoded.ok);
  return encoded.bytes;
}

/** A record assembled from raw field values, so that malformed lengths, flags
  *  and field values can be produced; the bytes of the two content sections are
  *  appended as given, which may deliberately disagree with the declarations. */
QByteArray rawRecord(quint32 declaredRecordLength, quint16 eventType, quint8 direction,
                     quint8 flags, quint64 sequence, quint32 sourceId, qint64 sourceTimestampNs,
                     qint64 timestampNs, quint32 metadataLength, quint32 payloadLength,
                     const QByteArray &metadataJson = QByteArray(),
                     const QByteArray &payload = QByteArray())
{
  QByteArray record;
  appendLittleEndian<quint32>(record, declaredRecordLength);
  appendLittleEndian<quint16>(record, eventType);
  appendLittleEndian<quint8>(record, direction);
  appendLittleEndian<quint8>(record, flags);
  appendLittleEndian<quint64>(record, sequence);
  appendLittleEndian<quint32>(record, sourceId);
  appendLittleEndian<qint64>(record, sourceTimestampNs);
  appendLittleEndian<qint64>(record, timestampNs);
  appendLittleEndian<quint32>(record, metadataLength);
  appendLittleEndian<quint32>(record, payloadLength);
  record.append(metadataJson);
  record.append(payload);
  return record;
}

/** The prefix-half of a record that declares itself as @p declaredRecordLength. */
QByteArray rawRecordPrefix(quint32 declaredRecordLength, quint16 eventType, quint8 direction,
                           quint8 flags, quint64 sequence, quint32 sourceId,
                           qint64 sourceTimestampNs, qint64 timestampNs, quint32 metadataLength,
                           quint32 payloadLength)
{
  return rawRecord(declaredRecordLength, eventType, direction, flags, sequence, sourceId,
                   sourceTimestampNs, timestampNs, metadataLength, payloadLength);
}

/** A structurally valid Data event. */
SessionEvent dataEvent(quint64 sequence, SessionDirection direction,
                       const QByteArray &payload = QByteArrayLiteral("AB"),
                       qint64 sourceTimestampNs = 1000, qint64 timestampNs = 250,
                       quint32 sourceId = 1)
{
  SessionEvent event;
  event.sequence = sequence;
  event.sourceId = sourceId;
  event.sourceTimestampNs = sourceTimestampNs;
  event.timestampNs = timestampNs;
  event.type = SessionEventType::Data;
  event.direction = direction;
  event.payload = payload;
  return event;
}

/** A structurally valid non-data event. */
SessionEvent nonDataEvent(SessionEventType type, quint64 sequence, qint64 timestampNs = 250)
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

/** Load @p bytes through the injected byte-source seam. */
SessionLoadOutcome loadWithSource(ScriptedSource &source)
{
  const std::unique_ptr<SessionReader> reader = SessionReaderSeamsForTest::create(&source);
  return reader->load(QStringLiteral("/dev/null"));
}

/** Load @p bytes through a fresh scripted source. */
SessionLoadOutcome loadBytes(const QByteArray &bytes)
{
  ScriptedSource source;
  source.bytes = bytes;
  return loadWithSource(source);
}

/** Write @p bytes into @p directory and return the path. */
QString writeTemporaryFile(QTemporaryDir &directory, const QByteArray &bytes,
                           const QString &name = QStringLiteral("session.kpsession"))
{
  const QString path = directory.filePath(name);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly))
    return QString();
  file.write(bytes);
  file.close();
  return path;
}

// --- header mutation helpers (one per mutation the checks need) ---------------

QJsonObject headerWithRoot(const QJsonObject &header, const QString &key, const QJsonValue &value)
{
  QJsonObject mutated = header;
  mutated.insert(key, value);
  return mutated;
}

QJsonObject headerWithoutRoot(const QJsonObject &header, const QString &key)
{
  QJsonObject mutated = header;
  mutated.remove(key);
  return mutated;
}

QJsonArray sourceArray(const QJsonObject &header)
{
  return header.value(QStringLiteral("sources")).toArray();
}

QJsonObject headerWithSource(const QJsonObject &header, const QString &key, const QJsonValue &value)
{
  QJsonArray sources = sourceArray(header);
  QJsonObject source = sources.at(0).toObject();
  source.insert(key, value);
  sources.replace(0, source);
  return headerWithRoot(header, QStringLiteral("sources"), sources);
}

QJsonObject headerWithoutSourceMember(const QJsonObject &header, const QString &key)
{
  QJsonArray sources = sourceArray(header);
  QJsonObject source = sources.at(0).toObject();
  source.remove(key);
  sources.replace(0, source);
  return headerWithRoot(header, QStringLiteral("sources"), sources);
}

QJsonObject headerWithDomain(const QJsonObject &header, const QString &key,
                             const QJsonValue &value, int index = 0)
{
  QJsonArray domains = header.value(QStringLiteral("clockDomains")).toArray();
  QJsonObject domain = domains.at(index).toObject();
  domain.insert(key, value);
  domains.replace(index, domain);
  return headerWithRoot(header, QStringLiteral("clockDomains"), domains);
}

QJsonObject headerWithReference(const QJsonObject &header, const QString &key,
                                const QJsonValue &value, int index = 0)
{
  QJsonArray domains = header.value(QStringLiteral("clockDomains")).toArray();
  QJsonObject domain = domains.at(index).toObject();
  QJsonObject reference = domain.value(QStringLiteral("reference")).toObject();
  reference.insert(key, value);
  domain.insert(QStringLiteral("reference"), reference);
  domains.replace(index, domain);
  return headerWithRoot(header, QStringLiteral("clockDomains"), domains);
}

QJsonObject headerWithConfigurationMember(const QJsonObject &header, const QString &section,
                                          const QString &key, const QJsonValue &value)
{
  QJsonArray sources = sourceArray(header);
  QJsonObject source = sources.at(0).toObject();
  QJsonObject configuration = source.value(QStringLiteral("configuration")).toObject();
  QJsonObject mutatedSection = configuration.value(section).toObject();
  mutatedSection.insert(key, value);
  configuration.insert(section, mutatedSection);
  source.insert(QStringLiteral("configuration"), configuration);
  sources.replace(0, source);
  return headerWithRoot(header, QStringLiteral("sources"), sources);
}

QJsonObject headerWithoutConfigurationMember(const QJsonObject &header, const QString &section,
                                            const QString &key)
{
  QJsonArray sources = sourceArray(header);
  QJsonObject source = sources.at(0).toObject();
  QJsonObject configuration = source.value(QStringLiteral("configuration")).toObject();
  QJsonObject mutatedSection = configuration.value(section).toObject();
  mutatedSection.remove(key);
  configuration.insert(section, mutatedSection);
  source.insert(QStringLiteral("configuration"), configuration);
  sources.replace(0, source);
  return headerWithRoot(header, QStringLiteral("sources"), sources);
}

/** A second, well-formed clock domain entry appended to the array. */
QJsonObject headerWithSecondDomain(const QJsonObject &header,
                                   const QString &id = QStringLiteral("second-domain"))
{
  QJsonObject reference;
  reference.insert(QStringLiteral("sourceTimestampNs"), QStringLiteral("42"));
  reference.insert(QStringLiteral("sessionTimestampNs"), QStringLiteral("0"));
  QJsonObject domain;
  domain.insert(QStringLiteral("id"), id);
  domain.insert(QStringLiteral("kind"), QStringLiteral("agent-monotonic"));
  domain.insert(QStringLiteral("reference"), reference);

  QJsonArray domains = header.value(QStringLiteral("clockDomains")).toArray();
  domains.append(domain);
  return headerWithRoot(header, QStringLiteral("clockDomains"), domains);
}

/** The refusal half of every check: no session, no events, a reason. */
void expectRefusal(const SessionLoadOutcome &outcome, const char *what)
{
  QVERIFY2(!outcome.ok, what);
  QVERIFY2(!outcome.reason.isEmpty(), what);
  QVERIFY2(outcome.session == nullptr, what);
  QCOMPARE(outcome.eventCount, quint64(0));
  QVERIFY2(!outcome.recoveredTruncatedFinalRecord, what);
}

} // namespace

class TstSessionReader : public QObject
{
  Q_OBJECT

private slots:
  void readerLimitsMatchTheWriterCodec();
  void badMagicVersionOrEncodingRefusesTheFile();
  void headerAboveSixteenMibIsRefusedBeforeItIsRead();
  void headerLongerThanTheFileIsRefused();
  void headerThatIsNotAJsonObjectRefusesTheFile();
  void headerViolationsRefuseTheFile();
  void combinedHeaderViolationsReportTheFirstFailingRow();
  void bufferingMembersAreOnlyJsonNumbers();
  void numericHeaderShapesRefuseOutOfRangeValues();
  void lengthArithmeticDoesNotOverflow();
  void duplicateClockDomainIdIsRefused();
  void malformedWallClockCorrelationIsRefused();
  void permittedOptionalMembersAreAccepted();
  void localProfileValuesAreNotAnAdmissionTest();
  void multiSourceFileIsRefusedWithTheM10Message();
  void fileWithoutASourceIsRefused();
  void clockDomainCountIsNotAnM10Rule();
  void recordPrefixIsReadFieldByField();
  void recoveredTruncatedFinalRecordIsReportedNotDelivered();
  void truncationInsideTheStartBlockOrHeaderRefusesTheFile();
  void recordLengthBelowThePrefixIsRefused();
  void recordLengthAboveTheLimitIsRefusedBeforeRecovery();
  void inconsistentRecordLengthsRefuseTheWholeFile();
  void nonZeroFlagsRefuseTheFile();
  void nonObjectMetadataRefusesTheFile();
  void invalidEventFieldsRefuseTheFile();
  void recordSourceMustResolveToTheFileSource();
  void sequenceMustStrictlyIncrease();
  void aMidSessionRecordingLoadsWithItsTrueAnchor();
  void sessionTimeMustNotDecreaseInTheDomain();
  void payloadsSurviveByteExactly();
  void sessionWithEveryEventTypeLoadsFieldByField();
  void zeroRecordFileIsAValidSessionWithNoEvents();
  void readErrorRefusesTheFileWithoutASession();
  void theFileIsUnchangedByLoading();
  void theReaderHoldsNoStateBetweenLoads();
};

/** The reader's format facts are its own, and they agree with the writer's
  *  (ADR-011 D2): the codec cannot produce a file this reader must reject. The
  *  start-block numbers are pinned by the hand-built fixture of
  *  recordPrefixIsReadFieldByField. */
void TstSessionReader::readerLimitsMatchTheWriterCodec()
{
  QCOMPARE(SessionReader::magic(), SessionRecordCodec::magic());
  QCOMPARE(qint64(SessionReader::kRecordPrefixSize), qint64(SessionRecordCodec::kRecordPrefixSize));
  QCOMPARE(qint64(SessionReader::kMaxHeaderBytes), qint64(SessionRecordCodec::kMaxHeaderBytes));
  QCOMPARE(qint64(SessionReader::kMaxRecordBodyBytes),
           qint64(SessionRecordCodec::kMaxRecordBodyBytes));
  QCOMPARE(SessionReader::kFormatVersion, 1);
  QCOMPARE(SessionReader::kHeaderEncodingUtf8Json, 1);

  // The start block of a file the writer produced carries exactly those numbers.
  const QByteArray file = buildFile(standardHeader());
  QCOMPARE(file.left(8), SessionReader::magic());
  QCOMPARE(qFromLittleEndian<quint16>(file.constData() + 8), quint16(1));
  QCOMPARE(qFromLittleEndian<quint16>(file.constData() + 10), quint16(1));
  QCOMPARE(qFromLittleEndian<quint32>(file.constData() + 12),
           quint32(file.size() - 16));
  QVERIFY(loadBytes(file).ok);
}

/** Checks 3-6: a start block that is too short, a wrong magic and wrong version
  *  or encoding numbers are all refused, with no session. */
void TstSessionReader::badMagicVersionOrEncodingRefusesTheFile()
{
  const QByteArray valid = buildFile(standardHeader(), recordBytes(dataEvent(1, SessionDirection::Rx)));

  expectRefusal(loadBytes(valid.left(10)), "a file shorter than a start block");

  QByteArray badMagic = valid;
  badMagic[0] = 'X';
  expectRefusal(loadBytes(badMagic), "a wrong magic");

  QByteArray badVersion = valid;
  badVersion[8] = char(2);
  expectRefusal(loadBytes(badVersion), "an unsupported format version");

  QByteArray badEncoding = valid;
  badEncoding[10] = char(2);
  expectRefusal(loadBytes(badEncoding), "an unsupported header encoding");

  // The refusal names the number it found, so a user can tell the cases apart.
  const SessionLoadOutcome versionOutcome = loadBytes(badVersion);
  QVERIFY(versionOutcome.reason.contains(QStringLiteral("2")));
}

/** Check 7 has an exact boundary: exactly 16 MiB of header is admissible (a `>`
  *  rule), and the reader still reads and validates it. */
void TstSessionReader::headerAboveSixteenMibIsRefusedBeforeItIsRead()
{
  ScriptedSource source;
  source.bytes = buildFile(standardHeader());
  // Declare a header far beyond the limit; the bytes are not there at all.
  const quint32 declared = quint32(SessionReader::kMaxHeaderBytes) + 1;
  const quint32 little = qToLittleEndian(declared);
  source.bytes.replace(12, 4, QByteArray(reinterpret_cast<const char *>(&little), 4));

  const SessionLoadOutcome outcome = loadWithSource(source);
  expectRefusal(outcome, "an oversized declared header length");
  QVERIFY(outcome.reason.contains(QStringLiteral("16 MiB")));
  QCOMPARE(source.bytesRead, qint64(16));   // the start block only

  // The limit itself is admissible: ADR-006's rule is "at most 16 MiB", so a
  // header of exactly that size is read, validated and accepted. The padding is
  // an additional root member, which rows 11/12 accept.
  const qint64 limit = qint64(SessionReader::kMaxHeaderBytes);
  QJsonObject padded = standardHeader();
  qint64 padding = limit;
  for (int attempt = 0; attempt < 4; ++attempt) {
    padded.insert(QStringLiteral("notes"),
                  QString(int(qMax<qint64>(0, padding)), QLatin1Char('n')));
    const qint64 size = qint64(QJsonDocument(padded).toJson(QJsonDocument::Compact).size());
    if (size == limit)
      break;
    padding += limit - size;
  }
  const QByteArray atLimit = buildFile(padded);
  QCOMPARE(qint64(qFromLittleEndian<quint32>(atLimit.constData() + 12)), limit);
  const SessionLoadOutcome accepted = loadBytes(atLimit);
  QVERIFY2(accepted.ok, qPrintable(accepted.reason));
  QCOMPARE(accepted.eventCount, quint64(0));
}

/** Check 8: a header that is longer than the file is refused, in both forms -
  *  through the promised size and through a source that hides its size. */
void TstSessionReader::headerLongerThanTheFileIsRefused()
{
  const QByteArray file = buildFile(standardHeader());
  ScriptedSource throughSize;
  throughSize.bytes = file.left(file.size() - 4);   // the last four header bytes are gone
  expectRefusal(loadWithSource(throughSize), "a header longer than the file");

  ScriptedSource throughReads;
  throughReads.bytes = file.left(file.size() - 4);
  throughReads.hideSize = true;                     // the reads alone must refuse it
  const SessionLoadOutcome outcome = loadWithSource(throughReads);
  expectRefusal(outcome, "a header longer than the file (unknown size)");
  QVERIFY(outcome.reason.contains(QStringLiteral("exceeds the file")));
}

/** Check 9: the header bytes must be a UTF-8 JSON object. */
void TstSessionReader::headerThatIsNotAJsonObjectRefusesTheFile()
{
  QByteArray file = SessionRecordCodec::magic();
  appendLittleEndian<quint16>(file, 1);
  appendLittleEndian<quint16>(file, 1);
  const QByteArray notAnObject = QByteArrayLiteral("[1,2,3]");
  appendLittleEndian<quint32>(file, quint32(notAnObject.size()));
  file.append(notAnObject);
  const SessionLoadOutcome arrayOutcome = loadBytes(file);
  expectRefusal(arrayOutcome, "a header that is a JSON array");
  QVERIFY(arrayOutcome.reason.contains(QStringLiteral("JSON object")));

  // Truncated JSON is not an object either.
  QByteArray cut = file.left(file.size() - 3);
  expectRefusal(loadBytes(cut), "truncated header JSON");
}

/** The rows are ordered (SPEC-M10 5.3): a file that violates several of them is
  *  refused by the first row it violates, and the reported reason is that row's.
  *  These fixtures each violate two rules at once. */
void TstSessionReader::combinedHeaderViolationsReportTheFirstFailingRow()
{
  // Row 11 (presence) before row 10 (values).
  QJsonObject missingAndMisshaped = headerWithoutRoot(standardHeader(), QStringLiteral("format"));
  missingAndMisshaped = headerWithRoot(missingAndMisshaped, QStringLiteral("version"),
                                       QStringLiteral("1"));
  const SessionLoadOutcome missingOutcome = loadBytes(buildFile(missingAndMisshaped));
  expectRefusal(missingOutcome, "a missing member and a misshaped value");
  QVERIFY(missingOutcome.reason.contains(QStringLiteral("missing a required member")));

  // Row 13 (descriptor, including its domain resolution) before row 14.
  QJsonObject unresolvedAndBadConfiguration =
      headerWithSource(standardHeader(), QStringLiteral("clockDomainId"),
                       QStringLiteral("no-such-domain"));
  unresolvedAndBadConfiguration =
      headerWithConfigurationMember(unresolvedAndBadConfiguration, QStringLiteral("requested"),
                                    QStringLiteral("extra"), QStringLiteral("1"));
  const SessionLoadOutcome unresolvedOutcome = loadBytes(buildFile(unresolvedAndBadConfiguration));
  expectRefusal(unresolvedOutcome, "an unresolved domain and a bad configuration");
  QVERIFY(unresolvedOutcome.reason.contains(QStringLiteral("clockDomains[] entry")));

  // Row 15 (ids, kinds) before row 16 (reference pairs).
  QJsonObject duplicateAndBadReference = standardHeader();
  const QString domainId = sourceArray(duplicateAndBadReference).at(0).toObject()
                               .value(QStringLiteral("clockDomainId"))
                               .toString();
  duplicateAndBadReference = headerWithSecondDomain(duplicateAndBadReference, domainId);
  duplicateAndBadReference = headerWithReference(duplicateAndBadReference,
                                                QStringLiteral("sourceTimestampNs"), 12, 1);
  const SessionLoadOutcome duplicateOutcome = loadBytes(buildFile(duplicateAndBadReference));
  expectRefusal(duplicateOutcome, "a duplicate id and a bad reference");
  QVERIFY(duplicateOutcome.reason.contains(QStringLiteral("share the id")));

  // Row 16 (reference pairs) before row 17 (correlation).
  QJsonObject badReferenceAndNullCorrelation = headerWithSecondDomain(standardHeader());
  badReferenceAndNullCorrelation =
      headerWithReference(badReferenceAndNullCorrelation, QStringLiteral("sessionTimestampNs"),
                          QStringLiteral("1"), 0);
  badReferenceAndNullCorrelation =
      headerWithDomain(badReferenceAndNullCorrelation, QStringLiteral("wallClockCorrelation"),
                       QJsonValue(QJsonValue::Null), 1);
  const SessionLoadOutcome referenceOutcome = loadBytes(buildFile(badReferenceAndNullCorrelation));
  expectRefusal(referenceOutcome, "a bad reference and a null correlation");
  QVERIFY(referenceOutcome.reason.contains(QStringLiteral("does not reference session time")));
}

/** ADR-006 fixes the two `localBuffering` members as JSON *numbers* and nothing
  *  more; the writer profile's integral 32-bit rule is the writer's own and must
  *  not become a reader admission test (ADR-011 D2; review: slice-1
  *  implementation review, finding 2). */
void TstSessionReader::bufferingMembersAreOnlyJsonNumbers()
{
  const auto loads = [](const QJsonObject &header) { return loadBytes(buildFile(header)); };

  // JSON numbers outside the writer's range and with a fraction are conformant.
  QVERIFY(loads(headerWithConfigurationMember(standardHeader(), QStringLiteral("localBuffering"),
                                              QStringLiteral("rxQueue"), 1024.5)).ok);
  QVERIFY(loads(headerWithConfigurationMember(standardHeader(), QStringLiteral("localBuffering"),
                                              QStringLiteral("flushRate"), -3)).ok);
  QVERIFY(loads(headerWithConfigurationMember(standardHeader(), QStringLiteral("localBuffering"),
                                              QStringLiteral("rxQueue"), 4294967296.0)).ok);

  // What they must not be: a string, a boolean, or absent.
  expectRefusal(loads(headerWithConfigurationMember(standardHeader(),
                                                    QStringLiteral("localBuffering"),
                                                    QStringLiteral("rxQueue"),
                                                    QStringLiteral("1024"))),
                "a buffering member that is a string");
  expectRefusal(loads(headerWithConfigurationMember(standardHeader(),
                                                    QStringLiteral("localBuffering"),
                                                    QStringLiteral("flushRate"), true)),
                "a buffering member that is a boolean");
  expectRefusal(loads(headerWithoutConfigurationMember(standardHeader(),
                                                      QStringLiteral("localBuffering"),
                                                      QStringLiteral("flushRate"))),
                "a missing buffering member");
}

/** Checks 10-17: one case per rule, every one refused, none of them repaired. */
void TstSessionReader::headerViolationsRefuseTheFile()
{
  QList<QJsonObject> violations;

  // Missing required members (row 11) and misshaped required values (row 10).
  violations << headerWithoutRoot(standardHeader(), QStringLiteral("sources"));
  violations << headerWithoutRoot(standardHeader(), QStringLiteral("clockDomains"));
  violations << headerWithoutRoot(standardHeader(), QStringLiteral("application"));
  violations << headerWithRoot(standardHeader(), QStringLiteral("sources"), QJsonObject());
  violations << headerWithRoot(standardHeader(), QStringLiteral("format"),
                               QStringLiteral("other-format"));
  violations << headerWithRoot(standardHeader(), QStringLiteral("version"), 2);
  violations << headerWithRoot(standardHeader(), QStringLiteral("version"), QStringLiteral("1"));
  violations << headerWithRoot(standardHeader(), QStringLiteral("created"),
                               QStringLiteral("2026-09-18 12:00:00"));
  violations << headerWithRoot(standardHeader(), QStringLiteral("created"),
                               QStringLiteral("2026-09-18T12:00:00"));
  violations << headerWithRoot(standardHeader(), QStringLiteral("application"),
                               QJsonArray{ QStringLiteral("Komport") });
  violations << headerWithRoot(standardHeader(), QStringLiteral("application"),
                               QJsonObject{ { QStringLiteral("name"), 1 } });

  // The source descriptor (row 13).
  violations << headerWithSource(standardHeader(), QStringLiteral("sourceId"), 0);
  violations << headerWithSource(standardHeader(), QStringLiteral("sourceId"),
                                 QStringLiteral("1"));
  violations << headerWithSource(standardHeader(), QStringLiteral("clockDomainId"),
                                 QStringLiteral("no-such-domain"));
  violations << headerWithSource(standardHeader(), QStringLiteral("name"), 7);
  violations << headerWithSource(standardHeader(), QStringLiteral("transport"), QJsonArray());
  violations << headerWithoutSourceMember(standardHeader(), QStringLiteral("configuration"));
  violations << headerWithoutSourceMember(standardHeader(), QStringLiteral("sourceId"));

  // The exhaustive configuration schema (row 14).
  violations << headerWithConfigurationMember(standardHeader(), QStringLiteral("requested"),
                                              QStringLiteral("extra"),
                                              QStringLiteral("1"));
  violations << headerWithConfigurationMember(standardHeader(), QStringLiteral("requested"),
                                              QStringLiteral("baudRate"), 9600);
  violations << headerWithConfigurationMember(standardHeader(), QStringLiteral("compatibility"),
                                              QStringLiteral("startBits"), 1);
  violations << headerWithRoot(standardHeader(), QStringLiteral("sources"),
                               QJsonArray{ QJsonObject() });

  // The clock domains (rows 15-17).
  violations << headerWithDomain(standardHeader(), QStringLiteral("id"), 7);
  violations << headerWithDomain(standardHeader(), QStringLiteral("kind"),
                                 QStringLiteral("wall-clock"));
  violations << headerWithDomain(standardHeader(), QStringLiteral("reference"),
                                 QJsonObject{});
  violations << headerWithReference(standardHeader(), QStringLiteral("sourceTimestampNs"), 12);
  violations << headerWithReference(standardHeader(), QStringLiteral("sourceTimestampNs"),
                                    QStringLiteral("0012"));

  for (const QJsonObject &header : violations) {
    const SessionLoadOutcome outcome = loadBytes(buildFile(header));
    QVERIFY2(!outcome.ok, qPrintable(QStringLiteral("expected a refusal for %1")
                                         .arg(QString::fromUtf8(QJsonDocument(header).toJson(
                                             QJsonDocument::Compact)))));
    QVERIFY(!outcome.reason.isEmpty());
    QVERIFY(outcome.session == nullptr);
    QCOMPARE(outcome.eventCount, quint64(0));
  }
}

/** Check 13 and 16/17 at their boundaries: `sourceId` is an *integral* 32-bit
  *  number, and a 64-bit decimal must actually fit an `i64`. */
void TstSessionReader::numericHeaderShapesRefuseOutOfRangeValues()
{
  // The checks of rows 13/16/17 are header rules, so these fixtures carry no
  // record: what is asserted is the header's admission, not a stream.
  const auto loads = [](const QJsonObject &header) {
    return loadBytes(buildFile(header));
  };

  // The accepted boundaries: the smallest and the largest non-zero 32-bit id.
  QVERIFY(loads(headerWithSource(standardHeader(), QStringLiteral("sourceId"), 1)).ok);
  QVERIFY(loads(headerWithSource(standardHeader(), QStringLiteral("sourceId"),
                                 4294967295.0)).ok);

  expectRefusal(loads(headerWithSource(standardHeader(), QStringLiteral("sourceId"), 0)),
                "sourceId 0");
  expectRefusal(loads(headerWithSource(standardHeader(), QStringLiteral("sourceId"), 1.5)),
                "a fractional sourceId");
  expectRefusal(loads(headerWithSource(standardHeader(), QStringLiteral("sourceId"), -1)),
                "a negative sourceId");
  expectRefusal(loads(headerWithSource(standardHeader(), QStringLiteral("sourceId"),
                                       4294967296.0)),
                "a sourceId above UINT32_MAX");

  // A canonical-looking decimal outside the i64 range is not an i64.
  expectRefusal(loads(headerWithReference(standardHeader(), QStringLiteral("sourceTimestampNs"),
                                          QStringLiteral("99999999999999999999"))),
                "a reference decimal outside i64");
  expectRefusal(loads(headerWithReference(standardHeader(), QStringLiteral("sessionTimestampNs"),
                                          QStringLiteral("1"))),
                "a reference session time other than 0");

  // The canonical spelling itself: the i64 boundaries are representable (signed,
  // as ADR-006's field is), and every spelling the canonical form does not use
  // is refused - a plus sign, a negative zero, whitespace, a leading zero and a
  // value one past the upper bound.
  QVERIFY(loads(headerWithReference(standardHeader(), QStringLiteral("sourceTimestampNs"),
                                    QStringLiteral("9223372036854775807"))).ok);
  QVERIFY(loads(headerWithReference(standardHeader(), QStringLiteral("sourceTimestampNs"),
                                    QStringLiteral("-9223372036854775808"))).ok);
  QVERIFY(loads(headerWithReference(standardHeader(), QStringLiteral("sourceTimestampNs"),
                                    QStringLiteral("-1"))).ok);
  expectRefusal(loads(headerWithReference(standardHeader(), QStringLiteral("sourceTimestampNs"),
                                          QStringLiteral("+1"))),
                "a plus sign");
  expectRefusal(loads(headerWithReference(standardHeader(), QStringLiteral("sourceTimestampNs"),
                                          QStringLiteral("-0"))),
                "a negative zero");
  expectRefusal(loads(headerWithReference(standardHeader(), QStringLiteral("sourceTimestampNs"),
                                          QStringLiteral(" 1"))),
                "a leading space");
  expectRefusal(loads(headerWithReference(standardHeader(), QStringLiteral("sourceTimestampNs"),
                                          QStringLiteral("1 "))),
                "a trailing space");
  expectRefusal(loads(headerWithReference(standardHeader(), QStringLiteral("sourceTimestampNs"),
                                          QStringLiteral("01"))),
                "a leading zero");
  expectRefusal(loads(headerWithReference(standardHeader(), QStringLiteral("sourceTimestampNs"),
                                          QStringLiteral("9223372036854775808"))),
                "one past the i64 upper bound");
}

/** The length checks of rows 18-22 with u32-maximum fixtures: a declared length
  *  is refused, never wrapped into a plausible sum and never recovered. */
void TstSessionReader::lengthArithmeticDoesNotOverflow()
{
  const QByteArray header = buildFile(standardHeader());
  const quint32 u32Max = 0xFFFFFFFFu;

  // Row 19: a recordLength above the limit, with the file ending inside it. The
  // length is decided from the four bytes alone, so no byte beyond them is read.
  ScriptedSource oversized;
  oversized.bytes = header + rawRecordPrefix(u32Max, quint16(SessionEventType::Data),
                                             quint8(SessionDirection::Rx), 0, 1, 1, 1000, 250, 0,
                                             0);
  const SessionLoadOutcome oversizedOutcome = loadWithSource(oversized);
  expectRefusal(oversizedOutcome, "a declared recordLength above the limit");
  QVERIFY(oversizedOutcome.reason.contains(QStringLiteral("64 MiB")));
  QCOMPARE(oversized.bytesRead, qint64(header.size() + 4));

  // Row 22 with u32-maximum field values in a *complete* record (the declared
  // body is present, so the truncation rule cannot apply): the subtractive
  // checks must refuse the lengths instead of letting a sum wrap around.
  const QByteArray body(24, 'z');   // recordLength 64 = 40 + 24
  const QByteArray inconsistent =
      header
      + rawRecordPrefix(64, quint16(SessionEventType::Data), quint8(SessionDirection::Rx), 0, 1, 1,
                        1000, 250, u32Max, u32Max)
      + body;
  const SessionLoadOutcome inconsistentOutcome = loadBytes(inconsistent);
  expectRefusal(inconsistentOutcome, "content lengths that cannot fit the declared body");
  QVERIFY(inconsistentOutcome.reason.contains(QStringLiteral("inconsistent lengths")));

  // A metadata length that alone exceeds the body is refused as well.
  const QByteArray metadata =
      header
      + rawRecordPrefix(64, quint16(SessionEventType::Data), quint8(SessionDirection::Rx), 0, 1, 1,
                        1000, 250, u32Max, 0)
      + body;
  const SessionLoadOutcome metadataOutcome = loadBytes(metadata);
  expectRefusal(metadataOutcome, "a metadata length beyond the record body");
  QVERIFY(metadataOutcome.reason.contains(QStringLiteral("inconsistent lengths")));
}

/** Check 15's uniqueness rule: two clock domains sharing an id are refused. */
void TstSessionReader::duplicateClockDomainIdIsRefused()
{
  QJsonObject header = standardHeader();
  const QString id = sourceArray(header).at(0).toObject()
                         .value(QStringLiteral("clockDomainId"))
                         .toString();
  QVERIFY(!id.isEmpty());
  header = headerWithSecondDomain(header, id);

  const SessionLoadOutcome outcome = loadBytes(buildFile(header));
  expectRefusal(outcome, "two clock domains sharing an id");
  QVERIFY(outcome.reason.contains(id));
}

/** Check 17: the optional correlation is validated when present, and its
  *  presence or absence is never a reason to refuse. */
void TstSessionReader::malformedWallClockCorrelationIsRefused()
{
  const QByteArray records = recordBytes(dataEvent(1, SessionDirection::Rx));
  QJsonObject correlation;
  correlation.insert(QStringLiteral("wallClock"), QStringLiteral("2026-09-18T12:00:00Z"));
  correlation.insert(QStringLiteral("precisionNs"), QStringLiteral("1000"));

  // A well-formed correlation loads, and so does its absence.
  QVERIFY(loadBytes(buildFile(headerWithDomain(standardHeader(),
                                               QStringLiteral("wallClockCorrelation"),
                                               correlation),
                              records)).ok);
  QVERIFY(loadBytes(buildFile(standardHeader(), records)).ok);

  QJsonObject noZone = correlation;
  noZone.insert(QStringLiteral("wallClock"), QStringLiteral("2026-09-18T12:00:00"));
  expectRefusal(loadBytes(buildFile(headerWithDomain(standardHeader(),
                                                     QStringLiteral("wallClockCorrelation"),
                                                     noZone),
                                    records)),
                "a correlation without a UTC zone");

  QJsonObject notAString = correlation;
  notAString.insert(QStringLiteral("wallClock"), 20260918);
  expectRefusal(loadBytes(buildFile(headerWithDomain(standardHeader(),
                                                     QStringLiteral("wallClockCorrelation"),
                                                     notAString),
                                    records)),
                "a correlation whose wallClock is not a string");

  QJsonObject badPrecision = correlation;
  badPrecision.insert(QStringLiteral("precisionNs"), QStringLiteral("abc"));
  expectRefusal(loadBytes(buildFile(headerWithDomain(standardHeader(),
                                                     QStringLiteral("wallClockCorrelation"),
                                                     badPrecision),
                                    records)),
                "a correlation whose precisionNs is not canonical");

  QJsonObject precisionAsNumber = correlation;
  precisionAsNumber.insert(QStringLiteral("precisionNs"), 1000);
  expectRefusal(loadBytes(buildFile(headerWithDomain(standardHeader(),
                                                     QStringLiteral("wallClockCorrelation"),
                                                     precisionAsNumber),
                                    records)),
                "a correlation whose precisionNs is a JSON number");

  // An explicit JSON `null` is a *present* member that does not have the
  // specified shape; only an absent member is "absent" (review: slice-1
  // implementation review, finding 1).
  expectRefusal(loadBytes(buildFile(headerWithDomain(standardHeader(),
                                                     QStringLiteral("wallClockCorrelation"),
                                                     QJsonValue(QJsonValue::Null)),
                                    records)),
                "an explicit null correlation");

  // The correlation's own 64-bit value must be an i64 as well.
  QJsonObject precisionOutOfRange = correlation;
  precisionOutOfRange.insert(QStringLiteral("precisionNs"),
                             QStringLiteral("99999999999999999999"));
  expectRefusal(loadBytes(buildFile(headerWithDomain(standardHeader(),
                                                     QStringLiteral("wallClockCorrelation"),
                                                     precisionOutOfRange),
                                    records)),
                "a correlation whose precisionNs is outside the i64 range");
}

/** Row 12: ADR-006's permitted optional content is accepted and not interpreted
  *  - it is never a reason to refuse, and never reaches the typed facts. */
void TstSessionReader::permittedOptionalMembersAreAccepted()
{
  QJsonObject header = standardHeader();
  header.insert(QStringLiteral("notes"), QStringLiteral("a session recorded in the lab"));
  header.insert(QStringLiteral("decoderHints"),
                QJsonObject{ { QStringLiteral("suggested"), QStringLiteral("vt220") } });
  header.insert(QStringLiteral("alignmentMapping"), QJsonArray{ 1, 2, 3 });

  QJsonObject reference;
  reference.insert(QStringLiteral("sourceTimestampNs"), QStringLiteral("7"));
  reference.insert(QStringLiteral("sessionTimestampNs"), QStringLiteral("0"));
  QJsonObject domain;
  domain.insert(QStringLiteral("id"), QStringLiteral("local-process-monotonic-v1"));
  domain.insert(QStringLiteral("kind"), QStringLiteral("process-monotonic"));
  domain.insert(QStringLiteral("reference"), reference);
  domain.insert(QStringLiteral("wallClockCorrelation"),
                QJsonObject{ { QStringLiteral("wallClock"), QStringLiteral("2026-09-18T12:00:00Z") },
                             { QStringLiteral("precisionNs"), QStringLiteral("500") } });
  QJsonArray domains;
  domains.append(domain);
  header.insert(QStringLiteral("clockDomains"), domains);

  const SessionLoadOutcome outcome = loadBytes(buildFile(header, recordBytes(dataEvent(1, SessionDirection::Tx))));
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));
  QCOMPARE(outcome.eventCount, quint64(1));
  QCOMPARE(outcome.session->info.source.name, QStringLiteral("local serial"));
}

/** ADR-011 D2 (review round 1, finding 2): the fixed local profile values are
  *  not an admission test. Another in-range sourceId, another domain id and
  *  ADR-006's other kind all load, and every record resolves against the file's
  *  own descriptor. */
void TstSessionReader::localProfileValuesAreNotAnAdmissionTest()
{
  QJsonObject header = standardHeader();
  header = headerWithSource(header, QStringLiteral("sourceId"), 7);
  header = headerWithSource(header, QStringLiteral("name"), QStringLiteral("remote link"));
  header = headerWithSource(header, QStringLiteral("transport"), QStringLiteral("tcp"));
  header = headerWithSource(header, QStringLiteral("clockDomainId"),
                            QStringLiteral("agent-monotonic-1"));
  header = headerWithSecondDomain(header, QStringLiteral("agent-monotonic-1"));
  // The referenced entry is the second one; make it the agent kind.
  QJsonArray domains = header.value(QStringLiteral("clockDomains")).toArray();
  QJsonObject agent = domains.at(1).toObject();
  agent.insert(QStringLiteral("kind"), QStringLiteral("agent-monotonic"));
  domains.replace(1, agent);
  header.insert(QStringLiteral("clockDomains"), domains);

  const QByteArray records = recordBytes(dataEvent(4, SessionDirection::Rx,
                                                   QByteArrayLiteral("ping"), 4200, 900, 7));
  const SessionLoadOutcome outcome = loadBytes(buildFile(header, records));
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));
  QCOMPARE(outcome.session->info.source.sourceId, quint32(7));
  QCOMPARE(outcome.session->info.source.clockDomainId, QStringLiteral("agent-monotonic-1"));
  QCOMPARE(outcome.session->info.clockDomain.kind, QStringLiteral("agent-monotonic"));
  QCOMPARE(outcome.session->info.clockDomain.referenceSourceTimestampNs, qint64(42));
  QCOMPARE(outcome.session->info.clockDomain.referenceSessionTimestampNs, qint64(0));
  QCOMPARE(outcome.session->events.size(), 1);
  QCOMPARE(outcome.session->events.at(0).sourceId, quint32(7));
}

/** Row 29 and SPEC-M10 5.4: more than one source is refused with the verbatim
 *  M10 message; nothing is selected, inferred or rendered. */
void TstSessionReader::multiSourceFileIsRefusedWithTheM10Message()
{
  QJsonArray sources = sourceArray(standardHeader());
  QJsonObject second = sources.at(0).toObject();
  second.insert(QStringLiteral("sourceId"), 2);
  second.insert(QStringLiteral("name"), QStringLiteral("second link"));
  sources.append(second);
  const QJsonObject header = headerWithRoot(standardHeader(), QStringLiteral("sources"), sources);

  const SessionLoadOutcome outcome = loadBytes(buildFile(header, recordBytes(dataEvent(1, SessionDirection::Rx))));
  expectRefusal(outcome, "a multi-source session");
  QCOMPARE(outcome.reason, SessionReader::multiSourceRefusal());
  QCOMPARE(outcome.reason, QStringLiteral("multi-source sessions not supported in M10"));
}

/** Row 29's other half: a header that declares no source at all gets its own
  *  reason, so it cannot be mistaken for a multi-source file. */
void TstSessionReader::fileWithoutASourceIsRefused()
{
  const QJsonObject header = headerWithRoot(standardHeader(), QStringLiteral("sources"),
                                            QJsonArray());
  const SessionLoadOutcome outcome = loadBytes(buildFile(header));
  expectRefusal(outcome, "a file without a source");
  QVERIFY(outcome.reason.contains(QStringLiteral("declares no source")));
  QVERIFY(outcome.reason != SessionReader::multiSourceRefusal());
}

/** ADR-011 D2 (review round 1, finding 6): M10 adds no rule about the number of
  *  clock domains. A second, unreferenced entry is judged by ADR-006's rules
  *  alone, and the session's timeline is the domain the source resolves to. */
void TstSessionReader::clockDomainCountIsNotAnM10Rule()
{
  const QJsonObject header = headerWithSecondDomain(standardHeader());
  const SessionLoadOutcome outcome = loadBytes(buildFile(header, recordBytes(dataEvent(1, SessionDirection::Rx))));
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));
  QCOMPARE(outcome.session->info.clockDomain.id, QStringLiteral("local-process-monotonic-v1"));
  QCOMPARE(outcome.session->info.clockDomain.kind, QStringLiteral("process-monotonic"));
  QCOMPARE(outcome.session->info.clockDomain.referenceSourceTimestampNs,
           kAnchorSourceTimestampNs);
}

/** ADR-006's layout, field by field, against a hand-built byte fixture - and
  *  cross-checked with M9's independent test-side parser, so a shared defect in
  *  the writer codec cannot hide behind either reader. */
void TstSessionReader::recordPrefixIsReadFieldByField()
{
  // A hand-written v1 header carrying the local profile, not built by the codec.
  QJsonObject requested;
  requested.insert(QStringLiteral("endpoint"), QStringLiteral("/dev/pts/9"));
  requested.insert(QStringLiteral("baudRate"), QStringLiteral("115200"));
  requested.insert(QStringLiteral("dataBits"), QStringLiteral("7"));
  requested.insert(QStringLiteral("stopBits"), QStringLiteral("2"));
  requested.insert(QStringLiteral("parity"), QStringLiteral("EVEN"));
  requested.insert(QStringLiteral("flowControl"), QStringLiteral("RTS/CTS"));
  QJsonObject configuration;
  configuration.insert(QStringLiteral("requested"), requested);
  configuration.insert(QStringLiteral("effective"), requested);
  configuration.insert(QStringLiteral("localBuffering"),
                       QJsonObject{ { QStringLiteral("rxQueue"), 2048 },
                                     { QStringLiteral("flushRate"), 128 } });
  configuration.insert(QStringLiteral("compatibility"),
                       QJsonObject{ { QStringLiteral("startBits"), QStringLiteral("2") } });
  QJsonObject source;
  source.insert(QStringLiteral("sourceId"), 1);
  source.insert(QStringLiteral("clockDomainId"), QStringLiteral("local-process-monotonic-v1"));
  source.insert(QStringLiteral("name"), QStringLiteral("local serial"));
  source.insert(QStringLiteral("transport"), QStringLiteral("serial"));
  source.insert(QStringLiteral("configuration"), configuration);
  QJsonObject reference;
  reference.insert(QStringLiteral("sourceTimestampNs"), QStringLiteral("123456789"));
  reference.insert(QStringLiteral("sessionTimestampNs"), QStringLiteral("0"));
  QJsonObject domain;
  domain.insert(QStringLiteral("id"), QStringLiteral("local-process-monotonic-v1"));
  domain.insert(QStringLiteral("kind"), QStringLiteral("process-monotonic"));
  domain.insert(QStringLiteral("reference"), reference);
  QJsonObject header;
  header.insert(QStringLiteral("format"), QStringLiteral("komport-session"));
  header.insert(QStringLiteral("version"), 1);
  header.insert(QStringLiteral("created"), QStringLiteral("2026-09-18T12:00:00Z"));
  header.insert(QStringLiteral("application"),
                QJsonObject{ { QStringLiteral("name"), QStringLiteral("Komport") },
                             { QStringLiteral("version"), QStringLiteral("2.0-test") } });
  header.insert(QStringLiteral("sources"), QJsonArray{ source });
  header.insert(QStringLiteral("clockDomains"), QJsonArray{ domain });

  // A record whose every field differs from the fixture defaults.
  const QByteArray payload = QByteArrayLiteral("payload\x00with-NUL");
  const QByteArray metadataJson = QByteArrayLiteral("{\"note\":\"hand-built\"}");
  const quint32 recordLength = quint32(40 + metadataJson.size() + payload.size());
  const QByteArray record = rawRecord(recordLength, quint16(SessionEventType::Data),
                                      quint8(SessionDirection::Rx), 0, quint64(42), quint32(1),
                                      qint64(987654321), qint64(123456789),
                                      quint32(metadataJson.size()), quint32(payload.size()),
                                      metadataJson, payload);
  const QByteArray file = buildFile(header, record);

  const SessionLoadOutcome outcome = loadBytes(file);
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));
  QCOMPARE(outcome.eventCount, quint64(1));
  QCOMPARE(outcome.session->info.version, 1);
  QCOMPARE(outcome.session->info.createdUtc,
           QDateTime::fromString(QStringLiteral("2026-09-18T12:00:00Z"), Qt::ISODate));
  QCOMPARE(outcome.session->info.applicationName, QStringLiteral("Komport"));
  QCOMPARE(outcome.session->info.applicationVersion, QStringLiteral("2.0-test"));
  QCOMPARE(outcome.session->info.source.sourceId, quint32(1));
  QCOMPARE(outcome.session->info.source.name, QStringLiteral("local serial"));
  QCOMPARE(outcome.session->info.source.transport, QStringLiteral("serial"));
  QCOMPARE(outcome.session->info.clockDomain.id, QStringLiteral("local-process-monotonic-v1"));
  QCOMPARE(outcome.session->info.clockDomain.referenceSourceTimestampNs, qint64(123456789));
  QCOMPARE(outcome.session->info.source.configuration, configuration);

  const SessionEvent &event = outcome.session->events.at(0);
  QCOMPARE(event.sequence, quint64(42));
  QCOMPARE(event.sourceId, quint32(1));
  QCOMPARE(event.sourceTimestampNs, qint64(987654321));
  QCOMPARE(event.timestampNs, qint64(123456789));
  QCOMPARE(event.type, SessionEventType::Data);
  QCOMPARE(event.direction, SessionDirection::Rx);
  QCOMPARE(event.payload, payload);
  QCOMPARE(event.metadata, QJsonDocument::fromJson(metadataJson).object());

  // Cross-check: the hand-built fixture is what M9's independent test-side
  // parser reads as well.
  const SessionRecordFile parsed = readSessionRecordFile(file);
  QVERIFY2(parsed.loadable, qPrintable(parsed.error));
  QCOMPARE(parsed.records.size(), 1);
  QCOMPARE(parsed.records.at(0).eventType, quint16(SessionEventType::Data));
  QCOMPARE(parsed.records.at(0).direction, quint8(SessionDirection::Rx));
  QCOMPARE(parsed.records.at(0).flags, quint8(0));
  QCOMPARE(parsed.records.at(0).sequence, quint64(42));
  QCOMPARE(parsed.records.at(0).sourceId, quint32(1));
  QCOMPARE(parsed.records.at(0).sourceTimestampNs, qint64(987654321));
  QCOMPARE(parsed.records.at(0).timestampNs, qint64(123456789));
  QCOMPARE(parsed.records.at(0).recordLength, recordLength);
}

/** Checks 20/21: a cut-short final record is recovered and reported, and the
  *  truncated record is never delivered as an event. */
void TstSessionReader::recoveredTruncatedFinalRecordIsReportedNotDelivered()
{
  const SessionEvent complete = dataEvent(1, SessionDirection::Rx, QByteArrayLiteral("keep-me"));
  const SessionEvent cut = dataEvent(2, SessionDirection::Tx, QByteArrayLiteral("drop-me"));
  const QByteArray completeBytes = recordBytes(complete);
  const QByteArray cutBytes = recordBytes(cut);

  // A record whose declared length promises more than the file holds.
  const QByteArray file = buildFile(standardHeader(),
                                    completeBytes + cutBytes.left(cutBytes.size() - 3));
  const SessionLoadOutcome outcome = loadBytes(file);
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));
  QVERIFY(outcome.recoveredTruncatedFinalRecord);
  QCOMPARE(outcome.eventCount, quint64(1));
  QCOMPARE(outcome.session->events.size(), 1);
  QCOMPARE(outcome.session->events.at(0).payload, QByteArrayLiteral("keep-me"));
  QCOMPARE(outcome.session->events.at(0).sequence, quint64(1));

  // A final record whose first bytes are present but whose length field is not
  // complete: also recovered, and also nothing is delivered from it.
  const QByteArray almost = buildFile(standardHeader(), completeBytes + cutBytes.left(2));
  const SessionLoadOutcome almostOutcome = loadBytes(almost);
  QVERIFY2(almostOutcome.ok, qPrintable(almostOutcome.reason));
  QVERIFY(almostOutcome.recoveredTruncatedFinalRecord);
  QCOMPARE(almostOutcome.eventCount, quint64(1));
  QVERIFY(almostOutcome.session->events.at(0).payload != QByteArrayLiteral("drop-me"));
}

/** Checks 3/8 from the other side: a file that ends inside the start block or
  *  inside its header is refused, never recovered - ADR-006 recovers a record,
  *  not a header. */
void TstSessionReader::truncationInsideTheStartBlockOrHeaderRefusesTheFile()
{
  const QByteArray valid =
      buildFile(standardHeader(), recordBytes(dataEvent(1, SessionDirection::Rx)));
  expectRefusal(loadBytes(valid.left(12)), "a file cut inside the start block");

  const QByteArray headerOnly = buildFile(standardHeader());
  expectRefusal(loadBytes(headerOnly.left(headerOnly.size() - 5)),
                "a file cut inside the header");

  // The same cut with a source that hides its size: the reads alone must refuse.
  ScriptedSource source;
  source.bytes = headerOnly.left(headerOnly.size() - 5);
  source.hideSize = true;
  const SessionLoadOutcome outcome = loadWithSource(source);
  expectRefusal(outcome, "a file cut inside the header (unknown size)");
  QVERIFY(outcome.reason.contains(QStringLiteral("exceeds the file")));
}

/** Check 18: a declared `recordLength` below its own prefix is refused - and the
  *  loader stops there instead of reading on (review round 2, finding 4). */
void TstSessionReader::recordLengthBelowThePrefixIsRefused()
{
  const QByteArray header = buildFile(standardHeader());
  const QByteArray belowPrefix = rawRecordPrefix(20, quint16(SessionEventType::Data),
                                                 quint8(SessionDirection::Rx), 0, 1, 1, 1000, 250,
                                                 0, 0);

  // A valid record follows, so a loader that read on would consume it.
  const QByteArray following = recordBytes(dataEvent(2, SessionDirection::Tx));

  ScriptedSource source;
  source.bytes = header + belowPrefix + following;
  const SessionLoadOutcome outcome = loadWithSource(source);
  expectRefusal(outcome, "a recordLength below the prefix");
  QVERIFY(outcome.reason.contains(QStringLiteral("below its own prefix")));
  // The start block, the header and the four length bytes - nothing beyond them.
  QCOMPARE(source.bytesRead, qint64(header.size() + 4));
}

/** Check 19: a declared length above ADR-006's limit is refused *before* the
  *  truncation rule can treat the record as a cut-short write; the limit itself
  *  is inclusive. */
void TstSessionReader::recordLengthAboveTheLimitIsRefusedBeforeRecovery()
{
  const QByteArray header = buildFile(standardHeader());

  const QByteArray aboveLimit =
      header + rawRecordPrefix(quint32(SessionReader::kMaxRecordBodyBytes) + 1,
                               quint16(SessionEventType::Data), quint8(SessionDirection::Rx), 0, 1,
                               1, 1000, 250, 0, 0);
  const SessionLoadOutcome refused = loadBytes(aboveLimit);
  expectRefusal(refused, "a recordLength one byte above the limit");
  QVERIFY(refused.reason.contains(QStringLiteral("64 MiB")));

  // Exactly the limit is not "above" it: the record is still recovered as a
  // truncated final record, which is a different outcome with a different
  // meaning (row 21), not a limit refusal.
  const QByteArray atLimit =
      header + rawRecordPrefix(quint32(SessionReader::kMaxRecordBodyBytes),
                               quint16(SessionEventType::Data), quint8(SessionDirection::Rx), 0, 1,
                               1, 1000, 250, 0, 0);
  const SessionLoadOutcome recovered = loadBytes(atLimit);
  QVERIFY2(recovered.ok, qPrintable(recovered.reason));
  QVERIFY(recovered.recoveredTruncatedFinalRecord);
  QVERIFY(!recovered.reason.contains(QStringLiteral("64 MiB")));
  QCOMPARE(recovered.eventCount, quint64(0));
}

/** Check 22: `40 + metadataLength + payloadLength` must equal the declared
  *  `recordLength`, and a malformed record refuses the whole file - a good first
  *  record does not buy a broken second one. */
void TstSessionReader::inconsistentRecordLengthsRefuseTheWholeFile()
{
  const QByteArray header = buildFile(standardHeader());

  const QByteArray tooShort = rawRecord(50, quint16(SessionEventType::Data),
                                        quint8(SessionDirection::Rx), 0, 1, 1, 1000, 250, 0, 20,
                                        QByteArray(), QByteArray(20, 'x'));
  const SessionLoadOutcome outcome = loadBytes(header + tooShort);
  expectRefusal(outcome, "lengths that do not add up");
  QVERIFY(outcome.reason.contains(QStringLiteral("inconsistent lengths")));

  const QByteArray good = recordBytes(dataEvent(1, SessionDirection::Rx));
  const QByteArray bad = rawRecord(60, quint16(SessionEventType::Data),
                                   quint8(SessionDirection::Rx), 0, 2, 1, 2000, 500, 0, 30,
                                   QByteArray(), QByteArray(30, 'y'));
  const SessionLoadOutcome wholeFile = loadBytes(header + good + bad);
  expectRefusal(wholeFile, "a malformed second record");
  QVERIFY(wholeFile.reason.contains(QStringLiteral("inconsistent lengths")));
}

/** Check 23: ADR-006 fixes `flags` to zero in v1. */
void TstSessionReader::nonZeroFlagsRefuseTheFile()
{
  const QByteArray header = buildFile(standardHeader());
  const QByteArray record = rawRecord(42, quint16(SessionEventType::Data),
                                      quint8(SessionDirection::Rx), 1, 1, 1, 1000, 250, 0, 2,
                                      QByteArray(), QByteArrayLiteral("AB"));
  const SessionLoadOutcome outcome = loadBytes(header + record);
  expectRefusal(outcome, "a record with non-zero flags");
  QVERIFY(outcome.reason.contains(QStringLiteral("flags")));
}

/** Check 24: a non-empty metadata section must be a JSON object; the
  *  zero-length encoding is normal and is the empty object (checked in
  *  payloadsSurviveByteExactly). */
void TstSessionReader::nonObjectMetadataRefusesTheFile()
{
  const QByteArray header = buildFile(standardHeader());
  const QByteArray record = rawRecord(45, quint16(SessionEventType::Data),
                                      quint8(SessionDirection::Rx), 0, 1, 1, 1000, 250, 3, 2,
                                      QByteArrayLiteral("[1]"), QByteArrayLiteral("AB"));
  const SessionLoadOutcome outcome = loadBytes(header + record);
  expectRefusal(outcome, "metadata that is not a JSON object");
  QVERIFY(outcome.reason.contains(QStringLiteral("metadata")));
}

/** Check 25: ADR-002's event-local contract, case by case - an unknown type, a
  *  wrong direction, a wrong payload, sourceId 0 and negative timestamps. */
void TstSessionReader::invalidEventFieldsRefuseTheFile()
{
  const QByteArray header = buildFile(standardHeader());
  const quint8 rx = quint8(SessionDirection::Rx);
  const quint8 none = quint8(SessionDirection::None);
  const quint8 tx = quint8(SessionDirection::Tx);

  QList<QByteArray> violations;
  // An eventType outside the declared enumerators.
  violations << rawRecord(42, quint16(99), rx, 0, 1, 1, 1000, 250, 0, 2, QByteArray(),
                          QByteArrayLiteral("AB"));
  // Data without a direction, and with an undeclared one.
  violations << rawRecord(42, quint16(SessionEventType::Data), none, 0, 1, 1, 1000, 250, 0, 2,
                          QByteArray(), QByteArrayLiteral("AB"));
  violations << rawRecord(42, quint16(SessionEventType::Data), quint8(9), 0, 1, 1, 1000, 250, 0, 2,
                          QByteArray(), QByteArrayLiteral("AB"));
  // Data without a payload.
  violations << rawRecord(40, quint16(SessionEventType::Data), rx, 0, 1, 1, 1000, 250, 0, 0);
  // A non-data event with a direction, and one with a payload.
  violations << rawRecord(42, quint16(SessionEventType::Annotation), tx, 0, 1, 1, 1000, 250, 0, 2,
                          QByteArray(), QByteArrayLiteral("AB"));
  violations << rawRecord(42, quint16(SessionEventType::Bookmark), none, 0, 1, 1, 1000, 250, 0, 2,
                          QByteArray(), QByteArrayLiteral("AB"));
  // sourceId 0, then the two negative timestamps.
  violations << rawRecord(42, quint16(SessionEventType::Data), rx, 0, 1, 0, 1000, 250, 0, 2,
                          QByteArray(), QByteArrayLiteral("AB"));
  violations << rawRecord(42, quint16(SessionEventType::Data), rx, 0, 1, 1, -1, 250, 0, 2,
                          QByteArray(), QByteArrayLiteral("AB"));
  violations << rawRecord(42, quint16(SessionEventType::Data), rx, 0, 1, 1, 1000, -1, 0, 2,
                          QByteArray(), QByteArrayLiteral("AB"));

  for (const QByteArray &record : violations) {
    const SessionLoadOutcome outcome = loadBytes(header + record);
    expectRefusal(outcome, "an event-local violation");
  }
}

/** Check 26: a record whose `sourceId` is not the file's source resolves to
  *  nothing and refuses the file. */
void TstSessionReader::recordSourceMustResolveToTheFileSource()
{
  const QByteArray header = buildFile(standardHeader());
  const QByteArray record =
      recordBytes(dataEvent(1, SessionDirection::Rx, QByteArrayLiteral("AB"), 1000, 250, 2));
  const SessionLoadOutcome outcome = loadBytes(header + record);
  expectRefusal(outcome, "a record of an unknown source");
  QVERIFY(outcome.reason.contains(QStringLiteral("does not resolve")));
}

/** Check 27: the sequence is non-zero and strictly increasing - and, per
  *  ADR-010 4, it may start above 1 (a recording can begin mid-session). */
void TstSessionReader::sequenceMustStrictlyIncrease()
{
  const QByteArray header = buildFile(standardHeader());

  expectRefusal(loadBytes(header + recordBytes(dataEvent(0, SessionDirection::Rx))),
                "a zero sequence");

  const QByteArray first = recordBytes(dataEvent(5, SessionDirection::Rx, QByteArrayLiteral("A"),
                                                 1000, 250));
  const QByteArray repeated = recordBytes(dataEvent(5, SessionDirection::Tx,
                                                    QByteArrayLiteral("B"), 2000, 500));
  expectRefusal(loadBytes(header + first + repeated), "a repeated sequence");

  const QByteArray decreasing = recordBytes(dataEvent(4, SessionDirection::Tx,
                                                      QByteArrayLiteral("C"), 3000, 750));
  expectRefusal(loadBytes(header + first + decreasing), "a decreasing sequence");

  // A mid-session start is legitimate: only monotonicity is required.
  const QByteArray increasing = recordBytes(dataEvent(6, SessionDirection::Tx,
                                                      QByteArrayLiteral("D"), 3000, 750));
  const SessionLoadOutcome outcome = loadBytes(header + first + increasing);
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));
  QCOMPARE(outcome.eventCount, quint64(2));
}

/** A file recorded from an already-live session: the first record carries a
  *  sequence above 1 and a session time above 0, while the header carries the
  *  domain's true anchor - and every stored value comes back unchanged. */
void TstSessionReader::aMidSessionRecordingLoadsWithItsTrueAnchor()
{
  const QJsonObject header = headerWithReference(standardHeader(),
                                                QStringLiteral("sourceTimestampNs"),
                                                QStringLiteral("123456789"));
  const SessionEvent first = dataEvent(5, SessionDirection::Rx, QByteArrayLiteral("late"),
                                       5000000000LL, 1000000);
  const SessionEvent second = dataEvent(6, SessionDirection::Tx, QByteArrayLiteral("later"),
                                        5000001000LL, 1100000);

  const SessionLoadOutcome outcome =
      loadBytes(buildFile(header, recordBytes(first) + recordBytes(second)));
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));
  QCOMPARE(outcome.session->info.clockDomain.referenceSourceTimestampNs, qint64(123456789));
  QCOMPARE(outcome.session->info.clockDomain.referenceSessionTimestampNs, qint64(0));
  QCOMPARE(outcome.session->events.at(0).sequence, quint64(5));
  QCOMPARE(outcome.session->events.at(0).timestampNs, qint64(1000000));
  QCOMPARE(outcome.session->events.at(0).sourceTimestampNs, qint64(5000000000LL));
  QCOMPARE(outcome.session->events.at(1).sequence, quint64(6));
  QCOMPARE(outcome.session->events.at(1).payload, QByteArrayLiteral("later"));
}

/** Check 28: session time never decreases within the resolved domain - equal
  *  stored times are fine (ADR-002's producer property). */
void TstSessionReader::sessionTimeMustNotDecreaseInTheDomain()
{
  const QByteArray header = buildFile(standardHeader());
  const SessionEvent first = dataEvent(1, SessionDirection::Rx, QByteArrayLiteral("A"), 1000, 500);

  const SessionEvent backwards = dataEvent(2, SessionDirection::Tx, QByteArrayLiteral("B"), 2000,
                                           400);
  const SessionLoadOutcome refused = loadBytes(header + recordBytes(first) + recordBytes(backwards));
  expectRefusal(refused, "a decreasing session time");
  QVERIFY(refused.reason.contains(QStringLiteral("session time decreases")));

  const SessionEvent equal = dataEvent(2, SessionDirection::Tx, QByteArrayLiteral("B"), 2000, 500);
  const SessionLoadOutcome accepted = loadBytes(header + recordBytes(first) + recordBytes(equal));
  QVERIFY2(accepted.ok, qPrintable(accepted.reason));
  QCOMPARE(accepted.eventCount, quint64(2));
}

/** ADR-011 D5: payload bytes, the empty metadata encoding and 64-bit metadata
  *  values survive byte-exactly - all 256 byte values, an embedded NUL and a
  *  canonical decimal above 2^53. */
void TstSessionReader::payloadsSurviveByteExactly()
{
  QByteArray allByteValues;
  for (int value = 0; value <= 0xFF; ++value)
    allByteValues.append(char(value));

  const SessionEvent withEveryByte = dataEvent(1, SessionDirection::Rx, allByteValues, 1000, 250);
  const SessionEvent withEmptyMetadata = nonDataEvent(SessionEventType::Bookmark, 2, 500);
  SessionEvent withLargeMetadata =
      dataEvent(3, SessionDirection::Tx, QByteArrayLiteral("high"), 3000, 750);
  withLargeMetadata.metadata.insert(QStringLiteral("ns"),
                                    sessionJsonInteger(Q_INT64_C(9007199254740993)));

  const SessionLoadOutcome outcome =
      loadBytes(buildFile(standardHeader(), recordBytes(withEveryByte)
                                              + recordBytes(withEmptyMetadata)
                                              + recordBytes(withLargeMetadata)));
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));
  QCOMPARE(outcome.eventCount, quint64(3));
  QCOMPARE(outcome.session->events.at(0).payload, allByteValues);
  QCOMPARE(outcome.session->events.at(0).payload.size(), 256);
  QCOMPARE(outcome.session->events.at(1).metadata, QJsonObject());
  // The zero-length metadata encoding is the empty object, not the two bytes "{}".
  const SessionRecordFile parsed = readSessionRecordFile(
      buildFile(standardHeader(), recordBytes(withEmptyMetadata)));
  QVERIFY(parsed.loadable);
  QCOMPARE(parsed.records.at(0).metadataJson, QByteArray());
  QCOMPARE(outcome.session->events.at(2).metadata.value(QStringLiteral("ns")).toString(),
           QStringLiteral("9007199254740993"));
}

/** Every event type ADR-002 declares loads with its type, direction and payload
  *  unchanged - cross-checked against M9's independent parser. */
void TstSessionReader::sessionWithEveryEventTypeLoadsFieldByField()
{
  const QList<SessionEvent> events{
    dataEvent(1, SessionDirection::Tx, QByteArrayLiteral("out"), 1000, 100),
    dataEvent(2, SessionDirection::Rx, QByteArrayLiteral("in"), 1100, 200),
    nonDataEvent(SessionEventType::TransportOpened, 3, 300),
    nonDataEvent(SessionEventType::TransportClosed, 4, 400),
    nonDataEvent(SessionEventType::TransportConfigChanged, 5, 500),
    nonDataEvent(SessionEventType::LineStateChanged, 6, 600),
    nonDataEvent(SessionEventType::Error, 7, 700),
    nonDataEvent(SessionEventType::Annotation, 8, 800),
    nonDataEvent(SessionEventType::Bookmark, 9, 900),
  };

  QByteArray records;
  for (const SessionEvent &event : events)
    records.append(recordBytes(event));

  const SessionLoadOutcome outcome = loadBytes(buildFile(standardHeader(), records));
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));
  QCOMPARE(outcome.eventCount, quint64(9));
  QCOMPARE(outcome.session->events.size(), events.size());

  for (int index = 0; index < events.size(); ++index) {
    const SessionEvent &loaded = outcome.session->events.at(index);
    QCOMPARE(loaded.type, events.at(index).type);
    QCOMPARE(loaded.direction, events.at(index).direction);
    QCOMPARE(loaded.sequence, events.at(index).sequence);
    QCOMPARE(loaded.payload, events.at(index).payload);
    QCOMPARE(loaded.sourceId, events.at(index).sourceId);
    QCOMPARE(loaded.timestampNs, events.at(index).timestampNs);
    QCOMPARE(loaded.sourceTimestampNs, events.at(index).sourceTimestampNs);
  }

  const SessionRecordFile parsed = readSessionRecordFile(buildFile(standardHeader(), records));
  QVERIFY2(parsed.loadable, qPrintable(parsed.error));
  QCOMPARE(parsed.records.size(), 9);
  for (int index = 0; index < events.size(); ++index) {
    QCOMPARE(quint16(events.at(index).type), parsed.records.at(index).eventType);
    QCOMPARE(quint8(events.at(index).direction), parsed.records.at(index).direction);
  }
}

/** ADR-006 imposes no minimum record count: a file that ends after its header is
  *  a valid v1 session with zero events. (The replay half of this criterion -
  *  `play()` and the steps refusing with a reason - belongs to the player's
  *  slice.) */
void TstSessionReader::zeroRecordFileIsAValidSessionWithNoEvents()
{
  const QByteArray file = buildFile(standardHeader());
  const SessionLoadOutcome outcome = loadBytes(file);
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));
  QVERIFY(outcome.session != nullptr);
  QCOMPARE(outcome.eventCount, quint64(0));
  QVERIFY(outcome.session->events.isEmpty());
  QVERIFY(!outcome.recoveredTruncatedFinalRecord);
  QCOMPARE(outcome.bytesRead, qint64(file.size()));

  // The same file through the production reader, from a real file.
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionReader reader;
  const SessionLoadOutcome fromDisk =
      reader.load(writeTemporaryFile(directory, file, QStringLiteral("empty.kpsession")));
  QVERIFY2(fromDisk.ok, qPrintable(fromDisk.reason));
  QCOMPARE(fromDisk.eventCount, quint64(0));
}

/** Check 2 and the loader's open rule: an I/O error, a source that stops early
  *  and a path that cannot be opened all refuse, without a session. */
void TstSessionReader::readErrorRefusesTheFileWithoutASession()
{
  const QByteArray file = buildFile(standardHeader(),
                                    recordBytes(dataEvent(1, SessionDirection::Rx)));

  ScriptedSource failing;
  failing.bytes = file;
  failing.failAfter = 0;
  const SessionLoadOutcome errorOutcome = loadWithSource(failing);
  expectRefusal(errorOutcome, "an I/O error on the first read");
  QVERIFY(errorOutcome.reason.contains(QStringLiteral("could not be read")));

  ScriptedSource shortInStartBlock;
  shortInStartBlock.bytes = file;
  shortInStartBlock.stopAfter = 10;
  expectRefusal(loadWithSource(shortInStartBlock), "a source that stops inside the start block");

  ScriptedSource shortInHeader;
  shortInHeader.bytes = file;
  shortInHeader.stopAfter = 20;
  const SessionLoadOutcome headerOutcome = loadWithSource(shortInHeader);
  expectRefusal(headerOutcome, "a source that stops inside the header");
  QVERIFY(headerOutcome.reason.contains(QStringLiteral("exceeds the file")));

  ScriptedSource unopenable;
  unopenable.openFails = true;
  const SessionLoadOutcome openOutcome = loadWithSource(unopenable);
  expectRefusal(openOutcome, "a path that cannot be opened");
  QVERIFY(openOutcome.reason.contains(QStringLiteral("could not be opened")));
  QCOMPARE(unopenable.bytesRead, qint64(0));

  // The source is closed again in both outcomes, so no handle survives a load.
  QCOMPARE(unopenable.closeCalls, 0);   // never opened, so never closed
  QCOMPARE(shortInHeader.closeCalls, 1);
}

/** ADR-011 D5/D10: loading never writes, and the file is exactly what it was -
  *  after a successful load and after a refused one. */
void TstSessionReader::theFileIsUnchangedByLoading()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionReader reader;

  const QByteArray bytes =
      buildFile(standardHeader(), recordBytes(dataEvent(1, SessionDirection::Rx,
                                                        QByteArrayLiteral("bytes"))));
  const QString path = writeTemporaryFile(directory, bytes);
  QVERIFY(!path.isEmpty());

  const SessionLoadOutcome outcome = reader.load(path);
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));

  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), bytes);
  file.close();

  // A refused file is untouched as well.
  const QByteArray notASession = QByteArrayLiteral("this is not a session");
  const QString refusedPath =
      writeTemporaryFile(directory, notASession, QStringLiteral("other.kpsession"));
  expectRefusal(reader.load(refusedPath), "a file that is not a session");
  QFile refusedFile(refusedPath);
  QVERIFY(refusedFile.open(QIODevice::ReadOnly));
  QCOMPARE(refusedFile.readAll(), notASession);
  refusedFile.close();

  // A missing path and a directory both fail through the loader's own rule,
  // which is what keeps a directory from being read as a session.
  const SessionLoadOutcome missing = reader.load(directory.filePath(QStringLiteral("missing.kpsession")));
  expectRefusal(missing, "a missing file");
  QVERIFY(missing.reason.contains(QStringLiteral("could not be opened")));

  const SessionLoadOutcome asDirectory = reader.load(directory.path());
  expectRefusal(asDirectory, "a directory");
  QVERIFY(asDirectory.reason.contains(QStringLiteral("could not be opened")));
}

/** ADR-011 D10: the reader is stateless between loads - a refusal does not
  *  influence the next load, and two loads return independent session values. */
void TstSessionReader::theReaderHoldsNoStateBetweenLoads()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QByteArray good =
      buildFile(standardHeader(), recordBytes(dataEvent(1, SessionDirection::Rx,
                                                        QByteArrayLiteral("A"))));
  const QByteArray refused = buildFile(headerWithRoot(standardHeader(), QStringLiteral("format"),
                                                      QStringLiteral("other-format")));
  const QString goodPath = writeTemporaryFile(directory, good, QStringLiteral("good.kpsession"));
  const QString refusedPath =
      writeTemporaryFile(directory, refused, QStringLiteral("refused.kpsession"));

  SessionReader reader;
  const SessionLoadOutcome first = reader.load(goodPath);
  const SessionLoadOutcome broken = reader.load(refusedPath);
  const SessionLoadOutcome again = reader.load(goodPath);

  QVERIFY2(first.ok, qPrintable(first.reason));
  expectRefusal(broken, "a refused load between two successful ones");
  QVERIFY2(again.ok, qPrintable(again.reason));

  QCOMPARE(again.eventCount, first.eventCount);
  QCOMPARE(again.bytesRead, first.bytesRead);
  QCOMPARE(again.session->events.at(0).payload, first.session->events.at(0).payload);
  QVERIFY(!again.recoveredTruncatedFinalRecord);
  // Independent loads produce independent immutable session values.
  QVERIFY(again.session != first.session);
}

QTEST_MAIN(TstSessionReader)
#include "tst_sessionreader.moc"
