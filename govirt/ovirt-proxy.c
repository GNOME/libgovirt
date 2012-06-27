/*
 * ovirt-proxy.c
 *
 * Copyright (C) 2011, 2012 Red Hat, Inc.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
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

#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#include <rest/rest-xml-node.h>
#include <rest/rest-xml-parser.h>

G_DEFINE_TYPE (OvirtProxy, ovirt_proxy, REST_TYPE_PROXY);

struct _OvirtProxyPrivate {
    GHashTable *vms;
    gchar *ca_cert_path;
    gboolean delete_ca_cert;
};

#define OVIRT_PROXY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), OVIRT_TYPE_PROXY, OvirtProxyPrivate))

enum {
    PROP_0,
    PROP_CA_CERT
};

#define API_ENTRY_POINT "/api/"
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

static gboolean vm_set_name_from_xml(OvirtVm *vm, RestXmlNode *node)
{
    RestXmlNode *name_node;

    name_node = rest_xml_node_find(node, "name");
    if (name_node != NULL) {
        g_return_val_if_fail(name_node->content != NULL, FALSE);
        g_object_set(G_OBJECT(vm), "name", name_node->content, NULL);
        return TRUE;
    }

    return FALSE;
}

static gboolean vm_set_display_from_xml(OvirtVm *vm,
                                        RestXmlNode *root)
{
    RestXmlNode *node;
    OvirtVmDisplay *display;
    const char *display_key = g_intern_string("display");
    const char *type_key = g_intern_string("type");
    const char *address_key = g_intern_string("address");
    const char *port_key = g_intern_string("port");
    const char *secure_port_key = g_intern_string("secure_port");
    const char *monitors_key = g_intern_string("monitors");

    if (root == NULL) {
        return FALSE;
    }
    root = g_hash_table_lookup(root->children, display_key);
    g_return_val_if_fail(root != NULL, FALSE);
    display = ovirt_vm_display_new();

    node = g_hash_table_lookup(root->children, type_key);
    g_return_val_if_fail(node != NULL, FALSE);
    if (g_strcmp0(node->content, "spice") == 0) {
        g_object_set(G_OBJECT(display), "type", OVIRT_VM_DISPLAY_SPICE, NULL);
    } else if (g_strcmp0(node->content, "vnc") == 0) {
        g_object_set(G_OBJECT(display), "type", OVIRT_VM_DISPLAY_VNC, NULL);
    } else {
        g_warning("Unknown display type: %s", node->content);
        return FALSE;
    }

    node = g_hash_table_lookup(root->children, monitors_key);
    g_return_val_if_fail(node != NULL, FALSE);
    g_object_set(G_OBJECT(display),
                 "monitor-count", strtoul(node->content, NULL, 0),
                 NULL);

    /* on non started VMs, these 2 values will not be available */
    node = g_hash_table_lookup(root->children, address_key);
    if (node != NULL) {
        g_object_set(G_OBJECT(display), "address", node->content, NULL);
    }

    node = g_hash_table_lookup(root->children, port_key);
    if (node != NULL) {
        g_object_set(G_OBJECT(display),
                     "port", strtoul(node->content, NULL, 0),
                     NULL);
    }

    node = g_hash_table_lookup(root->children, secure_port_key);
    if (node != NULL) {
        g_object_set(G_OBJECT(display),
                     "secure-port", strtoul(node->content, NULL, 0),
                     NULL);
    }

    /* FIXME: this overrides the ticket/expiry which may
     * already be set
     */
    g_object_set(G_OBJECT(vm), "display", display, NULL);
    g_object_unref(G_OBJECT(display));

    return TRUE;
}

static gboolean vm_set_state_from_xml(OvirtVm *vm, RestXmlNode *node)
{
    RestXmlNode *state;

    state = rest_xml_node_find(node, "status");
    state = rest_xml_node_find(state, "state");
    if (state != NULL) {
        gboolean res = FALSE;
        g_return_val_if_fail(state->content != NULL, FALSE);
        if (strcmp(state->content, "down") == 0) {
            g_object_set(G_OBJECT(vm), "state", OVIRT_VM_STATE_DOWN, NULL);
            res = TRUE;
        } else if (strcmp(state->content, "up") == 0) {
            /* FIXME: check "UP" state name in the rsdl file */
            g_object_set(G_OBJECT(vm), "state", OVIRT_VM_STATE_UP, NULL);
            res = TRUE;
        } else {
            g_warning("Couldn't parse VM state: %s", state->content);
        }
        return res;
    }

    return FALSE;
}

static gboolean vm_set_actions_from_xml(OvirtVm *vm, RestXmlNode *node)
{
    RestXmlNode *link;
    RestXmlNode *rest_actions;
    const char *link_key = g_intern_string("link");

    rest_actions = rest_xml_node_find(node, "actions");
    if (rest_actions == NULL) {
        return FALSE;
    }

    link = g_hash_table_lookup(rest_actions->children, link_key);
    if (link == NULL)
        return FALSE;

    for (; link != NULL; link = link->next) {
        const char *link_name;
        const char *href;

        g_warn_if_fail(link != NULL);
        g_warn_if_fail(link->name != NULL);
        g_warn_if_fail(strcmp(link->name, "link") == 0);

        link_name = rest_xml_node_get_attr(link, "rel");
        href = rest_xml_node_get_attr(link, "href");
        if (g_str_has_prefix(href, API_ENTRY_POINT)) {
            href += strlen(API_ENTRY_POINT);
        } else {
            /* action href should always be prefixed by /api/ */
            /* it would be easier to remove /api/ from the RestProxy base
             * URL but unfortunately I couldn't get this to work
             */
            g_warn_if_reached();
        }

        if ((link_name != NULL) && (href != NULL)) {
            ovirt_vm_add_action(vm, link_name, href);
        }
    }

    return TRUE;
}

static OvirtVm *xml_to_vm(RestXmlNode *node)
{
    OvirtVm *vm;
    const char *uuid;
    const char *href;


    uuid = rest_xml_node_get_attr(node, "id");
    g_return_val_if_fail(uuid != NULL, NULL);
    href = rest_xml_node_get_attr(node, "href");
    g_return_val_if_fail(href != NULL, NULL);

    vm = ovirt_vm_new();
    g_object_set(G_OBJECT(vm), "uuid", uuid, "href", href, NULL);

    vm_set_name_from_xml(vm, node);
    vm_set_state_from_xml(vm, node);
    vm_set_actions_from_xml(vm, node);
    vm_set_display_from_xml(vm, node);

    return vm;
}

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
        vm = xml_to_vm(node);
#ifdef OVIRT_DEBUG
        dump_vm(vm);
#endif
        if (!OVIRT_IS_VM(vm)) {
            g_message("Failed to parse XML VM description");
            continue;
        }
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

    if (!rest_proxy_call_sync(call, error)) {
        g_warning("Error while getting VM list");
        g_object_unref(G_OBJECT(call));
        return FALSE;
    }

    proxy->priv->vms = parse_vms_xml(call);

    g_object_unref(G_OBJECT(call));

    return TRUE;
}

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
  GSimpleAsyncResult *result = user_data;

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

    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    result = g_simple_async_result_new (G_OBJECT(proxy), callback,
                                        user_data,
                                        ovirt_proxy_fetch_vms_async);

    call = REST_PROXY_CALL(ovirt_rest_call_new(REST_PROXY(proxy)));
    rest_proxy_call_set_function(call, "vms");
    if (cancellable != NULL) {
        g_signal_connect (cancellable, "cancelled",
                          G_CALLBACK (fetch_vms_cancelled_cb), call);
    }

    if (!rest_proxy_call_async(call, fetch_vms_async_cb, NULL,
                               result, &error)) {
        g_warning("Error while getting VM list");
        g_simple_async_result_set_from_error(result, error);
        g_simple_async_result_complete(result);
        g_object_unref(G_OBJECT(result));
        g_object_unref(G_OBJECT(call));
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
    if (g_str_has_suffix(base_uri, API_ENTRY_POINT))
        suffix_len = strlen(API_ENTRY_POINT);
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

typedef struct {
    OvirtProxy *proxy;
    GSimpleAsyncResult *result;
    GCancellable *cancellable;
    GFile *local_ca_cert;
    GFileIOStream *iostream; //stream to write to local_ca_cert
} FetchCaCertData;

static void fetch_ca_cert_data_free(FetchCaCertData *data, gboolean delete_temp)
{
    g_return_if_fail(data != NULL);

    if (data->proxy != NULL) {
        g_object_unref(data->proxy);
    }
    if (data->result != NULL) {
        g_object_unref(data->result);
    }
    if (data->cancellable != NULL) {
        g_object_unref(data->cancellable);
    }
    if (data->local_ca_cert != NULL) {
        if (delete_temp) {
            g_file_delete(data->local_ca_cert, NULL, NULL);
        }
        g_object_unref(data->local_ca_cert);
    }
    if (data->iostream != NULL) {
        g_object_unref(data->iostream);
    }
    g_slice_free(FetchCaCertData, data);
}

static void ca_file_failed_cb(FetchCaCertData *data, GError *error)
{
    g_return_if_fail(error != NULL);

    g_simple_async_result_take_error(data->result, error);
    g_simple_async_result_complete (data->result);
    fetch_ca_cert_data_free(data, TRUE);
}

static void ca_file_spliced_cb (GObject *source_object,
                                GAsyncResult *res,
                                gpointer user_data)
{
    FetchCaCertData *data = (FetchCaCertData *)user_data;
    GOutputStream *stream;
    GError *error = NULL;
    gssize spliced_bytes;

    stream = G_OUTPUT_STREAM(source_object);
    spliced_bytes = g_output_stream_splice_finish(stream, res, &error);
    if (error != NULL) {
        ca_file_failed_cb(data, error);
        return;
    } else {
      g_simple_async_result_set_op_res_gboolean(data->result, !!spliced_bytes);
      if (data->proxy->priv->delete_ca_cert) {
          g_unlink(data->proxy->priv->ca_cert_path);
      }
      g_free(data->proxy->priv->ca_cert_path);
      data->proxy->priv->ca_cert_path = g_file_get_path(data->local_ca_cert);
      data->proxy->priv->delete_ca_cert = TRUE;
      g_object_notify(G_OBJECT(data->proxy), "ca-cert");
    }

    g_simple_async_result_complete (data->result);

    fetch_ca_cert_data_free(data, FALSE);
}

static void ca_file_opened_cb (GObject *source_object,
                               GAsyncResult *res,
                               gpointer user_data)
{
    FetchCaCertData *data;
    GFileInputStream *source;
    GOutputStream *out_stream;
    GError *error = NULL;

    data = (FetchCaCertData *)user_data;
    source = g_file_read_finish(G_FILE(source_object), res, &error);
    if (error != NULL) {
        ca_file_failed_cb(data, error);
        return;
    }

    data->local_ca_cert = g_file_new_tmp("ovirt-ca-XXXXXX.crt",
                                         &data->iostream, &error);
    if (error != NULL) {
        ca_file_failed_cb(data, error);
        return;
    }

    out_stream = g_io_stream_get_output_stream(G_IO_STREAM(data->iostream));

    g_output_stream_splice_async(out_stream, G_INPUT_STREAM(source),
                                 G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE
                                 | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                 G_PRIORITY_DEFAULT, data->cancellable,
                                 ca_file_spliced_cb, data);
    g_object_unref(G_OBJECT(source));
}

void ovirt_proxy_fetch_ca_certificate_async(OvirtProxy *proxy,
                                            GCancellable *cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data)
{
    GFile *ca_file;
    FetchCaCertData *data;

    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    ca_file = get_ca_cert_file(proxy);
    g_return_if_fail(ca_file != NULL);

    data = g_slice_new0(FetchCaCertData);
    data->result = g_simple_async_result_new (G_OBJECT(proxy), callback,
                                              user_data,
                                              ovirt_proxy_fetch_ca_certificate_async);
    g_simple_async_result_set_check_cancellable(data->result, cancellable);
    data->proxy = g_object_ref(proxy);
    if (cancellable != NULL) {
        data->cancellable = g_object_ref(cancellable);
    }

    g_file_read_async(ca_file, G_PRIORITY_DEFAULT, cancellable,
                      ca_file_opened_cb, data);
    g_object_unref(ca_file);
}

const char *ovirt_proxy_fetch_ca_certificate_finish(OvirtProxy *proxy,
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

    return proxy->priv->ca_cert_path;
}

static void ovirt_proxy_get_property(GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    OvirtProxy *proxy = OVIRT_PROXY(object);

    switch (prop_id) {
    case PROP_CA_CERT:
        g_value_set_string(value, proxy->priv->ca_cert_path);
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
        if (proxy->priv->delete_ca_cert) {
            g_unlink(proxy->priv->ca_cert_path);
        }
        g_free(proxy->priv->ca_cert_path);
        proxy->priv->ca_cert_path = g_value_dup_string(value);
        proxy->priv->delete_ca_cert = FALSE;
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

    if (proxy->priv->delete_ca_cert) {
        g_unlink(proxy->priv->ca_cert_path);
    }
    g_free(proxy->priv->ca_cert_path);
    proxy->priv->ca_cert_path = NULL;

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
                                    g_param_spec_string("ca-cert",
                                                        "ca-cert",
                                                        "Path to the oVirt CA certificate to use when connecting to remote VM",
                                                        NULL,
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
 * Return value: (transfer none) (element-type GoVirt.Vm): the list of
 * #OvirtVm associated with #OvirtProxy. It must be populated with a call
 * to ovirt_proxy_fetch_vms_async() before being non-NULL. The returned
 * list should not be freed nor modified, and can become invalid any time
 * a #OvirtProxy call completes.
 */
GList *ovirt_proxy_get_vms(OvirtProxy *proxy)
{
    if (proxy->priv->vms != NULL) {
        return g_hash_table_get_values(proxy->priv->vms);
    }

    return NULL;
}
