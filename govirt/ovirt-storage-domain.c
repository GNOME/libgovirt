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

#define OVIRT_STORAGE_DOMAIN_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), OVIRT_TYPE_STORAGE_DOMAIN, OvirtStorageDomainPrivate))

struct _OvirtStorageDomainPrivate {
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

static void ovirt_storage_domain_class_init(OvirtStorageDomainClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GParamSpec *param_spec;

    g_type_class_add_private(klass, sizeof(OvirtStorageDomainPrivate));

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
