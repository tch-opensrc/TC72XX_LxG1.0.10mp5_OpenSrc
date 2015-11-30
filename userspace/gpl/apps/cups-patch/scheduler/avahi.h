/*
 * "$Id$"
 *
 *   Avahi poll implementation for the CUPS scheduler.
 *
 *   Copyright (C) 2010 Red Hat, Inc.
 *   Authors:
 *    Tim Waugh <twaugh@redhat.com>
 *
 *   Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

#include <config.h>

#ifdef HAVE_AVAHI
#  include <avahi-client/client.h>
#  include <avahi-client/publish.h>
#endif /* HAVE_AVAHI */

#ifdef HAVE_AUTHORIZATION_H
#  include <Security/Authorization.h>
#endif /* HAVE_AUTHORIZATION_H */


#ifdef HAVE_AVAHI
typedef struct
{
    AvahiPoll api;
    cups_array_t *watched_fds;
    cups_array_t *timeouts;
} AvahiCupsPoll;
#endif /* HAVE_AVAHI */

/*
 * Prototypes...
 */

#ifdef HAVE_AVAHI
extern AvahiCupsPoll *	avahi_cups_poll_new(void);
extern void		avahi_cups_poll_free(AvahiCupsPoll *cups_poll);
extern const AvahiPoll *avahi_cups_poll_get(AvahiCupsPoll *cups_poll);
#endif /* HAVE_AVAHI */


/*
 * End of "$Id$".
 */
