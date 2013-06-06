/*
 * ovirt-vm-xml.c
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
#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "ovirt-vm.h"
#include "ovirt-vm-display.h"
#include "ovirt-vm-private.h"

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
        } else if (strcmp(state->content, "reboot_in_progress") == 0) {
            g_object_set(G_OBJECT(vm), "state", OVIRT_VM_STATE_REBOOTING, NULL);
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
        if (g_str_has_prefix(href, OVIRT_API_BASE_DIR)) {
            href += strlen(OVIRT_API_BASE_DIR);
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

G_GNUC_INTERNAL gboolean ovirt_vm_refresh_from_xml(OvirtVm *vm, RestXmlNode *node)
{
    const char *uuid;
    const char *href;

    uuid = rest_xml_node_get_attr(node, "id");
    g_return_val_if_fail(uuid != NULL, FALSE);
    href = rest_xml_node_get_attr(node, "href");
    g_return_val_if_fail(href != NULL, FALSE);

    g_object_set(G_OBJECT(vm), "uuid", uuid, "href", href, NULL);

    vm_set_name_from_xml(vm, node);
    vm_set_state_from_xml(vm, node);
    vm_set_actions_from_xml(vm, node);
    vm_set_display_from_xml(vm, node);

    return TRUE;
}

