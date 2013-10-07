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

#include "ovirt-enum-types.h"
#include "ovirt-utils.h"
#include "ovirt-vm.h"
#include "ovirt-vm-display.h"
#include "ovirt-vm-private.h"

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
    const char *certificate_key = g_intern_string("certificate");
    const char *smartcard_key = g_intern_string("smartcard_enabled");
    const char *allow_override_key = g_intern_string("allow_override");

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

    node = g_hash_table_lookup(root->children, smartcard_key);
    if (node != NULL) {
        gboolean smartcard;

        smartcard = (g_strcmp0(node->content, "true") == 0);
        g_object_set(G_OBJECT(display),
                     "smartcard", smartcard,
                     NULL);
    }

    node = g_hash_table_lookup(root->children, allow_override_key);
    if (node != NULL) {
        gboolean allow_override;

        allow_override = (g_strcmp0(node->content, "true") == 0);
        g_object_set(G_OBJECT(display),
                     "allow-override", allow_override,
                     NULL);
    }

    node = g_hash_table_lookup(root->children, certificate_key);
    if (node != NULL) {
        const char *subject_key = g_intern_string("subject");
        node = g_hash_table_lookup(node->children, subject_key);
        if (node != NULL) {
            g_object_set(G_OBJECT(display),
                         "host-subject", node->content,
                         NULL);
        }
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
    RestXmlNode *state_node;

    state_node = rest_xml_node_find(node, "status");
    state_node = rest_xml_node_find(state_node, "state");
    if (state_node != NULL) {
        int state;

        g_return_val_if_fail(state_node->content != NULL, FALSE);
        state = ovirt_utils_genum_get_value(OVIRT_TYPE_VM_STATE,
                                            state_node->content,
                                            OVIRT_VM_STATE_UNKNOWN);
        g_object_set(G_OBJECT(vm), "state", state, NULL);

        return TRUE;
    }

    return FALSE;
}

G_GNUC_INTERNAL gboolean ovirt_vm_refresh_from_xml(OvirtVm *vm, RestXmlNode *node)
{
    vm_set_state_from_xml(vm, node);
    vm_set_display_from_xml(vm, node);

    return TRUE;
}
