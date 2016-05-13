/*
 * ovirt-proxy-private.h: oVirt virtual machine private header
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
#ifndef __OVIRT_PROXY_PRIVATE_H__
#define __OVIRT_PROXY_PRIVATE_H__

#include <rest/rest-xml-node.h>
#include <libsoup/soup-cookie-jar.h>
#include <libsoup/soup-session-feature.h>

#include "ovirt-proxy.h"
#include "ovirt-rest-call.h"

G_BEGIN_DECLS

struct _OvirtProxyPrivate {
    char *tmp_ca_file;
    GByteArray *display_ca;
    gboolean admin_mode;
    OvirtApi *api;
    char *jsessionid;
    SoupCookie *jsessionid_cookie;
    char *sso_token;

    SoupCookieJar *cookie_jar;
    GHashTable *additional_headers;

    gboolean setting_ca_file;
    gulong ssl_ca_file_changed_id;
};

RestXmlNode *ovirt_proxy_get_collection_xml(OvirtProxy *proxy,
                                            const char *href,
                                            GError **error);
typedef gboolean (*OvirtProxyGetCollectionAsyncCb)(OvirtProxy* proxy,
                                                   RestXmlNode *root_node,
                                                   gpointer user_data,
                                                   GError **error);
void ovirt_proxy_get_collection_xml_async(OvirtProxy *proxy,
                                          const char *href,
                                          GTask *task,
                                          GCancellable *cancellable,
                                          OvirtProxyGetCollectionAsyncCb callback,
                                          gpointer user_data,
                                          GDestroyNotify destroy_func);

typedef gboolean (*OvirtProxyCallAsyncCb)(OvirtProxy *proxy,
                                          RestProxyCall *call,
                                          gpointer user_data,
                                          GError **error);
void ovirt_rest_call_async(OvirtRestCall *call,
                           GTask *task,
                           GCancellable *cancellable,
                           OvirtProxyCallAsyncCb callback,
                           gpointer user_data,
                           GDestroyNotify destroy_func);
gboolean ovirt_rest_call_finish(GAsyncResult *result, GError **err);

/* Work around G_GNUC_DEPRECATED attribute on ovirt_proxy_get_vms() */
GList *ovirt_proxy_get_vms_internal(OvirtProxy *proxy);
void ovirt_proxy_append_additional_headers(OvirtProxy *proxy,
                                           RestProxyCall *call);
void ovirt_proxy_add_header(OvirtProxy *proxy, const char *header, const char *value);
void ovirt_proxy_add_headers(OvirtProxy *proxy, ...);
void ovirt_proxy_add_headers_from_valist(OvirtProxy *proxy, va_list headers);

G_END_DECLS

#endif /* __OVIRT_PROXY_H__ */
