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

#include <stdlib.h>
#include <string.h>

#include <rest/rest-proxy.h>
#include <rest/rest-xml-node.h>
#include <rest/rest-xml-parser.h>

G_DEFINE_TYPE (OvirtProxy, ovirt_proxy, G_TYPE_OBJECT);

struct _OvirtProxyPrivate {
    RestProxy *rest_proxy;
    GHashTable *vms;
};

#define OVIRT_PROXY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), OVIRT_TYPE_PROXY, OvirtProxyPrivate))

enum {
    PROP_0,
    PROP_URI,
};

#define API_ENTRY_POINT "/api/"

enum OvirtResponseStatus {
    OVIRT_RESPONSE_UNKNOWN,
    OVIRT_RESPONSE_FAILED,
    OVIRT_RESPONSE_PENDING,
    OVIRT_RESPONSE_IN_PROGRESS,
    OVIRT_RESPONSE_COMPLETE
};


#ifdef OVIRT_DEBUG
static void dump_display(OvirtVmDisplay *display)
{
    OvirtVmDisplayType type;
    guint monitor_count;
    gchar *address;
    guint port;
    gchar *ticket;
    guint expiry;

    g_object_get(G_OBJECT(display),
                 "type", &type,
                 "monitor-count", &monitor_count,
                 "address", &address,
                 "port", &port,
                 "ticket", &ticket,
                 "expiry", &expiry,
                 NULL);

    g_print("\tDisplay:\n");
    g_print("\t\tType: %s\n", (type == OVIRT_VM_DISPLAY_VNC)?"vnc":"spice");
    g_print("\t\tMonitors: %d\n", monitor_count);
    g_print("\t\tAddress: %s\n", address);
    g_print("\t\tPort: %d\n", port);
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

static gboolean ovirt_proxy_fetch_vms(OvirtProxy *proxy, GError **error)
{
    RestProxyCall *call;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);
    g_return_val_if_fail(REST_IS_PROXY(proxy->priv->rest_proxy), FALSE);

    call = REST_PROXY_CALL(ovirt_rest_call_new(proxy->priv->rest_proxy));
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

OvirtVm *ovirt_proxy_lookup_vm(OvirtProxy *proxy, const char *vm_name,
                               GError **error)
{
    OvirtVm *vm;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);
    g_return_val_if_fail(vm_name != NULL, NULL);

    if (proxy->priv->vms == NULL) {
        gboolean success;
        success = ovirt_proxy_fetch_vms(proxy, error);
        if (!success)
            return NULL;
    }
    if (proxy->priv->vms == NULL) {
        return NULL;
    }

    vm = g_hash_table_lookup(proxy->priv->vms, vm_name);

    if (vm == NULL) {
        return NULL;
    }

    return g_object_ref(vm);
}

static void parse_ticket_status(RestXmlNode *root, OvirtVm *vm, GError **error)
{
    RestXmlNode *node;
    const char *ticket_key = g_intern_string("ticket");
    const char *value_key = g_intern_string("value");
    const char *expiry_key = g_intern_string("expiry");
    OvirtVmDisplay *display;

    g_return_if_fail(root != NULL);
    g_return_if_fail(vm != NULL);
    g_return_if_fail(error == NULL || *error == NULL);

    root = g_hash_table_lookup(root->children, ticket_key);
    if (root == NULL) {
        g_return_if_reached();
    }
    node = g_hash_table_lookup(root->children, value_key);
    if (node == NULL) {
        g_return_if_reached();
    }

    g_object_get(G_OBJECT(vm), "display", &display, NULL);
    g_return_if_fail(display != NULL);
    g_object_set(G_OBJECT(display), "ticket", node->content, NULL);

    g_debug("Ticket: %s\n", node->content);

    node = g_hash_table_lookup(root->children, expiry_key);
    if (node == NULL) {
        g_object_unref(G_OBJECT(display));
        g_return_if_reached();
    }
    g_object_set(G_OBJECT(display),
                 "expiry", strtoul(node->content, NULL, 0),
                 NULL);
    g_object_unref(G_OBJECT(display));
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
        g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
    }
    node = g_hash_table_lookup(node->children, state_key);
    if (node == NULL) {
        g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
    }
    g_debug("State: %s\n", node->content);
    if (g_strcmp0(node->content, "complete") == 0) {
        return OVIRT_RESPONSE_COMPLETE;
    } else if (g_strcmp0(node->content, "pending") == 0) {
        return OVIRT_RESPONSE_PENDING;
    } else if (g_strcmp0(node->content, "in_progress") == 0) {
        return OVIRT_RESPONSE_IN_PROGRESS;
    } else if (g_strcmp0(node->content, "failed") == 0) {
        return OVIRT_RESPONSE_FAILED;
    }

    g_return_val_if_reached(OVIRT_RESPONSE_UNKNOWN);
}

static void parse_fault(RestXmlNode *root, GError **error)
{
    RestXmlNode *node;
    const char *reason_key = g_intern_string("reason");
    const char *detail_key = g_intern_string("detail");

    g_return_if_fail(g_strcmp0(root->name, "fault") == 0);

    node = g_hash_table_lookup(root->children, reason_key);
    if (node == NULL) {
        g_return_if_reached();
    }
    g_debug("Reason: %s\n", node->content);
    node = g_hash_table_lookup(root->children, detail_key);
    if (node == NULL) {
        g_return_if_reached();
    }
    g_debug("Detail: %s\n", node->content);
}

static void parse_action_response(RestProxyCall *call, OvirtVm *vm, GError **error)
{
    RestXmlParser *parser;
    RestXmlNode *root;

    parser = rest_xml_parser_new ();

    root = rest_xml_parser_parse_from_data (parser,
            rest_proxy_call_get_payload (call),
            rest_proxy_call_get_payload_length (call));

    if (g_strcmp0(root->name, "action") == 0) {
        if (parse_action_status(root, error) == OVIRT_RESPONSE_COMPLETE) {
            parse_ticket_status(root, vm, error);
        }
    } else if (g_strcmp0(root->name, "fault") == 0) {
        parse_fault(root, error);
    } else {
        g_warn_if_reached();
    }

    rest_xml_node_unref(root);
    g_object_unref(G_OBJECT(parser));
}


static gboolean ovirt_proxy_vm_action(OvirtProxy *proxy, OvirtVm *vm,
                                      const char *action, GError **error)
{
    RestProxyCall *call;
    const char *function;

    g_return_val_if_fail(OVIRT_IS_VM(vm), FALSE);
    g_return_val_if_fail(action != NULL, FALSE);
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);
    g_return_val_if_fail(REST_IS_PROXY(proxy->priv->rest_proxy), FALSE);
    g_return_val_if_fail((error == NULL) || (*error == NULL), FALSE);

    function = ovirt_vm_get_action(vm, action);
    g_return_val_if_fail(function != NULL, FALSE);

    call = REST_PROXY_CALL(ovirt_rest_call_new(proxy->priv->rest_proxy));
    rest_proxy_call_set_method(call, "POST");
    rest_proxy_call_set_function(call, function);
    rest_proxy_call_add_param(call, "async", "false");

    if (!rest_proxy_call_sync(call, error)) {
        g_warning("Error while running %s on %p", action, vm);
        parse_action_response(call, vm, NULL);
        g_object_unref(G_OBJECT(call));
        return FALSE;
    }

    parse_action_response(call, vm, NULL);

    g_object_unref(G_OBJECT(call));

    return TRUE;
}

gboolean ovirt_proxy_vm_get_ticket(OvirtProxy *proxy, OvirtVm *vm, GError **error)
{
    return ovirt_proxy_vm_action(proxy, vm, "ticket", error);
}

gboolean ovirt_proxy_vm_start(OvirtProxy *proxy, OvirtVm *vm, GError **error)
{
    return ovirt_proxy_vm_action(proxy, vm, "start", error);
}

gboolean ovirt_proxy_vm_stop(OvirtProxy *proxy, OvirtVm *vm, GError **error)
{
    return ovirt_proxy_vm_action(proxy, vm, "stop", error);
}

static void
ovirt_get_property(GObject *object, guint property_id,
                   GValue *value, GParamSpec *pspec)
{
    OvirtProxy *self = OVIRT_PROXY(object);

    switch (property_id) {
    case PROP_URI: {
        char *uri;
        g_object_get(G_OBJECT(self->priv->rest_proxy),
                     "url-format", &uri,
                     NULL);
        g_value_take_string(value, uri);
        break;
    }

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ovirt_set_property(GObject *object, guint property_id,
                   const GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
    OvirtProxy *self = OVIRT_PROXY(object);

    switch (property_id) {
    case PROP_URI:
        if (self->priv->rest_proxy)
            g_object_unref(self->priv->rest_proxy);
        self->priv->rest_proxy = rest_proxy_new(g_value_get_string(value), FALSE);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


static void
ovirt_proxy_dispose(GObject *obj)
{
    OvirtProxy *proxy = OVIRT_PROXY(obj);

    g_clear_object(&proxy->priv->rest_proxy);
    if (proxy->priv->vms) {
        g_hash_table_unref(proxy->priv->vms);
        proxy->priv->vms = NULL;
    }

    G_OBJECT_CLASS(ovirt_proxy_parent_class)->dispose(obj);
}

static void
ovirt_proxy_class_init(OvirtProxyClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS(klass);

    oclass->dispose = ovirt_proxy_dispose;
    oclass->get_property = ovirt_get_property;
    oclass->set_property = ovirt_set_property;

    g_type_class_add_private(klass, sizeof(OvirtProxyPrivate));

    g_object_class_install_property(oclass,
                                    PROP_URI,
                                    g_param_spec_string("uri",
                                                        "REST URI",
                                                        "REST URI",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
ovirt_proxy_init(OvirtProxy *self)
{
    self->priv = OVIRT_PROXY_GET_PRIVATE(self);
}

OvirtProxy *ovirt_proxy_new(const char *uri)
{
    return g_object_new(OVIRT_TYPE_PROXY, "uri", uri, NULL);
}
