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

gboolean ovirt_proxy_fetch_vms(OvirtProxy *proxy, GError **error)
{
    OvirtCollection *vms;
    OvirtApi *api;

    api = ovirt_proxy_fetch_api(proxy, error);
    if (api == NULL)
        return FALSE;

    vms = ovirt_api_get_vms(api);
    if (vms == NULL)
        return FALSE;

    return ovirt_collection_fetch(vms, proxy, error);
}


typedef struct {
    GCancellable *cancellable;
    GAsyncReadyCallback callback;
    gpointer user_data;
} ApiAsyncData;

static void fetch_api_async_cb(GObject *source_object,
                               GAsyncResult *result,
                               gpointer user_data)
{
    ApiAsyncData *data = user_data;
    OvirtProxy *proxy = OVIRT_PROXY(source_object);
    OvirtApi *api;
    GError *error = NULL;

    api = ovirt_proxy_fetch_api_finish(proxy, result, &error);
    if (api == NULL) {
        g_task_report_new_error(source_object,
                                data->callback, data->user_data,
                                fetch_api_async_cb,
                                OVIRT_ERROR, OVIRT_ERROR_FAILED,
                                "Could not fetch API endpoint");
    } else {
        OvirtCollection *vms;

        vms = ovirt_api_get_vms(api);
        g_return_if_fail(vms != NULL);

        ovirt_collection_fetch_async(vms, proxy, data->cancellable,
                                     data->callback, data->user_data);
    }
    g_free(data);
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
    OvirtApi *api;
    OvirtCollection *vms;

    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    api = ovirt_proxy_get_api(proxy);
    if (api == NULL) {
        ApiAsyncData *data = g_new0(ApiAsyncData, 1);
        data->cancellable = cancellable;
        data->callback = callback;
        data->user_data = user_data;
        ovirt_proxy_fetch_api_async(proxy, cancellable,
                                    fetch_api_async_cb, data);
        return;
    }

    vms = ovirt_api_get_vms(api);
    g_return_if_fail(vms != NULL);

    return ovirt_collection_fetch_async(vms, proxy, cancellable,
                                        callback, user_data);
}


/**
 * ovirt_proxy_fetch_vms_finish:
 * @proxy: a #OvirtProxy
 * @result: (transfer none): async method result
 *
 * Return value: (transfer container) (element-type OvirtVm): the list of
 * #OvirtVm associated with #OvirtProxy.
 * The returned list should be freed with g_list_free(), and can become
 * invalid any time a #OvirtProxy call completes.
 */
GList *
ovirt_proxy_fetch_vms_finish(OvirtProxy *proxy,
                             GAsyncResult *result,
                             GError **err)
{
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);

    if (g_task_had_error(G_TASK(result)))
        return NULL;

    return ovirt_proxy_get_vms_internal(proxy);
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
    OvirtApi *api;
    OvirtCollection *vm_collection;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);
    g_return_val_if_fail(vm_name != NULL, NULL);

    api = ovirt_proxy_get_api(proxy);
    if (api == NULL)
        return NULL;

    vm_collection = ovirt_api_get_vms(api);
    if (vm_collection == NULL)
        return NULL;

    return OVIRT_VM(ovirt_collection_lookup_resource(vm_collection, vm_name));
}


/**
 * ovirt_proxy_get_vms:
 *
 * Gets the list of remote VMs from the proxy object.
 * This method does not initiate any network activity, the remote VM list
 * must have been fetched with ovirt_proxy_fetch_vms() or
 * ovirt_proxy_fetch_vms_async() before calling this function.
 *
 * Return value: (transfer container) (element-type OvirtVm): the list of
 * #OvirtVm associated with #OvirtProxy.
 * The returned list should be freed with g_list_free(), and can become
 * invalid any time a #OvirtProxy call completes.
 */
GList *ovirt_proxy_get_vms(OvirtProxy *proxy)
{
    return ovirt_proxy_get_vms_internal(proxy);
}
