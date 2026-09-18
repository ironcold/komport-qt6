/***************************************************************************
                       sessionrecordcodec.h  -  Komport Serial Port Communicator
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

#ifndef SESSIONRECORDCODEC_H
#define SESSIONRECORDCODEC_H

// ADR-006 (accepted 2026-09-17; normative v1 writer profile added 2026-09-18),
// ADR-010 (live session recording, decisions 2, 3 and 6) and SPEC-M9 section
// 5.3/5.4: the byte-level assembly of a `.kpsession` v1 file - the start block
// and one record per accepted event - and nothing else. No file I/O lives here;
// the recorder writes what these functions return.
//
// The codec *enforces* the format it writes: the fixed local profile values are
// its own constants rather than caller-supplied facts, the session reference is
// always the v1 value, the configuration snapshot is checked against the
// exhaustive nested schema, and every size limit of ADR-006 is applied before a
// byte is allocated for the JSON. A caller cannot produce a header outside the
// accepted profile, and a refusal never returns a partial unit.
//
// This unit is part of M9's QtCore-only surface (ADR-009, ADR-010 section 7):
// no QWidget, no KomportApp and no other executable-specific type may enter it.
//
// The layout is fixed by ADR-006 and is little-endian throughout:
//
//   magic "KPSN" 0x1A CR LF NUL   (8 bytes)
//   u16 format version            (1)
//   u16 header encoding           (1 = UTF-8 JSON)
//   u32 header byte length
//   bytes UTF-8 JSON header
//
//   u32 recordLength              (bytes following this field)
//   u16 eventType
//   u8  direction
//   u8  flags                     (zero in v1)
//   u64 sequence
//   u32 sourceId
//   i64 sourceTimestampNs
//   i64 timestampNs
//   u32 metadataLength
//   u32 payloadLength
//   bytes metadata JSON           (absent when the metadata object is empty)
//   bytes payload

#include "sessionevent.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

/** Result of an assembly attempt.
  *
  * A refusal is never a partial buffer: either `bytes` holds one complete
  * header block or one complete record, or nothing was produced and `reason`
  * names why (SPEC-M9 section 5.3: an offending event is not written, and the
  * recorder reports it and stops as damaged).
  */
struct SessionRecordEncoding {
  QByteArray bytes;   ///< the complete header block or record, empty on refusal
  bool ok = false;    ///< true if `bytes` is a complete unit
  QString reason;     ///< human-readable reason on refusal
};

/** The facts the v1 header is built from (SPEC-M9 section 5.4).
  *
  * Deliberately small: everything ADR-006's v1 writer profile fixes is a
  * constant of the codec, not a caller choice, so the local profile cannot be
  * varied into a different dialect. What remains is what only the session
  * knows - who writes, when, what configuration was applied, and the anchor's
  * raw source time (ADR-010 decisions D7 and D8).
  */
struct SessionHeaderFacts {
  QString applicationName;                 ///< e.g. "Komport"
  QString applicationVersion;              ///< the writing application's version
  QDateTime createdUtc;                    ///< wall-clock creation time, written as ISO-8601 UTC
  QJsonObject configuration;               ///< the applied snapshot; must satisfy the ADR-006 schema
  qint64 referenceSourceTimestampNs = 0;   ///< the anchor event's raw source time
};

/** Byte-level assembly of the `.kpsession` v1 container (ADR-006, SPEC-M9 5.3/5.4). */
class SessionRecordCodec
{
public:
  // --- The fixed local profile of ADR-006's v1 writer profile ----------------
  static constexpr quint32 kSourceId = 1;                 ///< non-zero, unique in the file
  static constexpr qint64 kReferenceSessionTimestampNs = 0; ///< the anchor sits at session time 0 (ADR-005)
  /** The clock-domain id, used for both the source descriptor and the domain entry. */
  static QString clockDomainId();
  /** The source descriptor's human-readable name. */
  static QString sourceName();
  /** The transport name of the local serial source. */
  static QString transport();
  /** The clock domain's kind (ADR-006: "process-monotonic" for a local monotonic clock). */
  static QString clockDomainKind();

  /** Size of the fixed record prefix (ADR-006: 44 bytes). */
  static constexpr qsizetype kRecordPrefixSize = 44;
  /** Bytes of a record prefix that follow the leading `recordLength` field. */
  static constexpr qsizetype kRecordBodyPrefixSize = kRecordPrefixSize - 4;
  /** Reader limit ADR-006 defines for the header; the writer applies it too. */
  static constexpr qsizetype kMaxHeaderBytes = 16 * 1024 * 1024;
  /** Reader limit ADR-006 defines for a record body (prefix tail plus content). */
  static constexpr qsizetype kMaxRecordBodyBytes = 64 * 1024 * 1024;

  /** The 8-byte file magic of ADR-006: "KPSN", 0x1A, CR, LF, NUL. */
  static QByteArray magic();

  /** Checked arithmetic for the record sizing rules of SPEC-M9 section 5.3.
    *
    * Public on purpose: this is the format's size predicate, and the recorder
    * uses it to report the offending sizes of an event it refuses to write
    * (SPEC-M9 section 5.3 "the report names its sequence number and sizes"). It
    * is pure and allocation-free, so the limits are testable without building a
    * 64 MiB buffer.
    *
    * @param metadataBytes length of the metadata JSON (0 for an empty object)
    * @param payloadBytes length of the payload
    * @param reason optional; receives the reason on refusal
    * @return true if a record with these sizes is representable and allowed
    */
  static bool recordSizesFit(qint64 metadataBytes, qint64 payloadBytes, QString *reason = nullptr);

  /** Exact compact-JSON size of a value, computed without serialising it.
    *
    * Public for the same reason as recordSizesFit(): the recorder reports the
    * sizes of the unit it refused to write (SPEC-M9 section 5.3). It is exact
    * for every value shape - strings with their real escapes and UTF-8 lengths,
    * commas only between members and elements, and numbers measured through the
    * JSON writer itself - and allocates a few bytes per number rather than the
    * size of the unit. The test `jsonSizeCounterIsExactForEveryValueShape()` pins
    * it against the writer, because a refusal decision must never drop a unit
    * that fits.
    */
  static qint64 compactJsonSize(const QJsonValue &value);

  /** Build the start block: magic, version, header encoding, the u32 header
    * length and the UTF-8 JSON header of ADR-006's v1 writer profile.
    *
    * The configuration snapshot is validated against the exhaustive nested
    * schema first, and the JSON size is bounded before it is serialised, so an
    * oversized or non-conformant header is refused without a large allocation.
    *
    * @param facts the header facts
    * @return the complete block, or a refusal naming the reason
    */
  static SessionRecordEncoding encodeHeader(const SessionHeaderFacts &facts);

  /** Build one complete record for an accepted event.
    *
    * The event is validated structurally first (isValidSessionEvent), because a
    * malformed event is a producer defect and must never reach the file. The
    * metadata size is bounded before the metadata is serialised, and the record
    * sizes are checked with checked arithmetic before anything is allocated.
    *
    * @param event the event to persist
    * @return the complete record, or a refusal naming the reason
    */
  static SessionRecordEncoding encodeRecord(const SessionEvent &event);
};

#endif // SESSIONRECORDCODEC_H
