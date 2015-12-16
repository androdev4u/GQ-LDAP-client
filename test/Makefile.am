NULL=
AM_CPPFLAGS=\
	$(WARN_CFLAGS) \
	$(GQ_CFLAGS) \
	-I$(top_srcdir)/src \
	$(NULL)

#noinst_PROGRAMS=\
#	test-ldif \
#	test-schema \
#	$(NULL)
#TESTS=$(noinst_PROGRAMS)

test_ldif_SOURCES=\
	test-ldif.c \
	$(top_srcdir)/src/errorchain.c \
	$(top_srcdir)/src/ldif.c \
	$(top_srcdir)/src/util.c \
	ldif.h \
	$(NULL)
test_schema_SOURCES=\
	test-schema.c \
	$(NULL)

LDADD=$(GQ_LIBS)

