/***************************************************************************
                        sessionreader.h  -  Komport Serial Port Communicator
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

#ifndef SESSIONREADER_H
#define SESSIONREADER_H

// ADR-011 (accepted 2026-09-19), SPEC-M10 sections 5.3, 5.4 and 5.10: the
// production reader of the `.kpsession` v1 container.
//
// ADR-006 is the *sole normative source* of the format - its start block, its
// magic and numbers, its header content, its limits and its reader rules. This
// unit applies those rules and adds exactly one restriction of its own, the
// source count of SPEC-M10 5.4; a change to a format rule is a change to
// ADR-006, never to this file (ADR-011 D2).
//
// The reader is its own implementation: it calls no writer code and does not
// include M9's test-side parser (`tests/sessionrecordreader.h`). The format
// facts it shares with the writer's codec are its own constants, and
// `tst_sessionreader`'s `readerLimitsMatchTheWriterCodec` pins them equal.
//
// It is read-only and stateless between loads: no file handle survives a call,
// nothing is cached, and loading never writes. No transport, no session
// controller and no widget type appears here (ADR-009); the compile proof is
// tests/session_contract_compile.cpp, which builds this unit against Qt6::Core
// alone.
//
// QtCore only.

#include "sessionevent.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <memory>

/** The typed header facts of a loaded file (ADR-011 D2). */
struct SessionSourceDescriptor {
  quint32 sourceId = 0;         ///< non-zero, unique in the file
  QString clockDomainId;        ///< resolves to SessionFileInfo::clockDomain
  QString name;                 ///< e.g. "local serial"
  QString transport;            ///< e.g. "serial"
  QJsonObject configuration;    ///< the applied snapshot, verbatim (ADR-006 schema)
};

/** The single clock domain the loaded session's source resolves to. */
struct SessionClockDomain {
  QString id;
  QString kind;                 ///< ADR-006's kind of the resolved domain
                                ///< ("process-monotonic" local, "agent-monotonic"
                                ///< future remote); read as a fact, never an
                                ///< admission test
  qint64 referenceSourceTimestampNs = 0;   ///< the anchor event's raw source time
  qint64 referenceSessionTimestampNs = 0;  ///< exactly 0 in v1
};

/** The header of a loaded file, interpreted once (ADR-011 D2). */
struct SessionFileInfo {
  int version = 0;              ///< 1
  QDateTime createdUtc;         ///< ISO-8601 UTC, informational
  QString applicationName;      ///< informational
  QString applicationVersion;   ///< informational
  SessionSourceDescriptor source;
  SessionClockDomain clockDomain;
};

/** The one offline session of a successful load (ADR-011 D5: immutable, verbatim). */
struct SessionFile {
  SessionFileInfo info;
  QList<SessionEvent> events;   ///< stored order, stored values, never modified
};
using SessionFilePtr = std::shared_ptr<const SessionFile>;

/** Internal seam: the read-only file access the reader needs (SPEC-M10 5.10).
  *
  * Not part of the reader's public surface: production code uses the default
  * (a `QFile` opened read-only, implemented in the reader's unit) and a test
  * injects a scripted source through `SessionReaderSeamsForTest`, the same
  * friend-for-testability pattern M9 uses for `SessionRecorderSeamsForTest`.
  * An injected source is not owned by the reader and must outlive the call.
  *
  * The four methods are exactly the ones SPEC-M10 5.10 fixes. `open()` reports
  * success only, which is why the loader's refusal for a failed open is the
  * loader rule's own text (SPEC-M10 5.3.1 row 1) rather than an
  * operating-system message.
  */
class SessionByteSource
{
public:
  virtual ~SessionByteSource();

  /** Open @p path read-only; false when it cannot be opened or is not a regular file. */
  virtual bool open(const QString &path) = 0;
  /** The bytes the source promises, or -1 when that is not known. */
  virtual qint64 size() const = 0;
  /** Append at most @p maxBytes to @p out; the bytes appended, 0 at the end of the
    * source, or a negative value on an I/O error. */
  virtual qint64 read(QByteArray &out, qint64 maxBytes) = 0;
  /** Close the source; safe to call when it is not open. */
  virtual void close() = 0;
};

/** Loading outcome (ADR-011 D4: all-or-nothing).
  *
  * A refusal leaves `session` null, `eventCount` zero and nothing rendered;
  * `reason` names what was refused and why. `bytesRead` reports every byte the
  * loader consumed from the source - which includes the discarded truncated
  * suffix of a recovered load, so it is a measure of what was read, not of the
  * size of the delivered session. The recovered outcome is the one partial result
  * ADR-006 allows: the complete prefix is the session and the truncated final
  * record is reported instead of being delivered.
  */
struct SessionLoadOutcome {
  bool ok = false;
  QString path;
  QString reason;                        ///< why the file was refused, empty when ok
  SessionFilePtr session;                ///< null unless ok
  bool recoveredTruncatedFinalRecord = false; ///< ADR-006's recovery rule applied
  quint64 eventCount = 0;
  qint64 bytesRead = 0;
};

/** Reads `.kpsession` v1 files. ADR-006 is the sole normative source of the format, its
  * limits, its reader rules and its required header content; this reader applies them
  * (ADR-011 D1/D2/D3/D10) and adds exactly one restriction of its own (the single-source
  * rule of SPEC-M10 5.4). QtCore only. */
class SessionReader
{
public:
  /** The production reader: each load opens the file itself, read-only. */
  SessionReader();

  /** ADR-006's binary format version. */
  static constexpr int kFormatVersion = 1;
  /** ADR-006's header encoding id (UTF-8 JSON). */
  static constexpr int kHeaderEncodingUtf8Json = 1;
  /** ADR-006's fixed record prefix: the lengths plus the 40 bytes they cover. */
  static constexpr qsizetype kRecordPrefixSize = 44;
  /** ADR-006's header limit. */
  static constexpr qsizetype kMaxHeaderBytes = 16 * 1024 * 1024;
  /** ADR-006's record-body limit: the declared `recordLength` is this body. */
  static constexpr qsizetype kMaxRecordBodyBytes = 64 * 1024 * 1024;
  /** The 8-byte magic "KPSN" 0x1A CR LF NUL. */
  static QByteArray magic();
  /** The verbatim refusal of ADR-011 D2 for a file with more than one source - the only
    * loader restriction M10 adds to ADR-006. */
  static QString multiSourceRefusal();

  /** Load @p path into one offline session. Read-only; no state is kept between calls. */
  SessionLoadOutcome load(const QString &path) const;

private:
  /** Injection constructor for the SPEC-M10 5.10 byte-source seam; reachable only through
    * `SessionReaderSeamsForTest`, because the seam is not public API. The injected source
    * is not owned and must outlive the call. */
  explicit SessionReader(SessionByteSource *source);
  friend struct SessionReaderSeamsForTest;

  SessionByteSource *mSource;   ///< the injected seam, or null for the production source
};

#endif // SESSIONREADER_H
