NULL =

AM_CPPFLAGS = \
	$(GLIB_CFLAGS)						\
	$(REST_CFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/govirt					\
	-DG_LOG_DOMAIN=\"libgovirt\"				\
	-DTESTDATADIR=\""$(top_srcdir)/data/tests"\"		\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DLOCALEDIR="\"$(datadir)/locale\""			\
	$(NULL)

lib_LTLIBRARIES =						\
	libgovirt.la

libgovirt_ladir = $(includedir)/govirt-1.0/govirt
libgovirt_la_HEADERS =						\
	govirt.h						\
	ovirt-api.h						\
	ovirt-cdrom.h						\
	ovirt-collection.h					\
	ovirt-error.h						\
	ovirt-options.h						\
	ovirt-proxy.h						\
	ovirt-resource.h					\
	ovirt-rest-call-error.h					\
	ovirt-storage-domain.h					\
	ovirt-types.h						\
	ovirt-vm.h						\
	ovirt-vm-pool.h						\
	ovirt-vm-display.h					\
	$(NULL)

noinst_HEADERS =						\
	glib-compat.h						\
	govirt-private.h					\
	ovirt-action-rest-call.h				\
	ovirt-api-private.h					\
	ovirt-collection-private.h				\
	ovirt-proxy-private.h					\
	ovirt-resource-private.h				\
	ovirt-rest-call.h					\
	ovirt-resource-rest-call.h				\
	ovirt-storage-domain-private.h				\
	ovirt-utils.h						\
	ovirt-vm-private.h					\
	$(NULL)

libgovirt_la_SOURCES =						\
	glib-compat.c						\
	ovirt-action-rest-call.c				\
	ovirt-api.c						\
	ovirt-cdrom.c						\
	ovirt-collection.c					\
	ovirt-error.c						\
	ovirt-options.c						\
	ovirt-proxy.c						\
	ovirt-proxy-deprecated.c				\
	ovirt-resource.c					\
	ovirt-rest-call.c					\
	ovirt-resource-rest-call.c				\
	ovirt-storage-domain.c					\
	ovirt-utils.c						\
	ovirt-vm.c						\
	ovirt-vm-display.c					\
	ovirt-vm-xml.c						\
	ovirt-vm-pool.c						\
	$(NULL)

nodist_libgovirt_la_HEADERS =					\
	$(builddir)/ovirt-enum-types.h				\
	$(NULL)

nodist_libgovirt_la_SOURCES =					\
	$(builddir)/ovirt-enum-types.c				\
	$(builddir)/ovirt-enum-types-private.c			\
	$(builddir)/ovirt-enum-types-private.h			\
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
pkgconfig_DATA = govirt-1.0.pc

ovirt-enum-types.h: $(libgovirt_la_HEADERS) ovirt-enum-types.h.template
	$(AM_V_GEN) ( $(GLIB_MKENUMS) --template $(srcdir)/ovirt-enum-types.h.template $(libgovirt_la_HEADERS:%=$(srcdir)/%) ) \
	    > ovirt-enum-types.h

ovirt-enum-types.c: $(libgovirt_la_HEADERS) ovirt-enum-types.h ovirt-enum-types.c.template
	$(AM_V_GEN) ( $(GLIB_MKENUMS) --template $(srcdir)/ovirt-enum-types.c.template $(libgovirt_la_HEADERS:%=$(srcdir)/%) ) \
	    > ovirt-enum-types.c

ovirt-enum-types-private.h: $(noinst_HEADERS) ovirt-enum-types-private.h.template
	$(AM_V_GEN) ( $(GLIB_MKENUMS) --template $(srcdir)/ovirt-enum-types-private.h.template $(noinst_HEADERS:%=$(srcdir)/%) ) \
	    > ovirt-enum-types-private.h

ovirt-enum-types-private.c: $(noinst_HEADERS) ovirt-enum-types-private.h ovirt-enum-types-private.c.template
	$(AM_V_GEN) ( $(GLIB_MKENUMS) --template $(srcdir)/ovirt-enum-types-private.c.template $(noinst_HEADERS:%=$(srcdir)/%) ) \
	    > ovirt-enum-types-private.c

BUILT_SOURCES =							\
	ovirt-enum-types.h					\
	ovirt-enum-types.c					\
	ovirt-enum-types-private.h				\
	ovirt-enum-types-private.c				\
	$(NULL)

EXTRA_DIST =							\
	govirt-1.0.pc.in					\
	govirt.sym						\
	ovirt-enum-types.h.template				\
	ovirt-enum-types.c.template				\
	ovirt-enum-types-private.h.template			\
	ovirt-enum-types-private.c.template			\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -f *~

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS =					\
	--add-include-path=$(srcdir)				\
	--identifier-prefix=Ovirt				\
	--warn-all						\
	--c-include="govirt/govirt.h"				\
	$(NULL)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_sources = $(libgovirt_la_SOURCES) $(libgovirt_la_HEADERS) $(builddir)/ovirt-enum-types.c $(builddir)/ovirt-enum-types.h

GoVirt-1.0.gir: libgovirt.la Makefile
GoVirt_1_0_gir_INCLUDES = Gio-2.0 GObject-2.0 Rest-0.7
GoVirt_1_0_gir_CFLAGS = $(AM_CPPFLAGS) -I$(top_builddir)
GoVirt_1_0_gir_LIBS = libgovirt.la
GoVirt_1_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += GoVirt-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif # HAVE_INTROSPECTION

-include $(top_srcdir)/git.mk
