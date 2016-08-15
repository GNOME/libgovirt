
/*
 * ovirt-vm-private.h: oVirt virtual machine private header
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
#ifndef __OVIRT_VM_PRIVATE_H__
#define __OVIRT_VM_PRIVATE_H__

#include <govirt/ovirt-vm.h>
#include <rest/rest-xml-node.h>

G_BEGIN_DECLS

typedef enum {
    OVIRT_VM_ACTION_SHUTDOWN,
    OVIRT_VM_ACTION_START,
    OVIRT_VM_ACTION_STOP,
    OVIRT_VM_ACTION_SUSPEND,
    OVIRT_VM_ACTION_DETACH,
    OVIRT_VM_ACTION_EXPORT,
    OVIRT_VM_ACTION_MOVE,
    OVIRT_VM_ACTION_TICKET,
    OVIRT_VM_ACTION_MIGRATE
} OvirtVmAction;

gboolean ovirt_vm_refresh_from_xml(OvirtVm *vm, RestXmlNode *node);
OvirtVm *ovirt_vm_new_from_xml(RestXmlNode *node, GError **error);

G_END_DECLS

#endif /* __OVIRT_VM_PRIVATE_H__ */
