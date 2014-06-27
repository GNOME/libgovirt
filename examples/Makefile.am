noinst_PROGRAMS = async-test sync-test list-vms list-vms-async

EXTRA_DIST =

sync_test_CFLAGS = \
		$(WARNINGFLAGS_C) \
		$(GLIB_CFLAGS) \
		$(REST_CFLAGS) \
		-I$(top_srcdir)
sync_test_SOURCES = \
		sync-test.c
sync_test_LDADD = \
		../govirt/libgovirt.la \
		$(GLIB_LIBS) \
		$(REST_LIBS)

async_test_CFLAGS = \
		$(WARNINGFLAGS_C) \
		$(GLIB_CFLAGS) \
		$(REST_CFLAGS) \
		-I$(top_srcdir)
async_test_SOURCES = \
		async-test.c
async_test_LDADD = \
		../govirt/libgovirt.la \
		$(GLIB_LIBS) \
		$(REST_LIBS)

list_vms_CFLAGS = \
		$(WARNINGFLAGS_C) \
		$(GLIB_CFLAGS) \
		$(REST_CFLAGS) \
		-I$(top_srcdir)
list_vms_SOURCES = \
		list-vms.c
list_vms_LDADD = \
		../govirt/libgovirt.la \
		$(GLIB_LIBS) \
		$(REST_LIBS)

list_vms_async_CFLAGS = \
		$(WARNINGFLAGS_C) \
		$(GLIB_CFLAGS) \
		$(REST_CFLAGS) \
		-I$(top_srcdir)
list_vms_async_SOURCES = \
		list-vms-async.c
list_vms_async_LDADD = \
		../govirt/libgovirt.la \
		$(GLIB_LIBS) \
		$(REST_LIBS)

-include $(top_srcdir)/git.mk
