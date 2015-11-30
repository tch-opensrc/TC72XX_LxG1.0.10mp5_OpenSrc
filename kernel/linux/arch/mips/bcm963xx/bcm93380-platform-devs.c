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
    
#define USB_HOST 0xb5000000   // USB OTG block 0 registers
#define USB_HOST_SIZE (0x15040e04 - 0x15000000)

static struct resource bcm93380_usb_resources[] = {
	[0] = {
		.start		= USB_HOST,
		.end		= USB_HOST + USB_HOST_SIZE, 
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device bcm3380_usb_host = {
	.name		= "bcm3380-hcd-device",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(bcm93380_usb_resources),
	.resource	= bcm93380_usb_resources,
};

EXPORT_SYMBOL(bcm3380_usb_host);

static struct platform_device *bcm93380_platform_devices[] __initdata = {
	&bcm3380_usb_host,
};

int bcm93380_platform_init(void)
{
	return platform_add_devices(bcm93380_platform_devices, ARRAY_SIZE(bcm93380_platform_devices));
}

arch_initcall(bcm93380_platform_init);
