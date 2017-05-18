/*
 * ovirt-utils.h
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
#ifndef __OVIRT_UTILS_H__
#define __OVIRT_UTILS_H__

#include <rest/rest-proxy-call.h>
#include <rest/rest-xml-node.h>

G_BEGIN_DECLS

typedef struct _OvirtXmlElement OvirtXmlElement;
struct _OvirtXmlElement
{
    const char *prop_name;
    const char *xml_path;
    const char *xml_attr; /* if NULL, retrieve node content instead of attribute */
};

RestXmlNode *ovirt_rest_xml_node_from_call(RestProxyCall *call);
gboolean ovirt_rest_xml_node_parse(RestXmlNode *node,
                                   GObject *object,
                                   OvirtXmlElement *elements);
gboolean ovirt_utils_gerror_from_xml_fault(RestXmlNode *root, GError **error);
gboolean g_object_set_guint_property_from_xml(GObject *g_object,
                                                   RestXmlNode *node,
                                                   const gchar *node_name,
                                                   const gchar *prop_name);

const char *ovirt_utils_genum_get_nick (GType enum_type, gint value);
int ovirt_utils_genum_get_value (GType enum_type, const char *nick,
                                 gint default_value);
gboolean ovirt_utils_guint64_from_string(const char *value_str, guint64 *value);
gboolean ovirt_utils_guint_from_string(const char *value_str, guint *value);
gboolean ovirt_utils_boolean_from_string(const char *value);

G_END_DECLS

#endif /* __OVIRT_UTILS_H__ */
