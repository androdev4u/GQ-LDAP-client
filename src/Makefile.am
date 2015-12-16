## Process this file with automake to produce Makefile.in
OBJCLD=$(CCLD)
NULL=
AM_CPPFLAGS=\
	$(WARN_CFLAGS) \
	$(GQ_CFLAGS) \
	$(LIBGCRYPT_CFLAGS) \
	$(NULL)
if WITH_WERROR
AM_CPPFLAGS+=$(WERROR_CFLAGS)
endif

bin_PROGRAMS = gq

BUILT_SOURCES=COPYING.c
gq_SOURCES = \
	$(BUILT_SOURCES) \
	browse-dnd.c \
	browse-export.c \
	configfile.c \
	debug.c \
	dt_binary.c \
	dt_cert.c \
	dt_clist.c \
	dt_crl.c \
	dt_date.c \
	dt_entry.c \
	dt_generic_binary.c \
	dt_int.c \
	dt_jpeg.c \
	dt_numstr.c \
	dt_oc.c \
	dt_password.c \
	dt_text.c \
	dt_time.c \
	dtutil.c \
	encode.c \
	errorchain.c \
	filter.c \
	formfill.c \
        gq.c \
	gq-constants.h \
	gq-browser-model.c \
	gq-browser-model.h \
	gq-browser-node.c \
	gq-browser-node.h \
	gq-browser-node-dn.c \
	gq-browser-node-dn.h \
	gq-browser-node-reference.c \
	gq-browser-node-reference.h \
	gq-browser-node-server.c \
	gq-browser-node-server.h \
	gq-hash.c \
	gq-hash.h \
	gq-hash-gnutls.c \
	gq-hash-openssl.c \
	gq-keyring.h \
	gq-server.h \
	gq-server.c \
	gq-server-list.c \
	gq-server-list.h \
	gq-tab.c \
	gq-tab.h \
	gq-tab-browse.c \
	gq-tab-browse.h \
	gq-tab-schema.c \
	gq-tab-schema.h \
	gq-tab-search.c \
	gq-tab-search.h \
	gq-tree-widget.c \
	gq-tree-widget.h \
	gq-type-display.c \
	gq-type-display.h \
	gq-xml.c \
	iconv-helpers.h \
	input.c \
	ldapops.c \
	ldif.c \
        mainwin.c \
	prefs.c \
	progress.c \
	schema.c \
	state.c \
	syntax.c \
	tdefault.c \
	template.c \
	tinput.c \
	util.c \
	xmlparse.c \
	xmlutil.c \
	$(NULL)

if WITH_GNOME_KEYRING
gq_SOURCES+=gq-keyring.c
endif
if WITH_APPLE_KEYCHAIN
gq_SOURCES+=gq-keychain.m
endif

noinst_HEADERS = \
	mainwin.h \
	browse-export.h \
	schema.h \
	template.h \
	common.h \
	configfile.h \
	tinput.h \
	prefs.h \
	util.h \
	errorchain.h \
	ldif.h \
	formfill.h \
	input.h \
	encode.h \
	filter.h \
	debug.h \
	syntax.h \
	dtutil.h \
	dt_entry.h \
	dt_text.h \
	dt_generic_binary.h \
	dt_binary.h \
	dt_password.h \
	dt_oc.h \
	dt_jpeg.h \
	dt_clist.h \
	dt_cert.h \
	dt_crl.h \
	dt_time.h \
	dt_date.h \
	dt_int.h \
	dt_numstr.h \
	ldapops.h \
	tdefault.h \
	browse-dnd.h \
	xmlparse.h \
	gq-xml.h \
	state.h \
	xmlutil.h \
	progress.h \
	COPYING.h
gq_LDADD=\
	$(GQ_LIBS) \
	$(LIBGCRYPT_LIBS) \
	$(NULL)

COPYING.c: $(top_srcdir)/COPYING
	( echo "const char *license =" ; \
	  sed '/END OF TERMS AND CONDITIONS/,$$d' < $< | tr -cd "\t\n[:print:]" | \
	  sed -e 's/\(["\\]\)/\\\1/g' -e 's/^/    "/' -e 's/$$/\\n"/' ;\
	  echo ";" ) > $@

