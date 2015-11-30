#
# To add an app, add it to APP_DIRS, and write the target rule in the
# last section of this Makefile
#
APP_DIRS = atm2684 bridge-utils busybox ebtables ftpd \
           iproute2 iptables radvd udhcp urlfilterd \
           dynahelper mtd-utils ntfs-3g openl2tpd rng-tools-2 portmap e2fsprogs \
           sqlite avahi-app httpd libusb zlib cups-app samba libid3tag mt-daapd-app \
		   smartmontools

#
# We need to declare these directory names as phony targets,
# otherwise make will think the directories themselves are
# the targets and will never execute the cmds.
# See GNU Make, 4.6 
#
.PHONY : $(APP_DIRS)

# Some apps have names that are different from their directory
# name, so need to list them explictly here.
all : $(APP_DIRS) brctl dproxy

#
# Cleaning rules
# See example in GNU Make, 4.6
#
clean:  
	# Run make clean if it exists to remove any built apps
	# Run the app-clean target to fully clean all binaries 
	for dir in $(APP_DIRS); do \
		if [ -d $$dir ] ; then \
			$(MAKE) -C $$dir clean ; \
		else \
			$(MAKE) $$dir-clean ; \
		fi \
	done

	# Remove all folders
	rm -rf $(APPS_STAGING_DIR)
	rm -rf openssl-$(OPENSSL_VERSION)
	rm -rf cups-native
	rm -rf cups-$(CUPS_VERSION)
	rm -rf jpeg-$(JPEG_VERSION)
	rm -rf libusb-$(LIBUSB_VERSION)
	rm -rf hplip-$(HPLIP_VERSION)
	rm -rf zlib-$(ZLIB_VERSION)
	rm -rf libpng-$(LIBPNG_VERSION)
	rm -rf gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)
	rm -rf avahi-$(AVAHI_VERSION)
	rm -rf expat-$(EXPAT_VERSION)
	rm -rf dbus-$(DBUS_VERSION)
	rm -rf libdaemon-$(LIBDAEMON_VERSION)
	rm -rf sqlite-$(SQLITE_VERSION)
	rm -rf libid3tag-$(LIBID3TAG_VERSION)
	rm -rf mt-daapd-$(MT_DAAPD_VERSION)
	rm -rf busybox-$(BUSYBOX_VERSION)
#	rm -rf db-$(BERKELEYDB_VERSION)
#	rm -rf netatalk-$(NETATALK_VERSION)
	rm -rf smartmontools-5.42
	rm -rf e2fsprogs-$(E2FS_PROGS_VERSION)
	rm -rf ntfs-3g_ntfsprogs-2012.1.15

#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
# You do not need to modify this part.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

# If BUILD_AVAHI is enabled, use avahi libraries
ifneq ($(filter yes y, $(BUILD_AVAHI)), )
export AVAHI_LIBS = -lavahi-common -lavahi-client
else
export AVAHI_LIBS = 
endif

include $(BUILD_DIR)/make.common

export BUILD_DIR

export APPS_INSTALL_DIR = $(PROFILE_DIR)/fs.apps/usr/local
export APPS_STAGING_DIR = $(STAGE_DIR)

##############################################################
# 
# Rules for inidividual apps
# apps can be compiled or not depending on the profile (which
# is modified via make menuconfig)
#
##############################################################

ifneq ($(filter yes y, $(BUILD_CUPS)), )
cups-app: openssl jpeg zlib libpng expat dbus libusb cups hplip gnu-ghostscript
cups-app-clean: openssl-clean jpeg-clean libpng-clean cups-clean hplip-clean gnu-ghostscript-clean 
else
cups-app: cups
cups-app-clean:
endif

ifneq ($(filter yes y, $(BUILD_AVAHI)), )
avahi-app: libdaemon expat dbus avahi
avahi-app-clean: libdaemon-clean expat-clean dbus-clean avahi-clean
else
avahi-app: 
avahi-app-clean:
endif

ifneq ($(filter yes y, $(BUILD_MT_DAAPD)), )
mt-daapd-app: e2fsprogs expat dbus sqlite zlib libid3tag mt-daapd
mt-daapd-app-clean: expat-clean dbus-clean libid3tag-clean mt-daapd-clean
else
mt-daapd-app: 
mt-daapd-app-clean:
endif

ifneq ($(filter yes y, $(BUILD_NETATALK)), )
netatalk-app: netatalk
netatalk-app-clean: netatalk-clean
else
netatalk-app:
netatalk-app-clean:
endif

ifneq ($(strip $(BUILD_BRCTL)),)
brctl: sanity_check bridge-utils.tar.bz2
	mkdir -p $(INSTALL_DIR)/bin
	(tar xkfj bridge-utils.tar.bz2 2> /dev/null || true)
	$(MAKE) -C bridge-utils $(BUILD_BRCTL)
else
brctl: sanity_check
endif

ifeq ($(strip $(BRCM_IKOS)),y)
BUSYBOX_CONFIG=ikos.config
else
BUSYBOX_CONFIG=brcm.config
endif

ifneq ($(strip $(BUILD_BUSYBOX)),)
busybox: sanity_check busybox-$(BUSYBOX_VERSION).tar.bz2
	mkdir -p $(INSTALL_DIR)/bin
		(tar xkfj busybox-$(BUSYBOX_VERSION).tar.bz2 2> /dev/null || true)
	- cp busybox-patch/* busybox-$(BUSYBOX_VERSION)
	- cp busybox-patch/.config busybox-$(BUSYBOX_VERSION)
	$(MAKE) -C busybox-$(BUSYBOX_VERSION) install CONFIG_PREFIX=$(INSTALL_DIR) CROSS_COMPILE=$(CROSS_COMPILE)
	mkdir -p $(INSTALL_DIR)/etc/dhcp
	- cp busybox-$(BUSYBOX_VERSION)/examples/zcip.script $(INSTALL_DIR)/etc/dhcp/zcip.script
	- cp -p busybox-patch/zcip.init $(INSTALL_DIR)/etc/dhcp/dhcpc.leasefail
busybox-clean:
	$(MAKE) -C busybox-$(BUSYBOX_VERSION) clean
	rm -f $(INSTALL_DIR)/etc/dhcp/zcip.script
else
busybox: sanity_check
	@echo "skipping busybox (not configured)"
busybox-clean:
endif

ifneq ($(strip $(BUILD_DPROXY)),)
dproxy: sanity_check dproxy-nexgen.tar.bz2
	mkdir -p $(INSTALL_DIR)/bin
	(tar xkfj dproxy-nexgen.tar.bz2 2> /dev/null || true)
	$(MAKE) -C dproxy-nexgen $(BUILD_DPROXY)
else
dproxy: sanity_check
	@echo "skipping dproxy (not configured)"
endif

# in userapps->Dynamic ALG loader
ifneq ($(strip $(BUILD_DYNAHELPER)),)
dynahelper: sanity_check
	$(MAKE) -C dynahelper $(BUILD_DYNAHELPER)
else
dynahelper: sanity_check
	@echo "skipping dynahelper (not configured)"
endif

ifneq ($(strip $(BUILD_EBTABLES)),)
ebtables: sanity_check ebtables.tar.bz2 
	mkdir -p $(INSTALL_DIR)/bin
	(tar xkfj ebtables.tar.bz2 2> /dev/null || true)
	$(MAKE) -C ebtables $(BUILD_EBTABLES)
else
ebtables: sanity_check
	@echo "skipping ebtables (not configured)"
endif

# ftpd does sanity check in its own Makefile
ifneq ($(strip $(BUILD_FTPD)),)
ftpd: ftpd.tar.bz2
	(tar xkfj ftpd.tar.bz2 2> /dev/null || true)
	$(MAKE) -C ftpd $(BUILD_FTPD)
else
ftpd:
	@echo "skipping ftpd (not configured)"
endif

# Build the iproute2 package whenever IPROUTE2, IPV6, or BRCM_KERNEL_NETQOS
# is defined.  In make menuconfig, "traffic control" enables BRCM_KERNEL_NETQOS
ifneq ($(strip $(BUILD_IPROUTE2)),)
DO_BUILD_IPROUTE2=y
endif

ifneq ($(strip $(BUILD_IPV6)),)
DO_BUILD_IPROUTE2=y
endif

ifneq ($(strip $(BRCM_KERNEL_NETQOS)),)
DO_BUILD_IPROUTE2=y
endif

ifneq ($(strip $(DO_BUILD_IPROUTE2)),)
iproute2: sanity_check iproute2.tar.bz2
	(tar xkfj iproute2.tar.bz2 2> /dev/null || true)
	$(MAKE) -C iproute2 dynamic
else
iproute2: sanity_check
	@echo "skipping iproute2 (not configured)"
endif

# iptables is dependent on kernel netfilter modules
ifneq ($(strip $(BRCM_KERNEL_NETFILTER)),)
ifneq ($(strip $(BUILD_IPTABLES)),)
iptables: sanity_check iptables.tar.bz2
	(tar xkfj iptables.tar.bz2 2> /dev/null || true)
	$(MAKE) -C iptables $(BUILD_IPTABLES)
else
iptables: sanity_check
	@echo "skipping iptables (not configured)"
endif
else
iptables: sanity_check
	@echo Warning: To use iptables, you need to enable netfilter in the kernel !!!!!
endif

ifneq ($(strip $(BUILD_IPTABLES)),)
ifneq ($(strip $(BUILD_URLFILTER)),)
urlfilterd: sanity_check
	$(MAKE) -C urlfilterd $(BUILD_URLFILTER)
else
urlfilterd: sanity_check
endif
else
urlfilterd: sanity_check
	@echo Warning: To use urlfilterd, you need to enable iptables!!!!!
endif

ifneq ($(strip $(BUILD_IPV6)),)
radvd: sanity_check radvd.tar.bz2
	(tar xkjf radvd.tar.bz2 2> /dev/null || true)
	$(MAKE) -C radvd
else
radvd: sanity_check
	@echo "skipping radvd (not configured)"
endif

ifneq ($(strip $(BUILD_UDHCP)),)
udhcp: sanity_check
	$(MAKE) -C udhcp $(BUILD_UDHCP)
else
udhcp: sanity_check
	@echo "skipping udhcp (not configured)"
endif

ifneq ($(strip $(BUILD_ZEBRA)),)
zebra: sanity_check zebra.tar.bz2
	(tar xkfj zebra.tar.bz2 2> /dev/null || true)
	$(MAKE) -C zebra $(BUILD_ZEBRA)
else
zebra: sanity_check
	@echo "skipping zebra (not configured)"
endif

ifneq ($(strip $(BUILD_MTDUTILS)),)
mtd-utils: sanity_check mtd-utils.tar.gz
	mkdir -p $(INSTALL_DIR)/bin
	(tar xkfz mtd-utils.tar.gz 2> /dev/null || true)
	(cd mtd-utils; $(MAKE) install)
else
mtd-utils: sanity_check
	@echo "skipping mtd-utils (not configured)"
endif

ifneq ($(strip $(BUILD_NTFS_3G)),)
ntfs-3g: sanity_check ntfs-3g_ntfsprogs-2012.1.15.tgz
	(tar xkfz ntfs-3g_ntfsprogs-2012.1.15.tgz 2> /dev/null || true)
	if [ ! -f ./ntfs-3g_ntfsprogs-2012.1.15/Makefile ]; then \
	cd ./ntfs-3g_ntfsprogs-2012.1.15; ./configure --host=mips-linux \
				--target=mips-linux \
				--build=i386-linux-gnu \
				--prefix=$(APPS_STAGING_DIR); \
	fi
	$(MAKE) -C ./ntfs-3g_ntfsprogs-2012.1.15 
	install -m 755 ./ntfs-3g_ntfsprogs-2012.1.15/libntfs-3g/.libs/libntfs-3g.so.83 $(INSTALL_DIR)/lib/libntfs-3g.so.83
	install -m 755 ./ntfs-3g_ntfsprogs-2012.1.15/src/.libs/ntfs-3g  $(INSTALL_DIR)/bin/ntfs-3g	
	install -m 755 ./ntfs-3g_ntfsprogs-2012.1.15/ntfsprogs/.libs/ntfslabel  $(INSTALL_DIR)/bin/ntfslabel
else
ntfs-3g: sanity_check
	@echo "skipping $@ (not configured)"
endif

ifneq ($(strip $(BUILD_LIBDAEMON)),)
libdaemon: sanity_check libdaemon-$(LIBDAEMON_VERSION).tar.gz
	if [ ! -f libdaemon-$(LIBDAEMON_VERSION)/Makefile ]; then \
		tar xzf libdaemon-$(LIBDAEMON_VERSION).tar.gz 2> /dev/null; \
		export CFLAGS=-fPIC ; \
		export AUTOCONFIG_POSTFIX_EXTRAS=\
						ac_cv_func_setpgrp_void=yes; \
		cd libdaemon-$(LIBDAEMON_VERSION); \
		./configure	--host=mips-linux \
				--target=mips-linux \
				--build=i386-linux-gnu \
				--enable-static=no \
				--disable-lynx \
				--disable-examples \
				--prefix=$(APPS_STAGING_DIR); \
		cd ..; \
	fi
	$(MAKE) -C libdaemon-$(LIBDAEMON_VERSION) install-strip
	cp -P $(APPS_STAGING_DIR)/lib/libdaemon.* $(APPS_INSTALL_DIR)/lib
libdaemon-clean: 
	rm -f $(APPS_INSTALL_DIR)/lib/libdaemon.*
	if [ -f libdaemon-$(LIBDAEMON_VERSION)/Makefile ]; then \
		$(MAKE) -C libdaemon-$(LIBDAEMON_VERSION) distclean; \
	fi 
else
libdaemon: sanity_check
	@echo "skipping $@ (not configured)"
libdaemon-clean:
endif

ifneq ($(strip $(BUILD_EXPAT)),)
expat: sanity_check expat-$(EXPAT_VERSION).tar.gz
	if [ ! -f expat-$(EXPAT_VERSION)/Makefile ]; then \
		tar xzf expat-$(EXPAT_VERSION).tar.gz 2> /dev/null; \
		export CFLAGS=-fPIC ; \
		export PKG_CONFIG_PATH=$(APPS_STAGING_DIR)/lib/pkgconfig; \
		cd expat-$(EXPAT_VERSION); \
		./configure	--host=mips-linux \
				--target=mips-linux \
				--build=i386-linux-gnu \
				--enable-static=no \
				--prefix=$(APPS_STAGING_DIR); \
		cd ..; \
	fi
	$(MAKE) -C expat-$(EXPAT_VERSION) install
	(test -d $(APPS_INSTALL_DIR)/lib || mkdir -p $(APPS_INSTALL_DIR)/lib)
	cp -P $(APPS_STAGING_DIR)/lib/libexpat.* $(APPS_INSTALL_DIR)/lib
	$(STRIP) $(APPS_INSTALL_DIR)/lib/libexpat.so.?
expat-clean:
	rm -f $(APPS_INSTALL_DIR)/lib/libexpat.*
	if [ -f expat-$(EXPAT_VERSION)/Makefile ]; then \
		$(MAKE) -C expat-$(EXPAT_VERSION) distclean; \
	fi
else
expat: sanity_check
	@echo "skipping $@ (not configured)"
endif

ifneq ($(strip $(BUILD_DBUS)),)
dbus: sanity_check dbus-$(DBUS_VERSION).tar.gz
	if [ ! -f dbus-$(DBUS_VERSION)/Makefile ]; then \
		tar xzf dbus-$(DBUS_VERSION).tar.gz 2> /dev/null; \
		export CFLAGS='-fPIC -I$(APPS_STAGING_DIR)/include'; \
		export LDFLAGS=-L$(APPS_STAGING_DIR)/lib; \
#		export LIBXML_CFLAGS=-I$(CURR_DIR)/../../private/apps/dlna/src/x_dms/src/AppLibs/opensource/libxml2/libxml2-2.6.32/include; \
#		export LIBXML_LIBS='-L$(CURR_DIR)/../../private/apps/dlna/lib -lxml2'; \
#		export AUTOCONFIG_POSTFIX_EXTRAS=\
#						ac_cv_have_abstract_sockets="yes" \
#						ac_cv_lib_expat_XML_ParserCreate_MM="yes"; \
		cd dbus-$(DBUS_VERSION); \
		./configure	--host=mips-linux \
				--target=mips-linux \
				--build=i386-linux-gnu \
				--enable-static=no \
				--prefix=$(APPS_STAGING_DIR) \
				--disable-abstract-sockets \
				--disable-ansi \
				--disable-asserts \
				--disable-console-owner-file \
				--disable-doxygen-docs \
				--disable-compiler_coverage \
				--disable-selinux \
				--disable-verbose-mode \
				--disable-xml-docs \
				--with-xml=expat \
				--with-init-scripts=none \
				--with-system-pid-file=/usr/local/var/run/dbus/pid \
				--with-system-socket=/usr/local/var/run/dbus/socket \
				--with-dbus-user=root \
				--with-systemdsystemunitdir='' \
				--without-x \
				--disable-tests \
				--disable-installed-tests; \
		cd ..; \
	fi
	$(MAKE) -C dbus-$(DBUS_VERSION) install-strip
	cp -P $(APPS_STAGING_DIR)/lib/libdbus*.* $(APPS_INSTALL_DIR)/lib
	cp -P $(APPS_STAGING_DIR)/bin/dbus-daemon $(APPS_INSTALL_DIR)/bin
	cp -rP $(APPS_STAGING_DIR)/etc/dbus-1 $(APPS_INSTALL_DIR)/etc
	sed -i '/servicehelper/d' $(APPS_INSTALL_DIR)/etc/dbus-1/system.conf
	install -m 775 dbus-patch/dbus.init $(APPS_INSTALL_DIR)/etc/init.d/dbus
dbus-clean:
	rm -f  $(APPS_INSTALL_DIR)/lib/libdbus*.*
	rm -f  $(APPS_INSTALL_DIR)/bin/dbus-daemon
	rm -rf $(APPS_INSTALL_DIR)/etc/dbus-1
	rm -f  $(APPS_INSTALL_DIR)/etc/init.d/dbus 
	if [ -f dbus-$(DBUS_VERSION)/Makefile ]; then \
		$(MAKE) -C dbus-$(DBUS_VERSION) distclean; \
	fi
else
dbus: sanity_check
	@echo "skipping $@ (not configured)"
dbus-clean:
endif

ifneq ($(filter yes y, $(BUILD_AVAHI)), )
avahi: sanity_check avahi-$(AVAHI_VERSION).tar.gz
	if [ ! -f avahi-$(AVAHI_VERSION)/Makefile ]; then \
		tar xzf avahi-$(AVAHI_VERSION).tar.gz 2> /dev/null; \
		export CFLAGS='-fPIC -I$(APPS_STAGING_DIR)/include -UAVAHI_RUNTIME_DIR -DAVAHI_RUNTIME_DIR=\"/var/run\" -UAVAHI_IPCONF_SCRIPT -DAVAHI_IPCONF_SCRIPT=\"/usr/local/etc/avahi/avahi-autoipd.action\" -UAVAHI_IPDATA_DIR -DAVAHI_IPDATA_DIR=\"/usr/local/var/lib/avahi-autoipd\" -UAVAHI_DAEMON_RUNTIME_DIR -DAVAHI_DAEMON_RUNTIME_DIR=\"/var/run/avahi-daemon\" -UAVAHI_SOCKET -DAVAHI_SOCKET=\"/var/run/avahi-daemon/socket\" -UAVAHI_SERVICE_DIR -DAVAHI_SERVICE_DIR=\"/usr/local/etc/avahi/services\" -UAVAHI_CONFIG_FILE -DAVAHI_CONFIG_FILE=\"/usr/local/etc/avahi/avahi-daemon.conf\" -UAVAHI_HOSTS_FILE -DAVAHI_HOSTS_FILE=\"/usr/local/etc/avahi/hosts\" -UAVAHI_CONFIG_DIR -DAVAHI_CONFIG_DIR=\"/usr/local/etc/avahi\"'; \
		export LDFLAGS=-L$(APPS_STAGING_DIR)/lib; \
		export PKG_CONFIG_PATH=$(APPS_STAGING_DIR)/lib/pkgconfig; \
		export AUTOCONFIG_POSTFIX_EXTRAS=\
						ac_cv_func_chroot=no \
						ac_cv_header_sys_inotify_h=no; \
		cd avahi-$(AVAHI_VERSION); \
		sed -i 's/"0.35.0"/""/' configure; \
		./configure	--host=mips-linux \
				--target=mips-linux \
				--build=i386-linux-gnu \
				--enable-static=no \
				--prefix=$(APPS_STAGING_DIR) \
				--with-distro=none \
				--disable-qt3 \
				--disable-qt4 \
				--disable-gtk \
				--disable-gtk3 \
				--disable-python \
				--disable-pygtk \
				--disable-python-dbus \
				--disable-monodoc \
				--disable-doxygen-doc \
				--disable-doxygen-dot \
				--disable-doxygen-man \
				--disable-doxygen-rtf \
				--disable-doxygen-xml \
				--disable-doxygen-chm \
				--disable-doxygen-chi \
				--disable-doxygen-html \
				--disable-doxygen-ps \
				--disable-doxygen-pdf \
				--disable-xmltoman \
				--disable-manpages \
				--disable-mono \
				--disable-gdbm \
				--disable-glib \
				--disable-gobject \
				--disable-nls \
				--disable-autoipd \
				--enable-compat-libdns_sd \
				--with-xml=expat \
				--with-avahi-user=root \
				--with-avahi-group=root \
				--with-avahi-priv-access-group=root \
				--with-systemdsystemunitdir='' \
				--disable-stack-protector ; \
		cd ..; \
	fi
# use zcip instead of avahi-autoipd for zeroconf
#				--with-autoipd-user=root \
#				--with-autoipd-group=root \
	
#	cp avahi-patch/avahi-autoipd.action avahi-$(AVAHI_VERSION)/avahi-autoipd
	cp avahi-patch/avahi-daemon/*.c avahi-$(AVAHI_VERSION)/avahi-daemon
	
	$(MAKE) -C avahi-$(AVAHI_VERSION) install-strip
	cp -P $(APPS_STAGING_DIR)/lib/libavahi*.* $(APPS_INSTALL_DIR)/lib
	cp -P  $(APPS_STAGING_DIR)/bin/avahi-* $(APPS_INSTALL_DIR)/bin
	cp -P $(APPS_STAGING_DIR)/sbin/avahi* $(APPS_INSTALL_DIR)/sbin
	rm $(APPS_STAGING_DIR)/etc/avahi/services/*
	cp -rP $(APPS_STAGING_DIR)/etc/avahi $(APPS_INSTALL_DIR)/etc
	install -m 775 avahi-patch/avahi-daemon/router.service $(APPS_INSTALL_DIR)/etc/avahi/services
	install -m 775 avahi-patch/avahi-daemon/samba.service $(APPS_INSTALL_DIR)/etc/avahi/services
	install -m 775 avahi-patch/avahi-daemon/avahi-daemon.conf $(APPS_INSTALL_DIR)/etc/avahi/
	if [ -n "$(BUILD_NETATALK)" ]; then\
		install -m 775 avahi-patch/avahi-daemon/afpd.service $(APPS_INSTALL_DIR)/etc/avahi/services/; \
	fi
	install -m 775 avahi-patch/avahi.init $(APPS_INSTALL_DIR)/etc/init.d/avahi
	cp -rP $(APPS_STAGING_DIR)/etc/dbus-1 $(APPS_INSTALL_DIR)/etc


#	mkdir -p $(INSTALL_DIR)/etc/dhcp
#	cp -rP avahi-patch/avahi-autoipd.init $(INSTALL_DIR)/etc/dhcp/dhcpc.leasefail
avahi-clean:
	rm -f  $(APPS_INSTALL_DIR)/bin/avahi*
	rm -rf $(APPS_INSTALL_DIR)/etc/avahi
	rm -f  $(APPS_INSTALL_DIR)/etc/init.d/avahi
	rm -f  $(INSTALL_DIR)/etc/dhcp/dhcpc.leasefail  
	rm -f  $(APPS_INSTALL_DIR)/lib/libavahi*.*
	rm -f  $(APPS_INSTALL_DIR)/sbin/avahi*
	
	if [ -f avahi-$(AVAHI_VERSION)/Makefile ]; then \
		$(MAKE) -C avahi-$(AVAHI_VERSION) distclean; \
	fi
else
avahi: sanity_check
	@echo "skipping $@ (not configured)"
avahi-clean:
endif

ifneq ($(strip $(BUILD_HTTPD)),)
httpd: sanity_check
	cp -rP httpd/www $(APPS_INSTALL_DIR)/srv
	install -m 775 httpd/http.init $(APPS_INSTALL_DIR)/etc/init.d/http
	if [ ! -h $(APPS_INSTALL_DIR)/sbin/httpd ];  then ln -s /usr/sbin/httpd $(APPS_INSTALL_DIR)/sbin/httpd; fi
httpd-clean:
	rm -rf $(APPS_INSTALL_DIR)/etc/init.d/http
	rm -rf $(APPS_INSTALL_DIR)/sbin/httpd
	rm -rf $(APPS_INSTALL_DIR)/srv/*
else
httpd: sanity_check
	@echo "skipping $@ (not configured)"

endif

ifneq ($(strip $(BUILD_OPENSSL)),)
openssl: sanity_check openssl-$(OPENSSL_VERSION).tgz
	if [ ! -f ./openssl-$(OPENSSL_VERSION)/libcrypto.so.$(OPENSSL_SHLIB_VERSION) ]; then \
		tar xfz openssl-$(OPENSSL_VERSION).tgz ;\
		rm openssl-$(OPENSSL_VERSION)/Makefile ;\
		rm openssl-$(OPENSSL_VERSION)/Makefile.bak ;\
		cp openssl-patch/* openssl-$(OPENSSL_VERSION)/ ;\
	fi
	if [ ! -f openssl-$(OPENSSL_VERSION)/Makefile ]; then \
		(cd openssl-$(OPENSSL_VERSION) && ./Configure mips-linux) ;\
	fi
	if [ ! -f openssl-$(OPENSSL_VERSION)/libcrypto.so.$(OPENSSL_SHLIB_VERSION) ]; then \
		$(MAKE) -C ./openssl-$(OPENSSL_VERSION) ;\
		$(MAKE) -C ./openssl-$(OPENSSL_VERSION) build-shared ;\
	fi
	if [ ! -d openssl-$(OPENSSL_VERSION)/usr/local/ssl ]; then \
		$(MAKE) -C ./openssl-$(OPENSSL_VERSION) INSTALL_PREFIX=$$PWD/openssl-$(OPENSSL_VERSION) install ;\
	fi
	install -m 755 ./openssl-$(OPENSSL_VERSION)/libcrypto.so.$(OPENSSL_SHLIB_VERSION) $(INSTALL_DIR)/lib/libcrypto.so.$(OPENSSL_SHLIB_VERSION)
	$(STRIP) $(INSTALL_DIR)/lib/libcrypto.so.$(OPENSSL_SHLIB_VERSION)
	install -m 755 ./openssl-$(OPENSSL_VERSION)/libssl.so.$(OPENSSL_SHLIB_VERSION) $(INSTALL_DIR)/lib/libssl.so.$(OPENSSL_SHLIB_VERSION)
	$(STRIP) $(INSTALL_DIR)/lib/libssl.so.$(OPENSSL_SHLIB_VERSION)
	(cd $(INSTALL_DIR)/lib ;\
	test -f libcrypto.so || ln -s libcrypto.so.$(OPENSSL_SHLIB_VERSION) libcrypto.so ;\
	test -f libssl.so || ln -s libssl.so.$(OPENSSL_SHLIB_VERSION) libssl.so)
	mkdir -p $(APPS_STAGING_DIR)/include
	mkdir -p $(APPS_STAGING_DIR)/lib
	cp -a ./openssl-$(OPENSSL_VERSION)/usr/local/ssl/include/openssl $(APPS_STAGING_DIR)/include
	install -m 755 ./openssl-$(OPENSSL_VERSION)/libcrypto.so.$(OPENSSL_SHLIB_VERSION) $(APPS_STAGING_DIR)/lib/libcrypto.so.$(OPENSSL_SHLIB_VERSION)
	install -m 755 ./openssl-$(OPENSSL_VERSION)/libssl.so.$(OPENSSL_SHLIB_VERSION) $(APPS_STAGING_DIR)/lib/libssl.so.$(OPENSSL_SHLIB_VERSION)
	(cd $(APPS_STAGING_DIR)/lib ;\
	test -f libcrypto.so || ln -s libcrypto.so.$(OPENSSL_SHLIB_VERSION) libcrypto.so ;\
	test -f libssl.so || ln -s libssl.so.$(OPENSSL_SHLIB_VERSION) libssl.so)
	mkdir -p $(APPS_INSTALL_DIR)/bin/
	cp ./openssl-$(OPENSSL_VERSION)/usr/local/ssl/bin/openssl $(APPS_INSTALL_DIR)/bin/
	$(STRIP) $(APPS_INSTALL_DIR)/bin/openssl
	mkdir -p $(APPS_INSTALL_DIR)/ssl
	cp ./openssl-$(OPENSSL_VERSION)/usr/local/ssl/openssl.cnf $(APPS_INSTALL_DIR)/ssl
openssl-clean:
	rm -f $(APPS_INSTALL_DIR)/bin/openssl
	rm -f $(APPS_INSTALL_DIR)/ssl/openssl.cnf
else
openssl: sanity_check
	@echo "skipping $@ (not configured)"

endif

# original - ifneq ($(strip $(BUILD_CUPS)),no)
ifneq ($(filter yes y, $(BUILD_CUPS)), )
cups: sanity_check cups-$(CUPS_VERSION)-source.tar.bz2
	
	(test -d cups-native || tar xfj cups-$(CUPS_VERSION)-source.tar.bz2)
	(test -d cups-native || mv cups-$(CUPS_VERSION) cups-native)
	(test -f cups-native/makefile || cp cups-patch/makefile cups-native/makefile)
	(test -f cups-native/man/mantohtml || make -C cups-native native)
	(test -d cups-$(CUPS_VERSION) || tar xfj cups-$(CUPS_VERSION)-source.tar.bz2)
	(test -f cups-$(CUPS_VERSION)/makefile || cp -a cups-patch/* cups-$(CUPS_VERSION)/)
	(cd cups-$(CUPS_VERSION) && make)
	if [ ! -f cups-$(CUPS_VERSION)/usr/local/lib/libcups.so ]; then \
		(cd cups-$(CUPS_VERSION) && make install) ;\
	fi
	cp -f cups-patch/conf/cupsd.conf cups-$(CUPS_VERSION)/usr/local/etc/cups/
	cp -f cups-patch/conf/mime.convs cups-$(CUPS_VERSION)/usr/local/share/cups/mime/mime.convs
	cp -af cups-$(CUPS_VERSION)/usr/local/* $(APPS_STAGING_DIR)/
	cp -af cups-$(CUPS_VERSION)/usr/local/bin   $(APPS_INSTALL_DIR)
	cp -af cups-$(CUPS_VERSION)/usr/local/etc   $(APPS_INSTALL_DIR)
	cp -af cups-$(CUPS_VERSION)/usr/local/lib   $(APPS_INSTALL_DIR)
	chmod 755 $(APPS_INSTALL_DIR)/lib/libcups.so.?
	$(STRIP) $(APPS_INSTALL_DIR)/lib/libcups.so.?
	chmod 755 $(APPS_INSTALL_DIR)/lib/libcupsimage.so.?
	$(STRIP) $(APPS_INSTALL_DIR)/lib/libcupsimage.so.?
	chmod 755 $(APPS_INSTALL_DIR)/lib/libcupscgi.so.?
	$(STRIP) $(APPS_INSTALL_DIR)/lib/libcupscgi.so.?
	chmod 755 $(APPS_INSTALL_DIR)/lib/libcupsdriver.so.?
	$(STRIP) $(APPS_INSTALL_DIR)/lib/libcupsdriver.so.?
	chmod 755 $(APPS_INSTALL_DIR)/lib/libcupsppdc.so.?
	$(STRIP) $(APPS_INSTALL_DIR)/lib/libcupsppdc.so.?
	chmod 755 $(APPS_INSTALL_DIR)/lib/libcupsmime.so.?
	$(STRIP) $(APPS_INSTALL_DIR)/lib/libcupsmime.so.?
	cp -af cups-$(CUPS_VERSION)/usr/local/sbin  $(APPS_INSTALL_DIR)
	mkdir -p $(APPS_INSTALL_DIR)/share/doc/cups/images
	cp -af cups-$(CUPS_VERSION)/usr/local/share/doc/cups/*.css $(APPS_INSTALL_DIR)/share/doc/cups/
	cp -af cups-$(CUPS_VERSION)/usr/local/share/doc/cups/images $(APPS_INSTALL_DIR)/share/doc/cups/
	
	cp -af cups-$(CUPS_VERSION)/usr/local/share/cups $(APPS_INSTALL_DIR)/share/
	cp -af cups-patch/usr/local/share/cups/templates/* $(APPS_INSTALL_DIR)/share/cups/templates
	#cp -af cups-$(CUPS_VERSION)/usr/local/share $(APPS_INSTALL_DIR)
	cp -af cups-patch/usr/local/share/doc/cups/index.html $(APPS_INSTALL_DIR)/share/doc/cups/
	
	(test -L $(APPS_INSTALL_DIR)/var || ln -s /var $(APPS_INSTALL_DIR)/)
	install -m 775 cups-patch/cupsd.init $(APPS_INSTALL_DIR)/etc/init.d/cups
#	since we are not untarring samba but getting from version control, and we need to modify the config.h, make it writable
	if [ -f samba/source/include/config.h ]; then \
		chmod 777 samba/source/include/config.h; \
	fi
	cp -f samba/source/include/config.h.cups samba/source/include/config.h
	if [ -f $(APPS_INSTALL_DIR)/etc/sysconfig/services ]; then\
		sed -i s/CUPS=no/CUPS=yes/ $(APPS_INSTALL_DIR)/etc/sysconfig/services; \
	else \
		echo CUPS=yes > $(APPS_INSTALL_DIR)/etc/sysconfig/services; \
	fi

cups-clean:

	if [ -f cups-$(CUPS_VERSION)/Makefile ]; then\
		$(MAKE) -C cups-$(CUPS_VERSION) distclean; \
		$(MAKE) -C cups-native distclean; \
	fi
	rm -f $(APPS_INSTALL_DIR)/bin/cups*
	rm -f $(APPS_INSTALL_DIR)/bin/bdftops
	rm -f $(APPS_INSTALL_DIR)/bin/ipptool
	rm -f $(APPS_INSTALL_DIR)/bin/lp*
	rm -f $(APPS_INSTALL_DIR)/bin/wftopfa
	rm -rf $(APPS_INSTALL_DIR)/etc/cups
	rm -f $(APPS_INSTALL_DIR)/lib/libcups*
	rm -rf $(APPS_INSTALL_DIR)/lib/cups
	rm -f $(APPS_INSTALL_DIR)/sbin/cups*
	rm -rf $(APPS_INSTALL_DIR)/share/cups
	rm -rf $(APPS_INSTALL_DIR)/share/doc/cups
	rm -f $(APPS_INSTALL_DIR)/etc/init.d/cups
	rm -f $(APPS_INSTALL_DIR)/sbin/lp*
	rm -f $(APPS_INSTALL_DIR)/sbin/accept
	rm -f $(APPS_INSTALL_DIR)/sbin/reject
	rm -f $(APPS_INSTALL_DIR)/bin/cancel

else
cups: sanity_check 
	@echo "skipping $@ (not configured)"
#	since we are not untarring samba but getting from version control and we need to modify the config.h, make it writable
	if [ -f samba/source/include/config.h ]; then \
		chmod 777 samba/source/include/config.h; \
	fi
	cp samba/source/include/config.h.nocups samba/source/include/config.h
	if [ -f $(APPS_INSTALL_DIR)/etc/sysconfig/services ]; then\
		sed -i s/CUPS=no/CUPS=yes/ $(APPS_INSTALL_DIR)/etc/sysconfig/services; \
	else \
		echo CUPS=no > $(APPS_INSTALL_DIR)/etc/sysconfig/services; \
	fi


endif


ifneq ($(strip $(BUILD_JPEG)),)
jpeg: sanity_check jpegsrc.v$(JPEG_VERSION).tar.gz
	(test -d jpeg-$(JPEG_VERSION) || tar xfz jpegsrc.v$(JPEG_VERSION).tar.gz)
	(test -f jpeg-$(JPEG_VERSION)/makefile || cp -a jpeg-patch/* jpeg-$(JPEG_VERSION)/)
	(cd jpeg-$(JPEG_VERSION) && make)
	(cd jpeg-$(JPEG_VERSION) && make install)
else
jpeg: sanity_check 
	@echo "skipping $@ (not configured)"
endif

ifneq ($(strip $(BUILD_LIBUSB)),)
libusb: sanity_check libusb-$(LIBUSB_VERSION).tar.gz
	(test -d libusb-$(LIBUSB_VERSION) || tar xfz libusb-$(LIBUSB_VERSION).tar.gz)
	(test -f libusb-$(LIBUSB_VERSION)/makefile || cp -a libusb-patch/* libusb-$(LIBUSB_VERSION)/)
	(cd libusb-$(LIBUSB_VERSION) && make)
	(cd libusb-$(LIBUSB_VERSION) && make install)
	$(STRIP) $(STAGE_DIR)/lib/libusb*.so*
	cp -a $(STAGE_DIR)/lib/libusb*.so* $(APPS_INSTALL_DIR)/lib
libusb-clean:
	rm -f $(APPS_INSTALL_DIR)/lib/libusb*.so*
else
libusb: sanity_check 
	@echo "skipping $@ (not configured)"

endif 

ifneq ($(strip $(BUILD_ZLIB)),)
zlib: sanity_check zlib-$(ZLIB_VERSION).tar.gz
	(test -d zlib-$(ZLIB_VERSION) || tar xfz zlib-$(ZLIB_VERSION).tar.gz)
	(test -f zlib-$(ZLIB_VERSION)/makefile || cp -a zlib-patch/* zlib-$(ZLIB_VERSION)/)
	(cd zlib-$(ZLIB_VERSION) && make)
	(cd zlib-$(ZLIB_VERSION) && make install)
else
zlib: sanity_check 
	@echo "skipping $@ (not configured)"
endif

ifneq ($(strip $(BUILD_LIBPNG)),)
libpng: sanity_check libpng-$(LIBPNG_VERSION).tar.gz
	(test -d libpng-$(LIBPNG_VERSION) || tar xfz libpng-$(LIBPNG_VERSION).tar.gz)
	(test -f libpng-$(LIBPNG_VERSION)/makefile || cp -a libpng-patch/* libpng-$(LIBPNG_VERSION)/)
	if [ ! -f libpng-$(LIBPNG_VERSION)/.install1 ]; then \
		(cd libpng-$(LIBPNG_VERSION) && make)	;\
		(cd libpng-$(LIBPNG_VERSION) && make install)	;\
		touch libpng-$(LIBPNG_VERSION)/.install1	;\
	fi
else
libpng: sanity_check 
	@echo "skipping $@ (not configured)"
endif

ifneq ($(strip $(BUILD_GNU_GHOSTSCRIPT)),)
gnu-ghostscript: sanity_check gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION).tar.bz2
	(test -d gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION) || tar xfj gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION).tar.bz2)
	(test -f gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/makefile || cp -a gnu-ghostscript-patch/* gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION))
#	if [ ! -f gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/.install1 ]; then \
#		(cd gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION) && make config) ;\
#		(cd gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION) && make install) ;\
#		touch gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/.install1 ;\
#	fi
	if [ ! -f gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/bin/gs ]; then \
		(cd gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION) && make configlocal) ;\
		cp -a gnu-ghostscript-patch/obj gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/ ;\
		touch -t 203001010000 gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/obj/* ;\
		(cd gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION) && make) ;\
		cp -f gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/bin/gs $(APPS_STAGING_DIR)/bin ;\
		cp -f gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/cups/pstopxl $(APPS_STAGING_DIR)/lib/cups/filter/ ;\
		cp -f gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/cups/pstoraster $(APPS_STAGING_DIR)/lib/cups/filter/ ;\
		(cd gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION) && make installocal) ;\
	fi
	if [ gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/.install1 ]; then \
		cp -a gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/usr/local/bin $(APPS_INSTALL_DIR) ;\
		cp -f $(APPS_STAGING_DIR)/bin/gs $(APPS_INSTALL_DIR)/bin ;\
		$(STRIP) $(APPS_INSTALL_DIR)/bin/gs ;\
		cp -a gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/usr/local/lib $(APPS_INSTALL_DIR) ;\
		cp -a gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/usr/local/etc $(APPS_INSTALL_DIR) ;\
		cp -a gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/usr/local/share $(APPS_INSTALL_DIR) ;\
		rm -rf $(APPS_INSTALL_DIR)/share/man ;\
		rm -rf $(APPS_INSTALL_DIR)/share/ghostscript/*/doc ;\
		rm -rf $(APPS_INSTALL_DIR)/share/ghostscript/*/examples ;\
		rm -rf $(APPS_INSTALL_DIR)/share/ghostscript/*/lib ;\
		cp -f $(APPS_STAGING_DIR)/lib/cups/filter/pstopxl $(APPS_INSTALL_DIR)/lib/cups/filter/ ;\
		cp -f $(APPS_STAGING_DIR)/lib/cups/filter/pstoraster $(APPS_INSTALL_DIR)/lib/cups/filter/ ;\
	fi
gnu-ghostscript-clean:
	if [ -f gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION)/Makefile ]; then \
		$(MAKE) -C gnu-ghostscript-$(GNU_GHOSTSCRIPT_VERSION) distclean; \
	fi
	rm -f $(APPS_INSTALL_DIR)/bin/gs*
	rm -f $(APPS_INSTALL_DIR)/bin/dvipdf
	rm -f $(APPS_INSTALL_DIR)/bin/eps2eps
	rm -f $(APPS_INSTALL_DIR)/bin/fixmswrd.pl
	rm -f $(APPS_INSTALL_DIR)/bin/font2c
	rm -f $(APPS_INSTALL_DIR)/bin/pdf*
	rm -f $(APPS_INSTALL_DIR)/bin/pf*
	rm -f $(APPS_INSTALL_DIR)/bin/pp*
	rm -f $(APPS_INSTALL_DIR)/bin/printafm
	rm -f $(APPS_INSTALL_DIR)/bin/ps2*
	rm -f $(APPS_INSTALL_DIR)/bin/pv.sh
	rm -f $(APPS_INSTALL_DIR)/bin/unix-lpr.sh
	rm -f $(APPS_INSTALL_DIR)/bin/dumphint
	rm -rf $(APPS_INSTALL_DIR)/share/ghostscript
	rm -rf $(APPS_INSTALL_DIR)/share/local
	rm -rf $(APPS_INSTALL_DIR)/share/ghostscript
	rm -rf $(APPS_INSTALL_DIR)/share/ghostscript
else
gnu-ghostscript: sanity_check 
	@echo "skipping $@ (not configured)"

endif

ifneq ($(strip $(BUILD_FOOMATIC_FILTERS)),)
foomatic-filters: sanity_check foomatic-filters-$(FOOMATIC_FILTERS_VERSION).tar.gz
	(test -d foomatic-filters-$(FOOMATIC_FILTERS_VERSION) || tar xfz foomatic-filters-$(FOOMATIC_FILTERS_VERSION).tar.gz)
	(test -f foomatic-filters-$(FOOMATIC_FILTERS_VERSION)/makefile || cp -a foomatic-filters-patch/* foomatic-filters-$(FOOMATIC_FILTERS_VERSION)/)
	(cd foomatic-filters-$(FOOMATIC_FILTERS_VERSION) && make)
	(cd foomatic-filters-$(FOOMATIC_FILTERS_VERSION) && make install)
else
foomatic-filters: sanity_check 
	@echo "skipping $@ (not configured)"
endif

ifneq ($(strip $(BUILD_HPLIP)),)
hplip: sanity_check hplip-$(HPLIP_VERSION).run
	(test -d hplip-$(HPLIP_VERSION) || ./hplip-$(HPLIP_VERSION).run --nox11 --noexec)
	(test -f hplip-$(HPLIP_VERSION)/makefile || cp -a hplip-patch/* hplip-$(HPLIP_VERSION)/)
	(cd hplip-$(HPLIP_VERSION) && make)
	(cd hplip-$(HPLIP_VERSION) && make install)
	mkdir -p $(APPS_INSTALL_DIR)/share/cups/drv
	mkdir -p $(APPS_INSTALL_DIR)/share/ppd
	cp -a $(APPS_STAGING_DIR)/share/cups/drv/hp $(APPS_INSTALL_DIR)/share/cups/drv
	#cp -a $(APPS_STAGING_DIR)/share/ppd/HP $(APPS_INSTALL_DIR)/share/ppd
	cp $(APPS_STAGING_DIR)/lib/cups/filter/hpcups $(APPS_INSTALL_DIR)/lib/cups/filter/
	$(STRIP) $(APPS_INSTALL_DIR)/lib/cups/filter/hpcups
hplip-clean:
	if [ -f hplip-$(HPLIP_VERSION)/Makefile ]; then \
		$(MAKE) -C hplip-$(HPLIP_VERSION) distclean; \
	fi

	rm -rf $(APPS_INSTALL_DIR)/share/cups/drv/
	rm -rf $(APPS_STAGING_DIR)/lib/cups/filter/
	rm -rf $(APPS_INSTALL_DIR)/share/cups/drv
	rm -rf $(APPS_INSTALL_DIR)/share/ppd
else
hplip: sanity_check 
	@echo "skipping $@ (not configured)"

endif

ifneq ($(strip $(BUILD_SAMBA)),)
samba: sanity_check
#	(tar xkfz samba.tar.gz 2> /dev/null || true)	
#	since we are not untarring samba but getting from version control and we need to modify the config.h, make it writable
	if [ -f samba/source/include/config.h ]; then \
		chmod 777 samba/source/include/config.h; \
	fi
	if [ "$(BUILD_CUPS)" == "y" ] || [ "$(BUILD_CUPS)" == "yes" ] ; then \
		export PRINT_LIBS="-lcups -lssl -lcrypto -lz -lpthread -lcrypt -lpng -ldbus-1 $(AVAHI_LIBS)";\
		$(MAKE) -C ./samba; \
	else \
		cp samba/source/include/config.h.nocups samba/source/include/config.h; \
		$(MAKE) -C ./samba; \
	fi	
else
samba: sanity_check 
#	since we are not untarring samba but getting from version control and we need to modify the config.h, make it writable
	if [ -f samba/source/include/config.h ]; then \
		chmod 777 samba/source/include/config.h; \
	fi
	@echo "skipping $@ (not configured)"
endif

ifneq ($(strip $(BUILD_L2TPAC)),)
openl2tpd: sanity_check openl2tpd.tar.bz2
	(tar xkjf openl2tpd.tar.bz2 2> /dev/null || true)
	$(MAKE) -C openl2tpd
else
openl2tpd: sanity_check
	@echo "skipping openl2tpd (not configured)"
endif

ifneq ($(strip $(BUILD_RNGD)),)
rng-tools-2: sanity_check rng-tools-2.tar.gz
	mkdir -p $(INSTALL_DIR)/bin
	(tar xkfz rng-tools-2.tar.gz 2> /dev/null || true)
	($(MAKE) -C rng-tools-2 $(BUILD_RNGD))
else
rngd: sanity_check
	@echo "skipping rng-tools-2 (not configured)"
endif

portmap: sanity_check
	mkdir -p $(INSTALL_DIR)/bin
	$(MAKE) -C portmap install
	$(STRIP) $(INSTALL_DIR)/bin/portmap

ifneq ($(strip $(BUILD_SQLITE)),)
sqlite: sanity_check sqlite-$(SQLITE_VERSION).tar.gz
	if [ ! -f sqlite-$(SQLITE_VERSION)/Makefile ]; then \
		tar xzf sqlite-$(SQLITE_VERSION).tar.gz 2> /dev/null; \
		export CFLAGS='-I$(APPS_STAGING_DIR)/include'; \
		export LDFLAGS=-L$(APPS_STAGING_DIR)/lib; \
		export PKG_CONFIG_PATH=$(APPS_STAGING_DIR)/lib/pkgconfig; \
		cd sqlite-$(SQLITE_VERSION); \
		./configure	--host=mips-linux \
				--target=mips-linux \
				--build=i386-linux-gnu \
				--disable-static \
				--prefix=$(APPS_STAGING_DIR); \
		cd ..; \
	fi
	$(MAKE) -C sqlite-$(SQLITE_VERSION) install
else
sqlite: sanity_check 
	@echo "skipping $@ (not configured)"
endif

ifneq ($(strip $(BUILD_LIBID3TAG)),)
libid3tag: sanity_check libid3tag-$(LIBID3TAG_VERSION).tar.gz
	if [ ! -f libid3tag-$(LIBID3TAG_VERSION)/Makefile ]; then \
		tar xzf libid3tag-$(LIBID3TAG_VERSION).tar.gz 2> /dev/null; \
		export CFLAGS='-I$(APPS_STAGING_DIR)/include'; \
		export LDFLAGS=-L$(APPS_STAGING_DIR)/lib; \
		export PKG_CONFIG_PATH=$(APPS_STAGING_DIR)/lib/pkgconfig; \
		cd libid3tag-$(LIBID3TAG_VERSION); \
		./configure	--host=mips-linux \
				--target=mips-linux \
				--build=i386-linux-gnu \
				--disable-static \
				--prefix=$(APPS_STAGING_DIR); \
		cd ..; \
	fi
	$(MAKE) -C libid3tag-$(LIBID3TAG_VERSION) install
	cp -P $(APPS_STAGING_DIR)/lib/libid3tag.* $(APPS_INSTALL_DIR)/lib
	$(STRIP) $(APPS_INSTALL_DIR)/lib/libid3tag.so.?
libid3tag-clean:
	if [ -f libid3tag-$(LIBID3TAG_VERSION)/Makefile ]; then \
		$(MAKE) -C libid3tag-$(LIBID3TAG_VERSION) distclean; \
	fi
	rm -f $(APPS_INSTALL_DIR)/lib/libid3tag.*
else
libid3tag: sanity_check
	@echo "skipping $@ (not configured)"
libid3tag-clean:

endif

ifneq ($(filter yes y, $(BUILD_MT_DAAPD)), )
MT_DAAPD_AVAHI_OPT = $(patsubst ,no,$(patsubst n,no,$(patsubst y,yes,$(strip $(BUILD_AVAHI)))))
MT_DAAPD_MDNS_OPT = $(patsubst yes,=no,$(patsubst no,,$(patsubst n,no,$(patsubst y,yes,$(strip $(BUILD_AVAHI))))))
mt-daapd: sanity_check mt-daapd-$(MT_DAAPD_VERSION).tar.gz
	if [ ! -f mt-daapd-$(MT_DAAPD_VERSION)/Makefile ]; then \
		tar xzf mt-daapd-$(MT_DAAPD_VERSION).tar.gz 2> /dev/null; \
		export CFLAGS='-Os -fPIC -I$(APPS_STAGING_DIR)/include'; \
		export LDFLAGS='-L$(APPS_STAGING_DIR)/lib -L$(INSTALL_DIR)/lib'; \
		export PKG_CONFIG_PATH=$(APPS_STAGING_DIR)/lib/pkgconfig; \
		export AUTOCONFIG_POSTFIX_EXTRAS=\
						ac_cv_func_setpgrp_void=yes; \
		export LIBS=-ldbus-1; \
		cd mt-daapd-$(MT_DAAPD_VERSION); \
		./configure	--host=mips-linux \
				--target=mips-linux \
				--build=i386-linux-gnu \
				--enable-static=no \
				--prefix=$(APPS_STAGING_DIR) \
				--enable-sqlite3 \
				--enable-avahi=$(strip $(MT_DAAPD_AVAHI_OPT)) \
				--enable-mdns$(strip $(MT_DAAPD_MDNS_OPT)); \
		cd ..; \
	fi
	cp -rP mt-daapd-patch/src/ mt-daapd-$(MT_DAAPD_VERSION)/
	$(MAKE) -C mt-daapd-$(MT_DAAPD_VERSION) install-strip
	cp -P $(APPS_STAGING_DIR)/sbin/mt-daapd $(APPS_INSTALL_DIR)/sbin
#	cp -P $(APPS_STAGING_DIR)/bin/mt-daapd-ssc.sh $(APPS_INSTALL_DIR)/bin 
	cp -rP $(APPS_STAGING_DIR)/share/mt-daapd $(APPS_INSTALL_DIR)/share
	mkdir -p $(APPS_INSTALL_DIR)/lib/mt-daapd/plugins 
	cp -P mt-daapd-$(MT_DAAPD_VERSION)/src/plugins/.libs/*.so $(APPS_INSTALL_DIR)/lib/mt-daapd/plugins
	cp -rP mt-daapd-patch/admin-root $(APPS_INSTALL_DIR)/share/mt-daapd
	cp -P mt-daapd-patch/contrib/mt-daapd.conf $(APPS_INSTALL_DIR)/etc
	install -m 775 mt-daapd-patch/mt-daapd.init $(APPS_INSTALL_DIR)/etc/init.d/mt-daap

mt-daapd-clean:
	# distclean on mt-daapd directory (staging area cleaning)
	if [ -f mt-daapd-$(MT_DAAPD_VERSION)/Makefile ]; then \
		$(MAKE) -C mt-daapd-$(MT_DAAPD_VERSION) distclean; \
	fi

	rm -f  $(APPS_INSTALL_DIR)/sbin/mt-daapd
	rm -f  $(APPS_INSTALL_DIR)/bin/mt-daapd-ssc.sh
	rm -rf $(APPS_INSTALL_DIR)/lib/mt-daapd/
	#rm -rf $(APPS_INSTALL_DIR)/lib/mt-daapd/plugins 
	rm -rf $(APPS_INSTALL_DIR)/share/mt-daapd
	rm -f  $(APPS_INSTALL_DIR)/etc/mt-daapd.conf
	rm -f  $(APPS_INSTALL_DIR)/etc/init.d/mt-daap
	rm -rf $(APPS_INSTALL_DIR)/share/admin-root/

else
mt-daapd: sanity_check
	@echo "skipping $@ (not configured)"
mt-daapd-clean:
endif

ifneq ($(strip $(BUILD_E2FS_PROGS)),)
e2fsprogs: sanity_check e2fsprogs-$(E2FS_PROGS_VERSION).tar.gz
	if [ ! -f e2fsprogs-$(E2FS_PROGS_VERSION)/Makefile ]; then \
		(tar xkfz e2fsprogs-$(E2FS_PROGS_VERSION).tar.gz 2> /dev/null || true); \
		cd e2fsprogs-$(E2FS_PROGS_VERSION); \
		./configure	--host=mips-linux \
				--target=mips-linux \
				--build=i386-linux-gnu \
				--prefix=$(APPS_STAGING_DIR); \
		cd ..; \
	fi
	$(MAKE) -C e2fsprogs-$(E2FS_PROGS_VERSION)
	mkdir -p $(APPS_STAGING_DIR)/lib
	mkdir -p $(APPS_STAGING_DIR)/include/blkid
	cp ./e2fsprogs-$(E2FS_PROGS_VERSION)/lib/blkid/*.h $(APPS_STAGING_DIR)/include/blkid/
	cp ./e2fsprogs-$(E2FS_PROGS_VERSION)/lib/*.a $(APPS_STAGING_DIR)/lib
	cp -f ./e2fsprogs-$(E2FS_PROGS_VERSION)/misc/blkid $(APPS_INSTALL_DIR)/bin/blkid
	$(STRIP) $(APPS_INSTALL_DIR)/bin/blkid
else
e2fsprogs: sanity_check
	  @echo "skipping $@ (not configured)"
endif


ifneq ($(filter yes y, $(BUILD_NETATALK)), )
NETATALK_AVAHI_OPT = $(patsubst yes,--enable-zeroconf=$(APPS_STAGING_DIR),$(patsubst y,yes,$(patsubst no,,$(patsubst n,no,$(strip $(BUILD_AVAHI))))))
netatalk: sanity_check db-$(BERKELEYDB_VERSION).tar.gz netatalk-$(NETATALK_VERSION).tar.bz2
	if [ ! -d db-$(BERKELEYDB_VERSION) ]; then \
		tar xzf db-$(BERKELEYDB_VERSION).tar.gz 2> /dev/null; \
	fi
	if [ ! -f db-$(BERKELEYDB_VERSION)/build_unix/Makefile ]; then \
		export CFLAGS='-Os -fPIC -I$(APPS_STAGING_DIR)/include'; \
		export LDFLAGS=-L$(APPS_STAGING_DIR)/lib; \
		export PKG_CONFIG_PATH=$(APPS_STAGING_DIR)/lib/pkgconfig; \
		export AUTOCONFIG_POSTFIX_EXTRAS=\
						ac_cv_func_chroot=no \
						ac_cv_header_sys_inotify_h=no; \
		cd db-$(BERKELEYDB_VERSION)/dist; \
		cd ../build_unix; \
		../dist/configure	--host=mips-linux \
					--target=mips-linux \
					--build=i386-linux-gnu \
					--enable-static=no \
					--prefix=/usr/local \
					--with-mutex=MIPS/gcc-assembly; \
		cd ../..; \
	fi
	$(MAKE) -C db-$(BERKELEYDB_VERSION)/build_unix
	$(MAKE) -C db-$(BERKELEYDB_VERSION)/build_unix prefix=$(APPS_STAGING_DIR) install
	#install -p $(APPS_STAGING_DIR)/lib/libdb-*.so $(APPS_INSTALL_DIR)/lib
	install -p $(APPS_STAGING_DIR)/lib/libdb*.so $(APPS_INSTALL_DIR)/lib
	#rm $(APPS_INSTALL_DIR)/lib/libdb-5.so
	#$(STRIP) $(APPS_INSTALL_DIR)/lib/libdb-*.so
	$(STRIP) $(APPS_INSTALL_DIR)/lib/libdb-5.3.so
	
	if [ ! -d netatalk-$(NETATALK_VERSION) ]; then \
		tar xjf netatalk-$(NETATALK_VERSION).tar.bz2 2> /dev/null; \
	fi
	if [ ! -f netatalk-$(NETATALK_VERSION)/Makefile ]; then \
		export CFLAGS='-Os -fPIC -I$(APPS_STAGING_DIR)/include -Wl,-rpath-link=$(APPS_STAGING_DIR)/lib'; \
		export CPPFLAGS='-I$(APPS_STAGING_DIR)/include'; \
		export LDFLAGS=-L$(APPS_STAGING_DIR)/lib; \
		export PKG_CONFIG_PATH=$(APPS_STAGING_DIR)/lib/pkgconfig; \
		export AUTOCONFIG_POSTFIX_EXTRAS='ac_cv_func_chroot=no ac_cv_header_sys_inotify_h=no'; \
		export AVAHI_CFLAGS=; \
		export AVAHI_LIBS='-lavahi-client -lavahi-common -lavahi-core -ldbus-1'; \
		cd netatalk-$(NETATALK_VERSION); \
		./configure	--host=mips-linux \
				--target=mips-linux \
				--build=i386-linux-gnu \
				--enable-static=no \
				--disable-afs \
				--disable-ddp \
				--with-bdb=$(APPS_STAGING_DIR) \
				--with-libgcrypt-dir=no \
				--with-ssl-dir=no \
				--program-prefix= \
				--enable-overwrite \
				$(strip $(NETATALK_AVAHI_OPT)) \
		cd ..; \
	fi
	cp netatalk-patch/libatalk/dsi/dsi_tcp.c netatalk-$(NETATALK_VERSION)/libatalk/dsi
	cp netatalk-patch/libatalk/cnid/dbd/cnid_dbd.c netatalk-$(NETATALK_VERSION)/libatalk/cnid/dbd
	$(MAKE) -C netatalk-$(NETATALK_VERSION)
	$(MAKE) -C netatalk-$(NETATALK_VERSION) prefix=$(APPS_STAGING_DIR) install

	# You can replace the entire uncommented section with the two commented lines below  
	#cp -R $(APPS_STAGING_DIR)/etc/netatalk $(APPS_INSTALL_DIR)/etc/
	#rm $(APPS_INSTALL_DIR)/etc/netatalk/uams/*.la
	mkdir -p $(APPS_INSTALL_DIR)/etc/netatalk
	cp -P $(APPS_STAGING_DIR)/etc/netatalk/afpd.conf $(APPS_INSTALL_DIR)/etc/netatalk
	cp -P $(APPS_STAGING_DIR)/etc/netatalk/AppleVolumes.default $(APPS_INSTALL_DIR)/etc/netatalk
	cp -P $(APPS_STAGING_DIR)/etc/netatalk/AppleVolumes.system $(APPS_INSTALL_DIR)/etc/netatalk
	cp -P $(APPS_STAGING_DIR)/etc/netatalk/netatalk.conf $(APPS_INSTALL_DIR)/etc/netatalk
	mkdir -p $(APPS_INSTALL_DIR)/etc/netatalk/uams
	cp -P $(APPS_STAGING_DIR)/etc/netatalk/uams/uams_guest.so $(APPS_INSTALL_DIR)/etc/netatalk/uams
	cp -P $(APPS_STAGING_DIR)/etc/netatalk/uams/uams_passwd.so $(APPS_INSTALL_DIR)/etc/netatalk/uams
	(cd $(APPS_INSTALL_DIR)/etc/netatalk/uams ;\
	test -L uams_clrtxt.so || ln -s uams_passwd.so uams_clrtxt.so)
	
	cp -P $(APPS_STAGING_DIR)/sbin/afpd $(APPS_INSTALL_DIR)/sbin/
	cp -P $(APPS_STAGING_DIR)/sbin/cnid_dbd $(APPS_INSTALL_DIR)/sbin/
	cp -P $(APPS_STAGING_DIR)/sbin/cnid_metad $(APPS_INSTALL_DIR)/sbin/
	install -p netatalk-patch/config/netatalk $(APPS_INSTALL_DIR)/etc/init.d/netatalk
	install -p netatalk-patch/config/afpd.conf $(APPS_INSTALL_DIR)/etc/netatalk/afpd.conf
	install -p netatalk-patch/config/netatalk.conf $(APPS_INSTALL_DIR)/etc/netatalk/netatalk.conf
	install -p netatalk-patch/config/AppleVolumes.default $(APPS_INSTALL_DIR)/etc/netatalk/AppleVolumes.default
netatalk-clean:
	# distclean on netatalk + berkeleydb directory (staging area cleaning)
	if [ -f db-$(BERKELEYDB_VERSION)/Makefile ]; then \
		$(MAKE) -C db-$(BERKELEYDB_VERSION) distclean; \
	fi

	if [ -f netatalk-$(NETATALK_VERSION)/Makefile ]; then \
		$(MAKE) -C netatalk-$(NETATALK_VERSION) distclean; \
	fi

	# remove files from fs.apps 
	rm -rf $(APPS_INSTALL_DIR)/etc/netatalk/
	rm -f $(APPS_INSTALL_DIR)/etc/init.d/netatalk
	rm -f $(APPS_INSTALL_DIR)/lib/libdb-*
	rm -f $(APPS_INSTALL_DIR)/lib/libdb.*
	rm -f $(APPS_INSTALL_DIR)/sbin/afpd
	rm -f $(APPS_INSTALL_DIR)/sbin/cnid*
	
else
netatalk: sanity_check
	@echo "skipping $@ (not configured)"
netatalk-clean:
endif

ifneq ($(strip $(BUILD_SMARTMONTOOLS)),)
smartmontools: sanity_check
	if [ ! -f smartmontools-5.42/Makefile ]; then \
		tar xzf smartmontools-5.42.tar.gz 2> /dev/null; \
		export CFLAGS='-Os -fPIC -I$(APPS_STAGING_DIR)/include'; \
        	export CXXFLAGS='-g -Os -Wall -W'; \
		export LDFLAGS=-L$(APPS_STAGING_DIR)/lib; \
		export PKG_CONFIG_PATH=$(APPS_STAGING_DIR)/lib/pkgconfig; \
		cd smartmontools-5.42; \
		./configure	--host=mips-linux \
				--target=mips-linux \
				--build=i386-linux-gnu \
				--enable-static=no; \
		cd ..; \
	fi
	$(MAKE) -C smartmontools-5.42 DESTDIR=$(STAGE_ROOT_DIR) install-strip
	(test -d $(APPS_STAGING_DIR)/etc/smart || mkdir -p $(APPS_STAGING_DIR)/etc/smart)
	cp -rP ./smartmontools-patch/smartcmd/*.sh $(APPS_STAGING_DIR)/etc/smart
	cp -P $(APPS_STAGING_DIR)/sbin/smartctl $(APPS_INSTALL_DIR)/sbin
	cp -rP $(APPS_STAGING_DIR)/etc/smart $(APPS_INSTALL_DIR)/etc

smartmontools-clean:
	rm -f  $(APPS_STAGING_DIR)/sbin/smart*
	rm -rf  $(APPS_STAGING_DIR)/etc/smart
	rm -f  $(APPS_INSTALL_DIR)/sbin/smartctl
	rm -rf  $(APPS_INSTALL_DIR)/etc/smart
	if [ -d $(STAGE_ROOT_DIR)/usr/local ]; then \
		$(MAKE) -C smartmontools-5.42 DESTDIR=$(STAGE_ROOT_DIR) uninstall; \
	fi
	if [ -f smartmontools-5.42/Makefile ]; then \
		$(MAKE) -C smartmontools-5.42 DESTDIR=$(STAGE_ROOT_DIR) distclean; \
	fi
else
smartmontools: sanity_check
	@echo "skipping $@ (not configured)"
smartmontools-clean:
endif

