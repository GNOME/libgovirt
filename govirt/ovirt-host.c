/*
 * ovirt-host.c: oVirt host handling
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

#include <config.h>
#include "ovirt-enum-types.h"
#include "ovirt-host.h"
#include "govirt-private.h"

struct _OvirtHostPrivate {
    gchar *cluster_href;
    gchar *cluster_id;
    OvirtCollection *vms;
};

G_DEFINE_TYPE_WITH_PRIVATE(OvirtHost, ovirt_host, OVIRT_TYPE_RESOURCE);

enum {
    PROP_0,
    PROP_CLUSTER_HREF,
    PROP_CLUSTER_ID,
};


static const char *get_cluster_href(OvirtHost *host)
{
    if (host->priv->cluster_href == NULL &&
        host->priv->cluster_id != NULL) {
        host->priv->cluster_href = g_strdup_printf("%s/%s",
                                                   "/ovirt-engine/api/clusters",
                                                   host->priv->cluster_id);
    }

    return host->priv->cluster_href;
}

static void ovirt_host_get_property(GObject *object,
                                    guint prop_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
    OvirtHost *host = OVIRT_HOST(object);

    switch (prop_id) {
    case PROP_CLUSTER_HREF:
        g_value_set_string(value, get_cluster_href(host));
        break;
    case PROP_CLUSTER_ID:
        g_value_set_string(value, host->priv->cluster_id);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void ovirt_host_set_property(GObject *object,
                                    guint prop_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
    OvirtHost *host = OVIRT_HOST(object);

    switch (prop_id) {
    case PROP_CLUSTER_HREF:
        g_free(host->priv->cluster_href);
        host->priv->cluster_href = g_value_dup_string(value);
        break;
    case PROP_CLUSTER_ID:
        g_free(host->priv->cluster_id);
        host->priv->cluster_id = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}


static void
ovirt_host_dispose(GObject *obj)
{
    OvirtHost *host = OVIRT_HOST(obj);

    g_clear_pointer(&host->priv->cluster_href, g_free);
    g_clear_pointer(&host->priv->cluster_id, g_free);
    g_clear_object(&host->priv->vms);

    G_OBJECT_CLASS(ovirt_host_parent_class)->dispose(obj);
}


static gboolean ovirt_host_init_from_xml(OvirtResource *resource,
                                         RestXmlNode *node,
                                         GError **error)
{
    OvirtResourceClass *parent_class;
    OvirtXmlElement host_elements[] = {
        { .prop_name = "cluster-href",
          .xml_path = "cluster",
          .xml_attr = "href",
        },
        { .prop_name = "cluster-id",
          .xml_path = "cluster",
          .xml_attr = "id",
        },
        { NULL , },
    };

    if (!ovirt_rest_xml_node_parse(node, G_OBJECT(resource), host_elements))
        return FALSE;

    parent_class = OVIRT_RESOURCE_CLASS(ovirt_host_parent_class);
    return parent_class->init_from_xml(resource, node, error);
}


static void ovirt_host_class_init(OvirtHostClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    OvirtResourceClass *resource_class = OVIRT_RESOURCE_CLASS(klass);
    GParamSpec *param_spec;

    resource_class->init_from_xml = ovirt_host_init_from_xml;
    object_class->dispose = ovirt_host_dispose;
    object_class->get_property = ovirt_host_get_property;
    object_class->set_property = ovirt_host_set_property;

    param_spec = g_param_spec_string("cluster-href",
                                     "Cluster href",
                                     "Cluster href for the Host",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_CLUSTER_HREF,
                                    param_spec);

    param_spec = g_param_spec_string("cluster-id",
                                     "Cluster Id",
                                     "Cluster Id for the Host",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_CLUSTER_ID,
                                    param_spec);
}

static void ovirt_host_init(OvirtHost *host)
{
    host->priv = ovirt_host_get_instance_private(host);
}

G_GNUC_INTERNAL
OvirtHost *ovirt_host_new_from_id(const char *id,
                                  const char *href)
{
    OvirtResource *host = ovirt_resource_new_from_id(OVIRT_TYPE_HOST, id, href);
    return OVIRT_HOST(host);
}

G_GNUC_INTERNAL
OvirtHost *ovirt_host_new_from_xml(RestXmlNode *node,
                                   GError **error)
{
    OvirtResource *host = ovirt_resource_new_from_xml(OVIRT_TYPE_HOST, node, error);
    return OVIRT_HOST(host);
}

OvirtHost *ovirt_host_new(void)
{
    OvirtResource *host = ovirt_resource_new(OVIRT_TYPE_HOST);
    return OVIRT_HOST(host);
}

/**
 * ovirt_host_get_vms:
 * @host: a #OvirtHost
 *
 * Gets a #OvirtCollection representing the list of remote vms from a
 * host object. This method does not initiate any network
 * activity, the remote vm list must be then be fetched using
 * ovirt_collection_fetch() or ovirt_collection_fetch_async().
 *
 * Return value: (transfer none): a #OvirtCollection representing the list
 * of vms associated with @host.
 */
OvirtCollection *ovirt_host_get_vms(OvirtHost *host)
{
    g_return_val_if_fail(OVIRT_IS_HOST(host), NULL);

    if (host->priv->vms == NULL) {
        OvirtCollection *collection;
        collection = ovirt_sub_collection_new_from_resource(OVIRT_RESOURCE(host),
                                                            "vms",
                                                            "vms",
                                                            OVIRT_TYPE_VM,
                                                            "vm");
        host->priv->vms = collection;
    }

    return host->priv->vms;
}


/**
 * ovirt_host_get_cluster:
 * @host: a #OvirtHost
 *
 * Gets a #OvirtCluster representing the cluster the host belongs
 * to. This method does not initiate any network activity, the remote host must
 * be then be fetched using ovirt_resource_refresh() or
 * ovirt_resource_refresh_async().
 *
 * Return value: (transfer full): a #OvirtCluster representing cluster the @host
 * belongs to.
 */
OvirtCluster *ovirt_host_get_cluster(OvirtHost *host)
{
    g_return_val_if_fail(OVIRT_IS_HOST(host), NULL);
    g_return_val_if_fail(host->priv->cluster_id != NULL, NULL);
    return ovirt_cluster_new_from_id(host->priv->cluster_id, get_cluster_href(host));
}
