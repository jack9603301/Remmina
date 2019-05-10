/*
 * Remmina - The GTK+ Remote Desktop Client
 * Copyright (C) 2016-2019 Antenore Gatta, Giovanni Panozzo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 *  In addition, as a special exception, the copyright holders give
 *  permission to link the code of portions of this program with the
 *  OpenSSL library under certain conditions as described in each
 *  individual source file, and distribute linked combinations
 *  including the two.
 *  You must obey the GNU General Public License in all respects
 *  for all of the code used other than OpenSSL. *  If you modify
 *  file(s) with this exception, you may extend this exception to your
 *  version of the file(s), but you are not obligated to do so. *  If you
 *  do not wish to do so, delete this exception statement from your
 *  version. *  If you delete this exception statement from all source
 *  files in the program, then also delete it here.
 *
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>
#include <libsoup/soup.h>

#include "remmina_pref.h"
#include "remmina_utils.h"
#include "remmina_scheduler.h"
#include "rmnews.h"
#include "remmina/remmina_trace_calls.h"

/* Timers */
#define RMNEWS_CHECK_1ST_MS 3000
#define RMNEWS_CHECK_INTERVAL_MS 12000
/* How many seconds before to get news */
#define RMNEWS_INTERVAL_SEC 604800
/* TODO: move in config.h */
#define REMMINA_URL "https://remmina.org/"
#define RMNEWS_OUTPUT "/var/tmp/latest_news.html"

static SoupSession *session;
static const gchar *rmnews_url = NULL;
static const gchar *output_file_path = NULL;

static void rmnews_show_news ()
{
	TRACE_CALL(__func__);
	g_info("Showing news");
	/* TODO: Implement a markdown renderer like Gnome ToDo
	 * https://gitlab.gnome.org/search?group_id=&project_id=121&repository_ref=master&search=markdown&search_code=true
	 */
}
static void rmnews_get_url_cb (SoupSession *session, SoupMessage *msg, gpointer data)
{
	TRACE_CALL(__func__);
	const char *name;
	const char *header;
	FILE *output_file = NULL;
	GTimeVal t;

	g_info ("Status code %d", msg->status_code);

	name = soup_message_get_uri (msg)->path;

	if (msg->status_code == SOUP_STATUS_SSL_FAILED) {
		GTlsCertificateFlags flags;

		if (soup_message_get_https_status (msg, NULL, &flags))
			g_warning ("%s: %d %s (0x%x)\n", name, msg->status_code, msg->reason_phrase, flags);
		else
			g_warning ("%s: %d %s (no handshake status)\n", name, msg->status_code, msg->reason_phrase);
	} else if (SOUP_STATUS_IS_TRANSPORT_ERROR (msg->status_code))
		g_warning ("%s: %d %s\n", name, msg->status_code, msg->reason_phrase);

	if (SOUP_STATUS_IS_REDIRECTION (msg->status_code)) {
		header = soup_message_headers_get_one (msg->response_headers,
						       "Location");
		g_warning ("Redirection detected");
		if (header) {
			SoupURI *uri;
			char *uri_string;

			g_info ("  -> %s\n", header);

			uri = soup_uri_new_with_base (soup_message_get_uri (msg), header);
			uri_string = soup_uri_to_string (uri, FALSE);
			rmnews_get_url (uri_string);
			g_free (uri_string);
			soup_uri_free (uri);
		}
	} else if (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
		g_info ("Status 200");
		if (output_file_path) {
			g_info ("Opening %s output file for writing", output_file_path);
			output_file = fopen (output_file_path, "w");
			if (!output_file)
				g_printerr ("Error trying to create file %s.\n", output_file_path);
		} else {
			g_error ("Cannot open output file for writing, because output_file_path is NULL");
		}


		if (output_file) {
			fwrite (msg->response_body->data,
				1,
				msg->response_body->length,
				output_file);

			if (output_file_path)
				fclose (output_file);
			g_get_current_time(&t);
			remmina_pref.periodic_rmnews_last_get = t.tv_sec;
			remmina_pref_save();
			rmnews_show_news();
		}
	}
	g_object_unref (msg);
}

void rmnews_get_url (const char *url)
{
	TRACE_CALL(__func__);

	SoupMessage *msg;

	msg = soup_message_new ("GET", url);
	soup_message_set_flags (msg, SOUP_MESSAGE_NO_REDIRECT);

	g_info ("Fetching %s", url);

	g_object_ref (msg);
	soup_session_queue_message (session, msg, rmnews_get_url_cb, NULL);

}

void rmnews_get_news()
{
	TRACE_CALL(__func__);

	SoupLogger *logger = NULL;

	gchar *cachedir = g_build_path("/", g_get_user_cache_dir(), REMMINA_APP_ID, NULL);
	g_mkdir_with_parents(cachedir, 0750);
	output_file_path = g_build_path("/", cachedir, "latest_news.html", NULL);

	if (output_file_path) {
		g_info ("Output file set to %s", output_file_path);
	} else {
		output_file_path = RMNEWS_OUTPUT;
		g_warning ("Output file set to %s", output_file_path);
	}

	g_info ("Gathering news");
	session = g_object_new (SOUP_TYPE_SESSION,
				SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_CONTENT_DECODER,
				SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_COOKIE_JAR,
				SOUP_SESSION_USER_AGENT, "get ",
				SOUP_SESSION_ACCEPT_LANGUAGE_AUTO, TRUE,
				NULL);
	logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, -1);
	soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
	g_object_unref (logger);

	rmnews_get_url(g_strconcat(REMMINA_URL,
				"remmina_news.",
				VERSION,
				".html",
				NULL));

	g_object_unref (session);

}

static gboolean rmnews_periodic_check(gpointer user_data)
{
	TRACE_CALL(__func__);
	GTimeVal t;
	glong next;

	next = remmina_pref.periodic_rmnews_last_get + RMNEWS_INTERVAL_SEC;
	g_get_current_time(&t);
	if (t.tv_sec > next || (t.tv_sec < remmina_pref.periodic_rmnews_last_get && t.tv_sec > 1514764800)) {
		rmnews_get_news();
	}
	return G_SOURCE_CONTINUE;
}
void rmnews_schedule()
{
	TRACE_CALL(__func__);
	remmina_scheduler_setup(rmnews_periodic_check,
			NULL,
			RMNEWS_CHECK_1ST_MS,
			RMNEWS_CHECK_INTERVAL_MS);
}
