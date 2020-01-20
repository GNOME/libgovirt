/*
 * ovirt-vm.c: oVirt virtual machine
 *
 * Copyright (C) 2012, 2013 Red Hat, Inc.
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

#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include <rest/rest-xml-node.h>
#include <rest/rest-xml-parser.h>

#include "govirt.h"
#include "govirt-private.h"


static gboolean parse_ticket_status(RestXmlNode *root, OvirtResource *resource,
                                    GError **error);

struct _OvirtVmPrivate {
    OvirtCollection *cdroms;

    OvirtVmState state;
    OvirtVmDisplay *display;
    gchar *host_href;
    gchar *host_id;
    gchar *cluster_href;
    gchar *cluster_id;
} ;

G_DEFINE_TYPE_WITH_PRIVATE(OvirtVm, ovirt_vm, OVIRT_TYPE_RESOURCE);

enum OvirtResponseStatus {
    OVIRT_RESPONSE_UNKNOWN,
    OVIRT_RESPONSE_FAILED,
    OVIRT_RESPONSE_PENDING,
    OVIRT_RESPONSE_IN_PROGRESS,
    OVIRT_RESPONSE_COMPLETE
};

enum {
    PROP_0,
    PROP_STATE,
    PROP_DISPLAY,
    PROP_HOST_HREF,
    PROP_HOST_ID,
    PROP_CLUSTER_HREF,
    PROP_CLUSTER_ID,
};

static char *ensure_href_from_id(const char *id,
                                 const char *path)
{
    if (id == NULL)
        return NULL;

    return g_strdup_printf("%s/%s", path, id);
}

static const char *get_host_href(OvirtVm *vm)
{
    if (vm->priv->host_href == NULL)
        vm->priv->host_href = ensure_href_from_id(vm->priv->host_id, "/ovirt-engine/api/hosts");

    return vm->priv->host_href;
}

static const char *get_cluster_href(OvirtVm *vm)
{
    if (vm->priv->cluster_href == NULL)
        vm->priv->cluster_href = ensure_href_from_id(vm->priv->cluster_id, "/ovirt-engine/api/clusters");

    return vm->priv->cluster_href;
}

static void ovirt_vm_get_property(GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    OvirtVm *vm = OVIRT_VM(object);

    switch (prop_id) {
    case PROP_STATE:
        g_value_set_enum(value, vm->priv->state);
        break;
    case PROP_DISPLAY:
        g_value_set_object(value, vm->priv->display);
        break;
    case PROP_HOST_HREF:
        g_value_set_string(value, get_host_href(vm));
        break;
    case PROP_HOST_ID:
        g_value_set_string(value, vm->priv->host_id);
        break;
    case PROP_CLUSTER_HREF:
        g_value_set_string(value, get_cluster_href(vm));
        break;
    case PROP_CLUSTER_ID:
        g_value_set_string(value, vm->priv->cluster_id);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void ovirt_vm_set_property(GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    OvirtVm *vm = OVIRT_VM(object);

    switch (prop_id) {
    case PROP_STATE:
        vm->priv->state = g_value_get_enum(value);
        break;
    case PROP_DISPLAY:
        g_clear_object(&vm->priv->display);
        vm->priv->display = g_value_dup_object(value);
        break;
    case PROP_HOST_HREF:
        g_free(vm->priv->host_href);
        vm->priv->host_href = g_value_dup_string(value);
        break;
    case PROP_HOST_ID:
        g_free(vm->priv->host_id);
        vm->priv->host_id = g_value_dup_string(value);
        break;
    case PROP_CLUSTER_HREF:
        g_free(vm->priv->cluster_href);
        vm->priv->cluster_href = g_value_dup_string(value);
        break;
    case PROP_CLUSTER_ID:
        g_free(vm->priv->cluster_id);
        vm->priv->cluster_id = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}


static void ovirt_vm_dispose(GObject *object)
{
    OvirtVm *vm = OVIRT_VM(object);

    g_clear_object(&vm->priv->cdroms);
    g_clear_object(&vm->priv->display);
    g_clear_pointer(&vm->priv->host_href, g_free);
    g_clear_pointer(&vm->priv->host_id, g_free);
    g_clear_pointer(&vm->priv->cluster_href, g_free);
    g_clear_pointer(&vm->priv->cluster_id, g_free);

    G_OBJECT_CLASS(ovirt_vm_parent_class)->dispose(object);
}


static gboolean ovirt_vm_init_from_xml(OvirtResource *resource,
                                       RestXmlNode *node,
                                       GError **error)
{
    OvirtVmDisplay *display;
    RestXmlNode *display_node;
    OvirtResourceClass *parent_class;
    OvirtXmlElement vm_elements[] = {
        { .prop_name = "host-href",
          .xml_path = "host",
          .xml_attr = "href",
        },
        { .prop_name = "host-id",
          .xml_path = "host",
          .xml_attr = "id",
        },
        { .prop_name = "cluster-href",
          .xml_path = "cluster",
          .xml_attr = "href",
        },
        { .prop_name = "cluster-id",
          .xml_path = "cluster",
          .xml_attr = "id",
        },
        { .prop_name = "state",
          .xml_path = "status",
        },
        { NULL, },
    };

    display_node = rest_xml_node_find(node, "display");
    if (display_node == NULL) {
        g_debug("Could not find 'display' node");
        return FALSE;
    }

    display = ovirt_vm_display_new_from_xml(display_node);
    if (display == NULL)
        return FALSE;

    g_object_set(G_OBJECT(resource), "display", display, NULL);
    g_object_unref(G_OBJECT(display));

    if (!ovirt_rest_xml_node_parse(node, G_OBJECT(resource), vm_elements))
        return FALSE;

    parent_class = OVIRT_RESOURCE_CLASS(ovirt_vm_parent_class);

    return parent_class->init_from_xml(resource, node, error);
}

static void ovirt_vm_class_init(OvirtVmClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    OvirtResourceClass *resource_class = OVIRT_RESOURCE_CLASS(klass);

    resource_class->init_from_xml = ovirt_vm_init_from_xml;
    object_class->dispose = ovirt_vm_dispose;
    object_class->get_property = ovirt_vm_get_property;
    object_class->set_property = ovirt_vm_set_property;

    g_object_class_install_property(object_class,
                                    PROP_STATE,
                                    g_param_spec_enum("state",
                                                      "State",
                                                      "Virtual Machine State",
                                                      OVIRT_TYPE_VM_STATE,
                                                      OVIRT_VM_STATE_UNKNOWN,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_DISPLAY,
                                    g_param_spec_object("display",
                                                        "Display",
                                                        "Virtual Machine Display Information",
                                                        OVIRT_TYPE_VM_DISPLAY,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_HOST_HREF,
                                    g_param_spec_string("host-href",
                                                        "Host href",
                                                        "Host href for the Virtual Machine",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_HOST_ID,
                                    g_param_spec_string("host-id",
                                                        "Host Id",
                                                        "Host Id for the Virtual Machine",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_CLUSTER_HREF,
                                    g_param_spec_string("cluster-href",
                                                        "Cluster href",
                                                        "Cluster href for the Virtual Machine",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_CLUSTER_ID,
                                    g_param_spec_string("cluster-id",
                                                        "Cluster Id",
                                                        "Cluster Id for the Virtual Machine",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void ovirt_vm_init(G_GNUC_UNUSED OvirtVm *vm)
{
    vm->priv = ovirt_vm_get_instance_private(vm);
}

G_GNUC_INTERNAL
OvirtVm *ovirt_vm_new_from_xml(RestXmlNode *node, GError **error)
{
    OvirtResource *vm = ovirt_resource_new_from_xml(OVIRT_TYPE_VM, node, error);
    return OVIRT_VM(vm);
}

OvirtVm *ovirt_vm_new(void)
{
    OvirtResource *vm = ovirt_resource_new(OVIRT_TYPE_VM);
    return OVIRT_VM(vm);
}


void
ovirt_vm_get_ticket_async(OvirtVm *vm, OvirtProxy *proxy,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
    ovirt_resource_invoke_action_async(OVIRT_RESOURCE(vm), "ticket",
                                       proxy, parse_ticket_status,
                                       cancellable, callback,
                                       user_data);
}

gboolean
ovirt_vm_get_ticket_finish(OvirtVm *vm, GAsyncResult *result, GError **err)
{
    return ovirt_resource_action_finish(OVIRT_RESOURCE(vm), result, err);
}

void
ovirt_vm_start_async(OvirtVm *vm, OvirtProxy *proxy,
                     GCancellable *cancellable,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    ovirt_resource_invoke_action_async(OVIRT_RESOURCE(vm), "start", proxy, NULL,
                                       cancellable, callback, user_data);
}

gboolean
ovirt_vm_start_finish(OvirtVm *vm, GAsyncResult *result, GError **err)
{
    return ovirt_resource_action_finish(OVIRT_RESOURCE(vm), result, err);
}

void
ovirt_vm_stop_async(OvirtVm *vm, OvirtProxy *proxy,
                    GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
    ovirt_resource_invoke_action_async(OVIRT_RESOURCE(vm), "stop", proxy, NULL,
                                       cancellable, callback, user_data);
}

gboolean
ovirt_vm_stop_finish(OvirtVm *vm, GAsyncResult *result, GError **err)
{
    return ovirt_resource_action_finish(OVIRT_RESOURCE(vm), result, err);
}


gboolean ovirt_vm_get_ticket(OvirtVm *vm, OvirtProxy *proxy, GError **error)
{
    return ovirt_resource_action(OVIRT_RESOURCE(vm), proxy, "ticket",
                                 parse_ticket_status,
                                 error);
}

gboolean ovirt_vm_start(OvirtVm *vm, OvirtProxy *proxy, GError **error)
{
    return ovirt_resource_action(OVIRT_RESOURCE(vm), proxy, "start",
                                 NULL, error);
}

gboolean ovirt_vm_stop(OvirtVm *vm, OvirtProxy *proxy, GError **error)
{
    return ovirt_resource_action(OVIRT_RESOURCE(vm), proxy, "stop",
                                 NULL, error);
}

static gboolean parse_ticket_status(RestXmlNode *root, OvirtResource *resource, GError **error)
{
    OvirtVmDisplay *display;
    gchar *ticket = NULL;
    guint expiry = 0;
    gboolean ret = FALSE;
    OvirtXmlElement ticket_elements[] = {
        { .prop_name = "ticket",
          .xml_path = "value",
        },
        { .prop_name = "expiry",
          .xml_path = "expiry",
        },
        { NULL, },
    };

    g_return_val_if_fail(root != NULL, FALSE);
    g_return_val_if_fail(OVIRT_IS_VM(resource), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_object_get(G_OBJECT(resource), "display", &display, NULL);
    g_return_val_if_fail(display != NULL, FALSE);

    root = rest_xml_node_find(root, "ticket");
    if (root == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    _("Could not find 'ticket' node"));
        goto end;
    }

    ovirt_rest_xml_node_parse(root, G_OBJECT(display), ticket_elements);

    g_object_get(G_OBJECT(display), "ticket", &ticket, "expiry", &expiry, NULL);

    if (ticket == NULL) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    _("Could not find 'value' node"));
        goto end;
    }
    g_free(ticket);

    if (expiry == 0) {
        g_set_error(error, OVIRT_ERROR, OVIRT_ERROR_PARSING_FAILED,
                    _("Could not find 'expiry' node"));
        goto end;
    }

    ret = TRUE;

end:
    g_object_unref(G_OBJECT(display));
    return ret;
}


void ovirt_vm_refresh_async(OvirtVm *vm, OvirtProxy *proxy,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
    g_return_if_fail(OVIRT_IS_VM(vm));

    ovirt_resource_refresh_async(OVIRT_RESOURCE(vm), proxy,
                                 cancellable, callback,
                                 user_data);
}


gboolean ovirt_vm_refresh_finish(OvirtVm *vm,
                                 GAsyncResult *result,
                                 GError **err)
{
    g_return_val_if_fail(OVIRT_IS_VM(vm), FALSE);
    return ovirt_resource_refresh_finish(OVIRT_RESOURCE(vm),
                                         result, err);
}


/**
 * ovirt_vm_get_cdroms:
 * @vm: a #OvirtVm
 *
 * Gets a #OvirtCollection representing the list of remote cdroms from a
 * virtual machine object.  This method does not initiate any network
 * activity, the remote cdrom list must be then be fetched using
 * ovirt_collection_fetch() or ovirt_collection_fetch_async().
 *
 * Return value: (transfer none): a #OvirtCollection representing the list
 * of cdroms associated with @vm.
 */
OvirtCollection *ovirt_vm_get_cdroms(OvirtVm *vm)
{
    g_return_val_if_fail(OVIRT_IS_VM(vm), NULL);

    if (vm->priv->cdroms == NULL)
        vm->priv->cdroms = ovirt_sub_collection_new_from_resource(OVIRT_RESOURCE(vm),
                                                                  "cdroms",
                                                                  "cdroms",
                                                                  OVIRT_TYPE_CDROM,
                                                                  "cdrom");

    return vm->priv->cdroms;
}


/**
 * ovirt_vm_get_host:
 * @vm: a #OvirtVm
 *
 * Gets a #OvirtHost representing the host the virtual machine belongs to.
 * This method does not initiate any network activity, the remote host must be
 * then be fetched using ovirt_resource_refresh() or
 * ovirt_resource_refresh_async().
 *
 * Return value: (transfer full): a #OvirtHost representing host the @vm
 * belongs to.
 */
OvirtHost *ovirt_vm_get_host(OvirtVm *vm)
{
    g_return_val_if_fail(OVIRT_IS_VM(vm), NULL);
    g_return_val_if_fail(vm->priv->host_id != NULL, NULL);
    return ovirt_host_new_from_id(vm->priv->host_id, get_host_href(vm));
}


/**
 * ovirt_vm_get_cluster:
 * @vm: a #OvirtVm
 *
 * Gets a #OvirtCluster representing the cluster the virtual machine belongs
 * to. This method does not initiate any network activity, the remote host must
 * be then be fetched using ovirt_resource_refresh() or
 * ovirt_resource_refresh_async().
 *
 * Return value: (transfer full): a #OvirtCluster representing cluster the @vm
 * belongs to.
 */
OvirtCluster *ovirt_vm_get_cluster(OvirtVm *vm)
{
    g_return_val_if_fail(OVIRT_IS_VM(vm), NULL);
    g_return_val_if_fail(vm->priv->cluster_id != NULL, NULL);
    return ovirt_cluster_new_from_id(vm->priv->cluster_id, get_cluster_href(vm));
}
