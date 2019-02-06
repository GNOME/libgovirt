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
#include "ovirt-proxy-private.h"
#include "ovirt-rest-call.h"
#include "ovirt-rest-call-error.h"
#include "ovirt-utils.h"

struct _OvirtRestCallPrivate {
    char *href;
};


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(OvirtRestCall, ovirt_rest_call, REST_TYPE_PROXY_CALL);


enum {
    PROP_0,
    PROP_METHOD,
    PROP_HREF,
};


GQuark ovirt_rest_call_error_quark(void)
{
    return g_quark_from_static_string("ovirt-rest-call");
}


static void ovirt_rest_call_get_property(GObject *object,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
    OvirtRestCall *call = OVIRT_REST_CALL(object);

    switch (prop_id) {
    case PROP_METHOD: {
        const char *method;

        method = rest_proxy_call_get_method(REST_PROXY_CALL(call));
        g_value_set_string(value, method);
        break;
    }
    case PROP_HREF:
        g_value_set_string(value, call->priv->href);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}


static void ovirt_rest_call_set_property(GObject *object,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
    OvirtRestCall *call = OVIRT_REST_CALL(object);

    switch (prop_id) {
    case PROP_METHOD:
        rest_proxy_call_set_method(REST_PROXY_CALL(call),
                                   g_value_get_string(value));
        break;
    case PROP_HREF:
        g_free(call->priv->href);
        call->priv->href = g_value_dup_string(value);
        rest_proxy_call_set_function(REST_PROXY_CALL(call), call->priv->href);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}


static void ovirt_rest_call_finalize(GObject *object)
{
    OvirtRestCall *call = OVIRT_REST_CALL(object);

    g_free(call->priv->href);

    G_OBJECT_CLASS(ovirt_rest_call_parent_class)->finalize(object);
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
        ovirt_proxy_append_additional_headers(proxy, REST_PROXY_CALL(object));

        g_object_unref(proxy);
    }
}


static void ovirt_rest_call_class_init(OvirtRestCallClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GParamSpec *param_spec;

    object_class->get_property = ovirt_rest_call_get_property;
    object_class->set_property = ovirt_rest_call_set_property;
    object_class->constructed = ovirt_rest_call_constructed;
    object_class->finalize = ovirt_rest_call_finalize;

    param_spec = g_param_spec_string("method",
                                     "Method",
                                     "REST method for the call",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_METHOD,
                                    param_spec);
    param_spec = g_param_spec_string("href",
                                     "Href",
                                     "Resource Href",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_HREF,
                                    param_spec);
}


static void ovirt_rest_call_init(G_GNUC_UNUSED OvirtRestCall *call)
{
    call->priv = ovirt_rest_call_get_instance_private(call);
}
