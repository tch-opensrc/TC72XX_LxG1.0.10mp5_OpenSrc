/***********************************************************************
 *
 *  Copyright (c) 2009  Broadcom Corporation
 *  All Rights Reserved
 *
<:license-public
 * 
 ************************************************************************/

/*!\file flash_common.h
 * \brief This file contains definitions and prototypes used by both
 *        CFE and kernel.
 *
 */

#if !defined(_FLASH_COMMON_H)
#define _FLASH_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif


/** default scratch pad length */
#define SP_MAX_LEN          (8 * 1024)


/** Flash storage address information that is determined by the flash driver.
 *
 *  This structure is used by CFE and kernel.
 */
typedef struct flashaddrinfo
{
    int flash_persistent_start_blk;   /**< primary psi, for config file */
    int flash_persistent_number_blk;
    int flash_persistent_length;      /**< in bytes */
    unsigned long flash_persistent_blk_offset;
    int flash_scratch_pad_start_blk;  /**< start of scratch pad */
    int flash_scratch_pad_number_blk;
    int flash_scratch_pad_length;     /**< in bytes */
    unsigned long flash_scratch_pad_blk_offset;
    unsigned long flash_rootfs_start_offset; /**< offset from start of flash to fs+kernel image */
    int flash_backup_psi_start_blk;  /**< starting block of backup psi. Length is
                                          the same as primary psi.  
                                          Start at begining of sector, so offset is always 0.
                                          No sharing sectors with anybody else. */
    int flash_backup_psi_number_blk;  /**< The number of sectors for primary and backup
                                       *   psi may be different due to the irregular
                                       *   sector sizes at the end of the flash. */
    int flash_syslog_start_blk;  /**< starting block of persistent syslog. */
    int flash_syslog_number_blk; /**< number of blocks */
    int flash_syslog_length;     /**< in bytes, set from CFE, note busybox syslogd uses 16KB buffer.
                                      Like backup_psi, always start at beginning of sector,
                                      so offset is 0, and no sharing of sectors. */
    int flash_meta_start_blk; /**< The first block which is used for meta info such
                                   as the psi, scratch pad, syslog, backup psi.
                                   The kernel can use everything above this sector. */
} FLASH_ADDR_INFO, *PFLASH_ADDR_INFO;


/** Fill in the fInfo structure with primary PSI, scratch pad, syslog, secondary PSI info.
 *
 * @param nvRam (IN) nvram info.
 * @param fInfo (OUT) flash addr info that will be filled out by this function.
 */
void flash_init_info(const NVRAM_DATA *nvRam, FLASH_ADDR_INFO *fInfo);


/** Get the total number of bytes at the bottom of the flash used for PSI, scratch pad, etc.
 *
 * Even though this function is returning the number of bytes, it it guaranteed to
 * return the number of bytes of whole sectors at the end which are in use.
 * If customer enables backup PSI and persistent syslog, the number of bytes
 * may go above 64KB. This function replaces the old FLASH_RESERVED_AT_END define.
 *
 * @param fInfo (IN) Pointer to flash_addr_info struct.
 *
 * @return number of bytes reserved at the end.
 */
unsigned int flash_get_reserved_bytes_at_end(const FLASH_ADDR_INFO *fInfo);


#ifdef __cplusplus
}
#endif

#endif /* _FLASH_COMMON_H */

