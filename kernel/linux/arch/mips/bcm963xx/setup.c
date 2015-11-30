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
/*
 * Generic setup routines for Broadcom 963xx MIPS boards
 */

//#include <linux/config.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/console.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/bootmem.h>

#include <asm/addrspace.h>
#include <asm/bcache.h>
#include <asm/irq.h>
#include <asm/time.h>
#include <asm/reboot.h>
//#include <asm/gdb-stub.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>

extern unsigned long getMemorySize(void);
extern irqreturn_t brcm_timer_interrupt(int irq, void *dev_id);

#include <bcm_map_part.h>
#include <bcm_intr.h>
#include <board.h>
#include <boardparms.h>

#if defined(CONFIG_PCI)
#include <linux/pci.h>
#include <bcmpci.h>
#endif

#if defined(CONFIG_BCM93380)
#if defined(CONFIG_BCM_LOT1) && defined(CONFIG_BCM_UPPER_MEM)
#define CM_SDRAM_SIZE 0x04000000 /* 64MB */
#define CM_SDRAM_BASE 0x04000000 /* 64MB */
#else
#define CM_SDRAM_SIZE 0x02000000 /* 32MB */
#endif
#if defined(CONFIG_BCM_RAMDISK)
#define CM_RESERVED_SDRAM_SIZE 0x00600000 /* 6MB */
#else
#if defined(CONFIG_BCM_T0_IDLE)
#define CM_RESERVED_SDRAM_SIZE 0x00200000 /* 2MB */
#else
#define CM_RESERVED_SDRAM_SIZE 0x00000000 /* 0MB */
#endif
#endif
#define CM_RESERVED_SDRAM_BASE (CM_SDRAM_SIZE - CM_RESERVED_SDRAM_SIZE)
#define ADSL_SDRAM_IMAGE_SIZE CM_RESERVED_SDRAM_SIZE
#elif defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
#define CM_SDRAM_SIZE 0x04000000 /* 64MB */
#define CM_SDRAM_BASE 0x04000000 /* 64MB */
#if defined(CONFIG_BCM_RAMDISK)
#define CM_RESERVED_SDRAM_SIZE 0x01000000 /* 16MB */
#else
#define CM_RESERVED_SDRAM_SIZE 0x00200000 /* 2MB */
#endif
#define ADSL_SDRAM_IMAGE_SIZE CM_RESERVED_SDRAM_SIZE
#else
#include "softdsl/AdslCoreDefs.h"
#endif

#if defined(CONFIG_BCM_ENDPOINT_MODULE)
#include <dsp_mod_size.h>
#endif

#if (defined(CONFIG_BCM96362) || defined(CHIP_6362)) && defined(CONFIG_BCM_FAP_MODULE)
#include <fap_mod_size.h>
#endif

#if defined(CONFIG_BCM96816)
#include "bcmSpiRes.h"
#endif

#if 1

/***************************************************************************
 * C++ New and delete operator functions
 ***************************************************************************/

/* void *operator new(unsigned int sz) */
void *_Znwj(unsigned int sz)
{
    return( kmalloc(sz, GFP_KERNEL) );
}

/* void *operator new[](unsigned int sz)*/
void *_Znaj(unsigned int sz)
{
    return( kmalloc(sz, GFP_KERNEL) );
}

/* placement new operator */
/* void *operator new (unsigned int size, void *ptr) */
void *ZnwjPv(unsigned int size, void *ptr)
{
    return ptr;
}

/* void operator delete(void *m) */
void _ZdlPv(void *m)
{
    kfree(m);
}

/* void operator delete[](void *m) */
void _ZdaPv(void *m)
{
    kfree(m);
}

EXPORT_SYMBOL(_Znwj);
EXPORT_SYMBOL(_Znaj);
EXPORT_SYMBOL(ZnwjPv);
EXPORT_SYMBOL(_ZdlPv);
EXPORT_SYMBOL(_ZdaPv);

#endif


void __init plat_mem_setup(void)
{
#if defined(CONFIG_BCM96816)
    add_memory_region(0, (getMemorySize()), BOOT_MEM_RAM);
#elif (defined(CONFIG_BCM93380) && defined(CONFIG_BCM_LOT1) && defined(CONFIG_BCM_UPPER_MEM)) || defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
    add_memory_region(CM_SDRAM_BASE, (getMemorySize() - ADSL_SDRAM_IMAGE_SIZE), BOOT_MEM_RAM);
#else
    add_memory_region(0, (getMemorySize() - ADSL_SDRAM_IMAGE_SIZE), BOOT_MEM_RAM);
#endif
}


extern UINT32 __init calculateCpuSpeed(void);
#if defined(CONFIG_BCM_PWRMNGT) || defined(CONFIG_BCM_PWRMNGT_MODULE)
extern void BcmPwrMngtInitC0Speed (void);
#endif

void __init plat_time_init(void)
{
    /* JU: TBD: there was some special SMP handling added here in original kernel */
    mips_hpt_frequency = calculateCpuSpeed() / 2;
#if defined(CONFIG_BCM_PWRMNGT) || defined(CONFIG_BCM_PWRMNGT_MODULE)
    BcmPwrMngtInitC0Speed();
#else
    // Enable cp0 counter/compare interrupt only when not using power management
    write_c0_status(IE_IRQ5 | read_c0_status());
#endif
}


static void brcm_machine_restart(char *command)
{
    kerSysMipsSoftReset();
}

static void brcm_machine_halt(void)
{
    printk("System halted\n");
    while (1);
}

#if defined(CONFIG_PCI) && (defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816))
static void mpi_SetLocalPciConfigReg(uint32 reg, uint32 value)
{
    /* write index then value */
    MPI->pcicfgcntrl = PCI_CFG_REG_WRITE_EN + reg;;
    MPI->pcicfgdata = value;
}

static uint32 mpi_GetLocalPciConfigReg(uint32 reg)
{
    /* write index then get value */
    MPI->pcicfgcntrl = PCI_CFG_REG_WRITE_EN + reg;;
    return MPI->pcicfgdata;
}

#if !defined(CONFIG_BCM96816)
/*
 * mpi_ResetPcCard: Set/Reset the PcCard
 */
static void mpi_ResetPcCard(int cardtype, BOOL bReset)
{
    if (cardtype == MPI_CARDTYPE_NONE) {
        return;
    }

    if (cardtype == MPI_CARDTYPE_CARDBUS) {
        bReset = ! bReset;
    }

    if (bReset) {
        MPI->pcmcia_cntl1 = (MPI->pcmcia_cntl1 & ~PCCARD_CARD_RESET);
    } else {
        MPI->pcmcia_cntl1 = (MPI->pcmcia_cntl1 | PCCARD_CARD_RESET);
    }
}

/*
 * mpi_ConfigCs: Configure an MPI/EBI chip select
 */
static void mpi_ConfigCs(uint32 cs, uint32 base, uint32 size, uint32 flags)
{
    MPI->cs[cs].base = ((base & 0x1FFFFFFF) | size);
    MPI->cs[cs].config = flags;
}

/*
 * mpi_InitPcmciaSpace
 */
static void mpi_InitPcmciaSpace(void)
{
    // ChipSelect 4 controls PCMCIA Memory accesses
    mpi_ConfigCs(PCMCIA_COMMON_BASE, pcmciaMem, EBI_SIZE_1M, (EBI_WORD_WIDE|EBI_ENABLE));
    // ChipSelect 5 controls PCMCIA Attribute accesses
    mpi_ConfigCs(PCMCIA_ATTRIBUTE_BASE, pcmciaAttr, EBI_SIZE_1M, (EBI_WORD_WIDE|EBI_ENABLE));
    // ChipSelect 6 controls PCMCIA I/O accesses
    mpi_ConfigCs(PCMCIA_IO_BASE, pcmciaIo, EBI_SIZE_64K, (EBI_WORD_WIDE|EBI_ENABLE));

    MPI->pcmcia_cntl2 = ((PCMCIA_ATTR_ACTIVE << RW_ACTIVE_CNT_BIT) |
                         (PCMCIA_ATTR_INACTIVE << INACTIVE_CNT_BIT) |
                         (PCMCIA_ATTR_CE_SETUP << CE_SETUP_CNT_BIT) |
                         (PCMCIA_ATTR_CE_HOLD << CE_HOLD_CNT_BIT));

    MPI->pcmcia_cntl2 |= (PCMCIA_HALFWORD_EN | PCMCIA_BYTESWAP_DIS);
}

/*
 * cardtype_vcc_detect: PC Card's card detect and voltage sense connection
 *
 *   CD1#/      CD2#/     VS1#/     VS2#/    Card       Initial Vcc
 *  CCD1#      CCD2#     CVS1      CVS2      Type
 *
 *   GND        GND       open      open     16-bit     5 vdc
 *
 *   GND        GND       GND       open     16-bit     3.3 vdc
 *
 *   GND        GND       open      GND      16-bit     x.x vdc
 *
 *   GND        GND       GND       GND      16-bit     3.3 & x.x vdc
 *
 *====================================================================
 *
 *   CVS1       GND       CCD1#     open     CardBus    3.3 vdc
 *
 *   GND        CVS2      open      CCD2#    CardBus    x.x vdc
 *
 *   GND        CVS1      CCD2#     open     CardBus    y.y vdc
 *
 *   GND        CVS2      GND       CCD2#    CardBus    3.3 & x.x vdc
 *
 *   CVS2       GND       open      CCD1#    CardBus    x.x & y.y vdc
 *
 *   GND        CVS1      CCD2#     open     CardBus    3.3, x.x & y.y vdc
 *
 */
static int cardtype_vcc_detect(void)
{
    uint32 data32;
    int cardtype;

    cardtype = MPI_CARDTYPE_NONE;
    MPI->pcmcia_cntl1 = (CARDBUS_ENABLE|PCMCIA_GPIO_ENABLE); // Turn on the output enables and drive
                                        // the CVS pins to 0.
    data32 = MPI->pcmcia_cntl1;
    switch (data32 & (CD2_IN|CD1_IN))  // Test CD1# and CD2#, see if card is plugged in.
    {
    case (CD2_IN|CD1_IN):  // No Card is in the slot.
        printk("MPI: No Card is in the PCMCIA slot\n");
        break;

    case CD2_IN:  // Partial insertion, No CD2#.
        printk("MPI: Card in the PCMCIA slot partial insertion, no CD2 signal\n");
        break;

    case CD1_IN:  // Partial insertion, No CD1#.
        printk("MPI: Card in the PCMCIA slot partial insertion, no CD1 signal\n");
        break;

    case 0x00000000:
        MPI->pcmcia_cntl1 = (CARDBUS_ENABLE|PCMCIA_GPIO_ENABLE|VS2_OEN|VS1_OEN);
                                        // Turn off the CVS output enables and
                                        // float the CVS pins.
        mdelay(1);
        data32 = MPI->pcmcia_cntl1;
        // Read the Register.
        switch (data32 & (VS2_IN|VS1_IN))  // See what is on the CVS pins.
        {
        case 0x00000000: // CVS1 and CVS2 are tied to ground, only 1 option.
            printk("MPI: Detected 3.3 & x.x 16-bit PCMCIA card\n");
            cardtype = MPI_CARDTYPE_PCMCIA;
            break;

        case VS1_IN: // CVS1 is open or tied to CCD1/CCD2 and CVS2 is tied to ground.
                         // 2 valid voltage options.
        switch (data32 & (CD2_IN|CD1_IN))  // Test the values of CCD1 and CCD2.
        {
            case (CD2_IN|CD1_IN):  // CCD1 and CCD2 are tied to 1 of the CVS pins.
                              // This is not a valid combination.
                printk("MPI: Unknown card plugged into slot\n");
                break;

            case CD2_IN:  // CCD2 is tied to either CVS1 or CVS2.
                MPI->pcmcia_cntl1 = (CARDBUS_ENABLE|PCMCIA_GPIO_ENABLE|VS2_OEN); // Drive CVS1 to a 0.
                mdelay(1);
                data32 = MPI->pcmcia_cntl1;
                if (data32 & CD2_IN) { // CCD2 is tied to CVS2, not valid.
                    printk("MPI: Unknown card plugged into slot\n");
                } else {                   // CCD2 is tied to CVS1.
                    printk("MPI: Detected 3.3, x.x and y.y Cardbus card\n");
                    cardtype = MPI_CARDTYPE_CARDBUS;
                }
                break;

            case CD1_IN: // CCD1 is tied to either CVS1 or CVS2.
                             // This is not a valid combination.
                printk("MPI: Unknown card plugged into slot\n");
                break;

            case 0x00000000:  // CCD1 and CCD2 are tied to ground.
                printk("MPI: Detected x.x vdc 16-bit PCMCIA card\n");
                cardtype = MPI_CARDTYPE_PCMCIA;
                break;
            }
            break;

        case VS2_IN: // CVS2 is open or tied to CCD1/CCD2 and CVS1 is tied to ground.
                         // 2 valid voltage options.
            switch (data32 & (CD2_IN|CD1_IN))  // Test the values of CCD1 and CCD2.
            {
            case (CD2_IN|CD1_IN):  // CCD1 and CCD2 are tied to 1 of the CVS pins.
                              // This is not a valid combination.
                printk("MPI: Unknown card plugged into slot\n");
                break;

            case CD2_IN:  // CCD2 is tied to either CVS1 or CVS2.
                MPI->pcmcia_cntl1 = (CARDBUS_ENABLE|PCMCIA_GPIO_ENABLE|VS1_OEN);// Drive CVS2 to a 0.
                mdelay(1);
                data32 = MPI->pcmcia_cntl1;
                if (data32 & CD2_IN) { // CCD2 is tied to CVS1, not valid.
                    printk("MPI: Unknown card plugged into slot\n");
                } else {// CCD2 is tied to CVS2.
                    printk("MPI: Detected 3.3 and x.x Cardbus card\n");
                    cardtype = MPI_CARDTYPE_CARDBUS;
                }
                break;

            case CD1_IN: // CCD1 is tied to either CVS1 or CVS2.
                             // This is not a valid combination.
                printk("MPI: Unknown card plugged into slot\n");
                break;

            case 0x00000000:  // CCD1 and CCD2 are tied to ground.
                cardtype = MPI_CARDTYPE_PCMCIA;
                printk("MPI: Detected 3.3 vdc 16-bit PCMCIA card\n");
                break;
            }
            break;

        case (VS2_IN|VS1_IN):  // CVS1 and CVS2 are open or tied to CCD1/CCD2.
                          // 5 valid voltage options.

            switch (data32 & (CD2_IN|CD1_IN))  // Test the values of CCD1 and CCD2.
            {
            case (CD2_IN|CD1_IN):  // CCD1 and CCD2 are tied to 1 of the CVS pins.
                              // This is not a valid combination.
                printk("MPI: Unknown card plugged into slot\n");
                break;

            case CD2_IN:  // CCD2 is tied to either CVS1 or CVS2.
                              // CCD1 is tied to ground.
                MPI->pcmcia_cntl1 = (CARDBUS_ENABLE|PCMCIA_GPIO_ENABLE|VS1_OEN);// Drive CVS2 to a 0.
                mdelay(1);
                data32 = MPI->pcmcia_cntl1;
                if (data32 & CD2_IN) {  // CCD2 is tied to CVS1.
                    printk("MPI: Detected y.y vdc Cardbus card\n");
                } else {                    // CCD2 is tied to CVS2.
                    printk("MPI: Detected x.x vdc Cardbus card\n");
                }
                cardtype = MPI_CARDTYPE_CARDBUS;
                break;

            case CD1_IN: // CCD1 is tied to either CVS1 or CVS2.
                             // CCD2 is tied to ground.

                MPI->pcmcia_cntl1 = (CARDBUS_ENABLE|PCMCIA_GPIO_ENABLE|VS1_OEN);// Drive CVS2 to a 0.
                mdelay(1);
                data32 = MPI->pcmcia_cntl1;
                if (data32 & CD1_IN) {// CCD1 is tied to CVS1.
                    printk("MPI: Detected 3.3 vdc Cardbus card\n");
                } else {                    // CCD1 is tied to CVS2.
                    printk("MPI: Detected x.x and y.y Cardbus card\n");
                }
                cardtype = MPI_CARDTYPE_CARDBUS;
                break;

            case 0x00000000:  // CCD1 and CCD2 are tied to ground.
                cardtype = MPI_CARDTYPE_PCMCIA;
                printk("MPI: Detected 5 vdc 16-bit PCMCIA card\n");
                break;
            }
            break;

        default:
            printk("MPI: Unknown card plugged into slot\n");
            break;

        }
    }
    return cardtype;
}

/*
 * mpi_DetectPcCard: Detect the plugged in PC-Card
 * Return: < 0 => Unknown card detected
 *         0 => No card detected
 *         1 => 16-bit card detected
 *         2 => 32-bit CardBus card detected
 */
static int mpi_DetectPcCard(void)
{
    int cardtype;

    cardtype = cardtype_vcc_detect();
    switch(cardtype) {
        case MPI_CARDTYPE_PCMCIA:
            MPI->pcmcia_cntl1 &= ~(CARDBUS_ENABLE|PCMCIA_ENABLE|PCMCIA_GPIO_ENABLE); // disable enable bits
            MPI->pcmcia_cntl1 |= (PCMCIA_ENABLE | PCMCIA_GPIO_ENABLE);
            mpi_InitPcmciaSpace();
            mpi_ResetPcCard(cardtype, FALSE);
            // Hold card in reset for 10ms
            mdelay(10);
            mpi_ResetPcCard(cardtype, TRUE);
            // Let card come out of reset
            mdelay(100);
            break;
        case MPI_CARDTYPE_CARDBUS:
            // 8 => CardBus Enable
            // 1 => PCI Slot Number
            // C => Float VS1 & VS2
            MPI->pcmcia_cntl1 = (MPI->pcmcia_cntl1 & 0xFFFF0000) |
                                CARDBUS_ENABLE |
                                (CARDBUS_SLOT << 8)|
                                VS2_OEN |
                                VS1_OEN | PCMCIA_GPIO_ENABLE;
            /* access to this memory window will be to/from CardBus */
            MPI->l2pmremap1 |= CARDBUS_MEM;

            // Need to reset the Cardbus Card. There's no CardManager to do this,
            // and we need to be ready for PCI configuration.
            mpi_ResetPcCard(cardtype, FALSE);
            // Hold card in reset for 10ms
            mdelay(10);
            mpi_ResetPcCard(cardtype, TRUE);
            // Let card come out of reset
            mdelay(100);
            break;
        default:
            break;
    }
    return cardtype;
}
#endif

static int mpi_init(void)
{
    unsigned long data;
    unsigned int chipid;
    unsigned int chiprev;
    unsigned long sdramsize;
    unsigned int modesel;

    chipid  = (PERF->RevID & 0xFFFE0000) >> 16;
    chiprev = (PERF->RevID & 0xFF);
    sdramsize = getMemorySize();

    // UBUS to PCI address range
    // Memory Window 1. Used for devices in slot 0. Potentially can be CardBus
    MPI->l2pmrange1 = ~(BCM_PCI_MEM_SIZE-1);
    // UBUS to PCI Memory base address. This is akin to the ChipSelect base
    // register.
    MPI->l2pmbase1 = BCM_CB_MEM_BASE & BCM_PCI_ADDR_MASK;
    // UBUS to PCI Remap Address. Replaces the masked address bits in the
    // range register with this setting.
    // Also, enable direct I/O and direct Memory accesses
    MPI->l2pmremap1 = (BCM_PCI_MEM_BASE | MEM_WINDOW_EN);

    // Memory Window 2. Used for devices in other slots
    MPI->l2pmrange2 = ~(BCM_PCI_MEM_SIZE-1);
    // UBUS to PCI Memory base address.
    MPI->l2pmbase2 = BCM_PCI_MEM_BASE & BCM_PCI_ADDR_MASK;
    // UBUS to PCI Remap Address
    MPI->l2pmremap2 = (BCM_PCI_MEM_BASE | MEM_WINDOW_EN);

    // Setup PCI I/O Window range. Give 64K to PCI I/O
    MPI->l2piorange = ~(BCM_PCI_IO_SIZE-1);
    // UBUS to PCI I/O base address
    MPI->l2piobase = BCM_PCI_IO_BASE & BCM_PCI_ADDR_MASK;
    // UBUS to PCI I/O Window remap
    MPI->l2pioremap = (BCM_PCI_IO_BASE | MEM_WINDOW_EN);

    // enable PCI related GPIO pins and data swap between system and PCI bus
    MPI->locbuscntrl = (EN_PCI_GPIO | DIR_U2P_NOSWAP);

    /* Enable BusMaster and Memory access mode */
    data = mpi_GetLocalPciConfigReg(PCI_COMMAND);
    data |= (PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
    mpi_SetLocalPciConfigReg(PCI_COMMAND, data);

    /* Configure two 16 MByte PCI to System memory regions. */
    /* These memory regions are used when PCI device is a bus master */
    /* Accesses to the SDRAM from PCI bus will be "byte swapped" for this region */
    mpi_SetLocalPciConfigReg(PCI_BASE_ADDRESS_3, BCM_HOST_MEM_SPACE1);

    MPI->sp0remap = MEM_WINDOW_EN;

    /* Accesses to the SDRAM from PCI bus will be "byte swapped" for this region */
    mpi_SetLocalPciConfigReg(PCI_BASE_ADDRESS_4, BCM_HOST_MEM_SPACE2);

    MPI->sp1remap = MEM_WINDOW_EN;

    modesel = MPI->pcimodesel;
    modesel &= ~PCI_INT_BUS_RD_PREFETCH;
    modesel |= 0x100;
    MPI->pcimodesel = modesel;

    MPI->sp0range = ~(sdramsize-1);
    MPI->sp1range = ~(sdramsize-1);
    /*
     * Change PCI Cfg Reg. offset 0x40 to PCI memory read retry count infinity
     * by set 0 in bit 8~15.  This resolve read Bcm4306 srom return 0xffff in
     * first read.
     */
    data = mpi_GetLocalPciConfigReg(BRCM_PCI_CONFIG_TIMER);
    data &= ~BRCM_PCI_CONFIG_TIMER_RETRY_MASK;
    data |= 0x00000080;
    mpi_SetLocalPciConfigReg(BRCM_PCI_CONFIG_TIMER, data);

    /* enable pci interrupt */
    MPI->locintstat |= (EXT_PCI_INT << 16);

    return 0;
}
#endif

#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
static void pcie_init(void)
{
    /* pcie clock enable*/
    PERF->blkEnables |= PCIE_CLK_EN;

    /* pcie serdes enable */
#if defined(CONFIG_BCM96816)   
    GPIO->SerdesCtl |= (SERDES_PCIE_ENABLE|SERDES_PCIE_EXD_ENABLE);
#endif
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362)
    MISC->miscSerdesCtrl |= (SERDES_PCIE_ENABLE|SERDES_PCIE_EXD_ENABLE);
#endif    

    /* reset pcie and ext device */
    PERF->softResetB &= ~(SOFT_RST_PCIE|SOFT_RST_PCIE_EXT|SOFT_RST_PCIE_CORE);
#if defined(CONFIG_BCM96328)
    PERF->softResetB &= ~SOFT_RST_PCIE_HARD;
#endif
    mdelay(10);
    PERF->softResetB |= (SOFT_RST_PCIE|SOFT_RST_PCIE_CORE);
#if defined(CONFIG_BCM96328)
    PERF->softResetB |= SOFT_RST_PCIE_HARD;
#endif
    mdelay(10);
    PERF->softResetB |= (SOFT_RST_PCIE_EXT);
    /* this is a critical delay */
    mdelay(200);
}
#endif

#if defined(CONFIG_BCM96368)

static int __init bcm6368_hw_init(void)
{
#if defined(CONFIG_PCI)
    unsigned long data;
#endif
    unsigned short GPIOOverlays;

    /* Enable SPI interface */
    PERF->blkEnables |= SPI_CLK_EN;

    GPIO->GPIOMode = 0;

    if( BpGetGPIOverlays(&GPIOOverlays) == BP_SUCCESS ) {

        if (GPIOOverlays & BP_OVERLAY_PHY) {
            GPIO->GPIOMode |= (GPIO_MODE_ANALOG_AFE_0 | GPIO_MODE_ANALOG_AFE_1);
            GPIO->GPIODir |= (GPIO_MODE_ANALOG_AFE_0 | GPIO_MODE_ANALOG_AFE_1);
        }

        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_0) {
            GPIO->GPIOMode |= GPIO_MODE_EPHY0_LED;
            GPIO->GPIODir |= GPIO_MODE_EPHY0_LED;
        }

        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_1) {
            GPIO->GPIOMode |= GPIO_MODE_EPHY1_LED;
            GPIO->GPIODir |= GPIO_MODE_EPHY1_LED;
        }

        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_2) {
            GPIO->GPIOMode |= GPIO_MODE_EPHY2_LED;
            GPIO->GPIODir |= GPIO_MODE_EPHY2_LED;
        }

        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_3) {
            GPIO->GPIOMode |= GPIO_MODE_EPHY3_LED;
            GPIO->GPIODir |= GPIO_MODE_EPHY3_LED;
        }

        if (GPIOOverlays & BP_OVERLAY_SERIAL_LEDS) {
            GPIO->GPIOMode |= (GPIO_MODE_SERIAL_LED_CLK | GPIO_MODE_SERIAL_LED_DATA);
            GPIO->GPIODir |= (GPIO_MODE_SERIAL_LED_CLK | GPIO_MODE_SERIAL_LED_DATA);
            GPIO->SerialLed = 0xffffffff;
        }

        if (GPIOOverlays & BP_OVERLAY_INET_LED) {
            GPIO->GPIOMode |= GPIO_MODE_INET_LED;
            GPIO->GPIODir |= GPIO_MODE_INET_LED;
        }

#if defined(CONFIG_PCI)
        if (GPIOOverlays & BP_OVERLAY_PCI) {
            GPIO->GPIOMode |= (GPIO_MODE_PCI_GNT0 |
                GPIO_MODE_PCI_REQ0 |
                GPIO_MODE_PCI_INTB |
                GPIO_MODE_PCI_GNT1 |
                GPIO_MODE_PCI_REQ1);
            mpi_init();

            if (GPIOOverlays & BP_OVERLAY_CB) {
                GPIO->GPIOMode |= (GPIO_MODE_PCMCIA_VS2 |
                    GPIO_MODE_PCMCIA_VS1 |
                    GPIO_MODE_PCMCIA_CD2 |
                    GPIO_MODE_PCMCIA_CD1);
                mpi_DetectPcCard();
            }
            else {
                /*
                 * CardBus support is defaulted to Slot 0 because there is no external
                 * IDSEL for CardBus.  To disable the CardBus and allow a standard PCI
                 * card in Slot 0 set the cbus_idsel field to 0x1f.
                */
                data = MPI->pcmcia_cntl1;
                data |= CARDBUS_IDSEL;
                MPI->pcmcia_cntl1 = data;
            }
        }
#endif
    }

#if defined(CONFIG_USB)
    PERF->blkEnables |= USBH_CLK_EN;
    mdelay(100);
    USBH->SwapControl = EHCI_ENDIAN_SWAP | OHCI_ENDIAN_SWAP;
    USBH->Setup |= USBH_IOC;
#endif

    return 0;
}
#define bcm63xx_specific_hw_init() bcm6368_hw_init()

#elif defined(CONFIG_BCM96816)
#define BCM_SPI_SLAVE_ID     1
#define BCM_SPI_SLAVE_BUS    LEG_SPI_BUS_NUM
#define BCM_SPI_SLAVE_FREQ   6250000

static void bcm6829SpiIfInit( void )
{
	uint8_t  init_seq[3] = { 0x11, 0x01, 0xfc };
	uint8_t  init_csr[8] = { 0x11, 0x01, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00 };

    //printk(KERN_ALERT "Init SPI interface on 6829\n");

	if ( SPI_STATUS_OK != BcmSpi_Write(&init_seq[0], 3, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID, BCM_SPI_SLAVE_FREQ) )
	{
        printk(KERN_ALERT "bcm6829SpiIfInit: SPI write error\n");
		return;
	}

	if ( SPI_STATUS_OK != BcmSpi_Write(&init_csr[0], 8, BCM_SPI_SLAVE_BUS, BCM_SPI_SLAVE_ID, BCM_SPI_SLAVE_FREQ) )
	{
        printk(KERN_ALERT "bcm6829SpiIfInit: SPI write error\n");
		return;
	}
}

/* FIXME: The following function is temporary and must be rewritten when the
   GPON specific parameters are supported in boardparms.c */
#if defined(CONFIG_BCM_GPON_FPGA)
static void reset_gpon_fpga(void)
{
    unsigned short fpgaResetGpio;
    int rc;
    int retryCnt = 10;
    unsigned int confDone;

#define GPON_FPGA_CONFIG_GPIO   37
#define GPON_FPGA_DONE_GPIO     38

#if defined(GPON_FPGA_PROGRAM)
    /* re-program FPGA */
    printk("Programming GPON FPGA...");

    GPIO->GPIODir |= GPIO_NUM_TO_MASK(GPON_FPGA_CONFIG_GPIO);

    GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(GPON_FPGA_CONFIG_GPIO);;

    msleep(1000);

    GPIO->GPIOio |= GPIO_NUM_TO_MASK(GPON_FPGA_CONFIG_GPIO);;

    printk("Done. GPIOio=0x%016llX\n", GPIO->GPIOio);
#endif

    /* check if FPGA was programmed successfully */
    do
    {
        confDone = (GPIO->GPIOio & GPIO_NUM_TO_MASK(GPON_FPGA_DONE_GPIO)) >> GPON_FPGA_DONE_GPIO;

        printk("GPON FPGA State (%d): 0x%X (0x%016llX, 0x%016llX / 0x%016llX)\n",
               retryCnt, confDone, GPIO->GPIODir, GPIO->GPIOio,
               GPIO_NUM_TO_MASK(GPON_FPGA_DONE_GPIO));

        if(!confDone)
        {
            msleep(500);
        }

    } while(!confDone && --retryCnt);

    if(!confDone)
    {
        printk("\nERROR: *** GPON FPGA is not programmed ***\n\n");
        return;
    }

    /* reset FPGA */
    printk("Resetting GPON FPGA...");

    rc = BpGetFpgaResetGpio(&fpgaResetGpio);
    if (rc != BP_SUCCESS) {
      printk("\nERROR: *** FPGA Reset GPIO not found **\n\n");
        return;
    }

    GPIO->GPIODir |= GPIO_NUM_TO_MASK(fpgaResetGpio);
    GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(fpgaResetGpio);

    msleep(1000);

    GPIO->GPIOio |= GPIO_NUM_TO_MASK(fpgaResetGpio);

    msleep(1000);

    printk("Done\n");
}
#endif /* CONFIG_BCM_GPON */

static int __init bcm6816_hw_init(void)
{
    unsigned short GPIOOverlays;
    unsigned char  portInfo6829;

    /* Enable SPI interface and GPON MAC*/
    PERF->blkEnables |= SPI_CLK_EN | GPON_CLK_EN | GPON_SER_CLK_EN;
    udelay(500);
    /* Force the GPON serdes laser off so we don't introduce glitches on the fiber during init.*/
    GPON_SERDES->laserCfg = GPON_SERDES_LASERMODE_FORCE_OFF;
    GPON_SERDES->miscCfg = 5;

    GPIO->GPIOMode = 0;

    if( BpGetGPIOverlays(&GPIOOverlays) == BP_SUCCESS ) {

        if (GPIOOverlays & BP_OVERLAY_GPON_TX_EN_L) {
            GPIO->GPIOMode |= GPIO_MODE_GPON_TX_EN_L;
            GPIO->GPIODir |= GPIO_MODE_GPON_TX_EN_L;
        }
        else {
          GPIO->GPIODir |= GPIO_MODE_GPON_TX_EN_L;
          GPIO->GPIOio |= GPIO_MODE_GPON_TX_EN_L; /*Take optics out of reset*/
        }

        if (GPIOOverlays & BP_OVERLAY_GPHY_LED_0) {
            GPIO->GPIOMode |= GPIO_MODE_GPHY0_LED;
            GPIO->GPIODir |= GPIO_MODE_GPHY0_LED;
        }

        if (GPIOOverlays & BP_OVERLAY_GPHY_LED_1) {
            GPIO->GPIOMode |= GPIO_MODE_GPHY1_LED;
            GPIO->GPIODir |= GPIO_MODE_GPHY1_LED;
        }

        if (GPIOOverlays & BP_OVERLAY_SERIAL_LEDS) {
            GPIO->GPIOMode |= (GPIO_MODE_SERIAL_LED_CLK | GPIO_MODE_SERIAL_LED_DATA);
            GPIO->GPIODir |= (GPIO_MODE_SERIAL_LED_CLK | GPIO_MODE_SERIAL_LED_DATA);
            GPIO->SerialLed = 0xffffffff;
        }

        if (GPIOOverlays & BP_OVERLAY_MOCA_LED) {
            GPIO->GPIOMode |= GPIO_MODE_MOCA_LED;
            GPIO->GPIODir |= GPIO_MODE_MOCA_LED;
        }

#if defined(CONFIG_BCM_GPON_FPGA)
        /* Initialize GPON FPGA (before PCI is initialized) */
        reset_gpon_fpga();
#endif /*CONFIG_BCM_GPON_FPGA*/

#if defined(CONFIG_PCI)
        if (GPIOOverlays & BP_OVERLAY_PCI) {
            GPIO->GPIOMode |= (
                GPIO_MODE_PCI_GNT0
               |GPIO_MODE_PCI_REQ0
               |GPIO_MODE_PCI_INTB
#if !defined(CONFIG_BCM_GPON_FPGA) /*Don't enable PCI1 on GPON FPGA board. GPON Fail LED is on that GPIO*/
               |GPIO_MODE_PCI_GNT1
               |GPIO_MODE_PCI_REQ1
#endif /*CONFIG_BCM_GPON_FPGA*/
            );

            mpi_init();
        }
        pcie_init();
#endif
    }

#if defined(CONFIG_USB)
    PERF->blkEnables |= USBH_CLK_EN;
    mdelay(100);
    USBH->SwapControl = EHCI_ENDIAN_SWAP | OHCI_ENDIAN_SWAP;
    USBH->Setup |= USBH_IOC;
#else
    PERF->blkEnables &= ~USBH_CLK_EN;
#endif

    if( BP_SUCCESS == BpGet6829PortInfo(&portInfo6829) )
    {
       if ( 0 != portInfo6829 )
       {
          bcm6829SpiIfInit();
       }
    }

    return 0;
}
#define bcm63xx_specific_hw_init() bcm6816_hw_init()

#elif defined(CONFIG_BCM96362)

static int __init bcm6362_hw_init(void)
{
    unsigned short GPIOOverlays;

    /* Set LED blink rate for activity LEDs to 80mS */
    LED->ledInit &= LED_FAST_INTV_MASK;
    LED->ledInit |= (LED_INTERVAL_20MS * 4) << LED_FAST_INTV_SHIFT;

    if( BpGetGPIOverlays(&GPIOOverlays) == BP_SUCCESS ) {
        if (GPIOOverlays & BP_OVERLAY_SERIAL_LEDS) {
            GPIO->GPIOMode |= (GPIO_MODE_SERIAL_LED_CLK | GPIO_MODE_SERIAL_LED_DATA);
            LED->ledInit |= LED_SERIAL_LED_EN;
            /* MUX LEDs 0-7 to LED controller */
            LED->ledSerialMuxSelect = 0xff;
        }
        /* Map HW LEDs to LED controller inputs and enable LED controller to driver GPIO */
            LED->ledLinkActSelLow |= ((1 << LED_USB) << LED_0_ACT_SHIFT);
        if (GPIOOverlays & BP_OVERLAY_USB_LED) {
            GPIO->LEDCtrl |= (1 << LED_USB);
        }
            LED->ledLinkActSelLow |= ((1 << LED_INET) << LED_1_ACT_SHIFT);
            /* Start with INET LED disabled */
            LED->ledHWDis |= (1 << LED_INET);
        if (GPIOOverlays & BP_OVERLAY_INET_LED) {
            GPIO->LEDCtrl |= (1 << LED_INET);
        }
#if 0 // Wait until speed LEDs are fixed in B0
            LED->ledLinkActSelHigh |= ((1 << (LED_ENET0 - 4)) << LED_4_ACT_SHIFT);
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_0) {
            GPIO->LEDCtrl |= (1 << LED_ENET0);
        }
            LED->ledLinkActSelHigh |= ((1 << (LED_ENET1 - 4)) << LED_5_ACT_SHIFT);
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_1) {
            GPIO->LEDCtrl |= (1 << LED_ENET1);
        }
            LED->ledLinkActSelHigh |= ((1 << (LED_ENET2 - 4)) << LED_6_ACT_SHIFT);
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_2) {
            GPIO->LEDCtrl |= (1 << LED_ENET2);
        }
            LED->ledLinkActSelHigh |= ((1 << (LED_ENET3 - 4)) << LED_7_ACT_SHIFT);
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_3) {
            GPIO->LEDCtrl |= (1 << LED_ENET3);
        }
#endif
    }

#if defined(CONFIG_USB)
    PERF->blkEnables |= USBH_CLK_EN;
    mdelay(100);
    USBH->SwapControl = EHCI_ENDIAN_SWAP | OHCI_ENDIAN_SWAP;
    USBH->Setup |= USBH_IOC;
#else
    PERF->blkEnables &= ~USBH_CLK_EN;
#endif

#if !(defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))
    PERF->blkEnables &= ~FAP_CLK_EN;
#endif

#if defined(CONFIG_PCI)
    /* Enable WOC */  
    PERF->blkEnables |=WLAN_OCP_CLK_EN;
    mdelay(10);
    PERF->softResetB &= ~(SOFT_RST_WLAN_SHIM_UBUS | SOFT_RST_WLAN_SHIM);
    mdelay(1);
    PERF->softResetB |= (SOFT_RST_WLAN_SHIM_UBUS | SOFT_RST_WLAN_SHIM);
    mdelay(1);
    WLAN_SHIM->ShimMisc = (WLAN_SHIM_FORCE_CLOCKS_ON|WLAN_SHIM_MACRO_SOFT_RESET);
    mdelay(1);
    WLAN_SHIM->MacControl = (SICF_FGC|SICF_CLOCK_EN);
    WLAN_SHIM->ShimMisc = WLAN_SHIM_FORCE_CLOCKS_ON;
    WLAN_SHIM->ShimMisc = 0;
    WLAN_SHIM->MacControl = SICF_CLOCK_EN;

    /* enable PCIE */
    pcie_init();
#endif    
    return 0;
}
#define bcm63xx_specific_hw_init() bcm6362_hw_init()

#elif defined(CONFIG_BCM96328)

static int __init bcm6328_hw_init(void)
{
    unsigned short GPIOOverlays;

    /* Set LED blink rate for activity LEDs to 80mS */
    LED->ledInit &= LED_FAST_INTV_MASK;
    LED->ledInit |= (LED_INTERVAL_20MS * 4) << LED_FAST_INTV_SHIFT;

    if( BpGetGPIOverlays(&GPIOOverlays) == BP_SUCCESS ) {
    }

#if defined(CONFIG_USB)
    PERF->blkEnables |= USBH_CLK_EN;
    mdelay(100);
    USBH->SwapControl = EHCI_ENDIAN_SWAP | OHCI_ENDIAN_SWAP;
    USBH->Setup |= USBH_IOC;
#else
    PERF->blkEnables &= ~USBH_CLK_EN;
#endif

#if defined(CONFIG_PCI)
    /* enable PCIE */
    pcie_init();
#endif    
    return 0;
}
#define bcm63xx_specific_hw_init() bcm6328_hw_init()

#elif defined(CONFIG_BCM93380)

static int __init bcm3380_hw_init(void)
{
#if defined(CONFIG_BCM_LOT1)
   //*(unsigned int *)0xb4e00180 = ((*(unsigned int *)0xb4e00180) | 0x4000);
#endif
    return 0;
}
#define bcm63xx_specific_hw_init() bcm3380_hw_init()

#elif defined(CONFIG_BCMKILAUEA)

static int __init bcmkilauea_hw_init(void)
{
    return 0;
}
#define bcm63xx_specific_hw_init() bcmkilauea_hw_init()

#elif defined(CONFIG_BCM93383)

static int __init bcm3383_hw_init(void)
{
    return 0;
}
#define bcm63xx_specific_hw_init() bcm3383_hw_init()

#endif

static int __init bcm63xx_hw_init(void)
{
#if !defined(CONFIG_BRCM_IKOS)
    kerSysFlashInit();
#endif

    return bcm63xx_specific_hw_init();
}
arch_initcall(bcm63xx_hw_init);


static int __init brcm63xx_setup(void)
{
    extern int panic_timeout;

    _machine_restart = brcm_machine_restart;
    _machine_halt = brcm_machine_halt;
    pm_power_off = brcm_machine_halt;

    panic_timeout = 1;

    return 0;
}

arch_initcall(brcm63xx_setup);


unsigned long getMemorySize(void)
{
#if defined(CONFIG_BRCM_IKOS)
    return(31 * 1024 * 1024); /* voice DSP is loaded after this amount */
#elif defined(CONFIG_BCM96368)
    unsigned long size;
    unsigned long memCfg;

    size = 1;
    memCfg = MEMC->Config;
    /* Number of column bits */
    size <<= (((memCfg & MEMC_COL_MASK) >> MEMC_COL_SHFT) + 8);
    /* Plus number of row bits */
    size <<= (((memCfg & MEMC_ROW_MASK) >> MEMC_ROW_SHFT) + 11);
    /* Plus bus width */
    if (((memCfg & MEMC_WIDTH_MASK) >> MEMC_WIDTH_SHFT) == MEMC_32BIT_BUS)
        size <<= 2;
    else
        size <<= 1;

    /* Plus number of banks */
    size <<= 2;

    return( size );
#elif defined(CONFIG_BCM93380) || defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
    return CM_SDRAM_SIZE;
#else
    return (DDR->CSEND << 24);
#endif
}


/* Pointers to memory buffers allocated for the DSP module */
void *dsp_core;
void *dsp_init;
EXPORT_SYMBOL(dsp_core);
EXPORT_SYMBOL(dsp_init);
void __init allocDspModBuffers(void);
/*
*****************************************************************************
** FUNCTION:   allocDspModBuffers
**
** PURPOSE:    Allocates buffers for the init and core sections of the DSP
**             module. This module is special since it has to be allocated
**             in the 0x800.. memory range which is not mapped by the TLB.
**
** PARAMETERS: None
** RETURNS:    Nothing
*****************************************************************************
*/
void __init allocDspModBuffers(void)
{
#if defined(CONFIG_BCM_ENDPOINT_MODULE)
    printk("Allocating memory for DSP module core and initialization code\n");

  dsp_core = (void*)((DSP_CORE_SIZE > 0) ? alloc_bootmem((unsigned long)DSP_CORE_SIZE) : NULL);
  dsp_init = (void*)((DSP_INIT_SIZE > 0) ? alloc_bootmem((unsigned long)DSP_INIT_SIZE) : NULL);

  printk("Allocated DSP module memory - CORE=0x%x SIZE=%d, INIT=0x%x SIZE=%d\n",
         (unsigned int)dsp_core, DSP_CORE_SIZE, (unsigned int)dsp_init , DSP_INIT_SIZE);
#endif
}


/* Pointers to memory buffers allocated for the FAP module */
void *fap_core;
void *fap_init;
EXPORT_SYMBOL(fap_core);
EXPORT_SYMBOL(fap_init);
void __init allocFapModBuffers(void);
/*
*****************************************************************************
** FUNCTION:   allocFapModBuffers
**
** PURPOSE:    Allocates buffers for the init and core sections of the FAP
**             module. This module is special since it has to be allocated
**             in the 0x800.. memory range which is not mapped by the TLB.
**
** PARAMETERS: None
** RETURNS:    Nothing
*****************************************************************************
*/
void __init allocFapModBuffers(void)
{
#if (defined(CONFIG_BCM96362) || defined(CHIP_6362)) && defined(CONFIG_BCM_FAP_MODULE)
    printk("********** Allocating memory for FAP module core and initialization code\n");

    fap_core = (void*)((FAP_CORE_SIZE > 0) ? alloc_bootmem((unsigned long)FAP_CORE_SIZE) : NULL);
    fap_init = (void*)((FAP_INIT_SIZE > 0) ? alloc_bootmem((unsigned long)FAP_INIT_SIZE) : NULL);

    printk("Allocated FAP module memory - CORE=0x%x SIZE=%d, INIT=0x%x SIZE=%d\n",
           (unsigned int)fap_core, FAP_CORE_SIZE, (unsigned int)fap_init , FAP_INIT_SIZE);
#endif
}

