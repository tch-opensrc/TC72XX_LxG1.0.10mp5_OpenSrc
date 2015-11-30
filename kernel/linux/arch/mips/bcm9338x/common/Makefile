#
# Makefile for generic Broadcom MIPS boards
#
# Copyright (C) 2012 Broadcom Corporation
#

obj-y           := irq.o prom.o setup.o
obj-$(CONFIG_SMP)	+= smp-brcm.o
obj-$(CONFIG_PM)	+= power.o

ifeq ($(strip $(CONFIG_BCM93380)),y)
obj-$(CONFIG_BCM_PLATFORM_DEVS)  += bcm93380-platform-devs.o
endif

ifeq ($(strip $(CONFIG_BCM93383)),y)
obj-$(CONFIG_BCM_PLATFORM_DEVS)  += bcm93383-platform-devs.o
endif

ifneq ($(strip $(CONFIG_BCM_PWRMNGT)),)
obj-y += pwrmngtclk.o        
endif

SRCBASE         := $(TOPDIR)
EXTRA_CFLAGS    += -I$(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD) -I$(SRCBASE)/include -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
#EXTRA_CFLAGS    += -I$(INC_ADSLDRV_PATH) -DDBG
EXTRA_CFLAGS    += -I$(INC_ADSLDRV_PATH) 
cflags-$(CONFIG_MIPS_BRCM)      += -I$(srctree)/arch/mips/include/asm/bcm963xx
EXTRA_CFLAGS += -g


ifeq "$(ADSL)" "ANNEX_B"
EXTRA_CFLAGS += -DADSL_ANNEXB
endif
ifeq "$(ADSL)" "SADSL"
EXTRA_CFLAGS += -DADSL_SADSL
endif
ifeq "$(ADSL)" "ANNEX_C"
EXTRA_CFLAGS += -DADSL_ANNEXC
endif

