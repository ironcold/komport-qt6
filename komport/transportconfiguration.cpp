/***************************************************************************
                   transportconfiguration.cpp  -  Komport Serial Port Communicator
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

#include "transportconfiguration.h"

#include <QJsonArray>
#include <QLatin1String>

bool TransportConfiguration::hardwareEquals(const TransportConfiguration &other) const
{
  return endpoint == other.endpoint
      && baudRate == other.baudRate
      && dataBits == other.dataBits
      && stopBits == other.stopBits
      && parity == other.parity
      && flowControl == other.flowControl;
}

bool TransportConfiguration::bufferingEquals(const TransportConfiguration &other) const
{
  return rxQueue == other.rxQueue && flushRate == other.flushRate;
}

bool TransportConfiguration::compatibilityEquals(const TransportConfiguration &other) const
{
  return startBits == other.startBits;
}

QStringList TransportConfiguration::changedGroupsComparedTo(const TransportConfiguration &baseline) const
{
  // Fixed order so the result is deterministic and testable.
  QStringList changed;
  if (!hardwareEquals(baseline))
    changed.append(QLatin1String(TransportConfigurationGroup::kHardware));
  if (!bufferingEquals(baseline))
    changed.append(QLatin1String(TransportConfigurationGroup::kLocalBuffering));
  if (!compatibilityEquals(baseline))
    changed.append(QLatin1String(TransportConfigurationGroup::kCompatibility));
  return changed;
}

QJsonObject TransportConfiguration::hardwareToJson() const
{
  QJsonObject json;
  json.insert(QStringLiteral("endpoint"), endpoint);
  json.insert(QStringLiteral("baudRate"), baudRate);
  json.insert(QStringLiteral("dataBits"), dataBits);
  json.insert(QStringLiteral("stopBits"), stopBits);
  json.insert(QStringLiteral("parity"), parity);
  json.insert(QStringLiteral("flowControl"), flowControl);
  return json;
}

QString ConfigurationResult::applyStatusName() const
{
  switch (applyStatus) {
    case ConfigurationApplyStatus::Full:
      return QStringLiteral("full");
    case ConfigurationApplyStatus::Partial:
      return QStringLiteral("partial");
    case ConfigurationApplyStatus::Failed:
      return QStringLiteral("failed");
  }
  return QStringLiteral("failed");
}

QJsonObject appliedConfigurationSections(const TransportConfiguration &_requested,
                                         const QJsonObject &_effective)
{
  // Requested hardware settings, the transport-supplied read-back values, the
  // local buffering settings and the compatibility field. 32-bit values stay
  // JSON numbers (ADR-002); no credentials are ever added.
  QJsonObject sections;
  sections.insert(QStringLiteral("requested"), _requested.hardwareToJson());
  sections.insert(QStringLiteral("effective"), _effective);

  QJsonObject buffering;
  buffering.insert(QStringLiteral("rxQueue"), _requested.rxQueue);
  buffering.insert(QStringLiteral("flushRate"), _requested.flushRate);
  sections.insert(QStringLiteral("localBuffering"), buffering);

  QJsonObject compatibility;
  compatibility.insert(QStringLiteral("startBits"), _requested.startBits);
  sections.insert(QStringLiteral("compatibility"), compatibility);

  return sections;
}

QJsonObject ConfigurationResult::toMetadata() const
{
  QJsonObject metadata = appliedConfigurationSections(requested, effective);

  QJsonArray groups;
  for (const QString &group : changedGroups)
    groups.append(group);
  metadata.insert(QStringLiteral("changedGroups"), groups);

  metadata.insert(QStringLiteral("applyStatus"), applyStatusName());
  if (!message.isEmpty())
    metadata.insert(QStringLiteral("message"), message);

  return metadata;
}
