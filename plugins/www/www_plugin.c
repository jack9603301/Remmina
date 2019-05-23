/*
 * Remmina - The GTK+ Remote Desktop Client
 * Copyright (C) 2016-2019 Antenore Gatta & Giovanni Panozzo
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
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.
 * If you modify file(s) with this exception, you may extend this exception
 * to your version of the file(s), but you are not obligated to do so.
 * If you do not wish to do so, delete this exception statement from your
 * version.
 * If you delete this exception statement from all source
 * files in the program, then also delete it here.
 *
 */

#include "www_config.h"

#include "common/remmina_plugin.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>

#include <webkit2/webkit2.h>

#define GET_PLUGIN_DATA(gp) (RemminaPluginExecData*)g_object_get_data(G_OBJECT(gp), "plugin-data")

typedef struct _RemminaPluginWWWData {
	GtkWidget *box;
	WebKitSettings *settings;
	WebKitWebContext *context;
	WebKitCredential *credential;
	WebKitWebView *webview;

	gchar *url;

} RemminaPluginWWWData;

static RemminaPluginService *remmina_plugin_service = NULL;

static gboolean remmina_www_query_feature(RemminaProtocolWidget* gp, const RemminaProtocolFeature* feature)
{
	TRACE_CALL(__func__);
	return TRUE;
}

static void remmina_plugin_www_init(RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);

	RemminaPluginWWWData *gpdata;
	RemminaFile *remminafile;


	gpdata = g_new0(RemminaPluginWWWData, 1);
	g_object_set_data_full(G_OBJECT(gp), "plugin-data", gpdata, g_free);

	remminafile = remmina_plugin_service->protocol_plugin_get_file(gp);

	if (remmina_plugin_service->file_get_string(remminafile, "server")) {
		gpdata->url = strdup(remmina_plugin_service->file_get_string(remminafile, "server"));
	} else {
		gpdata->url = "https://remmina.org";
	}
	g_info ("URL is set to %s", gpdata->url);

	gpdata->settings = webkit_settings_new();
	gpdata->context = webkit_web_context_get_default ();

	/* enable-fullscreen, default TRUE, TODO: Try FALSE */

	/* user-agent. TODO: Add option. */
	/* enable-smooth-scrolling, TODO: Add option, default FALSE */
	if (remmina_plugin_service->file_get_int(remminafile, "enable-smooth-scrolling", FALSE)) {
		webkit_settings_set_enable_smooth_scrolling(gpdata->settings, TRUE);
		g_info ("enable-smooth-scrolling enabled");
	}
	/* enable-spatial-navigation, TODO: Add option, default FALSE */
	if (remmina_plugin_service->file_get_int(remminafile, "enable-spatial-navigation", FALSE)) {
		webkit_settings_set_enable_spatial_navigation(gpdata->settings, TRUE);
		g_info ("enable-spatial-navigation enabled");
	}
	/* enable-webaudio, TODO: Add option, default FALSE , experimental */
	if (remmina_plugin_service->file_get_int(remminafile, "enable-webaudio", FALSE)) {
		webkit_settings_set_enable_webaudio(gpdata->settings, TRUE);
		g_info ("enable-webaudio enabled");
	}
	/* enable-webgl. TODO: Add option, default FALSE , experimental */
	if (remmina_plugin_service->file_get_int(remminafile, "enable-webgl", FALSE)) {
		webkit_settings_set_enable_webgl(gpdata->settings, TRUE);
		g_info ("enable-webgl enabled");
	}


	if (remmina_plugin_service->file_get_int(remminafile, "ignore-tls-errors", FALSE)) {
		 webkit_web_context_set_tls_errors_policy(gpdata->context, WEBKIT_TLS_ERRORS_POLICY_IGNORE);
		g_info ("Ignore TLS errosrs");
	}

}

static gboolean remmina_plugin_www_open_connection(RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	RemminaPluginWWWData *gpdata;

	gpdata = (RemminaPluginWWWData*) g_object_get_data(G_OBJECT(gp), "plugin-data");

	gpdata->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(gp), gpdata->box);

	remmina_plugin_service->protocol_plugin_register_hostkey(gp, gpdata->box);

	gpdata->webview = WEBKIT_WEB_VIEW(webkit_web_view_new_with_settings(gpdata->settings));

	gtk_widget_set_hexpand(GTK_WIDGET(gpdata->webview), TRUE);
	gtk_widget_set_vexpand(GTK_WIDGET(gpdata->webview), TRUE);
	gtk_container_add(GTK_CONTAINER(gpdata->box), GTK_WIDGET(gpdata->webview));
	webkit_web_view_load_uri(gpdata->webview, gpdata->url);
	gtk_widget_show_all(gpdata->box);

	remmina_plugin_service->protocol_plugin_emit_signal(gp, "connect");

	return TRUE;
}

static gboolean remmina_plugin_www_close_connection(RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	remmina_plugin_service->protocol_plugin_emit_signal(gp, "disconnect");
	return FALSE;
}

/* Array of RemminaProtocolSetting for basic settings.
 * Each item is composed by:
 * a) RemminaProtocolSettingType for setting type
 * b) Setting name
 * c) Setting description
 * d) Compact disposition
 * e) Values for REMMINA_PROTOCOL_SETTING_TYPE_SELECT or REMMINA_PROTOCOL_SETTING_TYPE_COMBO
 * f) Unused pointer
 */
static const RemminaProtocolSetting remmina_plugin_www_basic_settings[] =
{
	{ REMMINA_PROTOCOL_SETTING_TYPE_TEXT, "server", N_("Address"), FALSE, NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_TEXT, "username", N_("Username"), FALSE, NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_PASSWORD, "password", N_("Password"), FALSE, NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_END, NULL, NULL, FALSE, NULL, NULL }
};

/* Array of RemminaProtocolSetting for advanced settings.
 * Each item is composed by:
 * a) RemminaProtocolSettingType for setting type
 * b) Setting name
 * c) Setting description
 * d) Compact disposition
 * e) Values for REMMINA_PROTOCOL_SETTING_TYPE_SELECT or REMMINA_PROTOCOL_SETTING_TYPE_COMBO
 * f) Unused pointer
 */
static const RemminaProtocolSetting remmina_plugin_www_advanced_settings[] =
{
	{ REMMINA_PROTOCOL_SETTING_TYPE_TEXT, "user-agent", N_("User Agent"), FALSE, NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK, "enable-smooth-scrolling", N_("Enabl smooth scrolling"), TRUE, NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK, "enable-spatial-navigation", N_("Enable Spatial Navigation"), TRUE, NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK, "enable-webgl", N_("Enable support for WebGL on pages"), TRUE, NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK, "enable-webaudio", N_("Enable support for WebAudio on pages"), TRUE, NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK, "ignore-tls-errors", N_("Ignore TLS errors"), TRUE, NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_END, NULL, NULL, FALSE, NULL, NULL }
};

/* Array for available features.
 * The last element of the array must be REMMINA_PROTOCOL_FEATURE_TYPE_END. */
static const RemminaProtocolFeature remmina_www_features[] =
{
	{ REMMINA_PROTOCOL_FEATURE_TYPE_END, 0, NULL, NULL, NULL}
};

/* Protocol plugin definition and features */
static RemminaProtocolPlugin remmina_plugin =
{
	REMMINA_PLUGIN_TYPE_PROTOCOL,         // Type
	PLUGIN_NAME,                          // Name
	PLUGIN_DESCRIPTION,                   // Description
	GETTEXT_PACKAGE,                      // Translation domain
	PLUGIN_VERSION,                       // Version number
	PLUGIN_APPICON,                       // Icon for normal connection
	NULL,                                 // Icon for SSH connection
	remmina_plugin_www_basic_settings,    // Array for basic settings
	remmina_plugin_www_advanced_settings, // Array for advanced settings
	REMMINA_PROTOCOL_SSH_SETTING_NONE,    // SSH settings type
	remmina_www_features,                 // Array for available features
	remmina_plugin_www_init,              // Plugin initialization
	remmina_plugin_www_open_connection,   // Plugin open connection
	remmina_plugin_www_close_connection,  // Plugin close connection
	remmina_www_query_feature,            // Query for available features
	NULL,                                 // Call a feature
	NULL,                                 // Send a keystroke
	NULL                                  // Capture screenshot
};

G_MODULE_EXPORT gboolean remmina_plugin_entry(RemminaPluginService *service)
{
	TRACE_CALL(__func__);
	remmina_plugin_service = service;

	bindtextdomain(GETTEXT_PACKAGE, REMMINA_RUNTIME_LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");


	if (!service->register_plugin((RemminaPlugin *) &remmina_plugin))
	{
		return FALSE;
	}
	return TRUE;
}
