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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Christophe Fergeau <cfergeau@redhat.com>
 */

#include <config.h>

#include <string.h>

#include <glib/gi18n-lib.h>
#include <libsoup/soup.h>
#include <rest/rest-params.h>

#include "ovirt-proxy.h"
#include "ovirt-resource-private.h"
#include "ovirt-resource-rest-call.h"
#include "ovirt-rest-call-error.h"
#include "ovirt-utils.h"

struct _OvirtResourceRestCallPrivate {
    OvirtResource *resource;
} ;
G_DEFINE_TYPE_WITH_PRIVATE(OvirtResourceRestCall, ovirt_resource_rest_call, OVIRT_TYPE_REST_CALL);

enum {
    PROP_0,
    PROP_RESOURCE,
};


static void ovirt_resource_rest_call_get_property(GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    OvirtResourceRestCall *call = OVIRT_RESOURCE_REST_CALL(object);

    switch (prop_id) {
    case PROP_RESOURCE:
        g_value_set_object(value, call->priv->resource);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}


static void ovirt_resource_rest_call_set_property(GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec)
{
    OvirtResourceRestCall *call = OVIRT_RESOURCE_REST_CALL(object);

    switch (prop_id) {
    case PROP_RESOURCE: {
        char *href;

        call->priv->resource = g_value_dup_object(value);
        if (call->priv->resource != NULL) {
            g_object_get(G_OBJECT(call->priv->resource), "href", &href, NULL);
            g_return_if_fail(href != NULL);
            g_object_set(object, "href", href, NULL);
            g_free(href);
        }
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}


static void append_params(OvirtResourceRestCall *call, RestParams *params)
{
    GHashTable *params_hash;

    params_hash = rest_params_as_string_hash_table(params);
    if (g_hash_table_size(params_hash) > 0) {
        char *serialized_params;
        char *href;
        char *new_href;

        serialized_params = soup_form_encode_hash(params_hash);
        g_object_get(G_OBJECT(call), "href", &href, NULL);
        new_href = g_strconcat(href, ";", serialized_params, NULL);
        g_object_set(G_OBJECT(call), "href", new_href, NULL);
        g_free(new_href);
        g_free(href);
        g_free(serialized_params);
    }
    g_hash_table_unref(params_hash);
}

static gboolean ovirt_resource_rest_call_class_serialize_params(RestProxyCall *call,
                                                                gchar **content_type,
                                                                gchar **content,
                                                                gsize *content_len,
                                                                GError **error)
{
    OvirtResourceRestCall *self;
    RestParams *params;

    g_return_val_if_fail(OVIRT_IS_RESOURCE_REST_CALL(call), FALSE);
    g_return_val_if_fail(content_type != NULL, FALSE);
    g_return_val_if_fail(content != NULL, FALSE);
    g_return_val_if_fail(content_len != NULL, FALSE);

    self = OVIRT_RESOURCE_REST_CALL(call);

    *content_type = g_strdup("application/xml");
    if (g_strcmp0(rest_proxy_call_get_method(call), "PUT") == 0) {
        g_return_val_if_fail(self->priv->resource != NULL, FALSE);
        *content = ovirt_resource_to_xml(self->priv->resource);
        *content_len = strlen(*content);
    } else {
        *content = NULL;
        *content_len = 0;
    }

    ovirt_resource_add_rest_params(self->priv->resource, call);
    params = rest_proxy_call_get_params(call);
    if (!rest_params_are_strings(params)) {
        g_set_error(error, OVIRT_REST_CALL_ERROR, 0,
                    _("Unexpected parameter type in REST call"));
        return FALSE;
    }
    append_params(self, params);

    return TRUE;
}


static void ovirt_resource_rest_call_dispose(GObject *object)
{
    OvirtResourceRestCall *call = OVIRT_RESOURCE_REST_CALL(object);

    g_clear_object(&call->priv->resource);

    G_OBJECT_CLASS(ovirt_resource_rest_call_parent_class)->dispose(object);
}


static void ovirt_resource_rest_call_class_init(OvirtResourceRestCallClass *klass)
{
    GParamSpec *param_spec;
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = ovirt_resource_rest_call_dispose;
    object_class->get_property = ovirt_resource_rest_call_get_property;
    object_class->set_property = ovirt_resource_rest_call_set_property;
    REST_PROXY_CALL_CLASS(klass)->serialize_params = ovirt_resource_rest_call_class_serialize_params;

    param_spec = g_param_spec_object("resource",
                                     "Resource",
                                     "Resource being manipulated through this REST call",
                                     OVIRT_TYPE_RESOURCE,
                                     G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_RESOURCE, param_spec);
}


static void ovirt_resource_rest_call_init(OvirtResourceRestCall *call)
{
    call->priv = ovirt_resource_rest_call_get_instance_private(call);
}

OvirtResourceRestCall *ovirt_resource_rest_call_new(RestProxy *proxy,
                                                    OvirtResource *resource)
{
    OvirtResourceRestCall *call;

    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), NULL);
    call = OVIRT_RESOURCE_REST_CALL(g_object_new(OVIRT_TYPE_RESOURCE_REST_CALL,
                                                 "proxy", proxy,
                                                 "resource", resource,
                                                 NULL));

    return call;
}
