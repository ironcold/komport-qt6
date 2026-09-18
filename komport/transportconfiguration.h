/***************************************************************************
                    transportconfiguration.h  -  Komport Serial Port Communicator
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

#ifndef TRANSPORTCONFIGURATION_H
#define TRANSPORTCONFIGURATION_H

// SPEC-M8 (accepted 2026-09-18), section 6.2: one configuration entry point
// takes the complete requested configuration as a value and produces exactly
// one configuration transaction. This header holds that value type and the
// transaction result.
//
// It is part of the public session/transport contract: QtCore only, no widget
// or application type (ADR-009). The field representation mirrors what
// KomportSerial stores today (the settings dialog and QSettings supply
// strings), so the authorised orchestration change of SPEC-M8 section 3 does not
// change any setting's meaning.

#include <QJsonObject>
#include <QString>
#include <QStringList>

/** The three groups a configuration transaction can change.
  *
  * - `hardware` is what reaches the port (and therefore what a session record
  *   must explain).
  * - `localBuffering` is Komport-side reception behaviour; a change of these
  *   values is recorded because they explain byte loss in the legacy display
  *   path (SPEC-M8 section 8).
  * - `compatibility` is stored and reported for UI/configuration compatibility
  *   but never applied to hardware (`startBits`: a UART always transmits one
  *   start bit and QSerialPort has no such setting).
  */
namespace TransportConfigurationGroup {
inline constexpr char kHardware[] = "hardware";
inline constexpr char kLocalBuffering[] = "localBuffering";
inline constexpr char kCompatibility[] = "compatibility";
}

/** The complete requested configuration of a local serial transport. */
struct TransportConfiguration {
  // Hardware group.
  QString endpoint;      ///< device name, e.g. /dev/ttyUSB0
  QString baudRate;      ///< parsed like the settings dialog supplies it
  QString dataBits;
  QString stopBits;
  QString parity;
  QString flowControl;

  // Local buffering group.
  int rxQueue = 1024;    ///< RX buffer high-water mark (clamped to >= 1 by the transport)
  int flushRate = 250;   ///< RX flush interval in ms (clamped to >= 1 by the transport)

  // Compatibility group.
  QString startBits;     ///< stored and reported only, never applied to hardware

  /** Compare the hardware group only. */
  bool hardwareEquals(const TransportConfiguration &other) const;
  /** Compare the local buffering group only. */
  bool bufferingEquals(const TransportConfiguration &other) const;
  /** Compare the compatibility group only. */
  bool compatibilityEquals(const TransportConfiguration &other) const;

  /** Group names whose values differ from @p baseline, in the fixed order
    * hardware, localBuffering, compatibility. Empty when nothing changed, which
    * is the documented no-op case of SPEC-M8 section 6.2. */
  QStringList changedGroupsComparedTo(const TransportConfiguration &baseline) const;

  /** The hardware group as JSON. 32-bit values stay JSON numbers (ADR-002). */
  QJsonObject hardwareToJson() const;
};

/** The outcome of one configuration transaction. */
enum class ConfigurationApplyStatus {
  Full,   ///< every requested setting was applied
  Partial,///< the port accepted only part of the requested settings
  Failed  ///< the application failed
};

/** The result of one configuration transaction.
  *
  * A transaction produces one or two transport observations (SPEC-M8 section
  * 6.2): a `configurationChanged` whenever the effective hardware settings
  * changed, plus a `transportError` with kind "apply" when the application was
  * partial or failed. Both carry this metadata.
  */
struct ConfigurationResult {
  TransportConfiguration requested;  ///< what the caller asked for
  QJsonObject effective;             ///< read-back values the transport accepted
  QStringList changedGroups;         ///< same order as changedGroupsComparedTo()
  ConfigurationApplyStatus applyStatus = ConfigurationApplyStatus::Full;
  QString message;                   ///< human-readable detail, empty on success
  /** True when `configure()` was called while the port was closed: the values
    * were stored and no transaction ran, so no observation was emitted and the
    * next `open()` performs the configuration (SPEC-M8 6.2, row "configure(request)
    * | port closed"). It is a return-value flag and never appears in the metadata.
    * It is false for a live endpoint-change request whose reopen failed: that path
    * reports its open failure as the single `Error(kind "open")` observation and
    * returns a failed result. */
  bool storedOnly = false;

  /** Machine-readable status name ("full", "partial", "failed"). */
  QString applyStatusName() const;

  /** Build the metadata object of the `configurationChanged` /
    * `transportError` observation. Contains the requested settings, the
    * transport-supplied read-back `effective` values, the local buffering
    * settings, the compatibility field `startBits`, `changedGroups` and
    * `applyStatus` - and no credentials. */
  QJsonObject toMetadata() const;
};

#endif // TRANSPORTCONFIGURATION_H
