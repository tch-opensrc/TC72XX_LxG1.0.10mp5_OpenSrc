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

#include <bcmpci.h>
#include <bcm_map_part.h>

extern struct pci_ops bcm63xx_pci_ops;
static struct resource bcm_pci_io_resource = {
    .name   = "bcm63xx pci IO space",
    .start  = BCM_PCI_IO_BASE,
    .end    = BCM_PCI_IO_BASE + BCM_PCI_IO_SIZE - 1,
    .flags  = IORESOURCE_IO
};

static struct resource bcm_pci_mem_resource = {
    .name   = "bcm63xx pci memory space",
    .start  = BCM_PCI_MEM_BASE,
    .end    = BCM_PCI_MEM_BASE + BCM_PCI_MEM_SIZE - 1,
    .flags  = IORESOURCE_MEM
};

struct pci_controller bcm63xx_controller = {
    .pci_ops	= &bcm63xx_pci_ops,
    .io_resource	= &bcm_pci_io_resource,
    .mem_resource	= &bcm_pci_mem_resource,
};

#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
extern struct pci_ops bcm63xx_pcie_ops;
static struct resource bcm_pcie_io_resource = {
    .name   = "bcm63xx pcie null io space",
    .start  = 0,
    .end    = 0,
    .flags  = 0
};

static struct resource bcm_pcie_mem_resource = {
    .name   = "bcm63xx pcie memory space",
    .start  = BCM_PCIE_MEM1_BASE,
    .end    = BCM_PCIE_MEM1_BASE + BCM_PCIE_MEM1_SIZE - 1,
    .flags  = IORESOURCE_MEM
};

struct pci_controller bcm63xx_pcie_controller = {
    .pci_ops	= &bcm63xx_pcie_ops,
    .io_resource	= &bcm_pcie_io_resource,
    .mem_resource	= &bcm_pcie_mem_resource,
};

#endif

static int __init bcm63xx_pci_init(void)
{
    /* adjust global io port range */
    ioport_resource.start = BCM_PCI_IO_BASE;
    ioport_resource.end = BCM_PCI_IO_BASE + BCM_PCI_IO_SIZE-1;

    /* bus 0 */
    register_pci_controller(&bcm63xx_controller);

#if defined(CONFIG_BCM96816)
    PCIEH_BRIDGE_REGS->bridgeOptReg1 |= (PCIE_BRIDGE_OPT_REG1_EN_RD_BE | PCIE_BRIDGE_OPT_REG1_EN_RD_BE_NOSWAP);
#endif

#if defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
    PCIEH_BRIDGE_REGS->bridgeOptReg1 |= (
            PCIE_BRIDGE_OPT_REG1_en_l1_int_status_mask_polarity |
            PCIE_BRIDGE_OPT_REG1_en_pcie_bridge_hole_detection  |
            PCIE_BRIDGE_OPT_REG1_en_rd_reply_be_fix |
            PCIE_BRIDGE_OPT_REG1_enable_rd_be_opt);
    
    PCIEH_BRIDGE_REGS->rcInterruptMask |= (
    	PCIE_BRIDGE_INTERRUPT_MASK_int_a_MASK |
        PCIE_BRIDGE_INTERRUPT_MASK_int_b_MASK |
        PCIE_BRIDGE_INTERRUPT_MASK_int_c_MASK |
        PCIE_BRIDGE_INTERRUPT_MASK_int_c_MASK );
#endif

#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)    
    /* setup class code, as bridge */
    PCIEH_BLK_428_REGS->idVal3 &= ~PCIE_IP_BLK428_ID_VAL3_CLASS_CODE_MASK;
    PCIEH_BLK_428_REGS->idVal3 |= (PCI_CLASS_BRIDGE_PCI << 8);	
    /* disable bar0 size */
    PCIEH_BLK_404_REGS->config2 &= ~PCIE_IP_BLK404_CONFIG_2_BAR1_SIZE_MASK;
    /*bus 1 and 2 */                        
    register_pci_controller(&bcm63xx_pcie_controller);
#endif
    return 0;
}

arch_initcall(bcm63xx_pci_init);
