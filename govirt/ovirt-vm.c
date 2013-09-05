/*
 * ovirt-vm.c: oVirt virtual machine
 *
 * Copyright (C) 2012, 2013 Red Hat, Inc.
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

#include <stdlib.h>
#include <rest/rest-xml-node.h>
#include <rest/rest-xml-parser.h>

#include "govirt.h"
#include "govirt-private.h"


typedef gboolean (*ActionResponseParser)(RestXmlNode *node, OvirtVm *vm, GError **error);
static gboolean parse_action_response(RestProxyCall *call, OvirtVm *vm,
                                      ActionResponseParser response_parser,
                                      GError **error);
static gboolean parse_ticket_status(RestXmlNode *root, OvirtVm *vm,
                                    GError **error);

#define OVIRT_VM_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), OVIRT_TYPE_VM, OvirtVmPrivate))

struct _OvirtVmPrivate {
    OvirtCollection *cdroms;

    OvirtVmState state;
    OvirtVmDisplay *display;
} ;
G_DEFINE_TYPE(OvirtVm, ovirt_vm, OVIRT_TYPE_RESOURCE);

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

    g_clear_object(&vm->priv->cdroms);
    g_clear_object(&vm->priv->display);

    G_OBJECT_CLASS(ovirt_vm_parent_class)->dispose(object);
}


static gboolean ovirt_vm_init_from_xml(OvirtResource *resource,
                                       RestXmlNode *node,
                                       GError **error)
{
    gboolean parsed_ok;
    OvirtResourceClass *parent_class;

    parsed_ok = ovirt_vm_refresh_from_xml(OVIRT_VM(resource), node);
    if (!parsed_ok) {
        return FALSE;
    }
    parent_class = OVIRT_RESOURCE_CLASS(ovirt_vm_parent_class);

    return parent_class->init_from_xml(resource, node, error);
}

static void ovirt_vm_class_init(OvirtVmClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    OvirtResourceClass *resource_class = OVIRT_RESOURCE_CLASS(klass);

    g_type_class_add_private(klass, sizeof(OvirtVmPrivate));

    resource_class->init_from_xml = ovirt_vm_init_from_xml;
    object_class->dispose = ovirt_vm_dispose;
    object_class->get_property = ovirt_vm_get_property;
    object_class->set_property = ovirt_vm_set_property;

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

OvirtVm *ovirt_vm_new_from_xml(RestXmlNode *node, GError **error)
{
    return OVIRT_VM(g_initable_new(OVIRT_TYPE_VM, NULL, error,
                                   "xml-node", node, NULL));
}

OvirtVm *ovirt_vm_new(void)
{
    return OVIRT_VM(g_initable_new(OVIRT_TYPE_VM, NULL, NULL, NULL));
}

typedef struct {
    OvirtVm *vm;
    ActionResponseParser parser;
} OvirtVmInvokeActionData;

static gboolean ovirt_vm_invoke_action_async_cb(OvirtProxy *proxy, RestProxyCall *call,
                                                gpointer user_data, GError **error)
{
    OvirtVmInvokeActionData *data;
    GError *action_error = NULL;

    g_return_val_if_fail(REST_IS_PROXY_CALL(call), FALSE);
    data = (OvirtVmInvokeActionData *)user_data;

    parse_action_response(call, data->vm, data->parser, &action_error);
    if (action_error != NULL) {
        g_propagate_error(error, action_error);
        return  FALSE;
    }

    return TRUE;
}

static void
ovirt_vm_invoke_action_data_free(OvirtVmInvokeActionData *data)
{
    g_slice_free(OvirtVmInvokeActionData, data);
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
    OvirtRestCall *call;
    GSimpleAsyncResult *result;
    const char *function;
    OvirtVmInvokeActionData *data;

    g_return_if_fail(OVIRT_IS_VM(vm));
    g_return_if_fail(action != NULL);
    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    g_debug("invoking '%s' action on %p using %p", action, vm, proxy);
    function = ovirt_resource_get_action(OVIRT_RESOURCE(vm), action);
    g_return_if_fail(function != NULL);

    result = g_simple_async_result_new(G_OBJECT(vm), callback,
                                       user_data,
                                       ovirt_vm_invoke_action_async);
    data = g_slice_new(OvirtVmInvokeActionData);
    data->vm = vm;
    data->parser = response_parser;

    call = ovirt_rest_call_new(proxy, "POST", function);

    ovirt_rest_call_async(call, result, cancellable,
                          ovirt_vm_invoke_action_async_cb, data,
                          (GDestroyNotify)ovirt_vm_invoke_action_data_free);
}

static gboolean
ovirt_vm_action_finish(OvirtVm *vm, GAsyncResult *result, GError **err)
{
    g_return_val_if_fail(OVIRT_IS_VM(vm), FALSE);
    g_return_val_if_fail(g_simple_async_result_is_valid(result, G_OBJECT(vm),
                                                        ovirt_vm_invoke_action_async),
                         FALSE);

    return ovirt_rest_call_finish(result, err);
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

    function = ovirt_resource_get_action(OVIRT_RESOURCE(vm), action);
    function = ovirt_utils_strip_api_base_dir(function);
    g_return_val_if_fail(function != NULL, FALSE);

    call = REST_PROXY_CALL(ovirt_action_rest_call_new(REST_PROXY(proxy)));
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
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED, "could not find 'ticket' node");
        g_return_val_if_reached(FALSE);
    }
    node = g_hash_table_lookup(root->children, value_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED, "could not find 'value' node");
        g_return_val_if_reached(FALSE);
    }

    g_object_get(G_OBJECT(vm), "display", &display, NULL);
    g_return_val_if_fail(display != NULL, FALSE);
    g_object_set(G_OBJECT(display), "ticket", node->content, NULL);

    g_debug("Ticket: %s\n", node->content);

    node = g_hash_table_lookup(root->children, expiry_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED, "could not find 'expiry' node");
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
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED, "could not find 'status' node");
        g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
    }
    node = g_hash_table_lookup(node->children, state_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED, "could not find 'state' node");
        g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
    }
    g_debug("State: %s\n", node->content);
    if (g_strcmp0(node->content, "complete") == 0) {
        return OVIRT_RESPONSE_COMPLETE;
    } else if (g_strcmp0(node->content, "pending") == 0) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_ACTION_FAILED, "action is pending");
        return OVIRT_RESPONSE_PENDING;
    } else if (g_strcmp0(node->content, "in_progress") == 0) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_ACTION_FAILED, "action is in progress");
        return OVIRT_RESPONSE_IN_PROGRESS;
    } else if (g_strcmp0(node->content, "failed") == 0) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_ACTION_FAILED, "action has failed");
        return OVIRT_RESPONSE_FAILED;
    }

    g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED, "unknown action failure");
    g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
}


static gboolean
parse_action_response(RestProxyCall *call, OvirtVm *vm,
                      ActionResponseParser response_parser, GError **error)
{
    RestXmlNode *root;
    gboolean result;

    result = FALSE;
    root = ovirt_rest_xml_node_from_call(call);

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
                ovirt_utils_gerror_from_xml_fault(fault_node, &fault_error);
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

    return result;
}

static gboolean ovirt_vm_refresh_async_cb(OvirtProxy *proxy, RestProxyCall *call,
                                          gpointer user_data, GError **error)
{
    OvirtVm *vm;
    RestXmlNode *root;
    gboolean refreshed;

    g_return_val_if_fail(REST_IS_PROXY_CALL(call), FALSE);
    g_return_val_if_fail(OVIRT_IS_VM(user_data), FALSE);

    root = ovirt_rest_xml_node_from_call(call);
    vm = OVIRT_VM(user_data);
    refreshed = ovirt_vm_refresh_from_xml(vm, root);

    rest_xml_node_unref(root);

    return refreshed;
}

void ovirt_vm_refresh_async(OvirtVm *vm, OvirtProxy *proxy,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
    OvirtRestCall *call;
    GSimpleAsyncResult *result;
    char *href;

    g_return_if_fail(OVIRT_IS_VM(vm));
    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    result = g_simple_async_result_new(G_OBJECT(vm), callback,
                                       user_data,
                                       ovirt_vm_refresh_async);
    g_object_get(G_OBJECT(vm), "href", &href, NULL);
    call = ovirt_rest_call_new(proxy, "GET", href);
    ovirt_rest_call_async(call, result, cancellable,
                          ovirt_vm_refresh_async_cb, vm, NULL);
    g_free(href);
}

gboolean ovirt_vm_refresh_finish(OvirtVm *vm,
                                 GAsyncResult *result,
                                 GError **err)
{
    g_return_val_if_fail(OVIRT_IS_VM(vm), FALSE);
    g_return_val_if_fail(g_simple_async_result_is_valid(result, G_OBJECT(vm),
                                                        ovirt_vm_invoke_action_async),
                         FALSE);

    return ovirt_rest_call_finish(result, err);
}


/**
 * ovirt_vm_get_cdroms:
 * @vm: a #OvirtVm
 *
 * Gets a #OvirtCollection representing the list of remote cdroms from a
 * virtual machine object.  This method does not initiate any network
 * activity, the remote cdrom list must be then be fetched using
 * ovirt_collection_fetch() or ovirt_collection_fetch_async().
 *
 * Return value: (transfer full): a #OvirtCollection representing the list
 * of cdroms associated with @vm.
 */
OvirtCollection *ovirt_vm_get_cdroms(OvirtVm *vm)
{
    const char *href;

    g_return_val_if_fail(OVIRT_IS_VM(vm), NULL);

    if (vm->priv->cdroms != NULL)
        return vm->priv->cdroms;

    href = ovirt_resource_get_sub_collection(OVIRT_RESOURCE(vm), "cdroms");
    if (href == NULL)
        return NULL;

    vm->priv->cdroms =  ovirt_collection_new(href, "cdroms",
                                             OVIRT_TYPE_CDROM,
                                             "cdrom");

    return vm->priv->cdroms;
}
