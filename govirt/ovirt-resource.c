/*
 * ovirt-resource.c: generic oVirt resource handling
 *
 * Copyright (C) 2013 Red Hat, Inc.
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

#include <string.h>

#include <glib/gi18n-lib.h>
#include <rest/rest-xml-node.h>
#include <rest/rest-xml-parser.h>

#define GOVIRT_UNSTABLE_API_ABI
#include "govirt-private.h"
#include "ovirt-error.h"
#include "ovirt-proxy-private.h"
#include "ovirt-resource.h"
#undef GOVIRT_UNSTABLE_API_ABI

struct _OvirtResourcePrivate {
    char *guid;
    char *href;
    char *name;
    char *description;

    GHashTable *actions;
    GHashTable *sub_collections;

    RestXmlNode *xml;
};

static void ovirt_resource_initable_iface_init(GInitableIface *iface);
static gboolean ovirt_resource_init_from_xml_real(OvirtResource *resource,
                                                  RestXmlNode *node,
                                                  GError **error);
static gboolean ovirt_resource_init_from_xml(OvirtResource *resource,
                                             RestXmlNode *node,
                                             GError **error);

G_DEFINE_TYPE_WITH_CODE(OvirtResource, ovirt_resource, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE,
                                              ovirt_resource_initable_iface_init);
                        G_ADD_PRIVATE(OvirtResource));


enum {
    PROP_0,
    PROP_DESCRIPTION,
    PROP_GUID,
    PROP_HREF,
    PROP_NAME,
    PROP_XML_NODE,
};

static void ovirt_resource_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
    OvirtResource *resource = OVIRT_RESOURCE(object);

    switch (prop_id) {
    case PROP_GUID:
        g_value_set_string(value, resource->priv->guid);
        break;
    case PROP_HREF:
        g_value_set_string(value, resource->priv->href);
        break;
    case PROP_NAME:
        g_value_set_string(value, resource->priv->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string(value, resource->priv->description);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void ovirt_resource_set_xml_node(OvirtResource *resource,
                                        RestXmlNode *node)
{
    g_clear_pointer(&resource->priv->xml, &rest_xml_node_unref);
    if (node != NULL) {
        resource->priv->xml = rest_xml_node_ref(node);
    }
}

static void ovirt_resource_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    OvirtResource *resource = OVIRT_RESOURCE(object);

    switch (prop_id) {
    case PROP_GUID:
        g_free(resource->priv->guid);
        resource->priv->guid = g_value_dup_string(value);
        break;
    case PROP_HREF:
        g_free(resource->priv->href);
        resource->priv->href = g_value_dup_string(value);
        break;
    case PROP_NAME:
        g_free(resource->priv->name);
        resource->priv->name = g_value_dup_string(value);
        break;
    case PROP_DESCRIPTION:
        g_free(resource->priv->description);
        resource->priv->description = g_value_dup_string(value);
        break;
    case PROP_XML_NODE:
        ovirt_resource_set_xml_node(OVIRT_RESOURCE(object),
                                    g_value_get_boxed(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void ovirt_resource_dispose(GObject *object)
{
    OvirtResource *resource = OVIRT_RESOURCE(object);

    g_clear_pointer(&resource->priv->actions, g_hash_table_unref);
    g_clear_pointer(&resource->priv->sub_collections, g_hash_table_unref);

    if (resource->priv->xml != NULL) {
        g_boxed_free(REST_TYPE_XML_NODE, resource->priv->xml);
        resource->priv->xml = NULL;
    }

    G_OBJECT_CLASS(ovirt_resource_parent_class)->dispose(object);
}

static void ovirt_resource_finalize(GObject *object)
{
    OvirtResource *resource = OVIRT_RESOURCE(object);

    g_free(resource->priv->description);
    g_free(resource->priv->guid);
    g_free(resource->priv->href);
    g_free(resource->priv->name);

    G_OBJECT_CLASS(ovirt_resource_parent_class)->finalize(object);
}

static gboolean ovirt_resource_initable_init(GInitable *initable,
                                             GCancellable *cancellable,
                                             GError  **error)
{
    OvirtResource *resource;

    g_return_val_if_fail (OVIRT_IS_RESOURCE(initable), FALSE);


    if (cancellable != NULL) {
        g_set_error_literal (error, OVIRT_ERROR, OVIRT_ERROR_NOT_SUPPORTED,
                             _("Cancellable initialization not supported"));
        return FALSE;
    }

    resource = OVIRT_RESOURCE(initable);

    if (resource->priv->xml == NULL) {
        return TRUE;
    }

    return ovirt_resource_init_from_xml(resource, resource->priv->xml, error);
}

static void ovirt_resource_initable_iface_init(GInitableIface *iface)
{
      iface->init = ovirt_resource_initable_init;
}

static void ovirt_resource_class_init(OvirtResourceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    klass->init_from_xml = ovirt_resource_init_from_xml_real;
    object_class->dispose = ovirt_resource_dispose;
    object_class->finalize = ovirt_resource_finalize;
    object_class->get_property = ovirt_resource_get_property;
    object_class->set_property = ovirt_resource_set_property;

    g_object_class_install_property(object_class,
                                    PROP_DESCRIPTION,
                                    g_param_spec_string("description",
                                                        "Name",
                                                        "Resource Description",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_GUID,
                                    g_param_spec_string("guid",
                                                        "GUID",
                                                        "Resource GUID",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_HREF,
                                    g_param_spec_string("href",
                                                        "Href",
                                                        "Resource Href",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_NAME,
                                    g_param_spec_string("name",
                                                        "Name",
                                                        "Resource Name",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_XML_NODE,
                                    g_param_spec_boxed("xml-node",
                                                       "Librest XML Node",
                                                       "XML data to fill this resource with",
                                                       REST_TYPE_XML_NODE,
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_STRINGS));
}

static void ovirt_resource_init(OvirtResource *resource)
{
    resource->priv = ovirt_resource_get_instance_private(resource);
    resource->priv->actions = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                    g_free, g_free);
    resource->priv->sub_collections = g_hash_table_new_full(g_str_hash,
                                                            g_str_equal,
                                                            g_free,
                                                            g_free);
}

static void
ovirt_resource_add_action(OvirtResource *resource,
                          const char *action,
                          const char *url)
{
    g_return_if_fail(OVIRT_IS_RESOURCE(resource));
    g_return_if_fail(action != NULL);
    g_return_if_fail(url != NULL);

    g_hash_table_insert(resource->priv->actions,
                        g_strdup(action),
                        g_strdup(url));
}

G_GNUC_INTERNAL const char *
ovirt_resource_get_action(OvirtResource *resource, const char *action)
{
    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), NULL);
    g_return_val_if_fail(resource->priv->actions != NULL, NULL);

    return g_hash_table_lookup(resource->priv->actions, action);
}

static void
ovirt_resource_add_sub_collection(OvirtResource *resource,
                                  const char *sub_collection,
                                  const char *url)
{
    g_return_if_fail(OVIRT_IS_RESOURCE(resource));
    g_return_if_fail(sub_collection != NULL);
    g_return_if_fail(url != NULL);

    g_hash_table_insert(resource->priv->sub_collections,
                        g_strdup(sub_collection),
                        g_strdup(url));
}

const char *
ovirt_resource_get_sub_collection(OvirtResource *resource,
                                  const char *sub_collection)
{
    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), NULL);
    g_return_val_if_fail(resource->priv->sub_collections != NULL, NULL);

    return g_hash_table_lookup(resource->priv->sub_collections,
                               sub_collection);
}

static gboolean
ovirt_resource_set_actions_from_xml(OvirtResource *resource, RestXmlNode *node)
{
    RestXmlNode *link_node;
    RestXmlNode *rest_actions;
    const char *link_key = g_intern_string("link");

    rest_actions = rest_xml_node_find(node, "actions");
    if (rest_actions == NULL) {
        return FALSE;
    }

    link_node = g_hash_table_lookup(rest_actions->children, link_key);
    if (link_node == NULL)
        return FALSE;

    for (; link_node != NULL; link_node = link_node->next) {
        const char *link_name;
        const char *href;

        g_warn_if_fail(link_node != NULL);
        g_warn_if_fail(link_node->name != NULL);
        g_warn_if_fail(g_strcmp0(link_node->name, "link") == 0);

        link_name = rest_xml_node_get_attr(link_node, "rel");
        href = rest_xml_node_get_attr(link_node, "href");

        if ((link_name != NULL) && (href != NULL)) {
            ovirt_resource_add_action(resource, link_name, href);
        }
    }

    return TRUE;
}

static gboolean
ovirt_resource_set_sub_collections_from_xml(OvirtResource *resource,
                                            RestXmlNode *node)
{
    RestXmlNode *link_node;
    const char *link_key = g_intern_string("link");

    link_node = g_hash_table_lookup(node->children, link_key);
    if (link_node == NULL)
        return FALSE;

    for (; link_node != NULL; link_node = link_node->next) {
        const char *link_name;
        const char *href;

        g_warn_if_fail(link_node != NULL);
        g_warn_if_fail(link_node->name != NULL);
        g_warn_if_fail(g_strcmp0(link_node->name, "link") == 0);

        link_name = rest_xml_node_get_attr(link_node, "rel");
        href = rest_xml_node_get_attr(link_node, "href");

        if ((link_name != NULL) && (href != NULL)) {
            ovirt_resource_add_sub_collection(resource, link_name, href);
        }
    }

    return TRUE;
}

static gboolean
ovirt_resource_set_name_from_xml(OvirtResource *resource, RestXmlNode *node)
{
    RestXmlNode *name_node;

    name_node = rest_xml_node_find(node, "name");
    if (name_node != NULL) {
        g_return_val_if_fail(name_node->content != NULL, FALSE);
        g_object_set(G_OBJECT(resource), "name", name_node->content, NULL);
        return TRUE;
    }

    return FALSE;
}

static gboolean
ovirt_resource_set_description_from_xml(OvirtResource *resource,
                                        RestXmlNode *node)
{
    RestXmlNode *desc_node;

    desc_node = rest_xml_node_find(node, "description");
    if (desc_node != NULL) {
        g_return_val_if_fail(desc_node->content != NULL, FALSE);
        g_object_set(G_OBJECT(resource),
                     "description", desc_node->content,
                     NULL);
        return TRUE;
    }

    return FALSE;
}

static gboolean ovirt_resource_init_from_xml_real(OvirtResource *resource,
                                                  RestXmlNode *node,
                                                  GError **error)
{
    const char *guid;
    const char *href;
    gboolean is_api;

    /* Special casing the root 'api' node here is ugly,
     * but this is the only resource-like object which does not have
     * href/guid properties, so I prefer not to make these optional,
     * nor to be able to make them mandatory/optional through a
     * gobject property. So this is just hardcoded here for now...
     */
    is_api = OVIRT_IS_API(resource);

    g_return_val_if_fail(node != NULL, FALSE);

    guid = rest_xml_node_get_attr(node, "id");
    if ((guid == NULL) && !is_api) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    _("Missing mandatory 'id' attribute"));
        return FALSE;
    }

    href = rest_xml_node_get_attr(node, "href");
    if ((href == NULL) && !is_api) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    _("Missing mandatory 'href' attribute"));
        return FALSE;
    }

    ovirt_resource_set_xml_node(resource, node);
    g_object_set(G_OBJECT(resource), "guid", guid, "href", href, NULL);

    ovirt_resource_set_name_from_xml(resource, node);
    ovirt_resource_set_description_from_xml(resource, node);
    ovirt_resource_set_actions_from_xml(resource, node);
    ovirt_resource_set_sub_collections_from_xml(resource, node);

    return TRUE;
}

static gboolean ovirt_resource_init_from_xml(OvirtResource *resource,
                                             RestXmlNode *node,
                                             GError **error)
{
    OvirtResourceClass *klass;

    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), FALSE);

    klass = OVIRT_RESOURCE_GET_CLASS(resource);
    g_return_val_if_fail(klass->init_from_xml != NULL, FALSE);

    return klass->init_from_xml(resource, node, error);
}


char *ovirt_resource_to_xml(OvirtResource *resource)
{
    OvirtResourceClass *klass;

    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), NULL);

    klass = OVIRT_RESOURCE_GET_CLASS(resource);
    if (klass->to_xml == NULL)
        return NULL;

    return klass->to_xml(resource);
}


G_GNUC_INTERNAL RestXmlNode *ovirt_resource_rest_call_sync(OvirtRestCall *call,
                                                           GError **error)
{
    RestXmlNode *root = NULL;
    if (!rest_proxy_call_sync(REST_PROXY_CALL(call), error)) {
        GError *local_error = NULL;

        root = ovirt_rest_xml_node_from_call(REST_PROXY_CALL(call));
        if (root != NULL) {
            ovirt_utils_gerror_from_xml_fault(root, &local_error);
            rest_xml_node_unref(root);
        }
        if (local_error != NULL) {
            g_clear_error(error);
            g_warning("Error while updating resource");
            g_warning("message: %s", local_error->message);
            g_propagate_error(error, local_error);
        }
        g_warn_if_fail(error == NULL || *error != NULL);

        return NULL;
    }

    return ovirt_rest_xml_node_from_call(REST_PROXY_CALL(call));
}


static RestXmlNode *ovirt_resource_rest_call(OvirtResource *resource,
                                             OvirtProxy *proxy,
                                             const char *method,
                                             GError **error)
{
    OvirtRestCall *call;
    RestXmlNode *root;

    call = OVIRT_REST_CALL(ovirt_resource_rest_call_new(REST_PROXY(proxy),
                                                        resource));
    rest_proxy_call_set_method(REST_PROXY_CALL(call), method);

    root = ovirt_resource_rest_call_sync(call, error);

    g_object_unref(G_OBJECT(call));

    return root;
}


gboolean ovirt_resource_update(OvirtResource *resource,
                               OvirtProxy *proxy,
                               GError **error)
{
    RestXmlNode *xml;

    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), FALSE);
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);
    g_return_val_if_fail((error == NULL) || (*error == NULL), FALSE);

    xml = ovirt_resource_rest_call(resource, proxy,
                                   "PUT", error);
    if (xml != NULL) {
        rest_xml_node_unref(xml);
        return TRUE;
    }

    return FALSE;
}

static gboolean ovirt_resource_update_async_cb(OvirtProxy *proxy, RestProxyCall *call,
                                               gpointer user_data, GError **error)
{
    g_return_val_if_fail(REST_IS_PROXY_CALL(call), FALSE);

    g_warn_if_reached();

    /* if error */
#if 0
    root = ovirt_rest_xml_node_from_call(call);
    parse_action_response(call, data->vm, data->parser, &action_error);
    rest_xml_node_unref(root);
#endif

    return TRUE;
}


void ovirt_resource_update_async(OvirtResource *resource,
                                 OvirtProxy *proxy,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
    GTask *task;
    OvirtResourceRestCall *call;

    g_return_if_fail(OVIRT_IS_RESOURCE(resource));
    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    task = g_task_new(G_OBJECT(resource),
                      cancellable,
                      callback,
                      user_data);

    call = ovirt_resource_rest_call_new(REST_PROXY(proxy), resource);
    rest_proxy_call_set_method(REST_PROXY_CALL(call), "PUT");
    ovirt_rest_call_async(OVIRT_REST_CALL(call), task, cancellable,
                          ovirt_resource_update_async_cb, NULL, NULL);
    g_object_unref(G_OBJECT(call));
}


gboolean ovirt_resource_update_finish(OvirtResource *resource,
                                      GAsyncResult *result,
                                      GError **err)
{
    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), FALSE);
    g_return_val_if_fail(g_task_is_valid(G_TASK(result), G_OBJECT(resource)),
                         FALSE);
    g_return_val_if_fail((err == NULL) || (*err == NULL), FALSE);

    return ovirt_rest_call_finish(result, err);
}


enum OvirtResponseStatus {
    OVIRT_RESPONSE_UNKNOWN,
    OVIRT_RESPONSE_FAILED,
    OVIRT_RESPONSE_PENDING,
    OVIRT_RESPONSE_IN_PROGRESS,
    OVIRT_RESPONSE_COMPLETE
};

static gboolean parse_action_response(RestProxyCall *call, OvirtResource *resource,
                                      ActionResponseParser response_parser,
                                      GError **error);

static RestProxyCall *
ovirt_resource_create_rest_call_for_action(OvirtResource *resource,
                                           OvirtProxy *proxy,
                                           const char *action)
{
    RestProxyCall *call;
    const char *function;

    function = ovirt_resource_get_action(resource, action);
    g_return_val_if_fail(function != NULL, NULL);

    call = REST_PROXY_CALL(ovirt_action_rest_call_new(REST_PROXY(proxy)));
    rest_proxy_call_set_method(call, "POST");
    rest_proxy_call_set_function(call, function);
    rest_proxy_call_add_param(call, "async", "false");
    ovirt_resource_add_rest_params(resource, call);

    return call;
}

gboolean
ovirt_resource_action(OvirtResource *resource, OvirtProxy *proxy,
                      const char *action,
                      ActionResponseParser response_parser,
                      GError **error)
{
    RestProxyCall *call;

    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), FALSE);
    g_return_val_if_fail(action != NULL, FALSE);
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);
    g_return_val_if_fail((error == NULL) || (*error == NULL), FALSE);

    call = ovirt_resource_create_rest_call_for_action(resource,
                                                      proxy,
						      action);
    g_return_val_if_fail(call != NULL, FALSE);

    if (!rest_proxy_call_sync(call, error)) {
        GError *call_error = NULL;
        g_warning("Error while running %s on %p", action, resource);
        /* Even in error cases we may have a response body describing
         * the failure, try to parse that */
        parse_action_response(call, resource, response_parser, &call_error);
        if (call_error != NULL) {
            g_clear_error(error);
            g_propagate_error(error, call_error);
        }

        g_object_unref(G_OBJECT(call));
        return FALSE;
    }

    parse_action_response(call, resource, response_parser, error);

    g_object_unref(G_OBJECT(call));

    return TRUE;
}


static enum OvirtResponseStatus parse_action_status(RestXmlNode *root,
                                                    GError **error)
{
    RestXmlNode *node;
    const char *status_key = g_intern_string("status");

    g_return_val_if_fail(g_strcmp0(root->name, "action") == 0,
                         OVIRT_RESPONSE_UNKNOWN);
    g_return_val_if_fail(error == NULL || *error == NULL,
                         OVIRT_RESPONSE_UNKNOWN);

    node = g_hash_table_lookup(root->children, status_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    _("Could not find 'status' node"));
        g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
    }
    g_debug("State: %s\n", node->content);
    if (g_strcmp0(node->content, "complete") == 0) {
        return OVIRT_RESPONSE_COMPLETE;
    } else if (g_strcmp0(node->content, "pending") == 0) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_ACTION_FAILED,
                    _("Action is pending"));
        return OVIRT_RESPONSE_PENDING;
    } else if (g_strcmp0(node->content, "in_progress") == 0) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_ACTION_FAILED,
                    _("Action is in progress"));
        return OVIRT_RESPONSE_IN_PROGRESS;
    } else if (g_strcmp0(node->content, "failed") == 0) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_ACTION_FAILED,
                    _("Action has failed"));
        return OVIRT_RESPONSE_FAILED;
    }

    g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                _("Unknown action failure"));
    g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
}


static gboolean
parse_action_response(RestProxyCall *call, OvirtResource *resource,
                      ActionResponseParser response_parser, GError **error)
{
    RestXmlNode *root;
    gboolean result;

    result = FALSE;
    root = ovirt_rest_xml_node_from_call(call);
    /* We are not guaranteed to get XML out of the call, for example we could
     * get HTML with a 404 error */
    if (root == NULL) {
        return FALSE;
    }

    if (g_strcmp0(root->name, "action") == 0) {
        enum OvirtResponseStatus status;

        status = parse_action_status(root, error);
        if (status  == OVIRT_RESPONSE_COMPLETE) {
            if (response_parser) {
                result = response_parser(root, resource, error);
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

typedef struct {
    OvirtResource *resource;
    ActionResponseParser parser;
} OvirtResourceInvokeActionData;

static gboolean ovirt_resource_invoke_action_async_cb(OvirtProxy *proxy,
                                                      RestProxyCall *call,
                                                      gpointer user_data,
                                                      GError **error)
{
    OvirtResourceInvokeActionData *data;
    GError *action_error = NULL;

    g_return_val_if_fail(REST_IS_PROXY_CALL(call), FALSE);
    data = (OvirtResourceInvokeActionData *)user_data;

    parse_action_response(call, data->resource, data->parser, &action_error);
    if (action_error != NULL) {
        g_propagate_error(error, action_error);
        return  FALSE;
    }

    return TRUE;
}


static void
ovirt_resource_invoke_action_data_free(OvirtResourceInvokeActionData *data)
{
    g_slice_free(OvirtResourceInvokeActionData, data);
}


void
ovirt_resource_invoke_action_async(OvirtResource *resource,
                                   const char *action,
                                   OvirtProxy *proxy,
                                   ActionResponseParser response_parser,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
    RestProxyCall *call;
    GTask *task;
    OvirtResourceInvokeActionData *data;

    g_return_if_fail(OVIRT_IS_RESOURCE(resource));
    g_return_if_fail(action != NULL);
    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    g_debug("invoking '%s' action on %p using %p", action, resource, proxy);
    call = ovirt_resource_create_rest_call_for_action(resource,
                                                      proxy,
						      action);
    g_return_if_fail(call != NULL);

    task = g_task_new(G_OBJECT(resource),
                      cancellable,
                      callback,
                      user_data);
    data = g_slice_new(OvirtResourceInvokeActionData);
    data->resource = resource;
    data->parser = response_parser;

    ovirt_rest_call_async(OVIRT_REST_CALL(call), task, cancellable,
                          ovirt_resource_invoke_action_async_cb, data,
                          (GDestroyNotify)ovirt_resource_invoke_action_data_free);
    g_object_unref(G_OBJECT(call));
}


gboolean
ovirt_resource_action_finish(OvirtResource *resource,
                             GAsyncResult *result,
                             GError **err)
{
    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), FALSE);
    g_return_val_if_fail(g_task_is_valid(G_TASK(result), G_OBJECT(resource)),
                         FALSE);

    return ovirt_rest_call_finish(result, err);
}


static gboolean ovirt_resource_refresh_async_cb(OvirtProxy *proxy,
                                                RestProxyCall *call,
                                                gpointer user_data,
                                                GError **error)
{
    OvirtResource *resource;
    RestXmlNode *root;
    gboolean refreshed = FALSE;

    g_return_val_if_fail(REST_IS_PROXY_CALL(call), FALSE);
    g_return_val_if_fail(OVIRT_IS_RESOURCE(user_data), FALSE);

    root = ovirt_rest_xml_node_from_call(call);
    if (root == NULL) {
        g_set_error_literal(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                            _("Failed to parse response from resource"));
        goto end;
    }

    resource = OVIRT_RESOURCE(user_data);
    refreshed = ovirt_resource_init_from_xml(resource, root, error);

    rest_xml_node_unref(root);

end:
    return refreshed;
}

void ovirt_resource_refresh_async(OvirtResource *resource,
                                  OvirtProxy *proxy,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    OvirtResourceRestCall *call;
    GTask *task;

    g_return_if_fail(OVIRT_IS_RESOURCE(resource));
    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    task = g_task_new(G_OBJECT(resource),
                      cancellable,
                      callback,
                      user_data);
    call = ovirt_resource_rest_call_new(REST_PROXY(proxy),
                                        OVIRT_RESOURCE(resource));
    /* FIXME: to set or not to set ?? */
    rest_proxy_call_add_header(REST_PROXY_CALL(call),
                               "All-Content", "true");
    rest_proxy_call_set_method(REST_PROXY_CALL(call), "GET");
    ovirt_rest_call_async(OVIRT_REST_CALL(call), task, cancellable,
                          ovirt_resource_refresh_async_cb, resource,
                          NULL);
    g_object_unref(G_OBJECT(call));
}


gboolean ovirt_resource_refresh_finish(OvirtResource *resource,
                                       GAsyncResult *result,
                                       GError **err)
{
    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), FALSE);
    g_return_val_if_fail(g_task_is_valid(G_TASK(result), G_OBJECT(resource)),
                         FALSE);

    return ovirt_rest_call_finish(result, err);
}


gboolean ovirt_resource_refresh(OvirtResource *resource,
                                OvirtProxy *proxy,
                                GError **error)
{
    RestXmlNode *root_node;
    gboolean success;

    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), FALSE);
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);

    root_node = ovirt_resource_rest_call(resource, proxy, "GET", error);
    if (root_node == NULL) {
        return FALSE;
    }

    success = ovirt_resource_init_from_xml(resource, root_node, error);
    rest_xml_node_unref(root_node);

    return success;
}

void ovirt_resource_add_rest_params(OvirtResource *resource,
                                    RestProxyCall *call)
{
    OvirtResourceClass *klass;

    g_return_if_fail(OVIRT_IS_RESOURCE(resource));
    g_return_if_fail(OVIRT_IS_REST_CALL(call));

    klass = OVIRT_RESOURCE_GET_CLASS(resource);
    if (klass->add_rest_params != NULL)
        klass->add_rest_params(resource, call);
}


/**
 * ovirt_resource_delete:
 * @resource: an #OvirtResource.
 * @proxy: an #OvirtProxy.
 * @error: return location for error or NULL.
 * Returns: TRUE if the call was successful, FALSE otherwise.
 *
 * Sends an HTTP DELETE request to @resource.
 *
 * The calling thread is blocked until this request is processed, see
 * ovirt_resource_delete_async() for the asynchronous version of this method.
 */
gboolean ovirt_resource_delete(OvirtResource *resource,
                               OvirtProxy *proxy,
                               GError **error)
{
    RestXmlNode *xml;

    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), FALSE);
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);
    g_return_val_if_fail((error == NULL) || (*error == NULL), FALSE);

    xml = ovirt_resource_rest_call(resource, proxy,
                                   "DELETE", error);
    if (xml != NULL) {
        rest_xml_node_unref(xml);
        return TRUE;
    }

    return FALSE;
}


static gboolean ovirt_resource_delete_async_cb(OvirtProxy *proxy, RestProxyCall *call,
                                               gpointer user_data, GError **error)
{
    OvirtResource *resource;
    RestXmlNode *xml;
    gboolean success;

    g_return_val_if_fail(REST_IS_PROXY_CALL(call), FALSE);
    g_return_val_if_fail(OVIRT_IS_RESOURCE(user_data), FALSE);

    resource = OVIRT_RESOURCE(user_data);
    xml = ovirt_rest_xml_node_from_call(call);
    success = parse_action_response(call, resource, NULL, error);
    rest_xml_node_unref(xml);

    return success;
}


/**
 * ovirt_resource_delete_async:
 * @resource: an #OvirtResource.
 * @proxy: an #OvirtProxy.
 * @cancellable: a #GCancellable or NULL.
 * @callback: a #GAsyncReadyCallback to call when the call completes, or NULL
 * if you don't care about the result of the method invocation.
 * @user_data: data to pass to @callback.
 *
 * Asynchronously send an HTTP DELETE request to @resource.
 *
 * When the call is complete, @callback will be invoked. You can then call
 * ovirt_resource_delete_finish() to get the result of the call.
 */
void ovirt_resource_delete_async(OvirtResource *resource,
                                 OvirtProxy *proxy,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
    GTask *task;
    OvirtResourceRestCall *call;

    g_return_if_fail(OVIRT_IS_RESOURCE(resource));
    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    task = g_task_new(G_OBJECT(resource),
                      cancellable,
                      callback,
                      user_data);

    call = ovirt_resource_rest_call_new(REST_PROXY(proxy), resource);
    rest_proxy_call_set_method(REST_PROXY_CALL(call), "DELETE");
    ovirt_rest_call_async(OVIRT_REST_CALL(call), task, cancellable,
                          ovirt_resource_delete_async_cb,
                          g_object_ref(resource), g_object_unref);
    g_object_unref(G_OBJECT(call));
}


/**
 * ovirt_resource_delete_finish:
 * @resource: an #OvirtResource.
 * @result: a #GAsyncResult.
 * @err: return location for error or NULL.
 * Returns: TRUE if the call was successful, FALSE otherwise.
 *
 * Finishes an asynchronous HTTP DELETE request on @resource.
 */
gboolean ovirt_resource_delete_finish(OvirtResource *resource,
                                      GAsyncResult *result,
                                      GError **err)
{
    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), FALSE);
    g_return_val_if_fail(g_task_is_valid(G_TASK(result), G_OBJECT(resource)),
                         FALSE);
    g_return_val_if_fail((err == NULL) || (*err == NULL), FALSE);

    return ovirt_rest_call_finish(result, err);
}


static OvirtResource *ovirt_resource_new_valist(GType type, GError **error, const char *prop_name, ...)
{
    gpointer resource;
    va_list var_args;
    GError *local_error = NULL;

    g_return_val_if_fail(g_type_is_a(type, OVIRT_TYPE_RESOURCE), NULL);

    va_start(var_args, prop_name);
    resource = g_initable_new_valist(type, prop_name, var_args, NULL, &local_error);
    va_end(var_args);

    if (local_error != NULL) {
        g_warning("Failed to create resource of type %s: %s", g_type_name(type), local_error->message);
        g_propagate_error(error, local_error);
    }

    return OVIRT_RESOURCE(resource);
}


G_GNUC_INTERNAL
OvirtResource *ovirt_resource_new(GType type)
{
    return ovirt_resource_new_valist(type, NULL, NULL);
}


G_GNUC_INTERNAL
OvirtResource *ovirt_resource_new_from_id(GType type, const char *id, const char *href)
{
    g_return_val_if_fail(id != NULL, NULL);
    g_return_val_if_fail(href != NULL, NULL);

    return ovirt_resource_new_valist(type, NULL, "guid", id, "href", href, NULL);
}


G_GNUC_INTERNAL
OvirtResource *ovirt_resource_new_from_xml(GType type, RestXmlNode *node, GError **error)
{
    g_return_val_if_fail(node != NULL, NULL);

    return ovirt_resource_new_valist(type, error, "xml-node", node, NULL);
}
