/*
 * ovirt-resource.c: generic oVirt resource handling
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

#include <rest/rest-xml-node.h>

#include "govirt-private.h"
#include "ovirt-error.h"
#include "ovirt-resource.h"

#define OVIRT_RESOURCE_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), OVIRT_TYPE_RESOURCE, OvirtResourcePrivate))

struct _OvirtResourcePrivate {
    char *guid;
    char *href;
    char *name;
    char *description;

    GHashTable *actions;

    RestXmlNode *xml;
};

static void ovirt_resource_initable_iface_init(GInitableIface *iface);
static gboolean ovirt_resource_init_from_xml_real(OvirtResource *resource,
                                                  RestXmlNode *node,
                                                  GError **error);
static gboolean ovirt_resource_init_from_xml(OvirtResource *resource,
                                             RestXmlNode *node,
                                             GError **error);

G_DEFINE_TYPE_WITH_CODE(OvirtResource, ovirt_resource, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE,
                                              ovirt_resource_initable_iface_init));


enum {
    PROP_0,
    PROP_DESCRIPTION,
    PROP_GUID,
    PROP_HREF,
    PROP_NAME,
    PROP_XML_NODE,
};

static void ovirt_resource_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
    OvirtResource *resource = OVIRT_RESOURCE(object);

    switch (prop_id) {
    case PROP_GUID:
        g_value_set_string(value, resource->priv->guid);
        break;
    case PROP_HREF:
        g_value_set_string(value, resource->priv->href);
        break;
    case PROP_NAME:
        g_value_set_string(value, resource->priv->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string(value, resource->priv->description);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void ovirt_resource_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    OvirtResource *resource = OVIRT_RESOURCE(object);

    switch (prop_id) {
    case PROP_GUID:
        g_free(resource->priv->guid);
        resource->priv->guid = g_value_dup_string(value);
        break;
    case PROP_HREF:
        g_free(resource->priv->href);
        resource->priv->href = g_value_dup_string(value);
        break;
    case PROP_NAME:
        g_free(resource->priv->name);
        resource->priv->name = g_value_dup_string(value);
        break;
    case PROP_DESCRIPTION:
        g_free(resource->priv->description);
        resource->priv->description = g_value_dup_string(value);
        break;
    case PROP_XML_NODE: {
        if (resource->priv->xml != NULL) {
            g_boxed_free(REST_TYPE_XML_NODE, resource->priv->xml);
        }
        resource->priv->xml = g_value_dup_boxed(value);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void ovirt_resource_dispose(GObject *object)
{
    OvirtResource *resource = OVIRT_RESOURCE(object);

    if (resource->priv->actions != NULL) {
        g_hash_table_unref(resource->priv->actions);
        resource->priv->actions = NULL;
    }
    if (resource->priv->xml != NULL) {
        g_boxed_free(REST_TYPE_XML_NODE, resource->priv->xml);
        resource->priv->xml = NULL;
    }

    G_OBJECT_CLASS(ovirt_resource_parent_class)->dispose(object);
}

static void ovirt_resource_finalize(GObject *object)
{
    OvirtResource *resource = OVIRT_RESOURCE(object);

    g_free(resource->priv->description);
    g_free(resource->priv->guid);
    g_free(resource->priv->href);
    g_free(resource->priv->name);

    G_OBJECT_CLASS(ovirt_resource_parent_class)->finalize(object);
}

static gboolean ovirt_resource_initable_init(GInitable *initable,
                                             GCancellable *cancellable,
                                             GError  **error)
{
    OvirtResource *resource;

    g_return_val_if_fail (OVIRT_IS_RESOURCE(initable), FALSE);


    if (cancellable != NULL) {
        g_set_error_literal (error, OVIRT_ERROR, OVIRT_ERROR_NOT_SUPPORTED,
                             "Cancellable initialization not supported");
        return FALSE;
    }

    resource = OVIRT_RESOURCE(initable);

    if (resource->priv->xml == NULL) {
        return TRUE;
    }

    return ovirt_resource_init_from_xml(resource, resource->priv->xml, error);
}

static void ovirt_resource_initable_iface_init(GInitableIface *iface)
{
      iface->init = ovirt_resource_initable_init;
}

static void ovirt_resource_class_init(OvirtResourceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(OvirtResourcePrivate));

    klass->init_from_xml = ovirt_resource_init_from_xml_real;
    object_class->dispose = ovirt_resource_dispose;
    object_class->finalize = ovirt_resource_finalize;
    object_class->get_property = ovirt_resource_get_property;
    object_class->set_property = ovirt_resource_set_property;

    g_object_class_install_property(object_class,
                                    PROP_DESCRIPTION,
                                    g_param_spec_string("description",
                                                        "Name",
                                                        "Resource Description",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_GUID,
                                    g_param_spec_string("guid",
                                                        "GUID",
                                                        "Resource GUID",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_HREF,
                                    g_param_spec_string("href",
                                                        "Href",
                                                        "Resource Href",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_NAME,
                                    g_param_spec_string("name",
                                                        "Name",
                                                        "Resource Name",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_XML_NODE,
                                    g_param_spec_boxed("xml-node",
                                                       "Librest XML Node",
                                                       "XML data to fill this resource with",
                                                       REST_TYPE_XML_NODE,
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_STRINGS));
}

static void ovirt_resource_init(OvirtResource *resource)
{
    resource->priv = OVIRT_RESOURCE_GET_PRIVATE(resource);
    resource->priv->actions = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                    g_free, g_free);
}

static void
ovirt_resource_add_action(OvirtResource *resource,
                          const char *action,
                          const char *url)
{
    g_return_if_fail(OVIRT_IS_RESOURCE(resource));
    g_return_if_fail(action != NULL);
    g_return_if_fail(url != NULL);

    g_hash_table_insert(resource->priv->actions,
                        g_strdup(action),
                        g_strdup(url));
}

G_GNUC_INTERNAL const char *
ovirt_resource_get_action(OvirtResource *resource, const char *action)
{
    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), NULL);
    g_return_val_if_fail(resource->priv->actions != NULL, NULL);

    return g_hash_table_lookup(resource->priv->actions, action);
}

static gboolean
ovirt_resource_set_actions_from_xml(OvirtResource *resource, RestXmlNode *node)
{
    RestXmlNode *link;
    RestXmlNode *rest_actions;
    const char *link_key = g_intern_string("link");

    rest_actions = rest_xml_node_find(node, "actions");
    if (rest_actions == NULL) {
        return FALSE;
    }

    link = g_hash_table_lookup(rest_actions->children, link_key);
    if (link == NULL)
        return FALSE;

    for (; link != NULL; link = link->next) {
        const char *link_name;
        const char *href;

        g_warn_if_fail(link != NULL);
        g_warn_if_fail(link->name != NULL);
        g_warn_if_fail(strcmp(link->name, "link") == 0);

        link_name = rest_xml_node_get_attr(link, "rel");
        href = rest_xml_node_get_attr(link, "href");

        if ((link_name != NULL) && (href != NULL)) {
            ovirt_resource_add_action(resource, link_name, href);
        }
    }

    return TRUE;
}

static gboolean
ovirt_resource_set_name_from_xml(OvirtResource *resource, RestXmlNode *node)
{
    RestXmlNode *name_node;

    name_node = rest_xml_node_find(node, "name");
    if (name_node != NULL) {
        g_return_val_if_fail(name_node->content != NULL, FALSE);
        g_object_set(G_OBJECT(resource), "name", name_node->content, NULL);
        return TRUE;
    }

    return FALSE;
}

static gboolean
ovirt_resource_set_description_from_xml(OvirtResource *resource,
                                        RestXmlNode *node)
{
    RestXmlNode *desc_node;

    desc_node = rest_xml_node_find(node, "description");
    if (desc_node != NULL) {
        g_return_val_if_fail(desc_node->content != NULL, FALSE);
        g_object_set(G_OBJECT(resource),
                     "description", desc_node->content,
                     NULL);
        return TRUE;
    }

    return FALSE;
}

static gboolean ovirt_resource_init_from_xml_real(OvirtResource *resource,
                                                  RestXmlNode *node,
                                                  GError **error)
{
    const char *guid;
    const char *href;

    g_return_val_if_fail(resource->priv->xml != NULL, FALSE);

    guid = rest_xml_node_get_attr(node, "id");
    if (guid == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    "missing mandatory 'id' attribute");
        return FALSE;
    }

    href = rest_xml_node_get_attr(node, "href");
    if (href == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    "missing mandatory 'href' attribute");
        return FALSE;
    }

    g_object_set(G_OBJECT(resource), "guid", guid, "href", href, NULL);

    ovirt_resource_set_name_from_xml(resource, node);
    ovirt_resource_set_description_from_xml(resource, node);
    ovirt_resource_set_actions_from_xml(resource, node);

    return TRUE;
}

static gboolean ovirt_resource_init_from_xml(OvirtResource *resource,
                                             RestXmlNode *node,
                                             GError **error)
{
    OvirtResourceClass *klass;

    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), FALSE);

    klass = OVIRT_RESOURCE_GET_CLASS(resource);
    g_return_val_if_fail(klass->init_from_xml != NULL, FALSE);

    return klass->init_from_xml(resource, node, error);
}
