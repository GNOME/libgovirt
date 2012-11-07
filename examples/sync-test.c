#include <govirt/govirt.h>

const char *REST_URI;
const char *VM_NAME;

int main(int argc, char **argv)
{
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
    gchar *ca_cert = NULL;

    proxy = ovirt_proxy_new (REST_URI);
    if (proxy == NULL)
        goto error;

    /*g_signal_connect(G_OBJECT(proxy), "authenticate",
                     G_CALLBACK(authenticate_cb), app);*/

    ovirt_proxy_fetch_ca_certificate(proxy, &error);
    if (error != NULL) {
        g_debug("failed to get CA certificate: %s", error->message);
        goto error;
    }
    g_object_get(G_OBJECT(proxy), "ca-cert", &ca_cert, NULL);

    ovirt_proxy_fetch_vms(proxy, &error);
    if (error != NULL) {
        g_debug("failed to lookup %s: %s", VM_NAME, error->message);
        goto error;
    }

    vm = ovirt_proxy_lookup_vm(proxy, VM_NAME);
    g_return_val_if_fail(vm != NULL, -1);
    g_object_get(G_OBJECT(vm), "state", &state, NULL);
    if (state != OVIRT_VM_STATE_UP) {
        ovirt_vm_start(vm, proxy, &error);
        if (error != NULL) {
            g_debug("failed to start %s: %s", VM_NAME, error->message);
            goto error;
        }
    }

    if (!ovirt_vm_get_ticket(vm, proxy, &error)) {
        g_debug("failed to get ticket for %s: %s", VM_NAME, error->message);
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
    g_print("Connection info for %s:\n", VM_NAME);
    g_print("\tConnection type: %s\n",
            (type == OVIRT_VM_DISPLAY_SPICE?"spice":"vnc"));
    g_print("\tVM IP address: %s", host);
    g_print("\tPort: %d", port);
    g_print("\tSecure port: %d", secure_port);
    g_print("\tCA certificate: %s", ca_cert);
    g_print("\tTicket: %s", ticket);

error:
    g_free(ticket);
    g_free(ca_cert);

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
