/*
 * ovirt-collection.c: generic oVirt collection handling
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

#include <string.h>

#include <glib/gi18n-lib.h>

#include "ovirt-collection.h"
#include "ovirt-error.h"
#include "govirt-private.h"

struct _OvirtCollectionPrivate {
    char *href;
    char *collection_xml_name;
    GType resource_type;
    char *resource_xml_name;

    GHashTable *resources;
};

G_DEFINE_TYPE_WITH_PRIVATE(OvirtCollection, ovirt_collection, G_TYPE_OBJECT);


enum {
    PROP_0,
    PROP_HREF,
    PROP_RESOURCE_TYPE,
    PROP_COLLECTION_XML_NAME,
    PROP_RESOURCE_XML_NAME,
    PROP_RESOURCES,
};


static void ovirt_collection_get_property(GObject *object,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec)
{
    OvirtCollection *collection = OVIRT_COLLECTION(object);

    switch (prop_id) {
    case PROP_HREF:
        g_value_set_string(value, collection->priv->href);
        break;
    case PROP_RESOURCE_TYPE:
        g_value_set_gtype(value, collection->priv->resource_type);
        break;
    case PROP_RESOURCES:
        g_value_set_boxed(value, collection->priv->resources);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


static void ovirt_collection_set_property(GObject *object,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
    OvirtCollection *collection = OVIRT_COLLECTION(object);

    switch (prop_id) {
    case PROP_HREF:
        collection->priv->href = g_value_dup_string(value);
        break;
    case PROP_RESOURCE_TYPE:
        collection->priv->resource_type = g_value_get_gtype(value);
        break;
    case PROP_COLLECTION_XML_NAME:
        collection->priv->collection_xml_name = g_value_dup_string(value);
        break;
    case PROP_RESOURCE_XML_NAME:
        collection->priv->resource_xml_name = g_value_dup_string(value);
        break;
    case PROP_RESOURCES:
        ovirt_collection_set_resources(collection, g_value_get_boxed(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


static void ovirt_collection_finalize(GObject *object)
{
    OvirtCollection *collection = OVIRT_COLLECTION(object);

    g_clear_pointer(&collection->priv->resources, g_hash_table_unref);
    g_free(collection->priv->href);
    g_free(collection->priv->collection_xml_name);
    g_free(collection->priv->resource_xml_name);

    G_OBJECT_CLASS(ovirt_collection_parent_class)->finalize(object);
}


static void ovirt_collection_class_init(OvirtCollectionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GParamSpec *param_spec;

    object_class->finalize = ovirt_collection_finalize;
    object_class->get_property = ovirt_collection_get_property;
    object_class->set_property = ovirt_collection_set_property;

    param_spec = g_param_spec_string("href",
                                     "Collection href",
                                     "relative href for the collection",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_HREF,
                                    param_spec);

    param_spec = g_param_spec_gtype("resource-type",
                                    "Resource Type",
                                    "Type of resources held by this collection",
                                    OVIRT_TYPE_RESOURCE,
                                    G_PARAM_READWRITE |
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_RESOURCE_TYPE,
                                    param_spec);

    param_spec = g_param_spec_string("collection-xml-name",
                                     "Collection XML Name",
                                     "Name of the XML element for the collection",
                                     NULL,
                                     G_PARAM_WRITABLE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_COLLECTION_XML_NAME,
                                    param_spec);

    param_spec = g_param_spec_string("resource-xml-name",
                                     "Resource XML Name",
                                     "Name of the XML element for the resources stored in that collection",
                                     NULL,
                                     G_PARAM_WRITABLE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_RESOURCE_XML_NAME,
                                    param_spec);

    param_spec = g_param_spec_boxed("resources",
                                     "Resources",
                                     "Hash table containing the resources contained in this collection",
                                     G_TYPE_HASH_TABLE,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_RESOURCES,
                                    param_spec);
}


static void ovirt_collection_init(OvirtCollection *collection)
{
    collection->priv = ovirt_collection_get_instance_private(collection);
}

/**
 * ovirt_collection_get_resources:
 *
 * Returns: (element-type utf8 OvirtResource) (transfer none):
 */
GHashTable *ovirt_collection_get_resources(OvirtCollection *collection)
{
    g_return_val_if_fail(OVIRT_IS_COLLECTION(collection), NULL);

    return collection->priv->resources;
}

/**
 * ovirt_collection_set_resources:
 * @collection:
 * @resources: (element-type utf8 OvirtResource) (transfer none):
 */
void ovirt_collection_set_resources(OvirtCollection *collection, GHashTable *resources)
{
    g_return_if_fail(OVIRT_IS_COLLECTION(collection));

    g_clear_pointer(&collection->priv->resources, g_hash_table_unref);

    if (resources != NULL) {
        collection->priv->resources = g_hash_table_ref(resources);
    }

    g_object_notify(G_OBJECT(collection), "resources");
}


static OvirtResource *
ovirt_collection_new_resource_from_xml(OvirtCollection *collection,
                                       RestXmlNode *node,
                                       GError **error)
{
    return ovirt_resource_new_from_xml(collection->priv->resource_type, node, error);
}


static gboolean
ovirt_collection_refresh_from_xml(OvirtCollection *collection,
                                  RestXmlNode *root_node,
                                  GError **error)
{
    RestXmlNode *resources_node;
    RestXmlNode *node;
    GHashTable *resources;
    const char *resource_key;

    g_return_val_if_fail(OVIRT_IS_COLLECTION(collection), FALSE);
    g_return_val_if_fail(root_node != NULL, FALSE);
    g_return_val_if_fail((error == NULL) || (*error == NULL), FALSE);

    if (strcmp(root_node->name, collection->priv->collection_xml_name) != 0) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    _("Got '%s' node, expected '%s'"), root_node->name,
                    collection->priv->collection_xml_name);
        return FALSE;
    }

    resource_key = g_intern_string(collection->priv->resource_xml_name);
    resources = g_hash_table_new_full(g_str_hash, g_str_equal,
                                      g_free, (GDestroyNotify)g_object_unref);
    resources_node = g_hash_table_lookup(root_node->children, resource_key);
    for (node = resources_node; node != NULL; node = node->next) {
        OvirtResource *resource;
        gchar *name;

        resource = ovirt_collection_new_resource_from_xml(collection, node, error);
        if (resource == NULL) {
            if ((error != NULL) && (*error != NULL)) {
                g_message("Failed to parse '%s' node: %s",
                          collection->priv->resource_xml_name, (*error)->message);
            } else {
                g_message("Failed to parse '%s' node",
                          collection->priv->resource_xml_name);
            }
            g_clear_error(error);
            continue;
        }
        g_object_get(G_OBJECT(resource), "name", &name, NULL);
        if (name == NULL) {
            g_message("'%s' resource had no name in its XML description",
                      collection->priv->resource_xml_name);
            g_object_unref(G_OBJECT(resource));
            continue;
        }
        if (g_hash_table_lookup(resources, name) != NULL) {
            g_message("'%s' resource with the same name ('%s') already exists",
                      collection->priv->resource_xml_name, name);
            g_object_unref(G_OBJECT(resource));
            g_free(name);
            continue;
        }
        g_hash_table_insert(resources, name, resource);
    }

    ovirt_collection_set_resources(OVIRT_COLLECTION(collection), resources);
    g_hash_table_unref(resources);

    return TRUE;
}


OvirtCollection *ovirt_collection_new(const char *href,
                                      const char *collection_name,
                                      GType resource_type,
                                      const char *resource_name)
{
    OvirtCollection *self;

    self = OVIRT_COLLECTION(g_object_new(OVIRT_TYPE_COLLECTION,
                                         "href", href,
                                         "collection-xml-name", collection_name,
                                         "resource-type", resource_type,
                                         "resource-xml-name", resource_name,
                                         NULL));

    return self;
}


OvirtCollection *ovirt_collection_new_from_xml(RestXmlNode *root_node,
                                               GType collection_type,
                                               const char *collection_name,
                                               GType resource_type,
                                               const char *resource_name,
                                               GError **error)
{
    OvirtCollection *self;

    self = OVIRT_COLLECTION(g_object_new(collection_type,
                                         "collection-xml-name", collection_name,
                                         "resource-type", resource_type,
                                         "resource-xml-name", resource_name,
                                         NULL));
    ovirt_collection_refresh_from_xml(self, root_node, error);

    return self;
}


OvirtCollection *ovirt_sub_collection_new_from_resource(OvirtResource *resource,
                                                        const char *href,
                                                        const char *collection_name,
                                                        GType resource_type,
                                                        const char *resource_name)
{
    const char *link = ovirt_resource_get_sub_collection(resource, href);

    if (link == NULL)
        return NULL;

    return ovirt_collection_new(link, collection_name, resource_type, resource_name);
}

OvirtCollection *ovirt_sub_collection_new_from_resource_search(OvirtResource *resource,
                                                               const char *href,
                                                               const char *collection_name,
                                                               GType resource_type,
                                                               const char *resource_name,
                                                               const char *query)
{
    const char *link;
    char *substr;
    gchar *link_query, *escaped_query;
    OvirtCollection *collection;

    link = ovirt_resource_get_sub_collection(resource, href);
    if (link == NULL)
        return NULL;

    /* link is will be something like "/ovirt-engine/api/vms?search={query}", so
     * we need to strip out {query} substring.
     */
    substr = g_strrstr(link, "{query}");
    if (substr != NULL)
        *substr = '\0';

    escaped_query = g_uri_escape_string(query, NULL, FALSE);
    link_query = g_strconcat(link, escaped_query, NULL);
    collection = ovirt_collection_new(link_query, collection_name, resource_type, resource_name);
    g_free(escaped_query);
    g_free(link_query);

    return collection;
}

/**
 * ovirt_collection_fetch:
 * @collection: a #OvirtCollection
 * @proxy: a #OvirtProxy
 * @error: #GError to set on error, or NULL
 */
gboolean ovirt_collection_fetch(OvirtCollection *collection,
                                OvirtProxy *proxy,
                                GError **error)
{
    RestXmlNode *xml;

    g_return_val_if_fail(OVIRT_IS_COLLECTION(collection), FALSE);
    g_return_val_if_fail(OVIRT_IS_PROXY(proxy), FALSE);
    g_return_val_if_fail(collection->priv->href != NULL, FALSE);

    xml = ovirt_proxy_get_collection_xml(proxy, collection->priv->href, NULL);
    if (xml == NULL)
        return FALSE;

    ovirt_collection_refresh_from_xml(collection, xml, error);

    rest_xml_node_unref(xml);

    return TRUE;
}


static gboolean ovirt_collection_fetch_async_cb(OvirtProxy* proxy,
                                                RestXmlNode *root_node,
                                                gpointer user_data,
                                                GError **error)
{
    OvirtCollection *collection = OVIRT_COLLECTION(user_data);

    g_return_val_if_fail(OVIRT_IS_COLLECTION(user_data), FALSE);

    return ovirt_collection_refresh_from_xml(collection, root_node, error);
}


/**
 * ovirt_collection_fetch_async:
 * @collection: a #OvirtCollection
 * @proxy: a #OvirtProxy
 * @callback: (scope async): completion callback
 * @user_data: (closure): opaque data for callback
 */
void ovirt_collection_fetch_async(OvirtCollection *collection,
                                  OvirtProxy *proxy,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    GTask *task;

    g_return_if_fail(OVIRT_IS_COLLECTION(collection));
    g_return_if_fail(OVIRT_IS_PROXY(proxy));
    g_return_if_fail((cancellable == NULL) || G_IS_CANCELLABLE(cancellable));

    task = g_task_new(G_OBJECT(collection),
                      cancellable,
                      callback,
                      user_data);
    ovirt_proxy_get_collection_xml_async(proxy, collection->priv->href,
                                         task, cancellable,
                                         ovirt_collection_fetch_async_cb,
                                         collection, NULL);
}


/**
 * ovirt_collection_fetch_finish:
 * @collection: a #OvirtCollection
 * @result: async method result
 *
 * Return value: TRUE if successful, FALSE otherwise, with @error set.
 */
gboolean ovirt_collection_fetch_finish(OvirtCollection *collection,
                                       GAsyncResult *result,
                                       GError **err)
{
    g_return_val_if_fail(OVIRT_IS_COLLECTION(collection), FALSE);
    g_return_val_if_fail(g_task_is_valid(G_TASK(result), collection),
                         FALSE);

    return g_task_propagate_boolean(G_TASK(result), err);
}


/**
 * ovirt_collection_lookup_resource:
 * @collection: a #OvirtCollection
 * @name: name of the resource to lookup
 *
 * Looks up a resource in @collection whose name is @name. If it cannot be
 * found, NULL is returned. This method does not initiate any network
 * activity, the remote collection content must have been fetched with
 * ovirt_collection_fetch() or ovirt_collection_fetch_async() before
 * calling this function.
 *
 * Return value: (transfer full): a #OvirtResource whose name is @name
 * or NULL
 */
OvirtResource *ovirt_collection_lookup_resource(OvirtCollection *collection,
                                                const char *name)
{
    OvirtResource *resource;

    g_return_val_if_fail(OVIRT_IS_COLLECTION(collection), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    if (collection->priv->resources == NULL) {
        return NULL;
    }

    resource = g_hash_table_lookup(collection->priv->resources, name);

    if (resource == NULL) {
        return NULL;
    }

    return g_object_ref(resource);
}
