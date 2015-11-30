
ifeq ($(CONFIG_MIPS_BRCM),y)

ifeq ($(strip $(BRCM_CHIP)),6362)
obj-y +=               \
	bcmHsSpi.o        
else
ifeq ($(strip $(BRCM_CHIP)),6816)
obj-y +=               \
	bcmHsSpi.o        
else
ifeq ($(strip $(BRCM_CHIP)),6328)
obj-y +=               \
	bcmHsSpi.o        
endif
endif
endif

ifneq ($(strip $(BRCM_CHIP)),kilauea)
ifneq ($(strip $(BRCM_CHIP)),3383)
ifneq ($(strip $(BRCM_CHIP)),6328)
obj-y +=               \
    bcmLegSpi.o
endif
else
obj-y +=               \
    bcmHsSpi.o
endif
else
obj-y +=               \
    bcmHsSpi.o
endif

obj-y +=               \
    bcmSpiRes.o 

EXTRA_CFLAGS += -DCONFIG_BCM9$(BRCM_CHIP) -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)

endif

