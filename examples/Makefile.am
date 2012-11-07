noinst_PROGRAMS = sync-test

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
