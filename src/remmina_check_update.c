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

/**
 * @file remmina_check_update.c
 * @brief New Remmina version availability handling.
 * @author Antenore Gatta and Giovanni Panozzo
 * @date 15 Jav 2019
 * When Remmina starts trys to download the file http://remmina.org/latesttag.txt
 * If it succeeds, it compare it's content with the client Remmina VERSION constant.
 * If they are different, we notify the user about a new version.
 *
 * @todo choose between:
 *       - Activate button in the header bar to open URL (https://remmina.org/download ?)
 *       - Open popup with latest release note and link to download.
 *       - Open url with release note in the default browser.
 *       - ...
 */

#include "config.h"
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include "json-glib/json-glib.h"

#include "remmina_public.h"
#include "remmina_main.h"
#include "remmina/remmina_trace_calls.h"
#include "remmina_log.h"

#if !JSON_CHECK_VERSION(1, 2, 0)
	#define json_node_unref(x) json_node_free(x)
#endif

#define REMMINA_TAG_URL "https://www.remmina.org/latesttag.json"
#define REMMINA_RELEASE_URL "https://remmina.org/tags/#release"

static void remmina_check_update_dialog (const gchar *tag, const gchar *description)
{
	GtkWindow *parent;
	GtkWidget *dialog, *label, *content_area;
	GtkDialogFlags flags;
	gchar *title, *wrapper;

	title = g_strdup_printf("%s Release note", tag);
	wrapper = g_strdup_printf("%s\n\n%s\n\n<a href=\"%s\">Click here for more information</a>\n",
			title,
			description,
			REMMINA_RELEASE_URL);
	parent = remmina_main_get_window();

	// Create the widgets
	flags = GTK_DIALOG_DESTROY_WITH_PARENT;
	dialog = gtk_dialog_new_with_buttons (title, parent, flags, _("_OK"), GTK_RESPONSE_NONE, NULL);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), wrapper);

	g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);

	gtk_container_add (GTK_CONTAINER (content_area), label);
	gtk_widget_show_all (dialog);
}

static void remmina_check_update_notify(const gchar *tag, const gchar *description)
{
    TRACE_CALL(__func__);
    gchar *ver;

    ver = g_strdup_printf("v%s", VERSION);
    g_debug ("%s: client version is: %s\n", __func__, ver);
    g_debug ("%s: Git tag is: %s\n", __func__, tag);
    g_debug ("%s: Git tag description is: %s\n", __func__, description);

    if (ver != NULL && tag != NULL && (g_strcmp0(ver, tag)) != 0) {
	    g_debug ("%s: There is a new version: %s\n", __func__, tag);
	    remmina_check_update_dialog(tag, description);
    }
}

static void remmina_check_update_gettag (SoupSession *session, SoupMessage *msg, gpointer user_data)
{
	TRACE_CALL(__func__);
	SoupBuffer *sb;

	JsonParser *parser;
	JsonReader *reader;
	GError *error = NULL;
	gchar *tag;
	gchar *description;

	/* tag looks like v1.3.0 --> future v10.99.12 */

	if (msg->status_code != 200) {
		g_debug("%s, HTTP status error getting tag: %d\n",__func__, msg->status_code);
		return;
	}

	sb = soup_message_body_flatten(msg->response_body);
	g_debug("%s: %s\n", __func__, sb->data);

	parser = json_parser_new ();
	json_parser_load_from_data (parser, sb->data, -1, &error);
	if (error)
	{
		g_print ("%s: Unable to parse '%s': %s\n", __func__, REMMINA_TAG_URL, error->message);
		g_error_free (error);
		g_object_unref (parser);
		return;
	}

	reader = json_reader_new (json_parser_get_root (parser));
	json_reader_read_member (reader, "release");
	json_reader_read_member (reader, "tag_name");
	tag = g_strdup(json_reader_get_string_value (reader));
	g_debug("%s: tag %s\n", __func__, tag);
	json_reader_end_member (reader);
	json_reader_read_member (reader, "description");
	description = g_strdup(json_reader_get_string_value (reader));
	g_debug("%s: tag %s\n", __func__, description);
	json_reader_end_member (reader);

	g_object_unref (reader);
	g_object_unref (parser);

	remmina_check_update_notify(tag, description);

	g_free (tag);

	soup_buffer_free(sb);
	return;
}

static void remmina_check_update_connect(const gchar *url)
{
    TRACE_CALL(__func__);
    SoupSession *session;
    SoupMessage *msg;

    session = soup_session_new();
    msg = soup_message_new ("GET", url);
    soup_session_queue_message(session, msg, remmina_check_update_gettag, NULL);

    g_debug("%s: sending GET request to url %s\n", __func__, url);
}

void remmina_check_update_init(void)
{
    TRACE_CALL(__func__);

    g_debug("Initializing remmina_check_update\n");

    remmina_check_update_connect(REMMINA_TAG_URL);

}
