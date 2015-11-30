# Makefile for the bcm963xx board drivers
#


obj-y := board.o bcm63xx_flash.o 

ifneq ($(strip $(BRCM_CHIP)),3380)
ifneq ($(strip $(BRCM_CHIP)),kilauea)
ifneq ($(strip $(BRCM_CHIP)),3383)
obj-y += bcm63xx_led.o
endif
endif
endif

ifeq ($(strip $(BRCM_CHIP)),6816)
obj-y += spidevices.o
endif

# Uncomment if you want to build the code to support the CPLD used on BCM96358RAVN2G board, supporting Standby functionality.
#CONFIG_BCMCPLD1=y
ifneq ($(strip $(CONFIG_BCMCPLD1)),)
    obj-y += bcmcpld1.o
	EXTRA_CFLAGS += -DCONFIG_BCMCPLD1
endif

EXTRA_CFLAGS += -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)

ifneq ($(strip $(CONFIG_BCM_WLAN)),)
	EXTRA_CFLAGS += -DWIRELESS
endif

-include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o *.a *.s

