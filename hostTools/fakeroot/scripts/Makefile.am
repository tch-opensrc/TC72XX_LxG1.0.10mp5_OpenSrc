AUTOMAKE_OPTIONS=foreign
bin_SCRIPTS=fakeroot
EXTRA_DIST = fakeroot.in
CLEANFILES=fakeroot

do_subst = sed -e 's,[@]prefix[@],$(prefix),g' \
	   -e 's,[@]bindir[@],$(bindir),g' \
	   -e 's,[@]libdir[@],$(libdir),g' \
	   -e 's,[@]fakeroot_transformed[@],'`echo fakeroot | sed -e '$(transform)'`',g' \
	   -e 's,[@]faked_transformed[@],'`echo faked | sed -e '$(transform)'`',g' \
	   -e 's,[@]signal[@],$(signal),g' \
	   -e 's,[@]SHELL[@],$(SHELL),g' \
	   -e 's,[@]VERSION[@],$(VERSION),g' \
	   -e 's,[@]DLSUFFIX[@],$(DLSUFFIX),g' \
	   -e 's,[@]LDLIBPATHVAR[@],$(LDLIBPATHVAR),g' \
	   -e 's,[@]LDPRELOADVAR[@],$(LDPRELOADVAR),g' \
	   -e 's,[@]LDPRELOADABS[@],$(LDPRELOADABS),g' \
	   -e 's,[@]LDEXTRAVAR[@],$(LDEXTRAVAR),g'

fakeroot: $(srcdir)/fakeroot.in Makefile
	$(do_subst) < $(srcdir)/fakeroot.in > fakeroot
	chmod +x fakeroot
