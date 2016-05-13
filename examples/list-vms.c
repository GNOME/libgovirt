/* Copyright 2013 Red Hat, Inc. and/or its affiliates.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdlib.h>

#include <govirt/govirt.h>

static gboolean
authenticate_cb(RestProxy *proxy, G_GNUC_UNUSED RestProxyAuth *auth,
                G_GNUC_UNUSED gboolean retrying, gpointer user_data)
{
    if (retrying)
        return FALSE;

    g_object_set(G_OBJECT(proxy),
                 "username", g_getenv("LIBGOVIRT_TEST_USERNAME"),
                 "password", g_getenv("LIBGOVIRT_TEST_PASSWORD"),
                 NULL);
    return TRUE;
}

static void dump_resource(gpointer key, gpointer value, gpointer user_data)
{
    g_assert(OVIRT_IS_RESOURCE(value));

    g_print("\t%s\n", (char *)key);
}


static void dump_collection(OvirtCollection *collection)
{
    GHashTable *resources;

    resources = ovirt_collection_get_resources(collection);
    g_hash_table_foreach(resources, dump_resource, NULL);
}


static void dump_vms(OvirtApi *api, OvirtProxy *proxy)
{
    OvirtCollection *vms;
    GError *error = NULL;

    vms = ovirt_api_get_vms(api);
    g_assert(vms != NULL);
    ovirt_collection_fetch(vms, proxy, &error);
    if (error != NULL) {
        g_debug("failed to fetch VMs: %s", error->message);
        g_error_free(error);
        return;
    }

    g_print("VMs:\n");
    dump_collection(vms);
}


static void dump_vm_pools(OvirtApi *api, OvirtProxy *proxy)
{
    OvirtCollection *pools;
    GError *error = NULL;

    pools = ovirt_api_get_vm_pools(api);
    g_assert(pools != NULL);
    ovirt_collection_fetch(pools, proxy, &error);
    if (error != NULL) {
        g_debug("failed to fetch VM pools: %s", error->message);
        g_error_free(error);
        return;
    }

    g_print("VM pools:\n");
    dump_collection(pools);
}


int main(int argc, char **argv)
{
    OvirtApi *api;
    OvirtProxy *proxy = NULL;
    GError *error = NULL;

    if (argc != 2) {
        g_print("Usage: %s URI\n", argv[0]);
        exit(1);
    }

    proxy = ovirt_proxy_new(argv[1]);
    if (proxy == NULL)
        goto error;

    g_signal_connect(G_OBJECT(proxy), "authenticate",
                     G_CALLBACK(authenticate_cb), NULL);

    /* Should be using ovirt_get_option_group/ovirt_set_proxy_options
     * instead as ovirt_proxy_fetch_ca_certificate is not checking
     * properly the validity of https certificates
     */
    ovirt_proxy_fetch_ca_certificate(proxy, &error);
    if (error != NULL) {
        g_debug("failed to get CA certificate: %s", error->message);
        goto error;
    }

    api = ovirt_proxy_fetch_api(proxy, &error);
    if (error != NULL) {
        g_debug("failed to lookup %s: %s", argv[2], error->message);
        goto error;
    }
    g_assert(api != NULL);

    dump_vms(api, proxy);
    dump_vm_pools(api, proxy);


error:
    if (error != NULL)
        g_error_free(error);
    if (proxy != NULL)
        g_object_unref(proxy);

    return 0;
}
