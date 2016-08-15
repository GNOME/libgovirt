/*
 * ovirt-data_center-private.h: oVirt data center resource
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
#ifndef __OVIRT_DATA_CENTER_PRIVATE_H__
#define __OVIRT_DATA_CENTER_PRIVATE_H__

#include <govirt/ovirt-data-center.h>
#include <rest/rest-xml-node.h>

G_BEGIN_DECLS

OvirtDataCenter *ovirt_data_center_new_from_id(const char *id,
                                               const char *href);
OvirtDataCenter *ovirt_data_center_new_from_xml(RestXmlNode *node,
                                                GError **error);

G_END_DECLS

#endif /* __OVIRT_DATA_CENTER_PRIVATE_H__ */
