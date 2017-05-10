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
    OvirtVmDisplay *display;
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
        { NULL, },
    };

    if (root == NULL) {
        return FALSE;
    }
    root = rest_xml_node_find(root, "display");
    if (root == NULL) {
        g_debug("Could not find 'display' node");
        return FALSE;
    }
    display = ovirt_vm_display_new();
    ovirt_rest_xml_node_parse(root, G_OBJECT(display), display_elements);
    g_object_get(G_OBJECT(display), "type", &type, NULL);
    if (type == OVIRT_VM_DISPLAY_INVALID) {
        return FALSE;
    }

    /* FIXME: this overrides the ticket/expiry which may
     * already be set
     */
    g_object_set(G_OBJECT(vm), "display", display, NULL);
    g_object_unref(G_OBJECT(display));

    return TRUE;
}

G_GNUC_INTERNAL gboolean ovirt_vm_refresh_from_xml(OvirtVm *vm, RestXmlNode *node)
{
    return vm_set_display_from_xml(vm, node);
}
