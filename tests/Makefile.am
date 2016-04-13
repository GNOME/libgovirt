TESTS = test-govirt
noinst_PROGRAMS = $(TESTS)

EXTRA_DIST =				\
	https-cert/ca-cert.pem		\
	https-cert/ca-key.pem		\
	https-cert/server-cert.pem	\
	https-cert/server-key.csr	\
	https-cert/server-key.pem	\
	$(NULL)

AM_LDFLAGS =					\
	$(top_builddir)/govirt/libgovirt.la	\
	$(GLIB_LIBS)


AM_CFLAGS =					\
	$(WARNINGFLAGS_C)			\
	$(GLIB_CFLAGS)				\
	$(REST_CFLAGS)				\
	-Dabs_srcdir="\"$(abs_srcdir)\""	\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/govirt

noinst_LTLIBRARIES = libmock-httpd.la
libmock_httpd_la_SOURCES = mock-httpd.c mock-httpd.h

test_govirt_LDADD = $(AM_LDFLAGS) libmock-httpd.la
