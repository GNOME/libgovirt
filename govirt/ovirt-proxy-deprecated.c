/*
 * ovirt-proxy-deprecated.c: Deprecated OvirtProxy methods
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

#undef OVIRT_DEBUG
#include <config.h>

#include "govirt.h"
#include "govirt-private.h"

#include <rest/rest-xml-node.h>


static gboolean
ovirt_proxy_parse_vms_xml(OvirtProxy *proxy, RestXmlNode *root, GError **error)
{
    OvirtCollection *collection;
    GHashTable *resources;

    collection = ovirt_collection_new_from_xml(root, OVIRT_TYPE_COLLECTION, "vms",
                                               OVIRT_TYPE_VM, "vm", error);
    if (collection == NULL) {
        return FALSE;
    }

    resources = ovirt_collection_get_resources(collection);

    if (proxy->priv->vms != NULL) {
        g_hash_table_unref(proxy->priv->vms);
        proxy->priv->vms = NULL;
    }
    if (resources != NULL) {
        proxy->priv->vms = g_hash_table_ref(resources);
    }

    return TRUE;
}


gboolean ovirt_proxy_fetch_vms(OvirtProxy *proxy, GError **error)
{
    RestXmlNode *vms_node;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);

    vms_node = ovirt_proxy_get_collection_xml(proxy, "/ovirt-engine/api/vms", error);
    if (vms_node == NULL)
        return FALSE;

    ovirt_proxy_parse_vms_xml(proxy, vms_node, error);

    rest_xml_node_unref(vms_node);

    return TRUE;
}


static gboolean fetch_vms_async_cb(OvirtProxy* proxy,
                                   RestXmlNode *root_node,
                                   gpointer user_data,
                                   GError **error)
{
    return ovirt_proxy_parse_vms_xml(proxy, root_node, error);
}

/**
 * ovirt_proxy_fetch_vms_async:
 * @proxy: a #OvirtProxy
 * @callback: (scope async): completion callback
 * @user_data: (closure): opaque data for callback
 */
void ovirt_proxy_fetch_vms_async(OvirtProxy *proxy,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
    GSimpleAsyncResult *result;

    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    result = g_simple_async_result_new (G_OBJECT(proxy), callback,
                                        user_data,
                                        /* Not using the _async function
                                         * name as is customary as this
                                         * would trigger a deprecation
                                         * warning */
                                        fetch_vms_async_cb);
    ovirt_proxy_get_collection_xml_async(proxy, "/ovirt-engine/api/vms", result, cancellable,
                                         fetch_vms_async_cb, NULL, NULL);
}

/**
 * ovirt_proxy_fetch_vms_finish:
 * @proxy: a #OvirtProxy
 * @result: (transfer none): async method result
 *
 * Return value: (transfer none) (element-type OvirtVm): the list of
 * #OvirtVm associated with #OvirtProxy. The returned list should not be
 * freed nor modified, and can become invalid any time a #OvirtProxy call
 * completes.
 */
GList *
ovirt_proxy_fetch_vms_finish(OvirtProxy *proxy,
                             GAsyncResult *result,
                             GError **err)
{
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);
    g_return_val_if_fail(g_simple_async_result_is_valid(result, G_OBJECT(proxy),
                                                        fetch_vms_async_cb),
                         NULL);

    if (g_simple_async_result_propagate_error(G_SIMPLE_ASYNC_RESULT(result), err))
        return NULL;

    if (proxy->priv->vms != NULL) {
        return g_hash_table_get_values(proxy->priv->vms);
    }

    return NULL;
}

/**
 * ovirt_proxy_lookup_vm:
 * @proxy: a #OvirtProxy
 * @vm_name: name of the virtual machine to lookup
 *
 * Looks up a virtual machine whose name is @name. If it cannot be found,
 * NULL is returned. This method does not initiate any network activity,
 * the remote VM list must have been fetched with ovirt_proxy_fetch_vms()
 * or ovirt_proxy_fetch_vms_async() before calling this function.
 *
 * Return value: (transfer full): a #OvirtVm whose name is @name or NULL
 */
OvirtVm *ovirt_proxy_lookup_vm(OvirtProxy *proxy, const char *vm_name)
{
    OvirtVm *vm;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);
    g_return_val_if_fail(vm_name != NULL, NULL);

    if (proxy->priv->vms == NULL) {
        return NULL;
    }

    vm = g_hash_table_lookup(proxy->priv->vms, vm_name);

    if (vm == NULL) {
        return NULL;
    }

    return g_object_ref(vm);
}


/**
 * ovirt_proxy_get_vms:
 *
 * Gets the list of remote VMs from the proxy object.
 * This method does not initiate any network activity, the remote VM list
 * must have been fetched with ovirt_proxy_fetch_vms() or
 * ovirt_proxy_fetch_vms_async() before calling this function.
 *
 * Return value: (transfer none) (element-type OvirtVm): the list of
 * #OvirtVm associated with #OvirtProxy.
 * The returned list should not be freed nor modified, and can become
 * invalid any time a #OvirtProxy call completes.
 */
GList *ovirt_proxy_get_vms(OvirtProxy *proxy)
{
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);

    if (proxy->priv->vms != NULL) {
        return g_hash_table_get_values(proxy->priv->vms);
    }

    return NULL;
}
