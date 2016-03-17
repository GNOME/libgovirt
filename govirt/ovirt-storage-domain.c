/*
 * ovirt-storage-domain.c: oVirt storage domain handling
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
#include "ovirt-enum-types.h"
#include "ovirt-storage-domain.h"
#include "govirt-private.h"

#define OVIRT_STORAGE_DOMAIN_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), OVIRT_TYPE_STORAGE_DOMAIN, OvirtStorageDomainPrivate))

struct _OvirtStorageDomainPrivate {
    OvirtCollection *files;

    OvirtStorageDomainType type;
    gboolean is_master;
    guint64 available;
    guint64 used;
    guint64 committed;
    OvirtStorageDomainFormatVersion version;
    OvirtStorageDomainState state;
};

G_DEFINE_TYPE(OvirtStorageDomain, ovirt_storage_domain, OVIRT_TYPE_RESOURCE);

enum {
    PROP_0,
    PROP_STORAGE_TYPE,
    PROP_MASTER,
    PROP_AVAILABLE,
    PROP_USED,
    PROP_COMMITTED,
    PROP_VERSION,
    PROP_STATE
};

static gboolean
ovirt_storage_domain_refresh_from_xml(OvirtStorageDomain *domain,
                                      RestXmlNode *node);

static void ovirt_storage_domain_get_property(GObject *object,
                                              guint prop_id,
                                              GValue *value,
                                              GParamSpec *pspec)
{
    OvirtStorageDomain *domain = OVIRT_STORAGE_DOMAIN(object);

    switch (prop_id) {
    case PROP_STORAGE_TYPE:
        g_value_set_enum(value, domain->priv->type);
        break;
    case PROP_MASTER:
        g_value_set_boolean(value, domain->priv->is_master);
        break;
    case PROP_AVAILABLE:
        g_value_set_uint64(value, domain->priv->available);
        break;
    case PROP_USED:
        g_value_set_uint64(value, domain->priv->used);
        break;
    case PROP_COMMITTED:
        g_value_set_uint64(value, domain->priv->committed);
        break;
    case PROP_VERSION:
        g_value_set_enum(value, domain->priv->version);
        break;
    case PROP_STATE:
        g_value_set_enum(value, domain->priv->state);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void ovirt_storage_domain_set_property(GObject *object,
                                              guint prop_id,
                                              const GValue *value,
                                              GParamSpec *pspec)
{
    OvirtStorageDomain *domain = OVIRT_STORAGE_DOMAIN(object);

    switch (prop_id) {
    case PROP_STORAGE_TYPE:
        domain->priv->type = g_value_get_enum(value);
        break;
    case PROP_MASTER:
        domain->priv->is_master = g_value_get_boolean(value);
        break;
    case PROP_AVAILABLE:
        domain->priv->available = g_value_get_uint64(value);
        break;
    case PROP_USED:
        domain->priv->used = g_value_get_uint64(value);
        break;
    case PROP_COMMITTED:
        domain->priv->committed = g_value_get_uint64(value);
        break;
    case PROP_VERSION:
        domain->priv->version = g_value_get_enum(value);
        break;
    case PROP_STATE:
        domain->priv->state = g_value_get_enum(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


static void
ovirt_storage_domain_dispose(GObject *obj)
{
    OvirtStorageDomain *domain = OVIRT_STORAGE_DOMAIN(obj);

    g_clear_object(&domain->priv->files);

    G_OBJECT_CLASS(ovirt_storage_domain_parent_class)->dispose(obj);
}


static gboolean ovirt_storage_domain_init_from_xml(OvirtResource *resource,
                                                   RestXmlNode *node,
                                                   GError **error)
{
    gboolean parsed_ok;
    OvirtResourceClass *parent_class;
    OvirtStorageDomain *domain;

    domain = OVIRT_STORAGE_DOMAIN(resource);
    parsed_ok = ovirt_storage_domain_refresh_from_xml(domain, node);
    if (!parsed_ok) {
        return FALSE;
    }
    parent_class = OVIRT_RESOURCE_CLASS(ovirt_storage_domain_parent_class);

    return parent_class->init_from_xml(resource, node, error);
}


static void ovirt_storage_domain_class_init(OvirtStorageDomainClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    OvirtResourceClass *resource_class = OVIRT_RESOURCE_CLASS(klass);
    GParamSpec *param_spec;

    g_type_class_add_private(klass, sizeof(OvirtStorageDomainPrivate));

    resource_class->init_from_xml = ovirt_storage_domain_init_from_xml;
    object_class->dispose = ovirt_storage_domain_dispose;
    object_class->get_property = ovirt_storage_domain_get_property;
    object_class->set_property = ovirt_storage_domain_set_property;

    param_spec = g_param_spec_enum("type",
                                   "Storage Type",
                                   "Type of the storage domain",
                                   OVIRT_TYPE_STORAGE_DOMAIN_TYPE,
                                   OVIRT_STORAGE_DOMAIN_TYPE_DATA,
                                   G_PARAM_READWRITE |
                                   G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_STORAGE_TYPE,
                                    param_spec);

    param_spec = g_param_spec_boolean("master",
                                      "Master Storage Domain?",
                                      "Indicates whether the storage domain is a master on not",
                                      FALSE,
                                      G_PARAM_READWRITE |
                                      G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_MASTER,
                                    param_spec);

    param_spec = g_param_spec_uint64("available",
                                     "Space available",
                                     "Space available in the storage domain in bytes",
                                     0,
                                     G_MAXUINT64,
                                     0,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_AVAILABLE,
                                    param_spec);

    param_spec = g_param_spec_uint64("used",
                                     "Space used",
                                     "Space used in the storage domain in bytes",
                                     0,
                                     G_MAXUINT64,
                                     0,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_USED,
                                    param_spec);

    param_spec = g_param_spec_uint64("committed",
                                     "Space committed",
                                     "Space committed in the storage domain in bytes",
                                     0,
                                     G_MAXUINT64,
                                     0,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_COMMITTED,
                                    param_spec);

    param_spec = g_param_spec_enum("version",
                                   "Storage Format Version",
                                   "Storage Format Version of the storage domain",
                                   OVIRT_TYPE_STORAGE_DOMAIN_FORMAT_VERSION,
                                   OVIRT_STORAGE_DOMAIN_FORMAT_VERSION_V1,
                                   G_PARAM_READWRITE |
                                   G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_VERSION,
                                    param_spec);

    param_spec = g_param_spec_enum("state",
                                   "Storage Domain State",
                                   "State of the storage domain",
                                   OVIRT_TYPE_STORAGE_DOMAIN_STATE,
                                   OVIRT_STORAGE_DOMAIN_STATE_UNKNOWN,
                                   G_PARAM_READWRITE |
                                   G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class,
                                    PROP_STATE,
                                    param_spec);
}

static void ovirt_storage_domain_init(OvirtStorageDomain *domain)
{
    domain->priv = OVIRT_STORAGE_DOMAIN_GET_PRIVATE(domain);
}

G_GNUC_INTERNAL
OvirtStorageDomain *ovirt_storage_domain_new_from_xml(RestXmlNode *node,
                                                      GError **error)
{
    GObject *domain;

    domain = g_initable_new(OVIRT_TYPE_STORAGE_DOMAIN, NULL, error,
                            "xml-node", node, NULL);

    return OVIRT_STORAGE_DOMAIN(domain);
}

OvirtStorageDomain *ovirt_storage_domain_new(void)
{
    GObject *domain;

    domain = g_initable_new(OVIRT_TYPE_STORAGE_DOMAIN, NULL, NULL, NULL);

    return OVIRT_STORAGE_DOMAIN(domain);
}

typedef struct {
    const char *xml_node;
    GType type;
    const char *prop_name;
} OvirtXmlElement;

static gboolean
ovirt_resource_parse_xml(OvirtResource *resource,
                         RestXmlNode *node,
                         OvirtXmlElement *elements)
{
    g_return_val_if_fail(OVIRT_IS_RESOURCE(resource), FALSE);
    g_return_val_if_fail(elements != NULL, FALSE);

    for (;elements->xml_node != NULL; elements++) {
        const char *value_str;
        GValue value = { 0, };

        value_str = ovirt_rest_xml_node_get_content_from_path(node, elements->xml_node);
        if (value_str == NULL) {
            continue;
        }
        g_value_init(&value, elements->type);

        if (G_TYPE_IS_ENUM(elements->type)) {
            int enum_value;
            enum_value = ovirt_utils_genum_get_value(elements->type,
                                                     value_str, 0);
            g_value_set_enum(&value, enum_value);
        } else if (elements->type == G_TYPE_BOOLEAN) {
            gboolean bool_value;

            bool_value = ovirt_utils_boolean_from_string(value_str);
            g_value_set_boolean(&value, bool_value);
        } else if (elements->type == G_TYPE_UINT64) {
            guint64 int64_value;

            int64_value = g_ascii_strtoull(value_str, NULL, 0);
            g_value_set_uint64(&value, int64_value);
        } else if (g_type_is_a(elements->type, OVIRT_TYPE_RESOURCE)) {
            GObject *resource_value;

            resource_value = g_initable_new(elements->type, NULL, NULL,
                                            "xml-node", node, NULL);
            g_value_set_object(&value, resource_value);
        }
        g_object_set_property(G_OBJECT(resource), elements->prop_name, &value);
        g_value_unset(&value);
    }

    return TRUE;
}

static gboolean
ovirt_storage_domain_refresh_from_xml(OvirtStorageDomain *domain,
                                      RestXmlNode *node)
{
    OvirtXmlElement storage_domain_elements[] = {
        { "type",           OVIRT_TYPE_STORAGE_DOMAIN_TYPE,           "type" },
        { "master",         G_TYPE_BOOLEAN,                           "master" },
        { "available",      G_TYPE_UINT64,                            "available" },
        { "used",           G_TYPE_UINT64,                            "used" },
        { "committed",      G_TYPE_UINT64,                            "committed" },
        { "storage_format", OVIRT_TYPE_STORAGE_DOMAIN_FORMAT_VERSION, "version" },
        { "status/state",   OVIRT_TYPE_STORAGE_DOMAIN_STATE,          "state" },
        { NULL,             G_TYPE_INVALID,                           NULL }
    };

    return ovirt_resource_parse_xml(OVIRT_RESOURCE(domain), node, storage_domain_elements);
}


/**
 * ovirt_storage_domain_get_files:
 * @domain: a #OvirtStorageDomain
 *
 * Gets a #OvirtCollection representing the list of remote files from a
 * storage domain object.  This method does not initiate any network
 * activity, the remote file list must be then be fetched using
 * ovirt_collection_fetch() or ovirt_collection_fetch_async().
 *
 * Return value: (transfer none): a #OvirtCollection representing the list
 * of files associated with @domain.
 */
OvirtCollection *ovirt_storage_domain_get_files(OvirtStorageDomain *domain)
{
    const char *href;

    g_return_val_if_fail(OVIRT_IS_STORAGE_DOMAIN(domain), NULL);

    if (domain->priv->files != NULL)
        return domain->priv->files;

    href = ovirt_resource_get_sub_collection(OVIRT_RESOURCE(domain), "files");
    if (href == NULL)
        return NULL;

    domain->priv->files = ovirt_collection_new(href, "files",
                                               OVIRT_TYPE_RESOURCE,
                                               "file");

    return domain->priv->files;
}
