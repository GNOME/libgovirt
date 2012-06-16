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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 * Author: Christophe Fergeau <cfergeau@redhat.com>
 */

#include <config.h>

#include "ovirt-enum-types.h"
#include "ovirt-proxy.h"
#include "ovirt-rest-call.h"
#include "ovirt-vm.h"
#include "ovirt-vm-display.h"

#include <rest/rest-xml-node.h>
#include <rest/rest-xml-parser.h>
#include <stdlib.h>

typedef gboolean (*ActionResponseParser)(RestXmlNode *node, OvirtVm *vm, GError **error);
static gboolean parse_action_response(RestProxyCall *call, OvirtVm *vm,
                                      ActionResponseParser response_parser,
                                      GError **error);
static gboolean parse_ticket_status(RestXmlNode *root, OvirtVm *vm,
                                    GError **error);

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

void ovirt_vm_add_action(OvirtVm *vm, const char *action, const char *url)
{
    g_return_if_fail(OVIRT_IS_VM(vm));

    if (vm->priv->actions == NULL) {
        vm->priv->actions = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  g_free, g_free);
    }
    g_hash_table_insert(vm->priv->actions, g_strdup(action), g_strdup(url));
}

const char *ovirt_vm_get_action(OvirtVm *vm, const char *action)
{
    g_return_val_if_fail(OVIRT_IS_VM(vm), NULL);
    g_return_val_if_fail(vm->priv->actions != NULL, NULL);

    return g_hash_table_lookup(vm->priv->actions, action);
}

typedef struct {
    OvirtVmActionAsyncCallback async_cb;
    gpointer user_data;
    OvirtVm *vm;
    char *action;
    ActionResponseParser response_parser;
} OvirtVmActionData;

static void action_async_cb(RestProxyCall *call, const GError *librest_error,
                            GObject *weak_object, gpointer user_data)
{
    OvirtVmActionData *data = (OvirtVmActionData *)user_data;
    OvirtProxy *proxy = OVIRT_PROXY(weak_object);
    const GError *error;
    GError *action_error = NULL;

    g_return_if_fail(data != NULL);

    if (librest_error == NULL) {
        parse_action_response(call, data->vm, data->response_parser, &action_error);
        error = action_error;
    } else {
        error = librest_error;
    }

    if (data->async_cb != NULL) {
        data->async_cb(data->vm, proxy, error, data->user_data);
    }

    if (action_error != NULL) {
        g_error_free(action_error);
    }
    g_free(data->action);
    g_object_unref(G_OBJECT(data->vm));
    g_slice_free(OvirtVmActionData, data);
    g_object_unref(G_OBJECT(call));
}

static gboolean
ovirt_vm_action_async(OvirtVm *vm, OvirtProxy *proxy,
                      const char *action,
                      ActionResponseParser response_parser,
                      OvirtVmActionAsyncCallback async_cb,
                      gpointer user_data, GError **error)
{
    RestProxyCall *call;
    OvirtVmActionData *data;
    const char *function;

    g_return_val_if_fail(OVIRT_IS_VM(vm), FALSE);
    g_return_val_if_fail(action != NULL, FALSE);
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);
    g_return_val_if_fail((error == NULL) || (*error == NULL), FALSE);

    function = ovirt_vm_get_action(vm, action);
    g_return_val_if_fail(function != NULL, FALSE);

    call = REST_PROXY_CALL(ovirt_rest_call_new(REST_PROXY(proxy)));
    rest_proxy_call_set_method(call, "POST");
    rest_proxy_call_set_function(call, function);
    rest_proxy_call_add_param(call, "async", "false");
    data = g_slice_new(OvirtVmActionData);
    data->async_cb = async_cb;
    data->user_data = user_data;
    data->action = g_strdup(action);
    data->vm = g_object_ref(G_OBJECT(vm));
    data->response_parser = response_parser;

    if (!rest_proxy_call_async(call, action_async_cb, G_OBJECT(proxy),
                               data, error)) {
        g_warning("Error while running %s on %p", action, vm);
        g_free(data->action);
        g_object_unref(G_OBJECT(data->vm));
        g_slice_free(OvirtVmActionData, data);
        g_object_unref(G_OBJECT(call));
        return FALSE;
    }
    return TRUE;
}

/**
 * ovirt_vm_get_ticket_async:
 * @proxy: a #OvirtProxy
 * @vm: a #OvirtVM
 * @async_cb: (scope async): completion callback
 * @user_data: (closure): opaque data for callback
 */
gboolean
ovirt_vm_get_ticket_async(OvirtVm *vm, OvirtProxy *proxy,
                          OvirtVmActionAsyncCallback async_cb,
                          gpointer user_data, GError **error)
{
    return ovirt_vm_action_async(vm, proxy, "ticket",
                                 parse_ticket_status,
                                 async_cb, user_data,
                                 error);
}

/**
 * ovirt_vm_start_async:
 * @proxy: a #OvirtProxy
 * @vm: a #OvirtVM
 * @async_cb: (scope async): completion callback
 * @user_data: (closure): opaque data for callback
 */
gboolean
ovirt_vm_start_async(OvirtVm *vm, OvirtProxy *proxy,
                     OvirtVmActionAsyncCallback async_cb,
                     gpointer user_data, GError **error)
{
    return ovirt_vm_action_async(vm, proxy, "start", NULL,
                                 async_cb, user_data, error);
}

/**
 * ovirt_vm_stop_async:
 * @proxy: a #OvirtProxy
 * @vm: a #OvirtVM
 * @async_cb: (scope async): completion callback
 * @user_data: (closure): opaque data for callback
 */
gboolean
ovirt_vm_stop_async(OvirtVm *vm, OvirtProxy *proxy,
                    OvirtVmActionAsyncCallback async_cb,
                    gpointer user_data, GError **error)
{
    return ovirt_vm_action_async(vm, proxy, "stop", NULL,
                                 async_cb, user_data, error);
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
    g_return_val_if_fail(function != NULL, FALSE);

    call = REST_PROXY_CALL(ovirt_rest_call_new(REST_PROXY(proxy)));
    rest_proxy_call_set_method(call, "POST");
    rest_proxy_call_set_function(call, function);
    rest_proxy_call_add_param(call, "async", "false");

    if (!rest_proxy_call_sync(call, error)) {
        g_warning("Error while running %s on %p", action, vm);
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
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, NULL);
        g_return_val_if_reached(FALSE);
    }
    node = g_hash_table_lookup(root->children, value_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, NULL);
        g_return_val_if_reached(FALSE);
    }

    g_object_get(G_OBJECT(vm), "display", &display, NULL);
    g_return_val_if_fail(display != NULL, FALSE);
    g_object_set(G_OBJECT(display), "ticket", node->content, NULL);

    g_debug("Ticket: %s\n", node->content);

    node = g_hash_table_lookup(root->children, expiry_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, NULL);
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
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, NULL);
        g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
    }
    node = g_hash_table_lookup(node->children, state_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, NULL);
        g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
    }
    g_debug("State: %s\n", node->content);
    if (g_strcmp0(node->content, "complete") == 0) {
        return OVIRT_RESPONSE_COMPLETE;
    } else if (g_strcmp0(node->content, "pending") == 0) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_ACTION_FAILED, NULL);
        return OVIRT_RESPONSE_PENDING;
    } else if (g_strcmp0(node->content, "in_progress") == 0) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_ACTION_FAILED, NULL);
        return OVIRT_RESPONSE_IN_PROGRESS;
    } else if (g_strcmp0(node->content, "failed") == 0) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_ACTION_FAILED, NULL);
        return OVIRT_RESPONSE_FAILED;
    }

    g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, NULL);
    g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
}

static void parse_fault(RestXmlNode *root, GError **error)
{
    RestXmlNode *node;
    const char *reason_key = g_intern_string("reason");

    g_return_if_fail(g_strcmp0(root->name, "fault") == 0);

    node = g_hash_table_lookup(root->children, reason_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_PARSING_FAILED, NULL);
        g_return_if_reached();
    }
    g_debug("Reason: %s\n", node->content);
    g_set_error_literal(error, OVIRT_PROXY_ERROR, OVIRT_PROXY_FAULT, node->content);
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
        if (parse_action_status(root, error) == OVIRT_RESPONSE_COMPLETE) {
            if (response_parser) {
                result = response_parser(root, vm, error);
            } else {
                result = TRUE;
            }
        }
    } else if (g_strcmp0(root->name, "fault") == 0) {
        parse_fault(root, error);
    } else {
        g_warn_if_reached();
    }

    rest_xml_node_unref(root);
    g_object_unref(G_OBJECT(parser));

    return result;
}
