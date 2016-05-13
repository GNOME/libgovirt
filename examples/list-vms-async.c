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

static GMainLoop *main_loop;

typedef struct {
    const char *uri;
    OvirtProxy *proxy;
    OvirtApi *api;
} AsyncData;


static gboolean
authenticate_cb(RestProxy *proxy, G_GNUC_UNUSED RestProxyAuth *auth,
                gboolean retrying, gpointer user_data)
{
    if (retrying)
        return FALSE;

    g_debug("setting username to %s", g_getenv("LIBGOVIRT_TEST_USERNAME"));
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



static void pools_fetched_cb(GObject *source_object,
                             GAsyncResult *result,
                             gpointer user_data)
{
    GError *error = NULL;
    OvirtCollection *pools;

    g_debug("Fetched pools");
    pools = OVIRT_COLLECTION(source_object);
    ovirt_collection_fetch_finish(pools, result, &error);
    if (error != NULL) {
        g_debug("failed to fetch pools: %s", error->message);
        g_error_free(error);
        g_main_loop_quit(main_loop);
        return;
    }
    g_print("Pools:\n");
    dump_collection(pools);
    g_main_loop_quit(main_loop);
}


static void vms_fetched_cb(GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
    GError *error = NULL;
    OvirtCollection *vms;
    OvirtCollection *pools;
    AsyncData *data = (AsyncData *)user_data;

    g_debug("Fetched VMs");
    vms = OVIRT_COLLECTION(source_object);
    ovirt_collection_fetch_finish(vms, result, &error);
    if (error != NULL) {
        g_debug("failed to fetch VMs: %s", error->message);
        g_error_free(error);
        g_main_loop_quit(main_loop);
        return;
    }
    g_print("VMs:\n");
    dump_collection(vms);

    pools = ovirt_api_get_vm_pools(data->api);
    ovirt_collection_fetch_async(pools, data->proxy, NULL, pools_fetched_cb, user_data);
}


static void api_fetched_cb(GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
    GError *error = NULL;
    OvirtProxy *proxy;
    OvirtCollection *vms;
    AsyncData *data = (AsyncData *)user_data;

    g_debug("Fetched API");
    proxy = OVIRT_PROXY(source_object);
    data->api = ovirt_proxy_fetch_api_finish(proxy, result, &error);
    if (error != NULL) {
        g_debug("failed to fetch api: %s", error->message);
        g_error_free(error);
        g_main_loop_quit(main_loop);
        return;
    }

    vms = ovirt_api_get_vms(data->api);
    g_assert(vms != NULL);

    ovirt_collection_fetch_async(vms, proxy, NULL, vms_fetched_cb, user_data);
}

static void fetched_ca_cert_cb(GObject *source_object,
                               GAsyncResult *result,
                               gpointer user_data)
{
    GError *error = NULL;
    GByteArray *ca_cert;
    OvirtProxy *proxy;

    g_debug("Fetched CA certificate");
    proxy = OVIRT_PROXY(source_object);
    ca_cert = ovirt_proxy_fetch_ca_certificate_finish(proxy, result, &error);
    if (error != NULL) {
        g_debug("failed to get CA certificate: %s", error->message);
        g_error_free(error);
        g_main_loop_quit(main_loop);
        return;
    }
    if (ca_cert == NULL) {
        g_debug("failed to get CA certificate");
        g_main_loop_quit(main_loop);
        return;
    }
    g_print("\tCA certificate: %p\n", ca_cert);
    g_byte_array_unref(ca_cert);
    ovirt_proxy_fetch_api_async(proxy, NULL, api_fetched_cb, user_data);
}

static gboolean start(gpointer user_data)
{
    AsyncData *data = (AsyncData *)user_data;

    data->proxy = ovirt_proxy_new (data->uri);
    if (data->proxy == NULL)
        return -1;

    g_signal_connect(G_OBJECT(data->proxy), "authenticate",
                     G_CALLBACK(authenticate_cb), NULL);

    ovirt_proxy_fetch_ca_certificate_async(data->proxy, NULL,
                                           fetched_ca_cert_cb,
                                           data);

    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
    AsyncData *data;

    if (argc != 2) {
        g_print("Usage: %s URI\n", argv[0]);
        exit(1);
    }

    data = g_new0(AsyncData, 1);
    data->uri = argv[1];

    g_idle_add(start, data);
    main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);

    if (data->proxy != NULL) {
        g_object_unref(data->proxy);
    }
    g_free(data);

    return 0;
}
