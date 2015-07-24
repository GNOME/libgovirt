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

#define OVIRT_API_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), OVIRT_TYPE_API, OvirtApiPrivate))


struct _OvirtApiPrivate {
    OvirtCollection *storage_domains;
    OvirtCollection *vms;
    OvirtCollection *vm_pools;
};


G_DEFINE_TYPE(OvirtApi, ovirt_api, OVIRT_TYPE_RESOURCE);


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

    g_clear_object(&api->priv->storage_domains);
    g_clear_object(&api->priv->vms);
    g_clear_object(&api->priv->vm_pools);

    G_OBJECT_CLASS(ovirt_api_parent_class)->dispose(object);
}


static void ovirt_api_class_init(OvirtApiClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    OvirtResourceClass *resource_class = OVIRT_RESOURCE_CLASS(klass);

    g_type_class_add_private(klass, sizeof(OvirtApiPrivate));

    object_class->dispose = ovirt_api_dispose;

    resource_class->init_from_xml = ovirt_api_init_from_xml;
}

static void ovirt_api_init(G_GNUC_UNUSED OvirtApi *api)
{
    api->priv = OVIRT_API_GET_PRIVATE(api);
}

OvirtApi *ovirt_api_new_from_xml(RestXmlNode *node, GError **error)
{
    return OVIRT_API(g_initable_new(OVIRT_TYPE_API, NULL, error,
                                   "xml-node", node, NULL));
}


OvirtApi *ovirt_api_new(void)
{
    return OVIRT_API(g_initable_new(OVIRT_TYPE_API, NULL, NULL, NULL));
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
    const char *href;

    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    if (api->priv->vms != NULL)
        return api->priv->vms;

    href = ovirt_resource_get_sub_collection(OVIRT_RESOURCE(api), "vms");
    if (href == NULL)
        return NULL;

    api->priv->vms = ovirt_collection_new(href, "vms", OVIRT_TYPE_VM, "vm");

    return api->priv->vms;
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
    const char *href;

    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    if (api->priv->vm_pools != NULL)
        return api->priv->vm_pools;

    href = ovirt_resource_get_sub_collection(OVIRT_RESOURCE(api), "vmpools");
    if (href == NULL)
        return NULL;

    api->priv->vm_pools = ovirt_collection_new(href, "vmpools", OVIRT_TYPE_VM_POOL, "vmpool");

    return api->priv->vm_pools;
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
    const char *href;

    g_return_val_if_fail(OVIRT_IS_API(api), NULL);

    if (api->priv->storage_domains != NULL)
        return api->priv->storage_domains;

    href = ovirt_resource_get_sub_collection(OVIRT_RESOURCE(api), "storagedomains");
    if (href == NULL)
        return NULL;

    api->priv->storage_domains = ovirt_collection_new(href, "storage_domains",
                                                      OVIRT_TYPE_STORAGE_DOMAIN,
                                                      "storage_domain");

    return api->priv->storage_domains;
}
