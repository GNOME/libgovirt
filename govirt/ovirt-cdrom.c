/*
 * ovirt-cdrom.c: oVirt cdrom handling
 *
 * Copyright (C) 2013 Red Hat, Inc.
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
#include "ovirt-cdrom.h"

#define OVIRT_CDROM_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), OVIRT_TYPE_CDROM, OvirtCdromPrivate))

struct _OvirtCdromPrivate {
    char *file;
};

G_DEFINE_TYPE(OvirtCdrom, ovirt_cdrom, OVIRT_TYPE_RESOURCE);


enum {
    PROP_0,
    PROP_FILE
};


static void ovirt_cdrom_get_property(GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    OvirtCdrom *cdrom = OVIRT_CDROM(object);

    switch (prop_id) {
    case PROP_FILE:
        g_value_set_string(value, cdrom->priv->file);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


static void ovirt_cdrom_set_property(GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    OvirtCdrom *cdrom = OVIRT_CDROM(object);

    switch (prop_id) {
    case PROP_FILE:
        cdrom->priv->file = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


static void ovirt_cdrom_finalize(GObject *object)
{
    OvirtCdrom *cdrom = OVIRT_CDROM(object);

    g_free(cdrom->priv->file);

    G_OBJECT_CLASS(ovirt_cdrom_parent_class)->finalize(object);
}


static gboolean ovirt_cdrom_refresh_from_xml(OvirtCdrom *cdrom,
                                             RestXmlNode *node)
{
    RestXmlNode *file_node;
    const char *file;
    const char *file_key = g_intern_string("file");

    file_node = g_hash_table_lookup(node->children, file_key);
    if (file_node == NULL)
        return FALSE;
    file = rest_xml_node_get_attr(file_node, "id");
    cdrom->priv->file = g_strdup(file);

    return TRUE;
}


static gboolean ovirt_cdrom_init_from_xml(OvirtResource *resource,
                                          RestXmlNode *node,
                                          GError **error)
{
    gboolean parsed_ok;
    OvirtResourceClass *parent_class;

    parsed_ok = ovirt_cdrom_refresh_from_xml(OVIRT_CDROM(resource), node);
    if (!parsed_ok) {
        return FALSE;
    }
    parent_class = OVIRT_RESOURCE_CLASS(ovirt_cdrom_parent_class);

    return parent_class->init_from_xml(resource, node, error);
}


static void ovirt_cdrom_class_init(OvirtCdromClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    OvirtResourceClass *resource_class = OVIRT_RESOURCE_CLASS(klass);
    GParamSpec *param_spec;

    g_type_class_add_private(klass, sizeof(OvirtCdromPrivate));

    resource_class->init_from_xml = ovirt_cdrom_init_from_xml;
    object_class->finalize = ovirt_cdrom_finalize;
    object_class->get_property = ovirt_cdrom_get_property;
    object_class->set_property = ovirt_cdrom_set_property;

    param_spec = g_param_spec_string("file",
                                     "File",
                                     "Name of the CD image",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_FILE,
                                    param_spec);
}


static void ovirt_cdrom_init(OvirtCdrom *cdrom)
{
    cdrom->priv = OVIRT_CDROM_GET_PRIVATE(cdrom);
}
