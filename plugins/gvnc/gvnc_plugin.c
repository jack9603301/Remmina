/*
 * Remmina - The GTK+ Remote Desktop Client
 * Copyright (C) 2016-2021 Antenore Gatta, Giovanni Panozzo
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

#include "gvnc_plugin_config.h"
#include "gvnc_plugin.h"

#include <vncdisplay.h>
#include <vncutil.h>
#include <vncaudiopulse.h>

#define GVNC_DEFAULT_PORT 5900

#ifndef VNC_CHECK_VERSION
# define VNC_CHECK_VERSION(a, b, c) 0
#endif
#if VNC_CHECK_VERSION(1, 2, 0)
# define HAVE_VNC_REMOTE_RESIZE
#endif

enum {
	REMMINA_PLUGIN_GVNC_FEATURE_PREF_VIEWONLY = 1,
	REMMINA_PLUGIN_GVNC_FEATURE_DYNRESUPDATE,
	REMMINA_PLUGIN_GVNC_FEATURE_PREF_DISABLECLIPBOARD,
	REMMINA_PLUGIN_GVNC_FEATURE_TOOL_SENDCTRLALTDEL,
	REMMINA_PLUGIN_GVNC_FEATURE_TOOL_USBREDIR,
	REMMINA_PLUGIN_GVNC_FEATURE_SCALE
};


static RemminaPluginService *remmina_plugin_service = NULL;
#define REMMINA_PLUGIN_DEBUG(fmt, ...) remmina_plugin_service->_remmina_debug(__func__, fmt, ## __VA_ARGS__)

static void remmina_plugin_gvnc_mouse_grab(GtkWidget *vncdisplay, RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);
	VncGrabSequence *seq = vnc_display_get_grab_keys(VNC_DISPLAY(gpdata->vnc));
	gchar *seqstr = vnc_grab_sequence_as_string(seq);

	REMMINA_PLUGIN_DEBUG("Pointer grabbed: %s", seqstr);
}

static void remmina_plugin_gvnc_mouse_ungrab(GtkWidget *vncdisplay, RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	REMMINA_PLUGIN_DEBUG("Pointer ungrabbed");
}

static void remmina_plugin_gvnc_on_vnc_error(GtkWidget *vncdisplay G_GNUC_UNUSED, const gchar *msg, RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);

	REMMINA_PLUGIN_DEBUG("Error: %s\n", msg);
	/* "vnc-error" is always followed by "vnc-disconnected",
	 * so save the error for that signal */
	g_free(gpdata->error_msg);
	gpdata->error_msg = g_strdup(msg);
}

static gboolean remmina_plugin_gvnc_get_screenshot(RemminaProtocolWidget *gp, RemminaPluginScreenshotData *rpsd)
{
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);
	gsize szmem;
	const VncPixelFormat *currentFormat;
	GError *err = NULL;

	if (!gpdata)
		return FALSE;

	/* Get current pixel format for server */
	currentFormat = vnc_connection_get_pixel_format(gpdata->conn);


	GdkPixbuf *pix = vnc_display_get_pixbuf(VNC_DISPLAY(gpdata->vnc));

	rpsd->width = gdk_pixbuf_get_width(pix);
	rpsd->height = gdk_pixbuf_get_height(pix);
	rpsd->bitsPerPixel = currentFormat->bits_per_pixel;
	rpsd->bytesPerPixel = rpsd->bitsPerPixel / 8;
	//szmem = gdk_pixbuf_get_byte_length(pix);

	//szmem = rpsd->width * rpsd->height * rpsd->bytesPerPixel;

	//REMMINA_PLUGIN_DEBUG("allocating %zu bytes for a full screenshot", szmem);
	//REMMINA_PLUGIN_DEBUG("Calculated screenshot size: %zu", szcalc);
	//rpsd->buffer = malloc(szmem);

	//memcpy(rpsd->buffer, pix, szmem);
	gdk_pixbuf_save_to_buffer(pix, &rpsd->buffer, &szmem, "jpeg", &err, "quality", "100", NULL);


	/* Returning TRUE instruct also the caller to deallocate rpsd->buffer */
	return TRUE;
}

void remmina_plugin_gvnc_paste_text(RemminaProtocolWidget *gp, const gchar *text)
{
	TRACE_CALL(__func__);
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);
	gchar *out;
	gsize a, b;
	GError *error = NULL;

	if (!gpdata) return;

	out = g_convert_with_fallback(text, -1, "iso8859-1//TRANSLIT", "utf-8", NULL, &a, &b, &error);
	if (out) {
		REMMINA_PLUGIN_DEBUG("Pasting text");
		vnc_display_client_cut_text(VNC_DISPLAY(gpdata->vnc), out);
		g_free(out);
	} else {
		REMMINA_PLUGIN_DEBUG("Error pasting text: %s", error->message);
		g_error_free(error);
	}
}

static void remmina_plugin_gvnc_clipboard_cb(GtkClipboard *cb, GdkEvent *event, RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);
	gchar *text;

	REMMINA_PLUGIN_DEBUG("owner-change event received");

	if (cb && gtk_clipboard_get_owner(cb) == (GObject *)gp)
		return;

	text = gtk_clipboard_wait_for_text(cb);
	if (!text)
		return;

	remmina_plugin_gvnc_paste_text(gp, text);
	g_free(text);
}


/* text was actually requested */
static void remmina_plugin_gvnc_clipboard_copy(GtkClipboard *clipboard G_GNUC_UNUSED, GtkSelectionData *data, guint info G_GNUC_UNUSED, RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);

	if (!gpdata) return;
	gtk_selection_data_set_text(data, gpdata->clipstr, -1);
	REMMINA_PLUGIN_DEBUG("Text copied");
}

static void remmina_plugin_gvnc_cut_text(VncDisplay *vnc G_GNUC_UNUSED, const gchar *text, RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);

	REMMINA_PLUGIN_DEBUG("Got clipboard request for \"%s\"", text);

	GtkClipboard *cb;
	gsize a, b;
	GtkTargetEntry targets[] = {
		{ g_strdup("UTF8_STRING"),   0, 0 },
		{ g_strdup("COMPOUND_TEXT"), 0, 0 },
		{ g_strdup("TEXT"),	     0, 0 },
		{ g_strdup("STRING"),	     0, 0 },
	};

	if (!text)
		return;
	g_free(gpdata->clipstr);
	gpdata->clipstr = g_convert(text, -1, "utf-8", "iso8859-1", &a, &b, NULL);

	if (gpdata->clipstr) {
		cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

		REMMINA_PLUGIN_DEBUG("setting clipboard with owner to owner %p", gp);
		gtk_clipboard_set_with_owner(cb,
					     targets,
					     G_N_ELEMENTS(targets),
					     (GtkClipboardGetFunc)remmina_plugin_gvnc_clipboard_copy,
					     NULL,
					     G_OBJECT(gp));
	}

	//g_signal_emit_by_name(session, "session-cut-text", text);
}


static void remmina_plugin_gvnc_desktop_resize(GtkWidget *vncdisplay G_GNUC_UNUSED, int width, int height)
{
	TRACE_CALL(__func__);
	REMMINA_PLUGIN_DEBUG("Remote desktop size changed to %dx%d\n", width, height);
}

static void remmina_plugin_gvnc_on_bell(RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	REMMINA_PLUGIN_DEBUG("Bell message received");
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);
	RemminaFile *remminafile = remmina_plugin_service->protocol_plugin_get_file(gp);

	if (remmina_plugin_service->file_get_int(remminafile, "disableserverbell", FALSE))
		return;
	GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(gp));

	if (window)
		gdk_window_beep(window);
	REMMINA_PLUGIN_DEBUG("Beep emitted");
}
static void remmina_plugin_gvnc_update_scale_mode(RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);

	gint width, height;
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);
	RemminaScaleMode scaleMode = remmina_plugin_service->remmina_protocol_widget_get_current_scale_mode(gp);

#if 0
	g_object_set(gpdata->display,
		     "scaling", (scaleMode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_SCALED),
		     "resize-guest", (scaleMode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_DYNRES),
		     NULL);
#endif
	if (scaleMode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_SCALED || scaleMode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_DYNRES)
		vnc_display_set_scaling(VNC_DISPLAY(gpdata->vnc), TRUE);
	else
		vnc_display_set_scaling(VNC_DISPLAY(gpdata->vnc), FALSE);

	width = remmina_plugin_service->protocol_plugin_get_width(gp);
	height = remmina_plugin_service->protocol_plugin_get_height(gp);

	if (scaleMode != REMMINA_PROTOCOL_WIDGET_SCALE_MODE_NONE) {
		/* In scaled mode, the VncDisplay will get its dimensions from its parent */
		gtk_widget_set_size_request(GTK_WIDGET(gpdata->vnc), -1, -1);
	} else {
		/* In non scaled mode, the plugins forces dimensions of the VncDisplay */
#if 0
		g_object_get(gpdata->display_channel,
			     "width", &width,
			     "height", &height,
			     NULL);
		gtk_widget_set_size_request(GTK_WIDGET(gpdata->vnc), width, height);
#endif
		gtk_widget_set_size_request(GTK_WIDGET(gpdata->vnc), width, height);
	}
	remmina_plugin_service->protocol_plugin_update_align(gp);
}

static gboolean remmina_plugin_gvnc_query_feature(RemminaProtocolWidget *gp, const RemminaProtocolFeature *feature)
{
	TRACE_CALL(__func__);

	return TRUE;
}

static void remmina_plugin_gvnc_call_feature(RemminaProtocolWidget *gp, const RemminaProtocolFeature *feature)
{
	TRACE_CALL(__func__);

	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);
	RemminaFile *remminafile = remmina_plugin_service->protocol_plugin_get_file(gp);

	switch (feature->id) {
	case REMMINA_PLUGIN_GVNC_FEATURE_DYNRESUPDATE:
	case REMMINA_PLUGIN_GVNC_FEATURE_SCALE:
		remmina_plugin_gvnc_update_scale_mode(gp);
		break;
	default:
		break;
	}
}

static void remmina_plugin_gvnc_auth_unsupported(VncDisplay *vnc G_GNUC_UNUSED, unsigned int authType, RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);

	g_clear_pointer(&gpdata->error_msg, g_free);
	gchar *msg = g_strdup_printf(_("Unsupported authentication type %u"), authType);

	remmina_plugin_service->protocol_plugin_set_error(gp, "%s", msg);
	g_free(msg);
}

static void remmina_plugin_gvnc_auth_failure(VncDisplay *vnc G_GNUC_UNUSED, const gchar *reason, RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);

	g_clear_pointer(&gpdata->error_msg, g_free);
	gchar *msg = g_strdup_printf(_("Authentication failure: %s"), reason);

	remmina_plugin_service->protocol_plugin_set_error(gp, "%s", msg);
	g_free(msg);
}

static gboolean remmina_plugin_gvnc_ask_auth(GtkWidget *vncdisplay, GValueArray *credList, RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);

	gint ret;
	gboolean disablepasswordstoring;
	gchar *s_username = NULL, *s_password = NULL;
	gboolean wantPassword = FALSE, wantUsername = FALSE;
	int i;
	gboolean save;


	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);
	RemminaFile *remminafile = remmina_plugin_service->protocol_plugin_get_file(gp);


	REMMINA_PLUGIN_DEBUG("Got credential request for %u credential(s)\n", credList->n_values);

	for (i = 0; i < credList->n_values; i++) {
		GValue *cred = g_value_array_get_nth(credList, i);
		switch (g_value_get_enum(cred)) {
		case VNC_DISPLAY_CREDENTIAL_USERNAME:
			wantUsername = TRUE;
			break;
		case VNC_DISPLAY_CREDENTIAL_PASSWORD:
			wantPassword = TRUE;
			break;
		case VNC_DISPLAY_CREDENTIAL_CLIENTNAME:
			break;
		default:
			REMMINA_PLUGIN_DEBUG("Unsupported credential type %d", g_value_get_enum(cred));
			vnc_display_close(VNC_DISPLAY(gpdata->vnc));
			goto cleanup;
		}
	}
	disablepasswordstoring = remmina_plugin_service->file_get_int(remminafile, "disablepasswordstoring", FALSE);

	ret = remmina_plugin_service->protocol_plugin_init_auth(gp,
								(disablepasswordstoring ? 0 : REMMINA_MESSAGE_PANEL_FLAG_SAVEPASSWORD)
								| (wantUsername ? REMMINA_MESSAGE_PANEL_FLAG_USERNAME : 0),
								_("Enter VNC authentication credentials"),
								(wantUsername ? remmina_plugin_service->file_get_string(remminafile, "username") : NULL),
								(wantPassword ? remmina_plugin_service->file_get_string(remminafile, "password") : NULL),
								NULL,
								NULL);
	if (ret == GTK_RESPONSE_OK) {
		s_username = remmina_plugin_service->protocol_plugin_init_get_username(gp);
		s_password = remmina_plugin_service->protocol_plugin_init_get_password(gp);
		remmina_plugin_service->file_set_string(remminafile, "username", s_username);

		save = remmina_plugin_service->protocol_plugin_init_get_savepassword(gp);
		if (save) {
			// User has requested to save credentials. We put the password
			// into remminafile->settings. It will be saved later, on successful connection, by
			// rcw.c
			remmina_plugin_service->file_set_string(remminafile, "password", s_password);
		} else {
			remmina_plugin_service->file_set_string(remminafile, "password", NULL);
		}

		for (i = 0; i < credList->n_values; i++) {
			GValue *cred = g_value_array_get_nth(credList, i);
			switch (g_value_get_enum(cred)) {
			case VNC_DISPLAY_CREDENTIAL_USERNAME:
				if (!s_username ||
				    vnc_display_set_credential(VNC_DISPLAY(gpdata->vnc),
							       g_value_get_enum(cred),
							       s_username)) {
					g_debug("Failed to set credential type %d", g_value_get_enum(cred));
					vnc_display_close(VNC_DISPLAY(gpdata->vnc));
				}
				break;
			case VNC_DISPLAY_CREDENTIAL_PASSWORD:
				if (!s_password ||
				    vnc_display_set_credential(VNC_DISPLAY(gpdata->vnc),
							       g_value_get_enum(cred),
							       s_password)) {
					g_debug("Failed to set credential type %d", g_value_get_enum(cred));
					vnc_display_close(VNC_DISPLAY(gpdata->vnc));
				}
				break;
			case VNC_DISPLAY_CREDENTIAL_CLIENTNAME:
				if (vnc_display_set_credential(VNC_DISPLAY(gpdata->vnc),
							       g_value_get_enum(cred),
							       "remmina")) {
					g_debug("Failed to set credential type %d", g_value_get_enum(cred));
					vnc_display_close(VNC_DISPLAY(gpdata->vnc));
				}
				break;
			default:
				g_debug("Unsupported credential type %d", g_value_get_enum(cred));
				vnc_display_close(VNC_DISPLAY(gpdata->vnc));
			}
		}

		if (s_username) g_free(s_username);
		if (s_password) g_free(s_password);

		return TRUE;
	} else {
		return FALSE;
	}

cleanup:
	g_free(s_username);
	g_free(s_password);

	//g_object_set(gpdata->session, "password", s_password, NULL);
	return TRUE;
}

static void remmina_plugin_gvnc_initialized(GtkWidget *vncdisplay, RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);

	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);
	RemminaFile *remminafile = remmina_plugin_service->protocol_plugin_get_file(gp);

	REMMINA_PLUGIN_DEBUG("Connection initialized");

	VncAudioFormat format = {
		VNC_AUDIO_FORMAT_RAW_S32,
		2,
		44100,
	};

	gpdata->conn = vnc_display_get_connection(VNC_DISPLAY(gpdata->vnc));

	if (remmina_plugin_service->file_get_int(remminafile, "enableaudio", FALSE)) {
		vnc_connection_set_audio_format(gpdata->conn, &format);
		vnc_connection_set_audio(gpdata->conn, VNC_AUDIO(gpdata->pa));
		vnc_connection_audio_enable(gpdata->conn);
	}
}

static void remmina_plugin_gvnc_disconnected(VncDisplay *vnc G_GNUC_UNUSED, RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);

	REMMINA_PLUGIN_DEBUG("[%s] Plugin disconnected", PLUGIN_NAME);
}
static gboolean remmina_plugin_gvnc_close_connection(RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);

	REMMINA_PLUGIN_DEBUG("[%s] Plugin close connection", PLUGIN_NAME);

	if (gpdata) {
		if (gpdata->error_msg) g_free(gpdata->error_msg);
		if (gpdata->vnc)
			vnc_display_close(VNC_DISPLAY(gpdata->vnc));
			//g_object_unref(gpdata->vnc);
	}

	/* Remove instance->context from gp object data to avoid double free */
	g_object_steal_data(G_OBJECT(gp), "plugin-data");
	remmina_plugin_service->protocol_plugin_signal_connection_closed(gp);
	return FALSE;
}

static void remmina_plugin_gvnc_init(RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	RemminaPluginGVncData *gpdata;
	//VncGrabSequence *seq;

	GtkClipboard *cb;

	gpdata = g_new0(RemminaPluginGVncData, 1);
	g_object_set_data_full(G_OBJECT(gp), "plugin-data", gpdata, g_free);

	gpdata->pa = NULL;

	REMMINA_PLUGIN_DEBUG("[%s] Plugin init", PLUGIN_NAME);

	RemminaFile *remminafile = remmina_plugin_service->protocol_plugin_get_file(gp);

	if (remmina_plugin_service->file_get_int(remminafile, "gvncdebug", FALSE))
		vnc_util_set_debug(TRUE);

	gpdata->vnc = vnc_display_new();
	if (remmina_plugin_service->file_get_int(remminafile, "enableaudio", FALSE))
		gpdata->pa = vnc_audio_pulse_new();


	g_signal_connect(gpdata->vnc, "vnc-auth-credential",
			 G_CALLBACK(remmina_plugin_gvnc_ask_auth), gp);
	g_signal_connect(gpdata->vnc, "vnc-auth-failure",
			 G_CALLBACK(remmina_plugin_gvnc_auth_failure), gp);
	g_signal_connect(gpdata->vnc, "vnc-auth-unsupported",
			 G_CALLBACK(remmina_plugin_gvnc_auth_unsupported), gp);
	g_signal_connect(gpdata->vnc, "vnc-disconnected",
			 G_CALLBACK(remmina_plugin_gvnc_disconnected), gp);
	g_signal_connect(gpdata->vnc, "vnc-initialized",
			 G_CALLBACK(remmina_plugin_gvnc_initialized), gp);
	g_signal_connect(gpdata->vnc, "vnc-desktop-resize",
			 G_CALLBACK(remmina_plugin_gvnc_desktop_resize), NULL);
	g_signal_connect(gpdata->vnc, "vnc-bell",
			 G_CALLBACK(remmina_plugin_gvnc_on_bell), gp);
	g_signal_connect(gpdata->vnc, "vnc-error",
			 G_CALLBACK(remmina_plugin_gvnc_on_vnc_error), gp);
	g_signal_connect(gpdata->vnc, "vnc-pointer-grab",
			 G_CALLBACK(remmina_plugin_gvnc_mouse_grab), gp);
	g_signal_connect(gpdata->vnc, "vnc-pointer-ungrab",
			 G_CALLBACK(remmina_plugin_gvnc_mouse_ungrab), gp);
	g_signal_connect(gpdata->vnc, "vnc-server-cut-text",
			 G_CALLBACK(remmina_plugin_gvnc_cut_text), gp);
	//seq = vnc_grab_sequence_new_from_string ("Control_R");
	//vnc_display_set_grab_keys(VNC_DISPLAY(gpdata->vnc), seq);
	//vnc_grab_sequence_free(seq);

	/* Setup the clipboard */
	cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gpdata->signal_clipboard = g_signal_connect(cb,
						    "owner-change",
						    G_CALLBACK(remmina_plugin_gvnc_clipboard_cb),
						    gp);
}

static gboolean remmina_plugin_gvnc_open_connection(RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);

	gint port;
	gchar *host, *tunnel;
	gint opt_zoom = 100;
	RemminaPluginGVncData *gpdata = GET_PLUGIN_DATA(gp);
	RemminaFile *remminafile = remmina_plugin_service->protocol_plugin_get_file(gp);

	REMMINA_PLUGIN_DEBUG("[%s] Plugin open connection", PLUGIN_NAME);

	gpdata->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(gp), gpdata->box);
	gtk_widget_set_hexpand(GTK_WIDGET(gpdata->vnc), TRUE);
	gtk_widget_set_vexpand(GTK_WIDGET(gpdata->vnc), TRUE);
	gtk_container_add(GTK_CONTAINER(gpdata->box), gpdata->vnc);

	remmina_plugin_service->protocol_plugin_register_hostkey(gp, gpdata->vnc);


	/* Setup SSH tunnel if needed */
	tunnel = remmina_plugin_service->protocol_plugin_start_direct_tunnel(gp, GVNC_DEFAULT_PORT, FALSE);

	if (!tunnel)
		return FALSE;

	remmina_plugin_service->get_server_port(tunnel,
						GVNC_DEFAULT_PORT,
						&host,
						&port);

	gpdata->depth_profile = remmina_plugin_service->file_get_int(remminafile, "depth_profile", 24);
	vnc_display_set_depth(VNC_DISPLAY(gpdata->vnc), gpdata->depth_profile);
	vnc_display_open_host(VNC_DISPLAY(gpdata->vnc), host, g_strdup_printf("%d", port));
	gpdata->lossy_encoding = remmina_plugin_service->file_get_int(remminafile, "lossy_encoding", FALSE);
	vnc_display_set_lossy_encoding(VNC_DISPLAY(gpdata->vnc), gpdata->shared);
	vnc_display_set_shared_flag (VNC_DISPLAY(gpdata->vnc), gpdata->shared);

	g_free(host);
	g_free(tunnel);

	/* TRUE Conflict with remmina? */
	vnc_display_set_keyboard_grab(VNC_DISPLAY(gpdata->vnc), FALSE);
	/* TRUE Conflict with remmina? */
	vnc_display_set_pointer_grab(VNC_DISPLAY(gpdata->vnc), FALSE);
	vnc_display_set_pointer_local(VNC_DISPLAY(gpdata->vnc), TRUE);

	vnc_display_set_force_size(VNC_DISPLAY(gpdata->vnc), FALSE);
	vnc_display_set_scaling(VNC_DISPLAY(gpdata->vnc), TRUE);
#ifdef HAVE_VNC_REMOTE_RESIZE
	vnc_display_set_allow_resize(VNC_DISPLAY(gpdata->vnc), TRUE);
	vnc_display_set_zoom_level(VNC_DISPLAY(gpdata->vnc), opt_zoom);
#endif
	remmina_plugin_service->protocol_plugin_signal_connection_opened(gp);
	gtk_widget_show_all(gpdata->box);
	return TRUE;
}

/* Array of key/value pairs for color depths */
static gpointer colordepth_list[] =
{
	"0", N_("Use server settings"),
	"1", N_("True colour (24 bits)"),
	"2", N_("High colour (16 bits)"),
	"3", N_("Low colour (8 bits)"),
	"4", N_("Ultra low colour (3 bits)"),
	NULL
};
/* Array of RemminaProtocolSetting for basic settings.
 * Each item is composed by:
 * a) RemminaProtocolSettingType for setting type
 * b) Setting name
 * c) Setting description
 * d) Compact disposition
 * e) Values for REMMINA_PROTOCOL_SETTING_TYPE_SELECT or REMMINA_PROTOCOL_SETTING_TYPE_COMBO
 * f) Setting Tooltip
 */
static const RemminaProtocolSetting remmina_plugin_gvnc_basic_settings[] =
{
	{ REMMINA_PROTOCOL_SETTING_TYPE_SERVER,	  "server",	    NULL,			FALSE, NULL,		NULL					     },
	{ REMMINA_PROTOCOL_SETTING_TYPE_PASSWORD, "password",	    N_("VNC password"),		FALSE, NULL,		NULL					     },
	{ REMMINA_PROTOCOL_SETTING_TYPE_SELECT,	  "depth_profile",  N_("Colour depth"),		FALSE, colordepth_list, NULL					     },
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK,	  "lossy_encoding", N_("Use JPEG Compression"), TRUE,  NULL,		N_("This might not work on all VNC servers") },
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK,	  "gvncdebug",	    N_("Enable GTK-VNC debug"), FALSE,  NULL,		NULL					     },
	{ REMMINA_PROTOCOL_SETTING_TYPE_END,	  NULL,		    NULL,			FALSE, NULL,		NULL					     }
};

/* Array of RemminaProtocolSetting for advanced settings.
 * Each item is composed by:
 * a) RemminaProtocolSettingType for setting type
 * b) Setting name
 * c) Setting description
 * d) Compact disposition
 * e) Values for REMMINA_PROTOCOL_SETTING_TYPE_SELECT or REMMINA_PROTOCOL_SETTING_TYPE_COMBO
 * f) Setting Tooltip
 */
static const RemminaProtocolSetting remmina_plugin_gvnc_advanced_settings[] =
{
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK, "disableclipboard",	 N_("No clipboard sync"),	    TRUE,  NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK, "disablepasswordstoring", N_("Forget passwords after use"),  FALSE, NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK, "disableserverbell",	 N_("Ignore remote bell messages"), TRUE,  NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK, "enableaudio",		 N_("Enable audio channel"),	    FALSE, NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK, "viewonly",		 N_("View only"),		    TRUE,  NULL, NULL },
	{ REMMINA_PROTOCOL_SETTING_TYPE_CHECK, "shared",		 N_("Shared connection"),	    TRUE,  NULL, N_("If the server should try to share the desktop by leaving other clients connected") },
	{ REMMINA_PROTOCOL_SETTING_TYPE_END,   NULL,			 NULL,				    TRUE,  NULL, NULL }
};

/* Array for available features.
 * The last element of the array must be REMMINA_PROTOCOL_FEATURE_TYPE_END. */
static const RemminaProtocolFeature remmina_plugin_gvnc_features[] =
{
	{ REMMINA_PROTOCOL_FEATURE_TYPE_PREF,	      REMMINA_PLUGIN_GVNC_FEATURE_PREF_VIEWONLY,	 GINT_TO_POINTER(REMMINA_PROTOCOL_FEATURE_PREF_CHECK), "viewonly",	   N_("View only")	   },
	{ REMMINA_PROTOCOL_FEATURE_TYPE_PREF,	      REMMINA_PLUGIN_GVNC_FEATURE_PREF_DISABLECLIPBOARD, GINT_TO_POINTER(REMMINA_PROTOCOL_FEATURE_PREF_CHECK), "disableclipboard", N_("No clipboard sync") },
	{ REMMINA_PROTOCOL_FEATURE_TYPE_TOOL,	      REMMINA_PLUGIN_GVNC_FEATURE_TOOL_SENDCTRLALTDEL,	 N_("Send Ctrl+Alt+Delete"),			       NULL,		   NULL			   },
	{ REMMINA_PROTOCOL_FEATURE_TYPE_TOOL,	      REMMINA_PLUGIN_GVNC_FEATURE_TOOL_USBREDIR,	 N_("Select USB devices for redirection"),	       NULL,		   NULL			   },
	{ REMMINA_PROTOCOL_FEATURE_TYPE_DYNRESUPDATE, REMMINA_PLUGIN_GVNC_FEATURE_DYNRESUPDATE,		 NULL,						       NULL,		   NULL			   },
	{ REMMINA_PROTOCOL_FEATURE_TYPE_SCALE,	      REMMINA_PLUGIN_GVNC_FEATURE_SCALE,		 NULL,						       NULL,		   NULL			   },
	{ REMMINA_PROTOCOL_FEATURE_TYPE_END,	      0,						 NULL,						       NULL,		   NULL			   }
};

/* Protocol plugin definition and features */
static RemminaProtocolPlugin remmina_plugin = {
	REMMINA_PLUGIN_TYPE_PROTOCOL,                   // Type
	PLUGIN_NAME,                                    // Name
	PLUGIN_DESCRIPTION,                             // Description
	GETTEXT_PACKAGE,                                // Translation domain
	PLUGIN_VERSION,                                 // Version number
	PLUGIN_APPICON,                                 // Icon for normal connection
	PLUGIN_APPICON,                                 // Icon for SSH connection
	remmina_plugin_gvnc_basic_settings,             // Array for basic settings
	remmina_plugin_gvnc_advanced_settings,          // Array for advanced settings
	REMMINA_PROTOCOL_SSH_SETTING_TUNNEL,            // SSH settings type
	remmina_plugin_gvnc_features,                   // Array for available features
	remmina_plugin_gvnc_init,                       // Plugin initialization
	remmina_plugin_gvnc_open_connection,            // Plugin open connection
	remmina_plugin_gvnc_close_connection,           // Plugin close connection
	remmina_plugin_gvnc_query_feature,              // Query for available features
	remmina_plugin_gvnc_call_feature,               // Call a feature
	NULL,                                           // Send a keystroke
	NULL,                                           // No screenshot support available
	//remmina_plugin_gvnc_get_screenshot,             // No screenshot support available
	NULL,                                           // RCW map event
	NULL                                            // RCW unmap event
};

G_MODULE_EXPORT gboolean remmina_plugin_entry(RemminaPluginService *service)
{
	TRACE_CALL(__func__);
	remmina_plugin_service = service;

	bindtextdomain(GETTEXT_PACKAGE, REMMINA_RUNTIME_LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

	if (!service->register_plugin((RemminaPlugin *)&remmina_plugin))
		return FALSE;

	return TRUE;
}
