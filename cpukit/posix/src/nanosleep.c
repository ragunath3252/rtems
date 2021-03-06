/**
 * @file
 *
 * @brief Suspends Execution of calling thread until Time elapses
 * @ingroup POSIXAPI
 */

/*
 *  COPYRIGHT (c) 1989-2015.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <time.h>
#include <errno.h>

#include <rtems/seterr.h>
#include <rtems/score/threadimpl.h>
#include <rtems/score/timespec.h>
#include <rtems/score/watchdogimpl.h>

/*
 *  14.2.5 High Resolution Sleep, P1003.1b-1993, p. 269
 */
int nanosleep(
  const struct timespec  *rqtp,
  struct timespec        *rmtp
)
{
  /*
   * It is critical to obtain the executing thread after thread dispatching is
   * disabled on SMP configurations.
   */
  Thread_Control *executing;

  Watchdog_Interval  ticks;
  Watchdog_Interval  elapsed;


  /*
   *  Return EINVAL if the delay interval is negative.
   *
   *  NOTE:  This behavior is beyond the POSIX specification.
   *         FSU and GNU/Linux pthreads shares this behavior.
   */
  if ( !_Timespec_Is_valid( rqtp ) )
    rtems_set_errno_and_return_minus_one( EINVAL );

  /*
   * Convert the timespec delay into the appropriate number of clock ticks.
   */
  ticks = _Timespec_To_ticks( rqtp );

  /*
   *  A nanosleep for zero time is implemented as a yield.
   *  This behavior is also beyond the POSIX specification but is
   *  consistent with the RTEMS API and yields desirable behavior.
   */
  if ( !ticks ) {
    _Thread_Disable_dispatch();
      executing = _Thread_Executing;
      _Thread_Yield( executing );
    _Thread_Enable_dispatch();
    if ( rmtp ) {
       rmtp->tv_sec = 0;
       rmtp->tv_nsec = 0;
    }
    return 0;
  }

  /*
   *  Block for the desired amount of time
   */
  _Thread_Disable_dispatch();
    executing = _Thread_Executing;
    _Thread_Set_state(
      executing,
      STATES_DELAYING | STATES_INTERRUPTIBLE_BY_SIGNAL
    );
    _Watchdog_Initialize(
      &executing->Timer,
      _Thread_Delay_ended,
      0,
      executing
    );
    _Watchdog_Insert_ticks( &executing->Timer, ticks );
  _Thread_Enable_dispatch();

  /*
   * Calculate the time that passed while we were sleeping and how
   * much remains from what we requested.
   */
  elapsed = executing->Timer.stop_time - executing->Timer.start_time;
  if ( elapsed >= ticks )
    ticks = 0;
  else
    ticks -= elapsed;

  /*
   * If the user wants the time remaining, do the conversion.
   */
  if ( rmtp ) {
    _Timespec_From_ticks( ticks, rmtp );
  }

  /*
   *  Only when POSIX is enabled, can a sleep be interrupted.
   */
  #if defined(RTEMS_POSIX_API)
    /*
     *  If there is time remaining, then we were interrupted by a signal.
     */
    if ( ticks )
      rtems_set_errno_and_return_minus_one( EINTR );
  #endif

  return 0;
}
