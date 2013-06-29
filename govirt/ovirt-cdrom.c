/*
 * ovirt-cdrom.c: oVirt cdrom handling
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
#include "ovirt-cdrom.h"

#define OVIRT_CDROM_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), OVIRT_TYPE_CDROM, OvirtCdromPrivate))

struct _OvirtCdromPrivate {
    gboolean unused;
};

G_DEFINE_TYPE(OvirtCdrom, ovirt_cdrom, OVIRT_TYPE_RESOURCE);

static void ovirt_cdrom_class_init(OvirtCdromClass *klass)
{
    g_type_class_add_private(klass, sizeof(OvirtCdromPrivate));
}

static void ovirt_cdrom_init(OvirtCdrom *cdrom)
{
    cdrom->priv = OVIRT_CDROM_GET_PRIVATE(cdrom);
}
