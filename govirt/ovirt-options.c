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
#include <config.h>

#include <glib.h>

#ifndef G_OS_WIN32
#include <pwd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

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
          N_("Root CA certificate file for secure SSL connections"), N_("<file>") },
        { "ovirt-version", '\0', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_version,
          N_("Display libgovirt version information"), NULL },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };
    GOptionGroup *grp;

    grp = g_option_group_new("ovirt", _("oVirt Options:"), _("Show oVirt Options"), NULL, NULL);
    g_option_group_add_entries(grp, entries);
    g_option_group_set_translation_domain(grp, GETTEXT_PACKAGE);

    return grp;
}

#ifndef G_OS_WIN32
/* Taken from gnome-vfs-utils.c */
static gchar *
expand_initial_tilde(const gchar *path)
{
    gchar *slash_after_user_name, *user_name;
    struct passwd *passwd_file_entry;

    if (path[1] == '/' || path[1] == '\0') {
        return g_build_filename(g_get_home_dir(), &path[1], NULL);
    }

    slash_after_user_name = strchr(&path[1], '/');
    if (slash_after_user_name == NULL) {
        user_name = g_strdup(&path[1]);
    } else {
        user_name = g_strndup(&path[1], slash_after_user_name - &path[1]);
    }
    passwd_file_entry = getpwnam(user_name);
    g_free(user_name);

    if (passwd_file_entry == NULL || passwd_file_entry->pw_dir == NULL) {
        return g_strdup(path);
    }

    return g_strconcat(passwd_file_entry->pw_dir,
                       slash_after_user_name,
                       NULL);
}
#endif

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

    if (ca_file) {
        gchar *ca_file_absolute_path = NULL;

#ifndef G_OS_WIN32
        /* We have to treat files with non-absolute paths starting
         * with tilde (eg, ~foo/bar/ca.crt or ~/bar/ca.cert).
         * Other non-absolute paths will be treated down in the
         * stack, by libsoup, simply prepending the user's current
         * dir to the file path */
        if (ca_file[0] == '~')
            ca_file_absolute_path = expand_initial_tilde(ca_file);
#endif

        g_object_set(G_OBJECT(proxy),
                     "ssl-ca-file",
                     ca_file_absolute_path != NULL ? ca_file_absolute_path : ca_file,
                     NULL);
        g_free (ca_file_absolute_path);
    }
}
