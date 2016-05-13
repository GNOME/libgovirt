/*
 * ovirt-cdrom.c: oVirt cdrom handling
 *
 * Copyright (C) 2013 Red Hat, Inc.
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
#define GOVIRT_UNSTABLE_API_ABI
#include "ovirt-cdrom.h"
#undef GOVIRT_UNSTABLE_API_ABI
#include "ovirt-proxy.h"
#include "ovirt-proxy-private.h"
#include "ovirt-resource-private.h"
#include "ovirt-resource-rest-call.h"
#include "ovirt-utils.h"

struct _OvirtCdromPrivate {
    char *file;
};

G_DEFINE_TYPE_WITH_PRIVATE(OvirtCdrom, ovirt_cdrom, OVIRT_TYPE_RESOURCE);


enum {
    PROP_0,
    PROP_FILE
};


static void ovirt_cdrom_add_rest_params(G_GNUC_UNUSED OvirtResource *resource,
                                        RestProxyCall *call);

static void ovirt_cdrom_get_property(GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    OvirtCdrom *cdrom = OVIRT_CDROM(object);

    switch (prop_id) {
    case PROP_FILE:
        g_value_set_string(value, cdrom->priv->file);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


static void ovirt_cdrom_set_property(GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    OvirtCdrom *cdrom = OVIRT_CDROM(object);

    switch (prop_id) {
    case PROP_FILE:
        g_free(cdrom->priv->file);
        cdrom->priv->file = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


static void ovirt_cdrom_finalize(GObject *object)
{
    OvirtCdrom *cdrom = OVIRT_CDROM(object);

    g_free(cdrom->priv->file);

    G_OBJECT_CLASS(ovirt_cdrom_parent_class)->finalize(object);
}


static gboolean ovirt_cdrom_init_from_xml(OvirtResource *resource,
                                          RestXmlNode *node,
                                          GError **error)
{
    char *name;
    OvirtResourceClass *parent_class;
    OvirtXmlElement cdrom_elements[] = {
        { .prop_name = "file",
          .xml_path = "file",
          .xml_attr = "id",
        },
        { NULL , },
    };

    parent_class = OVIRT_RESOURCE_CLASS(ovirt_cdrom_parent_class);

    if (!parent_class->init_from_xml(resource, node, error))
        return FALSE;

    ovirt_rest_xml_node_parse(node, G_OBJECT(resource), cdrom_elements);

    g_object_get(G_OBJECT(resource), "name", &name, NULL);
    if (name == NULL) {
        /* Build up fake name as ovirt_collection_refresh_from_xml()
         * expects it to be set (it uses it as a hash table key), but
         * it's not set in oVirt XML as of 3.2. There can only be
         * one cdrom node in an oVirt VM so this should be good
         * enough for now
         */
        g_debug("Setting fake 'name' for cdrom resource");
        g_object_set(G_OBJECT(resource), "name", "cdrom0", NULL);
    }

    g_free(name);
    return TRUE;
}


static char *ovirt_cdrom_to_xml(OvirtResource *resource)
{
    OvirtCdrom *cdrom;
    const char *file;

    g_return_val_if_fail(OVIRT_IS_CDROM(resource), NULL);
    cdrom = OVIRT_CDROM(resource);
    file = cdrom->priv->file;
    if (file == NULL) {
        file = "";
    }

    return g_strdup_printf("<cdrom>\n\t<file id=\"%s\"/>\n</cdrom>", file);
}


static void ovirt_cdrom_class_init(OvirtCdromClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    OvirtResourceClass *resource_class = OVIRT_RESOURCE_CLASS(klass);
    GParamSpec *param_spec;

    resource_class->init_from_xml = ovirt_cdrom_init_from_xml;
    resource_class->to_xml = ovirt_cdrom_to_xml;
    resource_class->add_rest_params = ovirt_cdrom_add_rest_params;
    object_class->finalize = ovirt_cdrom_finalize;
    object_class->get_property = ovirt_cdrom_get_property;
    object_class->set_property = ovirt_cdrom_set_property;

    param_spec = g_param_spec_string("file",
                                     "File",
                                     "Name of the CD image",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_FILE,
                                    param_spec);
}


static void ovirt_cdrom_init(OvirtCdrom *cdrom)
{
    cdrom->priv = ovirt_cdrom_get_instance_private(cdrom);
}


gboolean ovirt_cdrom_update(OvirtCdrom *cdrom,
                            gboolean current,
                            OvirtProxy *proxy,
                            GError **error)
{
    OvirtRestCall *call;
    RestXmlNode *root;

    call = OVIRT_REST_CALL(ovirt_resource_rest_call_new(REST_PROXY(proxy),
                                                        OVIRT_RESOURCE(cdrom)));
    rest_proxy_call_set_method(REST_PROXY_CALL(call), "PUT");

    if (current) {
        rest_proxy_call_add_param(REST_PROXY_CALL(call), "current", NULL);
    }
    root = ovirt_resource_rest_call_sync(call, error);

    g_object_unref(G_OBJECT(call));

    if (root != NULL) {
        rest_xml_node_unref(root);
        return TRUE;
    }

    return FALSE;
}


void ovirt_cdrom_update_async(OvirtCdrom *cdrom,
                              gboolean current,
                              OvirtProxy *proxy,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{

    GTask *task;
    OvirtResourceRestCall *call;

    g_return_if_fail(OVIRT_IS_CDROM(cdrom));
    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    task = g_task_new(G_OBJECT(cdrom),
                      cancellable,
                      callback,
                      user_data);

    call = ovirt_resource_rest_call_new(REST_PROXY(proxy), OVIRT_RESOURCE(cdrom));
    rest_proxy_call_set_method(REST_PROXY_CALL(call), "PUT");
    if (current) {
        rest_proxy_call_add_param(REST_PROXY_CALL(call), "current", NULL);
    }
    ovirt_rest_call_async(OVIRT_REST_CALL(call), task, cancellable,
                          NULL, NULL, NULL);
    g_object_unref(G_OBJECT(call));
}


gboolean ovirt_cdrom_update_finish(OvirtCdrom *cdrom,
                                   GAsyncResult *result,
                                   GError **err)
{
    g_return_val_if_fail(OVIRT_IS_CDROM(cdrom), FALSE);
    g_return_val_if_fail(g_task_is_valid(G_TASK(result), cdrom), FALSE);
    g_return_val_if_fail((err == NULL) || (*err == NULL), FALSE);

    return ovirt_rest_call_finish(result, err);
}


static void ovirt_cdrom_add_rest_params(G_GNUC_UNUSED OvirtResource *resource,
                                        RestProxyCall *call)
{
    rest_proxy_call_add_param(REST_PROXY_CALL(call), "current", NULL);
}
