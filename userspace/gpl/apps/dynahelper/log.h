#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>

#define ERROR(fmt, ...) do{syslog(LOG_ERR,"[%s:%d] "fmt"\n", \
		__FILE__, __LINE__,##__VA_ARGS__);}while(0)
#define NOTICE(fmt, ...) do{syslog(LOG_NOTICE, fmt"\n",##__VA_ARGS__);}while(0)

#ifdef _DEBUG_
#define INFO(fmt, ...) do{syslog(LOG_INFO, fmt"\n",##__VA_ARGS__);}while(0)
#define DEBUG(fmt, ...) do{syslog(LOG_DEBUG, fmt"\n",##__VA_ARGS__);}while(0)
#else
#define INFO(fmt, ...)
#define DEBUG(fmt, ...)
#endif

#endif
