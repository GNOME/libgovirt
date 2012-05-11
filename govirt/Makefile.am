INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(REST_CFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/govirt					\
	-DG_LOG_DOMAIN=\"libgovirt\"				\
	-DTESTDATADIR=\""$(top_srcdir)/data/tests"\"		\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"

lib_LTLIBRARIES =						\
	libgovirt.la

libgovirt_ladir = $(includedir)/govirt-1/govirt
libgovirt_la_HEADERS =						\
	ovirt-rest-call.h

libgovirt_la_SOURCES =						\
	ovirt-rest-call.c

libgovirt_la_LIBADD =						\
	$(GLIB_LIBS)						\
	$(REST_LIBS)

libgovirt_la_DEPENDENCIES =					\
	govirt.sym

libgovirt_la_LDFLAGS =						\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-Wl,--version-script=$(srcdir)/govirt.sym

libgovirt_la_CFLAGS =						\
	$(WARNINGFLAGS_C)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = govirt.pc

EXTRA_DIST =							\
	govirt-version.h.in					\
	govirt.pc.in						\
	govirt.sym

CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
