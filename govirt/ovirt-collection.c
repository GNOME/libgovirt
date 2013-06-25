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
#include "ovirt-collection.h"
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
