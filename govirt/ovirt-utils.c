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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n-lib.h>
#include <rest/rest-xml-parser.h>

#include "ovirt-utils.h"

#include "ovirt-error.h"
#include "ovirt-resource.h"
#include "ovirt-resource-private.h"

RestXmlNode *
ovirt_rest_xml_node_from_call(RestProxyCall *call)
{
    RestXmlParser *parser;
    RestXmlNode *node;
    const char * data = rest_proxy_call_get_payload (call);

    if (data == NULL)
        return NULL;

    parser = rest_xml_parser_new ();

    node = rest_xml_parser_parse_from_data (parser, data,
            rest_proxy_call_get_payload_length (call));

    g_object_unref(G_OBJECT(parser));

    return node;
}

static RestXmlNode *
ovirt_rest_xml_node_find(RestXmlNode *node, const char *path)
{
    guint i;
    GStrv pathv;

    g_return_val_if_fail((path != NULL), NULL);

    pathv = g_strsplit(path, "/", -1);

    for (i = 0; i < g_strv_length(pathv); ++i) {
        gchar *name = node->name;
        node = rest_xml_node_find(node, pathv[i]);
        if (node == NULL) {
            g_debug("could not find subnode '%s' of XML node '%s' (search: %s)", pathv[i], name, path);
            break;
        }
    }

    g_strfreev(pathv);

    return node;
}

static const char *
ovirt_rest_xml_node_get_content_from_path(RestXmlNode *node, const char *path)
{
    node = ovirt_rest_xml_node_find(node, path);

    if (node == NULL)
        return NULL;

    return node->content;
}

static const char *
ovirt_rest_xml_node_get_attr_from_path(RestXmlNode *node, const char *path, const char *attr)
{
    node = ovirt_rest_xml_node_find(node, path);
    if (node == NULL)
        return NULL;

    return rest_xml_node_get_attr(node, attr);
}

static GStrv
ovirt_rest_xml_node_get_str_array_from_path(RestXmlNode *node, const char *path, const char *attr)
{
    GArray *array;
    GHashTableIter iter;
    gpointer sub_node;

    node = ovirt_rest_xml_node_find(node, path);
    if (node == NULL)
        return NULL;

    array = g_array_new(TRUE, FALSE, sizeof(gchar *));

    g_hash_table_iter_init(&iter, node->children);
    while (g_hash_table_iter_next(&iter, NULL, &sub_node)) {
        const char *value;
        char *array_value;

        node = (RestXmlNode *) sub_node;

        if (attr != NULL)
            value = rest_xml_node_get_attr(node, attr);
        else
            value = node->content;

        if (value == NULL) {
            g_warning("node %s%s is NULL", attr ? "attribute:" : "content",  attr ? attr : "" );
            continue;
        }

        array_value = g_strdup(value);
        g_array_append_val(array, array_value);
    }

    return (GStrv) g_array_free(array, FALSE);
}

static gboolean
_set_property_value_from_basic_type(GValue *value,
                                    GType type,
                                    const char *value_str)
{
    switch(type) {
    case G_TYPE_BOOLEAN: {
        gboolean bool_value = ovirt_utils_boolean_from_string(value_str);
        g_value_set_boolean(value, bool_value);
        return TRUE;
    }
    case G_TYPE_STRING: {
        g_value_set_string(value, value_str);
        return TRUE;
    }
    case G_TYPE_UINT: {
        guint uint_value = strtoul(value_str, NULL, 0);
        g_value_set_uint(value, uint_value);
        return TRUE;
    }
    case G_TYPE_UINT64: {
        guint64 int64_value = g_ascii_strtoull(value_str, NULL, 0);
        g_value_set_uint64(value, int64_value);
        return TRUE;
    }
    default: {
        g_warning("Unexpected type '%s' with value '%s'", g_type_name(type), value_str);
    }
    }

    return FALSE;
}

static gboolean
_set_property_value_from_type(GValue *value,
                              GParamSpec *prop,
                              const char *path,
                              const char *attr,
                              RestXmlNode *node)
{
    gboolean ret = TRUE;
    const char *value_str;
    GType type = prop->value_type;

    /* These types do not require a value associated */
    if (g_type_is_a(type, OVIRT_TYPE_RESOURCE)) {
        OvirtResource *resource_value = ovirt_resource_new_from_xml(type, node, NULL);
        g_value_set_object(value, resource_value);
        goto end;
    } else if (g_type_is_a(type, G_TYPE_STRV)) {
        GStrv strv_value = ovirt_rest_xml_node_get_str_array_from_path(node, path, attr);
        if (strv_value == NULL) {
            ret = FALSE;
            goto end;
        }

        g_value_take_boxed(value, strv_value);
        goto end;
    }

    if (attr != NULL)
        value_str = ovirt_rest_xml_node_get_attr_from_path(node, path, attr);
    else
        value_str = ovirt_rest_xml_node_get_content_from_path(node, path);

    /* All other types require valid value_str */
    if (value_str == NULL)
        return FALSE;

    if (G_TYPE_IS_ENUM(type)) {
        GParamSpecEnum *enum_prop = G_PARAM_SPEC_ENUM(prop);
        int enum_value = ovirt_utils_genum_get_value(type, value_str, enum_prop->default_value);
        g_value_set_enum(value, enum_value);
        goto end;
    } else if (g_type_is_a(type, G_TYPE_BYTE_ARRAY)) {
        GByteArray *array = g_byte_array_new_take((guchar *)g_strdup(value_str), strlen(value_str));
        g_value_take_boxed(value, array);
        goto end;
    }

    ret = _set_property_value_from_basic_type(value, type, value_str);

end:
    return ret;
}

gboolean
ovirt_rest_xml_node_parse(RestXmlNode *node,
                          GObject *object,
                          OvirtXmlElement *elements)
{
    g_return_val_if_fail(G_IS_OBJECT(object), FALSE);
    g_return_val_if_fail(elements != NULL, FALSE);

    for (;elements->xml_path != NULL; elements++) {
        GValue value = { 0, };
        GParamSpec *prop;

        prop = g_object_class_find_property(G_OBJECT_GET_CLASS(object), elements->prop_name);
        g_return_val_if_fail(prop != NULL, FALSE);

        g_value_init(&value, prop->value_type);
        if (_set_property_value_from_type(&value, prop, elements->xml_path, elements->xml_attr, node))
            g_object_set_property(object, elements->prop_name, &value);
        g_value_unset(&value);
    }

    return TRUE;
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

G_GNUC_INTERNAL gboolean
ovirt_utils_guint64_from_string(const char *value_str, guint64 *value)
{
    char *end_ptr;
    guint64 result;

    g_return_val_if_fail(value_str != NULL, FALSE);

    result = g_ascii_strtoull(value_str, &end_ptr, 10);
    if ((result == G_MAXUINT64) && (errno == ERANGE)) {
        /* overflow */
        return FALSE;
    }
    if ((result == 0) && (errno == EINVAL)) {
        /* should not happen, invalid base */
        return FALSE;
    }
    if (*end_ptr != '\0') {
        return FALSE;
    }

    *value = result;

    return TRUE;
}

G_GNUC_INTERNAL gboolean
ovirt_utils_guint_from_string(const char *value_str, guint *value)
{
    guint64 value64;
    gboolean success;

    success = ovirt_utils_guint64_from_string(value_str, &value64);
    if (!success) {
        return FALSE;
    }
    if (value64 > G_MAXUINT) {
        return FALSE;
    }

    *value = (guint)value64;

    return TRUE;
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
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    _("Could not find 'reason' node"));
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


G_GNUC_INTERNAL gboolean g_object_set_guint_property_from_xml(GObject *g_object,
                                                                  RestXmlNode *node,
                                                                  const gchar *node_name,
                                                                  const gchar *prop_name)
{
    RestXmlNode *sub_node;
    GParamSpec *spec;
    sub_node = rest_xml_node_find(node, node_name);
    if (sub_node != NULL && sub_node->content != NULL) {
        guint value;
        if (!ovirt_utils_guint_from_string(sub_node->content, &value)) {
            return FALSE;
        }
        spec = g_object_class_find_property(G_OBJECT_GET_CLASS(g_object), prop_name);
        if (spec != NULL && spec->value_type == G_TYPE_UINT) {
            g_object_set(g_object, prop_name, value, NULL);
            return TRUE;
        }
    }
    return FALSE;
}
