#
# Makefile for generic Broadcom MIPS boards
#
# Copyright (C) 2004 Broadcom Corporation
#
#obj-y           := irq.o prom.o setup.o bcm63xx_flash.o bcm63xx_led.o flash_api.o boardparms.o board.o bcm63xx_cons.o spiflash.o
obj-$(CONFIG_BRCM_BOUNCE) += bounce.o
obj-$(CONFIG_BALOO) += baloo_drv.o baloo_log.o
obj-$(CONFIG_PMON) += pmontool.o

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

