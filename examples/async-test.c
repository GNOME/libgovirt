#include <stdlib.h>

#include <govirt/govirt.h>
#include "govirt/glib-compat.h"

static GMainLoop *main_loop;

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
    //GByteArray *ca_cert;

    g_debug("Got ticket");
    vm = OVIRT_VM(source_object);
    ovirt_vm_get_ticket_finish(vm, result, &error);
    if (error != NULL) {
        g_debug("failed to fetch ticket for VM: %s", error->message);
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
    g_print("Connection info for VM:\n");
    g_print("\tConnection type: %s\n",
            (type == OVIRT_VM_DISPLAY_SPICE?"spice":"vnc"));
    g_print("\tVM IP address: %s\n", host);
    g_print("\tPort: %d\n", port);
    g_print("\tSecure port: %d\n", secure_port);
    g_print("\tTicket: %s\n", ticket);
    g_main_loop_quit(main_loop);
}

static void vm_started_cb(GObject *source_object,
                          GAsyncResult *result,
                          gpointer user_data)
{
    GError *error = NULL;
    OvirtVm *vm;
    OvirtProxy *proxy;

    g_debug("Started VM");
    vm = OVIRT_VM(source_object);
    proxy = OVIRT_PROXY(user_data);
    ovirt_vm_start_finish(vm, result, &error);
    if (error != NULL) {
        g_debug("failed to start VM: %s", error->message);
        g_main_loop_quit(main_loop);
        return;
    }
    ovirt_vm_get_ticket_async(vm, proxy, NULL, got_ticket_cb, proxy);
}

static void vms_fetched_cb(GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
    GError *error = NULL;
    OvirtProxy *proxy;
    OvirtVm *vm;
    OvirtVmState state;

    g_debug("Fetched VMs");
    proxy = OVIRT_PROXY(source_object);
    ovirt_proxy_fetch_vms_finish(proxy, result, &error);
    if (error != NULL) {
        g_debug("failed to fetch VMs: %s", error->message);
        g_main_loop_quit(main_loop);
        return;
    }

    vm = ovirt_proxy_lookup_vm(proxy, user_data);
    if (vm == NULL) {
        g_debug("could not find VM '%s'", (char *)user_data);
        g_main_loop_quit(main_loop);
        return;
    }

    g_return_if_fail(vm != NULL);
    g_object_get(G_OBJECT(vm), "state", &state, NULL);
    if (state != OVIRT_VM_STATE_UP) {
        ovirt_vm_start_async(vm, proxy, NULL, vm_started_cb, proxy);
        if (error != NULL) {
            g_debug("failed to start VM: %s", error->message);
            g_main_loop_quit(main_loop);
            return;
        }
    } else {
        ovirt_vm_get_ticket_async(vm, proxy, NULL, got_ticket_cb, proxy);
    }
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
        g_main_loop_quit(main_loop);
        return;
    }
    if (ca_cert == NULL) {
        g_debug("failed to get CA certificate");
        g_main_loop_quit(main_loop);
        return;
    }
    g_print("\tCA certificate: %p\n", ca_cert);
    ovirt_proxy_fetch_vms_async(proxy, NULL, vms_fetched_cb, user_data);
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


    ovirt_proxy_fetch_ca_certificate_async(proxy, NULL,
                                           fetched_ca_cert_cb,
                                           argv[2]);

    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
    g_type_init();

    if (argc != 3) {
        g_print("Usage: %s URI VM-NAME\n", argv[0]);
        exit(1);
    }

    g_idle_add(start, argv);
    main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);

    return 0;
#if 0
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
#endif
}
