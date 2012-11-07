noinst_PROGRAMS = sync-test

EXTRA_DIST =

sync_test_CFLAGS = \
		$(WARNINGFLAGS_C) \
		$(GLIB_CFLAGS) \
		$(LIBREST_CFLAGS) \
		-I$(top_srcdir)
sync_test_SOURCES = \
		sync-test.c
sync_test_LDADD = \
		../libvirt-glib/libvirt-glib-1.0.la \
		$(LIBVIRT_LIBS) \
		$(GLIB2_LIBS)
