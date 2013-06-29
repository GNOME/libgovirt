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

#include "ovirt-collection.h"
#include "ovirt-error.h"
#include "govirt-private.h"

#define OVIRT_COLLECTION_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), OVIRT_TYPE_COLLECTION, OvirtCollectionPrivate))

struct _OvirtCollectionPrivate {
    GHashTable *resources;
};

G_DEFINE_TYPE(OvirtCollection, ovirt_collection, G_TYPE_OBJECT);

static void ovirt_collection_finalize(GObject *object)
{
    OvirtCollection *collection = OVIRT_COLLECTION(object);

    if (collection->priv->resources != NULL) {
        g_hash_table_unref(collection->priv->resources);
    }
    G_OBJECT_CLASS(ovirt_collection_parent_class)->finalize(object);
}

static void ovirt_collection_class_init(OvirtCollectionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(OvirtCollectionPrivate));

    object_class->finalize = ovirt_collection_finalize;
}

static void ovirt_collection_init(OvirtCollection *collection)
{
    collection->priv = OVIRT_COLLECTION_GET_PRIVATE(collection);
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

    if (collection->priv->resources != NULL) {
        g_hash_table_unref(collection->priv->resources);
    }
    if (resources != NULL) {
        collection->priv->resources = g_hash_table_ref(resources);
    } else {
        collection->priv->resources = NULL;
    }
}
static OvirtResource *ovirt_collection_new_resource_from_xml(GType resource_type,
                                                             RestXmlNode *node,
                                                             GError **error)
{
    return OVIRT_RESOURCE(g_initable_new(resource_type, NULL, error,
                                         "xml-node", node , NULL));
}

OvirtCollection *ovirt_collection_new_from_xml(RestXmlNode *root_node,
                                               GType collection_type,
                                               const char *collection_name,
                                               GType resource_type,
                                               const char *resource_name,
                                               GError **error)
{
    OvirtCollection *self;
    RestXmlNode *resources_node;
    RestXmlNode *node;
    GHashTable *resources;
    const char *resource_key = g_intern_string(resource_name);

    if (strcmp(root_node->name, collection_name) != 0) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    "Got '%s' node, expected '%s'", root_node->name, collection_name);
        return NULL;
    }

    resources = g_hash_table_new_full(g_str_hash, g_str_equal,
                                      g_free, (GDestroyNotify)g_object_unref);
    resources_node = g_hash_table_lookup(root_node->children, resource_key);
    for (node = resources_node; node != NULL; node = node->next) {
        OvirtResource *resource;
        gchar *name;

        resource = ovirt_collection_new_resource_from_xml(resource_type, node, error);
        if (resource == NULL) {
            if ((error != NULL) && (*error != NULL)) {
                g_message("Failed to parse '%s' node: %s",
                          resource_name, (*error)->message);
            } else {
                g_message("Failed to parse '%s' node", resource_name);
            }
            g_clear_error(error);
            continue;
        }
        g_object_get(G_OBJECT(resource), "name", &name, NULL);
        if (name == NULL) {
            g_message("'%s' resource had no name in its XML description",
                      resource_name);
            g_object_unref(G_OBJECT(resource));
            continue;
        }
        if (g_hash_table_lookup(resources, name) != NULL) {
            g_message("'%s' resource with the same name ('%s') already exists",
                      resource_name, name);
            g_object_unref(resources);
            continue;
        }
        g_hash_table_insert(resources, name, resource);
    }

    self = OVIRT_COLLECTION(g_object_new(collection_type, NULL));
    ovirt_collection_set_resources(OVIRT_COLLECTION(self), resources);
    g_hash_table_unref(resources);

    return self;
}
