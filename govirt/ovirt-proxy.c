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

#include "ovirt-error.h"
#include "ovirt-proxy.h"
#include "ovirt-proxy-private.h"
#include "ovirt-rest-call.h"
#include "ovirt-vm.h"
#include "ovirt-vm-display.h"
#include "govirt-private.h"

#include <string.h>
#include <glib/gstdio.h>

#include <rest/rest-xml-node.h>
#include <rest/rest-xml-parser.h>

G_DEFINE_TYPE (OvirtProxy, ovirt_proxy, REST_TYPE_PROXY);

#define OVIRT_PROXY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), OVIRT_TYPE_PROXY, OvirtProxyPrivate))

enum {
    PROP_0,
    PROP_CA_CERT,
    PROP_ADMIN,
};

#define CA_CERT_FILENAME "ca.crt"

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


RestXmlNode *ovirt_proxy_get_collection_xml(OvirtProxy *proxy,
                                            const char *href,
                                            GError **error)
{
    RestProxyCall *call;
    RestXmlNode *root;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);

    call = REST_PROXY_CALL(ovirt_action_rest_call_new(REST_PROXY(proxy)));
    href = ovirt_utils_strip_api_base_dir(href);
    rest_proxy_call_set_function(call, href);
    rest_proxy_call_add_header(call, "All-Content", "true");

    if (!rest_proxy_call_sync(call, error)) {
        g_warning("Error while getting collection");
        g_object_unref(G_OBJECT(call));
        return NULL;
    }

    root = ovirt_rest_xml_node_from_call(call);
    g_object_unref(G_OBJECT(call));

    return root;
}

typedef struct {
    OvirtProxy *proxy;
    GSimpleAsyncResult *result;
    GCancellable *cancellable;
    gulong cancellable_cb_id;
    OvirtProxyCallAsyncCb call_async_cb;
    gpointer call_user_data;
    GDestroyNotify destroy_call_data;
} OvirtProxyCallAsyncData;

static void ovirt_proxy_call_async_data_free(OvirtProxyCallAsyncData *data)
{
        if (data->destroy_call_data != NULL) {
            data->destroy_call_data(data->call_user_data);
        }
        if (data->proxy != NULL) {
            g_object_unref(G_OBJECT(data->proxy));
        }
        if (data->result != NULL) {
            g_object_unref(G_OBJECT(data->result));
        }
        if ((data->cancellable != NULL) && (data->cancellable_cb_id != 0)) {
            g_cancellable_disconnect(data->cancellable, data->cancellable_cb_id);
        }
        g_slice_free(OvirtProxyCallAsyncData, data);
}

static void
call_async_cancelled_cb (G_GNUC_UNUSED GCancellable *cancellable,
                         RestProxyCall *call)
{
    rest_proxy_call_cancel(call);
}


static void
call_async_cb(RestProxyCall *call, const GError *error,
              G_GNUC_UNUSED GObject *weak_object,
              gpointer user_data)
{
    OvirtProxyCallAsyncData *data = user_data;
    GSimpleAsyncResult *result = data->result;

    if (error != NULL) {
        g_simple_async_result_set_from_error(result, error);
    } else {
        GError *call_error = NULL;
        gboolean callback_result = FALSE;

        if (data->call_async_cb != NULL) {
            callback_result = data->call_async_cb(data->proxy, call,
                                                  data->call_user_data,
                                                  &call_error);
            if (call_error != NULL) {
                g_simple_async_result_set_from_error(result, call_error);
            }
        }

        g_simple_async_result_set_op_res_gboolean(result, callback_result);
    }

    g_simple_async_result_complete (result);
    ovirt_proxy_call_async_data_free(data);
}


OvirtRestCall *ovirt_rest_call_new(OvirtProxy *proxy,
                                   const char *method,
                                   const char *href)
{
    OvirtRestCall *call;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);

    call = OVIRT_REST_CALL(ovirt_action_rest_call_new(REST_PROXY(proxy)));
    if (method != NULL) {
        rest_proxy_call_set_method(REST_PROXY_CALL(call), method);
    }
    href = ovirt_utils_strip_api_base_dir(href);
    rest_proxy_call_set_function(REST_PROXY_CALL(call), href);
    /* FIXME: to set or not to set ?? */
    rest_proxy_call_add_header(REST_PROXY_CALL(call), "All-Content", "true");

    return call;
}


void ovirt_rest_call_async(OvirtRestCall *call,
                           GSimpleAsyncResult *result,
                           GCancellable *cancellable,
                           OvirtProxyCallAsyncCb callback,
                           gpointer user_data,
                           GDestroyNotify destroy_func)
{
    OvirtProxy *proxy;
    GError *error;
    OvirtProxyCallAsyncData *data;

    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));
    g_object_get(G_OBJECT(call), "proxy", &proxy, NULL);
    g_return_if_fail(OVIRT_IS_PROXY(proxy));

    data = g_slice_new0(OvirtProxyCallAsyncData);
    data->proxy = proxy;
    data->result = result;
    data->call_async_cb = callback;
    data->call_user_data = user_data;
    data->destroy_call_data = destroy_func;
    if (cancellable != NULL) {
        data->cancellable_cb_id = g_cancellable_connect(cancellable,
                                                        G_CALLBACK (call_async_cancelled_cb),
                                                        call, NULL);
    }

    if (!rest_proxy_call_async(REST_PROXY_CALL(call), call_async_cb, NULL,
                               data, &error)) {
        g_warning("Error while getting collection XML");
        g_simple_async_result_set_from_error(result, error);
        g_simple_async_result_complete(result);
        g_object_unref(G_OBJECT(call));
        ovirt_proxy_call_async_data_free(data);
    }
}


gboolean ovirt_rest_call_finish(GAsyncResult *result, GError **err)
{
    GSimpleAsyncResult *simple;

    simple = G_SIMPLE_ASYNC_RESULT(result);
    if (g_simple_async_result_propagate_error(simple, err))
        return FALSE;

    return g_simple_async_result_get_op_res_gboolean(simple);
}

typedef struct {
    OvirtProxyGetCollectionAsyncCb parser;
    gpointer user_data;
    GDestroyNotify destroy_user_data;
} OvirtProxyGetCollectionAsyncData;

static void
ovirt_proxy_get_collection_async_data_destroy(OvirtProxyGetCollectionAsyncData *data)
{
    if (data->destroy_user_data != NULL) {
        data->destroy_user_data(data->user_data);
    }
    g_slice_free(OvirtProxyGetCollectionAsyncData, data);
}

static gboolean get_collection_xml_async_cb(OvirtProxy* proxy,
                                            RestProxyCall *call,
                                            gpointer user_data,
                                            GError **error)
{
    RestXmlNode *root;
    OvirtProxyGetCollectionAsyncData *data;
    gboolean parsed = FALSE;

    data = (OvirtProxyGetCollectionAsyncData *)user_data;

    root = ovirt_rest_xml_node_from_call(call);

    /* Do the parsing */
    g_warn_if_fail(data->parser != NULL);
    if (data->parser != NULL) {
        parsed = data->parser(proxy, root, data->user_data, error);
    }

    rest_xml_node_unref(root);

    return parsed;
}

/**
 * ovirt_proxy_get_collection_xml_async:
 * @proxy: a #OvirtProxy
 * @callback: (scope async): completion callback
 * @user_data: (closure): opaque data for callback
 */
void ovirt_proxy_get_collection_xml_async(OvirtProxy *proxy,
                                          const char *href,
                                          GSimpleAsyncResult *result,
                                          GCancellable *cancellable,
                                          OvirtProxyGetCollectionAsyncCb callback,
                                          gpointer user_data,
                                          GDestroyNotify destroy_func)
{
    OvirtProxyGetCollectionAsyncData *data;
    OvirtRestCall *call;

    data = g_slice_new0(OvirtProxyGetCollectionAsyncData);
    data->parser = callback;
    data->user_data = user_data;
    data->destroy_user_data = destroy_func;

    call = ovirt_rest_call_new(proxy, "GET", href);


    ovirt_rest_call_async(call, result, cancellable,
                          get_collection_xml_async_cb, data,
                          (GDestroyNotify)ovirt_proxy_get_collection_async_data_destroy);
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
    g_debug("CA certificate URI: %s", ca_uri);
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
    if (source == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_BAD_URI, "could not extract ca cert filename from URI");
        goto error;
    }

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
                        NULL);
}


static void ovirt_proxy_set_api_from_xml(OvirtProxy *proxy,
                                         RestXmlNode *node,
                                         GError **error)
{
    if (proxy->priv->api != NULL) {
        g_object_unref(G_OBJECT(proxy->priv->api));
    }
    proxy->priv->api = ovirt_api_new_from_xml(node, error);

}


/**
 * ovirt_proxy_fetch_api:
 * @proxy: a #OvirtProxy
 * @error: #GError to set on error, or NULL
 *
 * Return value: (transfer full):
 */
OvirtApi *ovirt_proxy_fetch_api(OvirtProxy *proxy, GError **error)
{
    RestXmlNode *api_node;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);

    api_node = ovirt_proxy_get_collection_xml(proxy, "", error);
    if (api_node == NULL) {
        return NULL;
    }

    ovirt_proxy_set_api_from_xml(proxy, api_node, error);

    rest_xml_node_unref(api_node);

    return proxy->priv->api;
}


static gboolean fetch_api_async_cb(OvirtProxy* proxy,
                                   RestXmlNode *root_node,
                                   gpointer user_data,
                                   GError **error)
{
    ovirt_proxy_set_api_from_xml(proxy, root_node, error);

    return TRUE;
}


/**
 * ovirt_proxy_fetch_api_async:
 * @proxy: a #OvirtProxy
 * @callback: (scope async): completion callback
 * @user_data: (closure): opaque data for callback
 */
void ovirt_proxy_fetch_api_async(OvirtProxy *proxy,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
    GSimpleAsyncResult *result;

    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    result = g_simple_async_result_new (G_OBJECT(proxy), callback,
                                        user_data,
                                        ovirt_proxy_fetch_api_async);
    ovirt_proxy_get_collection_xml_async(proxy, "", result, cancellable,
                                         fetch_api_async_cb, NULL, NULL);
}


/**
 * ovirt_proxy_fetch_api_finish:
 * @proxy: a #OvirtProxy
 * @result: (transfer none): async method result
 *
 * Return value: (transfer none): an #OvirtApi instance to interact with
 * oVirt/RHEV REST API.
 */
OvirtApi *
ovirt_proxy_fetch_api_finish(OvirtProxy *proxy,
                             GAsyncResult *result,
                             GError **err)
{
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);
    g_return_val_if_fail(g_simple_async_result_is_valid(result, G_OBJECT(proxy),
                                                        ovirt_proxy_fetch_api_async),
                         NULL);

    if (g_simple_async_result_propagate_error(G_SIMPLE_ASYNC_RESULT(result), err))
        return NULL;

    return proxy->priv->api;
}
