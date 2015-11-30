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
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/addrspace.h>

#include <bcm_intr.h>
#include <bcm_map_part.h>
#include <bcmpci.h>
#include <boardparms.h>

#include <linux/delay.h>


#if defined(CONFIG_USB) || defined(CONFIG_PCI)
#if 0
#define DPRINT(x...)        printk(x)
#else
#define DPRINT(x...)
#endif

static int pci63xx_int_usb_read(unsigned int devfn, int where, u32 * value, int size);
static int pci63xx_int_usb_write(unsigned int devfn, int where, u32 * value, int size);

static bool usb_mem_size_rd = FALSE;
static uint32 usb_mem_base = 0;
static uint32 usb_cfg_space_cmd_reg = 0;

static int pci63xx_int_usb20_read(unsigned int devfn, int where, u32 * value, int size);
static int pci63xx_int_usb20_write(unsigned int devfn, int where, u32 * value, int size);

static bool usb20_mem_size_rd = FALSE;
static uint32 usb20_mem_base = 0;
static uint32 usb20_cfg_space_cmd_reg = 0;
#endif

#if defined(CONFIG_BCM96362)
static int pci63xx_wlan_pci_read(unsigned int devfn, int where, u32 * value, int size);
static int pci63xx_wlan_pci_write(unsigned int devfn, int where, u32 val, int size);
static uint32 pci63xx_wlan_soft_config_space[WLAN_ONCHIP_DEV_NUM][64] = {
	{WLAN_ONCHIP_PCI_ID, 0x00100006, 0x02800000, 0x00000010,  
	 WLAN_CHIPC_BASE,    0x00000000, 0x00000000, 0x00000000,  
	 0x00000000,  	     0x00000000, 0x00000000, 0x636214e4, /* 0x04f414e4 */
	 0x00000000,  	     0x00000040, 0x00000000, 0x0000010f,  	
	 0xce035801,         0x00004008, 0x0080d005, 0x00000000,
	 0x00000000,         0x00000000, 0x00784809, 0x00000010,
	 0x00000000,         0x00000000, 0x00000000, 0x00000000,
	 0x00000000,         0x00000000, 0x00000000, 0x00000000, 
	 0x18001000,         0x00000000, 0xffffffff, 0x00000003,
	 0x00000000,         0x00000100, 0x00000000, 0x00000000,
	 0x00000000,         0x00000000, 0x00010000, 0x18101000,
	 0x00000000,         0x00000000, 0x00000000, 0x00000000,
	 0x00000000,         0x00000000, 0x00000000, 0x00000000,
	 0x00010010,         0x00288fa0, 0x00190100, 0x00176c11,
	 0x30110040,         0x00000000, 0x00000000, 0x00000000,
	 0x00000000,         0x00000000, 0x00000000, 0x00000000,	
	 },
};                                      
#endif

#if (defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816))

static bool pci_mem_size_rd = FALSE;

static void mpi_SetupPciConfigAccess(uint32 addr)
{
    MPI->l2pcfgctl = (DIR_CFG_SEL | DIR_CFG_USEREG | addr) & ~CONFIG_TYPE;
}

static void mpi_ClearPciConfigAccess(void)
{
    MPI->l2pcfgctl = 0x00000000;
}
#endif

#if defined(CONFIG_USB)
/* --------------------------------------------------------------------------
    Name: pci63xx_int_usb_write
Abstract: PCI Config write on internal device(s)
 -------------------------------------------------------------------------- */
static int pci63xx_int_usb_write(unsigned int devfn, int where, u32 * value, int size)
{
    switch (size) {
        case 1:
            DPRINT("W => Slot: %d Where: %2X Len: %d Data: %02X\n",
                PCI_SLOT(devfn), where, size, *value);
            break;
        case 2:
            DPRINT("W => Slot: %d Where: %2X Len: %d Data: %04X\n",
                PCI_SLOT(devfn), where, size, *value);
            switch (where) {
                case PCI_COMMAND:
                    usb_cfg_space_cmd_reg = *value;
                    break;
                default:
                    break;
            }
            break;
        case 4:
            DPRINT("W => Slot: %d Where: %2X Len: %d Data: %08lX\n",
                PCI_SLOT(devfn), where, size, *value);
            switch (where) {
                case PCI_BASE_ADDRESS_0:
                    if (*value == 0xffffffff) {
                        usb_mem_size_rd = TRUE;
                    } else {
                        usb_mem_base = *value;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    return PCIBIOS_SUCCESSFUL;
}

/* --------------------------------------------------------------------------
    Name: pci63xx_int_usb_read
Abstract: PCI Config read on internal device(s)
 -------------------------------------------------------------------------- */
static int pci63xx_int_usb_read(unsigned int devfn, int where, u32 * value, int size)
{
    uint32 retValue = 0xFFFFFFFF;

    // For now, this is specific to the USB Host controller. We can
    // make it more general if we have to...
    // Emulate PCI Config accesses
    switch (where) {
        case PCI_VENDOR_ID:
        case PCI_DEVICE_ID:
#if defined(CONFIG_BCM96816)
            retValue = PCI_VENDOR_ID_BROADCOM | 0x68000000;
#else
            retValue = PCI_VENDOR_ID_BROADCOM | 0x63000000;
#endif
            break;
        case PCI_COMMAND:
        case PCI_STATUS:
            retValue = (0x0006 << 16) | usb_cfg_space_cmd_reg;
            break;
        case PCI_CLASS_REVISION:
        case PCI_CLASS_DEVICE:
            retValue = (PCI_CLASS_SERIAL_USB << 16) | (0x10 << 8) | 0x01;
            break;
        case PCI_BASE_ADDRESS_0:
            if (usb_mem_size_rd) {
                retValue = USB_BAR0_MEM_SIZE;
            } else {
                if (usb_mem_base != 0)
                    retValue = usb_mem_base;
                else
                    retValue = USB_OHCI_BASE;
            }
            usb_mem_size_rd = FALSE;
            break;
        case PCI_CACHE_LINE_SIZE:
            retValue = L1_CACHE_BYTES/4;
            break;
        case PCI_LATENCY_TIMER:
            retValue = 0;
            break;
        case PCI_HEADER_TYPE:
            retValue = PCI_HEADER_TYPE_NORMAL;
            break;
        case PCI_SUBSYSTEM_VENDOR_ID:
            retValue = PCI_VENDOR_ID_BROADCOM;
            break;
        case PCI_SUBSYSTEM_ID:
#if defined(CONFIG_BCM96816)
            retValue = 0x6800;
#else
            retValue = 0x6300;
#endif
            break;
        case PCI_INTERRUPT_LINE:
            retValue = INTERRUPT_ID_USBH;
            break;
        default:
            break;
    }

    switch (size) {
        case 1:
            *value = (retValue >> ((where & 3) << 3)) & 0xff;
            DPRINT("R <= Slot: %d Where: %2X Len: %d Data: %02X\n",
                PCI_SLOT(devfn), where, size, *value);
            break;
        case 2:
            *value = (retValue >> ((where & 3) << 3)) & 0xffff;
            DPRINT("R <= Slot: %d Where: %2X Len: %d Data: %04X\n",
                PCI_SLOT(devfn), where, size, *value);
            break;
        case 4:
            *value = retValue;
            DPRINT("R <= Slot: %d Where: %2X Len: %d Data: %08lX\n",
                PCI_SLOT(devfn), where, size, *value);
            break;
        default:
            break;
    }
    return PCIBIOS_SUCCESSFUL;
}

/* --------------------------------------------------------------------------
    Name: pci63xx_int_usb20_write
Abstract: PCI Config write on internal device(s)
 -------------------------------------------------------------------------- */
static int pci63xx_int_usb20_write(unsigned int devfn, int where, u32 * value, int size)
{
    switch (size) {
        case 1:
            DPRINT("W => Slot: %d Where: %2X Len: %d Data: %02X\n",
                PCI_SLOT(devfn), where, size, *value);
            break;
        case 2:
            DPRINT("W => Slot: %d Where: %2X Len: %d Data: %04X\n",
                PCI_SLOT(devfn), where, size, *value);
            switch (where) {
                case PCI_COMMAND:
                    usb20_cfg_space_cmd_reg = *value;
                    break;
                default:
                    break;
            }
            break;
        case 4:
            DPRINT("W => Slot: %d Where: %2X Len: %d Data: %08lX\n",
                PCI_SLOT(devfn), where, size, *value);
            switch (where) {
                case PCI_BASE_ADDRESS_0:
                    if (*value == 0xffffffff) {
                        usb20_mem_size_rd = TRUE;
                    } else {
                        usb20_mem_base = *value;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    return PCIBIOS_SUCCESSFUL;
}

/* --------------------------------------------------------------------------
    Name: pci63xx_int_usb20_read
Abstract: PCI Config read on internal device(s)
 -------------------------------------------------------------------------- */
static int pci63xx_int_usb20_read(unsigned int devfn, int where, u32 * value, int size)
{
    uint32 retValue = 0xFFFFFFFF;

    // For now, this is specific to the USB Host controller. We can
    // make it more general if we have to...
    // Emulate PCI Config accesses
    switch (where) {
        case PCI_VENDOR_ID:
        case PCI_DEVICE_ID:
#if defined(CONFIG_BCM96816)
            retValue = PCI_VENDOR_ID_BROADCOM | 0x68000000;
#else
            retValue = PCI_VENDOR_ID_BROADCOM | 0x63000000;
#endif
            break;
        case PCI_COMMAND:
        case PCI_STATUS:
            retValue = (0x0006 << 16) | usb20_cfg_space_cmd_reg;
            break;
        case PCI_CLASS_REVISION:
        case PCI_CLASS_DEVICE:
            retValue = (PCI_CLASS_SERIAL_USB << 16) | (0x20 << 8) | 0x01;
            break;
        case PCI_BASE_ADDRESS_0:
            if (usb20_mem_size_rd) {
                retValue = USB_BAR0_MEM_SIZE;
            } else {
                if (usb20_mem_base != 0)
                    retValue = usb20_mem_base;
                else
                    retValue = USB_EHCI_BASE;
            }
            usb20_mem_size_rd = FALSE;
            break;
        case PCI_CACHE_LINE_SIZE:
            retValue = L1_CACHE_BYTES/4;
            break;
        case PCI_LATENCY_TIMER:
            retValue = 0;
            break;
        case PCI_HEADER_TYPE:
            retValue = PCI_HEADER_TYPE_NORMAL;
            break;
        case PCI_SUBSYSTEM_VENDOR_ID:
            retValue = PCI_VENDOR_ID_BROADCOM;
            break;
        case PCI_SUBSYSTEM_ID:
#if defined(CONFIG_BCM96816)
            retValue = 0x6800;
#else
            retValue = 0x6300;
#endif
            break;
        case PCI_INTERRUPT_LINE:
            retValue = INTERRUPT_ID_USBH20;
            break;
        default:
            break;
    }

    switch (size) {
        case 1:
            *value = (retValue >> ((where & 3) << 3)) & 0xff;
            DPRINT("R <= Slot: %d Where: %2X Len: %d Data: %02X\n",
                PCI_SLOT(devfn), where, size, *value);
            break;
        case 2:
            *value = (retValue >> ((where & 3) << 3)) & 0xffff;
            DPRINT("R <= Slot: %d Where: %2X Len: %d Data: %04X\n",
                PCI_SLOT(devfn), where, size, *value);
            break;
        case 4:
            *value = retValue;
            DPRINT("R <= Slot: %d Where: %2X Len: %d Data: %08lX\n",
                PCI_SLOT(devfn), where, size, *value);
            break;
        default:
            break;
    }

    return PCIBIOS_SUCCESSFUL;
}
#endif

#if defined(CONFIG_BCM96362)
/* --------------------------------------------------------------------------
    Name: pci63xx_wlan_pci_write
Abstract: PCI Config write on internal device(s)
    
    extra read/write to take care / pass through 
    BASE ADDR 0, where=16 , R/W
    INTERRUPT PIN, where=60, R/W
    COMMAND, where=4, R/W
    LATENCY TIMER, where=13
 -------------------------------------------------------------------------- */
static int pci63xx_wlan_pci_write(unsigned int devfn, int where, u32 val, int size)
{
    uint32 data;
        
    if(where >= 256) {
    	return PCIBIOS_BAD_REGISTER_NUMBER;    
    }
    data = pci63xx_wlan_soft_config_space[PCI_SLOT(devfn)-WLAN_ONCHIP_DEV_SLOT][where/4];

    switch(size) {
        case 1:
            data = (data & ~(0xff << ((where & 3) << 3))) |
                (val << ((where & 3) << 3));
            break;
        case 2:
            data = (data & ~(0xffff << ((where & 3) << 3))) |
                (val << ((where & 3) << 3));
            break;
        case 4:
            data = val;
            break;
        default:
            break;
    }
    pci63xx_wlan_soft_config_space[PCI_SLOT(devfn)-WLAN_ONCHIP_DEV_SLOT][where/4] = data;

    return PCIBIOS_SUCCESSFUL;
}

/* --------------------------------------------------------------------------
    Name: pci63xx_wlan_pci_read
Abstract: PCI Config read on internal device(s)
 -------------------------------------------------------------------------- */
static int pci63xx_wlan_pci_read(unsigned int devfn, int where, u32 * val, int size)
{
    uint32 data;    
    
    if(where >= 256) {
    	data = 0xffffffff;
    }	
    else	
    	data = pci63xx_wlan_soft_config_space[PCI_SLOT(devfn)-WLAN_ONCHIP_DEV_SLOT][where/4];

    switch(size) {
        case 1:
            *val = (data >> ((where & 3) << 3)) & 0xff;
            break;
        case 2:
            *val = (data >> ((where & 3) << 3)) & 0xffff;
            break;
        case 4:
            *val = data;
             /* Special case for reading PCI device range */
            if ((where >= PCI_BASE_ADDRESS_0) && (where <= PCI_BASE_ADDRESS_5)) {
                if (data == 0xffffffff) {
                	if (where == PCI_BASE_ADDRESS_0)
                		*val = 0xFFFF0000;//PCI_SIZE_64K;
                	else 
                		*val = 0;
                }
            }
            break;
        default:
            break;
    }
    return PCIBIOS_SUCCESSFUL;
}
#endif

int bcm63xx_pcibios_read(struct pci_bus *bus, unsigned int devfn,
    int where, int size, u32 * val)
{
#if (defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816))
    unsigned short GPIOOverlays;
#endif

#if defined(CONFIG_USB)
    if (PCI_SLOT(devfn) == USB_HOST_SLOT)
        return pci63xx_int_usb_read(devfn, where, val, size);
    if (PCI_SLOT(devfn) == USB20_HOST_SLOT)
        return pci63xx_int_usb20_read(devfn, where, val, size);
#endif

#if defined(CONFIG_BCM96362)
    if (PCI_SLOT(devfn) == WLAN_ONCHIP_DEV_SLOT)
       	return pci63xx_wlan_pci_read(devfn, where, val, size);
#endif

#if (defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816))
    if( BpGetGPIOverlays(&GPIOOverlays) == BP_SUCCESS ) {
        if (GPIOOverlays & BP_OVERLAY_PCI) {
            volatile unsigned char *ioBase = (unsigned char *)(MPI->l2piobase | KSEG1);
            uint32 data;

            mpi_SetupPciConfigAccess(BCM_PCI_CFG(PCI_SLOT(devfn), PCI_FUNC(devfn), where));
            data = *(uint32 *)ioBase;
            switch(size) {
                case 1:
                    *val = (data >> ((where & 3) << 3)) & 0xff;
                    break;
                case 2:
                    *val = (data >> ((where & 3) << 3)) & 0xffff;
                    break;
                case 4:
                    *val = data;
                     /* Special case for reading PCI device range */
                    if ((where >= PCI_BASE_ADDRESS_0) && (where <= PCI_BASE_ADDRESS_5)) {
                        if (pci_mem_size_rd) {
                            /* bcm63xx PCI memory window minimum size is 64K */
                            /* preserve the lower 4 bits */
                            if ((*val & PCI_BASE_ADDRESS_SPACE) != PCI_BASE_ADDRESS_SPACE_IO)
                                *val &= (PCI_SIZE_64K + ~PCI_BASE_ADDRESS_MEM_MASK);
                        }
                    }
                    break;
                default:
                    break;
            }
            pci_mem_size_rd = FALSE;
            mpi_ClearPciConfigAccess();
        }
    }
    else {
        *val = 0xffffffff;
    }
#endif 

    return PCIBIOS_SUCCESSFUL;
}

int bcm63xx_pcibios_write(struct pci_bus *bus, unsigned int devfn,
    int where, int size, u32 val)
{
#if (defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816))
    unsigned short GPIOOverlays;
#endif

#if defined(CONFIG_USB)
    if (PCI_SLOT(devfn) == USB_HOST_SLOT)
        return pci63xx_int_usb_write(devfn, where, &val, size);
    if (PCI_SLOT(devfn) == USB20_HOST_SLOT)
        return pci63xx_int_usb20_write(devfn, where, &val, size);
#endif

#if defined(CONFIG_BCM96362)
    if (PCI_SLOT(devfn) == WLAN_ONCHIP_DEV_SLOT)
       	return pci63xx_wlan_pci_write(devfn, where, val, size);
#endif

#if (defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816))
    if( BpGetGPIOverlays(&GPIOOverlays) == BP_SUCCESS ) {
        if (GPIOOverlays & BP_OVERLAY_PCI) {
            volatile unsigned char *ioBase = (unsigned char *)(MPI->l2piobase | KSEG1);
            uint32 data;

            mpi_SetupPciConfigAccess(BCM_PCI_CFG(PCI_SLOT(devfn), PCI_FUNC(devfn), where));
            data = *(uint32 *)ioBase;
            switch(size) {
                case 1:
                    data = (data & ~(0xff << ((where & 3) << 3))) |
                        (val << ((where & 3) << 3));
                    break;
                case 2:
                    data = (data & ~(0xffff << ((where & 3) << 3))) |
                        (val << ((where & 3) << 3));
                    break;
                case 4:
                    data = val;
                    /* Special case for reading PCI device range */
                    if ((where >= PCI_BASE_ADDRESS_0) && (where <= PCI_BASE_ADDRESS_5)) {
                        if (val == 0xffffffff)
                            pci_mem_size_rd = TRUE;
                    }
                    break;
                default:
                    break;
            }
            *(uint32 *)ioBase = data;
            udelay(500);
            mpi_ClearPciConfigAccess();
        }
    }
#endif
    return PCIBIOS_SUCCESSFUL;
}

struct pci_ops bcm63xx_pci_ops = {
    .read   = bcm63xx_pcibios_read,
    .write  = bcm63xx_pcibios_write
};


#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
static inline u32 CFGOFFSET(u32 bus, u32 devfn, u32 where)
{
    if(bus == BCM_BUS_PCIE_ROOT ) {
        PCIEH_BRIDGE_REGS->bridgeOptReg2 = 0;
        /* access offset 0 */
        return where;
    } else {
        PCIEH_BRIDGE_REGS->bridgeOptReg2 = (PCI_FUNC(devfn)<<PCIE_BRIDGE_OPT_REG2_cfg_type1_func_no_SHIFT)
                                           |(PCI_SLOT(devfn)<<PCIE_BRIDGE_OPT_REG2_cfg_type1_dev_no_SHIFT)
                                           |(bus<<PCIE_BRIDGE_OPT_REG2_cfg_type1_bus_no_SHIFT)
                                           | PCIE_BRIDGE_OPT_REG2_cfg_type1_bd_sel_MASK;
        /* access offset 0x8000*/
        return     PCIEH_DEV_OFFSET|where;
    }
}

#define CFGADDR(bus,devfn,where)   CFGOFFSET((bus)->number,(devfn),where)

static inline u32 READCFG32(u32 addr)
{
    //printk("pcie_reading addr(0x%x) = 0x%x\n", (((u8*)PCIEH) + (addr&~3)),*(u32 *)(((u8*)PCIEH) + (addr&~3)));
    return *(u32 *)(((u8*)PCIEH) + (addr&~3));
}

static inline void WRITECFG32(u32 addr, u32 data)
{
    //printk("pcie_writing addr(0x%x) = 0x%x\n", (((u8*)PCIEH) + (addr & ~3)), data);
    *(u32 *)(((u8*)PCIEH) + (addr & ~3)) = data;
}

static int pci63xx_pcie_can_access(struct pci_bus *bus, int devfn)
{
    u32 devno;

        devno = PCI_SLOT(devfn);

    if (bus->number == BCM_BUS_PCIE_ROOT ) {
        /* bridge */
        return (devno == 0);

    } else {
        /* End Point */
        /* check link up*/
        if(!(PCIEH_BLK_1000_REGS->dlStatus&PCIE_IP_BLK1000_DL_STATUS_PHYLINKUP_MASK))
            return 0;

        return (devno == 0);
    }
}

static int bcm63xx_pciebios_read(struct pci_bus *bus, unsigned int devfn,
    int where, int size, u32 * val)
{
    u32 data = 0;
    //printk("pcie_read, bus=%d, devfn=%d, where=%d, size=%d, val=%d\n", bus->number, devfn, where, size, *val);
    if ((size == 2) && (where & 1)) {
        return PCIBIOS_BAD_REGISTER_NUMBER;
    }
    else if ((size == 4) && (where & 3)) {
        return PCIBIOS_BAD_REGISTER_NUMBER;
    }

    if (pci63xx_pcie_can_access(bus, devfn))
        data = READCFG32(CFGADDR(bus, devfn, where));
    else {
        data = 0xffffffff;
        return PCIBIOS_DEVICE_NOT_FOUND;
    }
    if (size == 1)
        *val = (data >> ((where & 3) << 3)) & 0xff;
    else if (size == 2)
        *val = (data >> ((where & 3) << 3)) & 0xffff;
    else
        *val = data;

    return PCIBIOS_SUCCESSFUL;

}

static int bcm63xx_pciebios_write(struct pci_bus *bus, unsigned int devfn,
    int where, int size, u32 val)
{
    u32 cfgaddr = CFGADDR(bus, devfn, where);
    u32 data = 0;

    //printk("pcie_write, bus=%d, devfn=%d, where=%d, size=%d, val=%d\n", bus->number, devfn, where, size, val);

    if ((size == 2) && (where & 1))
        return PCIBIOS_BAD_REGISTER_NUMBER;
    else if ((size == 4) && (where & 3))
        return PCIBIOS_BAD_REGISTER_NUMBER;

    if (!pci63xx_pcie_can_access(bus, devfn))
        return PCIBIOS_DEVICE_NOT_FOUND;

    data = READCFG32(cfgaddr);

    if (size == 1)
        data = (data & ~(0xff << ((where & 3) << 3))) |
            (val << ((where & 3) << 3));
    else if (size == 2)
        data = (data & ~(0xffff << ((where & 3) << 3))) |
            (val << ((where & 3) << 3));
    else
        data = val;

    WRITECFG32(cfgaddr, data);

    return PCIBIOS_SUCCESSFUL;

}

struct pci_ops bcm63xx_pcie_ops = {
    .read   = bcm63xx_pciebios_read,
    .write  = bcm63xx_pciebios_write
};
#endif
