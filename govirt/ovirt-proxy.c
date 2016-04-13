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
#include "ovirt-rest-call-error.h"
#include "ovirt-proxy.h"
#include "ovirt-proxy-private.h"
#include "ovirt-rest-call.h"
#include "ovirt-vm.h"
#include "ovirt-vm-display.h"
#include "govirt-private.h"

#include <errno.h>
#include <string.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <libsoup/soup-cookie.h>
#include <rest/rest-xml-node.h>
#include <rest/rest-xml-parser.h>

G_DEFINE_TYPE (OvirtProxy, ovirt_proxy, REST_TYPE_PROXY);

#define OVIRT_PROXY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), OVIRT_TYPE_PROXY, OvirtProxyPrivate))

enum {
    PROP_0,
    PROP_CA_CERT,
    PROP_ADMIN,
    PROP_SESSION_ID,
    PROP_SSO_TOKEN
};

#define CA_CERT_FILENAME "ca.crt"

static gboolean set_ca_cert_from_data(OvirtProxy *proxy,
                                      char *ca_cert_data,
                                      gsize ca_cert_len);
static GByteArray *get_ca_cert_data(OvirtProxy *proxy);
static void ovirt_proxy_set_tmp_ca_file(OvirtProxy *proxy, const char *ca_file);

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


static RestProxyCall *ovirt_rest_call_new(OvirtProxy *proxy,
                                          const char *method,
                                          const char *href)
{
    RestProxyCall *call;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);

    call = REST_PROXY_CALL(ovirt_action_rest_call_new(REST_PROXY(proxy)));
    if (method != NULL) {
        rest_proxy_call_set_method(call, method);
    }
    rest_proxy_call_set_function(call, href);
    /* FIXME: to set or not to set ?? */
    rest_proxy_call_add_header(call, "All-Content", "true");

    return call;
}


RestXmlNode *ovirt_proxy_get_collection_xml(OvirtProxy *proxy,
                                            const char *href,
                                            GError **error)
{
    RestProxyCall *call;
    RestXmlNode *root;
    GError *err = NULL;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);

    call = ovirt_rest_call_new(proxy, "GET", href);

    if (!rest_proxy_call_sync(call, &err)) {
        if (g_error_matches(err, REST_PROXY_ERROR, REST_PROXY_ERROR_CANCELLED)) {
            g_set_error_literal(error,
                                OVIRT_REST_CALL_ERROR, OVIRT_REST_CALL_ERROR_CANCELLED,
                                err->message);
            g_clear_error(&err);
        } else if (err != NULL) {
            g_warning("Error while getting collection: %s", err->message);
            g_propagate_error(error, err);
        } else {
            g_warning("Error while getting collection");
        }
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
        gboolean callback_result = TRUE;

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


void ovirt_rest_call_async(OvirtRestCall *call,
                           GSimpleAsyncResult *result,
                           GCancellable *cancellable,
                           OvirtProxyCallAsyncCb callback,
                           gpointer user_data,
                           GDestroyNotify destroy_func)
{
    OvirtProxy *proxy;
    GError *error = NULL;
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
        data->cancellable = cancellable;
        data->cancellable_cb_id = g_cancellable_connect(cancellable,
                                                        G_CALLBACK (call_async_cancelled_cb),
                                                        call, NULL);
    }

    if (!rest_proxy_call_async(REST_PROXY_CALL(call), call_async_cb, NULL,
                               data, &error)) {
        g_warning("Error while getting collection XML");
        g_simple_async_result_set_from_error(result, error);
        g_simple_async_result_complete(result);
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
    RestProxyCall *call;

    data = g_slice_new0(OvirtProxyGetCollectionAsyncData);
    data->parser = callback;
    data->user_data = user_data;
    data->destroy_user_data = destroy_func;

    call = ovirt_rest_call_new(proxy, "GET", href);

    ovirt_rest_call_async(OVIRT_REST_CALL(call), result, cancellable,
                          get_collection_xml_async_cb, data,
                          (GDestroyNotify)ovirt_proxy_get_collection_async_data_destroy);
    g_object_unref(call);
}


static GFile *get_ca_cert_file(OvirtProxy *proxy)
{
    gchar *base_uri = NULL;
    gchar *ca_uri = NULL;
    GFile *ca_file = NULL;

    g_object_get(G_OBJECT(proxy), "url-format", &base_uri, NULL);
    if (base_uri == NULL)
        goto error;

    ca_uri = g_build_filename(base_uri, CA_CERT_FILENAME, NULL);
    g_debug("CA certificate URI: %s", ca_uri);
    ca_file = g_file_new_for_uri(ca_uri);

error:
    g_free(base_uri);
    g_free(ca_uri);
    return ca_file;
}

static GByteArray *get_ca_cert_data(OvirtProxy *proxy)
{
    char *ca_file = NULL;
    char *content;
    gsize length;
    GError *error = NULL;
    gboolean read_success;

    g_object_get(G_OBJECT(proxy), "ssl-ca-file", &ca_file, NULL);
    if (ca_file == NULL) {
        return NULL;
    }
    read_success = g_file_get_contents(ca_file, &content, &length, &error);
    if (!read_success) {
        if (error != NULL) {
            g_warning("Couldn't read %s: %s", ca_file, error->message);
        } else {
            g_warning("Couldn't read %s", ca_file);
        }
        g_free(ca_file);
        return NULL;
    }
    g_free(ca_file);

    return g_byte_array_new_take((guchar *)content, length);
}

static void ovirt_proxy_free_tmp_ca_file(OvirtProxy *proxy)
{
    if (proxy->priv->tmp_ca_file != NULL) {
        int unlink_failed;
        unlink_failed = g_unlink(proxy->priv->tmp_ca_file);
        if (unlink_failed == -1) {
            g_warning("Failed to unlink '%s'", proxy->priv->tmp_ca_file);
        }
        g_free(proxy->priv->tmp_ca_file);
        proxy->priv->tmp_ca_file = NULL;
    }
}

static void ovirt_proxy_set_tmp_ca_file(OvirtProxy *proxy, const char *ca_file)
{
    ovirt_proxy_free_tmp_ca_file(proxy);
    proxy->priv->tmp_ca_file = g_strdup(ca_file);
    if (ca_file != NULL) {
        /* We block invokations of ssl_ca_file_changed() using the 'setting_ca_file' boolean
         * g_signal_handler_{un,}block is not working well enough as
         * ovirt_proxy_set_tmp_ca_file() can be called as part of a g_object_set call,
         * and unblocking "notify::ssl-ca-file" right after setting its value
         * is not enough to prevent ssl_ca_file_changed() from running.
         */
        proxy->priv->setting_ca_file = TRUE;
        g_object_set(G_OBJECT(proxy), "ssl-ca-file", ca_file, NULL);
    }
}


#if GLIB_CHECK_VERSION(2, 32, 0)
static char *write_to_tmp_file(const char *template,
                               const char *data,
                               gsize data_len,
                               GError **error)
{
    GFile *tmp_file = NULL;
    GFileIOStream *iostream = NULL;
    GOutputStream *output;
    gboolean write_ok;
    char *result = NULL;

    tmp_file = g_file_new_tmp(template, &iostream, error);
    if (tmp_file == NULL) {
        goto end;
    }
    output = g_io_stream_get_output_stream(G_IO_STREAM(iostream));
    g_return_val_if_fail(output != NULL, FALSE);
    write_ok = g_output_stream_write_all(output, data, data_len,
                                         NULL, NULL, error);
    if (!write_ok) {
        goto end;
    }

    result = g_file_get_path(tmp_file);

end:
    if (tmp_file != NULL) {
        g_object_unref(G_OBJECT(tmp_file));
    }
    if (iostream != NULL) {
        g_object_unref(G_OBJECT(iostream));
    }

    return result;
}
#else
static char *write_to_tmp_file(const char *template,
                               const char *data,
                               gsize data_len,
                               GError **error)
{
    int fd = -1;
    char *tmp_file = NULL;
    char *result = NULL;

    fd = g_file_open_tmp(template, &tmp_file, error);
    if (fd == -1) {
        goto end;
    }

    while (data_len != 0) {
        ssize_t bytes_written;
        bytes_written = write(fd, data, data_len);
        if (bytes_written == -1) {
            if ((errno != EINTR) || (errno != EAGAIN)) {
                g_set_error(error, G_FILE_ERROR,
                            g_file_error_from_errno(errno),
                            _("Failed to write to '%s': %s"),
                            tmp_file, strerror(errno));
                goto end;
            }
        }
        g_assert(bytes_written <= (ssize_t)data_len);
        data_len -= bytes_written;
    }

    result = tmp_file;
    tmp_file = NULL;

end:
    if (fd != -1) {
        int close_status = close(fd);
        if (close_status != 0) {
            g_set_error(error, G_FILE_ERROR,
                        g_file_error_from_errno(errno),
                        _("Failed to close '%s': %s"),
                        result, strerror(errno));
            g_free(result);
            result = NULL;
        }
    }
    g_free(tmp_file);
    return result;
}
#endif


static gboolean set_ca_cert_from_data(OvirtProxy *proxy,
                                      char *ca_cert_data,
                                      gsize ca_cert_len)
{
    /* This function is quite complicated for historical reasons, we
     * initially only had a ca-cert property in OvirtProxy with type
     * GByteArray. RestProxy then got a ssl-ca-file property storing a
     * filename. We want to use ssl-ca-file as the canonical property,
     * and set ca-cert value from it. However, when the user sets
     * the ca-cert property, we need to create a temporary file in order
     * to be able to keep the ssl-ca-file property synced with it
     */
    char *ca_file_path = NULL;
    GError *error = NULL;
    gboolean result = FALSE;

    if (ca_cert_data != NULL) {
        ca_file_path = write_to_tmp_file("govirt-ca-XXXXXX.crt",
                                         ca_cert_data, ca_cert_len,
                                         &error);
        if (ca_file_path == NULL) {
            g_warning("Failed to create temporary file for CA certificate: %s",
                      error->message);
            goto end;
        }
    } else {
        ca_file_path = NULL;
    }

    ovirt_proxy_set_tmp_ca_file(proxy, ca_file_path);
    g_free(ca_file_path);

    g_object_notify(G_OBJECT(proxy), "ca-cert");
    result = TRUE;

end:
    g_clear_error(&error);
    return result;
}


static void ovirt_proxy_update_vm_display_ca(OvirtProxy *proxy)
{
    GList *vms;
    GList *it;

    vms = ovirt_proxy_get_vms_internal(proxy);
    for (it = vms; it != NULL; it = it->next) {
        OvirtVm *vm = OVIRT_VM(it->data);
        OvirtVmDisplay *display;

        g_object_get(G_OBJECT(vm), "display", &display, NULL);
        if (display != NULL) {
            g_object_set(G_OBJECT(display),
                         "ca-cert", proxy->priv->display_ca,
                         NULL);
            g_object_unref(display);
        } else {
            char *name;
            g_object_get(vm, "name", &name, NULL);
            g_debug("Not setting display CA for '%s' since it has no display",  name);
            g_free(name);
        }
    }
    g_list_free(vms);
}


static void set_display_ca_cert_from_data(OvirtProxy *proxy,
                                          char *ca_cert_data,
                                          gsize ca_cert_len)

{
    if (proxy->priv->display_ca != NULL)
        g_byte_array_unref(proxy->priv->display_ca);

    proxy->priv->display_ca = g_byte_array_new_take((guint8 *)ca_cert_data,
                                                    ca_cert_len);

    /* While the fetched CA certificate has historically been used both as the CA
     * certificate used during REST API communication and as the one to use for
     * SPICE communication, this function really fetches the one meant for SPICE
     * communication. The one used for REST API communication may or may not be
     * the same.
     * An OvirtVmDisplay::ca-cert property has been added when this design issue
     * became apparent, so we need to update this property after fetching the
     * certificate.
     */
    ovirt_proxy_update_vm_display_ca(proxy);
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
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_BAD_URI,
                    _("Could not extract CA certificate filename from URI"));
        goto error;
    }

    load_ok = g_file_load_contents(source, NULL,
                                   &cert_data,
                                   &cert_length,
                                   NULL, error);
    if (!load_ok)
        goto error;

    set_ca_cert_from_data(proxy, cert_data, cert_length);
    /* takes ownership of cert_data */
    set_display_ca_cert_from_data(proxy, cert_data, cert_length);

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
        goto end;
    }

    proxy = g_async_result_get_source_object(G_ASYNC_RESULT(fetch_result));

    set_ca_cert_from_data(OVIRT_PROXY(proxy), cert_data, cert_length);
    /* takes ownership of cert_data */
    set_display_ca_cert_from_data(OVIRT_PROXY(proxy),
                                  cert_data, cert_length);
    g_object_unref(proxy);
    g_simple_async_result_set_op_res_gboolean(fetch_result, TRUE);

end:
    g_simple_async_result_complete (fetch_result);
    g_object_unref(fetch_result);
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

    return get_ca_cert_data(proxy);
}

static void ovirt_proxy_get_property(GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    OvirtProxy *proxy = OVIRT_PROXY(object);

    switch (prop_id) {
    case PROP_CA_CERT:
        g_value_take_boxed(value, get_ca_cert_data(proxy));
        break;
    case PROP_ADMIN:
        g_value_set_boolean(value, proxy->priv->admin_mode);
        break;
    case PROP_SESSION_ID:
        g_value_set_string(value, proxy->priv->jsessionid);
        break;
    case PROP_SSO_TOKEN:
        g_value_set_string(value, proxy->priv->sso_token);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}


static void ovirt_proxy_set_session_id(OvirtProxy *proxy, const char *session_id)
{
    char *url;
    char *domain;

    g_object_get(G_OBJECT(proxy), "url-format", &url, NULL);
    g_return_if_fail(url != NULL);
    if (g_str_has_prefix(url, "https://")) {
        domain = url + strlen("https://");
    } else {
        domain = url;
    }

    if (proxy->priv->jsessionid_cookie != NULL) {
        soup_cookie_jar_delete_cookie(proxy->priv->cookie_jar,
                proxy->priv->jsessionid_cookie);
        proxy->priv->jsessionid_cookie = NULL;
    }
    g_free(proxy->priv->jsessionid);
    proxy->priv->jsessionid = g_strdup(session_id);
    if (proxy->priv->jsessionid != NULL) {
        SoupCookie *cookie;
        cookie = soup_cookie_new("JSESSIONID", session_id, domain, "/ovirt-engine/api", -1);
        soup_cookie_jar_add_cookie(proxy->priv->cookie_jar, cookie);
        proxy->priv->jsessionid_cookie = cookie;
        ovirt_proxy_add_header(proxy, "Prefer", "persistent-auth");
    } else {
        ovirt_proxy_add_header(proxy, "Prefer", NULL);
    }
    g_free(url);
}

static void ovirt_proxy_set_sso_token(OvirtProxy *proxy, const char *sso_token)
{
    char *header_value;

    g_free(proxy->priv->sso_token);
    proxy->priv->sso_token = g_strdup(sso_token);

    header_value = g_strdup_printf("Bearer %s", sso_token);
    ovirt_proxy_add_header(proxy, "Authorization", header_value);
    g_free(header_value);
}


static void ovirt_proxy_set_property(GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    OvirtProxy *proxy = OVIRT_PROXY(object);

    switch (prop_id) {
    case PROP_CA_CERT: {
        GByteArray *ca_cert;
        ca_cert = g_value_get_boxed(value);
        if (ca_cert != NULL) {
            set_ca_cert_from_data(proxy, (char *)ca_cert->data, ca_cert->len);
        } else {
            set_ca_cert_from_data(proxy, NULL, 0);
        }
        break;
    }

    case PROP_ADMIN:
        proxy->priv->admin_mode = g_value_get_boolean(value);
        break;

    case PROP_SESSION_ID:
        ovirt_proxy_set_session_id(proxy, g_value_get_string(value));
        break;

    case PROP_SSO_TOKEN:
        ovirt_proxy_set_sso_token(proxy, g_value_get_string(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ovirt_proxy_dispose(GObject *obj)
{
    OvirtProxy *proxy = OVIRT_PROXY(obj);

    if (proxy->priv->cookie_jar) {
        g_object_unref(G_OBJECT(proxy->priv->cookie_jar));
        proxy->priv->cookie_jar = NULL;
    }

    g_warn_if_fail(proxy->priv->additional_headers != NULL);
    g_hash_table_unref(proxy->priv->additional_headers);
    proxy->priv->additional_headers = NULL;

    if (proxy->priv->api != NULL) {
        g_object_unref(proxy->priv->api);
        proxy->priv->api = NULL;
    }

    if (proxy->priv->display_ca != NULL) {
        g_byte_array_unref(proxy->priv->display_ca);
        proxy->priv->display_ca = NULL;
    }

    G_OBJECT_CLASS(ovirt_proxy_parent_class)->dispose(obj);
}

static void
ovirt_proxy_finalize(GObject *obj)
{
    OvirtProxy *proxy = OVIRT_PROXY(obj);

    ovirt_proxy_free_tmp_ca_file(proxy);
    g_free(proxy->priv->jsessionid);
    g_free(proxy->priv->sso_token);

    G_OBJECT_CLASS(ovirt_proxy_parent_class)->finalize(obj);
}


static void ovirt_proxy_constructed(GObject *gobject)
{
    if (g_getenv("GOVIRT_NO_SSL_STRICT") != NULL) {
        g_warning("Disabling strict checking of SSL certificates");
        g_object_set(OVIRT_PROXY(gobject), "ssl-strict", FALSE, NULL);
    }

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(ovirt_proxy_parent_class)->constructed)
        G_OBJECT_CLASS(ovirt_proxy_parent_class)->constructed(gobject);
}


static void
ovirt_proxy_class_init(OvirtProxyClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS(klass);

    oclass->dispose = ovirt_proxy_dispose;
    oclass->finalize = ovirt_proxy_finalize;
    oclass->constructed = ovirt_proxy_constructed;

    oclass->get_property = ovirt_proxy_get_property;
    oclass->set_property = ovirt_proxy_set_property;

    /**
     * OvirtProxy:ca-cert:
     *
     * Path to a file containing the CA certificates to use for the HTTPS
     * REST API communication with the oVirt instance
     */
    g_object_class_install_property(oclass,
                                    PROP_CA_CERT,
                                    g_param_spec_boxed("ca-cert",
                                                       "ca-cert",
                                                       "Virt CA certificate to use for HTTPS REST communication",
                                                        G_TYPE_BYTE_ARRAY,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

    /**
     * OvirtProxy:admin:
     *
     * Indicates whether to connect to the REST API as an admin, or as a regular user.
     * Different content will be shown for the same user depending on if they connect as
     * an admin or not. Connecting as an admin requires to have admin priviledges on the
     * oVirt instance.
     *
     * Since: 0.0.2
     */
    g_object_class_install_property(oclass,
                                    PROP_ADMIN,
                                    g_param_spec_boolean("admin",
                                                         "admin",
                                                         "Use REST API as an admin",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));
    /**
     * OvirtProxy:session-id:
     *
     * jsessionid cookie value. This allows to use the REST API without
     * authenticating first. This was used by oVirt 3.6 and is now replaced
     * by OvirtProxy:sso-token.
     *
     * Since: 0.3.1
     */
    g_object_class_install_property(oclass,
                                    PROP_SESSION_ID,
                                    g_param_spec_string("session-id",
                                                        "session-id",
                                                        "oVirt/RHEV JSESSIONID",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

    /**
     * OvirtProxy:sso-token:
     *
     * Token to use for SSO. This allows to use the REST API without
     * authenticating first. This is used starting with oVirt 4.0.
     *
     * Since: 0.3.4
     */
    g_object_class_install_property(oclass,
                                    PROP_SSO_TOKEN,
                                    g_param_spec_string("sso-token",
                                                        "sso-token",
                                                        "oVirt/RHEV SSO token",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

    g_type_class_add_private(klass, sizeof(OvirtProxyPrivate));
}

static void ssl_ca_file_changed(GObject *gobject,
                                GParamSpec *pspec,
                                gpointer user_data)
{
    OvirtProxy *proxy = OVIRT_PROXY(gobject);
    if (proxy->priv->setting_ca_file) {
        proxy->priv->setting_ca_file = FALSE;
        return;
    }
    ovirt_proxy_free_tmp_ca_file(OVIRT_PROXY(gobject));
}

static void
ovirt_proxy_init(OvirtProxy *self)
{
    gulong handler_id;

    self->priv = OVIRT_PROXY_GET_PRIVATE(self);

    handler_id = g_signal_connect(G_OBJECT(self), "notify::ssl-ca-file",
                                  (GCallback)ssl_ca_file_changed, NULL);
    self->priv->ssl_ca_file_changed_id = handler_id;
    self->priv->cookie_jar = soup_cookie_jar_new();
    rest_proxy_add_soup_feature(REST_PROXY(self),
                                SOUP_SESSION_FEATURE(self->priv->cookie_jar));
    self->priv->additional_headers = g_hash_table_new_full(g_str_hash,
                                                           g_str_equal,
                                                           g_free,
                                                           g_free);
}

/* FIXME : "uri" should just be a base domain, foo.example.com/some/path
 * govirt will then prepend https://
 * /api/ is part of what librest call 'function'
 * -> get rid of all the /api/ stripping here and there
 */
OvirtProxy *ovirt_proxy_new(const char *hostname)
{
    char *uri;
    OvirtProxy *proxy;
    gsize suffix_len = 0;
    int i;

    if (!g_str_has_prefix(hostname, "http://") && !g_str_has_prefix(hostname, "https://")) {
        if (g_getenv("GOVIRT_DISABLE_HTTPS") != NULL) {
            g_warning("Using plain text HTTP connection");
            uri = g_strconcat("http://", hostname, NULL);
        } else {
            uri = g_strconcat("https://", hostname, NULL);
        }
    } else {
        /* Fallback code for backwards API compat, early libgovirt versions
         * expected a full fledged URI */
        g_warning("Passing a full http:// or https:// URI to "
                  "ovirt_proxy_new() is deprecated");
        uri = g_strdup(hostname);
    }

    /* More backwards compat code, strip "/api" from URI */
    if (g_str_has_suffix(uri, "api")) {
        suffix_len = strlen("api");
    } else if (g_str_has_suffix(uri, "/api")) {
        suffix_len = strlen("/api");
    } else if (g_str_has_suffix(uri, "/api/")) {
        suffix_len = strlen("/api/");
    }
    if (suffix_len != 0) {
        g_warning("Passing an URI ending in /api to ovirt_proxy_new() "
                  "is deprecated");
        uri[strlen(uri) - suffix_len] = '\0';
    }

    /* Strip trailing '/' */
    for (i = strlen(uri)-1; i >= 0; i--) {
        if (uri[i] != '/') {
            break;
        }
        uri[i] = '\0';
    }

    /* We disable cookies upon OvirtProxy creation as we will be
     * adding our own cookie jar to OvirtProxy
     */
    proxy =  OVIRT_PROXY(g_object_new(OVIRT_TYPE_PROXY,
                                      "url-format", uri,
                                      "disable-cookies", TRUE,
                                      NULL));
    g_free(uri);

    return proxy;
}


static void vm_collection_changed(GObject *gobject,
                                  GParamSpec *pspec,
                                  gpointer user_data)
{
    ovirt_proxy_update_vm_display_ca(OVIRT_PROXY(user_data));
}


/**
 * ovirt_proxy_add_header:
 * @proxy: a #OvirtProxy
 * @header: name of the header to add to each request
 * @value: value of the header to add to each request
 *
 * Add a http header called @header with the value @value to each oVirt REST
 * API call. If a header with this name already exists, the new value will
 * replace the old. If @value is NULL then the header will be removed.
 */
void ovirt_proxy_add_header(OvirtProxy *proxy, const char *header, const char *value)
{
    g_return_if_fail(OVIRT_IS_PROXY(proxy));

    if (value != NULL) {
        g_hash_table_replace(proxy->priv->additional_headers,
                             g_strdup(header),
                             g_strdup(value));
    } else {
        g_hash_table_remove(proxy->priv->additional_headers, header);
    }
}


/**
 * ovirt_proxy_add_headers:
 * @proxy: a #OvirtProxy
 * @...: header name and value pairs, followed by %NULL
 *
 * Add the specified http header and value pairs to @proxy. These headers will
 * be sent with each oVirt REST API call. If a header already exists, the new
 * value will replace the old.
 */
void ovirt_proxy_add_headers(OvirtProxy *proxy, ...)
{
    va_list headers;

    g_return_if_fail(OVIRT_IS_PROXY(proxy));

    va_start(headers, proxy);
    ovirt_proxy_add_headers_from_valist(proxy, headers);
    va_end(headers);
}


/**
 * ovirt_proxy_add_headers_from_valist:
 * @proxy: a #OvirtProxy
 * @headers: header name and value pairs
 *
 * Add the specified http header and value pairs to @proxy. These headers will
 * be sent with each oVirt REST API call. If a header already exists, the new
 * value will replace the old.
 */
void ovirt_proxy_add_headers_from_valist(OvirtProxy *proxy, va_list headers)
{
    const char *header = NULL;
    const char *value;

    g_return_if_fail(OVIRT_IS_PROXY(proxy));

    header = va_arg(headers, const char *);
    while (header != NULL) {
        value = va_arg(headers, const char *);
        ovirt_proxy_add_header(proxy, header, value);
        header = va_arg(headers, const char *);
    }
}


void ovirt_proxy_append_additional_headers(OvirtProxy *proxy,
                                           RestProxyCall *call)
{
    GHashTableIter iter;
    gpointer key;
    gpointer value;

    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail(REST_IS_PROXY_CALL(call));

    g_hash_table_iter_init(&iter, proxy->priv->additional_headers);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
        rest_proxy_call_add_header(call, key, value);
    }
}


static void ovirt_proxy_set_api_from_xml(OvirtProxy *proxy,
                                         RestXmlNode *node,
                                         GError **error)
{
    OvirtCollection *vms;

    if (proxy->priv->api != NULL) {
        g_object_unref(G_OBJECT(proxy->priv->api));
    }
    proxy->priv->api = ovirt_api_new_from_xml(node, error);

    vms = ovirt_api_get_vms(proxy->priv->api);
    g_return_if_fail(vms != NULL);
    g_signal_connect(G_OBJECT(vms), "notify::resources",
                     (GCallback)vm_collection_changed, proxy);
}


/**
 * ovirt_proxy_fetch_api:
 * @proxy: a #OvirtProxy
 * @error: #GError to set on error, or NULL
 *
 * Return value: (transfer none):
 */
OvirtApi *ovirt_proxy_fetch_api(OvirtProxy *proxy, GError **error)
{
    RestXmlNode *api_node;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);

    api_node = ovirt_proxy_get_collection_xml(proxy, "/ovirt-engine/api", error);
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
    ovirt_proxy_get_collection_xml_async(proxy, "/ovirt-engine/api", result, cancellable,
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


/**
 * ovirt_proxy_get_api:
 *
 * Gets the api entry point to access remote oVirt resources and collections.
 * This method does not initiate any network activity, the remote API entry point
 * must have been fetched with ovirt_proxy_fetch_api() or
 * ovirt_proxy_fetch_api_async() before calling this function.
 *
 * Return value: (transfer none): an #OvirtApi instance used to interact with
 * oVirt REST API.
 */
OvirtApi *
ovirt_proxy_get_api(OvirtProxy *proxy)
{
    return proxy->priv->api;
}


GList *ovirt_proxy_get_vms_internal(OvirtProxy *proxy)
{
    OvirtApi *api;
    OvirtCollection *vm_collection;
    GHashTable *vms;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);

    api = ovirt_proxy_get_api(proxy);
    if (api == NULL)
        return NULL;

    vm_collection = ovirt_api_get_vms(api);
    if (vm_collection == NULL)
        return NULL;

    vms = ovirt_collection_get_resources(vm_collection);
    if (vms == NULL)
        return NULL;

    return g_hash_table_get_values(vms);
}
