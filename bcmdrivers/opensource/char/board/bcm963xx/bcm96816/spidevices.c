/*
<:copyright-gpl
 Copyright 2002 Broadcom Corp. All Rights Reserved.

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
#if defined(CONFIG_BCM93329)

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <bcm_map_part.h>
#include <linux/device.h>
#include <bcmSpiRes.h>
#include <board.h>
#include <boardparms.h>
#include <linux/mii.h>

#define BCM_SPI_SLAVE_ID     1
#define BCM_SPI_SLAVE_BUS    LEG_SPI_BUS_NUM
#define BCM_SPI_SLAVE_FREQ   6250000

// HW SPI supports 2 modes rev0 and rev1 - should use rev1 but can go back to rev0
// by using the define below
//#define USE_REV0_SPI_IF

static struct mutex bcmSpiSlaveMutex;

#ifdef USE_REV0_SPI_IF
static int spi_setup_addr( uint32_t addr, uint32_t len )
{
   uint8_t buf[7];
   int     status;

   if ((addr & ~(len-1)) != addr)
   {
      printk(KERN_ERR "spi_setup_addr: Invalid address - bad alignment\n");
      return(-1);
   }

   buf[0] = 0x11;
   buf[1] = 0x01;
   buf[2] = ((1 << len) - 1) << ((4 - len) - (addr & 3));
   buf[3] = (uint8_t)(addr >> 0);
   buf[4] = (uint8_t)(addr >> 8);
   buf[5] = (uint8_t)(addr >> 16);
   buf[6] = (uint8_t)(addr >> 24);

   status = BcmSpiSyncTrans(buf, NULL, 0, 7, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
   if ( SPI_STATUS_OK != status )
   {
      printk(KERN_ERR "spi_setup_addr: BcmSpiSyncTrans error\n");
      return(-1);
   }

   return(0);
}

static int spi_read_status(uint8_t *data)
{
   uint8_t read_status[2] = {0x10, 0x00};
   int     status;

   status = BcmSpiSyncTrans(read_status, &read_status[0], 2, 1, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
   if ( SPI_STATUS_OK != status )
   {
      printk(KERN_ERR "spi_read_status: BcmSpiSyncTrans returned error\n");
      *data = read_status[0];
      return(-1);
   }

   *data = read_status[0];
   return(0);
}

int kerSysBcmSpiSlaveRead(unsigned long addr, unsigned long *data, unsigned long len)
{
   uint8_t buf[4] = { 0, 0, 0, 0 };
   int     status;

   *data = 0;
   mutex_lock(&bcmSpiSlaveMutex);

   addr &= 0x1fffffff;
   spi_setup_addr( addr, len );

   buf[0] = 0x11;
   buf[1] = 0x01;
   buf[2] = (((1 << len) - 1) << ((4 - len) - (addr & 3))) | 0x20;
   status = BcmSpiSyncTrans(&buf[0], NULL, 0, 3, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
   if ( SPI_STATUS_OK != status )
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveRead: BcmSpiSyncTrans returned error\n");
      mutex_unlock(&bcmSpiSlaveMutex);
      return(-1);
   }

   buf[0] = 0x12;
   buf[1] = (uint8_t)(addr >> 0);
   status = BcmSpiSyncTrans(&buf[0], &buf[0], 2, len, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
   if ( SPI_STATUS_OK != status )
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveRead: BcmSpiSyncTrans returned error\n");
      mutex_unlock(&bcmSpiSlaveMutex);
      return(-1);
   }
   
   *data = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
   *data >>= ((4 - len) * 8);

   if((spi_read_status(&buf[0]) == -1) || (buf[0] & 0x0f))
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveRead: spi_read_status returned error - %02x\n", buf[0]);
      mutex_unlock(&bcmSpiSlaveMutex);
      return(-1);
   }
   
   mutex_unlock(&bcmSpiSlaveMutex);

   return(0);
}

int kerSysBcmSpiSlaveWrite(unsigned long addr, unsigned long data, unsigned long len)
{
   uint8_t buf[6];
   int     status;

   mutex_lock(&bcmSpiSlaveMutex);

   addr &= 0x1fffffff;
   if(spi_setup_addr(addr, len) == -1)
   {
      mutex_unlock(&bcmSpiSlaveMutex);
      return(-1);
   }
   
   data <<= 8 * (4 - len);

   buf[0] = 0x13;
   buf[1] = addr & 0xff;
   buf[2] = data >> 24;
   buf[3] = data >> 16;
   buf[4] = data >> 8;
   buf[5] = data >> 0;
   status = BcmSpiSyncTrans(buf, NULL, 0, 2 + len, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
   if ( SPI_STATUS_OK != status )
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveWrite: BcmSpiSyncTrans returned error\n");
      mutex_unlock(&bcmSpiSlaveMutex);
      return(-1);
   }

   if((spi_read_status(buf) == -1) || (buf[0] & 0x0f))
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveWrite: spi_read_status returned error - %02x\n", buf[0]);
      mutex_unlock(&bcmSpiSlaveMutex);
      return(-1);
   }

   mutex_unlock(&bcmSpiSlaveMutex);

   return(0);
}


int kerSysBcmSpiSlaveWriteBuf(unsigned long addr, unsigned long *data, unsigned long len, unsigned int unitSize)
{
   int            status;
   int            maxSize;
   unsigned char *pWriteData;
   unsigned long  nBytes = 0;
   unsigned long  length = len;   

   /* determine the maximum size transfer taking setup bytes and unitSize into consideration */
   maxSize    = BcmSpi_GetMaxRWSize(BCM_SPI_SLAVE_BUS);
   maxSize   -= 2;
   maxSize   &= ~(unitSize - 1);
   pWriteData = kmalloc(maxSize+2, GFP_KERNEL);
   if ( NULL == pWriteData )
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveWriteBuf: Out of memory\n");
      return(SPI_STATUS_ERR);
   }

   mutex_lock(&bcmSpiSlaveMutex);

   addr &= 0x1fffffff;
   while ( length > 0 )
   {
      if(spi_setup_addr(addr, unitSize) == -1)
      {
         mutex_unlock(&bcmSpiSlaveMutex);
         status = SPI_STATUS_ERR;
         goto out;
      }
      
      nBytes        = (length > maxSize) ? maxSize : length;
      pWriteData[0] = 0x13;
      pWriteData[1] = addr & 0xff;
      memcpy(&pWriteData[2], data, nBytes);
      status = BcmSpiSyncTrans(&pWriteData[0], NULL, 0, nBytes+2, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
      if ( SPI_STATUS_OK != status )
      {
         printk(KERN_ERR "kerSysBcmSpiSlaveWriteBuf: BcmSpiSyncTrans returned error\n");
         status = SPI_STATUS_ERR;
         goto out;
      }
      
      if((spi_read_status(pWriteData) == -1) || (pWriteData[0] & 0x0f))
      {
         printk(KERN_ERR "kerSysBcmSpiSlaveWrite: spi_read_status returned error - %02x\n", pWriteData[0]);
         status = SPI_STATUS_ERR;
         goto out;
      }
      addr    = (unsigned int)addr + nBytes;
      data    = (unsigned long *)((unsigned long)data + nBytes);
      length -= nBytes;
   }
   
out:
   mutex_unlock(&bcmSpiSlaveMutex);
   kfree(pWriteData);

   return( status );
}

#else

int kerSysBcmSpiSlaveRead(unsigned long addr, unsigned long *data, unsigned long len)
{
   uint8_t buf[6] = { 0, 0, 0, 0, 0, 0 };
   int     status;
   int     pollCount = 0;;

   *data = 0;
   switch ( len )
   {
      case 1: buf[2] = 0x00; break;
      case 2: buf[2] = 0x20; break;
      case 4: buf[2] = 0x40; break;
      default: return(SPI_STATUS_INVALID_LEN);
   }

   mutex_lock(&bcmSpiSlaveMutex);
   buf[0] = 0x11;
   buf[1] = 0x03;
   status = BcmSpiSyncTrans(&buf[0], NULL, 0, 3, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
   if ( SPI_STATUS_OK != status )
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveRead: BcmSpiSyncTrans returned error\n");
      mutex_unlock(&bcmSpiSlaveMutex);
      return(SPI_STATUS_ERR);
   }

   addr &= 0x1fffffff;
   buf[0] = 0x10;
   buf[1] = 0xC0 | ((addr >>  0) & 0x3f);
   buf[2] = 0x80 | ((addr >>  6) & 0x7f);
   buf[3] = 0x80 | ((addr >> 13) & 0x7f);
   buf[4] = 0x80 | ((addr >> 20) & 0x7f);
   buf[5] = 0x00 | ((addr >> 27) & 0x1f);
   status = BcmSpiSyncTrans(&buf[0], NULL, 0, 6, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
   if ( SPI_STATUS_OK != status )
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveRead: BcmSpiSyncTrans returned error\n");
      mutex_unlock(&bcmSpiSlaveMutex);
      return(SPI_STATUS_ERR);
   }

   while( 1 )
   {
      pollCount++;
      
      buf[0] = 0x10;
      buf[1] = 0x80 | (addr & 0x3f);
      status = BcmSpiSyncTrans(&buf[0], &buf[0], 2, len+1, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
      if ( SPI_STATUS_OK != status )
      {
         printk(KERN_ERR "kerSysBcmSpiSlaveRead: BcmSpiSyncTrans returned error\n");
         mutex_unlock(&bcmSpiSlaveMutex);
         return(-1);
      }

      if ( 0x01 & buf[0] )
      {
         /* completed successfully */
         break;
      }
      else if ( 0x0E & buf[0] )
      {
         /* completed with error - read the status */
         buf[0] = 0x10;
         buf[1] = 0x02;
         status = BcmSpiSyncTrans(&buf[0], &buf[0], 2, 1, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
         if ( SPI_STATUS_OK != status )
         {
            printk(KERN_ERR "kerSysBcmSpiSlaveWrite: BcmSpiSyncTrans returned error\n");
            mutex_unlock(&bcmSpiSlaveMutex);
            return(SPI_STATUS_ERR);
         }
         printk(KERN_ERR "kerSysBcmSpiSlaveRead: SPI error: %x\n", buf[0] );
         mutex_unlock(&bcmSpiSlaveMutex);
         return(SPI_STATUS_ERR);
      }
      // else not completed

      if ( pollCount > 10 )
      {
         printk(KERN_ERR "kerSysBcmSpiSlaveRead: SPI timeout: %x\n", buf[0] );
         mutex_unlock(&bcmSpiSlaveMutex);
         return(SPI_STATUS_ERR);
      }

      udelay( 100 );
   }

   *data = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
   *data >>= ((4 - len) * 8);
   
   mutex_unlock(&bcmSpiSlaveMutex);

   return( SPI_STATUS_OK );
}

int kerSysBcmSpiSlaveWrite(unsigned long addr, unsigned long data, unsigned long len)
{
   uint8_t buf[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
   int     status;
   int     pollCount = 0;

   switch ( len )
   {
      case 1: buf[2] = 0x00; break;
      case 2: buf[2] = 0x20; break;
      case 4: buf[2] = 0x40; break;
      default: return(SPI_STATUS_INVALID_LEN);
   }

   mutex_lock(&bcmSpiSlaveMutex);
   buf[0] = 0x11;
   buf[1] = 0x03;
   status = BcmSpiSyncTrans(&buf[0], NULL, 0, 3, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
   if ( SPI_STATUS_OK != status )
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveWrite: BcmSpiSyncTrans returned error\n");
      mutex_unlock(&bcmSpiSlaveMutex);
      return(SPI_STATUS_ERR);
   }

   addr  &= 0x1fffffff;
   data <<= 8 * (4 - len);
   buf[0] = 0x11;
   buf[1] = 0xC0 | ((addr >>  0) & 0x3f);
   buf[2] = 0x80 | ((addr >>  6) & 0x7f);
   buf[3] = 0x80 | ((addr >> 13) & 0x7f);
   buf[4] = 0x80 | ((addr >> 20) & 0x7f);
   buf[5] = 0x00 | ((addr >> 27) & 0x1f);
   buf[6] = data >> 24;
   buf[7] = data >> 16;
   buf[8] = data >> 8;
   buf[9] = data >> 0;
   status = BcmSpiSyncTrans(&buf[0], NULL, 0, 6+len, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
   if ( SPI_STATUS_OK != status )
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveWrite: BcmSpiSyncTrans returned error\n");
      mutex_unlock(&bcmSpiSlaveMutex);
      return(SPI_STATUS_ERR);
   }

   while( 1 )
   {
      pollCount++;
      
      /* read the status */
      buf[0] = 0x10;
      buf[1] = 0x02;
      status = BcmSpiSyncTrans(&buf[0], &buf[0], 2, 1, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
      if ( SPI_STATUS_OK != status )
      {
         printk(KERN_ERR "kerSysBcmSpiSlaveWrite: BcmSpiSyncTrans returned error\n");
         mutex_unlock(&bcmSpiSlaveMutex);
         return(SPI_STATUS_ERR);
      }

      if ( 0x00 == (0xC0 & buf[0]) )
      {
         if ( 0x00 != (0xE & buf[0]) )
         {
            printk(KERN_ERR "kerSysBcmSpiSlaveWrite: SPI error: %x\n", buf[0] );
            status = SPI_STATUS_ERR;
         }
         break;
      }
      
      if ( pollCount > 10 )
      {
         printk(KERN_ERR "kerSysBcmSpiSlaveWrite: SPI timeout: %x\n", buf[0] );
         status = SPI_STATUS_ERR;
         break;
      }
      // else not completed

      udelay( 100 );
   }
 
   mutex_unlock(&bcmSpiSlaveMutex);

   return( status );
}


int kerSysBcmSpiSlaveWriteBuf(unsigned long addr, unsigned long *data, unsigned long len, unsigned int unitSize)
{
   int            status;
   int            pollCount = 0;
   int            maxSize;
   unsigned char *pWriteData;
   unsigned long  length = len;
   unsigned long  nBytes = 0;

   /* determine the maximum size transfer taking setup bytes and unitSize into consideration */
   maxSize    = BcmSpi_GetMaxRWSize(BCM_SPI_SLAVE_BUS);
   maxSize   -= 2;
   maxSize   &= ~(unitSize - 1);
   pWriteData = kmalloc(maxSize+2, GFP_KERNEL);
   if ( NULL == pWriteData )
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveWriteBuf: Out of memory\n");
      return(SPI_STATUS_ERR);
   }

   switch ( unitSize )
   {
      case 1: pWriteData[2] = 0x00; break;
      case 2: pWriteData[2] = 0x20; break;
      case 4: pWriteData[2] = 0x40; break;
      default: kfree(pWriteData); return(SPI_STATUS_INVALID_LEN);
   }

   mutex_lock(&bcmSpiSlaveMutex);

   pWriteData[0] = 0x11;
   pWriteData[1] = 0x03;
   status = BcmSpiSyncTrans(&pWriteData[0], NULL, 0, 3, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
   if ( SPI_STATUS_OK != status )
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveWriteBuf: BcmSpiSyncTrans returned error\n");
      status = SPI_STATUS_ERR;
      goto out;
   }

   addr         &= 0x1fffffff;
   pWriteData[0] = 0x11;
   pWriteData[1] = 0x05;
   pWriteData[2] = (addr >>  0);
   pWriteData[3] = (addr >>  8);
   pWriteData[4] = (addr >> 16);
   pWriteData[5] = (addr >> 24);
   status = BcmSpiSyncTrans(&pWriteData[0], NULL, 0, 6, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
   if ( SPI_STATUS_OK != status )
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveWriteBuf: BcmSpiSyncTrans returned error\n");
      status = SPI_STATUS_ERR;
      goto out;
   }

   while ( length > 0 )
   {
      nBytes        = (length > maxSize) ? maxSize : length;
      pWriteData[0] = 0x11;
      pWriteData[1] = 0x80 | (addr & 0x3f);
      memcpy(&pWriteData[2], data, nBytes);

      status = BcmSpiSyncTrans(&pWriteData[0], NULL, 0, nBytes+2, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
      if ( SPI_STATUS_OK != status )
      {
         printk(KERN_ERR "kerSysBcmSpiSlaveWriteBuf: BcmSpiSyncTrans returned error\n");
         status = SPI_STATUS_ERR;
         goto out;
      }

      while( 1 )
      {
         pollCount++;
         
         /* read the status */
         pWriteData[0] = 0x10;
         pWriteData[1] = 0x02;
         status = BcmSpiSyncTrans(&pWriteData[0], &pWriteData[0], 2, 1, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID);
         if ( SPI_STATUS_OK != status )
         {
            printk(KERN_ERR "kerSysBcmSpiSlaveWriteBuf: BcmSpiSyncTrans returned error\n");
            status = SPI_STATUS_ERR;
            goto out;
         }

         if ( 0x00 == (0xC0 & pWriteData[0]) )
         {
            if ( 0x00 != (0xE & pWriteData[0]) )
            {
               printk(KERN_ERR "kerSysBcmSpiSlaveWriteBuf: SPI error: %x\n", pWriteData[0] );
               status = SPI_STATUS_ERR;
               goto out;
            }
            break;
         }
         
         if ( pollCount > 10 )
         {
            printk(KERN_ERR "kerSysBcmSpiSlaveWriteBuf: SPI timeout: %x\n", pWriteData[0] );
            status = SPI_STATUS_ERR;
            goto out;
         }
         // else not completed

         udelay( 100 );
      }

      addr    = (unsigned int)addr + nBytes;
      data    = (unsigned long *)((unsigned long)data + nBytes);
      length -= nBytes;
   }

out:
   mutex_unlock(&bcmSpiSlaveMutex);
   kfree(pWriteData);

   return( status );
}
#endif

unsigned long kerSysBcmSpiSlaveReadReg32(unsigned long addr)
{
   unsigned long data = 0;

   BUG_ON(addr & 3);
   addr &= 0x1fffffff;
   
   if(kerSysBcmSpiSlaveRead(addr, &data, 4) < 0)
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveReadReg32: can't read %08x\n", (unsigned int)addr);
   }

   return(data);
}

void kerSysBcmSpiSlaveWriteReg32(unsigned long addr, unsigned long data)
{
   BUG_ON(addr & 3);
   addr &= 0x1fffffff;

   if(kerSysBcmSpiSlaveWrite(addr, data, 4) < 0)
   {
      printk(KERN_ERR "kerSysBcmSpiSlaveWriteReg32: can't write %08x (data %08x)\n", (unsigned int)addr, (unsigned int)data);
   }

}

static int kerSysBcmSpiSlaveInit( void )
{
   unsigned long data;
   int32_t       retVal      = 0;
#ifdef USE_REV0_SPI_IF
	uint8_t       init_seq[3] = { 0x11, 0x01, 0xfc };
	uint8_t       init_cfg[3] = { 0x11, 0x01, 0x0f };
    uint8_t       init_adr[7] = { 0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
#else   
	uint8_t       init_seq[3] = { 0x11, 0x01, 0xfd };
	uint8_t       init_cfg[3] = { 0x11, 0x03, 0x58 };
	uint8_t       init_adr[7] = { 0x11, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 };
#endif 

   /* reset 6829 - in this configuration GPIO1 has nothing to do with GPON 
      it is tied to the 6829 reset pin - use the current define since it is
      the correct bit */
   GPIO->GPIOMode |=  GPIO_MODE_GPON_TX_APC_FAIL;
   GPIO->GPIODir  |=  GPIO_MODE_GPON_TX_APC_FAIL;
   GPIO->GPIOio   &= ~GPIO_MODE_GPON_TX_APC_FAIL;
   mdelay(1);
   GPIO->GPIOio   |=  GPIO_MODE_GPON_TX_APC_FAIL;
   mdelay(350);

   mutex_init(&bcmSpiSlaveMutex);

   BcmSpiReserveSlave(LEG_SPI_BUS_NUM, BCM_SPI_SLAVE_ID, BCM_SPI_SLAVE_FREQ);
   if ( SPI_STATUS_OK != BcmSpiSyncTrans(&init_seq[0], NULL, 0, 3, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID) )
   {
      printk(KERN_ALERT "bcm6829SpiIfInit: SPI write error\n");
      return -1;
   }

   if ( SPI_STATUS_OK != BcmSpiSyncTrans(&init_cfg[0], NULL, 0, 3, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID) )
   {
      printk(KERN_ALERT "bcm6829SpiIfInit: SPI write error\n");
      return -1;
   }

   if ( SPI_STATUS_OK != BcmSpiSyncTrans(&init_adr[0], NULL, 0, 7, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID) )
   {
      printk(KERN_ALERT "bcm6829SpiIfInit: SPI write error\n");
      return -1;
   }   

   if ((kerSysBcmSpiSlaveRead(0x10000000, &data, 4) == -1) ||
       (data == 0) || (data == 0xffffffff))
   {   
      printk(KERN_ERR "kerSysBcmSpiSlaveInit: Failed to read the Chip ID: 0x%08x\n", (unsigned int)data);
      return -1;
   }
   else
   {
      printk(KERN_INFO "kerSysBcmSpiSlaveInit: Chip ID: 0x%08x\n", (unsigned int)data);
   }

   return( retVal );

}

#define REG_MDIO_CTRL_WRITE                       (1 << 31)
#define REG_MDIO_CTRL_READ                        (1 << 30)
#define REG_MDIO_CTRL_EXT                         (1 << 16)
#define REG_MDIO_CTRL_EXT_BIT(id)                 (((id) >= 0x10) ? REG_MDIO_CTRL_EXT: 0)
#define REG_MDIO_CTRL_ID_SHIFT                    25
#define REG_MDIO_CTRL_ID_MASK                     (0x1f << REG_MDIO_CTRL_ID_SHIFT)
#define REG_MDIO_CTRL_ADDR_SHIFT                  20
#define REG_MDIO_CTRL_ADDR_MASK                   (0x1f << REG_MDIO_CTRL_ADDR_SHIFT)
static void ethswMdioRead6829(int phy_id, int reg, uint16_t *data)
{
    unsigned long reg_value;
    
    reg_value = 0;
    kerSysBcmSpiSlaveWrite(0xb0e000b0, 0, 4);
    reg_value = (REG_MDIO_CTRL_EXT_BIT(phy_id)) | (REG_MDIO_CTRL_READ) |
        (phy_id << REG_MDIO_CTRL_ID_SHIFT) | (reg  << REG_MDIO_CTRL_ADDR_SHIFT);
    kerSysBcmSpiSlaveWrite(0xb0e000b0, reg_value, 4);
    udelay(50);
    kerSysBcmSpiSlaveRead(0xb0e000b4, &reg_value, 2);
    *data = (uint16_t)reg_value;
    
}


static void ethswMdioWrite6829(int phy_id, int reg, uint16_t *data)
{
    unsigned long reg_value;

    reg_value = 0;
    kerSysBcmSpiSlaveWrite(0xb0e000b0, 0, 4);
    reg_value = (REG_MDIO_CTRL_EXT_BIT(phy_id)) | (REG_MDIO_CTRL_WRITE) |
        (phy_id << REG_MDIO_CTRL_ID_SHIFT) | (reg  << REG_MDIO_CTRL_ADDR_SHIFT) |
        *data;
    kerSysBcmSpiSlaveWrite(0xb0e000b0, reg_value, 4);
    mdelay(1);
    
}


void board_Init6829( void )
{
   unsigned char     portInfo6829;
   int               retVal;
   unsigned long     data;
   unsigned long     data2;
   ETHERNET_MAC_INFO EnetInfo;
   int               i;
   
   /* disable interfaces on the 6829 that are not being used */
   retVal = BpGet6829PortInfo(&portInfo6829);
   if ( (BP_SUCCESS == retVal) && (0 != portInfo6829))
   {
      /* intialize SPI access to the 6829 chip */
      kerSysBcmSpiSlaveInit();    

      /* initialize the switch */
      data = 0;
      kerSysBcmSpiSlaveWrite(0xb00000bc, data, 4);
      mdelay(1);
      
      data = EPHY_RST_1;
      kerSysBcmSpiSlaveWrite(0xb00000bc, data, 4);
      mdelay(1);
      
      data |= (RSW_MII_DUMB_FWDG_EN | RSW_HW_FWDG_EN);
      kerSysBcmSpiSlaveWrite(0xb00000bc, data, 4);
      mdelay(1);

      kerSysBcmSpiSlaveRead(0xb0000010, &data, 4);
      data &= ~SOFT_RST_SWITCH;
      kerSysBcmSpiSlaveWrite(0xb0000010, data, 4);
      mdelay(1);
      data |= SOFT_RST_SWITCH;
      kerSysBcmSpiSlaveWrite(0xb0000010, data, 4);
      mdelay(1);

      portInfo6829 &= ~0x80;
      if ( BpGetEthernetMacInfo(&EnetInfo, 1) == BP_SUCCESS )
      {
         int      phy_id;
         uint16_t mdioData;

         for (i = 0; i < 2; i++)
         {
            phy_id = EnetInfo.sw.phy_id[i];
            if ((portInfo6829 & (1 << i)) != 0)
            {
               /*reset MII */
               ethswMdioRead6829(phy_id, MII_BMCR, &mdioData);
               mdioData |= BMCR_RESET;
               ethswMdioWrite6829(phy_id, MII_BMCR, &mdioData);

               // Restart auto-negotiation
               ethswMdioRead6829(phy_id, MII_BMCR, &mdioData);
               mdioData |= BMCR_ANRESTART;
               ethswMdioWrite6829(phy_id, MII_BMCR, &mdioData);
            }
            else
            {
               ethswMdioRead6829(phy_id, MII_BMCR, &mdioData);
               mdioData |= BMCR_PDOWN;
               ethswMdioWrite6829(phy_id, MII_BMCR, &mdioData);
            }
         }
      }    

      /* MoCA reset */      
      kerSysBcmSpiSlaveRead(0xb000180C, &data, 4);
      data &= ~(MISC_MOCA_RST_REF_DIV2RST | MISC_MOCA_RST_REF_FBDIVRST);
      kerSysBcmSpiSlaveWrite(0xb000180C, data, 4);
      data &= ~MISC_MOCA_RST_REF_VCRST;
      kerSysBcmSpiSlaveWrite(0xb000180C, data, 4);
      data &= ~(MISC_MOCA_RST_REF_OUTDIV_RESET_M_MASK | MISC_MOCA_RST_REF_MDIV2RST);
      kerSysBcmSpiSlaveWrite(0xb000180C, data, 4);

      kerSysBcmSpiSlaveRead(0xb0001804, &data2, 4);
      data2 |= (7 << MISC_MOCA_CTL_REF_QP_ICTRL_SHIFT);
      kerSysBcmSpiSlaveWrite(0xb0001804, data2, 4);
      
      data &= ~MISC_MOCA_RST_REF_LD_RESET_STRT;
      kerSysBcmSpiSlaveWrite(0xb000180C, data, 4);     

      /* Set blink rate for hardware LEDs. */
      kerSysBcmSpiSlaveRead(0xb0000090, &data, 4);
      data &= ~LED_INTERVAL_SET_MASK;
      kerSysBcmSpiSlaveWrite(0xb0000090, data, 4);
      data |= LED_INTERVAL_SET_80MS;
      kerSysBcmSpiSlaveWrite(0xb0000090, data, 4);

      /* enable required ports */
      kerSysBcmSpiSlaveWrite(0xb0e00000, 0x00, 1);
      kerSysBcmSpiSlaveWrite(0xb0e00004, 0x00, 1);
      kerSysBcmSpiSlaveWrite(0xb0e00005, 0x00, 1);
      /* disable all other ports */
      kerSysBcmSpiSlaveWrite(0xb0e00001, 0x03, 1);
      kerSysBcmSpiSlaveWrite(0xb0e00002, 0x03, 1);
      kerSysBcmSpiSlaveWrite(0xb0e00003, 0x03, 1);
      kerSysBcmSpiSlaveWrite(0xb0e00006, 0x03, 1);
      kerSysBcmSpiSlaveWrite(0xb0e00007, 0x03, 1);
      kerSysBcmSpiSlaveWrite(0xb0e00008, 0x03, 1);

      for ( i = 0; i < 8; i++ )
      {
         kerSysBcmSpiSlaveWrite(0xb0e00058 + i, 0x40, 1);
      }

      /* setup learning */
      kerSysBcmSpiSlaveWrite(0xb0e0003c, 0x1ff /*0x0190*/, 2);

      // Reset MIB counters
      kerSysBcmSpiSlaveWrite(0xb0e00200, 1, 1);
      udelay(100);
      kerSysBcmSpiSlaveWrite(0xb0e00200, 0, 1);
     
      /* force GPON laser off */
      kerSysBcmSpiSlaveWrite(0xb0004848, GPON_SERDES_LASERMODE_FORCE_OFF, 4);
      kerSysBcmSpiSlaveWrite(0xb0004850, 5, 4);

      /* setup GPIOs */
      kerSysBcmSpiSlaveWrite(0xb0000098, 0, 4);

      /* set bits for sys irq and gphy0 led */
      data  = kerSysBcmSpiSlaveReadReg32(0xb0000098);
      data |= (GPIO_MODE_GPHY0_LED | GPIO_MODE_SYS_IRQ);
      kerSysBcmSpiSlaveWriteReg32(0xb0000098, data);

      /* set direction for sys irq and gphy0 led*/
      data  = kerSysBcmSpiSlaveReadReg32(0xb0000084);
      data |= (GPIO_MODE_GPHY0_LED | GPIO_MODE_SYS_IRQ);
      kerSysBcmSpiSlaveWriteReg32(0xb0000084, data); 

      /* disable all interrupts except for MoCA general interrupt */
      kerSysBcmSpiSlaveWrite(0xb0000020, (1 << 10), 4);
      kerSysBcmSpiSlaveWrite(0xb0000024, 0, 4);  

      /* disable the APM block */
      kerSysBcmSpiSlaveWrite(0xb0008120, 0x01800000, 4);
      kerSysBcmSpiSlaveWrite(0xb0008120, 0x01C00000, 4);
      kerSysBcmSpiSlaveWrite(0xb0008124, 0x03000300, 4);
      kerSysBcmSpiSlaveWrite(0xb0008128, 0x22000000, 4);
      kerSysBcmSpiSlaveWrite(0xb000812C, 0x22000000, 4);
      kerSysBcmSpiSlaveWrite(0xb0008130, 0x22000000, 4);
      kerSysBcmSpiSlaveWrite(0xb0008134, 0x40000000, 4);

      /* disable the USBH */
      data  = kerSysBcmSpiSlaveReadReg32(0xb0002718);
      data &= ~(1 << 3);
      kerSysBcmSpiSlaveWrite(0xb0002718, data, 4);

      data  = kerSysBcmSpiSlaveReadReg32(0xb0002710);
      data &= ~((3<<30) | (3<<28) | (1<<24) | (1<<25));
      kerSysBcmSpiSlaveWrite(0xb0002710, data, 4);

      /* disable DDR */
      // temp -- check strap to make sure boot select is reserved otherwise DDR access hangs
      data  = kerSysBcmSpiSlaveReadReg32(0xb0001814);
      if ( 2 == ((data & MISC_STRAP_BUS_BOOT_SEL_MASK)>>MISC_STRAP_BUS_BOOT_SEL_SHIFT))
      {
         kerSysBcmSpiSlaveWrite(0xb0003238, 0x80000177, 4);
         kerSysBcmSpiSlaveWrite(0xb000333C, 0x800FFFFF, 4);
         kerSysBcmSpiSlaveWrite(0xb000343C, 0x800FFFFF, 4);
         kerSysBcmSpiSlaveWrite(0xb000353C, 0x800FFFFF, 4);
         kerSysBcmSpiSlaveWrite(0xb000363C, 0x800FFFFF, 4);
         kerSysBcmSpiSlaveWrite(0xb0003444, 0x00000001, 4);
         kerSysBcmSpiSlaveWrite(0xb0003204, 0x00000001, 4);
      }      

      /* disable clocks */
      data = SPI_CLK_EN | ROBOSW_CLK_EN | GPON_SER_CLK_EN;
      kerSysBcmSpiSlaveWrite(0xb0000004, data, 4);
      udelay(500); 
   }
}

#if 0
void board_Reset6829( void )
{
   unsigned char portInfo6829;
   int           retVal;
        
   retVal = BpGet6829PortInfo(&portInfo6829);
   if ( (BP_SUCCESS == retVal) && (0 != portInfo6829))
   {
      kerSysBcmSpiSlaveWriteReg32(0xb0000008, 0x1);
      kerSysBcmSpiSlaveWriteReg32(0xb0000008, 0x0);
   }
}
#endif

#endif
