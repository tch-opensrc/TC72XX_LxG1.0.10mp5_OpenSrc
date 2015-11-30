/***********************************************************************
 *
 *  Copyright (c) 2008  Broadcom Corporation
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

#include <string.h>
#include "cms.h"
#include "cms_mem.h"
#include "cms_log.h"


CmsRet cmsXml_escapeString(const char *string, char **escapedString)
{
   UINT32 len, len2, i=0, j=0;
   char *tmpStr;

   if (string == NULL)
   {
      return CMSRET_SUCCESS;
   }

   len = strlen(string);
   len2 = len;

   /* see how many characters need to be escaped and what the new length is */
   while (i < len)
   {
      if (string[i] == '<' || string[i] == '>')
      {
         len2 += 3;
      }
      else if (string[i] == '&' || string[i] == '%')
      {
         len2 += 4;
      }
      i++;
   }

   if ((tmpStr = cmsMem_alloc(len2+1, ALLOC_ZEROIZE)) == NULL)
   {
      cmsLog_error("failed to allocate %d bytes", len+1);
      return CMSRET_RESOURCE_EXCEEDED;
   }

   i=0;
   while (i < len)
   {
      if (string[i] == '<')
      {
         tmpStr[j++] = '&';
         tmpStr[j++] = 'l';
         tmpStr[j++] = 't';
         tmpStr[j++] = ';';
      }
      else if (string[i] == '>')
      {
         tmpStr[j++] = '&';
         tmpStr[j++] = 'g';
         tmpStr[j++] = 't';
         tmpStr[j++] = ';';
      }
      else if (string[i] == '&')
      {
         tmpStr[j++] = '&';
         tmpStr[j++] = 'a';
         tmpStr[j++] = 'm';
         tmpStr[j++] = 'p';
         tmpStr[j++] = ';';
      }
      else if (string[i] == '%')
      {
         tmpStr[j++] = '&';
         tmpStr[j++] = '#';
         tmpStr[j++] = '3';
         tmpStr[j++] = '7';
         tmpStr[j++] = ';';
      }
      else if (string[i] == '"')
      {
         tmpStr[j++] = '&';
         tmpStr[j++] = 'q';
         tmpStr[j++] = 'u';
         tmpStr[j++] = 'o';
         tmpStr[j++] = 't';
         tmpStr[j++] = ';';
      }
      else
      {
         tmpStr[j++] = string[i];
      }

      i++;
   }

   *escapedString = tmpStr;

   return CMSRET_SUCCESS;
}


CmsRet cmsXml_unescapeString(const char *escapedString, char **string)
{
   UINT32 len, i=0, j=0;
   char *tmpStr;

   if (escapedString == NULL)
   {
      return CMSRET_SUCCESS;
   }

   len = strlen(escapedString);

   if ((tmpStr = cmsMem_alloc(len+1, ALLOC_ZEROIZE)) == NULL)
   {
      cmsLog_error("failed to allocate %d bytes", len+1);
      return CMSRET_RESOURCE_EXCEEDED;
   }

   while (i < len)
   {
      if (escapedString[i] != '&')
      {
         tmpStr[j++] = escapedString[i++];
      }
      else
      {
         /*
          * We have a possible escape sequence.  Check for the
          * whole thing in 1 shot.
          */
         if ((i+3<len) &&
             escapedString[i+1] == 'g' &&
             escapedString[i+2] == 't' &&
             escapedString[i+3] == ';')
         {
            tmpStr[j++] = '>';
            i += 4;
         }
         else if ((i+3<len) &&
             escapedString[i+1] == 'l' &&
             escapedString[i+2] == 't' &&
             escapedString[i+3] == ';')
         {
            tmpStr[j++] = '<';
            i += 4;
         }
         else if ((i+4<len) &&
             escapedString[i+1] == 'a' &&
             escapedString[i+2] == 'm' &&
             escapedString[i+3] == 'p' &&
             escapedString[i+4] == ';')
         {
            tmpStr[j++] = '&';
            i += 5;
         }
         else if ((i+4<len) &&
             escapedString[i+1] == '#' &&
             escapedString[i+2] == '3' &&
             escapedString[i+3] == '7' &&
             escapedString[i+4] == ';')
         {
            tmpStr[j++] = '%';
            i += 5;
         }
         else if ((i+5<len) &&
             escapedString[i+1] == 'q' &&
             escapedString[i+2] == 'u' &&
             escapedString[i+3] == 'o' &&
             escapedString[i+4] == 't' &&
             escapedString[i+5] == ';')
         {
            tmpStr[j++] = '"';
            i += 6;
         }
         else
         {
            /* not a valid escape sequence, just copy it */
            tmpStr[j++] = escapedString[i++];
         }
      }
   }

   *string = tmpStr;

   return CMSRET_SUCCESS;
}




