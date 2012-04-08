NULL =

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
	govirt.h						\
	ovirt-proxy.h						\
	ovirt-rest-call.h					\
	ovirt-vm.h						\
	ovirt-vm-display.h					\
	$(NULL)

libgovirt_la_SOURCES =						\
	ovirt-proxy.c						\
	ovirt-rest-call.c					\
	ovirt-vm.c						\
	ovirt-vm-display.c					\
	$(NULL)

nodist_libgovirt_la_HEADERS =					\
	$(builddir)/ovirt-enum-types.h				\
	$(NULL)

nodist_libgovirt_la_SOURCES =					\
	$(builddir)/ovirt-enum-types.c				\
	$(NULL)

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

ovirt-enum-types.h: $(libgovirt_la_HEADERS) ovirt-enum-types.h.template
	$(AM_V_GEN) ( $(GLIB_MKENUMS) --template $(srcdir)/ovirt-enum-types.h.template $(libgovirt_la_HEADERS:%=$(srcdir)/%) ) \
	    > ovirt-enum-types.h

ovirt-enum-types.c: $(libgovirt_la_HEADERS) ovirt-enum-types.h ovirt-enum-types.c.template
	$(AM_V_GEN) ( $(GLIB_MKENUMS) --template $(srcdir)/ovirt-enum-types.c.template $(libgovirt_la_HEADERS:%=$(srcdir)/%) ) \
	    > ovirt-enum-types.c

BUILT_SOURCES =							\
	ovirt-enum-types.h					\
	ovirt-enum-types.c					\
	$(NULL)

EXTRA_DIST =							\
	govirt-version.h.in					\
	govirt.pc.in						\
	govirt.sym						\
	ovirt-enum-types.h.template				\
	ovirt-enum-types.c.template				\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
