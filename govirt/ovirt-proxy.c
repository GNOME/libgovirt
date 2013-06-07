/*
 * ovirt-proxy.c
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

#include "ovirt-rest-call.h"
#include "ovirt-proxy.h"
#include "ovirt-vm.h"
#include "ovirt-vm-display.h"
#include "ovirt-vm-private.h"

#include <string.h>
#include <glib/gstdio.h>

#include <rest/rest-xml-node.h>
#include <rest/rest-xml-parser.h>

G_DEFINE_TYPE (OvirtProxy, ovirt_proxy, REST_TYPE_PROXY);

struct _OvirtProxyPrivate {
    GHashTable *vms;
    GByteArray *ca_cert;
    gboolean admin_mode;
};

#define OVIRT_PROXY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), OVIRT_TYPE_PROXY, OvirtProxyPrivate))

enum {
    PROP_0,
    PROP_CA_CERT,
    PROP_ADMIN,
};

#define CA_CERT_FILENAME "ca.crt"

GQuark ovirt_proxy_error_quark(void)
{
      return g_quark_from_static_string ("ovirt-proxy-error-quark");
}

#ifdef OVIRT_DEBUG
static void dump_display(OvirtVmDisplay *display)
{
    OvirtVmDisplayType type;
    guint monitor_count;
    gchar *address;
    guint port;
    guint secure_port;
    gchar *ticket;
    guint expiry;

    g_object_get(G_OBJECT(display),
                 "type", &type,
                 "monitor-count", &monitor_count,
                 "address", &address,
                 "port", &port,
                 "secure-port", &secure_port,
                 "ticket", &ticket,
                 "expiry", &expiry,
                 NULL);

    g_print("\tDisplay:\n");
    g_print("\t\tType: %s\n", (type == OVIRT_VM_DISPLAY_VNC)?"vnc":"spice");
    g_print("\t\tMonitors: %d\n", monitor_count);
    g_print("\t\tAddress: %s\n", address);
    g_print("\t\tPort: %d\n", port);
    g_print("\t\tSecure Port: %d\n", secure_port);
    g_print("\t\tTicket: %s\n", ticket);
    g_print("\t\tExpiry: %d\n", expiry);
    g_free(address);
    g_free(ticket);
}

static void dump_key(gpointer key, gpointer value, gpointer user_data)
{
    g_print("[%s] -> %p\n", (char *)key, value);
}

static void dump_action(gpointer key, gpointer value, gpointer user_data)
{
    g_print("\t\t%s -> %s\n", (char *)key, (char *)value);
}

static void dump_vm(OvirtVm *vm)
{
    gchar *name;
    gchar *uuid;
    gchar *href;
    OvirtVmState state;
    GHashTable *actions = NULL;
    OvirtVmDisplay *display;

    g_object_get(G_OBJECT(vm),
                 "name", &name,
                 "uuid", &uuid,
                 "href", &href,
                 "state", &state,
                 "display", &display,
                 NULL);


    g_print("VM:\n");
    g_print("\tName: %s\n", name);
    g_print("\tuuid: %s\n", uuid);
    g_print("\thref: %s\n", href);
    g_print("\tState: %s\n", (state == OVIRT_VM_STATE_UP)?"up":"down");
    if (actions != NULL) {
        g_print("\tActions:\n");
        g_hash_table_foreach(actions, dump_action, NULL);
        g_hash_table_unref(actions);
    }
    if (display != NULL) {
        dump_display(display);
        g_object_unref(display);
    }
    g_free(name);
    g_free(uuid);
    g_free(href);
}
#endif

static GHashTable *parse_vms_xml(RestProxyCall *call)
{
    RestXmlParser *parser;
    RestXmlNode *root;
    RestXmlNode *xml_vms;
    RestXmlNode *node;
    GHashTable *vms;
    const char *vm_key = g_intern_string("vm");

    parser = rest_xml_parser_new ();

    root = rest_xml_parser_parse_from_data (parser,
            rest_proxy_call_get_payload (call),
            rest_proxy_call_get_payload_length (call));

    vms = g_hash_table_new_full(g_str_hash, g_str_equal,
                                g_free, (GDestroyNotify)g_object_unref);
#ifdef OVIRT_DEBUG
    g_hash_table_foreach(root->children, dump_key, NULL);
#endif
    xml_vms = g_hash_table_lookup(root->children, vm_key);
    for (node = xml_vms; node != NULL; node = node->next) {
        OvirtVm *vm;
        gchar *name;
        vm = ovirt_vm_new();
        if (!ovirt_vm_refresh_from_xml(vm, node)) {
            g_message("Failed to parse XML VM description");
            g_object_unref(G_OBJECT(vm));
            continue;
        }
#ifdef OVIRT_DEBUG
        dump_vm(vm);
#endif
        g_object_get(G_OBJECT(vm), "name", &name, NULL);
        if (name == NULL) {
            g_message("VM had no name in its XML description");
            g_object_unref(vm);
            continue;
        }
        if (g_hash_table_lookup(vms, name) != NULL) {
            g_message("VM with the same name ('%s') already exists", name);
            g_object_unref(vm);
            continue;
        }
        g_hash_table_insert(vms, name, vm);
    }
    rest_xml_node_unref(root);
    g_object_unref(G_OBJECT(parser));

    return vms;
}

gboolean ovirt_proxy_fetch_vms(OvirtProxy *proxy, GError **error)
{
    RestProxyCall *call;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);

    call = REST_PROXY_CALL(ovirt_rest_call_new(REST_PROXY(proxy)));
    rest_proxy_call_set_function(call, "vms");
    rest_proxy_call_add_header(call, "All-Content", "true");

    if (!rest_proxy_call_sync(call, error)) {
        g_warning("Error while getting VM list");
        g_object_unref(G_OBJECT(call));
        return FALSE;
    }

    proxy->priv->vms = parse_vms_xml(call);

    g_object_unref(G_OBJECT(call));

    return TRUE;
}

typedef struct {
    GSimpleAsyncResult *result;
    GCancellable *cancellable;
    gulong cancellable_cb_id;
} OvirtProxyFetchVmData;

static void
fetch_vms_cancelled_cb (G_GNUC_UNUSED GCancellable *cancellable,
                        RestProxyCall *call)
{
  rest_proxy_call_cancel (call);
}

static void
fetch_vms_async_cb(RestProxyCall *call, const GError *error,
                   G_GNUC_UNUSED GObject *weak_object, gpointer user_data)
{
  OvirtProxyFetchVmData *data = user_data;
  GSimpleAsyncResult *result = data->result;

  if (error != NULL) {
      g_simple_async_result_set_from_error(result, error);
  } else {
      OvirtProxy *proxy;
      proxy = OVIRT_PROXY (
              g_async_result_get_source_object (G_ASYNC_RESULT (result)));

      if (proxy->priv->vms != NULL) {
          g_hash_table_unref(proxy->priv->vms);
      }
      proxy->priv->vms = parse_vms_xml(call);

      g_simple_async_result_set_op_res_gboolean (result, TRUE);
      g_object_unref (proxy);
  }

  g_simple_async_result_complete (result);
  g_object_unref (result);

  if (data->cancellable != NULL) {
      g_cancellable_disconnect(data->cancellable, data->cancellable_cb_id);
  }
  g_slice_free(OvirtProxyFetchVmData, data);
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
    RestProxyCall *call;
    GError *error;
    OvirtProxyFetchVmData *data;

    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    result = g_simple_async_result_new (G_OBJECT(proxy), callback,
                                        user_data,
                                        ovirt_proxy_fetch_vms_async);

    call = REST_PROXY_CALL(ovirt_rest_call_new(REST_PROXY(proxy)));
    rest_proxy_call_set_function(call, "vms");
    rest_proxy_call_add_header(call, "All-Content", "true");

    data = g_slice_new(OvirtProxyFetchVmData);
    data->result = result;
    if (cancellable != NULL) {
        data->cancellable_cb_id = g_cancellable_connect (cancellable,
                                                         G_CALLBACK (fetch_vms_cancelled_cb),
                                                         call, NULL);
    }

    if (!rest_proxy_call_async(call, fetch_vms_async_cb, NULL,
                               data, &error)) {
        g_warning("Error while getting VM list");
        g_simple_async_result_set_from_error(result, error);
        g_simple_async_result_complete(result);
        g_object_unref(G_OBJECT(result));
        g_object_unref(G_OBJECT(call));
        g_slice_free(OvirtProxyFetchVmData, data);
    }
}

/**
 * ovirt_proxy_fetch_vms_finish:
 * @proxy: a #OvirtProxy
 * @result: (transfer none): async method result
 *
 * Return value: (transfer none) (element-type GoVirt.Vm): the list of
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
                                                        ovirt_proxy_fetch_vms_async),
                         NULL);

    if (g_simple_async_result_propagate_error(G_SIMPLE_ASYNC_RESULT(result), err))
        return NULL;

    return ovirt_proxy_get_vms(proxy);
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

static GFile *get_ca_cert_file(OvirtProxy *proxy)
{
    gchar *base_uri = NULL;
    gchar *ca_uri = NULL;
    GFile *ca_file = NULL;
    gsize suffix_len;

    g_object_get(G_OBJECT(proxy), "url-format", &base_uri, NULL);
    if (base_uri == NULL)
        goto error;
    if (g_str_has_suffix(base_uri, OVIRT_API_BASE_DIR))
        suffix_len = strlen(OVIRT_API_BASE_DIR);
    else if (g_str_has_suffix(base_uri, "/api"))
        suffix_len = strlen("/api");
    else
        g_return_val_if_reached(NULL);

    base_uri[strlen(base_uri) - suffix_len] = '\0';

    ca_uri = g_build_filename(base_uri, CA_CERT_FILENAME, NULL);
    ca_file = g_file_new_for_uri(ca_uri);

error:
    g_free(base_uri);
    g_free(ca_uri);
    return ca_file;
}

static void set_downloaded_ca_cert(OvirtProxy *proxy,
                                   char *ca_cert_data,
                                   gsize ca_cert_len)
{
    if (proxy->priv->ca_cert != NULL)
        g_byte_array_unref(proxy->priv->ca_cert);
    proxy->priv->ca_cert = g_byte_array_new_take((guint8 *)ca_cert_data,
                                                 ca_cert_len);
    g_object_notify(G_OBJECT(proxy), "ca-cert");
}

gboolean ovirt_proxy_fetch_ca_certificate(OvirtProxy *proxy, GError **error)
{
    GFile *source = NULL;
    char *cert_data;
    gsize cert_length;
    gboolean load_ok = FALSE;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);
    g_return_val_if_fail((error == NULL) || (*error == NULL), FALSE);

    source = get_ca_cert_file(proxy);
    load_ok = g_file_load_contents(source, NULL,
                                   &cert_data,
                                   &cert_length,
                                   NULL, error);
    if (!load_ok)
        goto error;

    set_downloaded_ca_cert(proxy, cert_data, cert_length);

error:
    if (source != NULL)
        g_object_unref(source);

    return load_ok;
}

static void ca_file_loaded_cb(GObject *source_object,
                              GAsyncResult *res,
                              gpointer user_data)
{
    GSimpleAsyncResult *fetch_result;
    GObject *proxy;
    GError *error = NULL;
    char *cert_data;
    gsize cert_length;

    fetch_result = G_SIMPLE_ASYNC_RESULT(user_data);
    g_file_load_contents_finish(G_FILE(source_object), res,
                                &cert_data, &cert_length,
                                NULL, &error);
    if (error != NULL) {
        g_simple_async_result_take_error(fetch_result, error);
        g_simple_async_result_complete (fetch_result);
        return;
    }

    proxy = g_async_result_get_source_object(G_ASYNC_RESULT(fetch_result));

    set_downloaded_ca_cert(OVIRT_PROXY(proxy), cert_data, cert_length);
    g_object_unref(proxy);
    g_simple_async_result_set_op_res_gboolean(fetch_result, TRUE);
    g_simple_async_result_complete (fetch_result);
}

void ovirt_proxy_fetch_ca_certificate_async(OvirtProxy *proxy,
                                            GCancellable *cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data)
{
    GFile *ca_file;
    GSimpleAsyncResult *result;

    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    ca_file = get_ca_cert_file(proxy);
    g_return_if_fail(ca_file != NULL);

    result = g_simple_async_result_new(G_OBJECT(proxy), callback,
                                       user_data,
                                       ovirt_proxy_fetch_ca_certificate_async);

    g_file_load_contents_async(ca_file, cancellable, ca_file_loaded_cb, result);
    g_object_unref(ca_file);
}

/**
 * ovirt_proxy_fetch_ca_certificate_finish:
 *
 * Return value: (transfer full):
 */
GByteArray *ovirt_proxy_fetch_ca_certificate_finish(OvirtProxy *proxy,
                                                    GAsyncResult *result,
                                                    GError **err)
{
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);
    g_return_val_if_fail(g_simple_async_result_is_valid(result, G_OBJECT(proxy),
                                                        ovirt_proxy_fetch_ca_certificate_async),
                         NULL);
    g_return_val_if_fail(err == NULL || *err == NULL, NULL);

    if (g_simple_async_result_propagate_error(G_SIMPLE_ASYNC_RESULT(result), err))
        return NULL;

    if (proxy->priv->ca_cert == NULL)
        return NULL;

    return g_byte_array_ref(proxy->priv->ca_cert);
}

static void ovirt_proxy_get_property(GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    OvirtProxy *proxy = OVIRT_PROXY(object);

    switch (prop_id) {
    case PROP_CA_CERT:
        g_value_set_boxed(value, proxy->priv->ca_cert);
        break;
    case PROP_ADMIN:
        g_value_set_boolean(value, proxy->priv->admin_mode);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void ovirt_proxy_set_property(GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    OvirtProxy *proxy = OVIRT_PROXY(object);

    switch (prop_id) {
    case PROP_CA_CERT:
        if (proxy->priv->ca_cert != NULL)
            g_byte_array_unref(proxy->priv->ca_cert);
        proxy->priv->ca_cert = g_value_dup_boxed(value);
        break;

    case PROP_ADMIN:
        proxy->priv->admin_mode = g_value_get_boolean(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ovirt_proxy_dispose(GObject *obj)
{
    OvirtProxy *proxy = OVIRT_PROXY(obj);

    if (proxy->priv->vms) {
        g_hash_table_unref(proxy->priv->vms);
        proxy->priv->vms = NULL;
    }

    G_OBJECT_CLASS(ovirt_proxy_parent_class)->dispose(obj);
}

static void
ovirt_proxy_finalize(GObject *obj)
{
    OvirtProxy *proxy = OVIRT_PROXY(obj);

    if (proxy->priv->ca_cert != NULL)
        g_byte_array_unref(proxy->priv->ca_cert);
    proxy->priv->ca_cert = NULL;

    G_OBJECT_CLASS(ovirt_proxy_parent_class)->finalize(obj);
}

static void
ovirt_proxy_class_init(OvirtProxyClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS(klass);

    oclass->dispose = ovirt_proxy_dispose;
    oclass->finalize = ovirt_proxy_finalize;

    oclass->get_property = ovirt_proxy_get_property;
    oclass->set_property = ovirt_proxy_set_property;

    g_object_class_install_property(oclass,
                                    PROP_CA_CERT,
                                    g_param_spec_boxed("ca-cert",
                                                       "ca-cert",
                                                       "Virt CA certificate to use when connecting to remote VM",
                                                        G_TYPE_BYTE_ARRAY,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(oclass,
                                    PROP_ADMIN,
                                    g_param_spec_boolean("admin",
                                                         "admin",
                                                         "Use REST API as an admin",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

    g_type_class_add_private(klass, sizeof(OvirtProxyPrivate));
}

static void
ovirt_proxy_init(OvirtProxy *self)
{
    self->priv = OVIRT_PROXY_GET_PRIVATE(self);
}

OvirtProxy *ovirt_proxy_new(const char *uri)
{
    return g_object_new(OVIRT_TYPE_PROXY,
                        "url-format", uri,
                        "ssl-strict", FALSE,
                        NULL);
}

/**
 * ovirt_proxy_get_vms:
 *
 * Gets the list of remote VMs from the proxy object.
 * This method does not initiate any network activity, the remote VM list
 * must have been fetched with ovirt_proxy_fetch_vms() or
 * ovirt_proxy_fetch_vms_async() before calling this function.
 *
 * Return value: (transfer none) (element-type GoVirt.Vm): the list of
 * #OvirtVm associated with #OvirtProxy.
 * The returned list should not be freed nor modified, and can become
 * invalid any time a #OvirtProxy call completes.
 */
GList *ovirt_proxy_get_vms(OvirtProxy *proxy)
{
    if (proxy->priv->vms != NULL) {
        return g_hash_table_get_values(proxy->priv->vms);
    }

    return NULL;
}
