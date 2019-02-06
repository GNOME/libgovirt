/*
 * ovirt-data_center.c: oVirt data center handling
 *
 * Copyright (C) 2017 Red Hat, Inc.
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
#include "ovirt-data-center.h"
#include "govirt-private.h"

struct _OvirtDataCenterPrivate {
    OvirtCollection *clusters;
    OvirtCollection *storage_domains;
};

G_DEFINE_TYPE_WITH_PRIVATE(OvirtDataCenter, ovirt_data_center, OVIRT_TYPE_RESOURCE);

static void
ovirt_data_center_dispose(GObject *obj)
{
    OvirtDataCenter *data_center = OVIRT_DATA_CENTER(obj);

    g_clear_object(&data_center->priv->clusters);
    g_clear_object(&data_center->priv->storage_domains);

    G_OBJECT_CLASS(ovirt_data_center_parent_class)->dispose(obj);
}

static void ovirt_data_center_class_init(OvirtDataCenterClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = ovirt_data_center_dispose;
}


static void ovirt_data_center_init(OvirtDataCenter *data_center)
{
    data_center->priv = ovirt_data_center_get_instance_private(data_center);
}

G_GNUC_INTERNAL
OvirtDataCenter *ovirt_data_center_new_from_id(const char *id,
                                               const char *href)
{
    OvirtResource *data_center = ovirt_resource_new_from_id(OVIRT_TYPE_DATA_CENTER, id, href);
    return OVIRT_DATA_CENTER(data_center);
}

G_GNUC_INTERNAL
OvirtDataCenter *ovirt_data_center_new_from_xml(RestXmlNode *node,
                                                GError **error)
{
    OvirtResource *data_center = ovirt_resource_new_from_xml(OVIRT_TYPE_DATA_CENTER, node, error);
    return OVIRT_DATA_CENTER(data_center);
}

OvirtDataCenter *ovirt_data_center_new(void)
{
    OvirtResource *data_center = ovirt_resource_new(OVIRT_TYPE_DATA_CENTER);
    return OVIRT_DATA_CENTER(data_center);
}


/**
 * ovirt_data_center_get_clusters:
 * @data_center: a #OvirtDataCenter
 *
 * Gets a #OvirtCollection representing the list of remote clusters from a
 * data center object. This method does not initiate any network
 * activity, the remote cluster list must be then be fetched using
 * ovirt_collection_fetch() or ovirt_collection_fetch_async().
 *
 * Return value: (transfer none): a #OvirtCollection representing the list
 * of clusters associated with @data_center.
 */
OvirtCollection *ovirt_data_center_get_clusters(OvirtDataCenter *data_center)
{
    g_return_val_if_fail(OVIRT_IS_DATA_CENTER(data_center), NULL);

    if (data_center->priv->clusters == NULL) {
        OvirtCollection *collection;
        collection = ovirt_sub_collection_new_from_resource(OVIRT_RESOURCE(data_center),
                                                            "clusters",
                                                            "clusters",
                                                            OVIRT_TYPE_CLUSTER,
                                                            "cluster");
        data_center->priv->clusters = collection;
    }

    return data_center->priv->clusters;
}


/**
 * ovirt_data_center_get_storage_domains:
 * @data_center: a #OvirtDataCenter
 *
 * Gets a #OvirtCollection representing the list of remote storage domains from a
 * data center object. This method does not initiate any network
 * activity, the remote storage domain list must be then be fetched using
 * ovirt_collection_fetch() or ovirt_collection_fetch_async().
 *
 * Return value: (transfer none): a #OvirtCollection representing the list
 * of storage_domains associated with @data_center.
 */
OvirtCollection *ovirt_data_center_get_storage_domains(OvirtDataCenter *data_center)
{
    g_return_val_if_fail(OVIRT_IS_DATA_CENTER(data_center), NULL);

    if (data_center->priv->storage_domains == NULL) {
        OvirtCollection *collection;
        collection = ovirt_sub_collection_new_from_resource(OVIRT_RESOURCE(data_center),
                                                            "storagedomains",
                                                            "storage_domains",
                                                            OVIRT_TYPE_STORAGE_DOMAIN,
                                                            "storage_domain");
        data_center->priv->storage_domains = collection;
    }

    return data_center->priv->storage_domains;
}
