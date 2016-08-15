/*
 * ovirt-action-rest-call.h: oVirt librest call proxy
 *
 * Copyright (C) 2012, 2013 Red Hat, Inc.
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
#ifndef __OVIRT_ACTION_REST_CALL_H__
#define __OVIRT_ACTION_REST_CALL_H__

#include <govirt/ovirt-rest-call.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_ACTION_REST_CALL            (ovirt_action_rest_call_get_type ())
#define OVIRT_ACTION_REST_CALL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_ACTION_REST_CALL, OvirtActionRestCall))
#define OVIRT_ACTION_REST_CALL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_ACTION_REST_CALL, OvirtActionRestCallClass))
#define OVIRT_IS_ACTION_REST_CALL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_ACTION_REST_CALL))
#define OVIRT_IS_ACTION_REST_CALL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_ACTION_REST_CALL))
#define OVIRT_ACTION_REST_CALL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_ACTION_REST_CALL, OvirtActionRestCallClass))

typedef struct _OvirtActionRestCall OvirtActionRestCall;
typedef struct _OvirtActionRestCallPrivate OvirtActionRestCallPrivate;
typedef struct _OvirtActionRestCallClass OvirtActionRestCallClass;

struct _OvirtActionRestCall
{
    OvirtRestCall parent;

    OvirtActionRestCallPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtActionRestCallClass
{
    OvirtRestCallClass parent_class;

    gpointer padding[20];
};

G_GNUC_INTERNAL GType ovirt_action_rest_call_get_type(void);
G_GNUC_INTERNAL OvirtActionRestCall *ovirt_action_rest_call_new(RestProxy *proxy);

G_END_DECLS

#endif /* __OVIRT_ACTION_REST_CALL_H__ */
