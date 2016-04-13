/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2003, Ximian, Inc.
 */
#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <libsoup/soup.h>
#include <glib/gstdio.h>
#include <govirt/govirt.h>

#include "mock-httpd.h"

#define GOVIRT_HTTPS_PORT 8088

struct GovirtMockHttpd {
    GThread *thread;
    GMainLoop *loop;

    guint port;
    gboolean disable_tls;

    GHashTable *requests;
};


typedef struct {
    char *method;
    char *path;
    char *content;
} GovirtMockHttpdRequest;


static const char *
govirt_mock_httpd_find_request (GovirtMockHttpd *mock_httpd,
				const char *method,
				const char *path)
{
	GovirtMockHttpdRequest *request;

	request = g_hash_table_lookup (mock_httpd->requests, path);

	if (request == NULL) {
		return NULL;
	}
	if (g_strcmp0 (request->method, method) != 0) {
		return NULL;
	}

	return request->content;
}


static void
govirt_mock_htttpd_request_free (GovirtMockHttpdRequest *request)
{
	g_free (request->method);
	g_free (request->path);
	g_free (request->content);
	g_free (request);
}


static void
server_callback (SoupServer *server, SoupMessage *msg,
		 const char *path, GHashTable *query,
		 SoupClientContext *context, gpointer data)
{
	SoupMessageHeadersIter iter;
	const char *name, *value;
	const char *content;
	GovirtMockHttpd *mock_httpd = data;

	g_debug ("%s %s HTTP/1.%d\n", msg->method, path,
		 soup_message_get_http_version (msg));
	soup_message_headers_iter_init (&iter, msg->request_headers);
	while (soup_message_headers_iter_next (&iter, &name, &value))
		g_debug ("%s: %s\n", name, value);
	if (msg->request_body->length)
		g_debug ("%s\n", msg->request_body->data);

	content = govirt_mock_httpd_find_request(mock_httpd, msg->method, path);
	if (content == NULL) {
		soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
	} else {
		soup_message_body_append (msg->response_body, SOUP_MEMORY_STATIC,
					  content, strlen(content));
		soup_message_set_status (msg, SOUP_STATUS_OK);
	}
	g_debug ("  -> %d %s\n\n", msg->status_code, msg->reason_phrase);
}


static gpointer
govirt_httpd_run (gpointer user_data)
{
	GovirtMockHttpd *mock_httpd = user_data;
	GSList *uris, *u;
	SoupServer *server;
	GError *error = NULL;
	GMainContext *context;

	context = g_main_loop_get_context (mock_httpd->loop);
	g_main_context_push_thread_default (context);

	if (!mock_httpd->disable_tls) {
		GTlsCertificate *cert;

		cert = g_tls_certificate_new_from_files (abs_srcdir "/https-cert/server-cert.pem",
							 abs_srcdir "/https-cert/server-key.pem",
							 &error);
		g_assert (error == NULL);
		server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "simple-soup-httpd ",
					  SOUP_SERVER_TLS_CERTIFICATE, cert,
					  NULL);
		g_object_unref (cert);

		soup_server_listen_local (server, mock_httpd->port,
					  SOUP_SERVER_LISTEN_HTTPS, &error);
	} else {
		server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "simple-soup-httpd ",
					  NULL);
		soup_server_listen_local (server, mock_httpd->port, 0, &error);
	}
	g_assert (error == NULL);

	soup_server_add_handler (server, NULL,
				 server_callback, mock_httpd, NULL);

	uris = soup_server_get_uris (server);
	for (u = uris; u; u = u->next) {
		char *str;
		str = soup_uri_to_string (u->data, FALSE);
		g_debug ("Listening on %s\n", str);
		g_free (str);
		soup_uri_free (u->data);
	}
	g_slist_free (uris);

	g_debug ("\nWaiting for requests...\n");

	g_main_loop_run (mock_httpd->loop);
	g_main_context_pop_thread_default (context);
	g_object_unref (server);

	return NULL;
}


GovirtMockHttpd *
govirt_mock_httpd_new (guint port)
{
	GMainContext *context;
	GovirtMockHttpd *mock_httpd;

	mock_httpd = g_new0 (GovirtMockHttpd, 1);

	mock_httpd->port = port;
	context = g_main_context_new ();
	mock_httpd->loop = g_main_loop_new (context, TRUE);
	g_main_context_unref (context);

	mock_httpd->requests = g_hash_table_new_full (g_str_hash, g_str_equal,
						      NULL,
						     (GDestroyNotify) govirt_mock_htttpd_request_free);

	return mock_httpd;
}


void
govirt_mock_httpd_disable_tls (GovirtMockHttpd *mock_httpd, gboolean disable_tls)
{
	g_return_if_fail(mock_httpd->thread == NULL);

	mock_httpd->disable_tls = disable_tls;
}


void
govirt_mock_httpd_add_request (GovirtMockHttpd *mock_httpd,
			       const char *method,
			       const char *path,
			       const char *content)
{
	GovirtMockHttpdRequest *request;

	g_return_if_fail(mock_httpd->thread == NULL);

	/* FIXME: just one method is supported for a given path right now */
	request = g_hash_table_lookup (mock_httpd->requests, path);
	if (request != NULL) {
		g_assert (g_strcmp0 (request->method, method) == 0);
	}

	request = g_new0 (GovirtMockHttpdRequest, 1);
	request->method = g_strdup (method);
	request->path = g_strdup (path);
	request->content = g_strdup (content);

	g_hash_table_replace (mock_httpd->requests, request->path, request);
}


void
govirt_mock_httpd_start (GovirtMockHttpd *mock_httpd)
{
	g_return_if_fail(mock_httpd->thread == NULL);

	mock_httpd->thread = g_thread_new ("simple-soup-httpd",
					   govirt_httpd_run, mock_httpd);
}


static gboolean
govirt_mock_httpd_stop_idle (gpointer user_data)
{
	GMainLoop *loop = user_data;

	if (g_main_loop_is_running (loop)) {
		g_main_loop_quit (loop);
	}

	return G_SOURCE_REMOVE;
}


void
govirt_mock_httpd_stop (GovirtMockHttpd *mock_httpd)
{
	GMainContext *context;
	GSource *idle_source;

	context = g_main_loop_get_context (mock_httpd->loop);
	idle_source = g_idle_source_new ();
	g_source_set_callback (idle_source,
			       govirt_mock_httpd_stop_idle,
			       g_main_loop_ref (mock_httpd->loop),
			       (GDestroyNotify)g_main_loop_unref);
	g_source_attach (idle_source, context);
	g_source_unref (idle_source);

	g_thread_join (mock_httpd->thread);
	g_hash_table_unref (mock_httpd->requests);
	g_main_loop_unref (mock_httpd->loop);
	g_free (mock_httpd);
}
