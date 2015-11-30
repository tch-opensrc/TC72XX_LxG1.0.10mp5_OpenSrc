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
 * File Name  : bcmspucfg.h
 *
 * Description: This file contains the definitions, structures and function
 *              prototypes for the SPU Configuration driver.
 ***************************************************************************/

#if !defined(_BCMSPUCFG_H_)
#define _BCMSPUCFG_H_

/***************************************************************************
 * Constant Definitions
 ***************************************************************************/

#define SPU_TRC_LEVEL_NONE	0x00000000
#define SPU_TRC_LEVEL_DBG       0x00000001  /* flow debug */
#define SPU_TRC_LEVEL_INFO      0x00000004  /* detailed pkt debug */

/* Return status values. */
typedef enum BcmSpuStatus
{
    SPUSTS_SUCCESS = 0,
    SPUSTS_MEMERR,
    SPUSTS_ERROR
} SPU_STATUS;

typedef struct SpuStatistics
{
   UINT32         numOfEncryptions; /* # of Outbound packets for Encryption */
   UINT32         numOfDecryptions; /* # of Inbound packets for Decryption */
} SPU_STAT_PARMS, *PSPU_STAT_PARMS;




/***************************************************************************
 * Function Prototypes
 ***************************************************************************/

#endif /* _BCMSPUCFG_H_ */


 


