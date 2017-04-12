/*
 * ovirt-data_center.h: oVirt data center resource
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
#ifndef __OVIRT_DATA_CENTER_H__
#define __OVIRT_DATA_CENTER_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <govirt/ovirt-collection.h>
#include <govirt/ovirt-resource.h>
#include <govirt/ovirt-types.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_DATA_CENTER            (ovirt_data_center_get_type ())
#define OVIRT_DATA_CENTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_DATA_CENTER, OvirtDataCenter))
#define OVIRT_DATA_CENTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_DATA_CENTER, OvirtDataCenterClass))
#define OVIRT_IS_DATA_CENTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_DATA_CENTER))
#define OVIRT_IS_DATA_CENTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_DATA_CENTER))
#define OVIRT_DATA_CENTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_DATA_CENTER, OvirtDataCenterClass))

typedef struct _OvirtDataCenterPrivate OvirtDataCenterPrivate;
typedef struct _OvirtDataCenterClass OvirtDataCenterClass;

struct _OvirtDataCenter
{
    OvirtResource parent;

    OvirtDataCenterPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtDataCenterClass
{
    OvirtResourceClass parent_class;

    gpointer padding[20];
};

GType ovirt_data_center_get_type(void);

OvirtDataCenter *ovirt_data_center_new(void);
OvirtCollection *ovirt_data_center_get_clusters(OvirtDataCenter *data_center);
OvirtCollection *ovirt_data_center_get_storage_domains(OvirtDataCenter *data_center);

G_END_DECLS

#endif /* __OVIRT_DATA_CENTER_H__ */
