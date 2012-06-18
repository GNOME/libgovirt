
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 * Author: Christophe Fergeau <cfergeau@redhat.com>
 */
#ifndef __OVIRT_VM_PRIVATE_H__
#define __OVIRT_VM_PRIVATE_H__

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

void ovirt_vm_add_action(OvirtVm *vm, const char *action, const char *url);

G_END_DECLS

#endif /* __OVIRT_VM_H__ */
