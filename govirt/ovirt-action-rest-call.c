/*
 * ovirt-action-rest-call.c: oVirt librest call proxy
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

#include <glib/gi18n-lib.h>

#include "ovirt-action-rest-call.h"
#include "ovirt-proxy.h"
#include "ovirt-rest-call-error.h"
#include <rest/rest-params.h>

G_DEFINE_TYPE(OvirtActionRestCall, ovirt_action_rest_call, OVIRT_TYPE_REST_CALL);

static gboolean ovirt_action_rest_call_class_serialize_params(RestProxyCall *call,
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

    g_return_val_if_fail(OVIRT_IS_ACTION_REST_CALL(call), FALSE);
    g_return_val_if_fail(content_type != NULL, FALSE);
    g_return_val_if_fail(content != NULL, FALSE);
    g_return_val_if_fail(content_len != NULL, FALSE);

    params = rest_proxy_call_get_params(call);
    if (!rest_params_are_strings(params)) {
        g_set_error(error, OVIRT_REST_CALL_ERROR, 0,
                    _("Unexpected parameter type in REST call"));
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

static void ovirt_action_rest_call_class_init(OvirtActionRestCallClass *klass)
{
    REST_PROXY_CALL_CLASS(klass)->serialize_params = ovirt_action_rest_call_class_serialize_params;
}


static void ovirt_action_rest_call_init(G_GNUC_UNUSED OvirtActionRestCall *call)
{
}

OvirtActionRestCall *ovirt_action_rest_call_new(RestProxy *proxy)
{
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);
    return OVIRT_ACTION_REST_CALL(g_object_new(OVIRT_TYPE_ACTION_REST_CALL,
                                               "proxy", proxy, NULL));
}
