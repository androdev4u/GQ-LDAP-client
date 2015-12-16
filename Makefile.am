## Process this file with automake to produce Makefile.in

SUBDIRS=\
	data \
	icons \
	pixmaps \
	src \
	test \
	po \
	$(NULL)

intltool=\
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	$(NULL)
EXTRA_DIST=\
	gq.spec.in \
	$(intltool) \
	README.langpack \
	$(NULL)
DISTCLEANFILES=\
	$(intltool:.in=) \
	$(NULL)

dist-hook: gq.spec
	cp gq.spec $(distdir)
	rm -f $(distdir)/po/LINGUAS $(distdir)/po/*.po  # no po files, these go into the language pack
	echo "# Download a language pack to add other languages" > $(distdir)/po/LINGUAS

LANGPACK_DIST	=	COPYING AUTHORS README.langpack
LANGPACK_VERSION	=	@LANGPACK_VERSION@

LANGPACK	=	$(PACKAGE)-$(VERSION)-langpack-$(LANGPACK_VERSION)

langpack-dist:
	-rm -rf $(LANGPACK) && mkdir $(LANGPACK)
	ls -1d po po/*.po po/LINGUAS langpack $(LANGPACK_DIST) | cpio -p $(LANGPACK)
	GZIP=$(GZIP_ENV) tar chozf $(LANGPACK).tar.gz $(LANGPACK)
	-rm -rf $(LANGPACK) 

