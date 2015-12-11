# File: bcmdrivers/broadcom/net/dqm/impl1/Makefile
#
# Makefile for the BCM388x DQM driver
#

ifneq ($(strip $(CONFIG_BCM_DQM)),)
obj-$(CONFIG_BCM_DQM) = $(BRCM_CHIP)/dqm.o
endif
dqm-objs := $(BRCM_CHIP)/bcmdqm.o

EXTRA_CFLAGS += -I$(obj) 
EXTRA_CFLAGS += -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -I$(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)
ifneq ($(strip $(CONFIG_BCM93380)),)
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/bcm3380
dqm-objs += platform/dqm_3380.o
endif
ifneq ($(strip $(CONFIG_BCM93383)),)
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/bcm3383
dqm-objs += platform/dqm_3383.o
endif
ifneq ($(strip $(CONFIG_BCM3384)),)
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/bcm3384
dqm-$(CONFIG_BMIPS5000) += platform/dqm_3384.o
dqm-$(CONFIG_BMIPS4350) += platform/dqm_3384tp1.o
endif
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/ioproc/iop_common/include
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/ioproc/dtp 
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/ioproc/fap
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/ioproc/mpeg_proc
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/ioproc/msg_proc
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)/ioproc/utp


-include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o *.s
