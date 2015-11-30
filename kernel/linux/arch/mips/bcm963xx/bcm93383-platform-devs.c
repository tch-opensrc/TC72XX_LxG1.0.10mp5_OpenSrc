/******************************************************************************
 * Copyright 2009-2012 Broadcom Corporation
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php,
 * or by writing to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA
 *
 *****************************************************************************/
/**************************************************************************
 * File Name  : platform-devs.c
 *
 * Description:
 *
 * Updates    : 12/15/2008  	   Created.
 ****************************************************************************/
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/resource.h>
#include <3383_map_part.h>
#include <3383_intr.h>
#include <linux/dma-mapping.h>


#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/map.h>
#include <linux/mtd/bchp_nand.h>


#define NUM_CS 1

/*
 * PWRON and PWRFLT for the two USB Host ports are
 * GPIOs 97-99, which are controlled via the  GPIO_Data_Hi 
 * register in the GPIO block. The PWRFLT lines are inputs
 * while the PWRON lines are outputs. The default pinmux
 * configures these pins correctly (I think).
 */
#define GPIO_USB0_PWRFLT  (1 << 1)
#define GPIO_USB0_PWRON   (1 << 2)
#define GPIO_USB1_PWRFLT  (1 << 3)
#define GPIO_USB1_PWRON   (1 << 4)

#define GPIO_DATA_HI	((volatile int *) 0xb4e0018c)

static unsigned char noflash = 0;

static int brcm_chip_usb_init()
{
   int i;

  /* 
   * Disable interrupts; this is a critical section 
   * that accesses common registers to turn on clocks
   * and sets up pinmuxes.
   */

  /* Turn on UBUS USB clock */
  PERF->ClkCtrlUBus |= (USB_UBUS_CLK_EN);

  /* Next, turn on USB clocks */
  PERF->ClkCtrlLow |= (USB_CLK_EN);

  USB_CTL->CtrlSetup |= (SFTRST);
  for(i=0;i<1000;i++);
  USB_CTL->CtrlSetup &= ~(SFTRST);

  /* Set the polarity of the PWRFLT lines */
  // USB_CTL->CtrlSetup |= (IOC | WABO | FNBO); 
  /* USB_CTL->CtrlSetup &= ~BABO; */
  USB_CTL->CtrlSetup |= IOC;

  // USB_CTL->SwapCtrl |= (EHCI_SWAP | OHCI_SWAP);
  USB_CTL->SwapCtrl |= 0x9;

  USB_CTL->PllCtrl1 = 0x512750c0;

  /* Finally, power on the two ports */
  //*GPIO_DATA_HI |= (GPIO_USB0_PWRON | GPIO_USB1_PWRON);

  /* Re-enable interrupts */
  return 0;
}

static struct platform_device *brcm_new_usb_host(char *name, int id,
	uintptr_t base, int irq)
{
  struct resource res[2];
  struct platform_device *pdev;
  static const u64 usb_dmamask = DMA_BIT_MASK(32);

  memset(&res, 0, sizeof(res));
  res[0].start = base;
  res[0].end = base + 0xff;
  res[0].flags = IORESOURCE_MEM;

  res[1].start = res[1].end = irq;
  res[1].flags = IORESOURCE_IRQ;

  pdev = platform_device_alloc(name, id);
  platform_device_add_resources(pdev, res, 2);

  pdev->dev.dma_mask = (u64 *)&usb_dmamask;
  pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

  return pdev;
}

#if 0
static struct mtd_partition fixed_partition_map[] = {
    /* name			offset		size */
    { "entire_device",	0x00000000,	MTDPART_SIZ_FULL },
};

/*
 * Use the partition map defined at compile time
 */
int __init get_partition_map(struct mtd_partition **p)
{
  *p = fixed_partition_map;
  return ARRAY_SIZE(fixed_partition_map);
}

static int __init brcm_mtd_setup(void)
{
  struct platform_device *pdev;
  struct mtd_partition *parts;
  struct brcmnand_platform_data pdata;
  int nr_parts;
  int i;

  if (noflash)
      return 0;

  nr_parts = get_partition_map(&parts);

  /* scan each chip select to see what (if anything) lives there */
  for (i = 0; i < NUM_CS; i++) 
  {
      if ((*(volatile unsigned int *)BCHP_NAND_CS_NAND_SELECT) & (0x100 << i))
      {
          pdata.chip_select = i;
  	  pdata.nr_parts = nr_parts;
	  /* We'll parse MTD partitions later, during driver probing */
  	  pdata.parts = NULL;

  	  pdev = platform_device_alloc("brcmnand", 0);
  	  if (!pdev || platform_device_add_data(pdev, &pdata, sizeof(pdata)) || platform_device_add(pdev))
             platform_device_put(pdev);
      }
  }

  return 0;
}

/*
 * late_initcall means the flash drivers are already loaded, so we control
 * the order in which the /dev/mtd* devices get created.
 */
late_initcall(brcm_mtd_setup);
#endif

int bcm93383_platform_init(void)
{
  struct platform_device *pdev;

  /* First setup the clocks and power on the controllers */
  brcm_chip_usb_init();

  /* Add the OHCI Host controller */
  // pdev = brcm_new_usb_host("OHCI-brcm", 0, OHCI_BASE, INTERRUPT_ID_USB_OHCI);
  // platform_device_add(pdev);

  /* Add the EHCI Host controller */
  pdev = brcm_new_usb_host("EHCI-brcm", 0, EHCI_BASE, INTERRUPT_ID_USB_EHCI);
  platform_device_add(pdev);   

  printk("Added platform devs!\n");
  return 0;
}

arch_initcall(bcm93383_platform_init);
