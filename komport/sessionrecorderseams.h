/***************************************************************************
                     sessionrecorderseams.h  -  Komport Serial Port Communicator
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

#ifndef SESSIONRECORDERSEAMS_H
#define SESSIONRECORDERSEAMS_H

// The three seams of SPEC-M9 section 5.9: the byte sink, the flush scheduler and
// the monotonic clock.
//
// These are *internal* interfaces, not part of the recorder's public contract:
// SPEC-M9 says the seams are not part of the documented public surface, and the
// production code uses them only as its own defaults. They live in this separate
// header (not in sessionrecorder.h) so that a consumer of the recorder cannot
// mistake them for an extension point, and the injection constructor that takes
// them is private to `SessionRecorder`, reachable only through the friend test
// access struct `SessionRecorderSeamsForTest`.
//
// Lifetime: an injected seam is *not* owned by the recorder - it must outlive it.
//
// QtCore-only, like everything else in M9's surface (ADR-009).

#include <QByteArray>
#include <QString>

#include <functional>

/** Where the recorded bytes go (SPEC-M9 section 5.9). */
class SessionRecordSink
{
public:
  virtual ~SessionRecordSink() = default;

  /** Open the target, discarding any previous content. */
  virtual bool open(const QString &path) = 0;
  /** Write as much as possible; the number of bytes accepted, or -1 on failure. */
  virtual qint64 write(const QByteArray &bytes) = 0;
  /** Hand the buffered bytes to the operating system. */
  virtual bool flush() = 0;
  /** Close the target; safe to call when it is not open. */
  virtual void close() = 0;
};

/** The periodic flush's timing source (SPEC-M9 section 5.9).
  *
  * A scheduler replaces any pending schedule rather than queueing a second one,
  * which is what keeps the flush rate at one per burst. */
class SessionFlushScheduler
{
public:
  virtual ~SessionFlushScheduler() = default;

  /** Run @p callback after @p milliseconds, replacing any pending schedule. */
  virtual void schedule(int milliseconds, std::function<void()> callback) = 0;
  /** Drop a pending schedule; safe to call when none is pending. */
  virtual void cancel() = 0;
};

/** The monotonic time source of the reported duration (SPEC-M9 sections 5.6/5.9).
  *
  * The duration of a recording is wall-clock time on this clock, from the
  * successful start flush to the finalisation - never a session timestamp, which
  * measures time since the controller's anchor and would include time before the
  * recording began. */
class SessionMonotonicClock
{
public:
  virtual ~SessionMonotonicClock() = default;

  /** Monotonically non-decreasing nanoseconds. */
  virtual qint64 nowNs() = 0;
};

#endif // SESSIONRECORDERSEAMS_H
