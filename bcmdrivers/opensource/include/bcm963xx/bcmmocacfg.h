/*
<:copyright-gpl
 Copyright 2007 Broadcom Corp. All Rights Reserved.

 This program is free software; you can distribute it and/or modify it
 under the terms of the GNU General Public License (Version 2) as
 published by the Free Software Foundation.

 This program is distributed in the hope it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
:>
*/

/***************************************************************************
 * File Name  : bcmMoCAcfg.h
 *
 * Description: This file contains the definitions, structures and function
 *              prototypes for the Broadcom Multimedia Over Coax
 *              Alliance (MoCA) Configuration driver.
 ***************************************************************************/

#if !defined(_BCMMoCACFG_H_)
#define _BCMMoCACFG_H_

#define MoCA_INIT_WITH_CONFIG_PARAMS
#define MoCA_PQoS_SUPPORT

/***************************************************************************
 * Constant Definitions
 ***************************************************************************/

#define MOCA_TRC_LEVEL_NONE		0x00000000
#define MOCA_TRC_LEVEL_FN_ENTRY  0x00000001  /* Fn Entry points, Args, if any */
#define MOCA_TRC_LEVEL_FN_EXIT   0x00000002  /* Fn Exit points, Args, if any */
#define MOCA_TRC_LEVEL_INFO      0x00000004  /* During the course of flow execution */
#define MOCA_TRC_LEVEL_DBG       0x00000008  /* Debug information */
#define MOCA_TRC_LEVEL_ERR       0x00000010  /* Erroneous Situations */
#define MOCA_TRC_LEVEL_WARN      0x00000020  /* Warning messages */
#define MOCA_TRC_LEVEL_VERBOSE   0x00000040  /* Verbose messages */
#define MOCA_DUMP_HOST_CORE      0x00010000  /* Host-Core message exchanges */
#define MOCA_TRC_LEVEL_TRAP      0x00020000  /* Trap message information */
#define MOCA_TRC_LEVEL_CORE      0x00040000  /* Core Traces */
#define MOCA_TRC_TIMESTAMPS      0x00080000  /* Show timestamps */
#define MOCA_TRC_LEVEL_ALL       (MOCA_TRC_LEVEL_FN_ENTRY   | \
                                  MOCA_TRC_LEVEL_FN_EXIT    | \
                                  MOCA_TRC_LEVEL_INFO       | \
                                  MOCA_TRC_LEVEL_DBG        | \
                                  MOCA_TRC_LEVEL_ERR        | \
                                  MOCA_TRC_LEVEL_WARN       | \
                                  MOCA_TRC_LEVEL_VERBOSE    | \
                                  MOCA_DUMP_HOST_CORE       | \
                                  MOCA_TRC_LEVEL_TRAP       | \
                                  MOCA_TRC_LEVEL_CORE       | \
                                  MOCA_TRC_TIMESTAMPS)
#define MOCA_TRC_LEVEL_MAX       (MOCA_TRC_LEVEL_CORE|MOCA_TRC_LEVEL_ALL)

#define MOCA_HOST_CONTROL_NONE	 0x00000000
#define MOCA_HOST_CONTROL_ENABLE  0x00000001
#define MOCA_HOST_CONTROL_DISABLE 0x00000002

/* MoCA Sizing values */
#define MoCA_MIN_PASSWORD_LEN             12
#define MoCA_MAX_PASSWORD_LEN             17
#define MoCA_MAX_KEY_LEN                  8
#define MoCA_MAX_PQOS_ENTRIES             16
#define MoCA_MAX_QOS_ENTRIES              16 /* Max Ethernet Prio ?? */
#define MoCA_MAX_NODES                    16
#define MoCA_MAX_UC_FWD_ENTRIES           128
//#define MoCA_MAX_MC_FWD_ENTRIES         128
#define MoCA_MAX_MC_FWD_ENTRIES           32  /* For the near term */
#define MoCA_NUM_MEMBERS_FOR_NON_BCAST    4
#define MoCA_NUM_MEMBERS_PER_MC_GROUP     MoCA_MAX_NODES
#define MoCA_MAX_SUB_CARRIERS             256
#define MoCA_MAX_SRC_ADDR_ENTRIES         128

#define MAX_BITS_PER_SUB_CARRIER          4
#define BITS_PER_BYTE                     8
#define BYTES_PER_WORD                    4

typedef UINT8  MAC_ADDRESS[6] ;

/* MoCA NV Parms values & definitions */
typedef enum {
   MoCA_AUTO_NEGOTIATE_FOR_NC = 0x0,                 /* Default */
   MoCA_ALWAYS_NC,
   MoCA_NEVER_NC,
   MoCA_DEF_NC_MODE = MoCA_AUTO_NEGOTIATE_FOR_NC
} MoCA_NC_MODE ;

#define MoCA_AUTO_NW_SCAN_DISABLED                0  /* Default */
#define MoCA_AUTO_NW_SCAN_ENABLED                 1
#define MoCA_DEF_AUTO_NW_SEARCH                   MoCA_AUTO_NW_SCAN_ENABLED

#define MoCA_PRIVACY_DISABLED                     0     /* Default */
#define MoCA_PRIVACY_ENABLED                      1
#define MoCA_DEF_PRIVACY                          MoCA_PRIVACY_DISABLED

#define MoCA_TPC_DISABLED                         0     /* Default */
#define MoCA_TPC_ENABLED                          1
#define MoCA_DEF_TPC                              MoCA_TPC_ENABLED

#define MoCA_NORMAL_OPERATION                     0     /* Default */
#define MoCA_CONTINUOUS_TX_PROBE_I                1
#define MoCA_CONTINUOUS_RX                        2
#define MoCA_EXTERNAL_CONTROL                     3
#define MoCA_DEF_CONST_TX_MODE                    MoCA_NORMAL_OPERATION

#define MoCA_MIN_TX_POWER                         -31
#define MoCA_MAX_TX_POWER                         3
#define MoCA_DEF_TX_POWER                         3

#define MoCA_DEF_WAN_FREQ_MHZ                     1000
#define MoCA_DEF_LAN_FREQ_MHZ                     1150
#define MoCA_DEF_FREQ_MHZ                         MoCA_DEF_LAN_FREQ_MHZ
#define MoCA_NULL_FREQ_MHZ                        0
#define MoCA_MIN_FREQ_MHZ                         25  /* To support Mid RF range */
#define MoCA_FREQ_INC_MHZ                         25
#define MoCA_MAX_FREQ_MHZ                         1825
#define MoCA_DBAND_FREQ_MASK                      0x0000FF00 /* Represents 1150, 1200, 1250, 1300, 
                                                              * 1350, 1400, 1450, 1500. (D-band)
                                                              * bits 15-8 - DBand
                                                              * bits  7-0 - Reserved,
                                                              */
#define MoCA_DEF_FREQ_MASK                        MoCA_DBAND_FREQ_MASK
#define MoCA_VALID_FREQ_MASK                      MoCA_DBAND_FREQ_MASK

#define MoCA_DEF_USER_PASSWORD                    "99999999988888888"
#define MoCA_DEF_PASSWORD_SIZE                    MoCA_MAX_PASSWORD_LEN

#define MoCA_MCAST_NORMAL_MODE                    0
#define MoCA_MCAST_BCAST_MODE                     1
#define MoCA_DEF_MCAST_MODE                       MoCA_MCAST_BCAST_MODE

#define MoCA_STANDARD_CLASSIFICATION_MODE         0
#define MoCA_QTAG_CLASSIFICATION_MODE             1
#define MoCA_DEF_CLASSIFICATION_MODE              MoCA_STANDARD_CLASSIFICATION_MODE

#define MoCA_NORMAL_MODE                          0
#define MoCA_LAB_MODE                             1
#define MoCA_DEF_MODE                             MoCA_NORMAL_MODE

#define MoCA_MIN_TABOO_MASK_START                 32  /* Corrsponding to 800 Mhz  */
#define MoCA_MAX_TABOO_MASK_START                 60  /* Corrsponding to 1500 Mhz */
#define MoCA_MAX_TABOO_CHANNELS                   24
#define MoCA_VALID_TABOO_CHAN_MASK                0x00FFFFFF
#define MoCA_DEF_TABOO_START_CHAN                 0
#define MoCA_DEF_TABOO_CHAN_MASK                  0x0

#define MoCA_MIN_PAD_POWER                        -15
#define MoCA_MAX_PAD_POWER                        -5
#define MoCA_DEF_PAD_POWER                        -7

#define MoCA_VERSION_10                           0x10
#define MoCA_VERSION_11                           0x11
#define MoCA_DEF_OPERATING_VERSION                MoCA_VERSION_11

#define MoCA_NO_PREFERED_NC_MODE                  0
#define MoCA_PREFERED_NC_MODE                     1
#define MoCA_DEF_PREFERED_NC_MODE                 MoCA_NO_PREFERED_NC_MODE

#define MoCA_LED_MODE_NW_SEARCH                   0
#define MoCA_LED_MODE_LINK_ON_OFF                 1
#define MoCA_DEF_LED_MODE                         MoCA_LED_MODE_NW_SEARCH

#define MoCA_LOOPBACK_DISABLED                    0     /* Default */
#define MoCA_LOOPBACK_ENABLED                     1
#define MoCA_DEF_LOOPBACK_EN                      MoCA_LOOPBACK_DISABLED

#define MoCA_MIN_FRAME_SIZE                       2048
#define MoCA_MAX_FRAME_SIZE                       9216
#define MoCA_DEF_FRAME_SIZE                       6148

#define MoCA_MIN_TRANSMIT_TIME                    300
#define MoCA_MAX_TRANSMIT_TIME                    1000
#define MoCA_DEF_TRANSMIT_TIME                    400

#define MoCA_MIN_BW_ALARM_THRESHOLD_DISABLED      0
#define MoCA_MIN_BW_ALARM_THRESHOLD               50
#define MoCA_DEF_BW_ALARM_THRESHOLD               100
#define MoCA_MAX_BW_ALARM_THRESHOLD               300

#define MoCA_LMO_REPORT_DISABLED                  0     /* Default */
#define MoCA_LMO_REPORT_ENABLED                   1
#define MoCA_DEF_LMO_REPORT_EN                    MoCA_LMO_REPORT_DISABLED

#define MoCA_CONTINUOUS_IE_RR_INSERT_OFF          0
#define MoCA_CONTINUOUS_IE_RR_INSERT_ON           1
#define MoCA_DEF_CONTINUOUS_IE_RR_INSERT          MoCA_CONTINUOUS_IE_RR_INSERT_OFF

#define MoCA_CONTINUOUS_IE_MAP_INSERT_OFF         0
#define MoCA_CONTINUOUS_IE_MAP_INSERT_ON          1
#define MoCA_DEF_CONTINUOUS_IE_MAP_INSERT         MoCA_CONTINUOUS_IE_MAP_INSERT_OFF

#define MoCA_MIN_PKT_AGGR                         1
#define MoCA_MAX_PKT_AGGR                         10
#define MoCA_DEF_PKT_AGGR                         6

#define MoCA_MAX_CONSTELLATION_ALL_NODES          0xFFFFFFFF
#define MoCA_DEF_CONSTELLATION_NODE               0
typedef enum _Constellation {
   MIN_CONSTELLATION = 0,
   BPSK,
   QPSK,
   QAM8,
   QAM16,
   QAM32,
   QAM64,
   QAM128,
   QAM256,
   QAM512,
   QAM1024,
   MAX_CONSTELLATION
} MaxConstellation ;
#define MoCA_DEF_CONSTELLATION_INFO               BPSK

#define MoCA_MIN_PMK_EXCHANGE_INTERVAL            1
#define MoCA_MAX_PMK_EXCHANGE_INTERVAL            12
#define MoCA_DEF_PMK_EXCHANGE_INTERVAL            11

#define MoCA_MIN_TEK_EXCHANGE_INTERVAL            1
#define MoCA_MAX_TEK_EXCHANGE_INTERVAL            10
#define MoCA_DEF_TEK_EXCHANGE_INTERVAL            9

#define MoCA_MIN_USER_SNR_MARGIN                  -3   /* in dBs */
#define MoCA_DEF_USER_SNR_MARGIN                  2
#define MoCA_MAX_USER_SNR_MARGIN                  25

#define MoCA_MIN_USER_SNR_MARGIN_OFFSET           -3   /* in dBs */
#define MoCA_DEF_USER_SNR_MARGIN_OFFSET           0
#define MoCA_MAX_USER_SNR_MARGIN_OFFSET           25


#define MoCA_PRIO_ALLOC_RESV_HIGH                 300
#define MoCA_PRIO_ALLOC_RESV_MED                  300
#define MoCA_PRIO_ALLOC_RESV_LOW                  300
#define MoCA_PRIO_ALLOC_LIMIT_HIGH                300
#define MoCA_PRIO_ALLOC_LIMIT_MED                 300
#define MoCA_PRIO_ALLOC_LIMIT_LOW                 300

#define MoCA_DEF_PRIO_RESV_HIGH                   9
#define MoCA_DEF_PRIO_RESV_MED                    64
#define MoCA_DEF_PRIO_RESV_LOW                    64
#define MoCA_DEF_PRIO_LIMIT_HIGH                  300
#define MoCA_DEF_PRIO_LIMIT_MED                   300
#define MoCA_DEF_PRIO_LIMIT_LOW                   300

#define MoCA_MIN_NOMINAL_MAP_CYCLE                1000
#define MoCA_DEF_NOMINAL_MAP_CYCLE                1000
#define MoCA_MAX_NOMINAL_MAP_CYCLE                2500

#define MoCA_MIN_MAX_INC_MAP_CYCLE                1
#define MoCA_DEF_MAX_INC_MAP_CYCLE                300
#define MoCA_MAX_MAX_INC_MAP_CYCLE                300

#define MoCA_MIN_MAX_DEC_MAP_CYCLE                1
#define MoCA_DEF_MAX_DEC_MAP_CYCLE                100
#define MoCA_MAX_MAX_DEC_MAP_CYCLE                100

#define MoCA_MIN_TARGET_PHY_RATE_QAM128           0
#define MoCA_DEF_TARGET_PHY_RATE_QAM128           235
#define MoCA_MAX_TARGET_PHY_RATE_QAM128           500

#define MoCA_MIN_TARGET_PHY_RATE_QAM256           0
#define MoCA_DEF_TARGET_PHY_RATE_QAM256           275
#define MoCA_MAX_TARGET_PHY_RATE_QAM256           500

#define MoCA_CORE_TRACE_CONTROL_DISABLE           0
#define MoCA_CORE_TRACE_CONTROL_ENABLE            1
#define MoCA_DEF_CORE_TRACE_CONTROL_ENABLE        MoCA_CORE_TRACE_CONTROL_DISABLE

#define MoCA_IQ_BURST_TYPE_BEACON                  1
#define MoCA_IQ_BURST_TYPE_MAP                     6
#define MoCA_IQ_BURST_TYPE_UNICAST                 7
#define MoCA_IQ_BURST_TYPE_BROADCAST               8

#define MoCA_IQ_ACMT_SYM_NUM_LAST_SYM              1
#define MoCA_IQ_ACMT_SYM_NUM_SYM_BEFORE_LAST       2

/* Return status values. */
typedef enum BcmMoCAStatus
{
    MoCASTS_SUCCESS = 0,
    MoCASTS_STANDALONE_MODE,
    MoCASTS_INIT_FAILED,
    MoCASTS_ERROR,
    MoCASTS_LOAD_ERROR,
    MoCASTS_STATE_ERROR,
    MoCASTS_PARAMETER_ERROR,
    MoCASTS_ALLOC_ERROR,
    MoCASTS_RESOURCE_ERROR,
    MoCASTS_IN_USE,
    MoCASTS_NOT_FOUND,
    MoCASTS_NOT_SUPPORTED,
    MoCASTS_HOST_REQ_TIMEOUT,
    MoCASTS_CORE_RESP_TIMEOUT,
    MoCASTS_GET_NEXT,
    MoCASTS_NOT_EXIST
} BCMMoCA_STATUS;

/* status definitions */

#define MoCA_QAM_256_SUPPORT_ON                1

#define MoCA_LINK_DOWN								  0
#define MoCA_LINK_UP									  1

#define MoCA_OPER_STATUS_ENABLED               0
#define MoCA_OPER_STATUS_HW_ERROR              1

#define MoCA_NODE_INACTIVE                     0
#define MoCA_NODE_ACTIVE                       1

#define MoCA_NODE_IS_NOT_NC                    0
#define MoCA_NODE_IS_NC                        1

typedef enum _MoCA_CALLBACK_EVENT {
   MoCA_CALLBACK_EVENT_LINK_STATUS = 0,
   MoCA_CALLBACK_EVENT_LOF,
   MoCA_MAX_CALLBACK_EVENTS
} MoCA_CALLBACK_EVENT;

typedef struct _MoCA_CALLBACK_DATA {
   union {
      UINT32      linkStatus ;
      UINT32      lof ;
   } data ;   
} MoCA_CALLBACK_DATA;

typedef struct _MoCANvParams {
   UINT32         lastOperFreq ;
} MoCA_NV_PARAMS, *PMoCA_NV_PARAMS ;

/* Masks defined here are used in selecting the required parameter from Mgmt
 * application.
 */
#define MoCA_MAX_INIT_PARAMS                  19
#define MoCA_INIT_PARAM_ALL_MASK              0x0007FFFF
typedef struct MoCAInitialization
{
#define MoCA_INIT_PARAM_NC_MODE_MASK                  0x00000001
   MoCA_NC_MODE   ncMode ;                   /* Network Control Mode */
#define MoCA_INIT_PARAM_AUTO_NETWORK_SEARCH_EN_MASK   0x00000002
   UINT32         autoNetworkSearchEn ;
#define MoCA_INIT_PARAM_PRIVACY_MASK                  0x00000004
   UINT32         privacyEn ;
#define MoCA_INIT_PARAM_TX_PWR_CONTROL_EN_MASK        0x00000008
   UINT32         txPwrControlEn ;           /* Transmit Power Control */
#define MoCA_INIT_PARAM_CONST_TRANSMIT_MODE_MASK      0x00000010
   UINT32         constTransmitMode ;
#define MoCA_INIT_PARAM_NV_PARAMS_LOF_MASK            0x00000020
   MoCA_NV_PARAMS nvParams ;
#define MoCA_INIT_PARAM_MAX_TX_POWER_MASK             0x00000040
   signed int     maxTxPower ;
#define MoCA_INIT_PARAM_NW_SEARCH_FREQ_MASK           0x00000080
   UINT32         freqMask ;
#define MoCA_INIT_PARAM_PASSWORD_SIZE_MASK            0x00000100
   UINT32         passwordSize ;
#define MoCA_INIT_PARAM_PASSWORD_MASK                 0x00000200
   UINT8          password [MoCA_MAX_PASSWORD_LEN] ;
#define MoCA_INIT_PARAM_MCAST_MODE_MASK               0x00000400
   UINT32         mcastMode ;
#define MoCA_INIT_PARAM_ECL_QTAG_MODE_MASK            0x00000800
   UINT32         eclQtagMode ;
#define MoCA_INIT_PARAM_LAB_MODE_MASK                 0x00001000
   UINT32         labMode ;
#define MoCA_INIT_PARAM_TABOO_START_CHAN_MASK         0x00002000
   UINT32         tabooStartChan ;
#define MoCA_INIT_PARAM_TABOO_CHAN_MASK               0x00004000
   UINT32         tabooChanMask ;
#define MoCA_INIT_PARAM_PAD_POWER_MASK                0x00008000
   signed int     padPower ;
#define MoCA_INIT_PARAM_OPERATING_VERSION_MASK        0x00010000
   signed int     operatingVersion ;
#define MoCA_INIT_PARAM_PREFERED_NC_MASK              0x00020000
   UINT32         preferedNC ;
#define MoCA_INIT_PARAM_LED_MODE_MASK                 0x00040000
   UINT32         ledMode ;
} MoCA_INITIALIZATION_PARMS, *PMoCA_INITIALIZATION_PARMS ;


/* Masks defined here are used in selecting the required parameter from Mgmt
 * application.
 */

typedef struct  _constellation {
   UINT32         node ;
   UINT32         info ;
} MoCAMaxConstellation ;

typedef struct  _PrioAllocations {
   UINT32   resvHigh ;
   UINT32   resvMed ;
   UINT32   resvLow ;
   UINT32   limitHigh ;
   UINT32   limitMed ;
   UINT32   limitLow ;
} MoCAPrioAllocations ;

typedef struct _IqDiagramSet {
   UINT32   nodeId ;
   UINT32   burstType ;
   UINT32   acmtSymNum ;
} MoCAIqDiagramSet ;

typedef struct _LabRegMem {
   UINT32   input ;
   UINT32   value ;
} MoCALabRegMem ;

typedef struct _LabCallFunc {
   UINT32   funcAddr ;
   UINT32   param1 ;
   UINT32   param2 ;
   UINT32   param3 ;
   UINT32   numParams;
} MoCALabCallFunc ;

#define MoCA_CFG_MAX_PARAMS                           29
#define MoCA_LAB_CFG_MAX_PARAMS                       5
#define MoCA_CFG_PARAM_ALL_MASK                       0x0FFFFFFF
#define MoCA_CFG_NON_LAB_PARAM_ALL_MASK               0x00BFF7EF  /* Due to issue with constelltion and OutOfOrderLmo and
                                                                     core trace control */
#define MoCA_CFG_DEF_PARAM_MASK                       0x00FFE72F  /* Parameters for which the defaults exist */
#define MoCA_MAX_SNR_TBL_INDEX                        10
typedef struct _MoCAConfigParams {
#define MoCA_CFG_PARAM_LPBK_EN_MASK                   0x00000001
   UINT32               lpbkEn ;
#define MoCA_CFG_PARAM_MAX_FRAME_SIZE_MASK            0x00000002
   UINT32               maxFrameSize ;
#define MoCA_CFG_PARAM_MAX_TRANS_TIME_MASK            0x00000004
   UINT32               maxTransmitTime ;
#define MoCA_CFG_PARAM_MIN_BW_ALARM_THRE_MASK         0x00000008
   UINT32               minBwAlarmThreshold ;
#define MoCA_CFG_PARAM_OUT_OF_ORDER_LMO_MASK          0x00000010
   UINT32               outOfOrderLmo ;
#define MoCA_CFG_PARAM_LMO_REPORT_EN_MASK             0x00000020
   UINT32               lmoReportEn ;
#define MoCA_CFG_PARAM_RESV1_MASK                     0x00000040
   UINT32               resv1 ;
#define MoCA_CFG_PARAM_RESV2_MASK                     0x00000080
   UINT32               resv2 ;
#define MoCA_CFG_PARAM_CONT_IE_RR_INS_MASK            0x00000100
   UINT32               continuousIERRInsert ;
#define MoCA_CFG_PARAM_CONT_IE_MAP_INS_MASK           0x00000200
   UINT32               continuousIEMapInsert ;
#define MoCA_CFG_PARAM_MAX_PKT_AGGR_MASK              0x00000400
   UINT32               maxPktAggr ;
#define MoCA_CFG_PARAM_MAX_CONSTELLATION_MASK         0x00000800
   MoCAMaxConstellation constellation ;
#define MoCA_CFG_PARAM_RESV3_MASK                     0x00001000
   UINT32               resv3 ;
#define MoCA_CFG_PARAM_PMK_EXCHG_INTVL_MASK           0x00002000
   UINT32               pmkExchangeInterval ;
#define MoCA_CFG_PARAM_TEK_EXCHG_INTVL_MASK           0x00004000
   UINT32               tekExchangeInterval ;
#define MoCA_CFG_PARAM_PRIO_ALLOCATIONS_MASK          0x00008000
   MoCAPrioAllocations  prioAllocation ;
#define MoCA_CFG_PARAM_SNR_MARGIN_MASK                0x00010000
   signed int           snrMargin ;
#define MoCA_CFG_PARAM_NOMINAL_MAP_CYCLE_MASK         0x00020000
   UINT32               nominalMapCycle ;
#define MoCA_CFG_PARAM_MAX_INC_MAP_CYCLE_MASK         0x00040000
   UINT32               maxIncMapCycle ;
#define MoCA_CFG_PARAM_MAX_DEC_MAP_CYCLE_MASK         0x00080000
   UINT32               maxDecMapCycle ;
#define MoCA_CFG_PARAM_TARGET_PHY_RATE_QAM128_MASK    0x00100000
   UINT32               targetPhyRateQAM128 ;
#define MoCA_CFG_PARAM_TARGET_PHY_RATE_QAM256_MASK    0x00200000
   UINT32               targetPhyRateQAM256 ;
#define MoCA_CFG_PARAM_CORE_TRACE_CONTROL_MASK        0x00400000
   UINT32               coreTraceControl ;
#define MoCA_CFG_PARAM_SNR_MARGIN_OFFSET_MASK         0x00800000
   signed int           snrMarginOffset [MoCA_MAX_SNR_TBL_INDEX] ;

   /* All these lab configurations should appear in the end of the
    * structure, Important restriction to keep in mind.
    */
#define MoCA_CFG_PARAM_LAB_START_MASK                 MoCA_CFG_PARAM_LAB_PILOTS_MASK
#define MoCA_CFG_PARAM_LAB_PILOTS_MASK                0x01000000
   UINT8                pilots [8] ;
#define MoCA_CFG_PARAM_IQ_DIAGRAM_SET_MASK            0x02000000
   MoCAIqDiagramSet     iq ;
#define MoCA_CFG_PARAM_SNR_GRAPH_SET_MASK             0x04000000
   UINT32               MoCASnrGraphSetNodeId ;
#define MoCA_CFG_PARAM_LAB_REG_MEM_MASK               0x08000000
   MoCALabRegMem        RegMem ;
#define MoCA_CFG_PARAM_LAB_CALL_FUNC_MASK             0x10000000
   MoCALabCallFunc      callFunc ;

   /* Following are the translated values from User Anytime parameter to
    * MoCA core anytime parameter.
    * User Management application does not need to be bothered with this. This
    * is used by the MoCA control Library.
    */
   struct _snrMarginTable {
      UINT32            table1 [MoCA_MAX_SNR_TBL_INDEX] ;
      UINT32            table2 [MoCA_MAX_SNR_TBL_INDEX] ;
   } snrMarginTable ;
} MoCA_CONFIG_PARAMS, *PMoCA_CONFIG_PARAMS ;


typedef struct _MoCAPQoSParamsEntry {
   MAC_ADDRESS    ethDestAddr ;
   MAC_ADDRESS    ethSrcAddr ;
   UINT32         ethTypeOrLen ;
   UINT32         qosMask ;
   UINT32         tciPri ;
   UINT32         vlanId ;
   UINT32         tciMask ;
   UINT32         rulePrecedence ;
   UINT32         peakDataRate ;
   UINT32         pktSize ;
   UINT32         leaseTime ;
   UINT32         burstSize ;
} MoCA_PQOS_PARAMS_ENTRY, *PMoCA_PQOS_PARAMS_ENTRY ;

#ifdef MoCA_PQoS_SUPPORT
typedef struct _MoCAPQoSHeaderParams {
#define MoCA_CFG_PARAM_PQoS_HDR_FMT                   8
   UINT32 hdrFmt: 8;
   UINT32 entryNodeId: 8;
   UINT32 entryIndex: 8;
   UINT32 reserved_0: 8;
   UINT32 vendorId:16;
#define MoCA_CFG_PARAM_PQoS_GENERAL_TRANS_TYPE        1
#define MoCA_CFG_PARAM_PQoS_FMR_TRANS_TYPE            2
   UINT32 transType: 8;
#define MoCA_CFG_PARAM_PQoS_CREATE_TRANS_SUBTYPE      1
#define MoCA_CFG_PARAM_PQoS_FMR_TRANS_SUBTYPE         1
#define MoCA_CFG_PARAM_PQoS_UPDATE_TRANS_SUBTYPE      2
#define MoCA_CFG_PARAM_PQoS_DELETE_TRANS_SUBTYPE      3
#define MoCA_CFG_PARAM_PQoS_LIST_TRANS_SUBTYPE        4
#define MoCA_CFG_PARAM_PQoS_QUERY_TRANS_SUBTYPE       5
   UINT32 transSubtype: 8;
   UINT32 wave0Nodemask:32;
   UINT32 reserved_1:32;
   UINT32 reserved_2: 8;
#define MoCA_CFG_PARAM_PQoS_FMR_MSG_PRIORITY          0x80
#define MoCA_CFG_PARAM_PQoS_CREATE_MSG_PRIORITY       0xF0
#define MoCA_CFG_PARAM_PQoS_UPDATE_MSG_PRIORITY       0xF0
#define MoCA_CFG_PARAM_PQoS_DELETE_MSG_PRIORITY       0xF0
#define MoCA_CFG_PARAM_PQoS_LIST_MSG_PRIORITY         0x80
#define MoCA_CFG_PARAM_PQoS_QUERY_MSG_PRIORITY        0x80
   UINT32 msgPriority: 8;
#define MoCA_CFG_PARAM_PQoS_FMR_TXN_LAST_WAVE_NUM     1
#define MoCA_CFG_PARAM_PQoS_CREATE_TXN_LAST_WAVE_NUM  2
#define MoCA_CFG_PARAM_PQoS_DELETE_TXN_LAST_WAVE_NUM  2
#define MoCA_CFG_PARAM_PQoS_UPDATE_TXN_LAST_WAVE_NUM  2
#define MoCA_CFG_PARAM_PQoS_LIST_TXN_LAST_WAVE_NUM    1
#define MoCA_CFG_PARAM_PQoS_QUERY_TXN_LAST_WAVE_NUM   1
        
   UINT32 txnLastWaveNum: 8;
   UINT32 reserved_3: 8;
} __attribute__((packed)) MoCA_PQoS_HEADER_PARAMS, *PMoCA_PQoS_HEADER_PARAMS ;

#define NUM_PQOS_CREATE_OPTIONS 10
typedef struct _MoCAPQoSCreateParams {
   UINT8   flowId[6];
   UINT32  reserved_0:16;
   UINT32  tPacketSize:16;
   UINT32  reserved_1:8;
   UINT32  ingressNodeId:8;
   UINT32  reserved_2:24;
   UINT32  egressNodeId:8;
   UINT32  flowTag;
   UINT8   packetDA[6];
   UINT32  reserved_3:24;
   UINT32  tPeakDataRate:24;
   UINT32  tLeaseTime;
   UINT32  tBurstSize:8;
   UINT32  reserved_4:24;
} __attribute__((packed)) MoCA_PQoS_CREATE_PARAMS, *PMoCA_PQoS_CREATE_PARAMS ;

#define NUM_INGR_CREATE_OPTIONS 14
typedef struct _MoCAIngrCreateParams {
   UINT32 FlowID[2];
   UINT32 FlowTag;
   UINT32 QTag;
   UINT32 TPeakDataRate;
   UINT32 TPacketSize;
   UINT32 TBurstSize;
   UINT32 TLeaseTime;
   UINT32 EgressNodeID;
   UINT32 FlowSA[2];
   UINT32 FlowDA[2];
   UINT32 FlowVlanPri;
   UINT32 FlowVlanId;
   UINT32 CommittedSTPS;
   UINT32 CommittedTXPS;
} __attribute__((packed)) MoCA_INGR_CREATE_PARAMS, *PMoCA_INGR_CREATE_PARAMS ;

#define NUM_INGR_UPDATE_OPTIONS 9
typedef struct _MoCAIngrUpdateParams {
   UINT32 FlowID[2];
   UINT32 FlowTag;
   UINT32 FlowDA[2];
   UINT32 TPeakDataRate;
   UINT32 TPacketSize;
   UINT32 TBurstSize;
   UINT32 TLeaseTime;
   UINT32 CommittedSTPS;
   UINT32 CommittedTXPS;
} __attribute__((packed)) MoCA_INGR_UPDATE_PARAMS, *PMoCA_INGR_UPDATE_PARAMS ;

#define NUM_PQOS_DELETE_OPTIONS 2
typedef struct MoCAPQoSDeleteParams{
   UINT8 flowId[6];
   UINT32  reserved_0:16;
   UINT32  reserved_1;
} __attribute__((packed)) MoCA_PQoS_DELETE_PARAMS, *PMoCA_PQoS_DELETE_PARAMS ;


#define NUM_INGR_DELETE_OPTIONS 1
typedef struct MoCAIngrDeleteParams{
   UINT32 FlowID[2];
} __attribute__((packed)) MoCA_INGR_DELETE_PARAMS, *PMoCA_INGR_DELETE_PARAMS ;


#define NUM_FMR_REQUEST_OPTIONS 1
typedef struct _MoCAFmrRequestParams {
   UINT32  reserved:32;
} __attribute__((packed)) MoCA_FMR_REQUEST_PARAMS, *PMoCA_FMR_REQUEST_PARAMS ;


#define NUM_PQOS_LIST_OPTIONS 3
typedef struct {
   UINT32  flowStartIndex;
   UINT32  flowMaxReturn:8;
   UINT32  reserved:24;
} __attribute__((packed)) MoCA_PQoS_LIST_PARAMS, *PMoCA_PQoS_LIST_PARAMS ;


#define NUM_PQOS_QUERY_OPTIONS 2
typedef struct {
   UINT32  reserved_0;
   UINT8  flowId[6];
   UINT32  reserved_1:16;
} __attribute__((packed)) MoCA_PQoS_QUERY_PARAMS, *PMoCA_PQoS_QUERY_PARAMS ;

 
#define NUM_PQOS_MAINTENANCE_OPTIONS 0

typedef struct
{
   MoCA_PQoS_HEADER_PARAMS  submitHdr;
   MoCA_PQoS_CREATE_PARAMS  submitPayload;
} MoCA_PQoS_CREATE_FULL_PARAMS, *PMoCA_PQoS_CREATE_FULL_PARAMS;

typedef struct
{
   MoCA_PQoS_HEADER_PARAMS  submitHdr;
   MoCA_PQoS_DELETE_PARAMS  submitPayload;
} MoCA_PQoS_DELETE_FULL_PARAMS, *PMoCA_PQoS_DELETE_FULL_PARAMS;

typedef struct
{
   MoCA_PQoS_HEADER_PARAMS  submitHdr;
   MoCA_PQoS_LIST_PARAMS  submitPayload;
} MoCA_PQoS_LIST_FULL_PARAMS, *PMoCA_PQoS_LIST_FULL_PARAMS;

typedef struct
{
   MoCA_PQoS_HEADER_PARAMS  submitHdr;
   MoCA_PQoS_QUERY_PARAMS  submitPayload;
} MoCA_PQoS_QUERY_FULL_PARAMS, *PMoCA_PQoS_QUERY_FULL_PARAMS;

typedef struct
{
   MoCA_PQoS_HEADER_PARAMS  submitHdr;
   MoCA_FMR_REQUEST_PARAMS     submitPayload;
} MoCA_FMR_REQUEST_FULL_PARAMS, *PMoCA_FMR_REQUEST_FULL_PARAMS;

typedef struct
{
   MoCA_INGR_CREATE_PARAMS     submitPayload;
} MoCA_INGR_CREATE_FULL_PARAMS, *PMoCA_INGR_CREATE_FULL_PARAMS;


typedef struct
{
   MoCA_INGR_UPDATE_PARAMS     submitPayload;
} MoCA_INGR_UPDATE_FULL_PARAMS, *PMoCA_INGR_UPDATE_FULL_PARAMS;

typedef struct
{
   MoCA_INGR_DELETE_PARAMS     submitPayload;
} MoCA_INGR_DELETE_FULL_PARAMS, *PMoCA_INGR_DELETE_FULL_PARAMS;
#endif

#define NUM_FUNC_CALL_OPTIONS 3
typedef struct {
   UINT32 Address;
   UINT32 param1;
   UINT32 param2;
   UINT32 param3;
   UINT32 numParams;
} MoCA_FUNC_CALL_PARAMS, *PMoCA_FUNC_CALL_PARAMS ;



#define MoCA_LOW_PRIO      0
#define MoCA_MED_PRIO      1
#define MoCA_HIGH_PRIO     2

#define MoCA_ETH_MIN_PRIO  0
#define MoCA_ETH_LOW_PRIO_MAX  3
#define MoCA_ETH_MED_PRIO_MAX  5
#define MoCA_ETH_HIGH_PRIO_MAX 7
#define MoCA_ETH_MAX_PRIO  7
#define MoCA_ETH_NUM_PRIO  8

/* MoCA standard recommendation.
 * Eth Prio 0,1,2,3 maps to MoCA_LOW_PRIO.
 * Eth Prio 4,5 maps to MoCA_MED_PRIO.
 * Eth Prio 6,7 maps to MoCA_HIGH_PRIO.
 */
typedef struct _MoCAQoSParamsEntry {
   UINT32         tciPri ;
   UINT32         MoCAPri ;
} MoCA_QOS_PARAMS_ENTRY, *PMoCA_QOS_PARAMS_ENTRY ;

typedef struct _MoCAUCFwdEntry {
   MAC_ADDRESS    nodeMacAddr ;
   UINT16         resv ;
   UINT32         ucDestNodeId ;
} MoCA_UC_FWD_ENTRY, *PMoCA_UC_FWD_ENTRY ;

typedef struct _NodeMacAddr {
   MAC_ADDRESS    macAddr ;
   UINT16         resv ;
} sNodeMacAddr ;

typedef struct _MoCAMCFwdEntry {
   UINT32         numOfMembers ;    /* number of node mac addresses */

   MAC_ADDRESS    mcMacAddr ;
   UINT16         resv ;
   sNodeMacAddr   nodeAddr [MoCA_NUM_MEMBERS_PER_MC_GROUP] ;

   UINT32         destNodeId ;      /* Read-only, For write accesses, it is dont care */
} MoCA_MC_FWD_ENTRY, *PMoCA_MC_FWD_ENTRY ;

typedef struct _MoCASrcAddrEntry {
   MAC_ADDRESS    nodeSrcMacAddr ;     /* Node address sourced from this device */
   UINT16         resv ;
   UINT32         selfNodeId ;         /* Self Node id */
} MoCA_SRC_ADDR_ENTRY, *PMoCA_SRC_ADDR_ENTRY ;

typedef struct _MoCAStatus {
   /* General status */
   struct _generalStatus {
      UINT32         vendorId ;
      UINT32         hwVersion ;
      UINT32         swVersion ;
      UINT32         selfMoCAVersion ;
      UINT32         networkVersionNumber ;
      UINT32         qam256Support ;
      UINT32         operStatus ;
      UINT32         linkStatus ;
      UINT32         connectedNodes ;
      UINT32         nodeId ;
      UINT32         ncNodeId ;
      UINT32         MoCAUpTime ;
      UINT32         linkUpTime ;
      UINT32         backupNcId ;
      UINT32         rfChannel ;
      UINT32         bwStatus ;
      UINT32         nodesUsableBitmask ;	   // Indicate the numbers of 1's in the GCD_BITMASK field 
                                             // reported in Type I Probe Reports. 
                                             // This value corresponds to the number of nodes that this 
                                             // node communicates to in the MoCA network. 
                                             // This value may be smaller than the number of nodes reported
                                             // by the NC node.
      UINT32         networkTabooMask ;   
      UINT32         networkTabooStart ; 
   } generalStatus ;

   /* Extended Status */
   struct _extendedStatus {
      signed int     lastTekExchange ;
      signed int     lastPmkExchange ;
      UINT32         MoCAVerChangeTime ;
      UINT8          pmkEvenKey [MoCA_MAX_KEY_LEN] ;
      UINT8          pmkOddKey [MoCA_MAX_KEY_LEN] ;
      UINT8          tekEvenKey [MoCA_MAX_KEY_LEN] ;
      UINT8          tekOddKey [MoCA_MAX_KEY_LEN] ;
      UINT32         networksDiscovered ;
   } extendedStatus ;

   /* Misc Status */
   struct _miscStatus {
      MAC_ADDRESS    macAddr ;
      UINT32         isNC ;
      UINT32         driverUpTime ;  /* Time In Secs. */
      UINT32         linkResetCount ;
      UINT32         lmoInfoAvailable ;
   } miscStatus ;

} MoCA_STATUS, *PMoCA_STATUS ;

/* Modes of statistics retrieval. INTERNAL is meant for manipulation of
 * 64-bit octet counters.
 */
#define MoCA_INTERNAL         1
#define MoCA_EXTERNAL         2

#define MoCA_NUM_AGGR_PKT_COUNTS 10
typedef struct _MoCAStatistics {
   /* General Stats */
   struct _generalStats {
      UINT32         inUcPkts ;
      UINT32         inDiscardPktsEcl ;
      UINT32         inDiscardPktsMac ;
      UINT32         inUnKnownPkts ;
      UINT32         inMcPkts ;
      UINT32         inBcPkts ;
      UINT32         inOctets_low ;
      UINT32         outUcPkts ;
      UINT32         outDiscardPkts ;
      UINT32         outBcPkts ;
      UINT32         outOctets_low ;
      UINT32         ncHandOffs ;
      UINT32         ncBackups ;
      UINT32         aggrPktStatsTx [MoCA_NUM_AGGR_PKT_COUNTS] ;
      UINT32         aggrPktStatsRx [MoCA_NUM_AGGR_PKT_COUNTS] ;
   } generalStats ;

   /* 64Bit Stats */
   struct _BitStats64 {
      UINT32         inOctets_hi ;
      UINT32         outOctets_hi ;
   } BitStats64 ;

   /* Extended Stats */
   struct _extendedStats {
      UINT32         rxMapPkts ;
      UINT32         rxRRPkts [MoCA_MAX_NODES] ;
      UINT32         rxBeacons ;
      UINT32         rxCtrlPkts ;
      UINT32         txBeacons ;
      UINT32         txMaps ;
      UINT32         txLinkCtrlPkts ;
      UINT32         txRRPkts ;
      UINT32         rxMapTimeouts ;
      UINT32         rxMapCrc ;
      UINT32         rxBeaconCrc ;
      UINT32         rxBeaconTimeout ;
      UINT32         rxLinkCtrlCrc ;
      UINT32         rxLinkCtrlTimeout ;
      UINT32         rxRRCrc ;
      UINT32         rxRRTimeout ;
      UINT32         rxDataTimeout ;
      UINT32         resyncAttempts ;
      UINT32         preemptionOccurs ;
      UINT32         gMiiTxBufFull ;
      UINT32         MoCARxBufFull ;
      UINT32         thisHandOffs ;
      UINT32         thisBackups ;
      UINT32         rxDataCrc ;
   } extendedStats ;
} MoCA_STATISTICS, *PMoCA_STATISTICS ;

typedef struct _MoCANodeExtendedStatus {
   UINT32         nBas ;
   UINT32         preambleType ;
   UINT32         cp ;
   UINT32         txPower ;
   UINT32         rxGain ;
   UINT32         bitLoading [MoCA_MAX_SUB_CARRIERS * MAX_BITS_PER_SUB_CARRIER/(BITS_PER_BYTE * BYTES_PER_WORD)] ;
} MoCA_NODE_EXTENDED_STATUS ;

typedef struct _MoCANodeRates {
   UINT32         txUcPhyRate ;
   UINT32         rxUcPhyRate ;
   UINT32         rxBcPhyRate ;
   UINT32         rxMapPhyRate ;
} MoCA_NODE_RATES ;

typedef struct _MoCANodeCommonRates {
   UINT32         txBcPhyRate ;
   UINT32         txMapPhyRate ;
} MoCA_NODE_COMMON_RATES ;

typedef struct _MoCANodeStatusEntry {
   /* Basic Status */
   UINT32                     nodeId ;
   MAC_ADDRESS                eui ;
   UINT16                     resv ;
   signed int                 freqOffset ;
   UINT32                     otherNodeUcPwrBackOff ;

   /* Extended Status */
   MoCA_NODE_EXTENDED_STATUS  txUc ;
   signed int                 selfUcPwrBackOff ;  /* max tx power - txUc.txPower */

   MoCA_NODE_EXTENDED_STATUS  rxUc ;
   MoCA_NODE_EXTENDED_STATUS  rxBc ;
   MoCA_NODE_EXTENDED_STATUS  rxMap ;
   MoCA_NODE_RATES            maxPhyRates ;
} MoCA_NODE_STATUS_ENTRY, *PMoCA_NODE_STATUS_ENTRY ;

typedef struct _MoCANodeCommonStatusEntry {
   MoCA_NODE_EXTENDED_STATUS  txBc ;
   MoCA_NODE_EXTENDED_STATUS  txMap ;
   MoCA_NODE_COMMON_RATES     maxCommonPhyRates ;
} MoCA_NODE_COMMON_STATUS_ENTRY, *PMoCA_NODE_COMMON_STATUS_ENTRY ;

typedef struct _MoCANodeStatisticsEntry {
   UINT32         nodeId ;
   UINT32         txPkts ;   /* Needs to be the first field, ordering cannot change, as some generalization
                              * is done in the code based on this */
   UINT32         rxPkts ;
   UINT32         rxCwUnError ;
   UINT32         rxCwCorrected ;
   UINT32         rxCwUncorrected ;
   UINT32         rxNoSync ;
   UINT32         rxNoEnergy ;
} MoCA_NODE_STATISTICS_ENTRY, *PMoCA_NODE_STATISTICS_ENTRY ;

/* Trace Configuration Parameters. */

typedef struct _MoCATraceParams {
   UINT32         traceLevel ;      /* Mask Value of the trace configuration level */
   UINT32         bRestoreDefault ; /* Force default trace level */
} MoCA_TRACE_PARAMS, *PMoCA_TRACE_PARAMS ;

/* Host Control Parameters. */

typedef struct _MoCAHostParams {
   UINT32         hostControl ;   /* Mask Value of the Host control level */
} MoCA_HOST_PARAMS, *PMoCA_HOST_PARAMS ;

/* Version Information */

typedef struct _MoCAVersion {
   UINT32         coreHwVersion ;
   UINT32         coreSwVersion ;
   UINT32         nwVersion ;
   UINT32         selfVersion ;
   UINT16         drvMnVersion ;
   UINT16         drvMjVersion ;
   UINT32         drvBuildVersion ;
} MoCA_VERSION, *PMoCA_VERSION ;

/* Asynchronous Notification Information.
 */

typedef struct _MoCAAsyncNotifyParams {
   signed int         userMoCADesc ;
} MoCA_ASYNC_NOTIFY_PARAMS, *PMoCA_ASYNC_NOTIFY_PARAMS ;

/* Asynchronous Information. All the traps that are sent to user are
 * detailed here.
 */

typedef enum _MoCAAsyncType {
	MOCA_EVENTS_LINK_STATE           = 0x1,	// Signals a MoCA link transition
	MOCA_EVENTS_ADMISSION_FAILED     = 0x2,	// Reported by the NN to indicate that its admission to the network failed because the PHY Rate between this NN node and the NC was below the NC’s Min Bandwidth Threshold 
	MOCA_EVENTS_LIMITED_BW           = 0x3,	// Indicate that the PHY link rate to one of the nodes of the network is below the Min Bandwidth Threshold specified into the General Parameters Table  
	MOCA_EVENTS_ERROR                = 0x4,	// Indicates that an error occurred
	MOCA_EVENTS_LMO_INFO             = 0x5,	// Report parameters about the recent LMO cycle.
	MOCA_EVENTS_KEY_CHANGED          = 0x6,	// Report the testbed clock timestamp when each TEK and PMK key refresh takes place.
	MOCA_EVENTS_TOPOLOGY_CHANGED     = 0x7,	// Report if a node joined/dropped from the network.
	MOCA_EVENTS_MOCA_VERSION_CHANGED = 0x8,	// Reporting a change in the Beacon MOCA_VERSION field.
   MOCA_MAX_EVENTS
} MoCA_ASYNC_TYPE ;

typedef struct _MoCAAsyncInfo {
   MoCA_ASYNC_TYPE  type ;
   union _info {
      UINT32 u32Val ;
      struct _lmoInfo {
         UINT32 nodeId ;
         UINT32 durationSec ;
         UINT32 isLmoSuccess ;
      } lmoInfo ;
      struct _keyChange {
         UINT32 timeStamp ;
         UINT32 tekPmk ;
         UINT32 evenOdd ;
      } keyChanged ;
      struct _topologyChange {
         UINT32 nodeId ;
         UINT32 joinedDropped ;
         UINT32 eventTimeSec ;
      } topologyChanged ;
      struct _versionChange {
         UINT32 newVersion ;
         UINT32 eventTimeSec ;
      } versionChange ;
   } uInfo ;
} MoCA_ASYNC_INFO, *PMoCA_ASYNC_INFO ;

#define MoCA_ALL_SUBCARRIER         MoCA_MAX_SUB_CARRIERS+1
#define MoCA_CARRIER_IQ_SIZE        4     /* 4 bytes */

typedef struct _MoCAIQData {
   UINT8       *pData ;
   UINT32      size ;
   UINT32      subCarrier ;
} MoCA_IQ_DATA, *PMoCA_IQ_DATA ;

#define MoCA_MAX_PROBES             8
#define MoCA_ALL_PROBES             MoCA_MAX_PROBES+1
#define MoCA_PROBE_POINTS           256
#define MoCA_POINT_SIZE             4     /* 4 bytes */

typedef struct _MoCASNRData {
   UINT8       *pData ;
   UINT32      size ;
   UINT32      probe ;
} MoCA_SNR_DATA, *PMoCA_SNR_DATA ;

typedef struct _MoCACIRData {
   UINT8       *pData ;
   UINT32      size ;
   UINT32      nodeId ;
} MoCA_CIR_DATA, *PMoCA_CIR_DATA ;

/***************************************************************************
 * Function Prototypes
 ***************************************************************************/

#ifdef MoCA_INIT_WITH_CONFIG_PARAMS
BCMMoCA_STATUS BcmMoCA_Initialize( PMoCA_INITIALIZATION_PARMS pInitParms, PMoCA_CONFIG_PARAMS pConfigParams, UINT32 configMask) ;
#else
BCMMoCA_STATUS BcmMoCA_Initialize( PMoCA_INITIALIZATION_PARMS pInitParms ) ;
#endif
BCMMoCA_STATUS BcmMoCA_Uninitialize( void ) ;
#ifdef MoCA_INIT_WITH_CONFIG_PARAMS
BCMMoCA_STATUS BcmMoCA_ReInitialize( PMoCA_INITIALIZATION_PARMS pReInitParms, UINT32 reInitMask, PMoCA_CONFIG_PARAMS pConfigParams, UINT32 configMask) ;
#else
BCMMoCA_STATUS BcmMoCA_ReInitialize( PMoCA_INITIALIZATION_PARMS pReInitParms, UINT32 reInitMask ) ;
#endif
BCMMoCA_STATUS BcmMoCA_GetInitParms( PMoCA_INITIALIZATION_PARMS pInitParms) ;
BCMMoCA_STATUS BcmMoCA_GetCfg( PMoCA_CONFIG_PARAMS pConfigParams) ;
BCMMoCA_STATUS BcmMoCA_SetCfg( PMoCA_CONFIG_PARAMS pConfigParams, UINT32 configMask) ;
BCMMoCA_STATUS BcmMoCA_GetPQoSTbl( PMoCA_PQOS_PARAMS_ENTRY pPQoSParamsEntry, UINT32 ulPQoSTblSize) ;
BCMMoCA_STATUS BcmMoCA_AddPQoSTblEntry(PMoCA_PQOS_PARAMS_ENTRY pPQoSParamsEntry, UINT32 ulPQoSTblSize) ;
BCMMoCA_STATUS BcmMoCA_DeletePQosTblEntry( MAC_ADDRESS nodeMacAddr) ;
BCMMoCA_STATUS BcmMoCA_GetQoSTbl( PMoCA_QOS_PARAMS_ENTRY pQoSParamsEntry, UINT32 *pulQoSTblSize) ;
BCMMoCA_STATUS BcmMoCA_ConfigQoSParams(PMoCA_QOS_PARAMS_ENTRY pQoSParamsEntry) ;
BCMMoCA_STATUS BcmMoCA_GetUCFwdTbl( PMoCA_UC_FWD_ENTRY pUcFwdEntry, UINT32 *pulUcFwdTblSize) ;
BCMMoCA_STATUS BcmMoCA_GetMCFwdTbl( PMoCA_MC_FWD_ENTRY pMcFwdEntry, UINT32 *pulMcFwdTblSize) ;
BCMMoCA_STATUS BcmMoCA_CreateMCFwdTblGroup(PMoCA_MC_FWD_ENTRY pMcFwdEntry) ;
BCMMoCA_STATUS BcmMoCA_DeleteMCFwdTblGroup(PMoCA_MC_FWD_ENTRY pMcFwdEntry) ;
BCMMoCA_STATUS BcmMoCA_AddMCFwdTblEntry( PMoCA_MC_FWD_ENTRY pMcFwdEntry) ;
BCMMoCA_STATUS BcmMoCA_DeleteMCFwdTblEntry( PMoCA_MC_FWD_ENTRY pMcFwdEntry) ;
BCMMoCA_STATUS BcmMoCA_GetSrcAddrTbl( PMoCA_SRC_ADDR_ENTRY pSrcAddrEntry, UINT32 *pulSrcAddrTblSize) ;
BCMMoCA_STATUS BcmMoCA_GetStatus( PMoCA_STATUS pMoCAStatus) ;
BCMMoCA_STATUS BcmMoCA_GetStatistics( PMoCA_STATISTICS pMoCAStatistics, UINT32 ulReset,
                                      UINT32 mode) ;
BCMMoCA_STATUS BcmMoCA_GetNodeTblStatus( PMoCA_NODE_STATUS_ENTRY pNodeStatusEntry,
                                         PMoCA_NODE_COMMON_STATUS_ENTRY pNodeCommonStatusEntry,
                                         UINT32 *pulNodeStatusTblSize) ;
BCMMoCA_STATUS BcmMoCA_GetNodeTblStatistics(
                                    PMoCA_NODE_STATISTICS_ENTRY pNodeStatisticsEntry,
                                    UINT32 *pulNodeStatisticsTblSize,
                                    UINT32 ulReset) ;
BCMMoCA_STATUS  BcmMoCA_GetNodeStatus( PMoCA_NODE_STATUS_ENTRY pNodeStatusEntry ) ;
BCMMoCA_STATUS  BcmMoCA_GetNodeStatistics( PMoCA_NODE_STATISTICS_ENTRY pNodeStatisticsEntry,
                                           UINT32 ulReset ) ;
BCMMoCA_STATUS BcmMoCA_GetTraceConfig(
                                    PMoCA_TRACE_PARAMS pTraceConfig) ;
BCMMoCA_STATUS BcmMoCA_SetTraceConfig(
                                    PMoCA_TRACE_PARAMS pTraceConfig) ;
BCMMoCA_STATUS BcmMoCA_SetHostConfig(
                                    PMoCA_HOST_PARAMS pHostConfig) ;
BCMMoCA_STATUS BcmMoCA_GetVersion( PMoCA_VERSION pMoCAVersion ) ;
BCMMoCA_STATUS BcmMoCA_SetAsyncNotifyDesc (PMoCA_ASYNC_NOTIFY_PARAMS pAsyncNotifyParams) ;
BCMMoCA_STATUS BcmMoCA_GetAsyncInfo (PMoCA_ASYNC_INFO pAsyncInfo) ;
BCMMoCA_STATUS BcmMoCA_GetIQData (PMoCA_IQ_DATA pIqData) ;
BCMMoCA_STATUS BcmMoCA_GetSNRData (PMoCA_SNR_DATA pSnrData) ;
#ifdef MoCA_PQoS_SUPPORT
BCMMoCA_STATUS BcmMoCA_FMRRequest (PMoCA_FMR_REQUEST_FULL_PARAMS KArg) ;
BCMMoCA_STATUS BcmMoCA_PQoSCreate (PMoCA_PQoS_CREATE_FULL_PARAMS KArg) ;
BCMMoCA_STATUS BcmMoCA_PQoSIngrAdd (PMoCA_INGR_CREATE_FULL_PARAMS KArg) ;
BCMMoCA_STATUS BcmMoCA_PQoSIngrUpdate (PMoCA_INGR_UPDATE_FULL_PARAMS KArg) ;
BCMMoCA_STATUS BcmMoCA_PQoSUpdate (PMoCA_PQoS_CREATE_FULL_PARAMS KArg) ;
BCMMoCA_STATUS BcmMoCA_PQoSDelete (PMoCA_PQoS_DELETE_FULL_PARAMS KArg) ;
BCMMoCA_STATUS BcmMoCA_PQoSIngrDelete (PMoCA_INGR_DELETE_FULL_PARAMS KArg) ;
BCMMoCA_STATUS BcmMoCA_GetPQoSList (PMoCA_PQoS_LIST_FULL_PARAMS KArg) ;
BCMMoCA_STATUS BcmMoCA_PQoSQuery (PMoCA_PQoS_QUERY_FULL_PARAMS KArg) ;
BCMMoCA_STATUS BcmMoCA_PQoSMaintenance ( void ) ;
#endif
BCMMoCA_STATUS BcmMoCA_DoFunctionCall (PMoCA_FUNC_CALL_PARAMS KArg) ;

#endif /* _BCMMoCACFG_H_ */
