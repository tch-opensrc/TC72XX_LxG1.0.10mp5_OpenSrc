
ifeq ($(CONFIG_MIPS_BRCM),y)

# Linux
# obj-y		+= boardparms.o
EXTRA_CFLAGS += -DCONFIG_BCM9$(BRCM_CHIP) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
-include $(TOPDIR)/Rules.make

endif


