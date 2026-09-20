/***************************************************************************
                    session_contract_compile.cpp  -  Komport Serial Port Communicator
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

// M8: compile/object proof that the public session and transport contracts need
// Qt Core only. This translation unit is built into the object target
// `komport_session_contract_check`, which links Qt6::Core alone - no
// komport_core, no Qt6::Widgets, no Qt6::Test. That is the machine-checkable
// form of SPEC-M8 section 14's "no widget dependency" criterion and of
// ADR-009's application-neutral contract rule.

#include "itransport.h"
#include "sessioncontroller.h"
#include "sessionevent.h"
#include "sessionreader.h"
#include "sessionreplayplayer.h"
#include "transportconfiguration.h"

#include <QJsonObject>
#include <QStringList>

#include <type_traits>

// SPEC-M8 section 14: the public session/transport contracts must have no widget
// or executable-specific dependency. The controller is part of that surface, so
// its header - and, in the object target, its implementation unit - is compiled
// here against Qt6::Core alone. The M10 reader joined that surface the same way
// (ADR-009): sessionreader.h and its value types are built into this target.
static_assert(sizeof(SessionController) > 0,
              "sessioncontroller.h must stay compilable against Qt6::Core (ADR-009)");
static_assert(sizeof(SessionReader) > 0,
              "sessionreader.h must stay compilable against Qt6::Core (ADR-009)");
static_assert(sizeof(SessionReplayPlayer) > 0,
              "sessionreplayplayer.h must stay compilable against Qt6::Core (ADR-009)");

namespace {

/** Minimal ITransport implementation; proves that the frozen interface can be
  * implemented from Qt Core alone. */
class ContractProbeTransport : public ITransport
{
public:
  using ITransport::ITransport;

  bool open() override { mOpen = true; return true; }
  void close() override { mOpen = false; }
  bool isOpen() const override { return mOpen; }
  qint64 writeBytes(const QByteArray &bytes) override { return mOpen ? bytes.size() : -1; }

  /** Touch every signal's signature so a change to the frozen surface would
    * break this target's compilation. */
  void exerciseSignals()
  {
    const QJsonObject metadata;
    const QByteArray bytes;
    emit bytesReceived(1, bytes, 0);
    emit bytesWritten(1, bytes, 0);
    emit opened(1, 0, metadata);
    emit closed(1, 0, metadata);
    emit configurationChanged(1, 0, metadata);
    emit lineStateChanged(1, 0, metadata);
    emit transportError(1, 0, metadata);
  }

private:
  bool mOpen = false;
};

} // namespace

/** Exercises every public session/transport value contract. Returns 0 when all
  * checks hold, 1 otherwise; the target is not executed, it only has to compile
  * and link against Qt Core. */
int komportSessionContractCompileProof()
{
  SessionEvent event;
  event.sequence = 1;
  event.sourceId = 1;
  event.type = SessionEventType::Data;
  event.direction = SessionDirection::Rx;
  event.payload = QByteArrayLiteral("x");
  const bool structurallyValid = isValidSessionEvent(event);

  const QJsonValue encoded = sessionJsonInteger(Q_INT64_C(1) << 40);
  qint64 decoded = 0;
  const bool losslessRoundTrip =
      sessionJsonIntegerToQInt64(encoded, &decoded) && decoded == (Q_INT64_C(1) << 40);

  TransportConfiguration requested;
  requested.endpoint = QStringLiteral("/dev/ttyS0");
  requested.startBits = QStringLiteral("1");
  const TransportConfiguration baseline;
  const QStringList changedGroups = requested.changedGroupsComparedTo(baseline);

  ConfigurationResult result;
  result.requested = requested;
  result.changedGroups = changedGroups;
  const QJsonObject metadata = result.toMetadata();

  ContractProbeTransport transport;
  transport.exerciseSignals();
  const bool opened = transport.open();
  const qint64 accepted = transport.writeBytes(QByteArrayLiteral("abc"));

  // M10: the reader's own surface and the value types it hands out compile and
  // link against Qt Core alone. The target is never executed, so nothing here
  // touches a file: only the contract's shape is exercised.
  const bool readerSurface =
      SessionReader::kRecordPrefixSize == 44
      && SessionReader::kMaxRecordBodyBytes == 64 * 1024 * 1024
      && SessionReader::kMaxHeaderBytes == 16 * 1024 * 1024
      && SessionReader::magic().size() == 8
      && !SessionReader::multiSourceRefusal().isEmpty();
  SessionLoadOutcome loadOutcome;
  loadOutcome.eventCount = 0;
  loadOutcome.bytesRead = 0;
  const SessionFilePtr session = loadOutcome.session;
  SessionFileInfo fileInfo;
  fileInfo.version = SessionReader::kFormatVersion;
  fileInfo.source.sourceId = 1;

  // The player's own surface: its enumerators, its result values and its
  // operations compile against Qt Core alone.
  const SessionReplayPlayer player;
  const SessionReplayState state = player.state();
  const SessionReplayTiming timing = player.timing();
  SessionReplayStart startResult;
  SessionReplayStep stepResult;
  SessionReplayTimingChange timingResult;
  SessionReplayReport report;
  startResult.reason = QStringLiteral("x");
  stepResult.advanced = 0;
  stepResult.matched = false;
  stepResult.reachedEnd = true;
  timingResult.timing = timing;
  report.finished = false;
  const bool playerSurface =
      state == SessionReplayState::Idle
      && timing == SessionReplayTiming::Original
      && player.position() == 0
      && player.deliveredCount() == 0
      && player.eventCount() == 0
      && !startResult.ok
      && !stepResult.matched
      && timingResult.timing == SessionReplayTiming::Original
      && !report.finished;

  const bool ok = structurallyValid
      && losslessRoundTrip
      && !changedGroups.isEmpty()
      && !metadata.isEmpty()
      && opened
      && transport.isOpen()
      && accepted == 3
      && readerSurface
      && session == nullptr
      && fileInfo.source.sourceId == 1
      && playerSurface;
  return ok ? 0 : 1;
}
