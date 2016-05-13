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
    OvirtProxy *proxy;
    const char *vm_name;
} AsyncData;

static AsyncData *async_data;

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


static void updated_cdrom_cb(GObject *source_object,
                            GAsyncResult *result,
                            gpointer user_data)
{
    GError *error = NULL;
    g_debug("updated cdrom cb");
    ovirt_cdrom_update_finish(OVIRT_CDROM(source_object),
                              result, &error);
    if (error != NULL) {
        g_debug("failed to update cdrom resource: %s", error->message);
        g_error_free(error);
    }

    g_main_loop_quit(main_loop);
}


static void cdroms_fetched_cb(GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
    OvirtCollection *cdroms = OVIRT_COLLECTION(source_object);
    GError *error = NULL;
    GHashTableIter resources;
    gpointer value;
    AsyncData *data = (AsyncData *)user_data;

    g_debug("fetched CDROMs");
    ovirt_collection_fetch_finish(cdroms, result, &error);
    if (error != NULL) {
        g_debug("failed to fetch cdroms collection: %s", error->message);
        g_error_free(error);
        g_main_loop_quit(main_loop);
        return;
    }
    g_debug("updating CDROM");
    g_hash_table_iter_init(&resources, ovirt_collection_get_resources(cdroms));
    while (g_hash_table_iter_next(&resources, NULL, &value)) {
        OvirtCdrom *cdrom;
        g_assert(OVIRT_IS_CDROM(value));
        cdrom = OVIRT_CDROM(value);
        g_object_set(G_OBJECT(cdrom), "file", "newimage.iso", NULL);
        ovirt_cdrom_update_async(OVIRT_CDROM(cdrom), FALSE, data->proxy, NULL,
                                 updated_cdrom_cb, user_data);

    }
}

static void got_ticket_cb(GObject *source_object,
                          GAsyncResult *result,
                          gpointer user_data)
{
    GError *error = NULL;
    OvirtVm *vm;
    OvirtVmDisplay *display;
    char *host = NULL;
    guint port;
    guint secure_port;
    OvirtVmDisplayType type;
    gchar *ticket = NULL;
    OvirtCollection *cdroms;
    AsyncData *data = (AsyncData *)user_data;

    g_debug("Got ticket");
    vm = OVIRT_VM(source_object);
    ovirt_vm_get_ticket_finish(vm, result, &error);
    if (error != NULL) {
        g_debug("failed to fetch ticket for VM: %s", error->message);
        g_error_free(error);
        g_main_loop_quit(main_loop);
        return;
    }

    g_object_get(G_OBJECT(vm), "display", &display, NULL);
    if (display == NULL) {
        g_debug("no display for VM");
        g_main_loop_quit(main_loop);
        return;
    }

    g_object_get(G_OBJECT(display),
                 "type", &type,
                 "address", &host,
                 "port", &port,
                 "secure-port", &secure_port,
                 "ticket", &ticket,
                 NULL);
    g_object_unref(G_OBJECT(display));
    g_print("Connection info for VM:\n");
    g_print("\tConnection type: %s\n",
            (type == OVIRT_VM_DISPLAY_SPICE?"spice":"vnc"));
    g_print("\tVM IP address: %s\n", host);
    g_print("\tPort: %d\n", port);
    g_print("\tSecure port: %d\n", secure_port);
    g_print("\tTicket: %s\n", ticket);
    g_free(host);
    g_free(ticket);

    cdroms = ovirt_vm_get_cdroms(vm);
    g_assert(cdroms != NULL);
    ovirt_collection_fetch_async(cdroms, data->proxy, NULL,
                                 cdroms_fetched_cb, data);
}

static void vm_started_cb(GObject *source_object,
                          GAsyncResult *result,
                          gpointer user_data)
{
    GError *error = NULL;
    OvirtVm *vm;
    AsyncData *data = (AsyncData *)user_data;

    g_debug("Started VM");
    vm = OVIRT_VM(source_object);
    ovirt_vm_start_finish(vm, result, &error);
    if (error != NULL) {
        g_debug("failed to start VM: %s", error->message);
        g_error_free(error);
        g_main_loop_quit(main_loop);
        return;
    }
    ovirt_vm_get_ticket_async(vm, data->proxy, NULL, got_ticket_cb, data);
}

static void vms_fetched_cb(GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
    GError *error = NULL;
    OvirtCollection *vms;
    OvirtVm *vm;
    OvirtVmState state;
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

    vm = OVIRT_VM(ovirt_collection_lookup_resource(vms, data->vm_name));
    if (vm == NULL) {
        g_debug("could not find VM '%s'", data->vm_name);
        g_main_loop_quit(main_loop);
        return;
    }

    g_return_if_fail(vm != NULL);
    g_object_get(G_OBJECT(vm), "state", &state, NULL);
    if (state != OVIRT_VM_STATE_UP) {
        ovirt_vm_start_async(vm, data->proxy, NULL, vm_started_cb, data);
        if (error != NULL) {
            g_debug("failed to start VM: %s", error->message);
            g_error_free(error);
            g_main_loop_quit(main_loop);
            return;
        }
    } else {
        ovirt_vm_get_ticket_async(vm, data->proxy, NULL, got_ticket_cb, data);
    }
    g_object_unref(vm);
}


static void api_fetched_cb(GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
    GError *error = NULL;
    OvirtProxy *proxy;
    OvirtApi *api;
    OvirtCollection *vms;

    g_debug("Fetched API");
    proxy = OVIRT_PROXY(source_object);
    api = ovirt_proxy_fetch_api_finish(proxy, result, &error);
    if (error != NULL) {
        g_debug("failed to fetch api: %s", error->message);
        g_error_free(error);
        g_main_loop_quit(main_loop);
        return;
    }

    vms = ovirt_api_get_vms(api);
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
    char **argv = (char **)user_data;
    OvirtProxy *proxy = NULL;

    proxy = ovirt_proxy_new (argv[1]);
    if (proxy == NULL)
        return -1;

    g_signal_connect(G_OBJECT(proxy), "authenticate",
                     G_CALLBACK(authenticate_cb), NULL);

    async_data = g_new0(AsyncData, 1);
    async_data->proxy = proxy;
    async_data->vm_name = argv[2];
    ovirt_proxy_fetch_ca_certificate_async(proxy, NULL,
                                           fetched_ca_cert_cb,
                                           async_data);

    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        g_print("Usage: %s URI VM-NAME\n", argv[0]);
        exit(1);
    }

    g_idle_add(start, argv);
    main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);

    if (async_data != NULL) {
        if (async_data->proxy != NULL) {
            g_object_unref(async_data->proxy);
        }
        g_free(async_data);
    }

    return 0;
}
