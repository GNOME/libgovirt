/* Copyright 2016 Red Hat, Inc. and/or its affiliates.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <config.h>

#include <govirt/govirt.h>

#include <stdlib.h>

#include "mock-httpd.h"

#define GOVIRT_HTTPS_PORT 8088


static void ovirt_proxy_set_mock_ca(OvirtProxy *proxy)
{
    gchar *data;
    gsize size;
    GByteArray *cacert;

    /* check_return: Calling "g_file_get_contents("/builddir/build/BUILD/libgovirt-0.3.7/tests/https-cert/ca-cert.pem", &data, &size, NULL)" without checking return value. This library function may fail and return an error code. */
    g_file_get_contents(abs_srcdir "/https-cert/ca-cert.pem", &data, &size, NULL);
    cacert = g_byte_array_new_take((guint8 *)data, size);
    g_object_set(proxy, "ca-cert", cacert, NULL);
    g_byte_array_unref(cacert);
}


static void test_govirt_https_ca(void)
{
    OvirtProxy *proxy;
    OvirtApi *api;
    GError *error = NULL;
    GovirtMockHttpd *httpd;

    /*FIXME: https://gitlab.gnome.org/GNOME/librest/-/issues/16
             https://gitlab.gnome.org/GNOME/librest/-/merge_requests/28
    */
    return;
    g_setenv("GOVIRT_NO_SSL_STRICT", "1", TRUE);
    httpd = govirt_mock_httpd_new(GOVIRT_HTTPS_PORT);
    govirt_mock_httpd_add_request(httpd, "GET", "/ovirt-engine/api", "<api></api>");
    govirt_mock_httpd_start(httpd);

    g_test_expect_message("libgovirt", G_LOG_LEVEL_WARNING,
                          "Disabling strict checking of SSL certificates");
    g_test_expect_message("libgovirt", G_LOG_LEVEL_CRITICAL,
                          "ovirt_proxy_set_api_from_xml: assertion 'vms != NULL' failed");
    proxy = ovirt_proxy_new("localhost:" G_STRINGIFY(GOVIRT_HTTPS_PORT));
    api = ovirt_proxy_fetch_api(proxy, &error);
    g_test_assert_expected_messages();
    g_assert_nonnull(api);
    g_assert_no_error(error);
    g_object_unref(proxy);
    g_unsetenv("GOVIRT_NO_SSL_STRICT");

    g_test_expect_message("Rest", G_LOG_LEVEL_WARNING,
                          "*runtime check failed*");
    g_test_expect_message("libgovirt", G_LOG_LEVEL_WARNING,
                          "Error while getting collection: Unacceptable TLS certificate");
    proxy = ovirt_proxy_new("localhost:" G_STRINGIFY(GOVIRT_HTTPS_PORT));
    api = ovirt_proxy_fetch_api(proxy, &error);
    g_test_assert_expected_messages();
    g_assert_null(api);
    g_assert_error(error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE);
    g_assert_cmpstr(error->message, ==, "Unacceptable TLS certificate");
    g_clear_error(&error);

    g_test_expect_message("libgovirt", G_LOG_LEVEL_CRITICAL,
                          "ovirt_proxy_set_api_from_xml: assertion 'vms != NULL' failed");
    ovirt_proxy_set_mock_ca(proxy);
    api = ovirt_proxy_fetch_api(proxy, &error);
    g_test_assert_expected_messages();
    g_assert_nonnull(api);
    g_assert_no_error(error);
    g_clear_object(&proxy);

    govirt_mock_httpd_stop(httpd);
}


static void test_govirt_http(void)
{
    OvirtProxy *proxy;
    OvirtApi *api;
    GError *error = NULL;
    GovirtMockHttpd *httpd;

    g_setenv("GOVIRT_DISABLE_HTTPS", "1", TRUE);
    httpd = govirt_mock_httpd_new(GOVIRT_HTTPS_PORT);
    govirt_mock_httpd_disable_tls(httpd, TRUE);
    govirt_mock_httpd_add_request(httpd, "GET", "/ovirt-engine/api", "<api></api>");
    govirt_mock_httpd_start(httpd);

    g_test_expect_message("libgovirt", G_LOG_LEVEL_WARNING,
                          "Using plain text HTTP connection");
    g_test_expect_message("libgovirt", G_LOG_LEVEL_CRITICAL,
                          "ovirt_proxy_set_api_from_xml: assertion 'vms != NULL' failed");
    proxy = ovirt_proxy_new("localhost:" G_STRINGIFY(GOVIRT_HTTPS_PORT));
    api = ovirt_proxy_fetch_api(proxy, &error);
    g_test_assert_expected_messages();
    g_assert_nonnull(api);
    g_assert_no_error(error);
    g_object_unref(proxy);
    g_unsetenv("GOVIRT_DISABLE_HTTPS");

    govirt_mock_httpd_stop(httpd);
}

static void check_vm_display(OvirtVm *vm)
{
    OvirtVmDisplay *display;
    gint type;
    char *address;
    char *proxy_url;
    guint port;
    guint secure_port;
    guint monitor_count;
    gboolean smartcard;

    g_object_get(vm, "display", &display, NULL);
    g_assert_nonnull(display);
    g_object_get(display,
                 "type", &type,
                 "address", &address,
                 "port", &port,
                 "secure-port", &secure_port,
                 "monitor-count", &monitor_count,
                 "smartcard", &smartcard,
                 "proxy-url", &proxy_url,
                 NULL);

    g_assert_cmpuint(type, ==, OVIRT_VM_DISPLAY_SPICE);
    g_assert_cmpstr(address, ==, "10.0.0.123");
    g_assert_cmpuint(port, ==, 0);
    g_assert_cmpuint(secure_port, ==, 5900);
    g_assert_cmpuint(monitor_count, ==, 1);
    g_assert_false(smartcard);
    g_assert_cmpstr(proxy_url, ==, "10.0.0.10");

    g_object_unref(display);
    g_free(address);
    g_free(proxy_url);
}

static void test_govirt_list_vms(void)
{
    OvirtProxy *proxy;
    OvirtApi *api;
    OvirtCollection *vms;
    OvirtResource *vm;
    GError *error = NULL;
    GovirtMockHttpd *httpd;

    const char *vms_body = "<vms> \
                              <vm href=\"/ovirt-engine/api/vms/uuid0\" id=\"uuid0\"> \
                                <name>vm0</name> \
                              </vm> \
                              <vm href=\"/ovirt-engine/api/vms/uuid1\" id=\"uuid1\"> \
                                <name>vm1</name> \
                                <type>desktop</type> \
                                <status>up</status> \
                                <display> \
                                    <type>spice</type> \
                                    <address>10.0.0.123</address> \
                                    <secure_port>5900</secure_port> \
                                    <monitors>1</monitors> \
                                    <single_qxl_pci>true</single_qxl_pci> \
                                    <allow_override>false</allow_override> \
                                    <smartcard_enabled>false</smartcard_enabled> \
                                    <proxy>10.0.0.10</proxy> \
                                    <file_transfer_enabled>true</file_transfer_enabled> \
                                    <copy_paste_enabled>true</copy_paste_enabled> \
                                </display> \
                              </vm> \
                              <vm href=\"/ovirt-engine/api/vms/uuid2\" id=\"uuid2\"> \
                                <name>vm2</name> \
                              </vm> \
                            </vms>";

    httpd = govirt_mock_httpd_new(GOVIRT_HTTPS_PORT);
    govirt_mock_httpd_add_request(httpd, "GET", "/ovirt-engine/api",
                                  "<api><link href=\"/ovirt-engine/api/vms\" rel=\"vms\"/></api>");
    govirt_mock_httpd_add_request(httpd, "GET", "/ovirt-engine/api/vms", vms_body);
    govirt_mock_httpd_start(httpd);

    proxy = ovirt_proxy_new("localhost:" G_STRINGIFY(GOVIRT_HTTPS_PORT));
    ovirt_proxy_set_mock_ca(proxy);
    api = ovirt_proxy_fetch_api(proxy, &error);
    g_test_assert_expected_messages();
    g_assert_nonnull(api);
    g_assert_no_error(error);

    vms = ovirt_api_get_vms(api);
    ovirt_collection_fetch(vms, proxy, &error);
    g_assert_no_error(error);

    vm = ovirt_collection_lookup_resource(vms, "vm1");
    g_assert_nonnull(vm);

    check_vm_display(OVIRT_VM(vm));

    g_object_unref(vm);
    g_object_unref(proxy);

    govirt_mock_httpd_stop(httpd);
}


typedef struct {
    const char *uuid;
    const char *name;
    const char *xml;
    const char *filename;
} MockOvirtVm;

static void govirt_mock_httpd_add_vms(GovirtMockHttpd *httpd, MockOvirtVm vms[], size_t count)
{
    GString *vms_xml;
    unsigned int i;

    govirt_mock_httpd_add_request(httpd, "GET", "/ovirt-engine/api",
                                  "<api><link href=\"/ovirt-engine/api/vms\" rel=\"vms\"/></api>");

    /* Create /api/vms */
    vms_xml = g_string_new("<vms>");
    for (i = 0; i < count; i++) {
        MockOvirtVm *vm = &vms[i];

        g_string_append_printf(vms_xml, "<vm href=\"/ovirt-engine/api/vms/%s\" id=\"%s\">", vm->uuid, vm->uuid);
        g_string_append_printf(vms_xml, "<name>%s</name>", vm->name);
        g_string_append_printf(vms_xml, "<display> \
                                           <type>spice</type> \
                                           <monitors>1</monitors> \
                                         </display>");
        g_string_append_printf(vms_xml, "</vm>");
    }
    g_string_append(vms_xml, "</vms>");

    govirt_mock_httpd_add_request(httpd, "GET", "/ovirt-engine/api/vms", vms_xml->str);
    g_string_free(vms_xml, TRUE);

    /* Create individual /api/vms/$uuid entry points */
    for (i = 0; i < count; i++) {
        MockOvirtVm *vm = &vms[i];
        char *href = g_strdup_printf("/ovirt-engine/api/vms/%s", vm->uuid);
        if (vm->xml != NULL) {
            govirt_mock_httpd_add_request(httpd, "GET", href, vm->xml);
        } else if (vm->filename != NULL) {
            char *body = NULL;
            char *path = NULL;
            gboolean ret;
            path = g_build_filename(srcdir, "mock-xml-data", vm->filename, NULL);
            ret = g_file_get_contents(path, &body, NULL, NULL);
            g_clear_pointer(&path, g_free);
            if (!ret) {
                g_warn_if_reached();
                continue;
            }

            govirt_mock_httpd_add_request(httpd, "GET", href, body);
            g_clear_pointer(&body, g_free);
        }
        g_clear_pointer(&href, g_free);
    }
}


static void test_govirt_parse_vm_host_cluster(void)
{
    OvirtProxy *proxy;
    OvirtApi *api;
    OvirtCollection *vms;
    OvirtResource *vm;
    GError *error = NULL;
    GovirtMockHttpd *httpd;
    OvirtHost *host;
    OvirtCluster *cluster;
    char *guid;
    MockOvirtVm mock_vm = { .uuid = "00000000-0000-0000-0000-000000000000",
                            .name = "vm1",
                            .filename="test-parse-vm-host-cluster.xml" };

    httpd = govirt_mock_httpd_new(GOVIRT_HTTPS_PORT);
    govirt_mock_httpd_add_vms(httpd, &mock_vm, 1);
    govirt_mock_httpd_add_request(httpd, "GET", "/ovirt-engine/api/clusters/00000000-0000-0000-0001-000000000000",
                                  "<cluster href=\"/ovirt-engine/api/clusters/00000000-0000-0000-0001-000000000000\" id=\"00000000-0000-0000-0001-000000000000\"/>");
    govirt_mock_httpd_start(httpd);

    proxy = ovirt_proxy_new("localhost:" G_STRINGIFY(GOVIRT_HTTPS_PORT));
    ovirt_proxy_set_mock_ca(proxy);
    api = ovirt_proxy_fetch_api(proxy, &error);
    g_test_assert_expected_messages();
    g_assert_nonnull(api);
    g_assert_no_error(error);

    vms = ovirt_api_get_vms(api);
    ovirt_collection_fetch(vms, proxy, &error);
    g_assert_no_error(error);

    vm = ovirt_collection_lookup_resource(vms, "vm1");
    g_assert_nonnull(vm);
    ovirt_resource_refresh(vm, proxy, &error);
    g_assert_no_error(error);

    check_vm_display(OVIRT_VM(vm));

    host = ovirt_vm_get_host(OVIRT_VM(vm));
    g_assert_nonnull(host);
    g_object_get(G_OBJECT(host), "guid", &guid, NULL);
    g_assert_cmpstr(guid, ==, "00000000-0000-0000-0002-000000000000");
    g_free(guid);
    g_object_unref(host);

    cluster = ovirt_vm_get_cluster(OVIRT_VM(vm));
    g_assert_nonnull(cluster);
    g_object_get(G_OBJECT(cluster), "guid", &guid, NULL);
    g_assert_cmpstr(guid, ==, "00000000-0000-0000-0001-000000000000");
    g_free(guid);
    ovirt_resource_refresh(OVIRT_RESOURCE(cluster), proxy, &error);
    g_assert_no_error(error);
    g_object_unref(cluster);

    g_object_unref(vm);
    g_object_unref(proxy);

    govirt_mock_httpd_stop(httpd);
}


static void test_govirt_list_duplicate_vms(void)
{
    OvirtProxy *proxy;
    OvirtApi *api;
    OvirtCollection *vms;
    OvirtResource *vm;
    GError *error = NULL;
    GovirtMockHttpd *httpd;

    const char *vms_body = "<vms> \
                              <vm href=\"/ovirt-engine/api/vms/uuid0\" id=\"uuid0\"> \
                                <name>vm0</name> \
                                <display> \
                                  <type>spice</type> \
                                  <monitors>1</monitors> \
                                </display> \
                              </vm> \
                              <vm href=\"/ovirt-engine/api/vms/uuid1\" id=\"uuid1\"> \
                                <name>vm0</name> \
                                <display> \
                                  <type>spice</type> \
                                  <monitors>1</monitors> \
                                </display> \
                              </vm> \
                            </vms>";

    httpd = govirt_mock_httpd_new(GOVIRT_HTTPS_PORT);
    govirt_mock_httpd_add_request(httpd, "GET", "/ovirt-engine/api",
                                  "<api><link href=\"/ovirt-engine/api/vms\" rel=\"vms\"/></api>");
    govirt_mock_httpd_add_request(httpd, "GET", "/ovirt-engine/api/vms", vms_body);
    govirt_mock_httpd_start(httpd);

    proxy = ovirt_proxy_new("localhost:" G_STRINGIFY(GOVIRT_HTTPS_PORT));
    ovirt_proxy_set_mock_ca(proxy);
    api = ovirt_proxy_fetch_api(proxy, &error);
    g_test_assert_expected_messages();
    g_assert_nonnull(api);
    g_assert_no_error(error);

    g_test_expect_message("libgovirt", G_LOG_LEVEL_MESSAGE,
                          "'vm' resource with the same name ('vm0') already exists");
    vms = ovirt_api_get_vms(api);
    ovirt_collection_fetch(vms, proxy, &error);
    g_test_assert_expected_messages();
    g_assert_no_error(error);

    vm = ovirt_collection_lookup_resource(vms, "vm0");
    g_assert_nonnull(vm);
    g_object_unref(vm);

    g_object_unref(proxy);

    govirt_mock_httpd_stop(httpd);
}

static void test_govirt_http_404(void)
{
    OvirtProxy *proxy;
    OvirtApi *api;
    OvirtCollection *vms;
    OvirtResource *vm;
    GError *error = NULL;
    GovirtMockHttpd *httpd;

    const char *vms_body = "<vms> \
                              <vm href=\"/ovirt-engine/api/vms/uuid0\" id=\"uuid0\"> \
                                <name>vm0</name> \
                                <display> \
                                  <type>spice</type> \
                                  <monitors>1</monitors> \
                                </display> \
                              </vm> \
                            </vms>";

    g_test_expect_message("libgovirt", G_LOG_LEVEL_WARNING,
                          "Error while getting collection: Not Found");

    httpd = govirt_mock_httpd_new(GOVIRT_HTTPS_PORT);
    govirt_mock_httpd_start(httpd);


    proxy = ovirt_proxy_new("localhost:" G_STRINGIFY(GOVIRT_HTTPS_PORT));
    ovirt_proxy_set_mock_ca(proxy);
    /* warning: Value stored to 'api' is never read */
    api = ovirt_proxy_fetch_api(proxy, &error);
    g_test_assert_expected_messages();
    g_assert_error(error, REST_PROXY_ERROR, 404);
    govirt_mock_httpd_stop(httpd);
    g_object_unref(proxy);
    g_clear_error(&error);


    httpd = govirt_mock_httpd_new(GOVIRT_HTTPS_PORT);
    govirt_mock_httpd_add_request(httpd, "GET", "/ovirt-engine/api",
                                  "<api><link href=\"/ovirt-engine/api/vms\" rel=\"vms\"/></api>");
    govirt_mock_httpd_add_request(httpd, "GET", "/ovirt-engine/api/vms", vms_body);
    govirt_mock_httpd_start(httpd);

    proxy = ovirt_proxy_new("localhost:" G_STRINGIFY(GOVIRT_HTTPS_PORT));
    ovirt_proxy_set_mock_ca(proxy);
    api = ovirt_proxy_fetch_api(proxy, &error);
    g_assert_nonnull(api);
    g_assert_no_error(error);

    vms = ovirt_api_get_vms(api);
    ovirt_collection_fetch(vms, proxy, &error);
    g_test_assert_expected_messages();
    g_assert_no_error(error);

    vm = ovirt_collection_lookup_resource(vms, "vm0");
    g_assert_nonnull(vm);
    ovirt_resource_refresh(OVIRT_RESOURCE(vm), proxy, &error);
    g_assert_error(error, REST_PROXY_ERROR, 404);
    g_clear_error(&error);

    govirt_mock_httpd_stop(httpd);

    g_clear_object(&vm);
    g_clear_object(&proxy);
}

static void quit(int sig)
{
    exit(1);
}


int
main(int argc, char **argv)
{
    /* exit cleanly on ^C in case we're valgrinding. */
    signal(SIGINT, quit);
    /* prevents core generations as this could cause some issues/timeout
     * depending on system configuration */
    signal(SIGABRT, quit);

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/govirt/test-https-ca", test_govirt_https_ca);
    g_test_add_func("/govirt/test-http", test_govirt_http);
    g_test_add_func("/govirt/test-list-vms", test_govirt_list_vms);
    g_test_add_func("/govirt/test-list-duplicate-vms", test_govirt_list_duplicate_vms);
    g_test_add_func("/govirt/test-parse-vm-host-cluster", test_govirt_parse_vm_host_cluster);
    g_test_add_func("/govirt/test-404", test_govirt_http_404);

    return g_test_run();
}
