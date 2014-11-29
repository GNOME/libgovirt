#!/bin/sh

PKG_NAME="govirt"
srcdir=`dirname "$0"`

which gnome-autogen.sh || {
    echo "You need to install gnome-common from the GNOME git"
    exit 1
}

. gnome-autogen.sh "$@"
