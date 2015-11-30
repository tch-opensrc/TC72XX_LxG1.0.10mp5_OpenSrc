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

/* force non-byteswapping reads/writes on LE and BE alike */
#define CONFIG_USB_EHCI_BIG_ENDIAN_MMIO		1
#define CONFIG_USB_EHCI_BIG_ENDIAN_DESC		1

#define readl_be(x)				__raw_readl(x)
#define writel_be(x, y)				__raw_writel(x, y)

#include <linux/platform_device.h>
#include <linux/autoconf.h>
#include <linux/mmdebug.h>
#include <linux/usb.h>
#include <hcd.h>


/* stock Linux drivers assume USB is always PCI-based on platforms with PCI */
#undef CONFIG_PCI

static struct platform_driver	ehci_hcd_brcm_driver;
#define PLATFORM_DRIVER		ehci_hcd_brcm_driver

#include "ehci-hcd.c"

/* for global USB settings, see arch/mips/brcmstb/bchip.c */

static int ehci_brcm_reset (struct usb_hcd *hcd)
{
	struct ehci_hcd		*ehci = hcd_to_ehci (hcd);
	int ret;

	ehci->big_endian_mmio = 1;
	ehci->big_endian_desc = 1;

	ehci->caps = (struct ehci_caps *) hcd->regs;
	ehci->regs = (struct ehci_regs *) (hcd->regs + 
		HC_LENGTH (ehci_readl(ehci, &ehci->caps->hc_capbase)));
	dbg_hcs_params (ehci, "reset");
	dbg_hcc_params (ehci, "reset");

	/* cache this readonly data; minimize PCI reads */
	ehci->hcs_params = ehci_readl(ehci, &ehci->caps->hcs_params);

	/* This fixes the lockup during reboot due to prior interrupts */
	ehci_writel(ehci, CMD_RESET, &ehci->regs->command);
	mdelay(10);

	/* force HC to halt state */
	ehci_halt(ehci);

	ret = ehci_init(hcd);
	if(ret)
		return(ret);

	ehci_port_power(ehci, 1);
	return(ret);
}

/*-------------------------------------------------------------------------*/

static const struct hc_driver ehci_brcm_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"Broadcom STB EHCI",
	.hcd_priv_size =	sizeof(struct ehci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			ehci_irq,
	.flags =		HCD_USB2 | HCD_MEMORY,

	/*
	 * basic lifecycle operations
	 */
	.reset =		ehci_brcm_reset,
	.start =		ehci_run,
	.stop =			ehci_stop,
	.shutdown =		ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		ehci_urb_enqueue,
	.urb_dequeue =		ehci_urb_dequeue,
	.endpoint_disable =	ehci_endpoint_disable,
	/* .endpoint_reset =	ehci_endpoint_reset,  */

	/*
	 * scheduling support
	 */
	.get_frame_number =	ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data =	ehci_hub_status_data,
	.hub_control =		ehci_hub_control,
#ifdef	CONFIG_PM
	.bus_suspend = ehci_bus_suspend,
	.bus_resume = ehci_bus_resume,
#endif
	.relinquish_port =	ehci_relinquish_port,
	.port_handed_over =	ehci_port_handed_over,
	/* .clear_tt_buffer_complete = ehci_clear_tt_buffer_complete, */
};

/*-------------------------------------------------------------------------*/

#define resource_len(r) (((r)->end - (r)->start) + 1)
static int ehci_hcd_brcm_probe(struct platform_device *pdev)
{
	struct resource *res = NULL;
	struct usb_hcd *hcd = NULL;
	int irq = -1;
	int ret;

	if (usb_disabled())
	    return -ENODEV;

#ifdef CONFIG_BRCM_PM
	brcm_pm_usb_add();
#endif

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err("platform_get_resource error.");
		return -ENODEV;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		err("platform_get_irq error.");
		return -ENODEV;
	}

	/* initialize hcd */
	hcd = usb_create_hcd(&ehci_brcm_hc_driver, &pdev->dev, (char *)hcd_name);
	if (!hcd) {
		err("Failed to create hcd");
		return -ENOMEM;
	}

	hcd->regs = ioremap_nocache(res->start, res->end - res->start); 
	/* hcd->regs = res->start; */
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_len(res);
	ret = usb_add_hcd(hcd, irq, IRQF_DISABLED);
	if (ret != 0) {
		err("Failed to add hcd");
		iounmap(hcd->regs);
		usb_put_hcd(hcd);
		return ret;
	}
	return ret;
}

static int ehci_hcd_brcm_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	usb_put_hcd(hcd);

	return 0;
}

static struct platform_driver ehci_hcd_brcm_driver = {
	.probe = ehci_hcd_brcm_probe,
	.remove = ehci_hcd_brcm_remove,
	.shutdown = usb_hcd_platform_shutdown,
	.driver = {
		.name = "EHCI-brcm",
		.bus = &platform_bus_type
	}
};

MODULE_ALIAS("ehci-brcm");
