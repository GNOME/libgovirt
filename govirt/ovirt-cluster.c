/*
 * ovirt-cluster.c: oVirt cluster handling
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
#include "ovirt-cluster.h"
#include "govirt-private.h"

struct _OvirtClusterPrivate {
    gchar *data_center_href;
    gchar *data_center_id;
    OvirtCollection *hosts;
};

G_DEFINE_TYPE_WITH_PRIVATE(OvirtCluster, ovirt_cluster, OVIRT_TYPE_RESOURCE);

enum {
    PROP_0,
    PROP_DATA_CENTER_HREF,
    PROP_DATA_CENTER_ID,
};

static const char *get_data_center_href(OvirtCluster *cluster)
{
    if (cluster->priv->data_center_href == NULL &&
        cluster->priv->data_center_id != NULL) {
        cluster->priv->data_center_href = g_strdup_printf("%s/%s",
                                                          "/ovirt-engine/api/data_centers",
                                                          cluster->priv->data_center_id);
    }

    return cluster->priv->data_center_href;
}

static void ovirt_cluster_get_property(GObject *object,
                                       guint prop_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
    OvirtCluster *cluster = OVIRT_CLUSTER(object);

    switch (prop_id) {
    case PROP_DATA_CENTER_HREF:
        g_value_set_string(value, get_data_center_href(cluster));
        break;
    case PROP_DATA_CENTER_ID:
        g_value_set_string(value, cluster->priv->data_center_id);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void ovirt_cluster_set_property(GObject *object,
                                       guint prop_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
    OvirtCluster *cluster = OVIRT_CLUSTER(object);

    switch (prop_id) {
    case PROP_DATA_CENTER_HREF:
        g_free(cluster->priv->data_center_href);
        cluster->priv->data_center_href = g_value_dup_string(value);
        break;
    case PROP_DATA_CENTER_ID:
        g_free(cluster->priv->data_center_id);
        cluster->priv->data_center_id = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}


static void
ovirt_cluster_dispose(GObject *obj)
{
    OvirtCluster *cluster = OVIRT_CLUSTER(obj);

    g_clear_pointer(&cluster->priv->data_center_href, g_free);
    g_clear_pointer(&cluster->priv->data_center_id, g_free);
    g_clear_object(&cluster->priv->hosts);

    G_OBJECT_CLASS(ovirt_cluster_parent_class)->dispose(obj);
}


static gboolean ovirt_cluster_init_from_xml(OvirtResource *resource,
                                            RestXmlNode *node,
                                            GError **error)
{
    OvirtResourceClass *parent_class;
    OvirtXmlElement cluster_elements[] = {
        { .prop_name = "data-center-href",
          .xml_path = "data_center",
          .xml_attr = "href",
        },
        { .prop_name = "data-center-id",
          .xml_path = "data_center",
          .xml_attr = "id",
        },
        { NULL , },
    };

    if (!ovirt_rest_xml_node_parse(node, G_OBJECT(resource), cluster_elements))
        return FALSE;

    parent_class = OVIRT_RESOURCE_CLASS(ovirt_cluster_parent_class);
    return parent_class->init_from_xml(resource, node, error);
}


static void ovirt_cluster_class_init(OvirtClusterClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    OvirtResourceClass *resource_class = OVIRT_RESOURCE_CLASS(klass);
    GParamSpec *param_spec;

    resource_class->init_from_xml = ovirt_cluster_init_from_xml;
    object_class->dispose = ovirt_cluster_dispose;
    object_class->get_property = ovirt_cluster_get_property;
    object_class->set_property = ovirt_cluster_set_property;

    param_spec = g_param_spec_string("data-center-href",
                                     "Data Center href",
                                     "Data Center href for the Cluster",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_DATA_CENTER_HREF,
                                    param_spec);

    param_spec = g_param_spec_string("data-center-id",
                                     "Data Center Id",
                                     "Data Center Id for the Cluster",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_DATA_CENTER_ID,
                                    param_spec);
}

static void ovirt_cluster_init(OvirtCluster *cluster)
{
    cluster->priv = ovirt_cluster_get_instance_private(cluster);
}

G_GNUC_INTERNAL
OvirtCluster *ovirt_cluster_new_from_id(const char *id,
                                        const char *href)
{
    OvirtResource *cluster = ovirt_resource_new_from_id(OVIRT_TYPE_CLUSTER, id, href);
    return OVIRT_CLUSTER(cluster);
}

G_GNUC_INTERNAL
OvirtCluster *ovirt_cluster_new_from_xml(RestXmlNode *node,
                                         GError **error)
{
    OvirtResource *cluster = ovirt_resource_new_from_xml(OVIRT_TYPE_CLUSTER, node, error);
    return OVIRT_CLUSTER(cluster);
}

OvirtCluster *ovirt_cluster_new(void)
{
    OvirtResource *cluster = ovirt_resource_new(OVIRT_TYPE_CLUSTER);
    return OVIRT_CLUSTER(cluster);
}

/**
 * ovirt_cluster_get_hosts:
 * @cluster: a #OvirtCluster
 *
 * Gets a #OvirtCollection representing the list of remote hosts from a
 * cluster object. This method does not initiate any network
 * activity, the remote host list must be then be fetched using
 * ovirt_collection_fetch() or ovirt_collection_fetch_async().
 *
 * Return value: (transfer none): a #OvirtCollection representing the list
 * of hosts associated with @cluster.
 */
OvirtCollection *ovirt_cluster_get_hosts(OvirtCluster *cluster)
{
    g_return_val_if_fail(OVIRT_IS_CLUSTER(cluster), NULL);

    if (cluster->priv->hosts == NULL) {
        OvirtCollection *collection;
        collection = ovirt_sub_collection_new_from_resource(OVIRT_RESOURCE(cluster),
                                                            "hosts",
                                                            "hosts",
                                                            OVIRT_TYPE_HOST,
                                                            "host");
        cluster->priv->hosts = collection;
    }

    return cluster->priv->hosts;
}


/**
 * ovirt_cluster_get_data_center:
 * @cluster: a #OvirtCluster
 *
 * Gets a #OvirtCluster representing the data center the cluster belongs
 * to. This method does not initiate any network activity, the remote data center must
 * be then be fetched using ovirt_resource_refresh() or
 * ovirt_resource_refresh_async().
 *
 * Return value: (transfer full): a #OvirtDataCenter representing data center
 * the @host belongs to.
 */
OvirtDataCenter *ovirt_cluster_get_data_center(OvirtCluster *cluster)
{
    g_return_val_if_fail(OVIRT_IS_CLUSTER(cluster), NULL);
    g_return_val_if_fail(cluster->priv->data_center_id != NULL, NULL);
    return ovirt_data_center_new_from_id(cluster->priv->data_center_id, get_data_center_href(cluster));
}
