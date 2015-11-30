#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdio.h>
#include "cms.h"
#include "cms_eid.h"
#include "cms_util.h"

#define OPEN_LOG(name) do {if (!strcmp(name, "udhcpc")) cmsLog_init(EID_DHCPC); else cmsLog_init(EID_DHCPD);} while (0)
#define CLOSE_LOG() cmsLog_cleanup();

#define LOG(level, str, args...) do{if(level==LOG_ERR) cmsLog_error(str, ## args); else cmsLog_debug(str, ## args);} while (0)


#ifdef VERBOSE
# define DEBUG(level, str, args...) LOG(level, str, ## args)
#else
# define DEBUG(level, str, args...) do {;} while(0)
#endif


#endif  /* _DEBUG_H */
