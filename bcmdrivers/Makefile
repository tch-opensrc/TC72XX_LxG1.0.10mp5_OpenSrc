# File: bcmdrivers/Makefile
#
# Makefile for the Linux kernel modules.
#

LN_NAME=bcm9$(BRCM_CHIP)
LN_DRIVER_DIRS =
CLEAN_DRIVER_DIRS =

-include $(KERNEL_DIR)/.config


# All kernel drivers are instrumented, when BOUNCE is configured for datapath.
ifdef CONFIG_BRCM_BOUNCE_DATAPATH
ifeq ($(SUBLEVEL),30)
	EXTRA_CFLAGS  += -O0 -finstrument-functions
else
	CFLAGS  += -O0 -finstrument-functions
endif
endif

#ifneq ($(CONFIG_BCM_BOARD),)
#  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_BOARD_IMPL) opensource/char/board/$(BRCM_BOARD)/$(LN_NAME);
#obj-$(CONFIG_BCM_BOARD) += opensource/char/board/$(BRCM_BOARD)/$(LN_NAME)/
#endif

ifneq ($(CONFIG_BCM_SERIAL),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_SERIAL_IMPL) opensource/char/serial/$(LN_NAME);
obj-$(CONFIG_BCM_SERIAL) += opensource/char/serial/$(LN_NAME)/
endif

# Broadcom Packet Flow Forwarding Cache
ifneq ($(CONFIG_BCM_PKTFLOW),)
	LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_PKTFLOW_IMPL) broadcom/char/pktflow/$(LN_NAME);
	obj-$(CONFIG_BCM_PKTFLOW) += broadcom/char/pktflow/$(LN_NAME)/
endif

# Broadcom Packet CMF
ifneq ($(CONFIG_BCM_PKTCMF),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_PKTCMF_IMPL) broadcom/char/pktcmf/$(LN_NAME);
  obj-$(CONFIG_BCM_PKTCMF) += broadcom/char/pktcmf/$(LN_NAME)/
endif

ifeq ($(CONFIG_BCM_PKTCMF_IMPL),2)
  CLEAN_DRIVER_DIRS += broadcom/char/pktcmf/impl$(CONFIG_BCM_PKTCMF_IMPL)
endif

# Broadcom FAP
ifneq ($(CONFIG_BCM_FAP),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_FAP_IMPL) broadcom/char/fap/$(LN_NAME);
  obj-$(CONFIG_BCM_FAP) += broadcom/char/fap/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_PKTDMA),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_PKTDMA_IMPL) opensource/net/lib/pktdma/$(LN_NAME);
  obj-$(CONFIG_BCM_PKTDMA) += opensource/net/lib/pktdma/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_XTMCFG),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_XTMRT_IMPL) opensource/net/xtmrt/$(LN_NAME);
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_XTMCFG_IMPL) broadcom/char/xtmcfg/$(LN_NAME);
  obj-y += opensource/net/xtmrt/$(LN_NAME)/
  obj-$(CONFIG_BCM_XTMCFG) += broadcom/char/xtmcfg/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_SMUX),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_SMUX_IMPL) opensource/net/bcmsmux/$(LN_NAME);
  obj-$(CONFIG_BCM_SMUX) += opensource/net/bcmsmux/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_ADSL),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_ADSL_IMPL) broadcom/char/adsl/$(LN_NAME);
  obj-$(CONFIG_BCM_ADSL) += broadcom/char/adsl/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_PROCFS),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_PROCFS_IMPL) broadcom/char/bcmprocfs/$(LN_NAME);
  obj-$(CONFIG_BCM_PROCFS) += broadcom/char/bcmprocfs/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_ENDPOINT),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_ENDPOINT_IMPL) broadcom/char/endpoint/$(LN_NAME);
  obj-$(CONFIG_BCM_ENDPOINT) += broadcom/char/endpoint/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_BCMPROF),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_BCMPROF_IMPL) broadcom/char/profiler/$(LN_NAME);
  obj-$(CONFIG_BCM_BCMPROF) += broadcom/char/profiler/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_BCMDSP),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_BCMDSP_IMPL) broadcom/char/dspapp/$(LN_NAME);
  obj-$(CONFIG_BCM_BCMDSP) += broadcom/char/dspapp/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_DECT),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_DECT_IMPL) broadcom/char/dect/$(LN_NAME);
  obj-$(CONFIG_BCM_DECT) += broadcom/char/dect/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_P8021AG),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_P8021AG_IMPL) broadcom/char/p8021ag/$(LN_NAME);
  obj-$(CONFIG_BCM_P8021AG) += broadcom/char/p8021ag/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_ENET),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_ENET_IMPL) broadcom/net/enet/$(LN_NAME);
  obj-$(CONFIG_BCM_ENET) += broadcom/net/enet/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_GPON),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_GPON_IMPL) broadcom/char/gpon/$(LN_NAME);
  obj-$(CONFIG_BCM_GPON) += broadcom/char/gpon/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_MoCA),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_MoCA_IMPL) broadcom/char/moca/$(LN_NAME);
  obj-$(CONFIG_BCM_MoCA) += broadcom/char/moca/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_LOG),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_LOG_IMPL) broadcom/char/bcmlog/$(LN_NAME);
  obj-$(CONFIG_BCM_LOG) += broadcom/char/bcmlog/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_I2C_BUS),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_I2C_BUS_IMPL) broadcom/char/i2c/busses/$(LN_NAME);
  obj-$(CONFIG_BCM_I2C_BUS) += broadcom/char/i2c/busses/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_I2C_CHIP),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_I2C_CHIP_IMPL) broadcom/char/i2c/chips/$(LN_NAME);
  obj-$(CONFIG_BCM_I2C_CHIP) += broadcom/char/i2c/chips/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_VLAN),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_VLAN_IMPL) broadcom/char/vlan/$(LN_NAME);
  obj-$(CONFIG_BCM_VLAN) += broadcom/char/vlan/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_USB),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_USB_IMPL) broadcom/net/usb/$(LN_NAME);
  obj-$(CONFIG_BCM_USB) += broadcom/net/usb/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_ISDN),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_ISDN_IMPL) broadcom/char/isdn/$(LN_NAME);
  obj-$(CONFIG_BCM_ISDN) += broadcom/char/isdn/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_WLAN),)
ifneq ($(WL),)
   CONFIG_BCM_WLAN_IMPL=$(patsubst IMPL%,%,$(WL))  
endif
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_WLAN_IMPL) broadcom/net/wl/$(LN_NAME);
  obj-$(CONFIG_BCM_WLAN) += broadcom/net/wl/$(LN_NAME)/
  
endif

ifneq ($(CONFIG_BCM_SPU),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_SPU_IMPL) broadcom/char/spudd/$(LN_NAME);
  obj-$(CONFIG_BCM_SPU) += broadcom/char/spudd/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_PWRMNGT),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_PWRMNGT_IMPL) broadcom/char/pwrmngt/$(LN_NAME);
  obj-$(CONFIG_BCM_PWRMNGT) += broadcom/char/pwrmngt/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_TRNG),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_TRNG_IMPL) opensource/char/trng/$(LN_NAME);
  obj-$(CONFIG_BCM_TRNG) += opensource/char/trng/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_IOP),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_IOP_IMPL) broadcom/net/iop/$(LN_NAME);
  obj-$(CONFIG_BCM_IOP) += broadcom/net/iop/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_DQM),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_DQM_IMPL) broadcom/net/dqm/$(LN_NAME);
  obj-$(CONFIG_BCM_DQM) += broadcom/net/dqm/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_FPM),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_FPM_IMPL) broadcom/net/fpm/$(LN_NAME);
  obj-$(CONFIG_BCM_FPM) += broadcom/net/fpm/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_VENET),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_VENET_IMPL) broadcom/net/venet/$(LN_NAME);
  obj-$(CONFIG_BCM_VENET) += broadcom/net/venet/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_VNIHAL),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_VNIHAL_IMPL) broadcom/net/vnihal/$(LN_NAME);
  obj-$(CONFIG_BCM_VNIHAL) += broadcom/net/vnihal/$(LN_NAME)/
endif

ifneq ($(CONFIG_BCM_USB_HOST),)
  LN_DRIVER_DIRS +=ln -sn impl$(CONFIG_BCM_USB_HOST_IMPL) broadcom/usb/host/$(LN_NAME);
  obj-$(CONFIG_BCM_USB_HOST) += broadcom/usb/host/$(LN_NAME)/
endif


.PHONY : applinks symlinks clean

applinks:
ifneq ($(CONFIG_BCM_PKTCMF),)
ifeq ($(CONFIG_BCM_PKTCMF_IMPL),2)
	$(MAKE) -C $(BRCMDRIVERS_DIR)/broadcom/char/pktcmf/impl$(CONFIG_BCM_PKTCMF_IMPL) applinks
endif
endif


symlinks: applinks
	find . -lname "*" -name "$(LN_NAME)" -print -exec rm -f "{}" ";"
	$(CONFIG_SHELL) -c "$(LN_DRIVER_DIRS)"


clean:
	for dir in $(CLEAN_DRIVER_DIRS) ; do \
		$(MAKE) -C $$dir clean; \
	done
