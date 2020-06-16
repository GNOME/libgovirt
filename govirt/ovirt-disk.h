/*
 * ovirt-disk.h: oVirt disk resource
 *
 * Copyright (C) 2020 Red Hat, Inc.
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
 * Author: Eduardo Lima (Etrunko) <etrunko@redhat.com>
 */
#ifndef __OVIRT_DISK_H__
#define __OVIRT_DISK_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <govirt/ovirt-resource.h>
#include <govirt/ovirt-types.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_DISK            (ovirt_disk_get_type ())
#define OVIRT_DISK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_DISK, OvirtDisk))
#define OVIRT_DISK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_DISK, OvirtDiskClass))
#define OVIRT_IS_DISK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_DISK))
#define OVIRT_IS_DISK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_DISK))
#define OVIRT_DISK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_DISK, OvirtDiskClass))

typedef enum {
    OVIRT_DISK_CONTENT_TYPE_DATA,
    OVIRT_DISK_CONTENT_TYPE_HOSTED_ENGINE,
    OVIRT_DISK_CONTENT_TYPE_HOSTED_ENGINE_CONFIGURATION,
    OVIRT_DISK_CONTENT_TYPE_HOSTED_ENGINE_METADATA,
    OVIRT_DISK_CONTENT_TYPE_HOSTED_ENGINE_SANLOCK,
    OVIRT_DISK_CONTENT_TYPE_ISO,
    OVIRT_DISK_CONTENT_TYPE_MEMORY_DUMP_VOLUME,
    OVIRT_DISK_CONTENT_TYPE_METADATA_VOLUME,
    OVIRT_DISK_CONTENT_TYPE_OVF_STORE,
} OvirtDiskContentType;

typedef struct _OvirtDiskPrivate OvirtDiskPrivate;
typedef struct _OvirtDiskClass OvirtDiskClass;

struct _OvirtDisk
{
    OvirtResource parent;

    OvirtDiskPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtDiskClass
{
    OvirtResourceClass parent_class;

    gpointer padding[20];
};

GType ovirt_disk_get_type(void);

OvirtDisk *ovirt_disk_new(void);

G_END_DECLS

#endif /* __OVIRT_DISK_H__ */
