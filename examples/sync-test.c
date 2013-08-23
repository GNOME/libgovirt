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
#include "govirt/glib-compat.h"


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


int main(int argc, char **argv)
{
    OvirtApi *api;
    OvirtCollection *vms;
    OvirtProxy *proxy = NULL;
    OvirtVm *vm = NULL;
    OvirtVmDisplay *display = NULL;
    OvirtVmState state;
    GError *error = NULL;
    char *host = NULL;
    guint port;
    guint secure_port;
    OvirtVmDisplayType type;
    gchar *ticket = NULL;
    GByteArray *ca_cert = NULL;

    g_type_init();

    if (argc != 3) {
        g_print("Usage: %s URI VM-NAME\n", argv[0]);
        exit(1);
    }

    proxy = ovirt_proxy_new (argv[1]);
    if (proxy == NULL)
        goto error;

    g_signal_connect(G_OBJECT(proxy), "authenticate",
                     G_CALLBACK(authenticate_cb), NULL);

    ovirt_proxy_fetch_ca_certificate(proxy, &error);
    if (error != NULL) {
        g_debug("failed to get CA certificate: %s", error->message);
        goto error;
    }
    g_object_get(G_OBJECT(proxy), "ca-cert", &ca_cert, NULL);

    api = ovirt_proxy_fetch_api(proxy, &error);
    if (error != NULL) {
        g_debug("failed to lookup %s: %s", argv[2], error->message);
        goto error;
    }

    g_assert(api != NULL);
    vms= ovirt_api_get_vms(api);
    g_assert(vms != NULL);
    ovirt_collection_fetch(vms, proxy, &error);
    if (error != NULL) {
        g_debug("failed to lookup %s: %s", argv[2], error->message);
        goto error;
    }
    vm = OVIRT_VM(ovirt_collection_lookup_resource(vms, argv[2]));
    g_return_val_if_fail(vm != NULL, -1);
    g_object_get(G_OBJECT(vm), "state", &state, NULL);
    if (state != OVIRT_VM_STATE_UP) {
        ovirt_vm_start(vm, proxy, &error);
        if (error != NULL) {
            g_debug("failed to start %s: %s", argv[2], error->message);
            goto error;
        }
    }

    if (!ovirt_vm_get_ticket(vm, proxy, &error)) {
        g_debug("failed to get ticket for %s: %s", argv[2], error->message);
        goto error;
    }

    g_object_get(G_OBJECT(vm), "display", &display, NULL);
    if (display == NULL) {
        goto error;
    }

    g_object_get(G_OBJECT(display),
                 "type", &type,
                 "address", &host,
                 "port", &port,
                 "secure-port", &secure_port,
                 "ticket", &ticket,
                 NULL);
    g_print("Connection info for %s:\n", argv[2]);
    g_print("\tConnection type: %s\n",
            (type == OVIRT_VM_DISPLAY_SPICE?"spice":"vnc"));
    g_print("\tVM IP address: %s\n", host);
    g_print("\tPort: %d\n", port);
    g_print("\tSecure port: %d\n", secure_port);
    g_print("\tCA certificate: %p\n", ca_cert);
    g_print("\tTicket: %s\n", ticket);

error:
    g_free(ticket);
    if (ca_cert != NULL)
        g_byte_array_unref(ca_cert);
    if (error != NULL)
        g_error_free(error);
    if (display != NULL)
        g_object_unref(display);
    if (vm != NULL)
        g_object_unref(vm);
    if (proxy != NULL)
        g_object_unref(proxy);

    return 0;
}
