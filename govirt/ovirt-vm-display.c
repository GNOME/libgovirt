/*
 * ovirt-vm-display.c: oVirt virtual machine display information
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
#include "ovirt-vm-display.h"

#define OVIRT_VM_DISPLAY_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), OVIRT_TYPE_VM_DISPLAY, OvirtVmDisplayPrivate))

struct _OvirtVmDisplayPrivate {
    OvirtVmDisplayType type;
    char *address;
    guint port;
    guint monitor_count;
    char *ticket;
    guint expiry;
};

G_DEFINE_TYPE(OvirtVmDisplay, ovirt_vm_display, G_TYPE_OBJECT);

enum {
    PROP_0,
    PROP_TYPE,
    PROP_ADDRESS,
    PROP_PORT,
    PROP_MONITOR_COUNT,
    PROP_TICKET,
    PROP_EXPIRY
};

static void ovirt_vm_display_get_property(GObject *object,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec)
{
    OvirtVmDisplay *display = OVIRT_VM_DISPLAY(object);

    switch (prop_id) {
    case PROP_TYPE:
        g_value_set_enum(value, display->priv->type);
        break;
    case PROP_ADDRESS:
        g_value_set_string(value, display->priv->address);
        break;
    case PROP_PORT:
        g_value_set_uint(value, display->priv->port);
        break;
    case PROP_MONITOR_COUNT:
        g_value_set_uint(value, display->priv->monitor_count);
        break;
    case PROP_TICKET:
        g_value_set_string(value, display->priv->ticket);
        break;
    case PROP_EXPIRY:
        g_value_set_uint(value, display->priv->expiry);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void ovirt_vm_display_set_property(GObject *object,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
    OvirtVmDisplay *display = OVIRT_VM_DISPLAY(object);

    switch (prop_id) {
    case PROP_TYPE:
        display->priv->type = g_value_get_enum(value);
        break;
    case PROP_ADDRESS:
        g_free(display->priv->address);
        display->priv->address = g_value_dup_string(value);
        break;
    case PROP_PORT:
        display->priv->port = g_value_get_uint(value);
        break;
    case PROP_MONITOR_COUNT:
        display->priv->monitor_count = g_value_get_uint(value);
        break;
    case PROP_TICKET:
        g_free(display->priv->ticket);
        display->priv->ticket = g_value_dup_string(value);
        break;
    case PROP_EXPIRY:
        display->priv->expiry = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void ovirt_vm_display_finalize(GObject *object)
{
    OvirtVmDisplay *display = OVIRT_VM_DISPLAY(object);

    g_free(display->priv->address);
    g_free(display->priv->ticket);

    G_OBJECT_CLASS(ovirt_vm_display_parent_class)->finalize(object);
}

static void ovirt_vm_display_class_init(OvirtVmDisplayClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(OvirtVmDisplayPrivate));

    object_class->finalize = ovirt_vm_display_finalize;
    object_class->get_property = ovirt_vm_display_get_property;
    object_class->set_property = ovirt_vm_display_set_property;

    g_object_class_install_property(object_class,
                                    PROP_TYPE,
                                    g_param_spec_enum("type",
                                                        "Type",
                                                        "Display Type",
                                                        OVIRT_TYPE_VM_DISPLAY_TYPE,
                                                        OVIRT_VM_DISPLAY_SPICE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_ADDRESS,
                                    g_param_spec_string("address",
                                                        "Address",
                                                        "Display Address",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_PORT,
                                    g_param_spec_uint("port",
                                                      "Port",
                                                      "Display Port",
                                                      0, G_MAXUINT16,
                                                      0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_MONITOR_COUNT,
                                    g_param_spec_uint("monitor-count",
                                                      "Monitor Count",
                                                      "Virtual Machine Monitor Count",
                                                      0, G_MAXUINT,
                                                      0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_TICKET,
                                    g_param_spec_string("ticket",
                                                        "Ticket",
                                                        "Ticket to access the VM",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_EXPIRY,
                                    g_param_spec_uint("expiry",
                                                      "Expiry",
                                                      "Ticket Expiry Time",
                                                      0, G_MAXUINT,
                                                      0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));
}

static void ovirt_vm_display_init(G_GNUC_UNUSED OvirtVmDisplay *display)
{
    display->priv = OVIRT_VM_DISPLAY_GET_PRIVATE(display);
}

OvirtVmDisplay *ovirt_vm_display_new(void)
{
    return OVIRT_VM_DISPLAY(g_object_new(OVIRT_TYPE_VM_DISPLAY, NULL));
}