/*
 * ovirt-resource-private.h: generic oVirt resource
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
#ifndef __OVIRT_RESOURCE_PRIVATE_H__
#define __OVIRT_RESOURCE_PRIVATE_H__

#include <govirt/ovirt-resource.h>
#include <govirt/ovirt-resource-rest-call.h>

G_BEGIN_DECLS

OvirtResource *ovirt_resource_new(GType type);
OvirtResource *ovirt_resource_new_from_id(GType type, const char *id, const char *href);
OvirtResource *ovirt_resource_new_from_xml(GType type, RestXmlNode *node, GError **error);

const char *ovirt_resource_get_action(OvirtResource *resource,
                                      const char *action);
char *ovirt_resource_to_xml(OvirtResource *resource);
RestXmlNode *ovirt_resource_rest_call_sync(OvirtRestCall *call, GError **error);

typedef gboolean (*ActionResponseParser)(RestXmlNode *node, OvirtResource *resource, GError **error);
gboolean ovirt_resource_action(OvirtResource *resource, OvirtProxy *proxy,
                               const char *action,
                               ActionResponseParser response_parser,
                               GError **error);

void ovirt_resource_invoke_action_async(OvirtResource *resource,
                                        const char *action,
                                        OvirtProxy *proxy,
                                        ActionResponseParser response_parser,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data);
gboolean ovirt_resource_action_finish(OvirtResource *resource,
                                      GAsyncResult *result,
                                      GError **err);
void ovirt_resource_add_rest_params(OvirtResource *resource,
                                    RestProxyCall *call);

G_END_DECLS

#endif /* __OVIRT_RESOURCE_PRIVATE_H__ */
