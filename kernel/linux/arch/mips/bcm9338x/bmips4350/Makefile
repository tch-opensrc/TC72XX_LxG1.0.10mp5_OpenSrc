#
# Makefile for generic Broadcom MIPS boards
#
# Copyright (C) 2012 Broadcom Corporation
#

obj-y           := irq.o prom.o setup.o memory.o

SRCBASE         := $(TOPDIR)
EXTRA_CFLAGS    += -I$(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD) -I$(SRCBASE)/include -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
