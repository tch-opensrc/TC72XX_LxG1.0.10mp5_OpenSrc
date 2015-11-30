/*
 * "$Id$"
 *
 *   Timeout functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright (C) 2010 Red Hat, Inc.
 *   Authors:
 *     Tim Waugh <twaugh@redhat.com>
 *
 *   Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdInitTimeouts()  - Initialise timeout structure.
 *   cupsdAddTimeout()    - Add a timed callback.
 *   cupsdNextTimeout()   - Find the next enabled timed callback.
 *   cupsdUpdateTimeout() - Adjust the time of a timed callback or disable it.
 *   cupsdRemoveTimeout() - Discard a timed callback.
 *   compare_timeouts()   - Compare timed callbacks for array sorting.
 */

#include <config.h>

#ifdef HAVE_AVAHI /* Applies to entire file... */

/*
 * Include necessary headers...
 */

#include "cupsd.h"

#if defined(HAVE_MALLOC_H) && defined(HAVE_MALLINFO)
#  include <malloc.h>
#endif /* HAVE_MALLOC_H && HAVE_MALLINFO */

#ifdef HAVE_AVAHI
#  include <avahi-common/timeval.h>
#endif /* HAVE_AVAHI */


struct _cupsd_timeout_s
{
  struct timeval when;
  int enabled;
  cupsd_timeoutfunc_t callback;
  void *data;
};

/*
 * Local functions...
 */

/*
 * 'compare_timeouts()' - Compare timed callbacks for array sorting.
 */

static int
compare_timeouts (cupsd_timeout_t *p0, cupsd_timeout_t *p1)
{
  if (!p0->enabled || !p1->enabled)
  {
    if (!p0->enabled && !p1->enabled)
      return (0);

    return (p0->enabled ? -1 : 1);
  }

  return (avahi_timeval_compare (&p0->when, &p1->when));
}


/*
 * 'cupsdInitTimeouts()' - Initialise timeout structures.
 */

void
cupsdInitTimeouts(void)
{
  Timeouts = cupsArrayNew ((cups_array_func_t)compare_timeouts, NULL);
}


/*
 * 'cupsdAddTimeout()' - Add a timed callback.
 */

cupsd_timeout_t *				/* O - Timeout handle */
cupsdAddTimeout(const struct timeval *tv,	/* I - Absolute time */
		cupsd_timeoutfunc_t cb,		/* I - Callback function */
		void *data)			/* I - User data */
{
  cupsd_timeout_t *timeout;

  timeout = malloc (sizeof(cupsd_timeout_t));
  if (timeout != NULL)
  {
    timeout->enabled = (tv != NULL);
    if (tv)
    {
      timeout->when.tv_sec = tv->tv_sec;
      timeout->when.tv_usec = tv->tv_usec;
    }

    timeout->callback = cb;
    timeout->data = data;
    cupsArrayAdd (Timeouts, timeout);
  }

  return timeout;
}


/*
 * 'cupsdNextTimeout()' - Find the next enabled timed callback.
 */

cupsd_timeout_t *		/* O - Next enabled timeout or NULL */
cupsdNextTimeout(long *delay)	/* O - Seconds before scheduled */
{
  cupsd_timeout_t *first = cupsArrayFirst (Timeouts);
  struct timeval curtime;

  if (first && !first->enabled)
    first = NULL;

  if (first && delay)
  {
    gettimeofday (&curtime, NULL);
    if (avahi_timeval_compare (&curtime, &first->when) > 0)
    {
      *delay = 0;
    } else {
      *delay = 1 + first->when.tv_sec - curtime.tv_sec;
      if (first->when.tv_usec < curtime.tv_usec)
	(*delay)--;
    }
  }

  return (first);
}


/*
 * 'cupsdRunTimeout()' - Run a timed callback.
 */

void
cupsdRunTimeout(cupsd_timeout_t *timeout)	/* I - Timeout */
{
  if (!timeout)
    return;
  timeout->enabled = 0;
  if (!timeout->callback)
    return;
  timeout->callback (timeout, timeout->data);
}

/*
 * 'cupsdUpdateTimeout()' - Adjust the time of a timed callback or disable it.
 */

void
cupsdUpdateTimeout(cupsd_timeout_t *timeout,	/* I - Timeout */
		   const struct timeval *tv)	/* I - Absolute time or NULL */
{
  cupsArrayRemove (Timeouts, timeout);
  timeout->enabled = (tv != NULL);
  if (tv)
  {
    timeout->when.tv_sec = tv->tv_sec;
    timeout->when.tv_usec = tv->tv_usec;
  }
  cupsArrayAdd (Timeouts, timeout);
}


/*
 * 'cupsdRemoveTimeout()' - Discard a timed callback.
 */

void
cupsdRemoveTimeout(cupsd_timeout_t *timeout)	/* I - Timeout */
{
  cupsArrayRemove (Timeouts, timeout);
  free (timeout);
}


#endif /* HAVE_AVAHI ... from top of file */

/*
 * End of "$Id$".
 */
