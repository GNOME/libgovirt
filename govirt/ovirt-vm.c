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
#include <glib/gi18n-lib.h>
#include <rest/rest-xml-node.h>
#include <rest/rest-xml-parser.h>

#include "govirt.h"
#include "govirt-private.h"


static gboolean parse_ticket_status(RestXmlNode *root, OvirtResource *resource,
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


void
ovirt_vm_get_ticket_async(OvirtVm *vm, OvirtProxy *proxy,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
    ovirt_resource_invoke_action_async(OVIRT_RESOURCE(vm), "ticket",
                                       proxy, parse_ticket_status,
                                       cancellable, callback,
                                       user_data);
}

gboolean
ovirt_vm_get_ticket_finish(OvirtVm *vm, GAsyncResult *result, GError **err)
{
    return ovirt_resource_action_finish(OVIRT_RESOURCE(vm), result, err);
}

void
ovirt_vm_start_async(OvirtVm *vm, OvirtProxy *proxy,
                     GCancellable *cancellable,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    ovirt_resource_invoke_action_async(OVIRT_RESOURCE(vm), "start", proxy, NULL,
                                       cancellable, callback, user_data);
}

gboolean
ovirt_vm_start_finish(OvirtVm *vm, GAsyncResult *result, GError **err)
{
    return ovirt_resource_action_finish(OVIRT_RESOURCE(vm), result, err);
}

void
ovirt_vm_stop_async(OvirtVm *vm, OvirtProxy *proxy,
                    GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
    ovirt_resource_invoke_action_async(OVIRT_RESOURCE(vm), "stop", proxy, NULL,
                                       cancellable, callback, user_data);
}

gboolean
ovirt_vm_stop_finish(OvirtVm *vm, GAsyncResult *result, GError **err)
{
    return ovirt_resource_action_finish(OVIRT_RESOURCE(vm), result, err);
}


gboolean ovirt_vm_get_ticket(OvirtVm *vm, OvirtProxy *proxy, GError **error)
{
    return ovirt_resource_action(OVIRT_RESOURCE(vm), proxy, "ticket",
                                 parse_ticket_status,
                                 error);
}

gboolean ovirt_vm_start(OvirtVm *vm, OvirtProxy *proxy, GError **error)
{
    return ovirt_resource_action(OVIRT_RESOURCE(vm), proxy, "start",
                                 NULL, error);
}

gboolean ovirt_vm_stop(OvirtVm *vm, OvirtProxy *proxy, GError **error)
{
    return ovirt_resource_action(OVIRT_RESOURCE(vm), proxy, "stop",
                                 NULL, error);
}

static gboolean parse_ticket_status(RestXmlNode *root, OvirtResource *resource, GError **error)
{
    OvirtVm *vm;
    RestXmlNode *node;
    const char *ticket_key = g_intern_string("ticket");
    const char *value_key = g_intern_string("value");
    const char *expiry_key = g_intern_string("expiry");
    OvirtVmDisplay *display;

    g_return_val_if_fail(root != NULL, FALSE);
    g_return_val_if_fail(OVIRT_IS_VM(resource), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    vm = OVIRT_VM(resource);
    root = g_hash_table_lookup(root->children, ticket_key);
    if (root == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    _("Could not find 'ticket' node"));
        g_return_val_if_reached(FALSE);
    }
    node = g_hash_table_lookup(root->children, value_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    _("Could not find 'value' node"));
        g_return_val_if_reached(FALSE);
    }

    g_object_get(G_OBJECT(vm), "display", &display, NULL);
    g_return_val_if_fail(display != NULL, FALSE);
    g_object_set(G_OBJECT(display), "ticket", node->content, NULL);

    g_debug("Ticket: %s\n", node->content);

    node = g_hash_table_lookup(root->children, expiry_key);
    if (node == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    _("Could not find 'expiry' node"));
        g_object_unref(G_OBJECT(display));
        g_return_val_if_reached(FALSE);
    }
    g_object_set(G_OBJECT(display),
                 "expiry", strtoul(node->content, NULL, 0),
                 NULL);
    g_object_unref(G_OBJECT(display));

    return TRUE;
}


void ovirt_vm_refresh_async(OvirtVm *vm, OvirtProxy *proxy,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
    g_return_if_fail(OVIRT_IS_VM(vm));

    ovirt_resource_refresh_async(OVIRT_RESOURCE(vm), proxy,
                                 cancellable, callback,
                                 user_data);
}


gboolean ovirt_vm_refresh_finish(OvirtVm *vm,
                                 GAsyncResult *result,
                                 GError **err)
{
    g_return_val_if_fail(OVIRT_IS_VM(vm), FALSE);
    return ovirt_resource_refresh_finish(OVIRT_RESOURCE(vm),
                                         result, err);
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
 * Return value: (transfer none): a #OvirtCollection representing the list
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
