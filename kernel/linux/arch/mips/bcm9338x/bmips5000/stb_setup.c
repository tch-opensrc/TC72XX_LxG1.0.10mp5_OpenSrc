/*
 * Copyright (C) 2009 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/module.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/serial_8250.h>
#include <linux/platform_device.h>
#include <linux/ahci_platform.h>
#include <linux/bootmem.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/compiler.h>
#include <linux/bmoca.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/mmc/sdhci-pltfm.h>

#include <asm/serial.h>
#include <asm/reboot.h>
#include <asm/addrspace.h>
#include <asm/irq.h>
#include <asm/cpu-features.h>
#include <asm/war.h>
#include <asm/brcmstb/brcmstb.h>

#ifndef CONFIG_MTD
/* squash MTD warning on IKOS builds */
#define CONFIG_MTD_MAP_BANK_WIDTH_1 1
#endif

#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/map.h>

/* Default SPI flash chip selects to scan at boot time
   Can be overriden with spics=N kernel boot argument
*/
#define EXTRA_SPI_CS		0x00

/***********************************************************************
 * Platform device setup
 ***********************************************************************/

#ifdef CONFIG_BRCM_HAS_SATA3

#define SATA_MEM_START		BPHYSADDR(BCHP_SATA_AHCI_GHC_REG_START)
#define SATA_MEM_SIZE		0x00010000

static struct resource brcm_ahci_resource[] = {
	[0] = {
		.start	= SATA_MEM_START,
		.end	= SATA_MEM_START + SATA_MEM_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= BRCM_IRQ_SATA,
		.end 	= BRCM_IRQ_SATA,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 brcm_ahci_dmamask = DMA_BIT_MASK(32);

static int brcm_ahci_init(struct device *dev, void __iomem *addr)
{
	brcm_pm_sata3(1);
	bchip_sata3_init();
	return 0;
}

static void brcm_ahci_exit(struct device *dev)
{
	brcm_pm_sata3(0);
}

static int brcm_ahci_suspend(struct device *dev)
{
	brcm_pm_sata3(0);
	return 0;
}

static int brcm_ahci_resume(struct device *dev)
{
	brcm_pm_sata3(1);
	bchip_sata3_init();
	return 0;
}

static struct ahci_platform_data brcm_ahci_pdata = {
	.init = &brcm_ahci_init,
	.exit = &brcm_ahci_exit,
	.suspend = &brcm_ahci_suspend,
	.resume = &brcm_ahci_resume,
};

static struct platform_device brcm_ahci_pdev = {
	.name		= "ahci",
	.id		= 0,
	.resource	= brcm_ahci_resource,
	.num_resources	= ARRAY_SIZE(brcm_ahci_resource),
	.dev		= {
		.dma_mask		= &brcm_ahci_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &brcm_ahci_pdata,
	},
};

#endif /* CONFIG_BRCM_HAS_SATA3 */

#define BRCM_16550_PLAT_DEVICE(uart_addr, uart_irq) \
	{ \
		.mapbase = BPHYSADDR(uart_addr), \
		.irq = (uart_irq), \
		.regshift = 2, \
		.iotype = UPIO_MEM32, \
		.flags = UPF_IOREMAP | UPF_SKIP_TEST | UPF_FIXED_TYPE, \
		.type = PORT_16550A, \
	},

static struct plat_serial8250_port brcm_16550_ports[] = {
#ifdef CONFIG_BRCM_UARTA_IS_16550
BRCM_16550_PLAT_DEVICE(BCHP_UARTA_REG_START, BRCM_IRQ_UARTA)
#endif
#ifdef CONFIG_BRCM_UARTB_IS_16550
BRCM_16550_PLAT_DEVICE(BCHP_UARTB_REG_START, BRCM_IRQ_UARTB)
#endif
#ifdef CONFIG_BRCM_UARTC_IS_16550
BRCM_16550_PLAT_DEVICE(BCHP_UARTC_REG_START, BRCM_IRQ_UARTC)
#endif
	{
		.flags = 0,
	}
};

static struct platform_device brcm_16550_uarts = {
	.name = "serial8250",
	.dev = {
		.platform_data = &brcm_16550_ports,
	},
};

static inline void brcm_bogus_release(struct device *dev)
{
}

#if defined(CONFIG_BRCM_HAS_EMAC_0)

#ifdef BCHP_ENET_TOP_REG_START
#define BCHP_EMAC_0_REG_START	BCHP_ENET_TOP_REG_START
#define BCHP_EMAC_0_REG_END	BCHP_ENET_TOP_REG_END
#endif

static struct resource bcmemac_0_resources[] = {
	[0] = {
		.start		= BPHYSADDR(BCHP_EMAC_0_REG_START),
		.end		= BPHYSADDR(BCHP_EMAC_0_REG_END) + 3,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= BRCM_IRQ_EMAC_0,
		.end		= BRCM_IRQ_EMAC_0,
		.flags		= IORESOURCE_IRQ,
	},
};

struct bcmemac_platform_data bcmemac_0_plat_data = {
#if !defined(CONFIG_BCMEMAC_EXTMII)
	.phy_type		= BRCM_PHY_TYPE_INT,
#else
	.phy_type		= BRCM_PHY_TYPE_EXT_MII,
#endif
	.phy_id			= BRCM_PHY_ID_AUTO,
};

static struct platform_device bcmemac_0_plat_dev = {
	.name			= "bcmemac",
	.id			= 0,
	.num_resources		= ARRAY_SIZE(bcmemac_0_resources),
	.resource		= bcmemac_0_resources,
	.dev = {
		.platform_data	= &bcmemac_0_plat_data,
		.release	= &brcm_bogus_release,
	},
};

#endif /* defined(CONFIG_BRCM_HAS_EMAC_0) */

#if defined(CONFIG_BRCM_HAS_EMAC_1)

static struct resource bcmemac_1_resources[] = {
	[0] = {
		.start		= BPHYSADDR(BCHP_EMAC_1_REG_START),
		.end		= BPHYSADDR(BCHP_EMAC_1_REG_END) + 3,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= BRCM_IRQ_EMAC_1,
		.end		= BRCM_IRQ_EMAC_1,
		.flags		= IORESOURCE_IRQ,
	},
};

struct bcmemac_platform_data bcmemac_1_plat_data = {
	.phy_type		= BRCM_PHY_TYPE_EXT_MII,
	.phy_id			= BRCM_PHY_ID_AUTO,
};

static struct platform_device bcmemac_1_plat_dev = {
	.name			= "bcmemac",
	.id			= 1,
	.num_resources		= ARRAY_SIZE(bcmemac_1_resources),
	.resource		= bcmemac_1_resources,
	.dev = {
		.platform_data	= &bcmemac_1_plat_data,
		.release	= &brcm_bogus_release,
	},
};
#endif /* defined(CONFIG_BRCM_HAS_EMAC_1) */

#if defined(CONFIG_BRCM_SDIO)

static void __init brcm_add_sdio_host(int id, uintptr_t cfg_base,
	uintptr_t host_base, int irq)
{
	struct resource res[2];
	struct platform_device *pdev;

	/*
	 * CFE will disable EMMC (via CFG SCRATCH bit 0) if something else is
	 * connected to the shared EMMC/EBI pins
	 */
	if (bchip_sdio_init(id, cfg_base) < 0)
		return;

	memset(&res, 0, sizeof(res));
	res[0].start = BPHYSADDR(host_base);
	res[0].end = BPHYSADDR(host_base + 0xff);
	res[0].flags = IORESOURCE_MEM;

	res[1].start = res[1].end = irq;
	res[1].flags = IORESOURCE_IRQ;

	pdev = platform_device_alloc("sdhci", id);
	platform_device_add_resources(pdev, res, 2);
	platform_device_add_data(pdev, &sdhci_brcm_pdata,
		sizeof(sdhci_brcm_pdata));
	platform_device_add(pdev);
}

#endif /* defined(CONFIG_BRCM_SDIO) */

#define CAP_ACTIVE		0x01
#define CAP_LAST		0x02
#define CAP_TYPE		0x0c
#define CAP_TYPE_SHIFT		0x02
#define CAP_TYPE_EHCI		0x00
#define CAP_TYPE_OHCI		0x01
#define CAP_ADDR		0xffffff00

static void __init brcm_add_usb_host(int type, int id, uintptr_t base)
{
	struct resource res[2];
	struct platform_device *pdev;
	static const u64 usb_dmamask = ~(u32)0;
#ifdef CONFIG_BRCM_USB_DISABLE_MASK
	const unsigned long usb_disable_mask = CONFIG_BRCM_USB_DISABLE_MASK;
#else
	const unsigned long usb_disable_mask = 0x00;
#endif
	const int ehci_irqlist[] = BRCM_IRQLIST_EHCI;
	const int ohci_irqlist[] = BRCM_IRQLIST_OHCI;

	if (usb_disable_mask & (1 << id))
		return;

	memset(&res, 0, sizeof(res));
	res[0].start = BPHYSADDR(base);
	res[0].end = BPHYSADDR(base + 0xff);
	res[0].flags = IORESOURCE_MEM;

	res[1].flags = IORESOURCE_IRQ;

	if (type == CAP_TYPE_EHCI) {
		res[1].start = res[1].end = ehci_irqlist[id];
		pdev = platform_device_alloc("ehci-brcm", id);
	} else {
		res[1].start = res[1].end = ohci_irqlist[id];
		pdev = platform_device_alloc("ohci-brcm", id);
	}

	platform_device_add_resources(pdev, res, 2);

	pdev->dev.dma_mask = (u64 *)&usb_dmamask;
	pdev->dev.coherent_dma_mask = 0xffffffff;

#if defined(CONFIG_BCM7425B0) || defined(CONFIG_BCM7435A0) || \
	defined(CONFIG_BCM7435B0)
	/* SWLINUX-2259: Prevent OHCI from doing DMA to memc1 */
	if (type == CAP_TYPE_OHCI) {
		static const u64 lowmem_dma_mask = DMA_BIT_MASK(31);
		pdev->dev.dma_mask = (u64 *)&lowmem_dma_mask;
		pdev->dev.coherent_dma_mask = (u32)lowmem_dma_mask;
	}
#endif

	platform_device_add(pdev);
}

static void __init brcm_add_usb_hosts(void)
{
#if defined(CONFIG_BRCM_HAS_USB_CAPS)
	unsigned long caplist[] = { BCHP_USB_CAPS_REG_START,
#if defined(BCHP_USB1_CAPS_REG_START)
				    BCHP_USB1_CAPS_REG_START,
#endif
				    0 };
	unsigned long capp, cap;
	int i, ehci_id = 0, ohci_id = 0;

	for (i = 0; caplist[i] != 0; i++) {
		capp = caplist[i];
		do {
			cap = BDEV_RD(capp);
			capp += 4;

			switch ((cap & CAP_TYPE) >> CAP_TYPE_SHIFT) {
			case CAP_TYPE_EHCI:
				if (cap & CAP_ACTIVE)
					brcm_add_usb_host(CAP_TYPE_EHCI,
						ehci_id, cap & CAP_ADDR);
				ehci_id++;
				break;
			case CAP_TYPE_OHCI:
				if (cap & CAP_ACTIVE)
					brcm_add_usb_host(CAP_TYPE_OHCI,
						ohci_id, cap & CAP_ADDR);
				ohci_id++;
				break;
			}
#if defined(CONFIG_BCM7425B0) || defined(CONFIG_BCM7231B0) || \
	defined(CONFIG_BCM7344B0) || defined(CONFIG_BCM7346B0)
		/* bug: incorrect CAP_LAST bit on some chips */
		} while (capp != (caplist[i] + 0x20));
#else
		} while (!(cap & CAP_LAST));
#endif
	}

#else /* CONFIG_BRCM_HAS_USB_CAPS */

#define USB_BASE(shortname)	BCHP_##shortname##_REG_START

#if defined(BCHP_USB_EHCI_REG_START)
	brcm_add_usb_host(CAP_TYPE_EHCI, 0, USB_BASE(USB_EHCI));
#endif
#if defined(BCHP_USB_EHCI1_REG_START)
	brcm_add_usb_host(CAP_TYPE_EHCI, 1, USB_BASE(USB_EHCI1));
#endif
#if defined(BCHP_USB_EHCI2_REG_START)
	brcm_add_usb_host(CAP_TYPE_EHCI, 2, USB_BASE(USB_EHCI2));
#endif
#if defined(BCHP_USB_OHCI_REG_START)
	brcm_add_usb_host(CAP_TYPE_OHCI, 0, USB_BASE(USB_OHCI));
#endif
#if defined(BCHP_USB_OHCI1_REG_START)
	brcm_add_usb_host(CAP_TYPE_OHCI, 1, USB_BASE(USB_OHCI1));
#endif
#if defined(BCHP_USB_OHCI2_REG_START)
	brcm_add_usb_host(CAP_TYPE_OHCI, 2, USB_BASE(USB_OHCI2));
#endif
#if defined(BCHP_USB1_EHCI_REG_START)
	brcm_add_usb_host(CAP_TYPE_EHCI, 2, USB_BASE(USB1_EHCI));
#endif
#if defined(BCHP_USB1_EHCI1_REG_START)
	brcm_add_usb_host(CAP_TYPE_EHCI, 3, USB_BASE(USB1_EHCI1));
#endif
#if defined(BCHP_USB1_OHCI_REG_START)
	brcm_add_usb_host(CAP_TYPE_OHCI, 2, USB_BASE(USB1_OHCI));
#endif
#if defined(BCHP_USB1_OHCI1_REG_START)
	brcm_add_usb_host(CAP_TYPE_OHCI, 3, USB_BASE(USB1_OHCI1));
#endif

#endif /* CONFIG_BRCM_HAS_USB_CAPS */
}

#if defined(CONFIG_BRCM_HAS_GENET)

/* legacy names */
#if !defined(BCHP_GENET_0_SYS_REG_START)
#define BCHP_GENET_0_SYS_REG_START	BCHP_GENET_SYS_REG_START
#endif
#if !defined(BCHP_GENET_1_SYS_REG_START)
#define BCHP_GENET_1_SYS_REG_START	BCHP_MOCA_GENET_SYS_REG_START
#endif

static void __init brcm_register_genet(int id, uintptr_t base,
	int irq0, int irq1, int phy_type)
{
	struct resource res[3];
	struct platform_device *pdev;
	struct bcmemac_platform_data pdata;

	memset(&res, 0, sizeof(res));
	res[0].start = BPHYSADDR(base);
	res[0].end = BPHYSADDR(base + 0x4fff);
	res[0].flags = IORESOURCE_MEM;

	res[1].start = res[1].end = irq0;
	res[1].flags = IORESOURCE_IRQ;

	res[2].start = res[2].end = irq1;
	res[2].flags = IORESOURCE_IRQ;

	pdata.phy_type = phy_type;

	switch (phy_type) {
	case BRCM_PHY_TYPE_INT:
		pdata.phy_id = 1;
		break;
	case BRCM_PHY_TYPE_MOCA:
		pdata.phy_id = BRCM_PHY_ID_NONE;
		break;
	default:
		if (brcm_enet_no_mdio)
			pdata.phy_id = BRCM_PHY_ID_NONE;
		else
			pdata.phy_id = BRCM_PHY_ID_AUTO;
	}
	brcm_alloc_macaddr(pdata.macaddr);

	pdev = platform_device_alloc("bcmgenet", id);
	platform_device_add_resources(pdev, res, 3);
	platform_device_add_data(pdev, &pdata, sizeof(pdata));
	platform_device_add(pdev);
}
#endif /* defined(CONFIG_BRCM_HAS_GENET) */

#if defined(CONFIG_BRCM_HAS_MOCA)
static void __init brcm_register_moca(int enet_id)
{
	struct resource res[2];
	struct platform_device *pdev;
	struct moca_platform_data pdata;
	u8 macaddr[ETH_ALEN];

	bchip_moca_init();

	memset(&res, 0, sizeof(res));
	res[0].start = BPHYSADDR(BCHP_DATA_MEM_REG_START);
#ifdef BCHP_MOCA_HOSTMISC_MMP_REG_END
	res[0].end = BPHYSADDR(BCHP_MOCA_HOSTMISC_MMP_REG_END) + 3;
#else
	res[0].end = BPHYSADDR(BCHP_MOCA_HOSTMISC_REG_END) + 3;
#endif
	res[0].flags = IORESOURCE_MEM;

	res[1].start = BRCM_IRQ_MOCA;
	res[1].flags = IORESOURCE_IRQ;

	brcm_alloc_macaddr(macaddr);
	mac_to_u32(&pdata.macaddr_hi, &pdata.macaddr_lo, macaddr);

	strcpy(pdata.enet_name, "bcmgenet");
	pdata.enet_id = enet_id;
	pdata.bcm3450_i2c_addr = 0x70;
	pdata.bcm3450_i2c_base = brcm_moca_i2c_base;
	pdata.chip_id = (BRCM_CHIP_ID() << 16) | (BRCM_CHIP_REV() + 0xa0);
	pdata.hw_rev = CONFIG_BRCM_MOCA_VERS;
	pdata.rf_band = brcm_moca_rf_band;

	if (brcm_moca_i2c_base == 0) {
		printk(KERN_WARNING
			"error: bmoca I2C base addr is not set\n");
		return;
	}

	pdev = platform_device_alloc("bmoca", 0);
	platform_device_add_resources(pdev, res, 2);
	platform_device_add_data(pdev, &pdata, sizeof(pdata));
	platform_device_add(pdev);
}
#endif /* defined(CONFIG_BRCM_HAS_MOCA) */

static int __init platform_devices_setup(void)
{
	int i;

	/* UARTs */

	brcm_16550_ports[0].uartclk = brcm_base_baud0 * 16;
	for (i = 1; i < ARRAY_SIZE(brcm_16550_ports) - 1; i++)
		brcm_16550_ports[i].uartclk = brcm_base_baud * 16;
	platform_device_register(&brcm_16550_uarts);

#if defined(CONFIG_BRCM_IKOS)
	/* the remaining devices do not exist in emulation */
	return 0;
#endif

	/* USB controllers */

	if (brcm_usb_enabled) {
		bchip_usb_init();
		brcm_add_usb_hosts();
	}

	/* Network interfaces */

#if defined(CONFIG_BRCM_HAS_EMAC_0)
	brcm_alloc_macaddr(bcmemac_0_plat_data.macaddr);
	platform_device_register(&bcmemac_0_plat_dev);
#endif

#if defined(CONFIG_BRCM_HAS_EMAC_1)
	if (brcm_enet1_enabled) {
		brcm_alloc_macaddr(bcmemac_1_plat_data.macaddr);
		if (brcm_enet_no_mdio)
			bcmemac_1_plat_data.phy_id = BRCM_PHY_ID_NONE;
		platform_device_register(&bcmemac_1_plat_dev);
	}
#endif

#if defined(CONFIG_BRCM_HAS_GENET)
	/*
	 * Supported GENET configurations:
	 *
	 * GENET_0 INT (7468)
	 * GENET_0 EXT (7468 alt)
	 * GENET_0 INT, GENET_1 MOCA (7420)
	 * GENET_0 EXT, GENET_1 MOCA (7420 alt)
	 * GENET_1 MOCA (7125) (deprecated)
	 * GENET_1 EXT (7019) (deprecated)
	 *
	 * 65nm chips use GENET (0) / MOCA_GENET (1) (deprecated)
	 * 40nm chips use GENET_0 / GENET_1
	 */
	if (brcm_enet_enabled) {
		int phy_type = BRCM_PHY_TYPE_INT;
		int id = 0;

#if defined(CONFIG_BRCM_HAS_GENET_0)

#if defined(CONFIG_BRCM_MOCA_ON_GENET_0)
		if (!brcm_moca_enabled)
			phy_type = brcm_ext_mii_mode;
		else {
			phy_type = BRCM_PHY_TYPE_MOCA;
			brcm_register_moca(id);
		}
#endif

#if !defined(CONFIG_BCMGENET_0_GPHY)
		if (brcm_enet0_force_ext_mii)
#endif
			phy_type = brcm_ext_mii_mode;

		brcm_register_genet(id++, BCHP_GENET_0_SYS_REG_START,
			BRCM_IRQ_GENET_0_A, BRCM_IRQ_GENET_0_B, phy_type);
#endif /* defined(CONFIG_BRCM_HAS_GENET_0) */

#if defined(CONFIG_BRCM_HAS_GENET_1)

		phy_type = brcm_ext_mii_mode;
#if defined(CONFIG_BRCM_MOCA_ON_GENET_1)
		if (brcm_moca_enabled) {
			phy_type = BRCM_PHY_TYPE_MOCA;
			brcm_register_moca(id);
		}
#endif
#if defined(CONFIG_BCMGENET_1_GPHY)
		phy_type = brcm_ext_mii_mode;
#endif
		if (brcm_enet1_enabled)
			brcm_register_genet(id++, BCHP_GENET_1_SYS_REG_START,
				BRCM_IRQ_GENET_1_A, BRCM_IRQ_GENET_1_B,
				phy_type);
#endif /* defined(CONFIG_BRCM_HAS_GENET_1) */
	}
#endif /* defined(CONFIG_BRCM_HAS_GENET) */

#if defined(CONFIG_BRCM_SDIO)
	brcm_add_sdio_host(0, BCHP_SDIO_0_CFG_REG_START,
		BCHP_SDIO_0_HOST_REG_START, BRCM_IRQ_SDIO0);

#if defined(BCHP_SDIO_1_CFG_REG_START)
	brcm_add_sdio_host(1, BCHP_SDIO_1_CFG_REG_START,
		BCHP_SDIO_1_HOST_REG_START, BRCM_IRQ_SDIO1);
#endif /* defined(BCHP_SDIO_1_CFG_REG_START) */

#endif /* defined(CONFIG_BRCM_SDIO) */

	/* AHCI */
#if defined(CONFIG_BRCM_HAS_SATA3)
	if (brcm_sata_enabled)
		platform_device_register(&brcm_ahci_pdev);
#endif
	return 0;
}

arch_initcall(platform_devices_setup);

#if defined(CONFIG_BRCM_FLASH)

/***********************************************************************
 * Flash device setup
 ***********************************************************************/

#if defined(BCHP_EBI_CS_BASE_5)
#define NUM_CS			6
#elif defined(BCHP_EBI_CS_BASE_4)
#define NUM_CS			5
#elif defined(BCHP_EBI_CS_BASE_3)
#define NUM_CS			4
#elif defined(BCHP_EBI_CS_BASE_2)
#define NUM_CS			3
#else
#define NUM_CS			2
#endif

#define TYPE_NONE		0
#define TYPE_NOR		1
#define TYPE_NAND		2
#define TYPE_SPI		3
#define TYPE_MAX		TYPE_SPI

static const char type_names[][8] = { "NONE", "NOR", "NAND", "SPI" };

struct ebi_cs_info {
	int			type;
	unsigned long		start;
	unsigned long		len;
	int			width;
	unsigned long		base_reg;
	unsigned long		config_reg;
};

static struct ebi_cs_info cs_info[NUM_CS];

#ifdef CONFIG_BRCM_HAS_SPI
static int __init brcm_setup_spi_flash(int cs, int bus_num, int nr_parts,
	struct mtd_partition *parts)
{
	struct spi_board_info board_info;
	struct flash_platform_data *pdata;
	struct spi_master *master;

	master = spi_busnum_to_master(bus_num);
	if (!master) {
		printk(KERN_WARNING "%s: can't locate SPI master\n",
			__func__);
		return -ENODEV;
	}

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->nr_parts = nr_parts;
	pdata->parts = parts;

	memset(&board_info, 0, sizeof(board_info));

	strcpy(board_info.modalias, "m25p80");
	board_info.bus_num = bus_num;
	board_info.chip_select = cs;
	board_info.mode = SPI_MODE_3;
	board_info.platform_data = pdata;

	if (spi_new_device(master, &board_info) == NULL) {
		printk(KERN_WARNING "%s: can't add SPI device\n",
			__func__);
		kfree(pdata);
		return -ENODEV;
	}

	return 0;
}

static int __init brcm_setup_spi_master(int cs, int bus_id)
{
	struct resource res[4];
	struct brcmspi_platform_data pdata;
	struct platform_device *pdev;
	memset(&pdata, 0, sizeof(pdata));

	pdata.flash_cs = cs;

	memset(&res, 0, sizeof(res));
	res[0].start = BPHYSADDR(BCHP_HIF_MSPI_REG_START);
	res[0].end = BPHYSADDR(BCHP_HIF_MSPI_REG_END) + 3;
	res[0].flags = IORESOURCE_MEM;

	res[1].start = BRCM_IRQ_HIF_SPI;
	res[1].end = BRCM_IRQ_HIF_SPI;
	res[1].flags = IORESOURCE_IRQ;

	res[2].start = BPHYSADDR(BCHP_BSPI_REG_START);
	res[2].end = BPHYSADDR(BCHP_BSPI_REG_END) + 3;
	res[2].flags = IORESOURCE_MEM;

	res[3].start = BPHYSADDR(BCHP_BSPI_RAF_REG_START);
	res[3].end = BPHYSADDR(BCHP_BSPI_RAF_REG_END) + 3;
	res[3].flags = IORESOURCE_MEM;

	pdev = platform_device_alloc("spi_brcmstb", bus_id);
	if (!pdev ||
	    platform_device_add_resources(pdev, res, 4) ||
	    platform_device_add_data(pdev, &pdata, sizeof(pdata)) ||
	    platform_device_add(pdev)) {
		platform_device_put(pdev);
		return -ENODEV;
	}
	return 0;
}
#endif

static int __initdata extra_spi_cs = EXTRA_SPI_CS;

static int __init spics_setup(char *str)
{
	if (!*str || !*(str+1))
		return 0;
	str++;
	get_option(&str, &extra_spi_cs);
	return 0;
}

__setup("spics", spics_setup);

static void __init brcm_setup_cs(int cs, int nr_parts,
	struct mtd_partition *parts)
{
	struct platform_device *pdev;

	switch (cs_info[cs].type) {
	case TYPE_NOR: {
#ifdef CONFIG_BRCM_HAS_NOR
		struct physmap_flash_data pdata;
		struct resource res;
		static int nor_id;

		memset(&res, 0, sizeof(res));
		memset(&pdata, 0, sizeof(pdata));

		pdata.width = cs_info[cs].width;
		pdata.nr_parts = nr_parts;
		pdata.parts = parts;

		res.start = cs_info[cs].start;
		res.end = cs_info[cs].start + cs_info[cs].len - 1;
		res.flags = IORESOURCE_MEM;

		pdev = platform_device_alloc("physmap-flash", nor_id++);
		if (!pdev ||
		    platform_device_add_resources(pdev, &res, 1) ||
		    platform_device_add_data(pdev, &pdata, sizeof(pdata)) ||
		    platform_device_add(pdev))
			platform_device_put(pdev);
#endif
		break;
	}
	case TYPE_NAND: {
		struct brcmnand_platform_data pdata;
		static int nand_id;

		pdata.chip_select = cs;
		pdata.nr_parts = nr_parts;
		pdata.parts = parts;

		pdev = platform_device_alloc("brcmnand", nand_id++);
		if (!pdev ||
		    platform_device_add_data(pdev, &pdata, sizeof(pdata)) ||
		    platform_device_add(pdev))
			platform_device_put(pdev);
		break;
	}
	case TYPE_SPI: {
#ifdef CONFIG_BRCM_HAS_SPI
		const int bus_num = 0;
		static int spi_master_registered;
		int ret;

		if (!spi_master_registered) {
			/* spi master must know about all CSs used by
			   spi interface */
			int spi_cs = (1 << cs) | extra_spi_cs;
			ret = brcm_setup_spi_master(spi_cs, bus_num);
			if (ret) {
				printk(KERN_WARNING
					"%s: can't register SPI master "
					"(error %d)\n", __func__, ret);
				break;
			}
			spi_master_registered = 1;
		}

		ret = brcm_setup_spi_flash(cs, bus_num, nr_parts, parts);
		if (ret < 0) {
			printk(KERN_WARNING
				"%s: can't register SPI flash (error %d)\n",
				__func__, ret);
			break;
		}
#endif
		break;
	}
	default:
		BUG();
	}
}

static int __initdata noflash;
static int __initdata nandcs[NUM_CS];

void ebi_restore_settings(void)
{
	int i;
	for (i = 0; i < NUM_CS; i++) {
		BDEV_WR(BCHP_EBI_CS_BASE_0 + (i * 8), cs_info[i].base_reg);
#ifdef BCHP_EBI_CS_CONFIG_0
		BDEV_WR(BCHP_EBI_CS_CONFIG_0 + (i * 8), cs_info[i].config_reg);
#endif
	}
}

static struct map_info brcm_dummy_map = {
	.name			= "DUMMY",
};

static int __init brcmstb_mtd_setup(void)
{
	struct mtd_partition *parts;
	int nr_parts;
	int i, first = -1, primary = -1, primary_type = TYPE_NAND;

	if (noflash)
		return 0;

	nr_parts = board_get_partition_map(&parts);
	if (nr_parts <= 0) {
		struct mtd_info *mtd;

		/*
		 * mtd0 is normally the rootfs partition.  If it is missing,
		 * create a dummy partition so that any person or script
		 * trying to mount or erase mtd0 does not corrupt the
		 * whole flash device.
		 */
		mtd = do_map_probe("map_absent", &brcm_dummy_map);
		if (mtd)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
			mtd_device_register(mtd, NULL, 0);
#else
			add_mtd_device(mtd);
#endif

		printk(KERN_WARNING
			"warning: unable to build a flash partition map, "
			"using entire device\n");

		nr_parts = 0;
		parts = NULL;
	}

	/* parse CFE FLASH_TYPE variable */
	for (i = TYPE_NOR; i <= TYPE_MAX; i++)
		if (strcmp(brcm_mtd_flash_type, type_names[i]) == 0)
			primary_type = i;

	/* scan each chip select to see what (if anything) lives there */
	for (i = 0; i < NUM_CS; i++) {
		u32 base, size, config __maybe_unused;

		cs_info[i].type = TYPE_NONE;

		base = BDEV_RD(BCHP_EBI_CS_BASE_0 + (i * 8));
		size = base & 0x0f;

		cs_info[i].base_reg = base;
		cs_info[i].start = (base >> (13 + size)) << (13 + size);
		cs_info[i].len = 8192UL << (base & 0xf);

#ifdef BCHP_EBI_CS_CONFIG_0
		config = BDEV_RD(BCHP_EBI_CS_CONFIG_0 + (i * 8));
		cs_info[i].config_reg = config;
		if (config & BCHP_EBI_CS_CONFIG_0_enable_MASK)
			cs_info[i].type = TYPE_NOR;

		if (config & BCHP_EBI_CS_CONFIG_0_dest_size_MASK)
			cs_info[i].width = 2;
		else
			cs_info[i].width = 1;
#endif
#ifdef BCHP_EBI_CS_SPI_SELECT
		/*
		 * bits 7:0 - owned by HW (at most one bit active)
		 * bits 23:16 - owned by SW (any number of '1' bits OK)
		 * A '1' in either position means there is a SPI chip there.
		 *
		 * 65nm chips don't have SW bits 15:8 so EXTRA_SPI_CS
		 * should be set at compile time for multiple SPI flashes.
		 */
		if ((BDEV_RD(BCHP_EBI_CS_SPI_SELECT) & (0x101 << i)) ||
				(extra_spi_cs & (0x01 << i)))
			cs_info[i].type = TYPE_SPI;
#endif
#ifdef BCHP_NAND_CS_NAND_SELECT
		if (BDEV_RD(BCHP_NAND_CS_NAND_SELECT) & (0x100 << i))
			cs_info[i].type = TYPE_NAND;
#endif

		if (cs_info[i].type != TYPE_NAND && nandcs[i] != 0) {
			cs_info[i].type = TYPE_NAND;
		} else {
			/*
			 * nandcs = ineligible to be primary.  The partition
			 * information reported by CFE is not for the NAND
			 * flash if CFE doesn't know the system has a
			 * NAND flash.
			 */
			if (primary == -1 && primary_type == cs_info[i].type)
				primary = i;
		}
		if (first == -1 && cs_info[i].type != TYPE_NONE)
			first = i;
	}

	if (primary == -1) {
		if (first == -1) {
			printk(KERN_INFO "EBI: No flash devices detected\n");
			return -ENODEV;
		}
		primary = first;
		primary_type = cs_info[primary].type;
	}

	/* set up primary first, so that it owns mtd0/mtd1/(mtd2) */
	printk(KERN_INFO "EBI CS%d: setting up %s flash (primary)\n", primary,
		type_names[primary_type]);
	brcm_setup_cs(primary, nr_parts, parts);

	for (i = 0; i < NUM_CS; i++) {
		if (i != primary && cs_info[i].type != TYPE_NONE) {
			printk(KERN_INFO "EBI CS%d: setting up %s flash\n", i,
				type_names[cs_info[i].type]);
			brcm_setup_cs(i, 0, NULL);
		}
	}

	return 0;
}

/*
 * late_initcall means the flash drivers are already loaded, so we control
 * the order in which the /dev/mtd* devices get created.
 */
late_initcall(brcmstb_mtd_setup);

static int __init noflash_setup(char *str)
{
	noflash = 1;
	return 0;
}

__setup("noflash", noflash_setup);

static int __init nandcs_setup(char *str)
{
	int opts[NUM_CS + 1], i;

	if (*str == 0)
		return 0;
	get_options(str + 1, NUM_CS, opts);

	for (i = 0; i < opts[0]; i++) {
		int cs = opts[i + 1];
		if ((cs >= 0) && (cs < NUM_CS))
			nandcs[cs] = 1;
	}
	return 0;
}

__setup("nandcs", nandcs_setup);

#else /* defined(CONFIG_BRCM_FLASH) */

void ebi_restore_settings(void)
{
}

#endif /* defined(CONFIG_BRCM_FLASH) */

/***********************************************************************
 * Miscellaneous platform-specific functions
 ***********************************************************************/

void __init bus_error_init(void)
{
}

static void brcm_machine_restart(char *command)
{
/* PR21527 - Fix SMP reboot problem */
#ifdef CONFIG_SMP
	smp_send_stop();
	udelay(10);
#endif

#ifdef BCHP_SUN_TOP_CTRL_SW_RESET
	BDEV_WR_F_RB(SUN_TOP_CTRL_RESET_CTRL, master_reset_en, 1);
	BDEV_WR_F_RB(SUN_TOP_CTRL_SW_RESET, chip_master_reset, 1);
#else
	BDEV_WR_F_RB(SUN_TOP_CTRL_RESET_SOURCE_ENABLE,
		sw_master_reset_enable, 1);
	BDEV_WR_F_RB(SUN_TOP_CTRL_SW_MASTER_RESET, chip_master_reset, 1);
#endif

	while (1)
		;
}

static void brcm_machine_halt(void)
{
#ifdef CONFIG_BRCM_IRW_HALT
	/* ultra low power standby - on wakeup, system will restart */
	BDEV_WR_F_RB(SUN_TOP_CTRL_GENERAL_CTRL_1, irw_top_sw_pwroff, 0);
	BDEV_WR_F_RB(SUN_TOP_CTRL_GENERAL_CTRL_1, irw_top_sw_pwroff, 1);
#endif
#ifdef CONFIG_BRCM_HAS_AON
	/* may be S3 cold boot */
	brcm_pm_s3_cold_boot();
#endif
	while (1)
		;
}

char *__devinit brcmstb_pcibios_setup(char *str)
{
	/* implement "pci=off" command line option */
	if (!strcmp(str, "off")) {
		brcm_pci_enabled = 0;
		brcm_sata_enabled = 0;
		brcm_pcie_enabled = 0;
		return NULL;
	}
	return str;
}

void __init plat_mem_setup(void)
{
	_machine_restart = brcm_machine_restart;
	_machine_halt = brcm_machine_halt;
	pm_power_off = brcm_machine_halt;

	panic_timeout = 180;

#ifdef CONFIG_PCI
	pcibios_plat_setup = brcmstb_pcibios_setup;
#endif

	add_preferred_console("ttyS", CONFIG_BRCM_CONSOLE_DEVICE, "115200");
}
