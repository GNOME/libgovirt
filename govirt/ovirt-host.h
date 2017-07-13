/*
 * ovirt-host.h: oVirt host resource
 *
 * Copyright (C) 2017 Red Hat, Inc.
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
#ifndef __OVIRT_HOST_H__
#define __OVIRT_HOST_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <govirt/ovirt-collection.h>
#include <govirt/ovirt-resource.h>
#include <govirt/ovirt-types.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_HOST            (ovirt_host_get_type ())
#define OVIRT_HOST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_HOST, OvirtHost))
#define OVIRT_HOST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_HOST, OvirtHostClass))
#define OVIRT_IS_HOST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_HOST))
#define OVIRT_IS_HOST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_HOST))
#define OVIRT_HOST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_HOST, OvirtHostClass))

typedef struct _OvirtHostPrivate OvirtHostPrivate;
typedef struct _OvirtHostClass OvirtHostClass;

struct _OvirtHost
{
    OvirtResource parent;

    OvirtHostPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtHostClass
{
    OvirtResourceClass parent_class;

    gpointer padding[20];
};

GType ovirt_host_get_type(void);

OvirtHost *ovirt_host_new(void);
OvirtCollection *ovirt_host_get_vms(OvirtHost *host);
OvirtCluster *ovirt_host_get_cluster(OvirtHost *host);

G_END_DECLS

#endif /* __OVIRT_HOST_H__ */
