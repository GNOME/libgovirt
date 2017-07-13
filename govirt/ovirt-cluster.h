/*
 * ovirt-cluster.h: oVirt cluster resource
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
#ifndef __OVIRT_CLUSTER_H__
#define __OVIRT_CLUSTER_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <govirt/ovirt-collection.h>
#include <govirt/ovirt-resource.h>
#include <govirt/ovirt-types.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_CLUSTER            (ovirt_cluster_get_type ())
#define OVIRT_CLUSTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_CLUSTER, OvirtCluster))
#define OVIRT_CLUSTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_CLUSTER, OvirtClusterClass))
#define OVIRT_IS_CLUSTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_CLUSTER))
#define OVIRT_IS_CLUSTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_CLUSTER))
#define OVIRT_CLUSTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_CLUSTER, OvirtClusterClass))

typedef struct _OvirtClusterPrivate OvirtClusterPrivate;
typedef struct _OvirtClusterClass OvirtClusterClass;

struct _OvirtCluster
{
    OvirtResource parent;

    OvirtClusterPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtClusterClass
{
    OvirtResourceClass parent_class;

    gpointer padding[20];
};

GType ovirt_cluster_get_type(void);

OvirtCluster *ovirt_cluster_new(void);
OvirtCollection *ovirt_cluster_get_hosts(OvirtCluster *cluster);
OvirtDataCenter *ovirt_cluster_get_data_center(OvirtCluster *cluster);

G_END_DECLS

#endif /* __OVIRT_CLUSTER_H__ */
