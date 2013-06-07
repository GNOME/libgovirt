/*
 * ovirt-vm.c: oVirt virtual machine
 *
 * Copyright (C) 2012 Red Hat, Inc.
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

#include <config.h>

#include "ovirt-enum-types.h"
#include "ovirt-proxy.h"
#include "ovirt-rest-call.h"
#include "ovirt-vm.h"
#include "ovirt-vm-display.h"
#include "ovirt-vm-private.h"

#include <rest/rest-xml-node.h>
#include <rest/rest-xml-parser.h>
#include <stdlib.h>
#include <string.h>

typedef gboolean (*ActionResponseParser)(RestXmlNode *node, OvirtVm *vm, GError **error);
static gboolean parse_action_response(RestProxyCall *call, OvirtVm *vm,
                                      ActionResponseParser response_parser,
                                      GError **error);
static gboolean parse_ticket_status(RestXmlNode *root, OvirtVm *vm,
                                    GError **error);

typedef void (*OvirtRestInvokeCallback)(RestProxyCall *call, GTask *result,
                                        gpointer user_data);
static void ovirt_rest_invoke_async(OvirtProxy *proxy,
                                    const char *method,
                                    const char *function,
                                    GTask *task,
                                    OvirtRestInvokeCallback callback,
                                    gpointer user_data,
                                    GDestroyNotify destroy_func);

#define OVIRT_VM_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), OVIRT_TYPE_VM, OvirtVmPrivate))

struct _OvirtVmPrivate {
    char *uuid;
    char *href;
    char *name;
    OvirtVmState state;
    GHashTable *actions;
    OvirtVmDisplay *display;
} ;
G_DEFINE_TYPE(OvirtVm, ovirt_vm, G_TYPE_OBJECT);

enum OvirtResponseStatus {
    OVIRT_RESPONSE_UNKNOWN,
    OVIRT_RESPONSE_FAILED,
    OVIRT_RESPONSE_PENDING,
    OVIRT_RESPONSE_IN_PROGRESS,
    OVIRT_RESPONSE_COMPLETE
};

ActionResponseParser response_parsers[] = {
    [OVIRT_VM_ACTION_TICKET] = parse_ticket_status
};

enum {
    PROP_0,
    PROP_UUID,
    PROP_HREF,
    PROP_NAME,
    PROP_STATE,
    PROP_DISPLAY
};

static void ovirt_vm_get_property(GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    OvirtVm *vm = OVIRT_VM(object);

    switch (prop_id) {
    case PROP_UUID:
        g_value_set_string(value, vm->priv->uuid);
        break;
    case PROP_HREF:
        g_value_set_string(value, vm->priv->href);
        break;
    case PROP_NAME:
        g_value_set_string(value, vm->priv->name);
        break;
    case PROP_STATE:
        g_value_set_enum(value, vm->priv->state);
        break;
    case PROP_DISPLAY:
        g_value_set_object(value, vm->priv->display);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void ovirt_vm_set_property(GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    OvirtVm *vm = OVIRT_VM(object);

    switch (prop_id) {
    case PROP_UUID:
        g_free(vm->priv->uuid);
        vm->priv->uuid = g_value_dup_string(value);
        break;
    case PROP_HREF:
        g_free(vm->priv->href);
        vm->priv->href = g_value_dup_string(value);
        break;
    case PROP_NAME:
        g_free(vm->priv->name);
        vm->priv->name = g_value_dup_string(value);
        break;
    case PROP_STATE:
        vm->priv->state = g_value_get_enum(value);
        break;
    case PROP_DISPLAY:
        if (vm->priv->display != NULL)
            g_object_unref(vm->priv->display);
        vm->priv->display = g_value_dup_object(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}


static void ovirt_vm_dispose(GObject *object)
{
    OvirtVm *vm = OVIRT_VM(object);

    if (vm->priv->actions != NULL) {
        g_hash_table_unref(vm->priv->actions);
        vm->priv->actions = NULL;
    }
    g_clear_object(&vm->priv->display);

    G_OBJECT_CLASS(ovirt_vm_parent_class)->dispose(object);
}

static void ovirt_vm_finalize(GObject *object)
{
    OvirtVm *vm = OVIRT_VM(object);

    g_free(vm->priv->name);
    g_free(vm->priv->href);
    g_free(vm->priv->uuid);

    G_OBJECT_CLASS(ovirt_vm_parent_class)->finalize(object);
}

static void ovirt_vm_class_init(OvirtVmClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(OvirtVmPrivate));

    object_class->dispose = ovirt_vm_dispose;
    object_class->finalize = ovirt_vm_finalize;
    object_class->get_property = ovirt_vm_get_property;
    object_class->set_property = ovirt_vm_set_property;

    g_object_class_install_property(object_class,
                                    PROP_UUID,
                                    g_param_spec_string("uuid",
                                                        "UUID",
                                                        "Virtual Machine UUID",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_HREF,
                                    g_param_spec_string("href",
                                                        "Href",
                                                        "Virtual Machine Href",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_NAME,
                                    g_param_spec_string("name",
                                                        "Name",
                                                        "Virtual Machine Name",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_STATE,
                                    g_param_spec_enum("state",
                                                      "State",
                                                      "Virtual Machine State",
                                                      OVIRT_TYPE_VM_STATE,
                                                      OVIRT_VM_STATE_DOWN,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_DISPLAY,
                                    g_param_spec_object("display",
                                                        "Display",
                                                        "Virtual Machine Display Information",
                                                        OVIRT_TYPE_VM_DISPLAY,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void ovirt_vm_init(G_GNUC_UNUSED OvirtVm *vm)
{
    vm->priv = OVIRT_VM_GET_PRIVATE(vm);
}

OvirtVm *ovirt_vm_new(void)
{
    return OVIRT_VM(g_object_new(OVIRT_TYPE_VM, NULL));
}

G_GNUC_INTERNAL void
ovirt_vm_add_action(OvirtVm *vm, const char *action, const char *url)
{
    g_return_if_fail(OVIRT_IS_VM(vm));

    if (vm->priv->actions == NULL) {
        vm->priv->actions = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  g_free, g_free);
    }
    g_hash_table_insert(vm->priv->actions, g_strdup(action), g_strdup(url));
}

static const char *
ovirt_vm_get_action(OvirtVm *vm, const char *action)
{
    g_return_val_if_fail(OVIRT_IS_VM(vm), NULL);
    g_return_val_if_fail(vm->priv->actions != NULL, NULL);

    return g_hash_table_lookup(vm->priv->actions, action);
}

static void
action_cancelled_cb (G_GNUC_UNUSED GCancellable *cancellable,
                     RestProxyCall *call)
{
    rest_proxy_call_cancel (call);
}

static void ovirt_vm_invoke_action_async_cb(RestProxyCall *call, GTask *task,
                                            gpointer user_data)
{
    OvirtVm *vm;
    GError *action_error = NULL;

    g_return_if_fail(REST_IS_PROXY_CALL(call));
    g_return_if_fail(G_IS_TASK(task));

    vm = OVIRT_VM(g_task_get_source_object(task));
    parse_action_response(call, vm, (ActionResponseParser)user_data, &action_error);
    if (action_error != NULL) {
        g_task_return_error(task, action_error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

static void
ovirt_vm_invoke_action_async(OvirtVm *vm,
                             const char *action,
                             OvirtProxy *proxy,
                             ActionResponseParser response_parser,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
    GTask *task;
    const char *function;

    g_return_if_fail(OVIRT_IS_VM(vm));
    g_return_if_fail(action != NULL);
    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    g_debug("invoking '%s' action on %p using %p", action, vm, proxy);
    function = ovirt_vm_get_action(vm, action);
    g_return_if_fail(function != NULL);

    task = g_task_new(vm, cancellable, callback, user_data);

    ovirt_rest_invoke_async(proxy, "POST", function,
                            task, ovirt_vm_invoke_action_async_cb,
                            response_parser, NULL);
}

static const char *ovirt_rest_strip_api_base_dir(const char *path)
{
    if (g_str_has_prefix(path, OVIRT_API_BASE_DIR)) {
        path += strlen(OVIRT_API_BASE_DIR);
    } else {
        /* action href should always be prefixed by /api/ */
        /* it would be easier to remove /api/ from the RestProxy base
         * URL but unfortunately I couldn't get this to work
         */
        g_warn_if_reached();
    }

    return path;
}

static gboolean
ovirt_vm_action_finish(OvirtVm *vm, GAsyncResult *result, GError **err)
{
    g_return_val_if_fail(OVIRT_IS_VM(vm), FALSE);
    g_return_val_if_fail(g_task_is_valid(result, vm), FALSE);

    return g_task_propagate_boolean(G_TASK (result), err);
}

void
ovirt_vm_get_ticket_async(OvirtVm *vm, OvirtProxy *proxy,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
    ovirt_vm_invoke_action_async(vm, "ticket", proxy, parse_ticket_status,
                                 cancellable, callback, user_data);
}

gboolean
ovirt_vm_get_ticket_finish(OvirtVm *vm, GAsyncResult *result, GError **err)
{
    return ovirt_vm_action_finish(vm, result, err);
}

void
ovirt_vm_start_async(OvirtVm *vm, OvirtProxy *proxy,
                     GCancellable *cancellable,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    ovirt_vm_invoke_action_async(vm, "start", proxy, NULL,
                                 cancellable, callback, user_data);
}

gboolean
ovirt_vm_start_finish(OvirtVm *vm, GAsyncResult *result, GError **err)
{
    return ovirt_vm_action_finish(vm, result, err);
}

void
ovirt_vm_stop_async(OvirtVm *vm, OvirtProxy *proxy,
                    GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
    ovirt_vm_invoke_action_async(vm, "stop", proxy, NULL,
                                 cancellable, callback, user_data);
}

gboolean
ovirt_vm_stop_finish(OvirtVm *vm, GAsyncResult *result, GError **err)
{
    return ovirt_vm_action_finish(vm, result, err);
}

static gboolean
ovirt_vm_action(OvirtVm *vm, OvirtProxy *proxy, const char *action,
                ActionResponseParser response_parser, GError **error)
{
    RestProxyCall *call;
    const char *function;

    g_return_val_if_fail(OVIRT_IS_VM(vm), FALSE);
    g_return_val_if_fail(action != NULL, FALSE);
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);
    g_return_val_if_fail((error == NULL) || (*error == NULL), FALSE);

    function = ovirt_vm_get_action(vm, action);
    function = ovirt_rest_strip_api_base_dir(function);
    g_return_val_if_fail(function != NULL, FALSE);

    call = REST_PROXY_CALL(ovirt_rest_call_new(REST_PROXY(proxy)));
    rest_proxy_call_set_method(call, "POST");
    rest_proxy_call_set_function(call, function);
    rest_proxy_call_add_param(call, "async", "false");

    if (!rest_proxy_call_sync(call, error)) {
        GError *call_error = NULL;
        g_warning("Error while running %s on %p", action, vm);
        /* Even in error cases we may have a response body describing
         * the failure, try to parse that */
        parse_action_response(call, vm, response_parser, &call_error);
        if (call_error != NULL) {
            g_clear_error(error);
            g_propagate_error(error, call_error);
        }

        g_object_unref(G_OBJECT(call));
        return FALSE;
    }

    parse_action_response(call, vm, response_parser, error);

    g_object_unref(G_OBJECT(call));

    return TRUE;
}

gboolean ovirt_vm_get_ticket(OvirtVm *vm, OvirtProxy *proxy, GError **error)
{
    return ovirt_vm_action(vm, proxy, "ticket",
                           parse_ticket_status,
                           error);
}

gboolean ovirt_vm_start(OvirtVm *vm, OvirtProxy *proxy, GError **error)
{
    return ovirt_vm_action(vm, proxy, "start", NULL, error);
}

gboolean ovirt_vm_stop(OvirtVm *vm, OvirtProxy *proxy, GError **error)
{
    return ovirt_vm_action(vm, proxy, "stop", NULL, error);
}

static gboolean parse_ticket_status(RestXmlNode *root, OvirtVm *vm, GError **error)
{
    RestXmlNode *node;
    const char *ticket_key = g_intern_string("ticket");
    const char *value_key = g_intern_string("value");
    const char *expiry_key = g_intern_string("expiry");
    OvirtVmDisplay *display;

    g_return_val_if_fail(root != NULL, FALSE);
    g_return_val_if_fail(vm != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    root = g_hash_table_lookup(root->children, ticket_key);
    if (root == NULL) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, "could not find 'ticket' node");
        g_return_val_if_reached(FALSE);
    }
    node = g_hash_table_lookup(root->children, value_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, "could not find 'value' node");
        g_return_val_if_reached(FALSE);
    }

    g_object_get(G_OBJECT(vm), "display", &display, NULL);
    g_return_val_if_fail(display != NULL, FALSE);
    g_object_set(G_OBJECT(display), "ticket", node->content, NULL);

    g_debug("Ticket: %s\n", node->content);

    node = g_hash_table_lookup(root->children, expiry_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, "could not find 'expiry' node");
        g_object_unref(G_OBJECT(display));
        g_return_val_if_reached(FALSE);
    }
    g_object_set(G_OBJECT(display),
                 "expiry", strtoul(node->content, NULL, 0),
                 NULL);
    g_object_unref(G_OBJECT(display));

    return TRUE;
}

static enum OvirtResponseStatus parse_action_status(RestXmlNode *root,
                                                    GError **error)
{
    RestXmlNode *node;
    const char *status_key = g_intern_string("status");
    const char *state_key = g_intern_string("state");

    g_return_val_if_fail(g_strcmp0(root->name, "action") == 0,
                         OVIRT_RESPONSE_UNKNOWN);
    g_return_val_if_fail(error == NULL || *error == NULL,
                         OVIRT_RESPONSE_UNKNOWN);

    node = g_hash_table_lookup(root->children, status_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, "could not find 'status' node");
        g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
    }
    node = g_hash_table_lookup(node->children, state_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, "could not find 'state' node");
        g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
    }
    g_debug("State: %s\n", node->content);
    if (g_strcmp0(node->content, "complete") == 0) {
        return OVIRT_RESPONSE_COMPLETE;
    } else if (g_strcmp0(node->content, "pending") == 0) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_ACTION_FAILED, "action is pending");
        return OVIRT_RESPONSE_PENDING;
    } else if (g_strcmp0(node->content, "in_progress") == 0) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_ACTION_FAILED, "action is in progress");
        return OVIRT_RESPONSE_IN_PROGRESS;
    } else if (g_strcmp0(node->content, "failed") == 0) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_ACTION_FAILED, "action has failed");
        return OVIRT_RESPONSE_FAILED;
    }

    g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, "unknown action failure");
    g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
}

static void parse_fault(RestXmlNode *root, GError **error)
{
    RestXmlNode *reason_node;
    RestXmlNode *detail_node;
    const char *reason_key = g_intern_string("reason");
    const char *detail_key = g_intern_string("detail");

    g_return_if_fail(g_strcmp0(root->name, "fault") == 0);

    reason_node = g_hash_table_lookup(root->children, reason_key);
    if (reason_node == NULL) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, "could not find 'reason' node");
        g_return_if_reached();
    }
    g_debug("Reason: %s\n", root->content);
    detail_node = g_hash_table_lookup(root->children, detail_key);
    g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_FAULT, "%s: %s", reason_node->content,
                (detail_node == NULL)?"":detail_node->content);
}

static gboolean
parse_action_response(RestProxyCall *call, OvirtVm *vm,
                      ActionResponseParser response_parser, GError **error)
{
    RestXmlParser *parser;
    RestXmlNode *root;
    gboolean result;

    result = FALSE;
    parser = rest_xml_parser_new ();

    root = rest_xml_parser_parse_from_data (parser,
            rest_proxy_call_get_payload (call),
            rest_proxy_call_get_payload_length (call));

    if (g_strcmp0(root->name, "action") == 0) {
        enum OvirtResponseStatus status;

        status = parse_action_status(root, error);
        if (status  == OVIRT_RESPONSE_COMPLETE) {
            if (response_parser) {
                result = response_parser(root, vm, error);
            } else {
                result = TRUE;
            }
        } if (status == OVIRT_RESPONSE_FAILED) {
            const char *fault_key = g_intern_string("fault");
            GError *fault_error = NULL;
            RestXmlNode *fault_node = NULL;

            fault_node = g_hash_table_lookup(root->children, fault_key);
            if (fault_node != NULL) {
                parse_fault(fault_node, &fault_error);
                if (fault_error != NULL) {
                    g_clear_error(error);
                    g_propagate_error(error, fault_error);
                }
            }
        }
    } else {
        g_warn_if_reached();
    }

    rest_xml_node_unref(root);
    g_object_unref(G_OBJECT(parser));

    return result;
}

typedef void (*OvirtRestInvokeCallback)(RestProxyCall *call, GTask *result,
                                        gpointer user_data);

typedef struct {
    OvirtRestInvokeCallback callback;
    gulong cancellable_cb_id;
    gpointer user_data;
    GDestroyNotify destroy_func;
} OvirtRestInvokeData;

static void ovirt_rest_invoke_data_free(OvirtRestInvokeData *data)
{
    if ((data->user_data != NULL) && (data->destroy_func != NULL)) {
        data->destroy_func(data->user_data);
    }
    g_slice_free(OvirtRestInvokeData, data);
}

static void invoke_async_cb(RestProxyCall *call, const GError *librest_error,
                            GObject *weak_object, gpointer user_data)
{
    GTask *task = G_TASK(user_data);
    OvirtRestInvokeData *data;

    g_return_if_fail(G_IS_TASK(user_data));
    data = g_task_get_task_data(task);
    g_warn_if_fail(data->callback != NULL);
    if (librest_error != NULL) {
        g_task_return_error(task, g_error_copy(librest_error));
    } else {
        data->callback(call, task, data->user_data);
    }

    if (g_task_get_cancellable(task) != NULL) {
        g_cancellable_disconnect(g_task_get_cancellable(task),
                                 data->cancellable_cb_id);
        data->cancellable_cb_id = 0;
    }
    g_object_unref(task);
}

static void ovirt_rest_invoke_async(OvirtProxy *proxy,
                                    const char *method,
                                    const char *function,
                                    GTask *task,
                                    OvirtRestInvokeCallback callback,
                                    gpointer user_data,
                                    GDestroyNotify destroy_func)
{
    RestProxyCall *call;
    GError *error = NULL;
    OvirtRestInvokeData *data;
    GCancellable *cancellable;

    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail(function != NULL);
    g_return_if_fail(G_IS_TASK(task));

    g_debug("invoking '%s' function using %p", function, proxy);

    call = REST_PROXY_CALL(ovirt_rest_call_new(REST_PROXY(proxy)));
    function = ovirt_rest_strip_api_base_dir(function);
    if (method != NULL) {
        rest_proxy_call_set_method(call, method);
    }
    rest_proxy_call_set_function(call, function);
    rest_proxy_call_add_param(call, "async", "false");

    data = g_slice_new0(OvirtRestInvokeData);
    data->callback = callback;
    data->user_data = user_data;
    data->destroy_func = destroy_func;
    cancellable = g_task_get_cancellable(task);
    if (cancellable != NULL) {
        data->cancellable_cb_id = g_cancellable_connect(cancellable,
                                                        G_CALLBACK (action_cancelled_cb),
                                                        call, NULL);
    }
    g_task_set_task_data(task, data, (GDestroyNotify)ovirt_rest_invoke_data_free);

    if (!rest_proxy_call_async(call, invoke_async_cb, G_OBJECT(proxy),
                               task, &error)) {
        g_warning("Error while running %s on %p", function, proxy);
        if (cancellable != NULL) {
            g_cancellable_disconnect(cancellable, data->cancellable_cb_id);
            data->cancellable_cb_id = 0;
        }
        g_task_return_error(task, error);
        g_object_unref(task);
    }
    g_object_unref(G_OBJECT(call));
}

static void ovirt_vm_refresh_async_cb(RestProxyCall *call, GTask *task,
                                      gpointer user_data)
{
    OvirtVm *vm;
    RestXmlParser *parser;
    RestXmlNode *root;
    gboolean refreshed;

    g_return_if_fail(REST_IS_PROXY_CALL(call));
    g_return_if_fail(G_IS_TASK(task));

    parser = rest_xml_parser_new ();
    root = rest_xml_parser_parse_from_data (parser,
            rest_proxy_call_get_payload (call),
            rest_proxy_call_get_payload_length (call));

    vm = OVIRT_VM(g_task_get_source_object(task));
    refreshed = ovirt_vm_refresh_from_xml(vm, root);

    g_task_return_boolean(task, refreshed);

    rest_xml_node_unref(root);
    g_object_unref(G_OBJECT(parser));
}

void ovirt_vm_refresh_async(OvirtVm *vm, OvirtProxy *proxy,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
    GTask *task;

    g_return_if_fail(OVIRT_IS_VM(vm));
    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    task = g_task_new(vm, cancellable, callback, user_data);

    ovirt_rest_invoke_async(proxy, "GET", vm->priv->href,
                            task, ovirt_vm_refresh_async_cb,
                            NULL, NULL);
}

gboolean ovirt_vm_refresh_finish(OvirtVm *vm,
                                 GAsyncResult *result,
                                 GError **err)
{
    g_return_val_if_fail(g_task_is_valid(result, vm), FALSE);

    return g_task_propagate_boolean(G_TASK (result), err);
}
