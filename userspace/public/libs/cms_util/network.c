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

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>

#include "cms.h"
#include "cms_util.h"
#include "oal.h"

static SINT32 cmsNet_getLeftMostOneBits(SINT32 num);

CmsRet cmsNet_getLanInfo(const char *lan_ifname, struct in_addr *lan_ip, struct in_addr *lan_subnetmask)
{
   return (oal_getLanInfo(lan_ifname, lan_ip, lan_subnetmask));
}


UBOOL8 cmsNet_isInterfaceUp(const char *ifname)
{
   return (oal_isInterfaceUp(ifname));
}


UBOOL8 cmsNet_isAddressOnLanSide(const char *ipAddr)
{
   UBOOL8 onLanSide = FALSE;

   /* determine the address family of ipAddr */
   if ((ipAddr != NULL) && (strchr(ipAddr, ':') == NULL))
   {
      /* ipv4 address */
      struct in_addr clntAddr, inAddr, inMask;

#ifdef DESKTOP_LINUX
      /*
       * Many desktop linux tests occur on loopback interface.  Consider that
       * as LAN side.
       */
      if (!strcmp(ipAddr, "127.0.0.1"))
      {
         return TRUE;
      }
#endif

      clntAddr.s_addr = inet_addr(ipAddr);

      oal_getLanInfo("br0", &inAddr, &inMask);
      /* check ip address of support user to see it is in LAN or not */
      if ( (clntAddr.s_addr & inMask.s_addr) == (inAddr.s_addr & inMask.s_addr) )
         onLanSide = TRUE;
      else {
         /* check ip address of support user to see if it is from secondary LAN */
         if (oal_isInterfaceUp("br0:0")) {
            oal_getLanInfo("br0:0", &inAddr, &inMask);
            if ( (clntAddr.s_addr & inMask.s_addr) == (inAddr.s_addr & inMask.s_addr) )
               onLanSide = TRUE;
         }
         /* This function will not work for ppp ip extension.  See dal_auth.c for detail */
      }
   }
#ifdef DMP_X_BROADCOM_COM_IPV6_1 /* aka SUPPORT_IPV6 */
   else
   {
      /* ipv6 address */
      char lanAddr6[CMS_IPADDR_LENGTH];

      if (oal_getLanAddr6("br0", lanAddr6) != CMSRET_SUCCESS)
      {
         cmsLog_error("cmsDal_getLanAddr6 returns error.");
      }
      else
      {
         /* see if the client addr is in the same subnet as br0. */
         onLanSide = cmsNet_isHostInSameSubnet(ipAddr, lanAddr6);
      }
   }
#endif

   return onLanSide;

}  /* End of cmsNet_isAddressOnLanSide() */


/***************************************************************************
// Function Name: cmsNet_getLeftMostOneBits.
// Description  : get the left most one bit number in the given number.
// Parameters   : num -- the given number.
// Returns      : the left most one bit number in the given number.
****************************************************************************/
SINT32 cmsNet_getLeftMostOneBits(SINT32 num) 
{
   int pos = 0;
   int numArr[8] = {128, 64, 32, 16, 8, 4, 2, 1};

   // find the left most zero bit position
   for ( pos = 0; pos < 8; pos++ )
   {
      if ( (num & numArr[pos]) == 0 )
         break;
   }

   return pos;
}

/***************************************************************************
// Function Name: cmsNet_getLeftMostOneBitsInMask.
// Description  : get number of left most one bit in the given subnet mask.
// Parameters   : ipMask -- subnet mask.
// Returns      : number of left most one bit in subnet mask.
****************************************************************************/
UINT32 cmsNet_getLeftMostOneBitsInMask(const char *ipMask)
{
   UINT32 num = 0;

   if (ipMask != NULL)
   {
      struct in_addr inetAddr;

      if (inet_aton(ipMask, &inetAddr) != 0)
      {
         SINT32 i;

         for (i = 31; i >= 0; i--)
         {
            if (inetAddr.s_addr & (UINT32)(1<<i))
            {
               num++;
            }
            else
            {
               break;
            }
         }
      }
   }
   return num;
}


void cmsNet_inet_cidrton(const char *cp, struct in_addr *ipAddr, struct in_addr *ipMask)
{
   char addrStr[BUFLEN_32];
   char *addr = NULL;
   char *last = NULL;

   ipAddr->s_addr = 0;
   ipMask->s_addr = 0;

   strncpy(addrStr, cp, sizeof(addrStr));

   if ((addr = strtok_r(cp, "/", &last)) != NULL)
   {
      if (inet_aton(addr, ipAddr) != 0)
      {
         char *mask = NULL;

         if ((mask = strtok_r(NULL, "/", &last)) != NULL)
         {
            SINT32 i;
            SINT32 cidrLen = atoi(mask);

            for (i = 0; (i < cidrLen) && (i < 32); i++)
            {
               ipMask->s_addr |= (UINT32)(1<<(31-i));
            }
         }
      }
   }
}

SINT32 cmsNet_getIfindexByIfname(char *ifname)
{
   int sockfd;
   int idx;
   int ifindex = -1;
   struct ifreq ifr;

   /* open socket to get INET info */
   if ((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) <= 0)
   {
      cmsLog_error("socket returns error. sockfd=%d", sockfd);
      return -1;
   }

   for (idx = 2; idx < 32; idx++)
   {
      memset(&ifr, 0, sizeof(struct ifreq));
      ifr.ifr_ifindex = idx;

      if (ioctl(sockfd, SIOCGIFNAME, &ifr) >= 0)
      {
         if (strcmp(ifname, ifr.ifr_name) == 0)
         {
            ifindex = idx;
            break;
         }
      }
   }
   
   close(sockfd);

   return ifindex;

}  /* End of cmsNet_getIfindexByIfname() */


CmsRet cmsNet_getIfNameList(char **ifNameList)
{
   return (oalNet_getIfNameList(ifNameList));
}

#ifdef DMP_ETHERNETWAN_1
CmsRet cmsNet_getPersistentWanIfNameList(char **PersistentWanifNameList)
{
   return (oal_Net_getPersistentWanIfNameList(PersistentWanifNameList));
}
#endif


#ifdef DMP_X_BROADCOM_COM_IPV6_1 /* aka SUPPORT_IPV6 */

CmsRet cmsNet_getIfAddr6(const char *ifname, UINT32 addrIdx,
                         char *ipAddr, UINT32 *ifIndex, UINT32 *prefixLen, UINT32 *scope, UINT32 *ifaFlags)
{
   return oal_getIfAddr6(ifname, addrIdx, ipAddr, ifIndex, prefixLen, scope, ifaFlags);
}

UBOOL8 cmsNet_areIp6AddrEqual(const char *ip6Addr1, const char *ip6Addr2)
{
   char address1[CMS_IPADDR_LENGTH];
   char address2[CMS_IPADDR_LENGTH];
   UINT32 plen1 = 0;
   UINT32 plen2 = 0;
   struct in6_addr   in6Addr1, in6Addr2;
   CmsRet ret;

   if (IS_EMPTY_STRING(ip6Addr1) && IS_EMPTY_STRING(ip6Addr2))
   {
      return TRUE;
   }
   if (ip6Addr1 == NULL || ip6Addr2 == NULL)
   {
      return FALSE;
   }

   if ((ret = cmsUtl_parsePrefixAddress(ip6Addr1, address1, &plen1)) != CMSRET_SUCCESS)
   {
      cmsLog_error("cmsUtl_parsePrefixAddress returns error. ret=%d", ret);
      return FALSE;
   }
   if ((ret = cmsUtl_parsePrefixAddress(ip6Addr2, address2, &plen2)) != CMSRET_SUCCESS)
   {
      cmsLog_error("cmsUtl_parsePrefixAddress returns error. ret=%d", ret);
      return FALSE;
   }

   if (inet_pton(AF_INET6, address1, &in6Addr1) <= 0)
   {
      cmsLog_error("Invalid address1=%s", address1);
      return FALSE;
   }
   if (inet_pton(AF_INET6, address2, &in6Addr2) <= 0)
   {
      cmsLog_error("Invalid address2=%s", address2);
      return FALSE;
   }

   return ((memcmp(&in6Addr1, &in6Addr2, sizeof(struct in6_addr)) == 0) && (plen1 == plen2));

}  /* cmsNet_areIp6AddrEqual() */

UBOOL8 cmsNet_areIp6DnsEqual(const char *dnsServers1, const char *dnsServers2)
{
   char dnsPri1[CMS_IPADDR_LENGTH];
   char dnsSec1[CMS_IPADDR_LENGTH];
   char dnsPri2[CMS_IPADDR_LENGTH];
   char dnsSec2[CMS_IPADDR_LENGTH];
   CmsRet ret;

   *dnsPri1 = '\0';
   *dnsSec1 = '\0';
   *dnsPri2 = '\0';
   *dnsSec2 = '\0';

   if (IS_EMPTY_STRING(dnsServers1) && IS_EMPTY_STRING(dnsServers2))
   {
      return TRUE;
   }
   if (dnsServers1 == NULL || dnsServers2 == NULL)
   {
      return FALSE;
   }

   if ((ret = cmsUtl_parseDNS(dnsServers1, dnsPri1, dnsSec1)) != CMSRET_SUCCESS)
   {
      cmsLog_error("cmsUtl_parseDNS returns error. ret=%d", ret);
      return FALSE;
   }
   if ((ret = cmsUtl_parseDNS(dnsServers2, dnsPri2, dnsSec2)) != CMSRET_SUCCESS)
   {
      cmsLog_error("cmsUtl_parseDNS returns error. ret=%d", ret);
      return FALSE;
   }

   if (!cmsNet_areIp6AddrEqual(dnsPri1, dnsPri2) ||
       !cmsNet_areIp6AddrEqual(dnsSec2, dnsSec2))
   {
      return FALSE;
   }

   return TRUE;

}  /* cmsNet_areIp6DnsEqual() */

UBOOL8 cmsNet_isHostInSameSubnet(const char *addrHost, const char *addrPrefix)
{
   char address[CMS_IPADDR_LENGTH];
   char prefix1[CMS_IPADDR_LENGTH];
   char prefix2[CMS_IPADDR_LENGTH];
   UINT32 plen = 0;
   CmsRet ret;

   *address = '\0';
   *prefix1 = '\0';
   *prefix2 = '\0';

   if (IS_EMPTY_STRING(addrHost) && IS_EMPTY_STRING(addrPrefix))
   {
      return TRUE;
   }
   if (addrHost == NULL || addrPrefix == NULL)
   {
      return FALSE;
   }

   if (strchr(addrHost, '/') != NULL)
   {
      cmsLog_error("Invalid host address %s", addrHost);
      return FALSE;
   }
   if (strchr(addrPrefix, '/') == NULL)
   {
      cmsLog_error("Invalid address prefix %s", addrPrefix);
      return FALSE;
   }

   if ((ret = cmsUtl_parsePrefixAddress(addrPrefix, address, &plen)) != CMSRET_SUCCESS)
   {
      cmsLog_error("cmsUtl_parsePrefixAddress returns error. ret=%d", ret);
      return FALSE;
   }

   if ((ret = cmsUtl_getAddrPrefix(addrHost, plen, prefix1)) != CMSRET_SUCCESS)
   {
      cmsLog_error("cmsUtl_getAddrPrefix returns error. ret=%d", ret);
      return FALSE;
   }
   if ((ret = cmsUtl_getAddrPrefix(address, plen, prefix2)) != CMSRET_SUCCESS)
   {
      cmsLog_error("cmsUtl_getAddrPrefix returns error. ret=%d", ret);
      return FALSE;
   }

   return (cmsNet_areIp6AddrEqual(prefix1, prefix2));

}  /* cmsNet_isHostInSameSubnet() */

CmsRet cmsNet_subnetIp6SitePrefix(const char *sp, UINT8 subnetId, UINT32 snPlen, char *snPrefix)
{
   char prefix[CMS_IPADDR_LENGTH];
   char address[CMS_IPADDR_LENGTH];
   UINT32 plen;
   struct in6_addr   in6Addr;
   CmsRet ret;

   if (snPrefix == NULL)
   {
      cmsLog_error("snPrefix is NULL.");
      return CMSRET_INVALID_ARGUMENTS;
   }
   *snPrefix = '\0';

   if (IS_EMPTY_STRING(sp))
   {
      cmsLog_error("sp is empty. do nothing.");
      return CMSRET_SUCCESS;
   }

   /* set a limitation to subnet prefix length to be at 8 bit boundary */
   if (snPlen % 8)
   {
      cmsLog_error("snPlen is not at 8 bit boundary. snPlen=%d", snPlen);
      return CMSRET_INVALID_ARGUMENTS;
   }

   if ((ret = cmsUtl_parsePrefixAddress(sp, address, &plen)) != CMSRET_SUCCESS)
   {
      cmsLog_error("cmsUtl_parsePrefixAddress returns error. ret=%d", ret);
      return CMSRET_INVALID_ARGUMENTS;
   }

   if ((snPlen <= plen) || (subnetId >= (1<<(snPlen-plen))))
   {
      cmsLog_error("plen=%d snPlen=%d subnetId=%d", plen, snPlen, subnetId);
      return CMSRET_INVALID_ARGUMENTS;
   }
   
   if ((ret = cmsUtl_getAddrPrefix(address, plen, prefix)) != CMSRET_SUCCESS)
   {
      cmsLog_error("cmsUtl_getAddrPrefix returns error. ret=%d", ret);
      return ret;
   }

   if (inet_pton(AF_INET6, prefix, &in6Addr) <= 0)
   {
      cmsLog_error("inet_pton returns error");
      return CMSRET_INVALID_ARGUMENTS;
   }

   /* subnet the site prefix */
   in6Addr.s6_addr[(snPlen-1)/8] |= subnetId;

   if (inet_ntop(AF_INET6, &in6Addr, snPrefix, CMS_IPADDR_LENGTH) == NULL)
   {
      cmsLog_error("inet_ntop returns error");
      return CMSRET_INTERNAL_ERROR;
   }

   return CMSRET_SUCCESS;

}  /* cmsNet_subnetIp6SitePrefix() */

#endif

