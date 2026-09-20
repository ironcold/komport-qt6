/***************************************************************************
                     sessionreplayseams.h  -  Komport Serial Port Communicator
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

#ifndef SESSIONREPLAYSEAMS_H
#define SESSIONREPLAYSEAMS_H

// The replay player's seam of SPEC-M10 section 5.10: "run this callback after
// this delay".
//
// Like M9's seams this is an *internal* interface, not part of the player's
// public surface: production code uses the default (a `QTimer`, implemented in
// the player's unit) and a test injects a scripted scheduler through
// `SessionReplayPlayerSeamsForTest`, so a test can fire deliveries explicitly
// and assert the computed delays exactly instead of waiting for wall-clock time
// (ADR-005: real-world precision is best-effort and is never asserted against).
//
// The monotonic clock the specification lists beside it is M9's existing
// `SessionMonotonicClock` (`sessionrecorderseams.h`), reused rather than
// redeclared: two classes of the same name and purpose in one library would be a
// latent name clash (amendment of 2026-09-19 in SPEC-M10 section 5.10).
//
// Lifetime: an injected seam is *not* owned by the player - it must outlive it.
//
// QtCore-only, like everything else in the session surface (ADR-009).

#include "sessionrecorderseams.h"   // SessionMonotonicClock

#include <functional>

/** The replay player's timing source (SPEC-M10 section 5.10).
  *
  * A scheduler replaces any pending schedule rather than queueing a second one:
  * the player delivers exactly one event per callback, so a replaced schedule
  * would silently drop an event and break the "complete prefix" property.
  */
class SessionReplayScheduler
{
public:
  virtual ~SessionReplayScheduler() = default;

  /** Run @p callback after @p delayNs nanoseconds, replacing any pending
    * schedule. A delay of zero still goes through the scheduler, so the event
    * loop turns between two deliveries. */
  virtual void scheduleAfter(qint64 delayNs, std::function<void()> callback) = 0;
  /** Drop a pending schedule; safe to call when none is pending. */
  /** Cancel the pending callback. **Guarantee** (SPEC-M10 5.10, amendment of 2026-09-20): a
    * callback that `cancelPending()` removed is never invoked afterwards - an implementation must
    * drop it, not merely stop its timer. The core (step 3a) relies on this and therefore keeps no
    * state check of its own inside the delivery callback. A callback that is already executing is
    * not affected by a cancellation. */
  virtual void cancelPending() = 0;
};

#endif // SESSIONREPLAYSEAMS_H
