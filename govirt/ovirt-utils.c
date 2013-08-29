/*
 * ovirt-utils.c
 *
 * Copyright (C) 2011, 2013 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Christophe Fergeau <cfergeau@redhat.com>
 */

#include <config.h>

#include <string.h>

#include <rest/rest-xml-parser.h>

#include "ovirt-utils.h"

#include "ovirt-error.h"

RestXmlNode *
ovirt_rest_xml_node_from_call(RestProxyCall *call)
{
    RestXmlParser *parser;
    RestXmlNode *node;

    parser = rest_xml_parser_new ();

    node = rest_xml_parser_parse_from_data (parser,
            rest_proxy_call_get_payload (call),
            rest_proxy_call_get_payload_length (call));

    g_object_unref(G_OBJECT(parser));

    return node;
}

G_GNUC_INTERNAL const char *
ovirt_rest_xml_node_get_content(RestXmlNode *node, ...)
{
    va_list args;

    g_return_val_if_fail(node != NULL, NULL);

    va_start(args, node);

    while (TRUE) {
        const char *node_name;
        node_name = va_arg(args, const char *);
        if (node_name == NULL)
            break;
        node = rest_xml_node_find(node, node_name);
        if (node == NULL) {
            g_debug("could not find XML node '%s'", node_name);
            return NULL;
        }
    }

    va_end(args);

    g_warn_if_fail(node != NULL);

    return node->content;
}


/* These 2 functions come from
 * libvirt-glib/libvirt-gconfig/libvirt-gconfig-helpers.c
 * Copyright (C) 2010, 2011 Red Hat, Inc.
 * LGPLv2.1+ licensed */
G_GNUC_INTERNAL const char *
ovirt_utils_genum_get_nick (GType enum_type, gint value)
{
    GEnumClass *enum_class;
    GEnumValue *enum_value;

    g_return_val_if_fail (G_TYPE_IS_ENUM (enum_type), NULL);

    enum_class = g_type_class_ref(enum_type);
    enum_value = g_enum_get_value(enum_class, value);
    g_type_class_unref(enum_class);

    if (enum_value != NULL)
        return enum_value->value_nick;

    g_return_val_if_reached(NULL);
}

G_GNUC_INTERNAL int
ovirt_utils_genum_get_value (GType enum_type, const char *nick,
                             gint default_value)
{
    GEnumClass *enum_class;
    GEnumValue *enum_value;

    g_return_val_if_fail(G_TYPE_IS_ENUM(enum_type), default_value);
    g_return_val_if_fail(nick != NULL, default_value);

    enum_class = g_type_class_ref(enum_type);
    enum_value = g_enum_get_value_by_nick(enum_class, nick);
    g_type_class_unref(enum_class);

    if (enum_value != NULL)
        return enum_value->value;

    g_return_val_if_reached(default_value);
}

G_GNUC_INTERNAL gboolean
ovirt_utils_boolean_from_string(const char *value)
{
    g_return_val_if_fail(value != NULL, FALSE);

    return (g_strcmp0(value, "true") == 0);
}


G_GNUC_INTERNAL const char *ovirt_utils_strip_api_base_dir(const char *path)
{
    if (g_str_has_prefix(path, OVIRT_API_BASE_DIR)) {
        g_debug("stripping %s from %s", OVIRT_API_BASE_DIR, path);
        path += strlen(OVIRT_API_BASE_DIR);
    }

    return path;
}


G_GNUC_INTERNAL gboolean ovirt_utils_gerror_from_xml_fault(RestXmlNode *root, GError **error)
{
    RestXmlNode *reason_node;
    RestXmlNode *detail_node;
    const char *reason_key = g_intern_string("reason");
    const char *detail_key = g_intern_string("detail");

    g_return_val_if_fail((error == NULL) || (*error == NULL), FALSE);

    if (g_strcmp0(root->name, "fault") != 0)
        return FALSE;

    reason_node = g_hash_table_lookup(root->children, reason_key);
    if (reason_node == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED, "could not find 'reason' node");
        g_return_val_if_reached(FALSE);
    }
    g_debug("Reason: %s\n", reason_node->content);
    detail_node = g_hash_table_lookup(root->children, detail_key);
    if (detail_node != NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_FAILED, "%s: %s",
                    reason_node->content, detail_node->content);
    } else {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_FAILED, "%s",
                    reason_node->content);
    }

    return TRUE;
}
