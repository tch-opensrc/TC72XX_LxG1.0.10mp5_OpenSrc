/*
 * "$Id: dnssd.c 9815 2011-06-05 16:58:33Z mike $"
 *
 *   DNS-SD discovery backend for CUPS.
 *
 *   Copyright 2008-2011 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   next_txt_record()       - Get next TXT record from a cups_txt_records_t.
 *   parse_txt_record_pair() - Read key/value pair in cups_txt_records_t.
 *   main()                  - Browse for printers.
 *   browse_callback()       - Browse devices.
 *   browse_local_callback() - Browse local devices.
 *   compare_devices()       - Compare two devices.
 *   exec_backend()          - Execute the backend that corresponds to the
 *                             resolved service name.
 *   device_type()           - Get DNS-SD type enumeration from string.
 *   get_device()            - Create or update a device.
 *   query_callback()        - Process query data.
 *   avahi_client_callback() - Avahi client callback function.
 *   avahi_query_callback()  - Avahi query callback function.
 *   avahi_browse_callback() - Avahi browse callback function.
 *   find_device()           - Find a device from its name and domain.
 *   sigterm_handler()       - Handle termination signals...
 *   unquote()               - Unquote a name string.
 */

/*
 * Include necessary headers.
 */

#include "backend-private.h"
#include <cups/array.h>
#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#endif /* HAVE_DNSSD */
#ifdef HAVE_AVAHI
#  include <avahi-client/client.h>
#  include <avahi-client/lookup.h>
#  include <avahi-common/simple-watch.h>
#  include <avahi-common/domain.h>
#  include <avahi-common/error.h>
#  include <avahi-common/malloc.h>
#define kDNSServiceMaxDomainName AVAHI_DOMAIN_NAME_MAX
#endif /* HAVE_AVAHI */


/*
 * Device structure...
 */

typedef enum
{
  CUPS_DEVICE_PRINTER = 0,		/* lpd://... */
  CUPS_DEVICE_IPP,			/* ipp://... */
  CUPS_DEVICE_IPPS,			/* ipps://... */
  CUPS_DEVICE_FAX_IPP,			/* ipp://... */
  CUPS_DEVICE_PDL_DATASTREAM,		/* socket://... */
  CUPS_DEVICE_RIOUSBPRINT		/* riousbprint://... */
} cups_devtype_t;


typedef struct
{
#ifdef HAVE_DNSSD
  DNSServiceRef	ref;			/* Service reference for resolve */
#endif /* HAVE_DNSSD */
#ifdef HAVE_AVAHI
  int		resolved;		/* Did we resolve the device? */
#endif /* HAVE_AVAHI */
  char		*name,			/* Service name */
		*domain,		/* Domain name */
		*fullName,		/* Full name */
		*make_and_model,	/* Make and model from TXT record */
		*device_id;		/* 1284 device ID from TXT record */
  cups_devtype_t type;			/* Device registration type */
  int		priority,		/* Priority associated with type */
		cups_shared,		/* CUPS shared printer? */
		sent;			/* Did we list the device? */
} cups_device_t;

typedef struct
{
  char key[256];
  char value[256];

#ifdef HAVE_DNSSD
  const uint8_t *data;
  const uint8_t *datanext;
  const uint8_t *dataend;
#else /* HAVE_AVAHI */
  AvahiStringList *txt;
#endif /* HAVE_DNSSD */
} cups_txt_records_t;


/*
 * Local globals...
 */

static int		job_canceled = 0;
					/* Set to 1 on SIGTERM */


/*
 * Local functions...
 */

#ifdef HAVE_DNSSD
static void		browse_callback(DNSServiceRef sdRef,
			                DNSServiceFlags flags,
				        uint32_t interfaceIndex,
				        DNSServiceErrorType errorCode,
				        const char *serviceName,
				        const char *regtype,
				        const char *replyDomain, void *context);
static void		browse_local_callback(DNSServiceRef sdRef,
					      DNSServiceFlags flags,
					      uint32_t interfaceIndex,
					      DNSServiceErrorType errorCode,
					      const char *serviceName,
					      const char *regtype,
					      const char *replyDomain,
					      void *context);
static void		query_callback(DNSServiceRef sdRef,
			               DNSServiceFlags flags,
				       uint32_t interfaceIndex,
				       DNSServiceErrorType errorCode,
				       const char *fullName, uint16_t rrtype,
				       uint16_t rrclass, uint16_t rdlen,
				       const void *rdata, uint32_t ttl,
				       void *context);
#endif /* HAVE_DNSSD */
#ifdef HAVE_AVAHI
static void		avahi_client_callback (AvahiClient *client,
					       AvahiClientState state,
					       void *context);
static void		avahi_browse_callback (AvahiServiceBrowser *browser,
					       AvahiIfIndex interface,
					       AvahiProtocol protocol,
					       AvahiBrowserEvent event,
					       const char *serviceName,
					       const char *regtype,
					       const char *replyDomain,
					       AvahiLookupResultFlags flags,
					       void *context);
#endif /* HAVE_AVAHI */

static cups_device_t *	find_device (cups_array_t *devices,
				     cups_txt_records_t *txt,
				     cups_device_t *dkey);
static int		compare_devices(cups_device_t *a, cups_device_t *b);
static void		exec_backend(char **argv);
static cups_device_t	*get_device(cups_array_t *devices,
			            const char *serviceName,
			            const char *regtype,
				    const char *replyDomain);
static void		sigterm_handler(int sig);
static void		unquote(char *dst, const char *src, size_t dstsize);

#ifdef HAVE_AVAHI
static AvahiSimplePoll *simple_poll = NULL;
static int avahi_got_callback;
#endif /* HAVE_AVAHI */


/*
 * 'next_txt_record()' - Get next TXT record from a cups_txt_records_t.
 */

static cups_txt_records_t *
next_txt_record (cups_txt_records_t *txt)
{
#ifdef HAVE_DNSSD
  txt->data = txt->datanext;
#else /* HAVE_AVAHI */
  txt->txt = avahi_string_list_get_next (txt->txt);
  if (txt->txt == NULL)
    return NULL;
#endif /* HAVE_DNSSD */

  return txt;
}


/*
 * 'parse_txt_record_pair()' - Read key/value pair in cups_txt_records_t.
 */

static int
parse_txt_record_pair (cups_txt_records_t *txt)
{
#ifdef HAVE_DNSSD
  uint8_t	datalen;
  uint8_t	*data = txt->data;
  char		*ptr;

 /*
  * Read a key/value pair starting with an 8-bit length.  Since the
  * length is 8 bits and the size of the key/value buffers is 256, we
  * don't need to check for overflow...
  */

  datalen = *data++;
  if (!datalen || (data + datalen) >= txt->dataend)
    return NULL;
  txt->datanext = data + datalen;

  for (ptr = txt->key; data < txt->datanext && *data != '='; data ++)
    *ptr++ = *data;
  *ptr = '\0';

  if (data < txt->datanext && *data == '=')
  {
    data++;

    if (data < datanext)
      memcpy (txt->value, data, txt->datanext - data);
    value[txt->datanext - data] = '\0';
  }
  else
    return 1;
#else /* HAVE_AVAHI */
  char *key, *value;
  size_t len;
  avahi_string_list_get_pair (txt->txt, &key, &value, &len);
  if (len > sizeof (txt->value) - 1)
    len = sizeof (txt->value) - 1;

  memcpy (txt->value, value, len);
  txt->value[len] = '\0';
  len = strlen (key);
  if (len > sizeof (txt->key) - 1)
    len = sizeof (txt->key) - 1;

  memcpy (txt->key, key, len);
  txt->key[len] = '\0';
  avahi_free (key);
  avahi_free (value);
#endif /* HAVE_AVAHI */

  return 0;
}


/*
 * 'main()' - Browse for printers.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  const char	*name;			/* Backend name */
  cups_array_t	*devices;		/* Device array */
  cups_device_t	*device;		/* Current device */
  char		uriName[1024];		/* Unquoted fullName for URI */
#ifdef HAVE_DNSSD
  int		fd;			/* Main file descriptor */
  fd_set	input;			/* Input set for select() */
  struct timeval timeout;		/* Timeout for select() */
  DNSServiceRef	main_ref,		/* Main service reference */
		fax_ipp_ref,		/* IPP fax service reference */
		ipp_ref,		/* IPP service reference */
		ipp_tls_ref,		/* IPP w/TLS service reference */
		ipps_ref,		/* IPP service reference */
		local_fax_ipp_ref,	/* Local IPP fax service reference */
		local_ipp_ref,		/* Local IPP service reference */
		local_ipp_tls_ref,	/* Local IPP w/TLS service reference */
		local_ipps_ref,		/* Local IPP service reference */
		local_printer_ref,	/* Local LPD service reference */
		pdl_datastream_ref,	/* AppSocket service reference */
		printer_ref,		/* LPD service reference */
		riousbprint_ref;	/* Remote IO service reference */
#endif /* HAVE_DNSSD */
#ifdef HAVE_AVAHI
  AvahiClient	*client;
  int		error;
#endif /* HAVE_AVAHI */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Don't buffer stderr, and catch SIGTERM...
  */

  setbuf(stderr, NULL);

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, sigterm_handler);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = sigterm_handler;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, sigterm_handler);
#endif /* HAVE_SIGSET */

 /*
  * Check command-line...
  */

  if (argc >= 6)
    exec_backend(argv);
  else if (argc != 1)
  {
    _cupsLangPrintf(stderr,
                    _("Usage: %s job-id user title copies options [file]"),
		    argv[0]);
    return (1);
  }

 /*
  * Only do discovery when run as "dnssd"...
  */

  if ((name = strrchr(argv[0], '/')) != NULL)
    name ++;
  else
    name = argv[0];

  if (strcmp(name, "dnssd"))
    return (0);

 /*
  * Create an array to track devices...
  */

  devices = cupsArrayNew((cups_array_func_t)compare_devices, NULL);

 /*
  * Browse for different kinds of printers...
  */

#ifdef HAVE_AVAHI
  if ((simple_poll = avahi_simple_poll_new ()) == NULL)
  {
    perror ("ERROR: Unable to create avahi simple poll object");
    return (1);
  }

  client = avahi_client_new (avahi_simple_poll_get (simple_poll),
			     0, avahi_client_callback, NULL, &error);
  if (!client)
  {
    perror ("DEBUG: Unable to create avahi client");
    return (0);
  }

  avahi_service_browser_new (client, AVAHI_IF_UNSPEC,
			     AVAHI_PROTO_UNSPEC,
			     "_fax-ipp._tcp", NULL, 0,
			     avahi_browse_callback, devices);
  avahi_service_browser_new (client, AVAHI_IF_UNSPEC,
			     AVAHI_PROTO_UNSPEC,
			     "_ipp._tcp", NULL, 0,
			     avahi_browse_callback, devices);
  avahi_service_browser_new (client, AVAHI_IF_UNSPEC,
			     AVAHI_PROTO_UNSPEC,
			     "_ipp-tls._tcp", NULL, 0,
			     avahi_browse_callback, devices);
  avahi_service_browser_new (client, AVAHI_IF_UNSPEC,
			     AVAHI_PROTO_UNSPEC,
			     "_pdl-datastream._tcp",
			     NULL, 0,
			     avahi_browse_callback,
			     devices);
  avahi_service_browser_new (client, AVAHI_IF_UNSPEC,
			     AVAHI_PROTO_UNSPEC,
			     "_printer._tcp", NULL, 0,
			     avahi_browse_callback, devices);
  avahi_service_browser_new (client, AVAHI_IF_UNSPEC,
			     AVAHI_PROTO_UNSPEC,
			     "_riousbprint._tcp", NULL, 0,
			     avahi_browse_callback, devices);
#endif /* HAVE_AVAHI */
#ifdef HAVE_DNSSD
  if (DNSServiceCreateConnection(&main_ref) != kDNSServiceErr_NoError)
  {
    perror("ERROR: Unable to create service connection");
    return (1);
  }

  fd = DNSServiceRefSockFD(main_ref);

  fax_ipp_ref = main_ref;
  DNSServiceBrowse(&fax_ipp_ref, kDNSServiceFlagsShareConnection, 0,
                   "_fax-ipp._tcp", NULL, browse_callback, devices);

  ipp_ref = main_ref;
  DNSServiceBrowse(&ipp_ref, kDNSServiceFlagsShareConnection, 0,
                   "_ipp._tcp", NULL, browse_callback, devices);

  ipp_tls_ref = main_ref;
  DNSServiceBrowse(&ipp_tls_ref, kDNSServiceFlagsShareConnection, 0,
                   "_ipp-tls._tcp", NULL, browse_callback, devices);

  ipps_ref = main_ref;
  DNSServiceBrowse(&ipp_ref, kDNSServiceFlagsShareConnection, 0,
                   "_ipps._tcp", NULL, browse_callback, devices);

  local_fax_ipp_ref = main_ref;
  DNSServiceBrowse(&local_fax_ipp_ref, kDNSServiceFlagsShareConnection,
                   kDNSServiceInterfaceIndexLocalOnly,
		   "_fax-ipp._tcp", NULL, browse_local_callback, devices);

  local_ipp_ref = main_ref;
  DNSServiceBrowse(&local_ipp_ref, kDNSServiceFlagsShareConnection,
                   kDNSServiceInterfaceIndexLocalOnly,
		   "_ipp._tcp", NULL, browse_local_callback, devices);

  local_ipp_tls_ref = main_ref;
  DNSServiceBrowse(&local_ipp_tls_ref, kDNSServiceFlagsShareConnection,
                   kDNSServiceInterfaceIndexLocalOnly,
                   "_ipp-tls._tcp", NULL, browse_local_callback, devices);

  local_ipps_ref = main_ref;
  DNSServiceBrowse(&local_ipp_ref, kDNSServiceFlagsShareConnection,
                   kDNSServiceInterfaceIndexLocalOnly,
		   "_ipps._tcp", NULL, browse_local_callback, devices);

  local_printer_ref = main_ref;
  DNSServiceBrowse(&local_printer_ref, kDNSServiceFlagsShareConnection,
                   kDNSServiceInterfaceIndexLocalOnly,
                   "_printer._tcp", NULL, browse_local_callback, devices);

  pdl_datastream_ref = main_ref;
  DNSServiceBrowse(&pdl_datastream_ref, kDNSServiceFlagsShareConnection, 0,
                   "_pdl-datastream._tcp", NULL, browse_callback, devices);

  printer_ref = main_ref;
  DNSServiceBrowse(&printer_ref, kDNSServiceFlagsShareConnection, 0,
                   "_printer._tcp", NULL, browse_callback, devices);

  riousbprint_ref = main_ref;
  DNSServiceBrowse(&riousbprint_ref, kDNSServiceFlagsShareConnection, 0,
                   "_riousbprint._tcp", NULL, browse_callback, devices);
#endif /* HAVE_DNSSD */

 /*
  * Loop until we are killed...
  */

  while (!job_canceled)
  {
    int announce = 0;

#ifdef HAVE_DNSSD
    FD_ZERO(&input);
    FD_SET(fd, &input);

    timeout.tv_sec  = 0;
    timeout.tv_usec = 250000;

    if (select(fd + 1, &input, NULL, NULL, &timeout) < 0)
      continue;

    if (FD_ISSET(fd, &input))
    {
     /*
      * Process results of our browsing...
      */

      DNSServiceProcessResult(main_ref);
    }
    else
    {
      announce = 1;
    }
#else /* HAVE_AVAHI */
    int r;
    avahi_got_callback = 0;
    r = avahi_simple_poll_iterate (simple_poll, 1);
    if (r != 0 && r != EINTR)
    {
     /*
      * We've been told to exit the loop.  Perhaps the connection to
      * avahi failed.
      */

      break;
    }

    if (avahi_got_callback)
      announce = 1;
#endif /* HAVE_DNSSD */

    if (announce)
    {
     /*
      * Announce any devices we've found...
      */

#ifdef HAVE_DNSSD
      DNSServiceErrorType status;	/* DNS query status */
#endif /* HAVE_DNSSD */
      cups_device_t *best;		/* Best matching device */
      char	device_uri[1024];	/* Device URI */
      int	count;			/* Number of queries */
      int	sent;			/* Number of sent */

      for (device = (cups_device_t *)cupsArrayFirst(devices),
               best = NULL, count = 0, sent = 0;
           device;
	   device = (cups_device_t *)cupsArrayNext(devices))
      {
        if (device->sent)
	  sent ++;

#ifdef HAVE_DNSSD
        if (device->ref)
	  count ++;

        if (!device->ref && !device->sent)
	{
	 /*
	  * Found the device, now get the TXT record(s) for it...
	  */

          if (count < 20)
	  {
	    device->ref = main_ref;

	    fprintf(stderr, "DEBUG: Querying \"%s\"...\n", device->fullName);

	    status = DNSServiceQueryRecord(&(device->ref),
				           kDNSServiceFlagsShareConnection,
				           0, device->fullName,
					   kDNSServiceType_TXT,
				           kDNSServiceClass_IN, query_callback,
				           devices);
            if (status != kDNSServiceErr_NoError)
	    {
	      fputs("ERROR: Unable to query for TXT records!\n", stderr);
	      fprintf(stderr, "DEBUG: DNSServiceQueryRecord returned %d\n",
	              status);
            }
	    else
	      count ++;
          }
	}
	else
#endif /* HAVE_DNSSD */
#ifdef HAVE_AVAHI
	if (!device->resolved)
	  continue;
        else
#endif /* HAVE_AVAHI */
	if (!device->sent)
	{
#ifdef HAVE_DNSSD
	 /*
	  * Got the TXT records, now report the device...
	  */

	  DNSServiceRefDeallocate(device->ref);
	  device->ref = 0;
#endif /* HAVE_DNSSD */

          if (!best)
	    best = device;
	  else if (_cups_strcasecmp(best->name, device->name) ||
	           _cups_strcasecmp(best->domain, device->domain))
          {
	    unquote(uriName, best->fullName, sizeof(uriName));

	    httpAssembleURI(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri),
			    "dnssd", NULL, uriName, 0,
			    best->cups_shared ? "/cups" : "/");

	    cupsBackendReport("network", device_uri, best->make_and_model,
	                      best->name, best->device_id, NULL);
	    best->sent = 1;
	    best       = device;

	    sent ++;
	  }
	  else if (best->priority > device->priority ||
	           (best->priority == device->priority &&
		    best->type < device->type))
          {
	    best->sent = 1;
	    best       = device;

	    sent ++;
	  }
	  else
	  {
	    device->sent = 1;

	    sent ++;
	  }
        }
      }

      if (best)
      {
	unquote(uriName, best->fullName, sizeof(uriName));

	httpAssembleURI(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri),
			"dnssd", NULL, uriName, 0,
			best->cups_shared ? "/cups" : "/");

	cupsBackendReport("network", device_uri, best->make_and_model,
			  best->name, best->device_id, NULL);
	best->sent = 1;
	sent ++;
      }

      if (sent == cupsArrayCount(devices))
	break;
    }
  }

  return (CUPS_BACKEND_OK);
}


#ifdef HAVE_DNSSD
/*
 * 'browse_callback()' - Browse devices.
 */

static void
browse_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Option flags */
    uint32_t            interfaceIndex,	/* I - Interface number */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *serviceName,	/* I - Name of service/device */
    const char          *regtype,	/* I - Type of service */
    const char          *replyDomain,	/* I - Service domain */
    void                *context)	/* I - Devices array */
{
  fprintf(stderr, "DEBUG2: browse_callback(sdRef=%p, flags=%x, "
                  "interfaceIndex=%d, errorCode=%d, serviceName=\"%s\", "
		  "regtype=\"%s\", replyDomain=\"%s\", context=%p)\n",
          sdRef, flags, interfaceIndex, errorCode,
	  serviceName ? serviceName : "(null)",
	  regtype ? regtype : "(null)",
	  replyDomain ? replyDomain : "(null)",
	  context);

 /*
  * Only process "add" data...
  */

  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

 /*
  * Get the device...
  */

  get_device((cups_array_t *)context, serviceName, regtype, replyDomain);
}


/*
 * 'browse_local_callback()' - Browse local devices.
 */

static void
browse_local_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Option flags */
    uint32_t            interfaceIndex,	/* I - Interface number */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *serviceName,	/* I - Name of service/device */
    const char          *regtype,	/* I - Type of service */
    const char          *replyDomain,	/* I - Service domain */
    void                *context)	/* I - Devices array */
{
  cups_device_t	*device;		/* Device */


  fprintf(stderr, "DEBUG2: browse_local_callback(sdRef=%p, flags=%x, "
                  "interfaceIndex=%d, errorCode=%d, serviceName=\"%s\", "
		  "regtype=\"%s\", replyDomain=\"%s\", context=%p)\n",
          sdRef, flags, interfaceIndex, errorCode,
	  serviceName ? serviceName : "(null)",
	  regtype ? regtype : "(null)",
	  replyDomain ? replyDomain : "(null)",
	  context);

 /*
  * Only process "add" data...
  */

  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

 /*
  * Get the device...
  */

  device = get_device((cups_array_t *)context, serviceName, regtype,
                      replyDomain);

 /*
  * Hide locally-registered devices...
  */

  fprintf(stderr, "DEBUG: Hiding local printer \"%s\"...\n",
	  device->fullName);
  device->sent = 1;
}
#endif /* HAVE_DNSSD */


/*
 * 'compare_devices()' - Compare two devices.
 */

static int				/* O - Result of comparison */
compare_devices(cups_device_t *a,	/* I - First device */
                cups_device_t *b)	/* I - Second device */
{
  return (strcmp(a->name, b->name));
}


/*
 * 'exec_backend()' - Execute the backend that corresponds to the
 *                    resolved service name.
 */

static void
exec_backend(char **argv)		/* I - Command-line arguments */
{
  const char	*resolved_uri,		/* Resolved device URI */
		*cups_serverbin;	/* Location of programs */
  char		scheme[1024],		/* Scheme from URI */
		*ptr,			/* Pointer into scheme */
		filename[1024];		/* Backend filename */


 /*
  * Resolve the device URI...
  */

  job_canceled = -1;

  while ((resolved_uri = cupsBackendDeviceURI(argv)) == NULL)
  {
    _cupsLangPrintFilter(stderr, "INFO", _("Unable to locate printer."));
    sleep(10);

    if (getenv("CLASS") != NULL)
      exit(CUPS_BACKEND_FAILED);
  }

 /*
  * Extract the scheme from the URI...
  */

  strlcpy(scheme, resolved_uri, sizeof(scheme));
  if ((ptr = strchr(scheme, ':')) != NULL)
    *ptr = '\0';

 /*
  * Get the filename of the backend...
  */

  if ((cups_serverbin = getenv("CUPS_SERVERBIN")) == NULL)
    cups_serverbin = CUPS_SERVERBIN;

  snprintf(filename, sizeof(filename), "%s/backend/%s", cups_serverbin, scheme);

 /*
  * Overwrite the device URI and run the new backend...
  */

  setenv("DEVICE_URI", resolved_uri, 1);

  argv[0] = (char *)resolved_uri;

  fprintf(stderr, "DEBUG: Executing backend \"%s\"...\n", filename);

  execv(filename, argv);

  fprintf(stderr, "ERROR: Unable to execute backend \"%s\": %s\n", filename,
          strerror(errno));
  exit(CUPS_BACKEND_STOP);
}


/*
 * 'device_type()' - Get DNS-SD type enumeration from string.
 */

static int
device_type (const char *regtype)
{
#ifdef HAVE_AVAHI
  if (!strcmp(regtype, "_ipp._tcp"))
    return (CUPS_DEVICE_IPP);
  else if (!strcmp(regtype, "_ipps._tcp") ||
	   !strcmp(regtype, "_ipp-tls._tcp"))
    return (CUPS_DEVICE_IPPS);
  else if (!strcmp(regtype, "_fax-ipp._tcp"))
    return (CUPS_DEVICE_FAX_IPP);
  else if (!strcmp(regtype, "_printer._tcp"))
    return (CUPS_DEVICE_PDL_DATASTREAM);
#else
  if (!strcmp(regtype, "_ipp._tcp."))
    return (CUPS_DEVICE_IPP);
  else if (!strcmp(regtype, "_ipps._tcp.") ||
	   !strcmp(regtype, "_ipp-tls._tcp."))
    return (CUPS_DEVICE_IPPS);
  else if (!strcmp(regtype, "_fax-ipp._tcp."))
    return (CUPS_DEVICE_FAX_IPP);
  else if (!strcmp(regtype, "_printer._tcp."))
    return (CUPS_DEVICE_PRINTER);
  else if (!strcmp(regtype, "_pdl-datastream._tcp."))
    return (CUPS_DEVICE_PDL_DATASTREAM);
#endif /* HAVE_AVAHI */

  return (CUPS_DEVICE_RIOUSBPRINT);
}


/*
 * 'get_device()' - Create or update a device.
 */

static cups_device_t *			/* O - Device */
get_device(cups_array_t *devices,	/* I - Device array */
           const char   *serviceName,	/* I - Name of service/device */
           const char   *regtype,	/* I - Type of service */
           const char   *replyDomain)	/* I - Service domain */
{
  cups_device_t	key,			/* Search key */
		*device;		/* Device */
  char		fullName[kDNSServiceMaxDomainName];
					/* Full name for query */


 /*
  * See if this is a new device...
  */

  key.name = (char *)serviceName;
  key.type = device_type (regtype);

  for (device = cupsArrayFind(devices, &key);
       device;
       device = cupsArrayNext(devices))
    if (_cups_strcasecmp(device->name, key.name))
      break;
    else if (device->type == key.type)
    {
      if (!_cups_strcasecmp(device->domain, "local.") &&
          _cups_strcasecmp(device->domain, replyDomain))
      {
       /*
        * Update the .local listing to use the "global" domain name instead.
	* The backend will try local lookups first, then the global domain name.
	*/

        free(device->domain);
	device->domain = strdup(replyDomain);

#ifdef HAVE_DNSSD
	DNSServiceConstructFullName(fullName, device->name, regtype,
	                            replyDomain);
#else /* HAVE_AVAHI */
	avahi_service_name_join (fullName, kDNSServiceMaxDomainName,
				 serviceName, regtype, replyDomain);
#endif /* HAVE_DNSSD */

	free(device->fullName);
	device->fullName = strdup(fullName);
      }

      return (device);
    }

 /*
  * Yes, add the device...
  */

  fprintf(stderr, "DEBUG: Found \"%s.%s%s\"...\n", serviceName, regtype,
	  replyDomain);

  device           = calloc(sizeof(cups_device_t), 1);
  device->name     = strdup(serviceName);
  device->domain   = strdup(replyDomain);
  device->type     = key.type;
  device->priority = 50;
#ifdef HAVE_AVAHI
  device->resolved = 0;
#endif /* HAVE_AVAHI */

  cupsArrayAdd(devices, device);

 /*
  * Set the "full name" of this service, which is used for queries...
  */

#ifdef HAVE_DNSSD
  DNSServiceConstructFullName(fullName, serviceName, regtype, replyDomain);
#else /* HAVE_AVAHI */
  avahi_service_name_join (fullName, kDNSServiceMaxDomainName,
			   serviceName, regtype, replyDomain);
#endif /* HAVE_DNSSD */

  device->fullName = strdup(fullName);

  return (device);
}


#ifdef HAVE_DNSSD
/*
 * 'query_callback()' - Process query data.
 */

static void
query_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Data flags */
    uint32_t            interfaceIndex,	/* I - Interface */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *fullName,	/* I - Full service name */
    uint16_t            rrtype,		/* I - Record type */
    uint16_t            rrclass,	/* I - Record class */
    uint16_t            rdlen,		/* I - Length of record data */
    const void          *rdata,		/* I - Record data */
    uint32_t            ttl,		/* I - Time-to-live */
    void                *context)	/* I - Devices array */
{
  cups_array_t	*devices;		/* Device array */
  char		name[1024],		/* Service name */
		*ptr;			/* Pointer into string */
  cups_device_t	dkey,			/* Search key */
		*device;		/* Device */
  cups_txt_records_t txt;

  fprintf(stderr, "DEBUG2: query_callback(sdRef=%p, flags=%x, "
                  "interfaceIndex=%d, errorCode=%d, fullName=\"%s\", "
		  "rrtype=%u, rrclass=%u, rdlen=%u, rdata=%p, ttl=%u, "
		  "context=%p)\n",
          sdRef, flags, interfaceIndex, errorCode,
	  fullName ? fullName : "(null)", rrtype, rrclass, rdlen, rdata, ttl,
	  context);

 /*
  * Only process "add" data...
  */

  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

 /*
  * Lookup the service in the devices array.
  */

  devices   = (cups_array_t *)context;
  dkey.name = name;

  unquote(name, fullName, sizeof(name));

  if ((dkey.domain = strstr(name, "._tcp.")) != NULL)
    dkey.domain += 6;
  else
    dkey.domain = (char *)"local.";

  if ((ptr = strstr(name, "._")) != NULL)
    *ptr = '\0';

  dkey.type = device_type (fullName);

  txt.data = rdata;
  txt.dataend = rdata + rdlen;
  device = find_device ((cups_array_t *) context, &txt, &dkey);
  if (!device)
    fprintf(stderr, "DEBUG: Ignoring TXT record for \"%s\"...\n", fullName);
}
#endif /* HAVE_DNSSD */


#ifdef HAVE_AVAHI
/*
 * 'avahi_client_callback()' - Avahi client callback function.
 */

static void
avahi_client_callback(AvahiClient *client,
		      AvahiClientState state,
		      void *context)
{
 /*
  * If the connection drops, quit.
  */

  if (state == AVAHI_CLIENT_FAILURE)
  {
    fprintf (stderr, "ERROR: Avahi connection failed\n");
    avahi_simple_poll_quit (simple_poll);
  }
}


/*
 * 'avahi_query_callback()' - Avahi query callback function.
 */

static void
avahi_query_callback(AvahiServiceResolver *resolver,
		     AvahiIfIndex interface,
		     AvahiProtocol protocol,
		     AvahiResolverEvent event,
		     const char *name,
		     const char *type,
		     const char *domain,
		     const char *host_name,
		     const AvahiAddress *address,
		     uint16_t port,
		     AvahiStringList *txt,
		     AvahiLookupResultFlags flags,
		     void *context)
{
  AvahiClient		*client;
  cups_device_t		key,
			*device;
  char			uqname[1024],
			*ptr;
  cups_txt_records_t	txtr;

  client = avahi_service_resolver_get_client (resolver);
  if (event != AVAHI_RESOLVER_FOUND)
  {
    if (event == AVAHI_RESOLVER_FAILURE)
    {
      fprintf (stderr, "ERROR: %s\n",
	       avahi_strerror (avahi_client_errno (client)));
    }

    avahi_service_resolver_free (resolver);
    return;
  }

 /*
  * Set search key for device.
  */

  key.name = uqname;
  unquote (uqname, name, sizeof (uqname));
  if ((ptr = strstr(name, "._")) != NULL)
    *ptr = '\0';

  key.domain = (char *) domain;
  key.type = device_type (type);

 /*
  * Find the device and the the TXT information.
  */

  txtr.txt = txt;
  device = find_device ((cups_array_t *) context, &txtr, &key);
  if (device)
  {
   /*
    * Let the main loop know to announce the device.
    */

    device->resolved = 1;
    avahi_got_callback = 1;
  }
  else
    fprintf (stderr, "DEBUG: Ignoring TXT record for \"%s\"...\n", name);

  avahi_service_resolver_free (resolver);
}


/*
 * 'avahi_browse_callback()' - Avahi browse callback function.
 */

static void
avahi_browse_callback(AvahiServiceBrowser *browser,
		      AvahiIfIndex interface,
		      AvahiProtocol protocol,
		      AvahiBrowserEvent event,
		      const char *name,
		      const char *type,
		      const char *domain,
		      AvahiLookupResultFlags flags,
		      void *context)
{
  AvahiClient *client = avahi_service_browser_get_client (browser);

  switch (event)
  {
  case AVAHI_BROWSER_FAILURE:
    fprintf (stderr, "ERROR: %s\n",
	     avahi_strerror (avahi_client_errno (client)));
    avahi_simple_poll_quit (simple_poll);
    return;

  case AVAHI_BROWSER_NEW:
   /*
    * This object is new on the network.
    */

    if (flags & AVAHI_LOOKUP_RESULT_LOCAL)
    {
     /*
      * This comes from the local machine so ignore it.
      */

      fprintf (stderr, "DEBUG: ignoring local service %s\n", name);
    }
    else
    {
     /*
      * Create a device entry for it if it doesn't yet exist.
      */

      get_device ((cups_array_t *)context, name, type, domain);

     /*
      * Now look for a TXT entry.
      */

      if (avahi_service_resolver_new (client, interface, protocol,
				      name, type, domain,
				      AVAHI_PROTO_UNSPEC, 0,
				      avahi_query_callback, context) == NULL)
      {
	fprintf (stderr, "ERROR: failed to resolve service %s: %s\n",
		 name, avahi_strerror (avahi_client_errno (client)));
      }
    }

    break;

  case AVAHI_BROWSER_REMOVE:
  case AVAHI_BROWSER_ALL_FOR_NOW:
  case AVAHI_BROWSER_CACHE_EXHAUSTED:
    break;
  }
}
#endif /* HAVE_AVAHI */


/*
 * 'find_device()' - Find a device from its name and domain.
 */

static cups_device_t *
find_device (cups_array_t *devices,
	     cups_txt_records_t *txt,
	     cups_device_t *dkey)
{
  cups_device_t	*device;
  char		*ptr;

  for (device = cupsArrayFind(devices, dkey);
       device;
       device = cupsArrayNext(devices))
  {
    if (_cups_strcasecmp(device->name, dkey->name) ||
        _cups_strcasecmp(device->domain, dkey->domain))
    {
      device = NULL;
      break;
    }
    else if (device->type == dkey->type)
    {
     /*
      * Found it, pull out the priority and make and model from the TXT
      * record and save it...
      */

      char		make_and_model[512],
				      	/* Manufacturer and model */
			model[256],	/* Model */
			device_id[2048]; /* 1284 device ID */

      device_id[0]      = '\0';
      make_and_model[0] = '\0';

      strcpy(model, "Unknown");

      for (;;)
      {
	char *key;
	char *value;

	if (parse_txt_record_pair (txt))
	  goto next;

	key = txt->key;
	value = txt->value;
        if (!strncasecmp(key, "usb_", 4))
	{
	 /*
	  * Add USB device ID information...
	  */

	  ptr = device_id + strlen(device_id);
	  snprintf(ptr, sizeof(device_id) - (ptr - device_id), "%s:%s;",
	           key + 4, value);
        }

        if (!_cups_strcasecmp(key, "usb_MFG") || !_cups_strcasecmp(key, "usb_MANU") ||
	    !_cups_strcasecmp(key, "usb_MANUFACTURER"))
	  strcpy(make_and_model, value);
        else if (!_cups_strcasecmp(key, "usb_MDL") || !_cups_strcasecmp(key, "usb_MODEL"))
	  strcpy(model, value);
	else if (!_cups_strcasecmp(key, "product") && !strstr(value, "Ghostscript"))
	{
	  if (value[0] == '(')
	  {
	   /*
	    * Strip parenthesis...
	    */

            if ((ptr = value + strlen(value) - 1) > value && *ptr == ')')
	      *ptr = '\0';

	    strcpy(model, value + 1);
	  }
	  else
	    strcpy(model, value);
        }
	else if (!_cups_strcasecmp(key, "ty"))
	{
          strcpy(model, value);

	  if ((ptr = strchr(model, ',')) != NULL)
	    *ptr = '\0';
	}
	else if (!_cups_strcasecmp(key, "priority"))
	  device->priority = atoi(value);
	else if ((device->type == CUPS_DEVICE_IPP ||
	          device->type == CUPS_DEVICE_IPPS ||
	          device->type == CUPS_DEVICE_PRINTER) &&
		 !_cups_strcasecmp(key, "printer-type"))
	{
	 /*
	  * This is a CUPS printer!
	  */

	  device->cups_shared = 1;

	  if (device->type == CUPS_DEVICE_PRINTER)
	    device->sent = 1;
	}

      next:
	if (next_txt_record (txt) == NULL)
	  break;
      }

      if (device->device_id)
        free(device->device_id);

      if (!device_id[0] && strcmp(model, "Unknown"))
      {
        if (make_and_model[0])
	  snprintf(device_id, sizeof(device_id), "MFG:%s;MDL:%s;",
	           make_and_model, model);
        else if (!_cups_strncasecmp(model, "designjet ", 10))
	  snprintf(device_id, sizeof(device_id), "MFG:HP;MDL:%s", model + 10);
        else if (!_cups_strncasecmp(model, "stylus ", 7))
	  snprintf(device_id, sizeof(device_id), "MFG:EPSON;MDL:%s", model + 7);
        else if ((ptr = strchr(model, ' ')) != NULL)
	{
	 /*
	  * Assume the first word is the make...
	  */

          memcpy(make_and_model, model, ptr - model);
	  make_and_model[ptr - model] = '\0';

	  snprintf(device_id, sizeof(device_id), "MFG:%s;MDL:%s",
		   make_and_model, ptr + 1);
        }
      }

      if (device_id[0])
        device->device_id = strdup(device_id);
      else
        device->device_id = NULL;

      if (device->make_and_model)
	free(device->make_and_model);

      if (make_and_model[0])
      {
	strlcat(make_and_model, " ", sizeof(make_and_model));
	strlcat(make_and_model, model, sizeof(make_and_model));

	device->make_and_model = strdup(make_and_model);
      }
      else
	device->make_and_model = strdup(model);
      break;
    }
  }

  return device;
}

/*
 * 'sigterm_handler()' - Handle termination signals...
 */

static void
sigterm_handler(int sig)		/* I - Signal number (unused) */
{
  (void)sig;

  if (job_canceled)
    exit(CUPS_BACKEND_OK);
  else
    job_canceled = 1;
}


/*
 * 'unquote()' - Unquote a name string.
 */

static void
unquote(char       *dst,		/* I - Destination buffer */
        const char *src,		/* I - Source string */
	size_t     dstsize)		/* I - Size of destination buffer */
{
  char	*dstend = dst + dstsize - 1;	/* End of destination buffer */


  while (*src && dst < dstend)
  {
    if (*src == '\\')
    {
      src ++;
      if (isdigit(src[0] & 255) && isdigit(src[1] & 255) &&
          isdigit(src[2] & 255))
      {
        *dst++ = ((((src[0] - '0') * 10) + src[1] - '0') * 10) + src[2] - '0';
	src += 3;
      }
      else
        *dst++ = *src++;
    }
    else
      *dst++ = *src ++;
  }

  *dst = '\0';
}


/*
 * End of "$Id: dnssd.c 9815 2011-06-05 16:58:33Z mike $".
 */
