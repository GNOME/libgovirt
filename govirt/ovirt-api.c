/*
 * ovirt-api.c: oVirt API entry point
 *
 * Copyright (C) 2012, 2013 Red Hat, Inc.
 * Copyright (C) 2013 Iordan Iordanov <i@iiordanov.com>
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

#include "ovirt-enum-types.h"
#include "ovirt-error.h"
#include "ovirt-proxy.h"
#include "ovirt-rest-call.h"
#include "ovirt-api.h"
#include "ovirt-vm-pool.h"
#include "govirt-private.h"

#include <rest/rest-xml-node.h>
#include <rest/rest-xml-parser.h>
#include <stdlib.h>
#include <string.h>

struct _OvirtApiPrivate {
    OvirtCollection *clusters;
    OvirtCollection *data_centers;
    OvirtCollection *hosts;
    OvirtCollection *storage_domains;
    OvirtCollection *vms;
    OvirtCollection *vm_pools;
};


G_DEFINE_TYPE_WITH_PRIVATE(OvirtApi, ovirt_api, OVIRT_TYPE_RESOURCE);


static gboolean ovirt_api_init_from_xml(OvirtResource *resource,
                                       RestXmlNode *node,
                                       GError **error)
{
    OvirtResourceClass *parent_class;
#if 0
    gboolean parsed_ok;

    parsed_ok = ovirt_api_refresh_from_xml(OVIRT_API(resource), node);
    if (!parsed_ok) {
        return FALSE;
    }
#endif
    parent_class = OVIRT_RESOURCE_CLASS(ovirt_api_parent_class);

    return parent_class->init_from_xml(resource, node, error);
}


static void ovirt_api_dispose(GObject *object)
{
    OvirtApi *api = OVIRT_API(object);

    g_clear_object(&api->priv->clusters);
    g_clear_object(&api->priv->data_centers);
    g_clear_object(&api->priv->hosts);
    g_clear_object(&api->priv->storage_domains);
    g_clear_object(&api->priv->vms);
    g_clear_object(&api->priv->vm_pools);

    G_OBJECT_CLASS(ovirt_api_parent_class)->dispose(object);
}


static void ovirt_api_class_init(OvirtApiClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    OvirtResourceClass *resource_class = OVIRT_RESOURCE_CLASS(klass);

    object_class->dispose = ovirt_api_dispose;

    resource_class->init_from_xml = ovirt_api_init_from_xml;
}

static void ovirt_api_init(G_GNUC_UNUSED OvirtApi *api)
{
    api->priv = ovirt_api_get_instance_private(api);
}

G_GNUC_INTERNAL
OvirtApi *ovirt_api_new_from_xml(RestXmlNode *node, GError **error)
{
    OvirtResource *api = ovirt_resource_new_from_xml(OVIRT_TYPE_API, node, error);
    return OVIRT_API(api);
}


OvirtApi *ovirt_api_new(void)
{
    OvirtResource *api = ovirt_resource_new(OVIRT_TYPE_API);
    return OVIRT_API(api);
}


/**
 * ovirt_api_get_vms:
 * @api: a #OvirtApi
 *
 * This method does not initiate any network activity, the collection
 * must be fetched with ovirt_collection_fetch() before having up-to-date
 * content.
 *
 * Return value: (transfer none):
 */
OvirtCollection *ovirt_api_get_vms(OvirtApi *api)
{
    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    if (api->priv->vms == NULL)
        api->priv->vms = ovirt_sub_collection_new_from_resource(OVIRT_RESOURCE(api),
                                                                "vms",
                                                                "vms",
                                                                OVIRT_TYPE_VM,
                                                                "vm");

    return api->priv->vms;
}

/**
 * ovirt_api_search_vms:
 * @api: a #OvirtApi
 * @query: search query
 *
 * Return value: (transfer full):
 */
OvirtCollection *ovirt_api_search_vms(OvirtApi *api, const char *query)
{
    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    return ovirt_sub_collection_new_from_resource_search(OVIRT_RESOURCE(api),
                                                         "vms/search",
                                                         "vms",
                                                         OVIRT_TYPE_VM,
                                                         "vm",
                                                         query);
}


/**
 * ovirt_api_get_vm_pools:
 * @api: a #OvirtApi
 *
 * This method does not initiate any network activity, the collection
 * must be fetched with ovirt_collection_fetch() before having up-to-date
 * content.
 *
 * Return value: (transfer none):
 */
OvirtCollection *ovirt_api_get_vm_pools(OvirtApi *api)
{
    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    if (api->priv->vm_pools == NULL)
        api->priv->vm_pools = ovirt_sub_collection_new_from_resource(OVIRT_RESOURCE(api),
                                                                     "vmpools",
                                                                     "vmpools",
                                                                     OVIRT_TYPE_VM_POOL,
                                                                     "vmpool");

    return api->priv->vm_pools;
}


/**
 * ovirt_api_search_vm_pools:
 * @api: a #OvirtApi
 * @query: search query
 *
 * Return value: (transfer full):
 */
OvirtCollection *ovirt_api_search_vm_pools(OvirtApi *api, const char *query)
{
    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    return ovirt_sub_collection_new_from_resource_search(OVIRT_RESOURCE(api),
                                                         "vmpools/search",
                                                         "vmpools",
                                                         OVIRT_TYPE_VM_POOL,
                                                         "vmpool",
                                                         query);
}


/**
 * ovirt_api_get_storage_domains:
 * @api: a #OvirtApi
 *
 * This method does not initiate any network activity, the collection
 * must be fetched with ovirt_collection_fetch() before having up-to-date
 * content.
 *
 * Return value: (transfer none):
 */
OvirtCollection *ovirt_api_get_storage_domains(OvirtApi *api)
{
    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    if (api->priv->storage_domains == NULL)
        api->priv->storage_domains = ovirt_sub_collection_new_from_resource(OVIRT_RESOURCE(api),
                                                                            "storagedomains",
                                                                            "storage_domains",
                                                                            OVIRT_TYPE_STORAGE_DOMAIN,
                                                                            "storage_domain");

    return api->priv->storage_domains;
}


/**
 * ovirt_api_search_storage_domains:
 * @api: a #OvirtApi
 * @query: search query
 *
 * Return value: (transfer full):
 */
OvirtCollection *ovirt_api_search_storage_domains(OvirtApi *api, const char *query)
{
    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    return ovirt_sub_collection_new_from_resource_search(OVIRT_RESOURCE(api),
                                                         "storagedomains/search",
                                                         "storage_domains",
                                                         OVIRT_TYPE_STORAGE_DOMAIN,
                                                         "storage_domain",
                                                         query);
}


/**
 * ovirt_api_get_hosts:
 * @api: a #OvirtApi
 *
 * This method does not initiate any network activity, the collection
 * must be fetched with ovirt_collection_fetch() before having up-to-date
 * content.
 *
 * Return value: (transfer none):
 */
OvirtCollection *ovirt_api_get_hosts(OvirtApi *api)
{
    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    if (api->priv->hosts == NULL)
        api->priv->hosts = ovirt_sub_collection_new_from_resource(OVIRT_RESOURCE(api),
                                                                  "hosts",
                                                                  "hosts",
                                                                  OVIRT_TYPE_HOST,
                                                                  "host");

    return api->priv->hosts;
}


/**
 * ovirt_api_search_hosts:
 * @api: a #OvirtApi
 * @query: search query
 *
 * Return value: (transfer none):
 */
OvirtCollection *ovirt_api_search_hosts(OvirtApi *api, const char *query)
{
    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    return ovirt_sub_collection_new_from_resource_search(OVIRT_RESOURCE(api),
                                                         "hosts",
                                                         "hosts",
                                                         OVIRT_TYPE_HOST,
                                                         "host",
                                                         query);
}


/**
 * ovirt_api_get_clusters:
 * @api: a #OvirtApi
 *
 * This method does not initiate any network activity, the collection
 * must be fetched with ovirt_collection_fetch() before having up-to-date
 * content.
 *
 * Return value: (transfer none):
 */
OvirtCollection *ovirt_api_get_clusters(OvirtApi *api)
{
    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    if (api->priv->clusters == NULL)
        api->priv->clusters = ovirt_sub_collection_new_from_resource(OVIRT_RESOURCE(api),
                                                                     "clusters",
                                                                     "clusters",
                                                                     OVIRT_TYPE_CLUSTER,
                                                                     "cluster");

    return api->priv->clusters;
}


/**
 * ovirt_api_search_clusters:
 * @api: a #OvirtApi
 * @query: search query
 *
 * Return value: (transfer none):
 */
OvirtCollection *ovirt_api_search_clusters(OvirtApi *api, const char *query)
{
    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    return ovirt_sub_collection_new_from_resource_search(OVIRT_RESOURCE(api),
                                                         "clusters/search",
                                                         "clusters",
                                                         OVIRT_TYPE_CLUSTER,
                                                         "cluster",
                                                         query);
}


/**
 * ovirt_api_get_data_centers:
 * @api: a #OvirtApi
 *
 * This method does not initiate any network activity, the collection
 * must be fetched with ovirt_collection_fetch() before having up-to-date
 * content.
 *
 * Return value: (transfer none):
 */
OvirtCollection *ovirt_api_get_data_centers(OvirtApi *api)
{
    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    if (api->priv->data_centers == NULL)
        api->priv->data_centers = ovirt_sub_collection_new_from_resource(OVIRT_RESOURCE(api),
                                                                         "datacenters",
                                                                         "data_centers",
                                                                         OVIRT_TYPE_DATA_CENTER,
                                                                         "data_center");

    return api->priv->data_centers;
}


/**
 * ovirt_api_search_data_centers:
 * @api: a #OvirtApi
 * @query: search query
 *
 * Return value: (transfer none):
 */
OvirtCollection *ovirt_api_search_data_centers(OvirtApi *api, const char *query)
{
    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    return ovirt_sub_collection_new_from_resource_search(OVIRT_RESOURCE(api),
                                                         "datacenters/search",
                                                         "data_centers",
                                                         OVIRT_TYPE_DATA_CENTER,
                                                         "data_center",
                                                         query);
}
