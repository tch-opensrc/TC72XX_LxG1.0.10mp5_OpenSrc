# File: bcmdrivers/broadcom/net/usb/Makefile
#
# Makefile for the BCM63xx USB driver
#

###############################################################
# OHCI+EHCI combo is broken on 3384A1 chip, will be fixed in B0
# obj-$(CONFIG_BCM_USB_HOST) = ehci-brcm.o ohci-brcm.o
obj-$(CONFIG_BCM_USB_HOST) = ehci-brcm.o
#bcm_usb_host-objs := bcm-hcd.o bcm-hub.o bcm-queue.o

EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/usb/core
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/usb/host
EXTRA_CFLAGS += -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -I$(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/bcm3380
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/bcm3380/ioproc/iop_common/include
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/bcm3380/ioproc/dtp 
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/bcm3380/ioproc/fap
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/bcm3380/ioproc/mpeg_proc
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/bcm3380/ioproc/msg_proc
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/bcm3380/ioproc/utp
EXTRA_CFLAGS += -DUSE_BH

-include $(TOPDIR)/Rules.make
