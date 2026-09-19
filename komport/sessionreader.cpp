/***************************************************************************
                       sessionreader.cpp  -  Komport Serial Port Communicator
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

#include "sessionreader.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStringList>

#include <QtEndian>

#include <cmath>
#include <memory>

// SPEC-M10 section 5.3: the loader applies ADR-006's rules in the order the
// tables list them, and every declared length is validated against its limit
// before the bytes it describes are read or a buffer is sized from it. The
// order below is the normative one (SPEC-M10 5.3.1 rows 1-24, 5.3.2 rows 25-28,
// 5.3.3 row 29 during header validation):
//
//   start block (3-6) -> declared header length (7, 8) -> header JSON (9) ->
//   header content (11, 10, 29, 13, 14, 15, 16, 17) -> records, one at a time:
//   the byte count of the final, incomplete record (20) or the declared
//   `recordLength` alone (18, 19) - the two length checks come *before* the
//   truncation rule, so a malformed length is never recovered as a truncation -
//   then the record body (21), the record's own fields (22, 23, 24), the event
//   contract of ADR-002 (25) and the stream properties (26, 27, 28).
//
// All length arithmetic is carried out in qint64 with subtractive bounds checks
// (no addition of two unvalidated quantities), so a declared length can neither
// wrap nor be widened into a plausible one.
//
// Record indices in the refusal reasons are zero-based, like the record list of
// the loader's own outcome and M9's test-side parser.

namespace {

/** ADR-006's record prefix without its leading `recordLength` field: the bytes a
  * declared `recordLength` has to cover at the very least (SPEC-M10 5.3.1 row 18)
  * and the tail the two content sections are measured against (row 22). */
constexpr qint64 kRecordTailBytes = 40;

/** The production byte source: `QFile`, opened read-only (SPEC-M10 5.10).
  *
  * A path that is not a regular file is refused rather than read: a directory,
  * a device or a pipe is not a v1 container, and reading one could block
  * indefinitely (SPEC-M10 5.3.1 row 1).
  */
class FileByteSource : public SessionByteSource
{
public:
  ~FileByteSource() override;

  bool open(const QString &path) override
  {
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile())
      return false;
    mFile.setFileName(path);
    return mFile.open(QIODevice::ReadOnly);
  }

  qint64 size() const override { return mFile.isOpen() ? mFile.size() : -1; }

  qint64 read(QByteArray &out, qint64 maxBytes) override
  {
    if (!mFile.isOpen() || maxBytes <= 0)
      return 0;
    const QByteArray chunk = mFile.read(maxBytes);
    if (chunk.isEmpty() && mFile.error() != QFileDevice::NoError && !mFile.atEnd())
      return -1;
    out.append(chunk);
    return chunk.size();
  }

  void close() override { mFile.close(); }

private:
  QFile mFile;
};

/** Close the source when the load returns, whatever the outcome. */
struct SourceCloser {
  explicit SourceCloser(SessionByteSource *source) : mSource(source) {}
  ~SourceCloser() { mSource->close(); }
  SessionByteSource *mSource;
};

/** One read attempt, kept distinct from "the source ended" and "the read failed"
  * so that SPEC-M10 5.3.1 checks 2, 3, 8, 20 and 21 stay separate outcomes. */
struct ReadOutcome {
  qint64 bytes = 0;     ///< bytes appended to the caller's buffer
  bool complete = false;///< exactly as many bytes as were asked for
  bool error = false;   ///< the source reported an I/O error
};

/** Read up to @p wanted bytes, stopping at the end of the source. A source that
  * hands out fewer bytes than asked for without an error is legal (a short
  * read); the caller decides whether that is a refusal or a recovery. */
ReadOutcome readUpTo(SessionByteSource *source, qint64 wanted, QByteArray &out)
{
  ReadOutcome outcome;
  while (outcome.bytes < wanted) {
    const qint64 got = source->read(out, wanted - outcome.bytes);
    if (got < 0) {
      outcome.error = true;
      return outcome;
    }
    if (got == 0)
      return outcome;
    outcome.bytes += got;
  }
  outcome.complete = true;
  return outcome;
}

/** An integral JSON number inside [@p minimum, @p maximum].
  *
  * ADR-006 keeps 32-bit quantities as JSON numbers, so a string, a fractional
  * value or a value outside the range is not the shape `sourceId` must have
  * (SPEC-M10 5.3.1 row 13; review round 3, finding 3). The only caller is that
  * check: the nested `configuration` numbers are JSON numbers and nothing more
  * (row 14), so no other member is range-checked here.
  */
bool integralJsonNumber(const QJsonValue &value, double minimum, double maximum, qint64 *out)
{
  if (!value.isDouble())
    return false;
  const double asDouble = value.toDouble();
  if (!std::isfinite(asDouble) || std::trunc(asDouble) != asDouble)
    return false;
  if (asDouble < minimum || asDouble > maximum)
    return false;
  if (out != nullptr)
    *out = static_cast<qint64>(asDouble);
  return true;
}

/** A *canonical* decimal string holding an `i64` - the only JSON shape ADR-006
  * allows for a 64-bit quantity.
  *
  * Canonical means exactly what a writer emits: an optional leading minus sign
  * for a negative value, decimal digits, no leading zero unless the value is
  * `"0"`, no plus sign and no whitespace, and a value inside the `i64` range
  * (SPEC-M10 5.3.1 rows 16 and 17; `"99999999999999999999"` looks canonical and
  * still refuses the file because it does not represent an `i64`).
  */
bool isCanonicalDecimalString(const QJsonValue &value, qint64 *out)
{
  if (!value.isString())
    return false;
  const QString text = value.toString();
  if (text.isEmpty() || text != text.trimmed())
    return false;

  int digits = 0;
  if (text.at(0) == QLatin1Char('-')) {
    if (text.size() == 1)
      return false;
    digits = 1;
  } else if (text.at(0) == QLatin1Char('+')) {
    return false;
  }
  if (text.size() - digits > 1 && text.at(digits) == QLatin1Char('0'))
    return false;
  for (int i = digits; i < text.size(); ++i) {
    if (!text.at(i).isDigit())
      return false;
  }
  bool ok = false;
  const qint64 parsed = text.toLongLong(&ok);
  // toLongLong() rejects an overflow; the round trip rejects "-0" and any
  // spelling the canonical form does not use.
  if (!ok || QString::number(parsed) != text)
    return false;
  if (out != nullptr)
    *out = parsed;
  return true;
}

/** The member list of a JSON object, sorted, for exact set comparisons. */
QStringList sortedKeys(const QJsonObject &object)
{
  QStringList keys = object.keys();
  keys.sort();
  return keys;
}

/** ADR-006's six hardware configuration members, sorted (QJsonObject::keys() is sorted). */
const QStringList &hardwareConfigurationMembers()
{
  static const QStringList members({ QStringLiteral("baudRate"), QStringLiteral("dataBits"),
                                     QStringLiteral("endpoint"), QStringLiteral("flowControl"),
                                     QStringLiteral("parity"), QStringLiteral("stopBits") });
  return members;
}

/** One hardware section of the configuration snapshot: exactly ADR-006's six
  * members, every one a JSON string (SPEC-M10 5.3.1 row 14). */
QString hardwareSectionViolation(const QJsonValue &section, const QString &name)
{
  if (!section.isObject())
    return QStringLiteral("the source configuration %1 section is not a JSON object").arg(name);
  const QJsonObject object = section.toObject();
  for (const QString &member : hardwareConfigurationMembers()) {
    if (!object.contains(member))
      return QStringLiteral("the source configuration %1 section is missing %2").arg(name, member);
    if (!object.value(member).isString())
      return QStringLiteral("the source configuration %1 section's %2 is not a string")
          .arg(name, member);
  }
  if (sortedKeys(object) != hardwareConfigurationMembers()) {
    return QStringLiteral("the source configuration %1 section carries a member ADR-006's "
                          "schema does not allow")
        .arg(name);
  }
  return QString();
}

/** Validate the applied configuration snapshot against ADR-006's exhaustive
  * nested schema: the four sections, and inside them no missing member, no
  * additional member and no wrong type (SPEC-M10 5.3.1 row 14).
  */
QString configurationViolation(const QJsonObject &configuration)
{
  if (configuration.isEmpty())
    return QStringLiteral("the source descriptor has no configuration snapshot");

  const QStringList expected({ QStringLiteral("compatibility"), QStringLiteral("effective"),
                               QStringLiteral("localBuffering"), QStringLiteral("requested") });
  if (sortedKeys(configuration) != expected) {
    return QStringLiteral("the source configuration snapshot is not ADR-006's "
                          "{requested, effective, localBuffering, compatibility}");
  }
  for (const QString &section : { QStringLiteral("requested"), QStringLiteral("effective") }) {
    const QString violation = hardwareSectionViolation(configuration.value(section), section);
    if (!violation.isEmpty())
      return violation;
  }

  const QJsonValue bufferingValue = configuration.value(QStringLiteral("localBuffering"));
  if (!bufferingValue.isObject())
    return QStringLiteral("the source configuration localBuffering section is not a JSON object");
  const QJsonObject buffering = bufferingValue.toObject();
  const QStringList bufferingMembers({ QStringLiteral("flushRate"), QStringLiteral("rxQueue") });
  if (sortedKeys(buffering) != bufferingMembers) {
    return QStringLiteral("the source configuration localBuffering section is not exactly "
                          "rxQueue and flushRate");
  }
  // ADR-006 fixes these two members as JSON *numbers* and nothing more. The
  // writer profile's integral 32-bit rule is the writer's own (SPEC-M9) and must
  // not become a reader admission test: requiring it here would refuse a
  // v1-conformant file (review: slice-1 implementation review, finding 2;
  // ADR-011 D2).
  for (const QString &member : bufferingMembers) {
    if (!buffering.value(member).isDouble()) {
      return QStringLiteral("the source configuration buffering member %1 is not a JSON number")
          .arg(member);
    }
  }

  const QJsonValue compatibilityValue = configuration.value(QStringLiteral("compatibility"));
  if (!compatibilityValue.isObject())
    return QStringLiteral("the source configuration compatibility section is not a JSON object");
  const QJsonObject compatibility = compatibilityValue.toObject();
  if (sortedKeys(compatibility) != QStringList({ QStringLiteral("startBits") })
      || !compatibility.value(QStringLiteral("startBits")).isString()) {
    return QStringLiteral("the source configuration compatibility section is not exactly the "
                          "string startBits");
  }
  return QString();
}

/** ADR-006's optional wall-clock correlation, when the domain carries one: the
  * two members and their shapes are validated, the member itself is never a
  * reason to refuse, and it is not interpreted (SPEC-M10 5.3.1 row 17). */
QString wallClockCorrelationViolation(const QJsonValue &correlation, const QString &domainId)
{
  // Only a *missing* member is "absent" (ADR-011 D2, SPEC-M10 5.3.1 row 17): an
  // explicit JSON `null` is a present member and does not have the specified
  // object shape (review: slice-1 implementation review, finding 1).
  if (correlation.isUndefined())
    return QString();
  if (!correlation.isObject()) {
    return QStringLiteral("the clock domain '%1' has a wallClockCorrelation that is not a JSON "
                          "object")
        .arg(domainId);
  }
  const QJsonObject object = correlation.toObject();
  const QJsonValue wallClock = object.value(QStringLiteral("wallClock"));
  const QDateTime wallClockValue =
      wallClock.isString() ? QDateTime::fromString(wallClock.toString(), Qt::ISODate)
                           : QDateTime();
  if (!wallClockValue.isValid() || wallClockValue.timeSpec() != Qt::UTC) {
    return QStringLiteral("the clock domain '%1' has a wallClockCorrelation whose wallClock is "
                          "not an ISO-8601 UTC string")
        .arg(domainId);
  }
  qint64 unused = 0;
  if (!isCanonicalDecimalString(object.value(QStringLiteral("precisionNs")), &unused)) {
    return QStringLiteral("the clock domain '%1' has a wallClockCorrelation whose precisionNs is "
                          "not a canonical decimal string holding an i64")
        .arg(domainId);
  }
  return QString();
}

/** Validate the header and interpret it once (ADR-011 D2, SPEC-M10 5.3.1 rows
  * 10-17 and 5.3.3 row 29). Returns an empty string when the header is
  * accepted, otherwise the reason of the refusal.
  *
  * The order is the table's: the required members and their kinds first (a value
  * cannot be checked before it exists), then the required root values, then the
  * source count of M10's own rule, then the source descriptor, its
  * configuration, the clock domains and the resolved domain's reference.
  */
QString validateHeader(const QJsonObject &header, SessionFileInfo *info)
{
  // Row 11: the required root members, each of its required kind.
  const auto missing = [&header](const char *name) {
    return header.value(QString::fromLatin1(name)).isUndefined();
  };
  for (const char *member : { "format", "version", "created", "application", "sources",
                              "clockDomains" }) {
    if (missing(member))
      return QStringLiteral("the header is missing a required member (%1)").arg(member);
  }
  if (!header.value(QStringLiteral("format")).isString())
    return QStringLiteral("the header member format is not of its required kind");
  if (!header.value(QStringLiteral("created")).isString())
    return QStringLiteral("the header member created is not of its required kind");
  if (!header.value(QStringLiteral("version")).isDouble())
    return QStringLiteral("the header member version is not of its required kind");
  if (!header.value(QStringLiteral("application")).isObject())
    return QStringLiteral("the header member application is not of its required kind");
  if (!header.value(QStringLiteral("sources")).isArray())
    return QStringLiteral("the header member sources is not of its required kind");
  if (!header.value(QStringLiteral("clockDomains")).isArray())
    return QStringLiteral("the header member clockDomains is not of its required kind");

  // Row 10: the required root values.
  if (header.value(QStringLiteral("format")).toString() != QStringLiteral("komport-session"))
    return QStringLiteral("the header's format is not 'komport-session'");
  const QJsonValue versionValue = header.value(QStringLiteral("version"));
  if (!versionValue.isDouble() || versionValue.toDouble() != 1.0)
    return QStringLiteral("the header's version is not the number 1");
  const QString createdText = header.value(QStringLiteral("created")).toString();
  const QDateTime created = QDateTime::fromString(createdText, Qt::ISODate);
  if (!created.isValid() || created.timeSpec() != Qt::UTC)
    return QStringLiteral("the header's created is not an ISO-8601 UTC timestamp");
  const QJsonObject application = header.value(QStringLiteral("application")).toObject();
  if (!application.value(QStringLiteral("name")).isString()
      || !application.value(QStringLiteral("version")).isString()) {
    return QStringLiteral("the header's application is not the {name, version} string pair");
  }

  // Row 29 (SPEC-M10 5.4): the one restriction M10 adds - exactly one source.
  const QJsonArray sources = header.value(QStringLiteral("sources")).toArray();
  if (sources.size() > 1)
    return SessionReader::multiSourceRefusal();
  if (sources.isEmpty())
    return QStringLiteral("the file declares no source");

  // The clock domain array is needed by rows 13, 15, 16 and 17.
  const QJsonArray domains = header.value(QStringLiteral("clockDomains")).toArray();

  // Row 13: the descriptor's fields, and its domain reference. ADR-006's *fixed
  // local profile values* are not an admission test: any in-range sourceId, any
  // domain id and either kind ADR-006 defines are read (ADR-011 D2).
  if (!sources.at(0).isObject())
    return QStringLiteral("the source descriptor is not a JSON object");
  const QJsonObject source = sources.at(0).toObject();
  qint64 sourceId = 0;
  if (!integralJsonNumber(source.value(QStringLiteral("sourceId")), 1.0, 4294967295.0, &sourceId)) {
    return QStringLiteral("the source descriptor's sourceId is not an integral JSON number "
                          "in 1..4294967295");
  }
  if (!source.value(QStringLiteral("clockDomainId")).isString())
    return QStringLiteral("the source descriptor's clockDomainId is not a string");
  const QString clockDomainId = source.value(QStringLiteral("clockDomainId")).toString();
  if (!source.value(QStringLiteral("name")).isString())
    return QStringLiteral("the source descriptor's name is not a string");
  if (!source.value(QStringLiteral("transport")).isString())
    return QStringLiteral("the source descriptor's transport is not a string");
  // ADR-006: "A source without a clockDomainId is not v1-conformant" - the
  // reference must resolve to an entry of the file's own array. This half of row
  // 13 is decided with the descriptor, before rows 14-17 (SPEC-M10 5.3's
  // ordering: the source descriptor is what the later rows are read against).
  const auto domainHasId = [&domains](const QString &wanted) {
    for (const QJsonValue &entry : domains) {
      const QJsonObject object = entry.toObject();
      if (object.value(QStringLiteral("id")).isString()
          && object.value(QStringLiteral("id")).toString() == wanted) {
        return true;
      }
    }
    return false;
  };
  if (!domainHasId(clockDomainId))
    return QStringLiteral("the source descriptor's clockDomainId does not resolve to a "
                          "clockDomains[] entry");

  // Row 14: the applied configuration snapshot.
  const QJsonValue configurationValue = source.value(QStringLiteral("configuration"));
  if (!configurationValue.isObject()) {
    return QStringLiteral("the source descriptor's configuration is not a JSON object");
  }
  const QJsonObject configuration = configurationValue.toObject();
  const QString configurationReason = configurationViolation(configuration);
  if (!configurationReason.isEmpty())
    return configurationReason;

  // Row 15: every clock domain entry - object, string id, unique id, defined
  // kind - checked for the whole array before row 16 begins.
  QStringList seenIds;
  for (const QJsonValue &entryValue : domains) {
    if (!entryValue.isObject())
      return QStringLiteral("a clock domain entry is not a JSON object");
    const QJsonObject entry = entryValue.toObject();
    if (!entry.value(QStringLiteral("id")).isString())
      return QStringLiteral("a clock domain entry has no string id");
    const QString domainId = entry.value(QStringLiteral("id")).toString();
    if (seenIds.contains(domainId))
      return QStringLiteral("two clock domains share the id '%1'").arg(domainId);
    seenIds.append(domainId);
    const QJsonValue kindValue = entry.value(QStringLiteral("kind"));
    if (!kindValue.isString() || (kindValue.toString() != QStringLiteral("process-monotonic")
                                  && kindValue.toString() != QStringLiteral("agent-monotonic"))) {
      return QStringLiteral("the clock domain '%1' has a kind ADR-006 does not define")
          .arg(domainId);
    }
  }

  // Row 16: the reference pair of every entry (ADR-006: a clock domain without
  // identifier and reference pair is not v1-conformant). The facts of the domain
  // the source resolves to are taken here, from the already validated pair.
  for (const QJsonValue &entryValue : domains) {
    const QJsonObject entry = entryValue.toObject();
    const QString domainId = entry.value(QStringLiteral("id")).toString();
    if (!entry.value(QStringLiteral("reference")).isObject())
      return QStringLiteral("the clock domain '%1' has no reference object").arg(domainId);
    const QJsonObject reference = entry.value(QStringLiteral("reference")).toObject();
    qint64 referenceSourceNs = 0;
    qint64 referenceSessionNs = 0;
    if (!isCanonicalDecimalString(reference.value(QStringLiteral("sourceTimestampNs")),
                                  &referenceSourceNs)
        || !isCanonicalDecimalString(reference.value(QStringLiteral("sessionTimestampNs")),
                                     &referenceSessionNs)) {
      return QStringLiteral("the clock domain '%1' has a reference pair that is not made of "
                            "canonical decimal strings holding an i64")
          .arg(domainId);
    }
    // ADR-006: the anchor sits at session time 0, and its canonical spelling is
    // exactly "0" - which the canonical rule above already enforces for a zero.
    if (referenceSessionNs != 0) {
      return QStringLiteral("the clock domain '%1' does not reference session time exactly \"0\"")
          .arg(domainId);
    }
    if (domainId == clockDomainId) {
      info->clockDomain.id = domainId;
      info->clockDomain.kind = entry.value(QStringLiteral("kind")).toString();
      info->clockDomain.referenceSourceTimestampNs = referenceSourceNs;
      info->clockDomain.referenceSessionTimestampNs = referenceSessionNs;
    }
  }

  // Row 17: ADR-006's optional correlation, validated when present.
  for (const QJsonValue &entryValue : domains) {
    const QJsonObject entry = entryValue.toObject();
    const QString correlationReason = wallClockCorrelationViolation(
        entry.value(QStringLiteral("wallClockCorrelation")),
        entry.value(QStringLiteral("id")).toString());
    if (!correlationReason.isEmpty())
      return correlationReason;
  }

  info->version = SessionReader::kFormatVersion;
  info->createdUtc = created;
  info->applicationName = application.value(QStringLiteral("name")).toString();
  info->applicationVersion = application.value(QStringLiteral("version")).toString();
  info->source.sourceId = static_cast<quint32>(sourceId);
  info->source.clockDomainId = clockDomainId;
  info->source.name = source.value(QStringLiteral("name")).toString();
  info->source.transport = source.value(QStringLiteral("transport")).toString();
  info->source.configuration = configuration;
  return QString();
}

/** The refusal reason of a record's own fields, or an empty string (SPEC-M10
  * 5.3.2 rows 25-28). The record's structure - lengths, flags, metadata kind -
  * has already been checked by the caller, which owns those bytes. */
QString recordViolation(const SessionEvent &event, qsizetype recordIndex, quint32 fileSourceId,
                        quint64 previousSequence, qint64 previousSessionTimeNs)
{
  const QString index = QString::number(recordIndex);

  // Row 25: ADR-002's event-local contract, applied by ADR-011 D3.
  QString invalidReason;
  if (!isValidSessionEvent(event, &invalidReason)) {
    return QStringLiteral("record %1 is not a valid SessionEvent: %2").arg(index, invalidReason);
  }
  // Row 26: the record resolves against the file's single source.
  if (event.sourceId != fileSourceId)
    return QStringLiteral("record %1's source does not resolve to the session's source").arg(index);
  // Row 27: the sequence continues the stream (it may start above 1 - a
  // recording can begin mid-session, ADR-010 4).
  if (event.sequence == 0 || event.sequence <= previousSequence)
    return QStringLiteral("record %1's sequence does not continue the stream").arg(index);
  // Row 28: session time never decreases within the resolved domain.
  if (event.timestampNs < previousSessionTimeNs)
    return QStringLiteral("record %1's session time decreases").arg(index);
  return QString();
}

/** The load itself, on an already opened source (SPEC-M10 5.3). */
SessionLoadOutcome loadFromSource(SessionByteSource *source, const QString &path);

} // namespace

SessionByteSource::~SessionByteSource() = default;
FileByteSource::~FileByteSource() = default;

QByteArray SessionReader::magic()
{
  // ADR-006: "KPSN" followed by 0x1A, CR, LF, NUL. A fixed byte array because
  // the magic contains a NUL and must keep its exact length.
  static const char kMagic[8] = { 'K', 'P', 'S', 'N', 0x1a, '\r', '\n', '\0' };
  return QByteArray(kMagic, 8);
}

QString SessionReader::multiSourceRefusal()
{
  return QStringLiteral("multi-source sessions not supported in M10");
}

SessionReader::SessionReader() : mSource(nullptr) {}

SessionReader::SessionReader(SessionByteSource *source) : mSource(source) {}

SessionLoadOutcome SessionReader::load(const QString &path) const
{
  SessionLoadOutcome outcome;
  outcome.path = path;

  std::unique_ptr<SessionByteSource> owned;
  SessionByteSource *source = mSource;
  if (source == nullptr) {
    owned = std::unique_ptr<SessionByteSource>(new FileByteSource());
    source = owned.get();
  }

  // Row 1: the loader's own I/O rule, not a format rule.
  if (!source->open(path)) {
    outcome.reason = QStringLiteral("the file could not be opened");
    return outcome;
  }
  const SourceCloser closer(source);
  return loadFromSource(source, path);
}

namespace {

SessionLoadOutcome loadFromSource(SessionByteSource *source, const QString &path)
{
  SessionLoadOutcome outcome;
  outcome.path = path;
  qint64 consumed = 0;   // bytes the loader has taken from the source

  const auto refuse = [&outcome, &consumed](const QString &reason) {
    outcome.ok = false;
    outcome.reason = reason;
    outcome.session.reset();
    outcome.eventCount = 0;
    outcome.recoveredTruncatedFinalRecord = false;
    outcome.bytesRead = consumed;
    return outcome;
  };

  // --- The start block: magic, version, encoding, the declared header length --
  QByteArray startBlock;
  const ReadOutcome startRead = readUpTo(source, 16, startBlock);
  consumed += startRead.bytes;
  if (startRead.error)
    return refuse(QStringLiteral("the file could not be read"));
  // Row 3: fewer than 16 bytes in total.
  if (!startRead.complete)
    return refuse(QStringLiteral("shorter than a start block"));
  // Row 4.
  if (startBlock.left(8) != SessionReader::magic())
    return refuse(QStringLiteral("bad magic"));
  // Row 5.
  const quint16 version = qFromLittleEndian<quint16>(startBlock.constData() + 8);
  if (version != SessionReader::kFormatVersion)
    return refuse(QStringLiteral("unsupported format version %1").arg(version));
  // Row 6.
  const quint16 encoding = qFromLittleEndian<quint16>(startBlock.constData() + 10);
  if (encoding != SessionReader::kHeaderEncodingUtf8Json)
    return refuse(QStringLiteral("unsupported header encoding %1").arg(encoding));

  // Row 7: the declared length is checked before the header is read or sized.
  const quint32 declaredHeaderLength = qFromLittleEndian<quint32>(startBlock.constData() + 12);
  const qint64 headerLength = qint64(declaredHeaderLength);
  if (headerLength > SessionReader::kMaxHeaderBytes)
    return refuse(QStringLiteral("the header exceeds ADR-006's 16 MiB limit"));
  // Row 8: 16 + headerLength against the bytes the source promises.
  const qint64 sourceSize = source->size();
  if (sourceSize >= 0 && sourceSize - 16 < headerLength)
    return refuse(QStringLiteral("the header length exceeds the file"));

  QByteArray headerBytes;
  const ReadOutcome headerRead = readUpTo(source, headerLength, headerBytes);
  consumed += headerRead.bytes;
  if (headerRead.error)
    return refuse(QStringLiteral("the file could not be read"));
  if (!headerRead.complete)
    return refuse(QStringLiteral("the header length exceeds the file"));

  // Row 9.
  const QJsonDocument headerDocument = QJsonDocument::fromJson(headerBytes);
  if (!headerDocument.isObject())
    return refuse(QStringLiteral("the header is not a JSON object"));

  // Rows 10-17 and 29: the header is interpreted once, here (ADR-011 D2).
  SessionFileInfo info;
  const QString headerReason = validateHeader(headerDocument.object(), &info);
  if (!headerReason.isEmpty())
    return refuse(headerReason);

  // --- The records, one at a time --------------------------------------------
  QList<SessionEvent> events;
  bool recovered = false;
  quint64 previousSequence = 0;
  qint64 previousSessionTimeNs = 0;

  for (;;) {
    QByteArray lengthBytes;
    const ReadOutcome lengthRead = readUpTo(source, 4, lengthBytes);
    consumed += lengthRead.bytes;
    if (lengthRead.error)
      return refuse(QStringLiteral("the file could not be read"));
    // A clean end: the file ends exactly at a record boundary.
    if (lengthRead.bytes == 0)
      break;
    // Row 20: fewer than four bytes remain, so no length can be read at all -
    // the one recoverable case in which nothing can be validated.
    if (!lengthRead.complete) {
      recovered = true;
      break;
    }

    // Rows 18 and 19: decided from the declared length alone, before the
    // record's own fields are parsed and before the truncation rule can absorb
    // a malformed or impossible length (review rounds 2 and 3).
    const quint32 declaredRecordLength = qFromLittleEndian<quint32>(lengthBytes.constData());
    const qint64 recordLength = qint64(declaredRecordLength);
    const QString recordIndex = QString::number(events.size());
    if (recordLength < kRecordTailBytes)
      return refuse(QStringLiteral("record %1 declares a length below its own prefix").arg(recordIndex));
    if (recordLength > SessionReader::kMaxRecordBodyBytes)
      return refuse(QStringLiteral("record %1 exceeds ADR-006's 64 MiB limit").arg(recordIndex));

    QByteArray bodyBytes;
    const ReadOutcome bodyRead = readUpTo(source, recordLength, bodyBytes);
    consumed += bodyRead.bytes;
    if (bodyRead.error)
      return refuse(QStringLiteral("the file could not be read"));
    // Row 21: a length that passed rows 18 and 19 and still runs past the end of
    // the file - the record was cut short mid-write.
    if (!bodyRead.complete) {
      recovered = true;
      break;
    }

    const char *prefix = bodyBytes.constData();
    const quint16 eventTypeValue = qFromLittleEndian<quint16>(prefix);
    const quint8 directionValue = quint8(prefix[2]);
    const quint8 flags = quint8(prefix[3]);
    const quint64 sequence = qFromLittleEndian<quint64>(prefix + 4);
    const quint32 sourceId = qFromLittleEndian<quint32>(prefix + 12);
    const qint64 sourceTimestampNs = qFromLittleEndian<qint64>(prefix + 16);
    const qint64 timestampNs = qFromLittleEndian<qint64>(prefix + 24);
    const quint32 metadataLength = qFromLittleEndian<quint32>(prefix + 32);
    const quint32 payloadLength = qFromLittleEndian<quint32>(prefix + 36);

    // Row 22: the three lengths agree. Subtractive checks, so no sum of two
    // unvalidated quantities can wrap (SPEC-M10 5.3.1's arithmetic note).
    const qint64 contentLength = recordLength - kRecordTailBytes;
    if (qint64(metadataLength) > contentLength)
      return refuse(QStringLiteral("record %1 has inconsistent lengths").arg(recordIndex));
    if (qint64(payloadLength) > contentLength - qint64(metadataLength))
      return refuse(QStringLiteral("record %1 has inconsistent lengths").arg(recordIndex));
    if (qint64(metadataLength) + qint64(payloadLength) != contentLength)
      return refuse(QStringLiteral("record %1 has inconsistent lengths").arg(recordIndex));

    // Row 23.
    if (flags != 0)
      return refuse(QStringLiteral("record %1 has non-zero flags").arg(recordIndex));

    // Row 24: an empty metadata section is ADR-006's encoding of an empty
    // metadata object; a non-empty one must be a JSON object.
    QJsonObject metadata;
    if (metadataLength > 0) {
      const QByteArray metadataJson =
          bodyBytes.mid(kRecordTailBytes, static_cast<int>(metadataLength));
      const QJsonDocument metadataDocument = QJsonDocument::fromJson(metadataJson);
      if (!metadataDocument.isObject())
        return refuse(QStringLiteral("record %1 has non-object metadata").arg(recordIndex));
      metadata = metadataDocument.object();
    }

    SessionEvent event;
    event.sequence = sequence;
    event.sourceId = sourceId;
    event.sourceTimestampNs = sourceTimestampNs;
    event.timestampNs = timestampNs;
    event.type = static_cast<SessionEventType>(eventTypeValue);
    event.direction = static_cast<SessionDirection>(directionValue);
    event.metadata = metadata;
    event.payload = bodyBytes.mid(kRecordTailBytes + static_cast<int>(metadataLength),
                                 static_cast<int>(payloadLength));

    // Rows 25-28.
    const QString violation = recordViolation(event, events.size(), info.source.sourceId,
                                              previousSequence, previousSessionTimeNs);
    if (!violation.isEmpty())
      return refuse(violation);

    events.append(event);
    previousSequence = event.sequence;
    previousSessionTimeNs = event.timestampNs;
  }

  // One immutable offline session: typed header facts plus the stored events,
  // verbatim and in stored order (ADR-011 D4/D5). The session is built once and
  // only ever handed out as the shared const view of the contract.
  auto built = std::make_shared<SessionFile>();
  built->info = info;
  built->events = events;
  const SessionFilePtr session = built;

  outcome.ok = true;
  outcome.reason.clear();
  outcome.session = session;
  outcome.recoveredTruncatedFinalRecord = recovered;
  outcome.eventCount = static_cast<quint64>(events.size());
  outcome.bytesRead = consumed;
  return outcome;
}

} // namespace
