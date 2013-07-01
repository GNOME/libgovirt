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

#include <ovirt-resource.h>

G_BEGIN_DECLS

const char *ovirt_resource_get_action(OvirtResource *resource,
                                      const char *action);

G_END_DECLS

#endif /* __OVIRT_RESOURCE_PRIVATE_H__ */
