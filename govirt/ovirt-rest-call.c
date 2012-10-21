/*
 * ovirt-rest-call.c: oVirt librest call proxy
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

#include <config.h>

#include "ovirt-proxy.h"
#include "ovirt-rest-call.h"
#include "ovirt-rest-call-error.h"
#include <rest/rest-params.h>

#define OVIRT_REST_CALL_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), OVIRT_TYPE_REST_CALL, OvirtRestCallPrivate))

G_DEFINE_TYPE(OvirtRestCall, ovirt_rest_call, REST_TYPE_PROXY_CALL);

GQuark ovirt_rest_call_error_quark(void)
{
    return g_quark_from_static_string("ovirt-rest-call");
}

static gboolean ovirt_rest_call_class_serialize_params(RestProxyCall *call,
                                                     gchar **content_type,
                                                     gchar **content,
                                                     gsize *content_len,
                                                     GError **error)
{
    RestParams *params;
    RestParamsIter it;
    GString *body;
    const char *name;
    RestParam *param;

    g_return_val_if_fail(OVIRT_IS_REST_CALL(call), FALSE);
    g_return_val_if_fail(content_type != NULL, FALSE);
    g_return_val_if_fail(content != NULL, FALSE);
    g_return_val_if_fail(content_len != NULL, FALSE);

    params = rest_proxy_call_get_params(call);
    if (!rest_params_are_strings(params)) {
        g_set_error(error, OVIRT_REST_CALL_ERROR, 0,
                    "unexpected parameter type in REST call");
        return FALSE;
    }

    body = g_string_new("<action>");
    rest_params_iter_init(&it, params);
    while (rest_params_iter_next(&it, &name, &param)) {
        const char *val = rest_param_get_content(param);
        g_string_append_printf(body, "<%s>%s</%s>", name, val, name);
    }
    g_string_append(body, "</action>");

    *content_type = g_strdup("application/xml");
    *content = body->str;
    *content_len = body->len;

    g_string_free(body, FALSE);

    return TRUE;
}

static void ovirt_rest_call_class_init(OvirtRestCallClass *klass)
{
    REST_PROXY_CALL_CLASS(klass)->serialize_params = ovirt_rest_call_class_serialize_params;
}


static void ovirt_rest_call_init(G_GNUC_UNUSED OvirtRestCall *call)
{
}

OvirtRestCall *ovirt_rest_call_new(RestProxy *proxy)
{
    OvirtRestCall *call;
    gboolean admin;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);
    call = OVIRT_REST_CALL(g_object_new(OVIRT_TYPE_REST_CALL, "proxy", proxy, NULL));
    g_return_val_if_fail(call != NULL, NULL);
    g_object_get(G_OBJECT(proxy), "admin", &admin, NULL);
    if (admin) {
        rest_proxy_call_add_header(REST_PROXY_CALL(call), "Filter", "false");
    } else {
        rest_proxy_call_add_header(REST_PROXY_CALL(call), "Filter", "true");
    }

    return call;
}
