AUTOMAKE_OPTIONS=foreign
SUBDIRS = es fr sv nl

man_MANS = faked.1 fakeroot.1

#another automake bug workaround
EXTRA_DIST = $(man_MANS)

dist-hook:
	cp -r $(srcdir)/po4a $(distdir)
	rm -rf `find $(distdir) -name .arch-ids`
	cd $(distdir) && po4a -k 0 --rm-backups --variable "srcdir=$(distdir)/" po4a/po4a.cfg

#all-recursive: po4a

#po4a:
#	@$(NORMAL_INSTALL)
#	po4a -k 0 --variable "srcdir=$(srcdir)/" $(srcdir)/po4a/po4a.cfg
#
#clean-recursive: po4a-clean
#
#po4a-clean:
#	@$(NORMAL_INSTALL)
#	po4a -k 0 --variable "srcdir=$(srcdir)/" \
#	   --rm-translations $(srcdir)/po4a/po4a.cfg
#
#.PHONY: po4a po4a-clean
