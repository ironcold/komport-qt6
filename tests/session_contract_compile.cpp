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
#include "sessionevent.h"
#include "transportconfiguration.h"

#include <QJsonObject>
#include <QStringList>

namespace {

/** Minimal ITransport implementation; proves that the frozen interface can be
  * implemented from Qt Core alone. */
class ContractProbeTransport : public ITransport
{
public:
  using ITransport::ITransport;

  bool open() override { return true; }
  void close() override {}
  bool isOpen() const override { return false; }
  qint64 writeBytes(const QByteArray &bytes) override { return bytes.size(); }

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
  const qint64 accepted = transport.writeBytes(QByteArrayLiteral("abc"));

  const bool ok = structurallyValid
      && losslessRoundTrip
      && !changedGroups.isEmpty()
      && !metadata.isEmpty()
      && accepted == 3;
  return ok ? 0 : 1;
}
