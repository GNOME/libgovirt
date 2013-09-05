/*
 * ovirt-rest-call.c: oVirt librest call proxy
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

#include <config.h>

#include <rest/rest-params.h>

#include "ovirt-proxy.h"
#include "ovirt-rest-call.h"
#include "ovirt-rest-call-error.h"
#include "ovirt-utils.h"

#define OVIRT_REST_CALL_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), OVIRT_TYPE_REST_CALL, OvirtRestCallPrivate))


struct _OvirtRestCallPrivate {
    gboolean unused;
};


G_DEFINE_ABSTRACT_TYPE(OvirtRestCall, ovirt_rest_call, REST_TYPE_PROXY_CALL);


GQuark ovirt_rest_call_error_quark(void)
{
    return g_quark_from_static_string("ovirt-rest-call");
}


static void ovirt_rest_call_constructed(GObject *object)
{
    OvirtProxy *proxy;

    G_OBJECT_CLASS(ovirt_rest_call_parent_class)->constructed(object);

    g_object_get(object, "proxy", &proxy, NULL);
    if (proxy != NULL) {
        gboolean admin;
        g_object_get(G_OBJECT(proxy), "admin", &admin, NULL);
        if (admin) {
            rest_proxy_call_add_header(REST_PROXY_CALL(object), "Filter", "false");
        } else {
            rest_proxy_call_add_header(REST_PROXY_CALL(object), "Filter", "true");
        }
        g_object_unref(proxy);
    }
}


static void ovirt_rest_call_class_init(OvirtRestCallClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GParamSpec *param_spec;

    g_type_class_add_private(klass, sizeof(OvirtRestCallPrivate));

    object_class->constructed = ovirt_rest_call_constructed;
}


static void ovirt_rest_call_init(G_GNUC_UNUSED OvirtRestCall *call)
{
    call->priv = OVIRT_REST_CALL_GET_PRIVATE(call);
}
