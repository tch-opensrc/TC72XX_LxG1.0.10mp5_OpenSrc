#ifndef __CMS_CRC_H__
#define __CMS_CRC_H__

/*!\file cms_crc.h
 * \brief Header file for CRC calculation functions.
 * 
 * Also includes some defines for the CRC in front of config files.
 */


/** fixed length header in front of crc checked config file */
#define CRC_CONFIG_HEADER_LENGTH 20

/** start of the CRC header */
#define CRC_CONFIG_HEADER        "<crc="

/** Initial CRC checksum value, for use in cmsCrc_getCrc32.
 * 
 * From bcmTag.h.
 */
#define CRC_INITIAL_VALUE 0xffffffff 

/** Return the CRC32 value for the given data buffer.
 *
 * @param pdata (IN) data to calculate CRC over.
 * @param len   (IN) length of the data.
 * @param crc   (IN) initial CRC value, see CRC32_INIT_VALUE
 *
 * @return CRC32 value.
 */
UINT32 cmsCrc_getCrc32(const UINT8 *pdata, UINT32 len, UINT32 crc);


#endif

