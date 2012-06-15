#!/bin/sh
# Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
#
# Run this to generate all the initial makefiles, etc.
#
# Licensed under the GNU General Public License Version 2
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

olddir=`pwd`
cd "$srcdir"

#GTKDOCIZE=`which gtkdocize`
#if test -z $GTKDOCIZE; then
#        echo "*** No GTK-Doc found, please install it ***"
#        exit 1
#fi

AUTORECONF=`which autoreconf`
if test -z $AUTORECONF; then
        echo "*** No autoreconf found, please install it ***"
        exit 1
fi

#gtkdocize || exit $?
AUTOPOINT='intltoolize --automake --copy' autoreconf --force --install --verbose

cd "$olddir"
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
