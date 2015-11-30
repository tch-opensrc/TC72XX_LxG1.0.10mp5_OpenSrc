/*
 * Copyright (C) 2009 - 2010 Broadcom Corporation
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

#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/sysdev.h>

#include <asm/debug.h>
#include <asm/brcmstb/brcmstb.h>

#include <spaces.h>

/* NOTE: all PHYSICAL addresses */

#if defined(CONFIG_BRCM_HAS_SATA2)
#define SATA_MEM_START		(BCHP_PCIX_BRIDGE_GRB_REG_START + 0x10010000)
#define SATA_MEM_SIZE		0x00010000
#endif

/* internal controller registers for configuration reads/writes */
#define PCI_CFG_INDEX		0x04
#define PCI_CFG_DATA		0x08

#define PCIE_CFG_INDEX		0x00
#define PCIE_CFG_DATA		0x04

/* Use assigned bus numbers so "ops" can tell the controllers apart */
#define BRCM_BUSNO_PCI23	0x00
#define BRCM_BUSNO_SATA		0x01
#define BRCM_BUSNO_PCIE		0x02

#if defined(CONFIG_BCM7420)

/* 7420 uses the legacy register names */

#define BCHP_HIF_TOP_CTRL_PCI_MWIN_CTRL BCHP_HIF_TOP_CTRL_MWIN_CTRL

#define SET_BRIDGE_RESET(x)	\
	BDEV_WR_F_RB(HIF_RGR1_SW_RESET_1, PCIE_BRIDGE_SW_RESET, (x))
#define SET_PERST(x)		\
	BDEV_WR_F_RB(HIF_RGR1_SW_RESET_1, PCIE_SW_PERST, (x))

#define PCIE_IO_REG_START	_AC(0xf1000000, UL)
#define PCIE_IO_REG_SIZE	_AC(0x00000020, UL)

#else /* defined(CONFIG_BCM7420) */

#define SET_BRIDGE_RESET(x)	\
	BDEV_WR_F_RB(HIF_RGR1_SW_INIT_1, PCIE_BRIDGE_SW_INIT, (x))
#define SET_PERST(x)		\
	BDEV_WR_F_RB(HIF_RGR1_SW_INIT_1, PCIE_SW_PERST, (x))

#define PCIE_IO_REG_START	BPHYSADDR(BCHP_PCIE_EXT_CFG_REG_START)
#define PCIE_IO_REG_SIZE	0x00000008

#endif /* defined(CONFIG_BCM7420) */

#if defined(CONFIG_BCM7420)
	/*
	 * legacy interface - this can be combined with the 7420 code above,
	 * once 7425a0/a1 (both non-production revs) reach EOL
	 */

#define PCIE_OUTBOUND_WIN(win, start, len) do { \
	BDEV_WR(BCHP_PCIE_MISC_CPU_2_PCIE_MEM_WIN##win##_LO, \
		(start) + MMIO_ENDIAN); \
	BDEV_WR(BCHP_PCIE_MISC_CPU_2_PCIE_MEM_WIN##win##_HI, 0); \
	} while (0)

#else /* defined(CONFIG_BCM7420) */

#define PCIE_OUTBOUND_WIN(win, start, len) do { \
	BDEV_WR(BCHP_PCIE_MISC_CPU_2_PCIE_MEM_WIN##win##_LO, \
		(start) + MMIO_ENDIAN); \
	BDEV_WR(BCHP_PCIE_MISC_CPU_2_PCIE_MEM_WIN##win##_HI, 0); \
	BDEV_WR(BCHP_PCIE_MISC_CPU_2_PCIE_MEM_WIN##win##_BASE_LIMIT, \
		(((start) >> 20) << 4) | \
		 ((((start) + (len) - 1) >> 20) << 20)); \
	} while (0)

#endif

static int brcm_pci_read_config(struct pci_bus *bus, unsigned int devfn,
	int where, int size, u32 *data);
static int brcm_pci_write_config(struct pci_bus *bus, unsigned int devfn,
	int where, int size, u32 data);

#if defined(CONFIG_BRCM_HAS_SATA2)
static int brcm_sata_read_config(struct pci_bus *bus, unsigned int devfn,
	int where, int size, u32 *data);
static int brcm_sata_write_config(struct pci_bus *bus, unsigned int devfn,
	int where, int size, u32 data);
#endif

static inline int get_busno_pci23(void) { return BRCM_BUSNO_PCI23; }
#if defined(CONFIG_BRCM_HAS_SATA2)
static inline int get_busno_sata(void)  { return BRCM_BUSNO_SATA;  }
#endif
static inline int get_busno_pcie(void)  { return BRCM_BUSNO_PCIE;  }

static struct pci_ops brcmstb_pci_ops = {
	.read = brcm_pci_read_config,
	.write = brcm_pci_write_config,
};

#if defined(CONFIG_BRCM_HAS_SATA2)
static struct pci_ops brcmstb_sata_ops = {
	.read = brcm_sata_read_config,
	.write = brcm_sata_write_config,
};

static u32 sata_pci_reg[] = { [PCI_INTERRUPT_PIN] = 0x01 };
#endif

/*
 * Notes on PCI IO space:
 *
 * This is only really supported by PCI2.3.
 * The SATA PCI-X controller does support IO space, but libata never uses it.
 * Our PCIe core does not support IO BARs at all.  MEM only.
 * The Linux PCI code insists on having IO resources and io_map_base for
 *   all PCI controllers, so the values for PCI-X and PCIe are totally bogus.
 */
#define IO_ADDR_PCI23		0x400

#define IO_ADDR_SATA		(IO_ADDR_PCI23 + PCI_IO_SIZE)
#define IO_ADDR_PCIE		(IO_ADDR_SATA + SATA_IO_SIZE)
#define SATA_IO_SIZE		0x400
#define PCIE_IO_SIZE		0x400
#define BOGUS_IO_MAP_BASE	1

/* pci_controller resources */

static struct resource pci23_mem_resource = {
	.name			= "External PCI2.3 MEM",
	.start			= PCI_MEM_START,
	.end			= PCI_MEM_START + PCI_MEM_SIZE - 1,
	.flags			= IORESOURCE_MEM,
};

#if defined(CONFIG_BRCM_HAS_SATA2)
static struct resource sata_mem_resource = {
	.name			= "Internal SATA PCI-X MEM",
	.start			= SATA_MEM_START,
	.end			= SATA_MEM_START + SATA_MEM_SIZE - 1,
	.flags			= IORESOURCE_MEM,
};
#endif

static struct resource pcie_mem_resource = {
	.name			= "External PCIe MEM",
	.start			= PCIE_MEM_START,
	.end			= PCIE_MEM_START + PCIE_MEM_SIZE - 1,
	.flags			= IORESOURCE_MEM,
};

static struct resource pci23_io_resource = {
	.name			= "External PCI2.3 IO",
	.start			= IO_ADDR_PCI23,
	.end			= IO_ADDR_PCI23 + PCI_IO_SIZE - 1,
	.flags			= IORESOURCE_IO,
};

#if defined(CONFIG_BRCM_HAS_SATA2)
static struct resource sata_io_resource = {
	.name			= "Internal SATA PCI-X IO (unavailable)",
	.start			= IO_ADDR_SATA,
	.end			= IO_ADDR_SATA + SATA_IO_SIZE - 1,
	.flags			= IORESOURCE_IO,
};
#endif

static struct resource pcie_io_resource = {
	.name			= "External PCIe IO (unavailable)",
	.start			= IO_ADDR_PCIE,
	.end			= IO_ADDR_PCIE + PCIE_IO_SIZE - 1,
	.flags			= IORESOURCE_MEM,
};

/* Definitions for each controller */

static struct pci_controller brcmstb_pci_controller = {
	.pci_ops		= &brcmstb_pci_ops,
	.io_resource		= &pci23_io_resource,
	.mem_resource		= &pci23_mem_resource,
	.get_busno		= &get_busno_pci23,
};

#if defined(CONFIG_BRCM_HAS_SATA2)
static struct pci_controller brcmstb_sata_controller = {
	.pci_ops		= &brcmstb_sata_ops,
	.io_resource		= &sata_io_resource,
	.mem_resource		= &sata_mem_resource,
	.get_busno		= &get_busno_sata,
	.io_map_base		= BOGUS_IO_MAP_BASE,
};
#endif

static struct pci_controller brcmstb_pcie_controller = {
	.pci_ops		= &brcmstb_pci_ops,
	.io_resource		= &pcie_io_resource,
	.mem_resource		= &pcie_mem_resource,
	.get_busno		= &get_busno_pcie,
	.io_map_base		= BOGUS_IO_MAP_BASE,
};

struct brcm_pci_bus {
	struct pci_controller	*controller;
	char			*name;
	unsigned int		hw_busnum;
	unsigned int		busnum_shift;
	unsigned int		slot_shift;
	unsigned int		func_shift;
	int			memory_hole;
	unsigned long		idx_reg;
	unsigned long		data_reg;
	struct clk		*clk;
};

static struct brcm_pci_bus brcm_buses[] = {
	[BRCM_BUSNO_PCI23] = {
		&brcmstb_pci_controller,  "PCI2.3",  0, 0,  11, 8,  0 },
#if defined(CONFIG_BRCM_HAS_SATA2)
	[BRCM_BUSNO_SATA] = {
		&brcmstb_sata_controller, "SATA",    0, 0,  15, 12, 1 },
#endif
	[BRCM_BUSNO_PCIE] = {
		&brcmstb_pcie_controller, "PCIe",    1, 20, 15, 12, 0 },
};


/***********************************************************************
 * PCI/PCIX/PCIe Bridge setup
 ***********************************************************************/

#define SATA_MEM_ENABLE		1
#define SATA_BUS_MASTER_ENABLE	2
#define SATA_PERR_ENABLE	0x10
#define SATA_SERR_ENABLE	0x20

#define PCI_BUS_MASTER  BCHP_PCI_CFG_STATUS_COMMAND_BUS_MASTER_MASK
#define PCI_IO_ENABLE   BCHP_PCI_CFG_STATUS_COMMAND_MEMORY_SPACE_MASK
#define PCI_MEM_ENABLE  BCHP_PCI_CFG_STATUS_COMMAND_IO_SPACE_MASK

#if defined(CONFIG_CPU_BIG_ENDIAN)
#define	DATA_ENDIAN		2	/* PCI->DDR inbound accesses */
#define MMIO_ENDIAN		2	/* MIPS->PCI outbound accesses */
#else
#define	DATA_ENDIAN		0
#define MMIO_ENDIAN		0
#endif


static inline void brcm_setup_sata_bridge(void)
{
#if defined(CONFIG_BRCM_HAS_SATA2)

	/* Internal PCI-X SATA bridge setup for 7400, 7405, 7335 */

	BDEV_WR(BCHP_PCIX_BRIDGE_PCIX_CTRL,
		(SATA_MEM_ENABLE|SATA_BUS_MASTER_ENABLE|
		 SATA_PERR_ENABLE|SATA_SERR_ENABLE));

	/* PCI slave window (SATA access to MIPS memory) */
	BDEV_WR(BCHP_PCIX_BRIDGE_PCIX_SLV_MEM_WIN_BASE, 1);
	BDEV_WR(BCHP_PCIX_BRIDGE_PCIX_SLV_MEM_WIN_MODE, DATA_ENDIAN);

	/* PCI master window (MIPS access to SATA BARs) */
	BDEV_WR(BCHP_PCIX_BRIDGE_CPU_TO_SATA_MEM_WIN_BASE,
		SATA_MEM_START | MMIO_ENDIAN);
	BDEV_WR(BCHP_PCIX_BRIDGE_CPU_TO_SATA_IO_WIN_BASE, MMIO_ENDIAN);

	/* Set up MMIO BAR in PCI configuration space */
	BDEV_WR_RB(BCHP_PCIX_BRIDGE_SATA_CFG_INDEX, PCI_BASE_ADDRESS_5);
	BDEV_WR_RB(BCHP_PCIX_BRIDGE_SATA_CFG_DATA, SATA_MEM_START);

	BDEV_WR_RB(BCHP_PCIX_BRIDGE_SATA_CFG_INDEX, PCI_COMMAND);
	BDEV_WR_RB(BCHP_PCIX_BRIDGE_SATA_CFG_DATA,
		PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

	/* PCI latency settings */
	BDEV_WR_RB(BCHP_PCIX_BRIDGE_SATA_CFG_INDEX, PCI_INTERRUPT_LINE);
	BDEV_WR_RB(BCHP_PCIX_BRIDGE_SATA_CFG_DATA, 0x000f0100);

	BDEV_WR_RB(BCHP_PCIX_BRIDGE_SATA_CFG_INDEX, PCI_CACHE_LINE_SIZE);
	BDEV_WR_RB(BCHP_PCIX_BRIDGE_SATA_CFG_DATA, 0x0080ff00);

	/* Device ID in the emulated PCI configuration registers */
	sata_pci_reg[PCI_CLASS_REVISION] = 0x01010f00;
	sata_pci_reg[PCI_VENDOR_ID] = 0x860214e4;

#endif
}

static inline void brcm_setup_pci_bridge(void)
{
#if defined(CONFIG_BRCM_HAS_PCI23)

	unsigned long win_size_mb;

	/* External PCI bridge setup (most chips) */

	BDEV_SET(BCHP_PCI_CFG_STATUS_COMMAND,
		PCI_BUS_MASTER|PCI_IO_ENABLE|PCI_MEM_ENABLE);

	BDEV_WR(BCHP_PCI_CFG_CPU_2_PCI_MEM_WIN0, PCI_MEM_START + 0x00000000 +
		MMIO_ENDIAN);
	BDEV_WR(BCHP_PCI_CFG_CPU_2_PCI_MEM_WIN1, PCI_MEM_START + 0x08000000 +
		MMIO_ENDIAN);
	BDEV_WR(BCHP_PCI_CFG_CPU_2_PCI_MEM_WIN2, PCI_MEM_START + 0x10000000 +
		MMIO_ENDIAN);
	BDEV_WR(BCHP_PCI_CFG_CPU_2_PCI_MEM_WIN3, PCI_MEM_START + 0x18000000 +
		MMIO_ENDIAN);

	BDEV_WR(BCHP_PCI_CFG_CPU_2_PCI_IO_WIN0, 0x00000000 | MMIO_ENDIAN);
	BDEV_WR(BCHP_PCI_CFG_CPU_2_PCI_IO_WIN1, 0x00200000 | MMIO_ENDIAN);
	BDEV_WR(BCHP_PCI_CFG_CPU_2_PCI_IO_WIN2, 0x00400000 | MMIO_ENDIAN);

#ifdef BCHP_HIF_TOP_CTRL_PCI_MWIN_CTRL
	/* force PCI->SDRAM window to 1GB on reduced-strap chips */
	BDEV_WR_RB(BCHP_HIF_TOP_CTRL_PCI_MWIN_CTRL, 0x1);
#endif

	BDEV_WR(BCHP_PCI_CFG_MEMORY_BASE_W0, 0xfffffff0);
	win_size_mb = (~(BDEV_RD(BCHP_PCI_CFG_MEMORY_BASE_W0) & ~0xfUL) +
			 1UL) >> 20;
	printk(KERN_INFO "PCI2.3->SDRAM window: %lu MB\n", win_size_mb);
	if (win_size_mb < brcm_dram0_size_mb)
		printk(KERN_WARNING
			"WARNING: PCI2.3 window size is smaller than "
			"system memory\n");
	BDEV_WR(BCHP_PCI_CFG_MEMORY_BASE_W0, 0x00000000);

	/* not used - move them out of the way */
	BDEV_WR(BCHP_PCI_CFG_MEMORY_BASE_W1, 0x80000000);
	BDEV_WR(BCHP_PCI_CFG_MEMORY_BASE_W2, 0x80000000);
	BDEV_WR(BCHP_PCI_CFG_GISB_BASE_W, 0x80000000);

	/* set endianness for W0 */
	BDEV_UNSET(BCHP_PCI_CFG_PCI_SDRAM_ENDIAN_CTRL,
		BCHP_PCI_CFG_PCI_SDRAM_ENDIAN_CTRL_ENDIAN_MODE_MWIN0_MASK);
	BDEV_SET(BCHP_PCI_CFG_PCI_SDRAM_ENDIAN_CTRL, DATA_ENDIAN);
#endif
}

#if defined(CONFIG_BRCM_HAS_PCIE)
static struct wktmr_time pcie_reset_started;

#define PCIE_LINK_UP() \
	(((BDEV_RD(BCHP_PCIE_MISC_PCIE_STATUS) & 0x30) == 0x30) ? 1 : 0)

#else

#define PCIE_LINK_UP() 0

#endif

void brcm_early_pcie_setup(void)
{
#if defined(CONFIG_BRCM_HAS_PCIE)
	/*
	 * Called from bchip_early_setup() in order to start PCIe link
	 * negotiation immediately at kernel boot time.  The RC is supposed
	 * to give the endpoint device 100ms to settle down before
	 * attempting configuration accesses.  So we let the link negotiation
	 * happen in the background instead of busy-waiting.
	 */

	struct wktmr_time tmp;

	/* reset the bridge and the endpoint device */
	SET_BRIDGE_RESET(1);
	SET_PERST(1);

	/* delay 100us */
	wktmr_read(&tmp);
	while (wktmr_elapsed(&tmp) < (100 * WKTMR_1US))
		;

	/* take the bridge out of reset */
	SET_BRIDGE_RESET(0);

	/* enable SCB_MAX_BURST_SIZE | CSR_READ_UR_MODE | SCB_ACCESS_EN */
	BDEV_WR(BCHP_PCIE_MISC_MISC_CTRL, 0x00103000);

	/* set up MIPS->PCIE memory windows (4x 128MB) */
	PCIE_OUTBOUND_WIN(0, PCIE_MEM_START + 0x00000000, 0x08000000);
	PCIE_OUTBOUND_WIN(1, PCIE_MEM_START + 0x08000000, 0x08000000);
	PCIE_OUTBOUND_WIN(2, PCIE_MEM_START + 0x10000000, 0x08000000);
	PCIE_OUTBOUND_WIN(3, PCIE_MEM_START + 0x18000000, 0x08000000);

#if defined(CONFIG_BRCM_HAS_2GB_MEMC0)
	/* set up 4GB PCIE->SCB memory window on BAR2 */
	BDEV_WR(BCHP_PCIE_MISC_RC_BAR2_CONFIG_LO, 0x00000011);
	BDEV_WR(BCHP_PCIE_MISC_RC_BAR2_CONFIG_HI, 0x00000000);
	BDEV_WR_F(PCIE_MISC_MISC_CTRL, SCB0_SIZE, 0x10);
	BDEV_WR_F(PCIE_MISC_MISC_CTRL, SCB1_SIZE, 0x0f);
#else
	/* set up 1GB PCIE->SCB memory window on BAR2 */
	BDEV_WR(BCHP_PCIE_MISC_RC_BAR2_CONFIG_LO, 0x0000000f);
	BDEV_WR(BCHP_PCIE_MISC_RC_BAR2_CONFIG_HI, 0x00000000);
#endif

	/* disable PCIE->GISB window */
	BDEV_WR(BCHP_PCIE_MISC_RC_BAR1_CONFIG_LO, 0x00000000);
	/* disable the other PCIE->SCB memory window */
	BDEV_WR(BCHP_PCIE_MISC_RC_BAR3_CONFIG_LO, 0x00000000);

	/* disable MSI (for now...) */
	BDEV_WR(BCHP_PCIE_MISC_MSI_BAR_CONFIG_LO, 0);

	/* set up L2 interrupt masks */
	BDEV_WR_RB(BCHP_PCIE_INTR2_CPU_CLEAR, 0);
	BDEV_WR_RB(BCHP_PCIE_INTR2_CPU_MASK_CLEAR, 0);
	BDEV_WR_RB(BCHP_PCIE_INTR2_CPU_MASK_SET, 0xffffffff);

	/* take the EP device out of reset */
	SET_PERST(0);

	/* record the current time */
	wktmr_read(&pcie_reset_started);

#endif
}

void brcm_setup_pcie_bridge(void)
{
#if defined(CONFIG_BRCM_HAS_PCIE)

	/* give the RC/EP time to wake up, before trying to configure RC */
	while (wktmr_elapsed(&pcie_reset_started) < (100 * WKTMR_1MS))
		;

	if (!PCIE_LINK_UP()) {
		struct clk *clk;

		brcm_pcie_enabled = 0;
		clk = brcm_buses[BRCM_BUSNO_PCIE].clk;
		if (clk) {
			clk_disable(clk);
			clk_put(clk);
		}
		printk(KERN_INFO "PCI: PCIe link down\n");
		return;
	}
	printk(KERN_INFO "PCI: PCIe link up\n");

	/* enable MEM_SPACE and BUS_MASTER for RC */
	BDEV_WR(BCHP_PCIE_RC_CFG_TYPE1_STATUS_COMMAND, 0x6);

	/* set base/limit for outbound transactions */
#if defined(CONFIG_BRCM_HAS_2GB_MEMC0)
	BDEV_WR(BCHP_PCIE_RC_CFG_TYPE1_RC_MEM_BASE_LIMIT, 0xeff0d000);
#else
	BDEV_WR(BCHP_PCIE_RC_CFG_TYPE1_RC_MEM_BASE_LIMIT, 0xbff0a000);
#endif

	/* disable the prefetch range */
	BDEV_WR(BCHP_PCIE_RC_CFG_TYPE1_RC_PREF_BASE_LIMIT, 0x0000fff0);

	/* set pri/sec bus numbers */
	BDEV_WR(BCHP_PCIE_RC_CFG_TYPE1_PRI_SEC_BUS_NO, 0x00010100);

	/* enable configuration request retry (see pci_scan_device()) */
	BDEV_WR_F(PCIE_RC_CFG_PCIE_ROOT_CAP_CONTROL, RC_CRS_EN, 1);

	/* PCIE->SCB endian mode for BAR2 */
	BDEV_WR_F_RB(PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1, ENDIAN_MODE_BAR2,
		DATA_ENDIAN);
#endif
}


#if defined(CONFIG_PM) && defined(CONFIG_BRCM_HAS_PCIE)
/*
 * sysdev device to handle PCIe bus suspend and resume
 */
static inline void pcie_enable(int enable)
{
	struct clk *clk;

	if (!brcm_pcie_enabled)
		return;

	clk = brcm_buses[BRCM_BUSNO_PCIE].clk;
	if (clk)
		enable ? clk_enable(clk) : clk_disable(clk);
}

static int pcie_suspend(struct sys_device *dev, pm_message_t state)
{
	pcie_enable(0);
	return 0;
}

static int pcie_resume(struct sys_device *dev)
{
	pcie_enable(1);
	return 0;
}

static struct sysdev_class pcie_sysclass = {
	.name		= "PCIe",
	.suspend	= pcie_suspend,
	.resume		= pcie_resume,
};

static struct sys_device pcie_ctlr = {
	.id		= 0,
	.cls		= &pcie_sysclass,
};
#endif

/***********************************************************************
 * PCI controller registration
 ***********************************************************************/

static int __init brcmstb_pci_init(void)
{
	unsigned long __maybe_unused reg_base;

#if defined(CONFIG_BRCM_HAS_PCI23)
	if (brcm_pci_enabled) {
		request_mem_region(PCI_IO_START, PCI_IO_ACTUAL_SIZE,
			"External PCI2.3 IO");
		request_mem_region(PCI_IO_REG_START, PCI_IO_REG_SIZE,
			"External PCI2.3 registers");

		reg_base = (unsigned long)
			ioremap(PCI_IO_REG_START, PCI_IO_REG_SIZE);
		brcm_buses[BRCM_BUSNO_PCI23].idx_reg = reg_base + PCI_CFG_INDEX;
		brcm_buses[BRCM_BUSNO_PCI23].data_reg = reg_base + PCI_CFG_DATA;

		brcm_setup_pci_bridge();

		brcmstb_pci_controller.io_map_base = (unsigned long)
			ioremap(PCI_IO_START, PCI_IO_SIZE);

		/*
		 * INVALID port ranges include:
		 *   0x0000 .. (IO_ADDR_PCI23 - 1)
		 *   SATA/PCIe IO regions
		 * These are (intentionally) located in the ioremap() guard
		 * pages so that they cause an exception on access.
		 */
		set_io_port_base(brcmstb_pci_controller.io_map_base -
			IO_ADDR_PCI23);

		register_pci_controller(&brcmstb_pci_controller);
	}
#endif
#if defined(CONFIG_BRCM_HAS_SATA2)
	if (brcm_sata_enabled) {
		brcm_setup_sata_bridge();
		register_pci_controller(&brcmstb_sata_controller);
	}
#endif
#ifdef CONFIG_BRCM_HAS_PCIE
	if (brcm_pcie_enabled) {
		brcm_buses[BRCM_BUSNO_PCIE].clk = clk_get(NULL, "pcie");
		brcm_setup_pcie_bridge();

		request_mem_region(PCIE_IO_REG_START, PCIE_IO_REG_SIZE,
			"External PCIe registers");
		reg_base = (unsigned long)ioremap(PCIE_IO_REG_START,
			PCIE_IO_REG_SIZE);

		brcm_buses[BRCM_BUSNO_PCIE].idx_reg = reg_base + PCIE_CFG_INDEX;
		brcm_buses[BRCM_BUSNO_PCIE].data_reg = reg_base + PCIE_CFG_DATA;

		register_pci_controller(&brcmstb_pcie_controller);

#if defined(CONFIG_PM)
		if (sysdev_class_register(&pcie_sysclass) == 0)
			sysdev_register(&pcie_ctlr);
#endif
	}
#endif
	return 0;
}

arch_initcall(brcmstb_pci_init);

/***********************************************************************
 * Read/write PCI configuration registers
 ***********************************************************************/

#define CFG_INDEX(bus, devfn, reg) \
	(((PCI_SLOT(devfn) & 0x1f) << (brcm_buses[bus->number].slot_shift)) | \
	 ((PCI_FUNC(devfn) & 0x07) << (brcm_buses[bus->number].func_shift)) | \
	 (brcm_buses[bus->number].hw_busnum << \
	  brcm_buses[bus->number].busnum_shift) | \
	 (reg))

static int devfn_ok(struct pci_bus *bus, unsigned int devfn)
{
	/* SATA is the only device on the bus, with devfn == 0 */
	if (bus->number == BRCM_BUSNO_SATA && devfn != 0)
		return 0;

	/* PCIe: check for link down or invalid slot number */
	if (bus->number == BRCM_BUSNO_PCIE &&
	    (!PCIE_LINK_UP() || PCI_SLOT(devfn) != 0))
		return 0;

	return 1;	/* OK */
}

static int brcm_pci_write_config(struct pci_bus *bus, unsigned int devfn,
	int where, int size, u32 data)
{
	u32 val = 0, mask, shift;

	if (!devfn_ok(bus, devfn))
		return PCIBIOS_FUNC_NOT_SUPPORTED;

	BUG_ON(((where & 3) + size) > 4);

	if (size < 4) {
		/* partial word - read, modify, write */
		DEV_WR_RB(brcm_buses[bus->number].idx_reg,
			CFG_INDEX(bus, devfn, where & ~3));
		val = DEV_RD(brcm_buses[bus->number].data_reg);
	}

	shift = (where & 3) << 3;
	mask = (0xffffffff >> ((4 - size) << 3)) << shift;

	DEV_WR_RB(brcm_buses[bus->number].idx_reg,
		CFG_INDEX(bus, devfn, where & ~3));
	val = (val & ~mask) | ((data << shift) & mask);
	DEV_WR_RB(brcm_buses[bus->number].data_reg, val);

	return PCIBIOS_SUCCESSFUL;
}

static int brcm_pci_read_config(struct pci_bus *bus, unsigned int devfn,
	int where, int size, u32 *data)
{
	u32 val, mask, shift;

	if (!devfn_ok(bus, devfn))
		return PCIBIOS_FUNC_NOT_SUPPORTED;

	BUG_ON(((where & 3) + size) > 4);

	DEV_WR_RB(brcm_buses[bus->number].idx_reg,
		CFG_INDEX(bus, devfn, where & ~3));
	val = DEV_RD(brcm_buses[bus->number].data_reg);

	shift = (where & 3) << 3;
	mask = (0xffffffff >> ((4 - size) << 3)) << shift;

	*data = (val & mask) >> shift;
	return PCIBIOS_SUCCESSFUL;
}

/*
 * SATA2 has real PCI configuration registers, but they are set up
 * once at boot time then subsequently emulated because they interfere with
 * power management.  i.e. SATA PCI registers are inaccessible when the
 * SATA core is clock gated.
 *
 * BAR5 (MMIO registers) will be 4KB @ SATA_MEM_START.  This is the
 * only active BAR, on both AHCI and Serverworks "K2" cores.
 */

#if defined(CONFIG_BRCM_HAS_SATA2)
static int brcm_sata_write_config(struct pci_bus *bus, unsigned int devfn,
	int where, int size, u32 data)
{
	if (!devfn_ok(bus, devfn))
		return PCIBIOS_FUNC_NOT_SUPPORTED;
	if (where == PCI_BASE_ADDRESS_5)
		sata_pci_reg[where] = data & ~0x0fff;
	else if (where <= PCI_INTERRUPT_PIN)
		sata_pci_reg[where] = data;
	return PCIBIOS_SUCCESSFUL;
}

static int brcm_sata_read_config(struct pci_bus *bus, unsigned int devfn,
	int where, int size, u32 *data)
{
	if (!devfn_ok(bus, devfn))
		return PCIBIOS_FUNC_NOT_SUPPORTED;
	if (where <= PCI_INTERRUPT_PIN)
		*data = sata_pci_reg[where];
	else
		*data = 0;
	return PCIBIOS_SUCCESSFUL;
}
#endif

/***********************************************************************
 * PCI slot to IRQ mappings (aka "fixup")
 ***********************************************************************/

int __devinit pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
#if defined(CONFIG_BRCM_HAS_SATA2)
	if (dev->bus->number == BRCM_BUSNO_SATA)
		return BRCM_IRQ_SATA;
#endif
#if defined(CONFIG_BRCM_HAS_PCIE)
	if (dev->bus->number == BRCM_BUSNO_PCIE) {
		const int pcie_irq[] = {
			BRCM_IRQ_PCIE_INTA, BRCM_IRQ_PCIE_INTB,
			BRCM_IRQ_PCIE_INTC, BRCM_IRQ_PCIE_INTD,
		};
		if ((pin - 1) > 3)
			return 0;
		return pcie_irq[pin - 1];
	}
#endif
#if defined(CONFIG_BRCM_HAS_PCI23)
	if ((slot >= BRCM_PCI_SLOTS) || ((pin - 1) > 3))
		return 0;
	return brcm_docsis_platform ?
		irq_tab_brcmstb_docsis[slot][pin - 1] :
		irq_tab_brcmstb[slot][pin - 1];
#else
	return 0;
#endif
}

/* Do platform specific device initialization at pci_enable_device() time */
int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}


/***********************************************************************
 * Per-device initialization
 ***********************************************************************/

static void __devinit brcm_pcibios_fixup(struct pci_dev *dev)
{
	int slot = PCI_SLOT(dev->devfn);

	printk(KERN_INFO
		"PCI: found device %04x:%04x on %s bus, slot %d (irq %d)\n",
		dev->vendor, dev->device, brcm_buses[dev->bus->number].name,
		slot, pcibios_map_irq(dev, slot, 1));

	/* zero out the BARs and let Linux assign an address */
	pci_write_config_dword(dev, PCI_COMMAND, 0);
	pci_write_config_dword(dev, PCI_BASE_ADDRESS_0, 0);
	pci_write_config_dword(dev, PCI_BASE_ADDRESS_1, 0);
	pci_write_config_dword(dev, PCI_BASE_ADDRESS_2, 0);
	pci_write_config_dword(dev, PCI_BASE_ADDRESS_3, 0);
	pci_write_config_dword(dev, PCI_BASE_ADDRESS_4, 0);
	pci_write_config_dword(dev, PCI_BASE_ADDRESS_5, 0);
	pci_write_config_dword(dev, PCI_INTERRUPT_LINE, 0);
}

DECLARE_PCI_FIXUP_EARLY(PCI_ANY_ID, PCI_ANY_ID, brcm_pcibios_fixup);

/***********************************************************************
 * DMA address remapping
 ***********************************************************************/

/*
 * PCI2.3 and PCIe inbound BARs collapse the "holes" in the chip's SCB
 * address space.  SATA PCI/PCI-X does not.  Therefore the PCI addresses
 * need to be adjusted as they will not match the SCB addresses (MIPS
 * physical addresses).
 *
 * The address maps can be found in <asm/mach-brcmstb/spaces.h> .
 */

static int dev_collapses_memory_hole(struct device *dev)
{
#if defined(CONFIG_BRCM_UPPER_MEMORY) || defined(CONFIG_HIGHMEM)

	struct pci_dev *pdev;

	if (unlikely(dev == NULL) ||
	    likely(dev->bus != &pci_bus_type))
		return 0;

	pdev = to_pci_dev(dev);
	if (unlikely(pdev == NULL) ||
	    brcm_buses[pdev->bus->number].memory_hole)
		return 0;

	return 1;
#else
	return 0;
#endif
}

static dma_addr_t brcm_phys_to_pci(struct device *dev, unsigned long phys)
{
	if (!dev_collapses_memory_hole(dev))
		return phys;
	if (phys >= MEMC1_START)
		return phys - MEMC1_PCI_OFFSET;
	if (phys >= (BRCM_PCI_HOLE_START + BRCM_PCI_HOLE_SIZE))
		return phys - BRCM_PCI_HOLE_SIZE;
	return phys;
}

dma_addr_t plat_map_dma_mem(struct device *dev, void *addr, size_t size)
{
	return brcm_phys_to_pci(dev, virt_to_phys(addr));
}

dma_addr_t plat_map_dma_mem_page(struct device *dev, struct page *page)
{
	return brcm_phys_to_pci(dev, page_to_phys(page));
}

unsigned long plat_dma_addr_to_phys(struct device *dev, dma_addr_t dma_addr)
{
	if (!dev_collapses_memory_hole(dev))
		return dma_addr;
	if (dma_addr >= (MEMC1_START - MEMC1_PCI_OFFSET))
		return dma_addr + MEMC1_PCI_OFFSET;
	if (dma_addr >= BRCM_PCI_HOLE_START)
		return dma_addr + BRCM_PCI_HOLE_SIZE;
	return dma_addr;
}
