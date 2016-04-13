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
#ifndef __GOVIRT_MOCK_HTTPD__
#define __GOVIRT_MOCK_HTTPD__

G_BEGIN_DECLS

typedef struct GovirtMockHttpd GovirtMockHttpd;

GovirtMockHttpd *govirt_mock_httpd_new (guint port);
void govirt_mock_httpd_start (GovirtMockHttpd *mock_httpd);
void govirt_mock_httpd_stop (GovirtMockHttpd *mock_httpd);
void govirt_mock_httpd_disable_tls (GovirtMockHttpd *mock_httpd, gboolean disable_tls);
void govirt_mock_httpd_add_request (GovirtMockHttpd *mock_httpd, const char *method,
                                    const char *path, const char *content);

G_END_DECLS

#endif /* __GOVIRT_MOCK_HTTPD__ */
