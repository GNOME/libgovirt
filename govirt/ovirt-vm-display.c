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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Christophe Fergeau <cfergeau@redhat.com>
 */

#include <config.h>

#include "ovirt-enum-types.h"
#include "ovirt-vm-display.h"
#include "ovirt-utils.h"

struct _OvirtVmDisplayPrivate {
    OvirtVmDisplayType type;
    char *address;
    guint port;
    guint secure_port;
    guint monitor_count;
    char *ticket;
    guint expiry;
    GByteArray *ca_cert;
    char *host_subject;
    gboolean smartcard;
    gboolean allow_override;
    char *proxy_url;
};

G_DEFINE_TYPE_WITH_PRIVATE(OvirtVmDisplay, ovirt_vm_display, G_TYPE_OBJECT);

enum {
    PROP_0,
    PROP_TYPE,
    PROP_ADDRESS,
    PROP_PORT,
    PROP_SECURE_PORT,
    PROP_MONITOR_COUNT,
    PROP_TICKET,
    PROP_EXPIRY,
    PROP_HOST_SUBJECT,
    PROP_SMARTCARD,
    PROP_ALLOW_OVERRIDE,
    PROP_PROXY_URL,
    PROP_CA_CERT,
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
    case PROP_SECURE_PORT:
        g_value_set_uint(value, display->priv->secure_port);
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
    case PROP_CA_CERT:
        g_value_set_boxed(value, display->priv->ca_cert);
        break;
    case PROP_HOST_SUBJECT:
        g_value_set_string(value, display->priv->host_subject);
        break;
    case PROP_SMARTCARD:
        g_value_set_boolean(value, display->priv->smartcard);
        break;
    case PROP_ALLOW_OVERRIDE:
        g_value_set_boolean(value, display->priv->allow_override);
        break;
    case PROP_PROXY_URL:
        g_value_set_string(value, display->priv->proxy_url);
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
    case PROP_SECURE_PORT:
        display->priv->secure_port = g_value_get_uint(value);
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
    case PROP_CA_CERT:
        if (display->priv->ca_cert != NULL) {
            g_byte_array_unref(display->priv->ca_cert);
        }
        display->priv->ca_cert = g_value_dup_boxed(value);
        break;
    case PROP_HOST_SUBJECT:
        g_free(display->priv->host_subject);
        display->priv->host_subject = g_value_dup_string(value);
        break;
    case PROP_SMARTCARD:
        display->priv->smartcard = g_value_get_boolean(value);
        break;
    case PROP_ALLOW_OVERRIDE:
        display->priv->allow_override = g_value_get_boolean(value);
        break;
    case PROP_PROXY_URL:
        g_free(display->priv->proxy_url);
        display->priv->proxy_url = g_value_dup_string(value);
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
    g_free(display->priv->host_subject);
    g_free(display->priv->proxy_url);
    if (display->priv->ca_cert != NULL) {
        g_byte_array_unref(display->priv->ca_cert);
    }

    G_OBJECT_CLASS(ovirt_vm_display_parent_class)->finalize(object);
}

static void ovirt_vm_display_class_init(OvirtVmDisplayClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = ovirt_vm_display_finalize;
    object_class->get_property = ovirt_vm_display_get_property;
    object_class->set_property = ovirt_vm_display_set_property;

    g_object_class_install_property(object_class,
                                    PROP_TYPE,
                                    g_param_spec_enum("type",
                                                        "Type",
                                                        "Display Type",
                                                        OVIRT_TYPE_VM_DISPLAY_TYPE,
                                                        OVIRT_VM_DISPLAY_INVALID,
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
                                    PROP_SECURE_PORT,
                                    g_param_spec_uint("secure-port",
                                                      "Secure Port",
                                                      "Secure Display Port",
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
    g_object_class_install_property(object_class,
                                    PROP_CA_CERT,
                                    g_param_spec_boxed("ca-cert",
                                                       "ca-cert",
                                                       "Virt CA certificate to use for TLS SPICE connections",
                                                        G_TYPE_BYTE_ARRAY,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_HOST_SUBJECT,
                                    g_param_spec_string("host-subject",
                                                        "Host Subject",
                                                        "Host subject of the VM certificate",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_SMARTCARD,
                                    g_param_spec_boolean("smartcard",
                                                        "Smartcard",
                                                        "Indicates whether smartcard support is enabled",
                                                        FALSE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_ALLOW_OVERRIDE,
                                    g_param_spec_boolean("allow-override",
                                                        "Allow override",
                                                        "Allow to override display connection",
                                                        FALSE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_PROXY_URL,
                                    g_param_spec_string("proxy-url",
                                                        "Proxy URL",
                                                        "URL of the proxy to use to access the VM",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void ovirt_vm_display_init(G_GNUC_UNUSED OvirtVmDisplay *display)
{
    display->priv = ovirt_vm_display_get_instance_private(display);
}

OvirtVmDisplay *ovirt_vm_display_new(void)
{
    return OVIRT_VM_DISPLAY(g_object_new(OVIRT_TYPE_VM_DISPLAY, NULL));
}

static gboolean ovirt_vm_display_set_from_xml(OvirtVmDisplay *display, RestXmlNode *node)
{
    OvirtVmDisplayType type;
    OvirtXmlElement display_elements[] = {
        { .prop_name = "type",
          .xml_path = "type",
        },
        { .prop_name = "address",
          .xml_path = "address",
        },
        { .prop_name = "port",
          .xml_path = "port",
        },
        { .prop_name = "secure-port",
          .xml_path = "secure_port",
        },
        { .prop_name = "monitor-count",
          .xml_path = "monitors",
        },
        { .prop_name = "smartcard",
          .xml_path = "smartcard_enabled",
        },
        { .prop_name = "allow-override",
          .xml_path = "allow_override",
        },
        { .prop_name = "host-subject",
          .xml_path = "certificate/subject",
        },
        { .prop_name = "proxy-url",
          .xml_path = "proxy",
        },
        { .prop_name = "ca-cert",
          .xml_path = "certificate/content",
        },
        { NULL, },
    };

    ovirt_rest_xml_node_parse(node, G_OBJECT(display), display_elements);
    g_object_get(G_OBJECT(display), "type", &type, NULL);
    if (type == OVIRT_VM_DISPLAY_INVALID) {
        return FALSE;
    }

    return TRUE;
}

OvirtVmDisplay *ovirt_vm_display_new_from_xml(RestXmlNode *node)
{
    OvirtVmDisplay *display;

    g_return_val_if_fail(node != NULL, NULL);

    display = ovirt_vm_display_new();

    if (!ovirt_vm_display_set_from_xml(display, node)) {
        g_object_unref(display);
        return NULL;
    }

    return display;
}
