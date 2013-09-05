/*
 * ovirt-rest-call.h: oVirt librest call proxy
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
#ifndef __OVIRT_REST_CALL_H__
#define __OVIRT_REST_CALL_H__

#include <rest/rest-proxy.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_REST_CALL            (ovirt_rest_call_get_type ())
#define OVIRT_REST_CALL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_REST_CALL, OvirtRestCall))
#define OVIRT_REST_CALL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_REST_CALL, OvirtRestCallClass))
#define OVIRT_IS_REST_CALL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_REST_CALL))
#define OVIRT_IS_REST_CALL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_REST_CALL))
#define OVIRT_REST_CALL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_REST_CALL, OvirtRestCallClass))

typedef struct _OvirtRestCall OvirtRestCall;
typedef struct _OvirtRestCallPrivate OvirtRestCallPrivate;
typedef struct _OvirtRestCallClass OvirtRestCallClass;

struct _OvirtRestCall
{
    RestProxyCall parent;

    OvirtRestCallPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtRestCallClass
{
    RestProxyCallClass parent_class;

    gpointer padding[20];
};

G_GNUC_INTERNAL GType ovirt_rest_call_get_type(void);

G_END_DECLS

#endif /* __OVIRT_REST_CALL_H__ */
