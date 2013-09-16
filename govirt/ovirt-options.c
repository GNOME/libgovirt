/*
   Copyright (C) 2010, 2013 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <glib-object.h>

#include "ovirt-options.h"

static char *ca_file = NULL;

G_GNUC_NORETURN
static void option_version(void)
{
    g_print(PACKAGE_STRING "\n");
    exit(0);
}

/**
 * ovirt_get_option_group: (skip)
 *
 * Returns: (transfer full): a #GOptionGroup for the commandline
 * arguments specific to libgovirt.  You have to call
 * ovirt_set_proxy_options() after to set the options on a
 * #OvirtProxy.
 **/
GOptionGroup* ovirt_get_option_group(void)
{
    const GOptionEntry entries[] = {
        { "ovirt-ca-file", '\0', 0, G_OPTION_ARG_FILENAME, &ca_file,
          "Root CA certificate file for secure SSL connections", "<file>" },
        { "ovirt-version", '\0', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_version,
          "Display libgovirt version information", NULL },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };
    GOptionGroup *grp;

    grp = g_option_group_new("ovirt", "oVirt Options:", "Show oVirt Options", NULL, NULL);
    g_option_group_add_entries(grp, entries);

    return grp;
}

/**
 * ovirt_set_proxy_options:
 * @proxy: a #OvirtProxy to set options upon
 *
 * Set various properties on @proxy, according to the commandline
 * arguments given to ovirt_get_option_group() option group.
 **/
void ovirt_set_proxy_options(OvirtProxy *proxy)
{
    g_return_if_fail(OVIRT_IS_PROXY(proxy));

    if (ca_file)
        g_object_set(G_OBJECT(proxy), "ssl-ca-file", ca_file, NULL);
}
