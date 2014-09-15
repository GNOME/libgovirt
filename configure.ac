# Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
# Copyright (C) 2011 Christophe Fergeau <cfergeau@redhat.com>
AC_PREREQ(2.63)

m4_define([govirt_major_version], [0])
m4_define([govirt_minor_version], [3])
m4_define([govirt_micro_version], [2])
m4_define([govirt_version],
          [govirt_major_version.govirt_minor_version.govirt_micro_version])

AC_INIT([libgovirt],[govirt_version])
AC_CONFIG_SRCDIR(govirt)
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz tar-ustar])

AM_SILENT_RULES([yes])

# set up gtk-doc
# GTK_DOC_CHECK(1.9)

# use this in cd-version.h
G_OVIRT_MAJOR_VERSION=govirt_major_version
G_OVIRT_MINOR_VERSION=govirt_minor_version
G_OVIRT_MICRO_VERSION=govirt_micro_version
AC_SUBST(G_OVIRT_MAJOR_VERSION)
AC_SUBST(G_OVIRT_MINOR_VERSION)
AC_SUBST(G_OVIRT_MICRO_VERSION)

# libtool versioning - this applies to libgovirt
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91
# for details
#
# increment;
# CURRENT	If the API or ABI interface has changed (reset REVISION to 0)
# REVISION	If the API and ABI remains the same, but bugs are fixed.
# AGE		Don't use.
LT_CURRENT=3
LT_REVISION=0
LT_AGE=1
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

# Check for programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

# i18 support
IT_PROG_INTLTOOL(0.40.0)

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[The name of the gettext domain])

dnl ---------------------------------------------------------------------------
dnl - Extra verbose warning switches
dnl ---------------------------------------------------------------------------

if test "$GCC" = "yes"; then
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wall"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align -Wno-uninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-declarations"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wpointer-arith"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wwrite-strings"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winit-self"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wreturn-type"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-nonliteral"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-security"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-include-dirs"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-format-attribute"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wclobbered"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wempty-body"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wignored-qualifiers"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wsign-compare"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wtype-limits"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wuninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Waggregate-return"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wdeclaration-after-statement"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wshadow"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wno-strict-aliasing"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winline"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-parameter-type"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Woverride-init"
else
	WARNINGFLAGS_C=""
fi
AC_SUBST(WARNINGFLAGS_C)

dnl ---------------------------------------------------------------------------
dnl - Check library dependencies
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES([GLIB], [gobject-2.0 gio-2.0 >= 2.26.0])

# Setup GLIB_MKENUMS to use glib-mkenums even if GLib is uninstalled.
GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
AC_SUBST(GLIB_MKENUMS)

GOBJECT_INTROSPECTION_CHECK([1.30.0])

dnl **** Check for librest ****
PKG_CHECK_MODULES([REST], [rest-0.7 >= 0.7.92])

dnl ---------------------------------------------------------------------------
dnl - Build self tests
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(tests, AS_HELP_STRING([--enable-tests],[enable unit test code]),
	      enable_tests=$enableval,enable_tests=yes)
AM_CONDITIONAL(GOVIRT_BUILD_TESTS, test x$enable_tests = xyes)

dnl ---------------------------------------------------------------------------
dnl - Makefiles, etc.
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES([
Makefile
examples/Makefile
govirt/govirt-1.0.pc
govirt/Makefile
po/Makefile.in
])
AC_OUTPUT

dnl ==========================================================================
echo "
                  libgovirt $VERSION
                  ===============

        prefix:                    ${prefix}
        datadir:                   ${datadir}
        compiler:                  ${CC}
        cflags:                    ${CFLAGS}
        cppflags:                  ${CPPFLAGS}
"
