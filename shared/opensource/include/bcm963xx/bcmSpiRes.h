#ifndef __BCMSPIRES_H__
#define __BCMSPIRES_H__  

#define SPI_STATUS_OK                (0)
#define SPI_STATUS_INVALID_LEN      (-1)
#define SPI_STATUS_ERR              (-2)

/* legacy and HS controllers can coexist - use bus num to differentiate */
#define LEG_SPI_BUS_NUM  0
#define HS_SPI_BUS_NUM   1

#define LEG_SPI_CLOCK_DEF   2

#if defined(_BCM96816_) || defined(CONFIG_BCM96816) || defined(_BCM96362_) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM93383)
#define HS_SPI_PLL_FREQ     400000000
#else
#define HS_SPI_PLL_FREQ     133333333
#endif

#define HS_SPI_CLOCK_DEF    40000000

#define HS_SPI_BUFFER_LEN   512

int BcmSpiReserveSlave(int busNum, int slaveId, int maxFreq);
int BcmSpiReleaseSlave(int busNum, int slaveId);
int BcmSpiSyncTrans(unsigned char *txBuf, unsigned char *rxBuf, int prependcnt, int nbytes, int busNum, int slaveId);
int BcmSpi_SetFlashCtrl( int, int, int, int, int );
int BcmSpi_GetMaxRWSize( int );
int BcmSpi_Read(unsigned char *, int, int, int, int, int);
int BcmSpi_Write(unsigned char *, int, int, int, int);

#endif /* __BCMSPIRES_H__ */

