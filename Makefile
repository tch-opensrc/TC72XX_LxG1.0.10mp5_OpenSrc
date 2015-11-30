#****************************************************************************
#
#  Copyright (c) 2001, 2002, 2003, 2004  Broadcom Corporation
#  All Rights Reserved
#  No portions of this material may be reproduced in any form without the
#  written permission of:
#          Broadcom Corporation
#          16251 Laguna Canyon Road
#          Irvine, California 92618
#  All information contained in this document is Broadcom Corporation
#  company private, proprietary, and trade secret.
#
#****************************************************************************

# Top-level Makefile for all commengine xDSL platforms

LIBOPT=n

###########################################
#
# This is the most important target: make all
# This is the first target in the Makefile, so it is also the default target.
# All the other targets are later in this file.
#
############################################

ifeq ($(strip $(INITRD)), y)
all: initrd
else
all: make_version_check kernel_version_check all_postcheck1
endif

all_postcheck1: sanity_check \
     create_install kernelbuild modbuild extmodbuild kernelbuildlite switchsdkbuild\
     userspace  gdbserver switchlibs vodsl hosttools buildimage 

############################################################################
# initrd (initramfs) build
############################################################################
initrd: sanity_check \
	create_install non_initrd_kernelbuild extmodbuild kernelbuildlite \
	userspace hosttools libcreduction initrd_kernelbuild initrd_buildimage

initrd_final: initrd_kernelbuild initrd_buildimage

############################################################################
#
# A lot of the stuff in the original Makefile has been moved over
# to make.common.
#
############################################################################
BUILD_DIR = $(shell pwd)
include $(BUILD_DIR)/make.common

############################################################################
#
# Make info for voice has been moved over to make.voice
#
############################################################################
ifneq ($(strip $(BUILD_VODSL)),)
include $(BUILD_DIR)/make.voice
endif

############################################################################
#
# Make info for FAP is stored in make.fap
#
############################################################################
ifeq ($(strip $(BRCM_CHIP)),6362)
ifeq ($(strip $(BRCM_DRIVER_FAP)),m)
include $(BUILD_DIR)/make.fap
endif
endif

############################################################################
#
# dsl, kernel defines
#
############################################################################

ifeq ($(strip $(BRCM_KERNEL_DEBUG)),y) 
KERNEL_DEBUG=1
endif

ifeq ($(strip $(BRCM_KERNEL_OPROFILE)),y) 
	export CONFIG_DEBUG_INFO=1
	export CONFIG_FRAME_POINTER=1
endif

# Default values for DECT
DECTMODE := $(DECT)
export CONFIG_BCM_DECTLIB=0
ifeq ($(strip $(DECT)),)
DECTMODE := 0
export CONFIG_BCM_DECTLIB=0
endif
ifeq ($(strip $(DECT)),intlib)
DECTMODE := int
export CONFIG_BCM_DECTLIB=1
endif

#Set up PC DSP image type and name
ifneq ($(strip $(PCTYPE)),)
	ifeq ($(strip $(PCTYPE)),15) 
		export CONFIG_BCM_BCMDSP=m
		export CONFIG_BCM_BCMDSP_IMPL=1
		export CONFIG_BCM_PCTYPE=pc15

		ifeq ($(strip $(DECTMODE)),0)
			export CONFIG_BCM_CODECTYPE=apm
		else
$(error "PCTYPE=15 does not support DECT=ext or DECT=int")
		endif

		ifeq ($(strip $(BRCM_CHIP)), 3380)
			PCIMAGE_VALID_NAMES := bv16_ilbc_faxr bv16_ilbc_g729abe_faxr g711_faxr g729abe_faxr
			PCIMAGE_DEFAULT := bv16_ilbc_faxr
		endif
		ifeq ($(strip $(BRCM_CHIP)), 3383)
			PCIMAGE_VALID_NAMES := bv16_ilbc_faxr all_codecs all_codecs_wb bv32_g722_amrwb_faxr_wb
			PCIMAGE_DEFAULT := bv16_ilbc_faxr
		endif
		ifeq ($(strip $(BRCM_CHIP)), 3384)
$(error "There is no DSP support for 3384 with PCTYPE=15")
		endif
	endif

	ifeq ($(strip $(PCTYPE)),20) 
		export CONFIG_BCM_BCMDSP=m
		export CONFIG_BCM_BCMDSP_IMPL=1
		export CONFIG_BCM_PCTYPE=pc20
		ifeq ($(strip $(DECTMODE)),0)
			export CONFIG_BCM_CODECTYPE=apm
		else
			ifeq ($(strip $(DECTMODE)),ext)
				export CONFIG_BCM_CODECTYPE=apm_pcm
			else
				ifeq ($(strip $(DECTMODE)),int)
					export CONFIG_BCM_CODECTYPE=apm_dect
				else
$(error "Valid DECT options are '0', 'ext', and 'int'")
                endif
			endif
		endif
		ifeq ($(strip $(BRCM_CHIP)), 3380)
$(error "There is no DSP support for 3380 with PCTYPE=20")
			PCIMAGE_VALID_NAMES := NONE
			PCIMAGE_DEFAULT := NODEFAULT
		endif
		ifeq ($(strip $(BRCM_CHIP)), 3383)
			ifeq ($(strip $(DECTMODE)),int)
$(error "There is no DSP support for 3383 with PCTYPE=20 DECT=int")
			else
				ifeq ($(strip $(DECTMODE)),ext)
# 3383 DECT=ext
					PCIMAGE_VALID_NAMES := g711_faxr all_codecs_wb bv16_ilbc_g728_g729e_amrwb_faxr_wb bv16_ilbc_g728_g729e_faxr bv32_g722_amrwb_faxr_wb comcast_wb super super_wb g722_faxr_wb all_codecs
					PCIMAGE_DEFAULT := g722_faxr_wb
				else
# 3383 DECT=0
					PCIMAGE_VALID_NAMES := g711_faxr all_codecs_wb bv16_ilbc_g728_g729e_amrwb_faxr_wb bv16_ilbc_g728_g729e_faxr bv32_g722_amrwb_faxr_wb comcast_wb super super_wb g722_faxr_wb all_codecs 
					PCIMAGE_DEFAULT := g711_faxr
				endif
			endif
		endif
		ifeq ($(strip $(BRCM_CHIP)), 3384)
		   ifeq ($(strip $(DECTMODE)),int)
				PCIMAGE_VALID_NAMES := g711_faxr all_codecs_wb bv16_ilbc_g728_g729e_amrwb_faxr_wb bv16_ilbc_g728_g729e_faxr bv32_g722_amrwb_faxr_wb comcast_wb super super_wb g722_faxr_wb all_codecs 
				PCIMAGE_DEFAULT := g722_faxr_wb
			else
				ifeq ($(strip $(DECTMODE)),ext)
$(error "There is no DSP support for 3384 with PCTYPE=20 DECT=ext")
				else
$(error "There is no DSP support for 3384 with PCTYPE=20 DECT=0")
				endif
			endif
		endif
	endif

	ifneq ($(strip $(CONFIG_BCM_BCMDSP)),m)
$(error "Invalid PCTYPE")
	endif
	ifeq ($(strip $(CONFIG_BCM_BCMDSP_IMPL)),)
$(error "CONFIG_BCM_BCMDSP_IMPL undefined")
	endif
	ifeq ($(PCIMAGE),)
		PCIMAGE=$(PCIMAGE_DEFAULT)
	endif
	ifneq ($(filter-out $(PCIMAGE_VALID_NAMES),$(PCIMAGE)),)
$(error "PCIMAGE not supported for this platform. Refer to release notes for list of supported apps.")
	endif
	export CONFIG_BCM_PCIMAGE=$(PCIMAGE)
endif

ifneq ($(PCIMAGE),)
	export BRCM_DRIVER_BCMDSP=m
	DSP_TARGET_BUILD := dspmodbuild
	DSP_TARGET_CLEAN := dspmodclean
endif

ifeq ($(strip $(DECTMODE)),int)
    DECT_TARGET_BUILD := dectmodbuild dectshimmodbuild
    DECT_TARGET_CLEAN := dectmodclean dectshimmodclean
endif

ifeq ($(strip $(PROFILE)),3384)
	ifneq ($(strip $(DECTMODE)),0)
$(error "PROFILE=3384 Full Linux on Zephyr Profile does not support DECT")
	endif
	ifneq ($(strip $(PCTYPE)),)
$(error "PROFILE=3384 Full Linux on Zephyr Profile does not support PCTYPE")
	endif
endif

#Set up ADSL standard
export ADSL=$(BRCM_ADSL_STANDARD)

#Set up ADSL_PHY_MODE  {file | obj}
export ADSL_PHY_MODE=file

#Set up ADSL_SELF_TEST
export ADSL_SELF_TEST=$(BRCM_ADSL_SELF_TEST)

#Set up ADSL_PLN_TEST
export ADSL_PLN_TEST=$(BUILD_TR69_XBRCM)

#WL command
ifneq ($(strip $(WL)),)
build_nop:=$(shell cd $(BUILD_DIR)/userspace/private/apps/wlan; make clean; cd $(BUILD_DIR))
export WL
endif

ifneq ($(strip $(BRCM_DRIVER_WIRELESS_USBAP)),)
    WLBUS ?= "usbpci"
endif
#default WLBUS for wlan pci driver
WLBUS ?="pci"
export WLBUS

ifeq ($(strip $(BRCM_CHIP)), 3380)
KERNEL_LOAD_ADDRESS=0x84010000
endif
ifeq ($(strip $(BRCM_CHIP)), 3383)
KERNEL_LOAD_ADDRESS=0x84010000
endif
ifeq ($(strip $(BRCM_CHIP)), 3384)
ifeq ($(strip $(BRCM_CPU)), TP1)
# previous kernel load address when image load directly from the bootloader.
#KERNEL_LOAD_ADDRESS=0x87001000
# kernal load address for dual_sto image.
KERNEL_LOAD_ADDRESS=0x87000000
else
KERNEL_LOAD_ADDRESS=0x00000000
endif
endif
ifeq ($(strip $(BRCM_CHIP)), 3385)
KERNEL_LOAD_ADDRESS=0x81701000
endif

#default PID
ifeq ($(strip $(PID)),)
PID=$(BRCM_CHIP)
endif

BRCM_LINUX_CONFIG_FILE=$(KERNEL_DIR)/arch/mips/defconfig.$(BRCM_CHIP)$(BRCM_CPU)$(BRCM_APP)

############################################################################
#
# When there is a directory name with the same name as a Make target,
# make gets confused.  PHONY tells Make to ignore the directory when
# trying to make these targets.
#
############################################################################
.PHONY: userspace unittests hostTools



############################################################################
#
# Other Targets. The default target is "all", defined at the top of the file.
#
############################################################################

userspace: sanity_check create_install 
	$(MAKE) -C userspace

switchlibs:
ifneq ($(strip $(BRCM_DRIVER_SWITCHSDK)),)
	$(MAKE) -f $(SDK)/Makefile.libinst
endif

#
# libcreduction analyzes the user applications and libraries and eliminates
# unused functions from the libraries, thus reducing image size.
#
ifneq ($(strip $(BUILD_LIBCREDUCTION)),)
libcreduction:
	$(MAKE) -C hostTools/libcreduction install
else
libcreduction:
	@echo "skipping libcreduction (not configured)"
endif


menuconfig:
	@cd $(INC_KERNEL_BASE); \
	if [ ! -e linux/CREDITS ]; then \
	  echo Untarring original Linux kernel source...; \
	  (tar xkfj $(ORIGINAL_KERNEL) 2> /dev/null || true); \
	fi
	$(MAKE) -C $(HOSTTOOLS_DIR)/scripts/lxdialog HOSTCC=gcc
	$(CONFIG_SHELL) $(HOSTTOOLS_DIR)/scripts/Menuconfig $(TARGETS_DIR)/config.in


#
# the userspace apps and libs make their own directories before
# they install, so they don't depend on this target to make the
# directory for them anymore.
#
create_install:
		mkdir -p $(PROFILE_DIR)/fs.install/etc
		mkdir -p $(PROFILE_DIR)/stage/usr/local
		mkdir -p $(INSTALL_DIR)/bin
		mkdir -p $(INSTALL_DIR)/lib

		# nimah - create fs.apps file structure tree
		mkdir -p $(PROFILE_DIR)/fs.apps/usr/local/bin
		mkdir -p $(PROFILE_DIR)/fs.apps/usr/local/etc
		mkdir -p $(PROFILE_DIR)/fs.apps/usr/local/etc/init.d
		mkdir -p $(PROFILE_DIR)/fs.apps/usr/local/etc/sysconfig
		mkdir -p $(PROFILE_DIR)/fs.apps/usr/local/lib
		mkdir -p $(PROFILE_DIR)/fs.apps/usr/local/sbin
		mkdir -p $(PROFILE_DIR)/fs.apps/usr/local/srv
		mkdir -p $(PROFILE_DIR)/fs.apps/usr/local/ssl



$(KERNEL_DIR)/vmlinux:
	$(GENDEFCONFIG_CMD) $(PROFILE_PATH) ${MAKEFLAGS}
	cd $(KERNEL_DIR); \
	cp -f $(BRCM_LINUX_CONFIG_FILE) $(KERNEL_DIR)/.config; \
	$(MAKE) oldconfig; $(MAKE); $(MAKE) modules_install

switchsdkbuild:
ifneq ($(strip $(BRCM_DRIVER_SWITCHSDK)),)
	cd $(SDK_USERBLD_DIR); $(MAKE)
endif


# If the target file wants to use a different kernel defconfig input file,
# then it would specify a value for the DEFCONFIG_EXT variable. If the DEFCONFIG_EXT
# var is found then append its value to the defconfig file name. Otherwise use the
# chip id.
ifneq ($(strip $(DEFCONFIG_EXT)),)
 CONF_EXT=$(DEFCONFIG_EXT)
 $(info ++ A default kernel config file extension [$(CONF_EXT)] is specified, use it)
else
 CONF_EXT = $(BRCM_CHIP)
endif

kernelbuild:
ifeq ($(wildcard $(KERNEL_DIR)/vmlinux),)
	echo starting kernel build at $(INC_KERNEL_BASE)
	@cd $(INC_KERNEL_BASE); \
	if [ ! -e linux/CREDITS ]; then \
	  echo Untarring original Linux kernel source...; \
	  (tar xkfj $(ORIGINAL_KERNEL) 2> /dev/null || true); \
	fi
	$(GENDEFCONFIG_CMD) $(PROFILE_PATH) ${MAKEFLAGS}
	cd $(KERNEL_DIR); \
	cp -f $(BRCM_LINUX_CONFIG_FILE) $(KERNEL_DIR)/.config; \
	echo "#define FS_KERNEL_IMAGE_NAME \"$(FS_KERNEL_IMAGE_NAME)_kernel\"" > $(KERNEL_DIR)/include/linux/linux_build_info.h; \
	echo "#define MAKEFLAGS \"$(MAKEFLAGS)\"" >> $(KERNEL_DIR)/include/linux/linux_build_info.h; \
	$(MAKE) oldconfig; $(MAKE)
else
	echo "#define FS_KERNEL_IMAGE_NAME \"$(FS_KERNEL_IMAGE_NAME)_kernel\"" > $(KERNEL_DIR)/include/linux/linux_build_info.h; \
	echo "#define MAKEFLAGS \"$(MAKEFLAGS)\"" >> $(KERNEL_DIR)/include/linux/linux_build_info.h; \
	cd $(KERNEL_DIR); $(MAKE)
endif

non_initrd_kernelbuild:
ifeq ($(wildcard $(TARGET_FS)),)
	cd $(KERNEL_DIR); \
	cp -f $(BRCM_LINUX_CONFIG_FILE) $(KERNEL_DIR)/.config; \
	echo "#define FS_KERNEL_IMAGE_NAME \"$(FS_KERNEL_IMAGE_NAME)_kernel\"" > $(KERNEL_DIR)/include/linux/linux_build_info.h; \
	echo "#define MAKEFLAGS \"$(MAKEFLAGS)\"" >> $(KERNEL_DIR)/include/linux/linux_build_info.h; \
	$(MAKE) oldconfig; $(MAKE)
endif


initrd_kernelbuild: 
	export FSSRC_DIR=nothing; \
	cd $(TARGETS_DIR); ./buildFS; \
	export BRCM_KERNEL_ROOTFS=nothing; \
	$(HOSTTOOLS_DIR)/fakeroot/fakeroot ./buildFS2; \
	cd $(KERNEL_DIR); \
	cp -f $(BRCM_LINUX_CONFIG_FILE)_initrd $(KERNEL_DIR)/.config; \
	echo "#define FS_KERNEL_IMAGE_NAME \"$(FS_KERNEL_IMAGE_NAME)_kernel\"" > $(KERNEL_DIR)/include/linux/linux_build_info.h; \
	echo "#define MAKEFLAGS \"$(MAKEFLAGS)\"" >> $(KERNEL_DIR)/include/linux/linux_build_info.h; \
	$(MAKE) oldconfig; $(MAKE)

kernelconfig:
	cd $(KERNEL_DIR); \
	cp -f $(BRCM_LINUX_CONFIG_FILE) $(KERNEL_DIR)/.config; \
	$(MAKE) menuconfig

busyboxconfig:
ifeq ($(strip $(BRCM_CHIP)), 3380)
	cd $(BUILD_DIR)/userspace/gpl/apps/busybox; \
	cp -f brcm.config .config; \
	$(MAKE) menuconfig
endif

kernel: sanity_check create_install kernelbuild hosttools buildimage

ifeq ($(strip $(BRCM_DRIVER_BCMDSP)),m)
kernelbuildlite:
	@echo "******************** Kernel Build Lite ********************"; \
	$(BRCMDRIVERS_DIR)/broadcom/char/dspapp/impl1/getDspModSizes.sh  $(BRCMDRIVERS_DIR)/broadcom/char/dspapp/impl1/dspdd.ko $(SHARED_DIR) $(CROSS_COMPILE) $(KERNEL_DEBUG);
	cd $(KERNEL_DIR); $(MAKE)
else
kernelbuildlite:
endif

ifeq ($(strip $(VOXXXLOAD)),1)
modbuild: touch_voice_files 
	cd $(KERNEL_DIR); $(MAKE) modules && $(MAKE) modules_install
else
modbuild:
	cd $(KERNEL_DIR); $(MAKE) modules && $(MAKE) modules_install

endif

extmodbuild: $(DSP_TARGET_BUILD) $(DECT_TARGET_BUILD)
extmodclean: $(DSP_TARGET_CLEAN) $(DECT_TARGET_CLEAN)

# implemetation 1 is the DECT drvier from RTX ver 711. (not fully ported)
# implemetation 2 is the DECT drvier from RTX ver 1009.
# implemetation 3 is the DECT drvier from RTX ver 1116.
export CONFIG_BCM_DECT_DRIVER_IMPL=3
export INC_DECTDRV_PATH=$(BRCMDRIVERS_DIR)/broadcom/char/dect/impl$(CONFIG_BCM_DECT_DRIVER_IMPL)
dectmodbuild:
	cd $(KERNEL_DIR); $(MAKE) M=$(INC_DECTDRV_PATH) modules && $(MAKE) M=$(INC_DECTDRV_PATH) modules_install

dectmodclean:
	cd $(KERNEL_DIR); $(MAKE) M=$(INC_DECTDRV_PATH) clean
	cd $(INC_DECTDRV_PATH); $(MAKE) clean 

# DECT SHIM is slightly different on different chip.  Use the implementation specific to the chip	
INC_DECTSHIMDRV_PATH=$(BRCMDRIVERS_DIR)/broadcom/char/dectshim/bcm9$(BRCM_CHIP)
dectshimmodbuild:
	@if [ -d $(INC_DECTSHIMDRV_PATH) ]; then \
		cd $(KERNEL_DIR); $(MAKE) M=$(INC_DECTSHIMDRV_PATH) modules && $(MAKE) M=$(INC_DECTSHIMDRV_PATH) modules_install; \
    else \
		echo "****** Error : missing DECT shim at '$(INC_DECTSHIMDRV_PATH)'"; exit 1; \
	fi

dectshimmodclean:
	cd $(KERNEL_DIR); $(MAKE) M=$(INC_DECTSHIMDRV_PATH) clean

dspmodbuild:
	$(BRCMDRIVERS_DIR)/broadcom/char/dspapp/impl1/getDspModSizes.sh  $(BRCMDRIVERS_DIR)/broadcom/char/dspapp/impl1/dspdd.ko $(SHARED_DIR) $(CROSS_COMPILE) $(KERNEL_DEBUG);
	cd $(KERNEL_DIR); $(MAKE) M=$(BRCMDRIVERS_DIR)/broadcom/char/dspapp/impl1 modules && $(MAKE) M=$(BRCMDRIVERS_DIR)/broadcom/char/dspapp/impl1 modules_install;

dspmodclean:
	cd $(KERNEL_DIR); $(MAKE) M=$(BRCMDRIVERS_DIR)/broadcom/char/dspapp/impl1 clean

mocamodbuild:
	cd $(KERNEL_DIR); $(MAKE) M=$(INC_MOCACFGDRV_PATH) modules 
mocamodclean:
	cd $(KERNEL_DIR); $(MAKE) M=$(INC_MOCACFGDRV_PATH) clean

adslmodbuild:
	cd $(KERNEL_DIR); $(MAKE) M=$(INC_ADSLDRV_PATH) modules 
adslmodbuildclean:
	cd $(KERNEL_DIR); $(MAKE) M=$(INC_ADSLDRV_PATH) clean

spumodbuild:
	cd $(KERNEL_DIR); $(MAKE) M=$(INC_SPUDRV_PATH) modules
spumodbuildclean:
	cd $(KERNEL_DIR); $(MAKE) M=$(INC_SPUDRV_PATH) clean

pwrmngtmodbuild:
	cd $(KERNEL_DIR); $(MAKE) M=$(INC_PWRMNGTDRV_PATH) modules
pwrmngtmodclean:
	cd $(KERNEL_DIR); $(MAKE) M=$(INC_PWRMNGTDRV_PATH) clean

modules: sanity_check create_install modbuild extmodbuild kernelbuildlite fapbuild switchsdkbuild hosttools buildimage

adslmodule: adslmodbuild
adslmoduleclean: adslmodbuildclean

spumodule: spumodbuild
spumoduleclean: spumodbuildclean

pwrmngtmodule: pwrmngtmodbuild
pwrmngtmoduleclean: pwrmngtmodclean

unittests:
	$(MAKE) -C unittests

unittests_run:
	$(MAKE) -C unittests unittests_run

doxygen_build:
	$(MAKE) -C hostTools build_doxygen

doxygen_docs: doxygen_build
	rm -rf $(BUILD_DIR)/docs/doxygen;
	mkdir $(BUILD_DIR)/docs/doxygen;
	cd hostTools/doxygen/bin; ./doxygen

doxygen_clean:
	$(MAKE) -C hostTools clean_doxygen


# touch_voice_files doesn't clean up voice, just enables incremental build of voice code
touch_voice_files:
	find bcmdrivers/broadcom/char/endpoint/ \( -name '*.o' -o -name '*.a' -o -name '*.lib' -o -name '*.ko' -o -name '*.cmd' -o -name '.*.cmd' -o -name '*.c' -o -name '*.mod' \) -print -exec rm -f "{}" ";"
	rm -rf kernel/linux/.tmp_versions/endpointdd.mod
	rm -rf kernel/linux/arch/mips/defconfig
	rm -rf kernel/linux/include/config/bcm/endpoint/
	find kernel/linux/lib/ -name '*.o' -print -exec rm -f "{}" ";"
	find kernel/linux/lib/ -name '*.lib' -print -exec rm -f "{}" ";"



############################################################################
#
# Build user applications depending on if they are
# specified in the profile.  Most of these BUILD_ checks should eventually get
# moved down to the userspace directory.
#
############################################################################

ifneq ($(strip $(BUILD_VCONFIG)),)
export BUILD_VCONFIG=y
endif


ifneq ($(strip $(BUILD_GDBSERVER)),)
gdbserver:
	install -m 755 $(BUILD_DIR)/userspace/gpl/apps/gdb/gdbserver $(INSTALL_DIR)/bin
	$(STRIP) $(INSTALL_DIR)/bin/gdbserver
else
gdbserver:
endif

ifneq ($(strip $(BUILD_ETHWAN)),)
export BUILD_ETHWAN=y
endif

ifneq ($(strip $(BUILD_IPSEC_TOOLS)),)
ipsec-tools:
	cd $(OPENSOURCE_DIR);   (tar xkfj ipsec-tools.tar.bz2 2> /dev/null || true)
	$(MAKE) -C $(OPENSOURCE_DIR)/ipsec-tools $(BUILD_IPSEC_TOOLS)
else
ipsec-tools:
endif


ifneq ($(strip $(BUILD_SIPROXD)),)
siproxd:
	cd $(OPENSOURCE_DIR);   (tar xkfj siproxd.tar.bz2 2> /dev/null || true)
	$(MAKE) -C $(OPENSOURCE_DIR)/siproxd $(BUILD_SIPROXD)
libosip2:
	cd $(OPENSOURCE_DIR);   (tar xkfj libosip2.tar.bz2 2> /dev/null || true)
	$(MAKE) -C $(OPENSOURCE_DIR)/libosip2
else
siproxd:

libosip2:

endif

ifneq ($(strip $(BUILD_SNMP)),)

ifneq ($(strip $(BUILD_SNMP_SET)),)
export SNMP_SET=1
else
export SNMP_SET=0
endif

ifneq ($(strip $(BUILD_SNMP_ADSL_MIB)),)
export SNMP_ADSL_MIB=1
else
export SNMP_ADSL_MIB=0
endif

ifneq ($(strip $(BUILD_SNMP_ATM_MIB)),)
export SNMP_ATM_MIB=1
else
export SNMP_ATM_MIB=0
endif

ifneq ($(strip $(BUILD_SNMP_BRIDGE_MIB)),)
export SNMP_BRIDGE_MIB=1
else
export SNMP_BRIDGE_MIB=0
endif

ifneq ($(strip $(BUILD_SNMP_AT_MIB)),)
export SNMP_AT_MIB=1
else
export SNMP_AT_MIB=0
endif

ifneq ($(strip $(BUILD_SNMP_SYSOR_MIB)),)
export SNMP_SYSOR_MIB=1
else
export SNMP_SYSOR_MIB=0
endif

ifneq ($(strip $(BUILD_SNMP_TCP_MIB)),)
export SNMP_TCP_MIB=1
else
export SNMP_TCP_MIB=0
endif

ifneq ($(strip $(BUILD_SNMP_UDP_MIB)),)
export SNMP_UDP_MIB=1
else
export SNMP_UDP_MIB=0
endif

ifneq ($(strip $(BUILD_SNMP_IP_MIB)),)
export SNMP_IP_MIB=1
else
export SNMP_IP_MIB=0
endif

ifneq ($(strip $(BUILD_SNMP_ICMP_MIB)),)
export SNMP_ICMP_MIB=1
else
export SNMP_ICMP_MIB=0
endif

ifneq ($(strip $(BUILD_SNMP_SNMP_MIB)),)
export SNMP_SNMP_MIB=1
else
export SNMP_SNMP_MIB=0
endif

ifneq ($(strip $(BUILD_SNMP_ATMFORUM_MIB)),)
export SNMP_ATMFORUM_MIB=1
else
export SNMP_ATMFORUM_MIB=0
endif


ifneq ($(strip $(BUILD_SNMP_CHINA_TELECOM_CPE_MIB)),)
export BUILD_SNMP_CHINA_TELECOM_CPE_MIB=y
endif

ifneq ($(strip $(BUILD_CT_1_39_OPEN)),)
export BUILD_CT_1_39_OPEN=y
endif

ifneq ($(strip $(BUILD_SNMP_CHINA_TELECOM_CPE_MIB_V2)),)
export BUILD_SNMP_CHINA_TELECOM_CPE_MIB_V2=y
endif

ifneq ($(strip $(BUILD_SNMP_BRCM_CPE_MIB)),)
export BUILD_SNMP_BRCM_CPE_MIB=y
endif

ifneq ($(strip $(BUILD_SNMP_UDP)),)
export BUILD_SNMP_UDP=y
endif

ifneq ($(strip $(BUILD_SNMP_EOC)),)
export BUILD_SNMP_EOC=y
endif

ifneq ($(strip $(BUILD_SNMP_AAL5)),)
export BUILD_SNMP_AAL5=y
endif

ifneq ($(strip $(BUILD_SNMP_AUTO)),)
export BUILD_SNMP_AUTO=y
endif

ifneq ($(strip $(BUILD_SNMP_DEBUG)),)
export BUILD_SNMP_DEBUG=y
endif

ifneq ($(strip $(BUILD_SNMP_TRANSPORT_DEBUG)),)
export BUILD_SNMP_TRANSPORT_DEBUG=y
endif

ifneq ($(strip $(BUILD_SNMP_LAYER_DEBUG)),)
export BUILD_SNMP_LAYER_DEBUG=y
endif

endif

ifneq ($(strip $(BUILD_4_LEVEL_QOS)),)
export BUILD_4_LEVEL_QOS=y
endif

ifneq ($(strip $(BUILD_ILMI)),)
ilmi:
	cd $(OPENSOURCE_DIR);   (tar xkfj net-snmp.tar.bz2 2> /dev/null || true)
	$(MAKE) -C $(BROADCOM_DIR)/ilmi $(BUILD_ILMI)
else
ilmi:
endif


# vodsl (voice daemon)
# mwang: for now, I still need to leave vodsl at this top level because
# it depends on so many of the voice defines in this top level Makefile.
ifneq ($(strip $(BUILD_VODSL)),)
vodsl: sanity_check
	$(MAKE) -C $(BUILD_DIR)/userspace/private/apps/vodsl $(BUILD_VODSL)
else
vodsl: sanity_check
	@echo "skipping vodsl (not configured)"
endif

dectd: sanity_check
	$(MAKE) -C $(BUILD_DIR)/userspace/private/apps/dectd

appsdectd: sanity_check
	$(MAKE) -C $(BUILD_DIR)/userspace/private/apps/dectd apps

# Leave it for the future when soap server is decoupled from cfm
ifneq ($(strip $(BUILD_SOAP)),)
ifeq ($(strip $(BUILD_SOAP_VER)),2)
soapserver:
	$(MAKE) -C $(BROADCOM_DIR)/SoapToolkit/SoapServer $(BUILD_SOAP)
else
soap:
	$(MAKE) -C $(BROADCOM_DIR)/soap $(BUILD_SOAP)
endif
else
soap:
endif



ifeq ($(strip $(BUILD_LIBUSB)),y)
libusb:
	cd $(OPENSOURCE_DIR);   (tar xkfj libusb.tar.bz2 2> /dev/null || true)
	$(MAKE) -C $(OPENSOURCE_DIR)/libusb
	install -m 755 $(OPENSOURCE_DIR)/libusb/.libs/libusb-0.1.so.4 $(INSTALL_DIR)/lib
else
libusb:
endif



ifneq ($(strip $(BUILD_DIAGAPP)),)
diagapp:
	$(MAKE) -C $(BROADCOM_DIR)/diagapp $(BUILD_DIAGAPP)
else
diagapp:
endif



ifneq ($(strip $(BUILD_IPPD)),)
ippd:
	$(MAKE) -C $(BROADCOM_DIR)/ippd $(BUILD_IPPD)
else
ippd:
endif


ifneq ($(strip $(BUILD_PORT_MIRRORING)),)
export BUILD_PORT_MIRRORING=1
else
export BUILD_PORT_MIRRORING=0
endif

ifeq ($(BRCM_USE_SUDO_IFNOT_ROOT),y)
BRCM_BUILD_USR=$(shell whoami)
BRCM_BUILD_USR1=$(shell sudo touch foo;ls -l foo | awk '{print $$3}';sudo rm -rf foo)
else
BRCM_BUILD_USR=root
endif

ifneq ($(strip $(BUILD_OPROFILE)),)
oprofile:
	cd $(OPENSOURCE_DIR);   (tar xkfj oprofile.tar.bz2 2> /dev/null || true)
	$(MAKE) -C $(OPENSOURCE_DIR)/oprofile
else
oprofile:
endif

hosttools:
	$(MAKE) -C $(HOSTTOOLS_DIR) BRCM_KERNEL_ROOTFS=ubifs
	$(MAKE) -C $(HOSTTOOLS_DIR) BRCM_KERNEL_ROOTFS=squashfs

############################################################################
#
# IKOS defines
#
############################################################################

CMS_VERSION_FILE=$(BUILD_DIR)/userspace/public/include/version.h

ifeq ($(strip $(BRCM_IKOS)),y)
FS_COMPRESSION=-noD -noI -no-fragments
else
FS_COMPRESSION=
endif

export BRCM_IKOS FS_COMPRESSION



# IKOS Emulator build that does not include the CFE boot loader.
# Edit targets/ikos/ikos and change the chip and board id to desired values.
# Then build: make PROFILE=ikos ikos
ikos:
	@echo -e '#define SOFTWARE_VERSION ""\n#define RELEASE_VERSION ""\n#define PSI_VERSION ""\n' > $(CMS_VERSION_FILE)
	@-mv -f $(FSSRC_DIR)/etc/profile $(FSSRC_DIR)/etc/profile.dontuse >& /dev/null
	@if [ ! -a $(CFE_FILE) ] ; then echo "no cfe" > $(CFE_FILE); echo "no cfe" > $(CFE_FILE).del; fi
	@-rm $(HOSTTOOLS_DIR)/bcmImageBuilder >& /dev/null
	$(MAKE) PROFILE=$(PROFILE)
	@-rm $(HOSTTOOLS_DIR)/bcmImageBuilder >& /dev/null
	@mv -f $(FSSRC_DIR)/etc/profile.dontuse $(FSSRC_DIR)/etc/profile
	@cd $(PROFILE_DIR); \
	$(OBJCOPY) --output-target=srec vmlinux vmlinux.srec; \
	xxd $(FS_KERNEL_IMAGE_NAME) | grep "^00000..:" | xxd -r > bcmtag.bin; \
	$(OBJCOPY) --output-target=srec --input-target=binary --change-addresses=0xb8010000 bcmtag.bin bcmtag.srec; \
	$(OBJCOPY) --output-target=srec --input-target=binary --change-addresses=0xb8010100 rootfs.img rootfs.srec; \
	rm bcmtag.bin; \
	grep -v "^S7" vmlinux.srec > bcm9$(BRCM_CHIP)_$(PROFILE).srec; \
	grep "^S3" bcmtag.srec >> bcm9$(BRCM_CHIP)_$(PROFILE).srec; \
	grep -v "^S0" rootfs.srec >> bcm9$(BRCM_CHIP)_$(PROFILE).srec
	@if [ ! -a $(CFE_FILE).del ] ; then rm -f $(CFE_FILE) $(CFE_FILE).del; fi
	@echo -e "\nAn image without CFE for the IKOS emulator has been built.  It is named"
	@echo -e "targets/$(PROFILE)/bcm9$(BRCM_CHIP)_$(PROFILE).srec\n"

# IKOS Emulator build that includes the CFE boot loader.
# Both Linux and CFE boot loader toolchains need to be installed.
# Edit targets/ikos/ikos and change the chip and board id to desired values.
# Then build: make PROFILE=ikos ikoscfe
ikoscfe:
	@echo -e '#define SOFTWARE_VERSION ""\n#define RELEASE_VERSION ""\n#define PSI_VERSION ""\n' > $(CMS_VERSION_FILE)
	@-mv -f $(FSSRC_DIR)/etc/profile $(FSSRC_DIR)/etc/profile.dontuse >& /dev/null
	$(MAKE) PROFILE=$(PROFILE)
	@mv -f $(FSSRC_DIR)/etc/profile.dontuse $(FSSRC_DIR)/etc/profile
	$(MAKE) -C $(BL_BUILD_DIR) clean
	$(MAKE) -C $(BL_BUILD_DIR)
	$(MAKE) -C $(BL_BUILD_DIR) ikos_finish
	cd $(PROFILE_DIR); \
	echo -n "** no kernel  **" > kernelfile; \
	$(HOSTTOOLS_DIR)/bcmImageBuilder --output $(CFE_FS_KERNEL_IMAGE_NAME) --chip $(BRCM_CHIP) --board $(BRCM_BOARD_ID) --blocksize $(BRCM_FLASHBLK_SIZE) --cfefile $(BL_BUILD_DIR)/cfe$(BRCM_CHIP).bin --rootfsfile rootfs.img --kernelfile kernelfile --include-cfe; \
	$(HOSTTOOLS_DIR)/createimg --boardid=$(BRCM_BOARD_ID) --numbermac=$(BRCM_NUM_MAC_ADDRESSES) --macaddr=$(BRCM_BASE_MAC_ADDRESS) --tp=$(BRCM_MAIN_TP_NUM) --psisize=$(BRCM_PSI_SIZE) --inputfile=$(CFE_FS_KERNEL_IMAGE_NAME) --outputfile=$(FLASH_IMAGE_NAME); \
	$(HOSTTOOLS_DIR)/addvtoken $(FLASH_IMAGE_NAME) $(FLASH_IMAGE_NAME).w; \
	$(OBJCOPY) --output-target=srec --input-target=binary --change-addresses=0xb8000000 $(FLASH_IMAGE_NAME).w $(FLASH_IMAGE_NAME).srec; \
	$(OBJCOPY) --output-target=srec vmlinux vmlinux.srec; \
	@rm kernelfile; \
	grep -v "^S7" vmlinux.srec > bcm9$(BRCM_CHIP)_$(PROFILE).srec; \
	grep "^S3" $(BL_BUILD_DIR)/cferam$(BRCM_CHIP).srec >> bcm9$(BRCM_CHIP)_$(PROFILE).srec; \
	grep -v "^S0" $(FLASH_IMAGE_NAME).srec >> bcm9$(BRCM_CHIP)_$(PROFILE).srec; \
	grep -v "^S7" vmlinux.srec > bcm9$(BRCM_CHIP)_$(PROFILE).utram.srec; \
	grep -v "^S0" $(BL_BUILD_DIR)/cferam$(BRCM_CHIP).srec >> bcm9$(BRCM_CHIP)_$(PROFILE).utram.srec;
	@echo -e "\nAn image with CFE for the IKOS emulator has been built.  It is named"
	@echo -e "targets/$(PROFILE)/bcm9$(BRCM_CHIP)_$(PROFILE).srec"
	@echo -e "\nBefore testing with the IKOS emulator, this build can be unit tested"
	@echo -e "with an existing chip and board as follows."
	@echo -e "1. Flash targets/$(PROFILE)/$(FLASH_IMAGE_NAME).w onto an existing board."
	@echo -e "2. Start the EPI EDB debugger.  At the edbice prompt, enter:"
	@echo -e "   edbice> fr m targets/$(PROFILE)/bcm9$(BRCM_CHIP)_$(PROFILE).utram.srec"
	@echo -e "   edbice> r"
	@echo -e "3. Program execution will start at 0xb8000000 (or 0xbfc00000) and,"
	@echo -e "   if successful, will enter the Linux shell.\n"


############################################################################
#
# This is where we build the image
#
#
#  Naming rules for 3383
#     	We build five images using FS_KERNEL_IMAGE_NAME and 
#       APPS_IMAGE_NAME as the base name:
#	kernel and rootfs for nor - $(FS_KERNEL_IMAGE_NAME)_kernel_rootfs_squash
#	apps partition for nor    - $(APPS_IMAGE_NAME)_nor_jffs2
#	kernel for nand           - $(FS_KERNEL_IMAGE_NAME)_kernel
#	rootfs for nand           - $(FS_KERNEL_IMAGE_NAME)_rootfs_ubifs_bs128k_ps2k
#	apps patition for nand    - $(APPS_IMAGE_NAME)_nand_ubifs_bs128k_ps2k
#	
#  Naming rules for 3380
#     	We build one image using FS_KERNEL_IMAGE_NAME as the base name 
#	kernel and rootfs for nor - $(FS_KERNEL_IMAGE_NAME)_fs_kernel
#      
############################################################################
vmlinux:
	cp $(KERNEL_DIR)/vmlinux $(PROFILE_DIR)
	$(STRIP) --remove-section=.note --remove-section=.comment $(PROFILE_DIR)/vmlinux
	$(OBJCOPY) -O binary $(PROFILE_DIR)/vmlinux $(PROFILE_DIR)/vmlinux.bin
	$(HOSTTOOLS_DIR)/cmplzma -k -2 $(PROFILE_DIR)/vmlinux $(PROFILE_DIR)/vmlinux.bin $(PROFILE_DIR)/vmlinux.lz


buildimage: $(KERNEL_DIR)/vmlinux libcreduction vmlinux
	cd $(TARGETS_DIR); ./buildFS;
ifeq ($(strip $(BRCM_CHIP)), 3380)
	make generate_3380_images
else
	make generate_images PROCESSOR_ID=$(PID)
endif

generate_images:
	cd $(PROFILE_DIR); \
	KERNEL_NAME=$(FS_KERNEL_IMAGE_NAME)_kernel; \
	$(HOSTTOOLS_DIR)/ProgramStore2 -f vmlinux.bin -o $$KERNEL_NAME -v 002.17h -a $(KERNEL_LOAD_ADDRESS) -n2 -p 65536 -c 4 -s 0x$(PROCESSOR_ID)

	export BRCM_KERNEL_ROOTFS=ubifs; \
	export BRCM_NANDFLASH_BLOCK_SIZE=131072; \
	export BRCM_NANDFLASH_PAGE_SIZE=2048; \
	export BRCM_NANDFLASH_PAD_SIZE=100; \
	cd $(TARGETS_DIR); $(HOSTTOOLS_DIR)/fakeroot/fakeroot ./buildFS2

	cd $(PROFILE_DIR); \
	ROOTFS_NAME=$(FS_KERNEL_IMAGE_NAME)_rootfs_ubifs_bs128k_ps2k; \
	APPS_NAME=$(APPS_IMAGE_NAME)_nand_ubifs_bs128k_ps2k; \
	BRCM_NANDFLASH_BLOCK_SIZE=131072; \
	BRCM_NANDFLASH_PAD_SIZE=100; \
	dd if=/dev/urandom of=filepad.tmp bs=1k count=$$BRCM_NANDFLASH_PAD_SIZE; \
	$(HOSTTOOLS_DIR)/ProgramStore2 -f filepad.tmp -f2 rootfs.img -o $$ROOTFS_NAME -v 002.17h -a $(KERNEL_LOAD_ADDRESS) -n2 -p $$BRCM_NANDFLASH_BLOCK_SIZE -c 0 -s 0x$(PROCESSOR_ID); \
	$(HOSTTOOLS_DIR)/ProgramStore2 -f filepad.tmp -f2 apps.img -o $$APPS_NAME -v 002.17h -a $(KERNEL_LOAD_ADDRESS) -n2 -p $$BRCM_NANDFLASH_BLOCK_SIZE -c 0 -s 0x$(PROCESSOR_ID);


	export BRCM_KERNEL_ROOTFS=squashfs; \
	export BRCM_SQUASHFS_BLOCK_SIZE=65536; \
	cd $(TARGETS_DIR); $(HOSTTOOLS_DIR)/fakeroot/fakeroot ./buildFS2

	cd $(PROFILE_DIR); \
	KERNEL_NAME=$(FS_KERNEL_IMAGE_NAME)_kernel_rootfs_squash; \
	APPS_NAME=$(APPS_IMAGE_NAME)_nor_jffs2; \
	$(HOSTTOOLS_DIR)/ProgramStore2 -f vmlinux.bin -f2 rootfs.img -o $$KERNEL_NAME -v 002.17h -a $(KERNEL_LOAD_ADDRESS) -n2 -p 65536 -c 4 -s 0x$(PROCESSOR_ID); \
	dd if=/dev/zero of=filepad.tmp bs=1k count=50; \
	$(HOSTTOOLS_DIR)/ProgramStore2 -f filepad.tmp -f2 apps.jffs2 -o $$APPS_NAME -v 002.17h -a $(KERNEL_LOAD_ADDRESS) -n2 -p 65536 -c 0 -s 0x$(PROCESSOR_ID); \
	rm filepad.tmp
	@echo
	@echo -e "Done! Image $(PROFILE) has been built in $(PROFILE_DIR).  Created the following target images:"
	@echo -e "\t kernel and rootfs for nor - $(FS_KERNEL_IMAGE_NAME)_kernel_rootfs_squash"
	@echo -e "\t apps partition for nor    - $(APPS_IMAGE_NAME)_nor_jffs2"
	@echo -e "\t kernel for nand           - $(FS_KERNEL_IMAGE_NAME)_kernel"
	@echo -e "\t rootfs for nand           - $(FS_KERNEL_IMAGE_NAME)_rootfs_ubifs_bs128k_ps2k"
	@echo -e "\t apps patition for nand    - $(APPS_IMAGE_NAME)_nand_ubifs_bs128k_ps2k"

generate_3380_images:
	export BRCM_KERNEL_ROOTFS=squashfs; \
	export BRCM_SQUASHFS_BLOCK_SIZE=65536; \
	cd $(TARGETS_DIR); $(HOSTTOOLS_DIR)/fakeroot/fakeroot ./buildFS2

	cd $(PROFILE_DIR); \
	cp $(KERNEL_DIR)/vmlinux . ; \
	$(STRIP) --remove-section=.note --remove-section=.comment vmlinux; \
	$(OBJCOPY) -O binary vmlinux vmlinux.bin; \
	$(HOSTTOOLS_DIR)/cmplzma -k -2 vmlinux vmlinux.bin vmlinux.lz;\
	ProgramStore2 -f vmlinux.bin -f2 rootfs.img -o $(FS_KERNEL_IMAGE_NAME)_fs_kernel -v 002.17h -a $(KERNEL_LOAD_ADDRESS) -n2 -p 65536 -c 4 -s 0x$(BRCM_CHIP);\
	#ProgramStore2 -f vmlinux.bin -f2 rootfs.img -o $(FS_KERNEL_IMAGE_NAME).mr0 -v 002.17h -a 0x80010000 -n2 -p 65536 -c 4 -s 0x$(BRCM_CHIP)
	@echo
	@echo -e "Done! Image $(FS_KERNEL_IMAGE_NAME)_fs_kernel has been built in $(PROFILE_DIR)."

ifeq ($(strip $(INITRD_IMAGENAME)),)
INITRD_IMAGE_NAME=vmlinux
endif
initrd_buildimage: vmlinux
	cd $(PROFILE_DIR); \
	$(HOSTTOOLS_DIR)/ProgramStore2 -f vmlinux.bin -o $(INITRD_IMAGE_NAME)_initrd_sto.bin -v 003.000 -a $(KERNEL_LOAD_ADDRESS) -c 4 -s 0x$(PROCESSOR_ID);



###########################################
#
# System code clean-up
#
###########################################

#
# mwang: since SUBDIRS are no longer defined, the next two targets are not useful anymore.
# how were they used anyways?
#
#subdirs: $(patsubst %, _dir_%, $(SUBDIRS))

#$(patsubst %, _dir_%, $(SUBDIRS)) :
#	$(MAKE) -C $(patsubst _dir_%, %, $@) $(TGT)

clean: target_clean bcmdrivers_clean userspace_clean kernel_clean \
       extmod_clean hosttools_clean voice_clean xchange_clean switchsdk_clean fsapps_clean
	rm -f $(HOSTTOOLS_DIR)/scripts/lxdialog/*.o
	rm -f .tmpconfig*


switchsdk_clean:
ifneq ($(strip $(BRCM_DRIVER_SWITCHSDK)),)
	cd $(SDK_USERBLD_DIR); $(MAKE) clean
endif

fssrc_clean:
	rm -fr $(FSSRC_DIR)/bin
	# rm -fr $(FSSRC_DIR)/sbin
	rm -fr $(FSSRC_DIR)/lib
	rm -fr $(FSSRC_DIR)/upnp
	rm -fr $(FSSRC_DIR)/docs
	rm -fr $(FSSRC_DIR)/webs
	# rm -fr $(FSSRC_DIR)/usr
	rm -fr $(FSSRC_DIR)/linuxrc
	rm -fr $(FSSRC_DIR)/images
	rm -fr $(FSSRC_DIR)/etc/wlan
	rm -fr $(FSSRC_DIR)/etc/certs


fsapps_clean:
	rm -rf $(PROFILE_DIR)/fs.apps


kernel_clean: sanity_check
	$(MAKE) -C $(KERNEL_DIR) mrproper
	rm -f $(KERNEL_DIR)/arch/mips/defconfig
	rm -f $(HOSTTOOLS_DIR)/lzma/decompress/*.o
	rm -rf $(XCHANGE_DIR)/dslx/lib/LinuxKernel
	rm -rf $(XCHANGE_DIR)/dslx/obj/LinuxKernel

bcmdrivers_clean:
	$(MAKE) -C bcmdrivers clean

userspace_clean: sanity_check app_expand_before_clean fssrc_clean 
	$(MAKE) -C userspace clean


app_expand_before_clean:
	# these archives need to be expended before cleaning - put makefile structures there there for cleaning
	# OProfile
	(tar xkfj $(BUILD_DIR)/userspace/gpl/apps/oprofile.tar.bz2 -C $(BUILD_DIR)/userspace/gpl/apps 2> /dev/null || true)

unittests_clean:
	$(MAKE) -C unittests clean

target_clean: sanity_check
	rm -f $(PROFILE_DIR)/rootfs*.img
	rm -f $(PROFILE_DIR)/vmlinux
	rm -f $(PROFILE_DIR)/vmlinux.bin
	rm -f $(PROFILE_DIR)/vmlinux.lz
	rm -f $(PROFILE_DIR)/$(FS_KERNEL_IMAGE_NAME)
	rm -f $(PROFILE_DIR)/$(CFE_FS_KERNEL_IMAGE_NAME)
	rm -f $(PROFILE_DIR)/$(FLASH_IMAGE_NAME)
	rm -f $(PROFILE_DIR)/$(FLASH_IMAGE_NAME).w
	rm -f $(PROFILE_DIR)/$(FLASH_NAND_RAW_FS_IMAGE_NAME_16).w
	rm -f $(PROFILE_DIR)/$(FLASH_NAND_FS_IMAGE_NAME_16).w
	rm -f $(PROFILE_DIR)/$(FLASH_NAND_RAW_FS_IMAGE_NAME_128).w
	rm -f $(PROFILE_DIR)/$(FLASH_NAND_FS_IMAGE_NAME_128).w
	rm -f $(PROFILE_DIR)/*.srec
	rm -rf $(PROFILE_DIR)/modules
	rm -rf $(PROFILE_DIR)/op
	rm -rf $(INSTALL_DIR)
	rm -rf $(TARGETS_DIR)/fs.app/usr/local/*
	rm -rf $(TARGET_FS)
	rm -rf $(PROFILE_DIR)/$(FS_KERNEL_IMAGE_NAME)_*
	rm -rf $(PROFILE_DIR)/apps*
	rm -rf $(PROFILE_DIR)/stage

hosttools_clean:
	$(MAKE) -C $(HOSTTOOLS_DIR) clean

xchange_clean:
	rm -rf $(XCHANGE_DIR)/dslx/lib/LinuxUser
	rm -rf $(XCHANGE_DIR)/dslx/obj/LinuxUser	

# In the data release tarball, vodsl is not present,
# so don't go in there unconditionally.
vodsl_clean: sanity_check
ifneq ($(strip $(BUILD_VODSL)),)
	find userspace/private/apps/vodsl -name '*.o' -exec rm -f "{}" ";"
	@if [ -e xChange/dslx/obj/LinuxUser ]; then \
	  (find xChange/dslx/obj/LinuxUser  -name '*' -exec rm -f "{}" ";") \
	fi
endif

voice_clean: sanity_check vodsl_clean
ifneq ($(strip $(BUILD_VODSL)),)
	find bcmdrivers/broadcom/char/endpoint -name '*.ko' -exec rm -f "{}" ";"
	find bcmdrivers/broadcom/char/endpoint -name '*.o' -exec rm -f "{}" ";"
	find bcmdrivers/broadcom/char/endpoint -name '*.a' -exec rm -f "{}" ";"
	find bcmdrivers/broadcom/char/endpoint -name '*.lib' -exec rm -f "{}" ";"
	find bcmdrivers/broadcom/char/dspapp -name '*.o' -exec rm -f "{}" ";"
	find bcmdrivers/broadcom/char/dspapp -name '*.ko' -exec rm -f "{}" ";"
	find kernel/linux/eptlib -name '*.lib' -exec rm -f "{}" ";"
endif
	rm -rf $(XCHANGE_DIR)/dslx/lib/LinuxKernel
	rm -rf $(XCHANGE_DIR)/dslx/obj/LinuxKernel
	rm -rf $(XCHANGE_DIR)/dslx/lib/LinuxUser
	rm -rf $(XCHANGE_DIR)/dslx/obj/LinuxUser
	rm -f $(KERNEL_DIR)/eptlib/*


dectd_clean: sanity_check
	$(MAKE) -C $(BUILD_DIR)/userspace/private/apps/dectd clean

slic_clean: sanity_check voice_clean
ifneq ($(strip $(BUILD_VODSL)),)
	find userspace/private/libs/cms_core/linux -name 'rut_voice.o' -exec rm -f "{}" ";"
	find userspace/private/libs/cms_core/linux -name 'rut_voice.d' -exec rm -f "{}" ";"
	find userspace/private/libs/cms_dal -name 'dal_voice.o' -exec rm -f "{}" ";"
	find userspace/private/libs/cms_dal -name 'dal_voice.d' -exec rm -f "{}" ";"
endif

extmod_clean: extmodclean

###########################################
#
# Temporary kernel patching mechanism
#
###########################################

.PHONY: genpatch patch

genpatch:
	@hostTools/kup_tmp/genpatch

patch:
#	@hostTools/kup_tmp/patch


###########################################
#
# System-wide exported variables
# (in alphabetical order)
#
###########################################


export \
BRCMAPPS                   \
BRCM_BOARD                 \
BRCM_DRIVER_PCI            \
BRCM_EXTRAVERSION          \
BRCM_KERNEL_NETQOS         \
BRCM_KERNEL_ROOTFS         \
BRCM_NANDFLASH_BLOCK_SIZE  \
BRCM_NANDFLASH_PAGE_SIZE   \
BRCM_SQUASHFS_BLOCK_SIZE   \
BRCM_SQUASHFS_FRAGMENT_CACHE_SIZE         \
BRCM_KERNEL_OPROFILE       \
BRCM_LDX_APP               \
BRCM_MIPS_ONLY_BUILD       \
BRCM_MIPS_ONLY_BUILD       \
BRCM_PSI_VERSION           \
BRCM_PTHREADS              \
BRCM_RELEASE               \
BRCM_RELEASETAG            \
BRCM_SNMP                  \
BRCM_VERSION               \
BUILD_FCCTL                \
BUILD_CMFCTL               \
BUILD_CMFVIZ               \
BUILD_CMFD                 \
BUILD_XDSLCTL              \
BUILD_XTMCTL               \
BUILD_VLANCTL              \
BUILD_BRCM_VLAN            \
BUILD_BRCTL                \
BUILD_BUSYBOX              \
BUILD_CERT                 \
BUILD_DDNSD                \
BUILD_DIAGAPP              \
BUILD_DIR                  \
BUILD_DNSPROBE             \
BUILD_DPROXY               \
BUILD_DYNAHELPER           \
BUILD_DNSSPOOF             \
BUILD_EBTABLES             \
BUILD_EPITTCP              \
BUILD_ETHWAN               \
BUILD_FTPD                 \
BUILD_FTPD_STORAGE         \
BUILD_GDBSERVER            \
BUILD_HTTPD                \
BUILD_HTTPD_SSL            \
BUILD_IGMP                 \
BUILD_MLD                  \
BUILD_IPPD                 \
BUILD_IPROUTE2             \
BUILD_IPSEC_TOOLS          \
BUILD_L2TPAC               \
BUILD_IPTABLES             \
BUILD_WPS_BTN              \
BUILD_LLTD                 \
BUILD_WSC                   \
BUILD_BCMCRYPTO \
BUILD_BCMSHARED \
BUILD_NAS                  \
BUILD_NVRAM                \
BUILD_OPROFILE             \
BUILD_PORT_MIRRORING			 \
BUILD_PPPD                 \
BUILD_SES                  \
BUILD_SIPROXD              \
BUILD_SLACTEST             \
BUILD_SNMP                 \
BUILD_SNTP                 \
BUILD_SOAP                 \
BUILD_SOAP_VER             \
BUILD_SSHD                 \
BUILD_SSHD_MIPS_GENKEY     \
BUILD_TOD                  \
BUILD_TR64                 \
BUILD_TR64_DEVICECONFIG    \
BUILD_TR64_DEVICEINFO      \
BUILD_TR64_LANCONFIGSECURITY \
BUILD_TR64_LANETHINTERFACECONFIG \
BUILD_TR64_LANHOSTS        \
BUILD_TR64_LANHOSTCONFIGMGMT \
BUILD_TR64_LANUSBINTERFACECONFIG \
BUILD_TR64_LAYER3          \
BUILD_TR64_MANAGEMENTSERVER  \
BUILD_TR64_TIME            \
BUILD_TR64_USERINTERFACE   \
BUILD_TR64_QUEUEMANAGEMENT \
BUILD_TR64_LAYER2BRIDGE   \
BUILD_TR64_WANCABLELINKCONFIG \
BUILD_TR64_WANCOMMONINTERFACE \
BUILD_TR64_WANDSLINTERFACE \
BUILD_TR64_WANDSLLINKCONFIG \
BUILD_TR64_WANDSLCONNECTIONMGMT \
BUILD_TR64_WANDSLDIAGNOSTICS \
BUILD_TR64_WANETHERNETCONFIG \
BUILD_TR64_WANETHERNETLINKCONFIG \
BUILD_TR64_WANIPCONNECTION \
BUILD_TR64_WANPOTSLINKCONFIG \
BUILD_TR64_WANPPPCONNECTION \
BUILD_TR64_WLANCONFIG      \
BUILD_TR69C                \
BUILD_TR69_QUEUED_TRANSFERS \
BUILD_TR69C_SSL            \
BUILD_TR69_XBRCM           \
BUILD_TR69_UPLOAD          \
BUILD__OMCI            \
BUILD_UDHCP                \
BUILD_UDHCP_RELAY          \
BUILD_UPNP                 \
BUILD_VCONFIG              \
BUILD_SUPERDMZ             \
BUILD_SIP_TLS              \
BUILD_WLCTL                \
BUILD_ZEBRA                \
BUILD_SAMBA                \
BUILD_LIBUSB               \
BUILD_OPENSSL              \
BUILD_CUPS                 \
BUILD_JPEG                 \
BUILD_HPLIP                \
BUILD_ZLIB                 \
BUILD_LIBPNG               \
BUILD_GNU_GHOSTSCRIPT      \
BUILD_WANVLANMUX           \
HOSTTOOLS_DIR              \
INC_KERNEL_BASE            \
INSTALL_DIR                \
JTAG_KERNEL_DEBUG          \
PROFILE_DIR                \
WEB_POPUP                  \
HTTPS_SUPPORT              \
BUILD_VIRT_SRVR            \
BUILD_PORT_TRIG            \
BUILD_TR69C_BCM_SSL        \
BUILD_IPV6                 \
LINUX_KERNEL_USBMASS       \
BUILD_IPSEC                \
BUILD_MoCACTL              \
BUILD_MoCACTL1             \
BUILD_MoCACTL2             \
BUILD_GPONCTL              \
BUILD_PMON                 \
BUILD_BOUNCE               \
BUILD_HELLO                \
BUILD_SPUCTL               \
BUILD_PWRCTL               \
BUILD_MROUTE               \
BUILD_RNGD                 \
RELEASE_BUILD        	\
NO_PRINTK_AND_BUG       \
FLASH_NAND_BLOCK_16KB      \
FLASH_NAND_BLOCK_128KB     \
BRCM_ROOTFS_SQUASHFS_3_0 \
BRCM_ROOTFS_SQUASHFS_4_0
