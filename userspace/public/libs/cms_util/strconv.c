/***********************************************************************
 *
 *  Copyright (c) 2007  Broadcom Corporation
 *  All Rights Reserved
 *
# 
# 
# Unless you and Broadcom execute a separate written software license 
# agreement governing use of this software, this software is licensed 
# to you under the terms of the GNU General Public License version 2 
# (the "GPL"), available at http://www.broadcom.com/licenses/GPLv2.php, 
# with the following added to such license:
# 
#    As a special exception, the copyright holders of this software give 
#    you permission to link this software with independent modules, and 
#    to copy and distribute the resulting executable under terms of your 
#    choice, provided that you also meet, for each linked independent 
#    module, the terms and conditions of the license of that module. 
#    An independent module is a module which is not derived from this
#    software.  The special exception does not apply to any modifications 
#    of the software.  
# 
# Not withstanding the above, under no circumstances may you combine 
# this software in any way with any other Broadcom software provided 
# under a license other than the GPL, without Broadcom's express prior 
# written consent. 
#
 * 
 ************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>     /* for isDigit, really should be in oal_strconv.c */
#include <sys/stat.h>  /* this really should be in oal_strconv.c */
#include <arpa/inet.h> /* for inet_aton */


#include "cms_util.h"
#include "oal.h"

UBOOL8 cmsUtl_isValidVpiVci(SINT32 vpi, SINT32 vci)
{
   if (vpi >= VPI_MIN && vpi <= VPI_MAX && vci >= VCI_MIN && vci <= VCI_MAX)
   {
      return TRUE;
   }
   
   cmsLog_error("invalid vpi/vci %d/%d", vpi, vci);
   return FALSE;
}

CmsRet cmsUtl_atmVpiVciStrToNum(const char *vpiVciStr, SINT32 *vpi, SINT32 *vci)
{
   char *pSlash;
   char vpiStr[BUFLEN_256];
   char vciStr[BUFLEN_256];
   char *prefix;
   
   *vpi = *vci = -1;   
   if (vpiVciStr == NULL)
   {
      cmsLog_error("vpiVciStr is NULL");
      return CMSRET_INVALID_ARGUMENTS;
   }      

   strncpy(vpiStr, vpiVciStr, sizeof(vpiStr));

   if (strstr(vpiStr, DSL_LINK_DESTINATION_PREFIX_SVC))
   {
      cmsLog_error("DesitinationAddress string %s with %s is not supported yet.", vpiStr, DSL_LINK_DESTINATION_PREFIX_SVC);
      return CMSRET_INVALID_PARAM_VALUE;
   }

   if ((prefix = strstr(vpiStr, DSL_LINK_DESTINATION_PREFIX_PVC)) == NULL)
   {
      cmsLog_error("Invalid DesitinationAddress string %s", vpiStr);
      return CMSRET_INVALID_PARAM_VALUE;
   }
 
   /* skip the prefix */
#if 0
   prefix += sizeof(DSL_LINK_DESTINATION_PREFIX_PVC);
#endif
   prefix += strlen(DSL_LINK_DESTINATION_PREFIX_PVC);
   /* skip the blank if there is one */
   if (*prefix == ' ')
   {
      prefix += 1;
   }

   pSlash = (char *) strchr(prefix, '/');
   if (pSlash == NULL)
   {
      cmsLog_error("vpiVciStr %s is invalid", vpiVciStr);
      return CMSRET_INVALID_ARGUMENTS;
   }
   strncpy(vciStr, (pSlash + 1), sizeof(vciStr));
   *pSlash = '\0';       
   *vpi = atoi(prefix);
   *vci = atoi(vciStr);
   if (cmsUtl_isValidVpiVci(*vpi, *vci) == FALSE)
   {
      return CMSRET_INVALID_PARAM_VALUE;
   }     

   return CMSRET_SUCCESS;
   
}


CmsRet cmsUtl_atmVpiVciNumToStr(const SINT32 vpi, const SINT32 vci, char *vpiVciStr)
{
   if (vpiVciStr == NULL)
   {
      cmsLog_error("vpiVciStr is NULL");
      return CMSRET_INVALID_ARGUMENTS;
   }         
   if (cmsUtl_isValidVpiVci(vpi, vci) == FALSE)
   {
      return CMSRET_INVALID_PARAM_VALUE;
   }     

   sprintf(vpiVciStr, "%s %d/%d", DSL_LINK_DESTINATION_PREFIX_PVC, vpi, vci);

   return CMSRET_SUCCESS;
   
}


CmsRet cmsUtl_macStrToNum(const char *macStr, UINT8 *macNum) 
{
   char *pToken = NULL;
   char *pLast = NULL;
   char *buf;
   SINT32 i;
   
   if (macNum == NULL || macStr == NULL) 
   {
      cmsLog_error("Invalid macNum/macStr %p/%p", macNum, macStr);
      return CMSRET_INVALID_ARGUMENTS;
   }    
   
   if (cmsUtl_isValidMacAddress(macStr) == FALSE)
   {
      return CMSRET_INVALID_PARAM_VALUE;
   }   
   
   if ((buf = (char *) cmsMem_alloc(MAC_STR_LEN+1, ALLOC_ZEROIZE)) == NULL)
   {
      cmsLog_error("alloc of %d bytes failed", MAC_STR_LEN+1);
      return CMSRET_RESOURCE_EXCEEDED;
   }

   /* need to copy since strtok_r updates string */
   strcpy(buf, macStr);

   /* Mac address has the following format
    * xx:xx:xx:xx:xx:xx where x is hex number 
    */
   pToken = strtok_r(buf, ":", &pLast);
   macNum[0] = (UINT8) strtol(pToken, (char **)NULL, 16);
   for (i = 1; i < MAC_ADDR_LEN; i++) 
   {
      pToken = strtok_r(NULL, ":", &pLast);
      macNum[i] = (UINT8) strtol(pToken, (char **)NULL, 16);
   }

   cmsMem_free(buf);

   return CMSRET_SUCCESS;
   
}

CmsRet cmsUtl_macNumToStr(const UINT8 *macNum, char *macStr) 
{
   if (macNum == NULL || macStr == NULL) 
   {
      cmsLog_error("Invalid macNum/macStr %p/%p", macNum, macStr);
      return CMSRET_INVALID_ARGUMENTS;
   }  

   sprintf(macStr, "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
           (UINT8) macNum[0], (UINT8) macNum[1], (UINT8) macNum[2],
           (UINT8) macNum[3], (UINT8) macNum[4], (UINT8) macNum[5]);

   return CMSRET_SUCCESS;
}


CmsRet cmsUtl_strtol(const char *str, char **endptr, SINT32 base, SINT32 *val)
{
   return(oal_strtol(str, endptr, base, val));
}


CmsRet cmsUtl_strtoul(const char *str, char **endptr, SINT32 base, UINT32 *val)
{
   return(oal_strtoul(str, endptr, base, val));
}


CmsRet cmsUtl_strtol64(const char *str, char **endptr, SINT32 base, SINT64 *val)
{
   return(oal_strtol64(str, endptr, base, val));
}


CmsRet cmsUtl_strtoul64(const char *str, char **endptr, SINT32 base, UINT64 *val)
{
   return(oal_strtoul64(str, endptr, base, val));
}


void cmsUtl_strToLower(char *string)
{
   char *ptr = string;
   for (ptr = string; *ptr; ptr++)
   {
       *ptr = tolower(*ptr);
   }
}

CmsRet cmsUtl_parseUrl(const char *url, UrlProto *proto, char **addr, UINT16 *port, char **path)
{
   int n = 0;
   char *p = NULL;
   char protocol[BUFLEN_16];
   char host[BUFLEN_1024];
   char uri[BUFLEN_1024];

   if (url == NULL)
   {
      cmsLog_debug("url is NULL");
      return CMSRET_INVALID_ARGUMENTS;
   }

  *port = 0;
   protocol[0] = host[0]  = uri[0] = '\0';

   /* proto */
   p = (char *) url;
   if ((p = strchr(url, ':')) == NULL) 
   {
      return CMSRET_INVALID_ARGUMENTS;
   }
   n = p - url;
   strncpy(protocol, url, n);
   protocol[n] = '\0';

   if (!strcmp(protocol, "http"))
   {
      *proto = URL_PROTO_HTTP;
   }
   else if (!strcmp(protocol, "https"))
   {
      *proto = URL_PROTO_HTTPS;
   }
   else if (!strcmp(protocol, "ftp"))
   {
      *proto = URL_PROTO_FTP;
   }
   else if (!strcmp(protocol, "tftp"))
   {
      *proto = URL_PROTO_TFTP;
   }
   else
   {
      cmsLog_error("unrecognized proto in URL %s", url);
      return CMSRET_INVALID_ARGUMENTS;
   }

   /* skip "://" */
   if (*p++ != ':') return CMSRET_INVALID_ARGUMENTS;
   if (*p++ != '/') return CMSRET_INVALID_ARGUMENTS;
   if (*p++ != '/') return CMSRET_INVALID_ARGUMENTS;

   /* host */
   {
      char *pHost = host;
    
      while (*p && *p != ':' && *p != '/') 
      {
         *pHost++ = *p++;
      }
      *pHost = '\0';
   }
   if (strlen(host) != 0)
   {
      *addr = cmsMem_strdup(host);
   }
   else
   {
      cmsLog_error("unrecognized host in URL %s", url);
      return CMSRET_INVALID_ARGUMENTS;
   }

   /* end */
   if (*p == '\0') 
   {
      *path = cmsMem_strdup("/");
       return CMSRET_SUCCESS;
   }

   /* port */
   if (*p == ':') 
   {
      char buf[BUFLEN_16];
      char *pBuf = buf;

      p++;
      while (isdigit(*p)) 
      {
         *pBuf++ = *p++;
      }
      *pBuf = '\0';
      if (strlen(buf) == 0)
      {
         CMSMEM_FREE_BUF_AND_NULL_PTR(*addr);
         cmsLog_error("unrecognized port in URL %s", url);
         return CMSRET_INVALID_ARGUMENTS;
      }
      *port = atoi(buf);
   }
  
   /* path */
   if (*p == '/') 
   {
      char *pUri = uri;

      while ((*pUri++ = *p++));
      *path = cmsMem_strdup(uri);  
   }
   else
   {
      *path = cmsMem_strdup("/");
   }

   return CMSRET_SUCCESS;
}

CmsRet cmsUtl_getBaseDir(char *pathBuf, UINT32 pathBufLen)
{
   UINT32 rc;

#ifdef DESKTOP_LINUX
   char pwd[BUFLEN_1024]={0};
   UINT32 pwdLen = sizeof(pwd);
   char *str;
   char *envDir;
   struct stat statbuf;

   getcwd(pwd, pwdLen);
   if (strlen(pwd) == pwdLen - 1)
   {
      return CMSRET_INTERNAL_ERROR;
   }

   str = strstr(pwd, "userspace");
   if (str == NULL)
   {
      str = strstr(pwd, "unittests");
   }

   if (str != NULL)
   {
      /*
       * OK, we are running from under userspace.
       * null terminate the string right before userspace and that
       * should give us the basedir.
       */
      str--;
      *str = 0;

      rc = snprintf(pathBuf, pathBufLen, "%s", pwd);
   }
   else
   {
      /* try to figure out location of CommEngine from env var */
      if ((envDir = getenv("CMS_BASE_DIR")) != NULL)
      {
         snprintf(pwd, sizeof(pwd), "%s/unittests", envDir);
         if ((rc = stat(pwd, &statbuf)) == 0)
         {
            /* env var is good, use it. */
            rc = snprintf(pathBuf, pathBufLen, "%s", envDir);
         }
         else
         {
            /* CMS_BASE_DIR is set, but points to bad location */
            return CMSRET_INVALID_ARGUMENTS;
         }
      }
      else
      {
         /* not running from under CommEngine and also no CMS_BASE_DIR */
         return CMSRET_INVALID_ARGUMENTS;
      }
   }


#else

   rc = snprintf(pathBuf, pathBufLen, "/var");

#endif /* DESKTOP_LINUX */

   if (rc >= pathBufLen)
   {
      return CMSRET_RESOURCE_EXCEEDED;
   }

   return CMSRET_SUCCESS;
}

CmsRet cmsUtl_parseDNS(const char *inDsnServers, char *outDnsPrimary, char *outDnsSecondary)
{
   CmsRet ret = CMSRET_SUCCESS;
   char *tmpBuf;
   char *separator;
   UINT32 len;

   if (inDsnServers == NULL)
   {
      return CMSRET_INVALID_ARGUMENTS;
   }      
   

   cmsLog_debug("entered: DDNSservers=>%s<=", inDsnServers);

   if (outDnsPrimary)
   {
      strcpy(outDnsPrimary, "0.0.0.0");
   }

   if (outDnsSecondary)
   {
      strcpy(outDnsSecondary, "0.0.0.0");
   }
   
   len = strlen(inDsnServers);

   if ((tmpBuf = cmsMem_alloc(len+1, 0)) == NULL)
   {
      cmsLog_error("alloc of %d bytes failed", len);
      ret = CMSRET_INTERNAL_ERROR;
   }
   else
   {
      sprintf(tmpBuf, "%s", inDsnServers);
      separator = strstr(tmpBuf, ",");
      if (separator != NULL)
      {
         /* break the string into two strings */
         *separator = 0;
         separator++;
         while ((isspace(*separator)) && (*separator != 0))
         {
            /* skip white space after comma */
            separator++;
         }

         if (outDnsSecondary != NULL)
         {
            if (cmsUtl_isValidIpv4Address(separator))
            {
               strcpy(outDnsSecondary, separator);
            }
            cmsLog_debug("dnsSecondary=%s", outDnsSecondary);
         }
      }

      if (outDnsPrimary != NULL)
      {
         if (cmsUtl_isValidIpv4Address(tmpBuf))
         {
            strcpy(outDnsPrimary, tmpBuf);
         }
         cmsLog_debug("dnsPrimary=%s", outDnsPrimary);
      }

      cmsMem_free(tmpBuf);
   }

   return ret;
   
}


SINT32 cmsUtl_syslogModeToNum(const char *modeStr)
{
   SINT32 mode=1;

   /*
    * These values are hard coded in httpd/html/logconfig.html.
    * Any changes to these values must also be reflected in that file.
    */
   if (!strcmp(modeStr, MDMVS_LOCAL_BUFFER))
   {
      mode = 1;
   }
   else if (!strcmp(modeStr, MDMVS_REMOTE))
   {
      mode = 2;
   }
   else if (!strcmp(modeStr, MDMVS_LOCAL_BUFFER_AND_REMOTE))
   {
      mode = 3;
   }
   else 
   {
      cmsLog_error("unsupported mode string %s, default to mode=%d", modeStr, mode);
   }

   /*
    * The data model also specifies LOCAL_FILE and LOCAL_FILE_AND_REMOTE,
    * but its not clear if syslogd actually supports local file mode.
    */

   return mode;
}


char * cmsUtl_numToSyslogModeString(SINT32 mode)
{
   char *modeString = MDMVS_LOCAL_BUFFER;

   /*
    * These values are hard coded in httpd/html/logconfig.html.
    * Any changes to these values must also be reflected in that file.
    */
   switch(mode)
   {
   case 1:
      modeString = MDMVS_LOCAL_BUFFER;
      break;

   case 2:
      modeString = MDMVS_REMOTE;
      break;

   case 3:
      modeString = MDMVS_LOCAL_BUFFER_AND_REMOTE;
      break;

   default:
      cmsLog_error("unsupported mode %d, default to %s", mode, modeString);
      break;
   }

   /*
    * The data model also specifies LOCAL_FILE and LOCAL_FILE_AND_REMOTE,
    * but its not clear if syslogd actually supports local file mode.
    */

   return modeString;
}


UBOOL8 cmsUtl_isValidSyslogMode(const char * modeStr)
{
   UINT32 mode;

   if (cmsUtl_strtoul(modeStr, NULL, 10, &mode) != CMSRET_SUCCESS) 
   {
      return FALSE;
   }

   return ((mode >= 1) && (mode <= 3));
}


SINT32 cmsUtl_syslogLevelToNum(const char *levelStr)
{
   SINT32 level=3; /* default all levels to error */

   /*
    * These values are from /usr/include/sys/syslog.h.
    */
   if (!strcmp(levelStr, MDMVS_EMERGENCY))
   {
      level = 0;
   }
   else if (!strcmp(levelStr, MDMVS_ALERT))
   {
      level = 1;
   }
   else if (!strcmp(levelStr, MDMVS_CRITICAL))
   {
      level = 2;
   }
   else if (!strcmp(levelStr, MDMVS_ERROR))
   {
      level = 3;
   }
   else if (!strcmp(levelStr, MDMVS_WARNING))
   {
      level = 4;
   }
   else if (!strcmp(levelStr, MDMVS_NOTICE))
   {
      level = 5;
   }
   else if (!strcmp(levelStr, MDMVS_INFORMATIONAL))
   {
      level = 6;
   }
   else if (!strcmp(levelStr, MDMVS_DEBUG))
   {
      level = 7;
   }
   else 
   {
      cmsLog_error("unsupported level string %s, default to level=%d", levelStr, level);
   }

   return level;
}


char * cmsUtl_numToSyslogLevelString(SINT32 level)
{
   char *levelString = MDMVS_ERROR;

   /*
    * These values come from /usr/include/sys/syslog.h.
    */
   switch(level)
   {
   case 0:
      levelString = MDMVS_EMERGENCY;
      break;

   case 1:
      levelString = MDMVS_ALERT;
      break;

   case 2:
      levelString = MDMVS_CRITICAL;
      break;

   case 3:
      levelString = MDMVS_ERROR;
      break;

   case 4:
      levelString = MDMVS_WARNING;
      break;

   case 5:
      levelString = MDMVS_NOTICE;
      break;

   case 6:
      levelString = MDMVS_INFORMATIONAL;
      break;

   case 7:
      levelString = MDMVS_DEBUG;
      break;

   default:
      cmsLog_error("unsupported level %d, default to %s", level, levelString);
      break;
   }

   return levelString;
}


UBOOL8 cmsUtl_isValidSyslogLevel(const char *levelStr)
{
   UINT32 level;

   if (cmsUtl_strtoul(levelStr, NULL, 10, &level) != CMSRET_SUCCESS) 
   {
      return FALSE;
   }

   return (level <= 7);
}


UBOOL8 cmsUtl_isValidSyslogLevelString(const char *levelStr)
{
   if ((!strcmp(levelStr, MDMVS_EMERGENCY)) ||
       (!strcmp(levelStr, MDMVS_ALERT)) ||
       (!strcmp(levelStr, MDMVS_CRITICAL)) ||
       (!strcmp(levelStr, MDMVS_ERROR)) ||
       (!strcmp(levelStr, MDMVS_WARNING)) ||
       (!strcmp(levelStr, MDMVS_NOTICE)) ||
       (!strcmp(levelStr, MDMVS_INFORMATIONAL)) ||
       (!strcmp(levelStr, MDMVS_DEBUG)))
   {
      return TRUE;
   }
   else
   {
      return FALSE;
   }
}


SINT32 cmsUtl_pppAuthToNum(const char *authStr)
{
   SINT32 authNum = PPP_AUTH_METHOD_AUTO;  /* default is auto  */

   if (!strcmp(authStr, MDMVS_AUTO_AUTH))
   {
      authNum = PPP_AUTH_METHOD_AUTO;
   }
   else if (!strcmp(authStr, MDMVS_PAP))
   {
      authNum = PPP_AUTH_METHOD_PAP;
   }
   else if (!strcmp(authStr, MDMVS_CHAP))
   {
       authNum = PPP_AUTH_METHOD_CHAP;
   }
   else if (!strcmp(authStr, MDMVS_MS_CHAP))
   {
         authNum = PPP_AUTH_METHOD_MSCHAP;
   }
   else 
   {
      cmsLog_error("unsupported auth string %s, default to auto=%d", authStr, authNum);
   }

   return authNum;
   
}


char * cmsUtl_numToPppAuthString(SINT32 authNum)
{
   char *authStr = MDMVS_AUTO_AUTH;   /* default to auto */

   switch(authNum)
   {
   case PPP_AUTH_METHOD_AUTO:
      authStr = MDMVS_AUTO_AUTH;
      break;

   case PPP_AUTH_METHOD_PAP:
      authStr = MDMVS_PAP;
      break;

   case PPP_AUTH_METHOD_CHAP:
      authStr = MDMVS_CHAP;
      break;

   case PPP_AUTH_METHOD_MSCHAP:
      authStr = MDMVS_MS_CHAP; 
      break;

   default:
      cmsLog_error("unsupported authNum %d, default to %s", authNum, authStr);
      break;
   }

   return authStr;
   
}


CmsLogLevel cmsUtl_logLevelStringToEnum(const char *logLevel)
{
   if (!strcmp(logLevel, MDMVS_ERROR))
   {
      return LOG_LEVEL_ERR;
   }
   else if (!strcmp(logLevel, MDMVS_NOTICE))
   {
      return LOG_LEVEL_NOTICE;
   }
   else if (!strcmp(logLevel, MDMVS_DEBUG))
   {
      return LOG_LEVEL_DEBUG;
   }
   else
   {
      cmsLog_error("unimplemented log level %s", logLevel);
      return DEFAULT_LOG_LEVEL;
   }
}


CmsLogDestination cmsUtl_logDestinationStringToEnum(const char *logDest)
{
   if (!strcmp(logDest, MDMVS_STANDARD_ERROR))
   {
      return LOG_DEST_STDERR;
   }
   else if (!strcmp(logDest, MDMVS_SYSLOG))
   {
      return LOG_DEST_SYSLOG;
   }
   else if (!strcmp(logDest, MDMVS_TELNET))
   {
      return LOG_DEST_TELNET;
   }
   else
   {
      cmsLog_error("unimplemented log dest %s", logDest);
      return DEFAULT_LOG_DESTINATION;
   }
}


UBOOL8 cmsUtl_isValidIpAddress(SINT32 af, const char* address)
{
#ifdef DMP_X_BROADCOM_COM_IPV6_1 /* aka SUPPORT_IPV6 */
   if (af == AF_INET6)
   {
      struct in6_addr in6Addr;
      UINT32 plen;
      char   addr[BUFLEN_40];

      if (cmsUtl_parsePrefixAddress(address, addr, &plen) != CMSRET_SUCCESS)
      {
         cmsLog_error("Invalid ipv6 address=%s", address);
         return FALSE;
      }

      if (inet_pton(AF_INET6, addr, &in6Addr) <= 0)
      {
         cmsLog_error("Invalid ipv6 address=%s", address);
         return FALSE;
      }

      if ( in6Addr.s6_addr32[0] == 0 && in6Addr.s6_addr32[1] == 0 && in6Addr.s6_addr32[2] == 0 &&
          (in6Addr.s6_addr32[3] == 0 || in6Addr.s6_addr32[3] == 1))
      {
         cmsLog_error("Unspecified or loopback ipv6 address=%s", address);
         return FALSE;
      }

      return TRUE;
   }
   else
#endif
   {
      if (af == AF_INET)
      {
         return cmsUtl_isValidIpv4Address(address);
      }
      else
      {
         return FALSE;
      }
   }
}  /* End of cmsUtl_isValidIpAddress() */

UBOOL8 cmsUtl_isValidIpv4Address(const char* input)
{
   UBOOL8 ret = TRUE;
   char *pToken = NULL;
   char *pLast = NULL;
   char buf[BUFLEN_16];
   UINT32 i, num;

   if (input == NULL || strlen(input) < 7 || strlen(input) > 15)
   {
      return FALSE;
   }

   /* need to copy since strtok_r updates string */
   strcpy(buf, input);

   /* IP address has the following format
      xxx.xxx.xxx.xxx where x is decimal number */
   pToken = strtok_r(buf, ".", &pLast);
   if ((cmsUtl_strtoul(pToken, NULL, 10, &num) != CMSRET_SUCCESS) ||
       (num > 255))
   {
      ret = FALSE;
   }
   else
   {
      for ( i = 0; i < 3; i++ )
      {
         pToken = strtok_r(NULL, ".", &pLast);

         if ((cmsUtl_strtoul(pToken, NULL, 10, &num) != CMSRET_SUCCESS) ||
             (num > 255))
         {
            ret = FALSE;
            break;
         }
      }
   }

   return ret;
}



UBOOL8 cmsUtl_isValidMacAddress(const char* input)
{
   UBOOL8 ret =  TRUE;
   char *pToken = NULL;
   char *pLast = NULL;
   char buf[BUFLEN_32];
   UINT32 i, num;

   if (input == NULL || strlen(input) != MAC_STR_LEN)
   {
      return FALSE;
   }

   /* need to copy since strtok_r updates string */
   strcpy(buf, input);

   /* Mac address has the following format
       xx:xx:xx:xx:xx:xx where x is hex number */
   pToken = strtok_r(buf, ":", &pLast);
   if ((strlen(pToken) != 2) ||
       (cmsUtl_strtoul(pToken, NULL, 16, &num) != CMSRET_SUCCESS))
   {
      ret = FALSE;
   }
   else
   {
      for ( i = 0; i < 5; i++ )
      {
         pToken = strtok_r(NULL, ":", &pLast);
         if ((strlen(pToken) != 2) ||
             (cmsUtl_strtoul(pToken, NULL, 16, &num) != CMSRET_SUCCESS))
         {
            ret = FALSE;
            break;
         }
      }
   }

   return ret;
}


UBOOL8 cmsUtl_isValidPortNumber(const char * portNumberStr)
{
   UINT32 portNum;

   if (cmsUtl_strtoul(portNumberStr, NULL, 10, &portNum) != CMSRET_SUCCESS) 
   {
      return FALSE;
   }

   return (portNum < (64 * 1024));
}


SINT32 cmsUtl_strcmp(const char *s1, const char *s2) 
{
   char emptyStr = '\0';
   char *str1 = (char *) s1;
   char *str2 = (char *) s2;

   if (str1 == NULL)
   {
      str1 = &emptyStr;
   }
   if (str2 == NULL)
   {
      str2 = &emptyStr;
   }

   return strcmp(str1, str2);
}


SINT32 cmsUtl_strcasecmp(const char *s1, const char *s2) 
{
   char emptyStr = '\0';
   char *str1 = (char *) s1;
   char *str2 = (char *) s2;

   if (str1 == NULL)
   {
      str1 = &emptyStr;
   }
   if (str2 == NULL)
   {
      str2 = &emptyStr;
   }

   return strcasecmp(str1, str2);
}


SINT32 cmsUtl_strncmp(const char *s1, const char *s2, SINT32 n) 
{
   char emptyStr = '\0';
   char *str1 = (char *) s1;
   char *str2 = (char *) s2;

   if (str1 == NULL)
   {
      str1 = &emptyStr;
   }
   if (str2 == NULL)
   {
      str2 = &emptyStr;
   }

   return strncmp(str1, str2, n);
}


SINT32 cmsUtl_strncasecmp(const char *s1, const char *s2, SINT32 n) 
{
   char emptyStr = '\0';
   char *str1 = (char *) s1;
   char *str2 = (char *) s2;

   if (str1 == NULL)
   {
      str1 = &emptyStr;
   }
   if (str2 == NULL)
   {
      str2 = &emptyStr;
   }

   return strncasecmp(str1, str2, n);
}


char *cmsUtl_strstr(const char *s1, const char *s2) 
{
   char emptyStr = '\0';
   char *str1 = (char *)s1;
   char *str2 = (char *)s2;

   if (str1 == NULL)
   {
      str1 = &emptyStr;
   }
   if (str2 == NULL)
   {
      str2 = &emptyStr;
   }

   return strstr(str1, str2);
}

char *cmsUtl_strncpy(char *dest, const char *src, SINT32 dlen)
{

   if((src == NULL) || (dest == NULL))
   {
      cmsLog_error("null pointer reference src =%u ,dest =%u", src, dest);
   }	

   if( strlen(src)+1 > dlen )
   {
      cmsLog_notice("truncating:src string length > dest buffer");
      strncpy(dest,src,dlen-1);
      dest[dlen-1] ='\0';
   }
   else
   {
      strcpy(dest,src);
   }
   return dest;
} 

SINT32 cmsUtl_strlen(const char *src)
{
   char emptyStr = '\0';
   char *str = (char *)src;
   
   if(src == NULL)
   {
      str = &emptyStr;
   }	

   return strlen(str);
} 


UBOOL8 cmsUtl_isSubOptionPresent(const char *fullOptionString, const char *subOption)
{
   const char *startChar, *currChar;
   UINT32 len=0;
   UBOOL8 found=FALSE;

   cmsLog_debug("look for subOption %s in fullOptionString=%s", subOption, fullOptionString);

   if (fullOptionString == NULL || subOption == NULL)
   {
      return FALSE;
   }

   startChar = fullOptionString;
   currChar = startChar;

   while (!found && *currChar != '\0')
   {
      /* get to the end of the current subOption */
      while (*currChar != ' ' && *currChar != ',' && *currChar != '\0')
      {
         currChar++;
         len++;
      }

      /* compare the current subOption with the subOption that was specified */
      if ((len == strlen(subOption)) &&
          (0 == strncmp(subOption, startChar, len)))
      {
         found = TRUE;
      }

      /* advance to the start of the next subOption */
      if (*currChar != '\0')
      {
         while (*currChar == ' ' || *currChar == ',')
         {
            currChar++;
         }

         len = 0;
         startChar = currChar;
      }
   }

   cmsLog_debug("found=%d", found);
   return found;
}


void cmsUtl_getWanProtocolName(UINT8 protocol, char *name) 
{
    if ( name == NULL ) 
      return;

    name[0] = '\0';
       
    switch ( protocol ) 
    {
        case CMS_WAN_TYPE_PPPOE:
            strcpy(name, "PPPoE");
            break;
        case CMS_WAN_TYPE_PPPOA:
            strcpy(name, "PPPoA");
            break;
        case CMS_WAN_TYPE_DYNAMIC_IPOE:
        case CMS_WAN_TYPE_STATIC_IPOE:
            strcpy(name, "IPoE");
            break;
        case CMS_WAN_TYPE_IPOA:
            strcpy(name, "IPoA");
            break;
        case CMS_WAN_TYPE_BRIDGE:
            strcpy(name, "Bridge");
            break;
#if SUPPORT_ETHWAN
        case CMS_WAN_TYPE_DYNAMIC_ETHERNET_IP:
            strcpy(name, "IPoW");
            break;
#endif
        default:
            strcpy(name, "Not Applicable");
            break;
    }
}

char *cmsUtl_getAggregateStringFromDhcpVendorIds(const char *vendorIds)
{
   char *aggregateString;
   const char *vendorId;
   UINT32 i, count=0;

   if (vendorIds == NULL)
   {
      return NULL;
   }

   aggregateString = cmsMem_alloc(MAX_PORTMAPPING_DHCP_VENDOR_IDS * (DHCP_VENDOR_ID_LEN + 1), ALLOC_ZEROIZE);
   if (aggregateString == NULL)
   {
      cmsLog_error("allocation of aggregate string failed");
      return NULL;
   }

   for (i=0; i < MAX_PORTMAPPING_DHCP_VENDOR_IDS; i++)
   {
      vendorId = &(vendorIds[i * (DHCP_VENDOR_ID_LEN + 1)]);
      if (*vendorId != '\0')
      {
         if (count > 0)
         {
            strcat(aggregateString, ",");
         }
         /* strncat writes at most DHCP_VENDOR_ID_LEN+1 bytes, which includes the trailing NULL */
         strncat(aggregateString, vendorId, DHCP_VENDOR_ID_LEN);
        
         count++;
      }
   }

   return aggregateString;
}


char *cmsUtl_getDhcpVendorIdsFromAggregateString(const char *aggregateString)
{
   char *vendorIds, *vendorId, *ptr, *savePtr=NULL;
   char *copy;
   UINT32 count=0;

   if (aggregateString == NULL)
   {
      return NULL;
   }

   vendorIds = cmsMem_alloc(MAX_PORTMAPPING_DHCP_VENDOR_IDS * (DHCP_VENDOR_ID_LEN + 1), ALLOC_ZEROIZE);
   if (vendorIds == NULL)
   {
      cmsLog_error("allocation of vendorIds buffer failed");
      return NULL;
   }

   copy = cmsMem_strdup(aggregateString);
   ptr = strtok_r(copy, ",", &savePtr);
   while ((ptr != NULL) && (count < MAX_PORTMAPPING_DHCP_VENDOR_IDS))
   {
      vendorId = &(vendorIds[count * (DHCP_VENDOR_ID_LEN + 1)]);
      /*
       * copy at most DHCP_VENDOR_ID_LEN bytes.  Since each chunk in the linear
       * buffer is DHCP_VENDOR_ID_LEN+1 bytes long and initialized to 0,
       * we are guaranteed that each vendor id is null terminated.
       */
      strncpy(vendorId, ptr, DHCP_VENDOR_ID_LEN);
      count++;

      ptr = strtok_r(NULL, ",", &savePtr);
   }

   cmsMem_free(copy);
   
   return vendorIds;
}


ConnectionModeType cmsUtl_connectionModeStrToNum(const char *connModeStr)
{
   ConnectionModeType connMode = CMS_CONNECTION_MODE_DEFAULT;
   if (connModeStr == NULL)
   {
      cmsLog_error("connModeStr is NULL");
      return connMode;
   }

   if (cmsUtl_strcmp(connModeStr, MDMVS_VLANMUXMODE) == 0)
   {
      connMode = CMS_CONNECTION_MODE_VLANMUX;
   }
   else if (cmsUtl_strcmp(connModeStr, MDMVS_MULTIPLESERVICEMODE) == 0)
   {
      connMode = CMS_CONNECTION_MODE_MSC;
   }
   return connMode;

}


#ifdef DMP_X_BROADCOM_COM_IPV6_1 /* aka SUPPORT_IPV6 */
CmsRet cmsUtl_standardizeIp6Addr(const char *address, char *stdAddr)
{
   struct in6_addr in6Addr;
   UINT32 plen;
   char   addr[BUFLEN_40];

   if (address == NULL || stdAddr == NULL)
   {
      return CMSRET_INVALID_ARGUMENTS;
   }

   if (cmsUtl_parsePrefixAddress(address, addr, &plen) != CMSRET_SUCCESS)
   {
      cmsLog_error("Invalid ipv6 address=%s", address);
      return CMSRET_INVALID_ARGUMENTS;
   }

   if (inet_pton(AF_INET6, addr, &in6Addr) <= 0)
   {
      cmsLog_error("Invalid ipv6 address=%s", address);
      return CMSRET_INVALID_ARGUMENTS;
   }

   inet_ntop(AF_INET6, &in6Addr, stdAddr, BUFLEN_40);

   if (strchr(address, '/') != NULL)
   {
      char prefix[BUFLEN_8];

      sprintf(prefix, "/%d", plen);
      strcat(stdAddr, prefix);
   }

   return CMSRET_SUCCESS;

}  /* End of cmsUtl_standardizeIp6Addr() */

UBOOL8 cmsUtl_isGlobalUnicastAddress(const char *address)
{
   struct in6_addr in6Addr;

   if (inet_pton(AF_INET6, address, &in6Addr) <= 0)
   {
      cmsLog_error("Invalid ipv6 address=%s", address);
      return FALSE;
   }

   /* currently IANA assigned global unicast address prefix is 001..... */
   if ((in6Addr.s6_addr[0] & 0x20) && !(in6Addr.s6_addr[0] & 0xc0))
   {
      return TRUE;
   }

   return FALSE;

}  /* End of cmsUtl_isGlobalUnicastAddress() */

CmsRet cmsUtl_getAddrPrefix(const char *address, UINT32 plen, char *prefix)
{
   struct in6_addr   in6Addr;
   UINT16 i, k, mask;

   if (plen > 128)
   {
      cmsLog_error("Invalid plen=%d", plen);
      return CMSRET_INVALID_ARGUMENTS;
   }
   if (inet_pton(AF_INET6, address, &in6Addr) <= 0)
   {
      cmsLog_error("Invalid address=%s", address);
      return CMSRET_INVALID_ARGUMENTS;
   }

   k = plen / 16;
   mask = 0;
   if (plen % 16)
   {
      mask = ~(UINT16)(((1 << (16 - (plen % 16))) - 1) & 0xFFFF);
   }

   in6Addr.s6_addr16[k] &= mask;
   
   for (i = k+1; i < 8; i++)
   {
      in6Addr.s6_addr16[i] = 0;
   } 
   
   if (inet_ntop(AF_INET6, &in6Addr, prefix, BUFLEN_40) == NULL)
   {
      cmsLog_error("inet_ntop returns NULL");
      return CMSRET_INTERNAL_ERROR;
   }

   return CMSRET_SUCCESS; 
   
}  /* End of cmsUtl_getAddrPrefix() */

CmsRet cmsUtl_replaceEui64(const char *address1, char *address2)
{
   struct in6_addr   in6Addr1, in6Addr2;

   if (inet_pton(AF_INET6, address1, &in6Addr1) <= 0)
   {
      cmsLog_error("Invalid address=%s", address1);
      return CMSRET_INVALID_ARGUMENTS;
   }
   if (inet_pton(AF_INET6, address2, &in6Addr2) <= 0)
   {
      cmsLog_error("Invalid address=%s", address2);
      return CMSRET_INVALID_ARGUMENTS;
   }

   in6Addr2.s6_addr32[2] = in6Addr1.s6_addr32[2];
   in6Addr2.s6_addr32[3] = in6Addr1.s6_addr32[3];

   if (inet_ntop(AF_INET6, &in6Addr2, address2, BUFLEN_40) == NULL)
   {
      cmsLog_error("inet_ntop returns NULL");
      return CMSRET_INTERNAL_ERROR;
   }

   return CMSRET_SUCCESS;
      
}  /* End of cmsUtl_replaceEui64() */

CmsRet cmsUtl_parsePrefixAddress(const char *prefixAddr, char *address, UINT32 *plen)
{
   CmsRet ret = CMSRET_SUCCESS;
   char *tmpBuf;
   char *separator;
   UINT32 len;

   if (prefixAddr == NULL || address == NULL || plen == NULL)
   {
      return CMSRET_INVALID_ARGUMENTS;
   }      
   
   cmsLog_debug("prefixAddr=%s", prefixAddr);

   *address = '\0';
   *plen    = 128;

   len = strlen(prefixAddr);

   if ((tmpBuf = cmsMem_alloc(len+1, 0)) == NULL)
   {
      cmsLog_error("alloc of %d bytes failed", len);
      ret = CMSRET_INTERNAL_ERROR;
   }
   else
   {
      sprintf(tmpBuf, "%s", prefixAddr);
      separator = strchr(tmpBuf, '/');
      if (separator != NULL)
      {
         /* break the string into two strings */
         *separator = 0;
         separator++;
         while ((isspace(*separator)) && (*separator != 0))
         {
            /* skip white space after comma */
            separator++;
         }

         *plen = atoi(separator);
         cmsLog_debug("plen=%d", *plen);
      }

      cmsLog_debug("address=%s", tmpBuf);
      if (strlen(tmpBuf) < BUFLEN_40 && *plen <= 128)
      {
         strcpy(address, tmpBuf);
      }
      else
      {
         ret = CMSRET_INVALID_ARGUMENTS;
      }
      cmsMem_free(tmpBuf);
   }

   return ret;
   
}  /* End of cmsUtl_parsePrefixAddress() */

#endif


UBOOL8 cmsUtl_ipStrToOctets(const char *input, char *output)
{
   UBOOL8 ret = TRUE;
   char *pToken = NULL;
   char *pLast = NULL;
   char buf[BUFLEN_16];
   UINT32 i, num;

   if (input == NULL || strlen(input) < 7 || strlen(input) > 15)
   {
      return FALSE;
   }

   /* need to copy since strtok_r updates string */
   strcpy(buf, input);

   /* IP address has the following format
      xxx.xxx.xxx.xxx where x is decimal number */
   pToken = strtok_r(buf, ".", &pLast);
   if ((cmsUtl_strtoul(pToken, NULL, 10, &num) != CMSRET_SUCCESS) ||
       (num > 255))
   {
      ret = FALSE;
   }
   else
   {
      output[0] = num;

      for ( i = 0; i < 3; i++ )
      {
         pToken = strtok_r(NULL, ".", &pLast);

         if ((cmsUtl_strtoul(pToken, NULL, 10, &num) != CMSRET_SUCCESS) ||
             (num > 255))
         {
            ret = FALSE;
            break;
         }
         else
         {
            output[i+1] = num;
         }
      }
   }
   return ret;
}
