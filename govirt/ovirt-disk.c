/*
 * ovirt-disk.c: oVirt disk handling
 *
 * Copyright (C) 2020 Red Hat, Inc.
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
 * Author: Eduardo Lima (Etrunko) <etrunko@redhat.com>
 */

#include <config.h>
#include "ovirt-enum-types.h"
#include "ovirt-disk.h"
#include "govirt-private.h"

struct _OvirtDiskPrivate {
    OvirtDiskContentType content_type;
};

G_DEFINE_TYPE_WITH_PRIVATE(OvirtDisk, ovirt_disk, OVIRT_TYPE_RESOURCE);

enum {
    PROP_0,
    PROP_CONTENT_TYPE,
};

static void ovirt_disk_get_property(GObject *object,
                                    guint prop_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
    OvirtDisk *disk = OVIRT_DISK(object);

    switch (prop_id) {
    case PROP_CONTENT_TYPE:
        g_value_set_enum(value, disk->priv->content_type);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void ovirt_disk_set_property(GObject *object,
                                    guint prop_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
    OvirtDisk *disk = OVIRT_DISK(object);

    switch (prop_id) {
    case PROP_CONTENT_TYPE:
        disk->priv->content_type = g_value_get_enum(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean ovirt_disk_init_from_xml(OvirtResource *resource,
                                         RestXmlNode *node,
                                         GError **error)
{
    gboolean parsed_ok;
    OvirtResourceClass *parent_class;
    OvirtXmlElement disk_elements[] = {
        { .prop_name = "content-type",
          .xml_path = "content_type",
        },
        { NULL , }
    };

    parsed_ok = ovirt_rest_xml_node_parse(node, G_OBJECT(resource), disk_elements);
    if (!parsed_ok) {
        return FALSE;
    }
    parent_class = OVIRT_RESOURCE_CLASS(ovirt_disk_parent_class);

    return parent_class->init_from_xml(resource, node, error);
}

static void ovirt_disk_class_init(OvirtDiskClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    OvirtResourceClass *resource_class = OVIRT_RESOURCE_CLASS(klass);
    GParamSpec *param_spec;

    resource_class->init_from_xml = ovirt_disk_init_from_xml;

    object_class->get_property = ovirt_disk_get_property;
    object_class->set_property = ovirt_disk_set_property;

    param_spec = g_param_spec_enum("content-type",
                                   "Content Type",
                                   "The actual content residing on the disk",
                                   OVIRT_TYPE_DISK_CONTENT_TYPE,
                                   OVIRT_DISK_CONTENT_TYPE_DATA,
                                   G_PARAM_READWRITE |
                                   G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_CONTENT_TYPE,
                                    param_spec);
}


static void ovirt_disk_init(OvirtDisk *disk)
{
    disk->priv = ovirt_disk_get_instance_private(disk);
}

G_GNUC_INTERNAL
OvirtDisk *ovirt_disk_new_from_id(const char *id,
                                  const char *href)
{
    OvirtResource *disk = ovirt_resource_new_from_id(OVIRT_TYPE_DISK, id, href);
    return OVIRT_DISK(disk);
}

G_GNUC_INTERNAL
OvirtDisk *ovirt_disk_new_from_xml(RestXmlNode *node,
                                   GError **error)
{
    OvirtResource *disk = ovirt_resource_new_from_xml(OVIRT_TYPE_DISK, node, error);
    return OVIRT_DISK(disk);
}

OvirtDisk *ovirt_disk_new(void)
{
    OvirtResource *disk = ovirt_resource_new(OVIRT_TYPE_DISK);
    return OVIRT_DISK(disk);
}
