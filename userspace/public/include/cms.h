/***********************************************************************
 *
 *  Copyright (c) 2006-2007  Broadcom Corporation
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

#ifndef __CMS_H__
#define __CMS_H__

/*!\file cms.h
 * \brief Header file containing common and constant definitions for
 *        the CPE Management System (CMS).  Parameters which may change
 *        depending on vendor preference or board configuration are located
 *        in cms_params.h (which is included by this file at the bottom.)
 */

#include "os_defs.h"
#include "cms_version.h"



/*!\enum CmsRet
 * \brief Return codes for all external functions, and some internal functions too.
 *
 * Codes from 9000-9799 are reserved for TR69C return values.
 * All Broadcom return codes should start at 9800.
 */
typedef enum
{
   CMSRET_SUCCESS              = 0,     /**<Success. */
   CMSRET_METHOD_NOT_SUPPORTED = 9000,  /**<Method not supported. */
   CMSRET_REQUEST_DENIED       = 9001,  /**< Request denied (no reason specified). */
   CMSRET_INTERNAL_ERROR       = 9002,  /**< Internal error. */
   CMSRET_INVALID_ARGUMENTS    = 9003,  /**< Invalid arguments. */
   CMSRET_RESOURCE_EXCEEDED    = 9004,  /**< Resource exceeded.
                                        *  (when used in association with
                                        *  setParameterValues, this MUST not be
                                        *  used to indicate parameters in error)
                                        */
   CMSRET_INVALID_PARAM_NAME   = 9005,  /**< Invalid parameter name.
                                        *  (associated with set/getParameterValues,
                                        *  getParameterNames,set/getParameterAtrributes)
                                        */
   CMSRET_INVALID_PARAM_TYPE   = 9006,  /**< Invalid parameter type.
                                        *  (associated with set/getParameterValues)
                                        */
   CMSRET_INVALID_PARAM_VALUE  = 9007,  /**< Invalid parameter value.
                                        *  (associated with set/getParameterValues)
                                        */
   CMSRET_SET_NON_WRITABLE_PARAM = 9008,/**< Attempt to set a non-writable parameter.
                                        *  (associated with setParameterValues)
                                        */
   CMSRET_NOTIFICATION_REQ_REJECTED = 9009, /**< Notification request rejected.
                                            *  (associated with setParameterAttributes)
                                            */
   CMSRET_DOWNLOAD_FAILURE     = 9010,  /**< Download failure.
                                         *  (associated with download or transferComplete)
                                         */
   CMSRET_UPLOAD_FAILURE       = 9011,  /**< Upload failure.
                                        *  (associated with upload or transferComplete)
                                        */
   CMSRET_FILE_TRANSFER_AUTH_FAILURE = 9012,  /**< File transfer server authentication
                                              *  failure.
                                              *  (associated with upload, download
                                              *  or transferComplete)
                                              */
   CMSRET_UNSUPPORTED_FILE_TRANSFER_PROTOCOL = 9013,/**< Unsupported protocol for file
                                                    *  transfer.
                                                    *  (associated with upload or
                                                    *  download)
                                                    */


   CMSRET_SUCCESS_REBOOT_REQUIRED = 9800, /**< Config successful, but requires reboot to take effect. */
   CMSRET_SUCCESS_UNRECOGNIZED_DATA_IGNORED = 9801,  /**<Success, but some unrecognized data was ignored. */
   CMSRET_SUCCESS_OBJECT_UNCHANGED = 9802,  /**<Success, furthermore object has not changed, returned by STL handler functions. */
   CMSRET_FAIL_REBOOT_REQUIRED = 9803,  /**<Config failed, and now system is in a bad state requiring reboot. */
   CMSRET_NO_MORE_INSTANCES = 9804,     /**<getnext operation cannot find any more instances to return. */
   CMSRET_MDM_TREE_ERROR = 9805,         /**<Error during MDM tree traversal */
   CMSRET_WOULD_DEADLOCK = 9806, /**< Caller is requesting a lock while holding the same lock or a different one. */
   CMSRET_LOCK_REQUIRED = 9807,  /**< The MDM lock is required for this operation. */
   CMSRET_OP_INTR = 9808,      /**<Operation was interrupted, most likely by a Linux signal. */
   CMSRET_TIMED_OUT = 9809,     /**<Operation timed out. */
   CMSRET_DISCONNECTED = 9810,  /**< Communications link is disconnected. */
   CMSRET_MSG_BOUNCED = 9811,   /**< Msg was sent to a process not running, and the
                                 *   bounceIfNotRunning flag was set on the header.  */
   CMSRET_OP_ABORTED_BY_USER = 9812,  /**< Operation was aborted/discontinued by the user */
   CMSRET_RECURSION_ERROR = 9817,     /**< too many levels of recursion */
   CMSRET_OPEN_FILE_ERROR = 9818,     /**< open file error */
   CMSRET_KEY_GENERATION_ERROR = 9830,     /** certificate key generation error */
   CMSRET_INVALID_CERT_REQ = 9831,     /** requested certificate does not match with issued certificate */
   CMSRET_INVALID_CERT_SUBJECT = 9832,     /** certificate has invalid subject information */
   CMSRET_OBJECT_NOT_FOUND = 9840,     /** failed to find object */

   CMSRET_INVALID_FILENAME = 9850,  /**< filename was not given for download */
   CMSRET_INVALID_IMAGE = 9851,     /**< bad image was given for download */
   CMSRET_INVALID_CONFIG_FILE = 9852,  /**< invalid config file was detected */
   CMSRET_CONFIG_PSI = 9853,         /**< old PSI/3.x config file was detected */
   CMSRET_IMAGE_FLASH_FAILED = 9854, /**< could not write the image to flash */

} CmsRet;


/** Check if the CmsRet code is either SUCCESS or SUCCESS_REBOOT_REQUIRED.
 */
#define IS_CMSRET_A_SUCCESS_VARIANT(r) (((r) == CMSRET_SUCCESS) || \
                                        ((r) == CMSRET_SUCCESS_REBOOT_REQUIRED))

/** Check if the CmsRet code is a TR69c recognized value.
 */
#define IS_CMSRET_A_TR69C_VARIANT(r) (((r) == CMSRET_SUCCESS) ||        \
                                      (((r) >= 9000) && ((r) < 9800)))



#ifndef TRUE
/** TRUE = 1
 */
#define TRUE  1
#endif

#ifndef FALSE
/** FALSE = 0
 */
#define FALSE 0
#endif

/** Maximum value for a UINT64 */
#define MAX_UINT64 18446744073709551615ULL

/** Maximum value for a SINT64 */
#define MAX_SINT64 9223372036854775807LL

/** Minimum value for a SINT64 */
#define MIN_SINT64 (-1 * MAX_SINT64 - 1)

/** Maximum value for a UINT32 */
#define MAX_UINT32 4294967295U

/** Maximum value for a SINT32 */
#define MAX_SINT32 2147483647

/** Minimum value for a SINT32 */
#define MIN_SINT32 (-2147483648)

/** Maximum value for a UINT16 */
#define MAX_UINT16  65535

/** Maximum value for a SINT16 */
#define MAX_SINT16  32767

/** Minimum value for a SINT16 */
#define MIN_SINT16  (-32768)


/**
 * This is common used string length types.
 */
#define BUFLEN_4        4     //!< buffer length 4
#define BUFLEN_8        8     //!< buffer length 8
#define BUFLEN_16       16    //!< buffer length 16
#define BUFLEN_18       18    //!< buffer length 18 -- for ppp session id
#define BUFLEN_24       24    //!< buffer length 24 -- mostly for password
#define BUFLEN_32       32    //!< buffer length 32
#define BUFLEN_40       40    //!< buffer length 40
#define BUFLEN_48       48    //!< buffer length 48
#define BUFLEN_64       64    //!< buffer length 64
#define BUFLEN_128      128   //!< buffer length 128
#define BUFLEN_256      256   //!< buffer length 256
#define BUFLEN_264      264   //!< buffer length 264
#define BUFLEN_512      512   //!< buffer length 512
#define BUFLEN_1024     1024  //!< buffer length 1024

#define IIDSTACK_BUF_LEN  40  //!< good length to use for mdm_dumpIidStack
#define MAC_ADDR_LEN    6     //!< Mac address len in an array of 6 bytes
#define MAC_STR_LEN     17    //!< Mac String len with ":". eg: xx:xx:xx:xx:xx:xx
#define VPI_MIN         0     //!< VPI min 
#define VPI_MAX         255   //!< VPI max 
#define VCI_MIN         32    //!< VCI min 
#define VCI_MAX         65535 //!< VCI max 

#define PPP_CONNECT_ERROR_REASON_LEN 48

#define CMS_IFNAME_LENGTH  BUFLEN_32   //!< broadcom interface name length

#ifdef DMP_X_BROADCOM_COM_IPV6_1    /* aka SUPPORT_IPV6 */
#define CMS_IPADDR_LENGTH  44          //!< IP address length to hold IPv6 in CIDR notation
#else
#define CMS_IPADDR_LENGTH  BUFLEN_16   //!< IP address length to hold IPv4 in ASCII
#endif /* DMP_X_BROADCOM_COM_IPV6_1 */
#
#define CMS_MAX_DEFAULT_GATEWAY     8  //!< max default gateways allowed in L3 X_BROADCOM_COM_DefaultConnectionServices
#define CMS_MAX_DNSIFNAME           8  //!< max dns wan interface names in X_BROADCOM_Networking.DNSIfName
#define CMS_MAX_ACTIVE_DNS_IP       2  //!< max active dns ip (in resolv.conf)
/**
 * Values for network protocol
 */
#define PROTO_PPPOE        0  //!< PPPoE protocol
#define PROTO_PPPOA        1  //!< PPPoA protocol
#define PROTO_MER          2  //!< MER protocol
#define PROTO_BRIDGE       3  //!< bridge protocol
#define PROTO_PPPOE_RELA   4  //!< PPPoE relay protocol
#define PROTO_IPOA         5  //!< ip over atm protocol
#define PROTO_IPOWAN       6  //!< ip over wan protocol
#define PROTO_NONE         10 //!< invalid protocol

#define IFC_WAN_MAX        8  //!< Max WAN Connection in the system.
#define IFC_VLAN_MAX       8  //!< Max VLAN on single PVC
#define ATM_QUEUE_MAX      8  //!< Max ATM queues

/*!\enum WanIfcType
 * \brief Enumerated values of WAN interface types.
 */
typedef enum {
   WAN_IFC_ATM=0,       /**< ATM */
   WAN_IFC_PPPOA=1,     /**< PPPoA */
   WAN_IFC_IPOA=2,      /**< IPoA */
   WAN_IFC_ETH=3,       /**< Eth */
   WAN_IFC_PTM=4,       /**< Ptm */
   WAN_IFC_NONE=10      /**< undefined/invalid */
} WanIfcType;

#ifdef NOT_USED_AND_USE_CMSWANCONNECTIONTYPE_BELOW
/******************* NOTE:  DO NOT USE WanProtocal. USE CmsWanConnetctionType Below !!!!! ***********
 * !\enum WanProtocol
 * \brief Enumerated values of WAN connection protocols.
 * This should be used to replace the same set of defines in cms.h
 */
typedef enum {
   WAN_PROTO_PPPOE=0,   /**< PPPoE */
   WAN_PROTO_PPPOA=1,   /** < PPPoA */
   WAN_PROTO_MER=2,     /**< static or dynamic mer */
   WAN_PROTO_BRIDGE=3,  /**< bridge */
   WAN_PROTO_PPPOE_RELAY=4,  /**< pppoe relay */
   WAN_PROTO_IPOA=5,     /**< IPoA */
   WAN_PROTO_IPOWAN=6,   /**< IP over Wan only when SUPPORT ETHWAN */
   WAN_PROTO_NONE=10     /**< no proto found/defined/invalid */
} WanProtocol;
#endif 

/* try to match with the above old defines  PROTO_PPPOE=0 PPPOA=1, MER=2, BRIDGE=3 thing 
 * so that no html changes need  */
typedef enum 

{
   CMS_WAN_TYPE_PPPOE               = 0,
   CMS_WAN_TYPE_PPPOA               = 1,
   CMS_WAN_TYPE_DYNAMIC_IPOE        = 2,
   CMS_WAN_TYPE_BRIDGE              = 3,
   CMS_WAN_TYPE_PPPOE_RELAY         = 4,
   CMS_WAN_TYPE_IPOA                = 5,
   CMS_WAN_TYPE_STATIC_IPOE         = 6,   
   
   CMS_WAN_TYPE_STATIC_ETHERNET_IP  = 10,
   CMS_WAN_TYPE_DYNAMIC_ETHERNET_IP = 11,
   CMS_WAN_TYPE_ETHERNET_PPPOE      = 12,
   CMS_WAN_TYPE_ETHERNET_BRIDGE     = 13,
   CMS_WAN_TYPE_UNDEFINED           = 99
} CmsWanConnectionType;



#define BRIDGE_PROTO_STR      "Bridge"
#define IPOA_PROTO_STR        "IPoA"
#define IPOE_PROTO_STR        "IPoE"
#define PPPOE_PROTO_STR       "PPPoE"
#define PPPOA_PROTO_STR       "PPPoA"
#define IPOW_PROTO_STR        "IPoW"

#define ETH_IFC_STR           "eth"
#define USB_IFC_STR           "usb"
#define WLAN_IFC_STR          "wl"
#define MOCA_IFC_STR          "moca"
#define ATM_IFC_STR           "atm"
#define PTM_IFC_STR           "ptm"
#define BRIDGE_IFC_STR        "br"
#define IPOA_IFC_STR          "ipoa"
#define IPOE_IFC_STR          "ipoe"
#define PPP_IFC_STR           "ppp"
#define PPPOE_IFC_STR         "pppoe"
#define PPPOA_IFC_STR         "pppoa"
#define IPA_IFC_STR           "ipa"
#define BRIDGE_SMUX_STR       "bridge"

/* for interface group with routed pvc */
#define RT_TABLES_BASE	200

typedef enum
{
   ATM=0,          /**< WanDev is used for DSL ATM  */               
   PTM=1,          /**< WanDev is used for DSL PTM  */     
   Ethernet=2      /**< WanDev is used for Ethernet  */
}WanLinkType;


typedef enum
{
   CMS_CONNECTION_MODE_DEFAULT=0,      /**< Default connection mdoe - single wan service over 1 connection */          
   CMS_CONNECTION_MODE_VLANMUX=1,      /**< Vlan mux connection mdoe - multiple vlan service over 1 connection */          
   CMS_CONNECTION_MODE_MSC=2,          /**< MSC connection mdoe - multiple wan service over 1 connection */           
} ConnectionModeType;
   

typedef enum
{
   ATM_EOA=0,        /**< WanDev is used for DSL ATM  */               
   ATM_IPOA=1,       /**< WanDev is used for DSL ATM IPOA */
   ATM_PPPOA=2,      /**< WanDev is used for DSL ATM PPPoA */
   PTM_EOA=3         /**< WanDev is used for DSL PTM  */     
   
}Layer2IfNameType;

/* RAM size defines */
#define SZ_8MB          0x800000
#define SZ_16MB         0x1000000


#define MAX_PERSISTENT_WAN_PORT     39       /**< Max Ethernet ports can be configured as WAN ports */

/* include cms_params.h after we have defined all other constants. */
#include "cms_params.h"

#endif /* __CMS_H__ */
