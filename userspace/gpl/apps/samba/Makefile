

all:
	$(MAKE) -C ./source
	install -m 755 ./source/bin/smbd $(INSTALL_DIR)/bin/smbd
	install -m 755 ./source/bin/nmbd $(INSTALL_DIR)/bin/nmbd
	install -m 755 ./source/bin/smbpasswd $(INSTALL_DIR)/bin/smbpasswd
	$(STRIP) $(INSTALL_DIR)/bin/smbd
	$(STRIP) $(INSTALL_DIR)/bin/nmbd
	$(STRIP) $(INSTALL_DIR)/bin/smbpasswd
	cp ./source/conf/smb.conf $(INSTALL_DIR)/etc
	cp ./source/conf/smbpasswd $(INSTALL_DIR)/etc

clean:
	if [ -f ./source/Makefile ];then $(MAKE) -C ./source clean;fi;

