/*
 * Remmina - The GTK+ Remote Desktop Client
 * Copyright (C) 2009-2011 Vic Lee
 * Copyright (C) 2014-2015 Antenore Gatta, Fabio Castelli, Giovanni Panozzo
 * Copyright (C) 2016-2023 Antenore Gatta, Giovanni Panozzo
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

#ifdef GDK_WINDOWING_X11
#include <cairo/cairo-xlib.h>
#else
#include <cairo/cairo.h>
#endif
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "remmina.h"
#include "remmina_main.h"
#include "rcw.h"
#include "remmina_applet_menu_item.h"
#include "remmina_applet_menu.h"
#include "remmina_file.h"
#include "remmina_file_manager.h"
#include "remmina_log.h"
#include "remmina_message_panel.h"
#include "remmina_ext_exec.h"
#include "remmina_plugin_manager.h"
#include "remmina_pref.h"
#include "remmina_protocol_widget.h"
#include "remmina_public.h"
#include "remmina_scrolled_viewport.h"
#include "remmina_unlock.h"
#include "remmina_utils.h"
#include "remmina_widget_pool.h"
#include "remmina/remmina_trace_calls.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif


#define DEBUG_KB_GRABBING 0
#include "remmina_exec.h"

gchar *remmina_pref_file;
RemminaPref remmina_pref;

G_DEFINE_TYPE(RemminaConnectionWindow, rcw, GTK_TYPE_WINDOW)

#define MOTION_TIME 100

/* default timeout used to hide the floating toolbar when switching profile */
#define TB_HIDE_TIME_TIME 1500

#define FULL_SCREEN_TARGET_MONITOR_UNDEFINED -1
#define FULL_SCREEN_TARGET_MONITOR_ZERO -2

struct _RemminaConnectionWindowPriv {
	GtkNotebook *					notebook;
	GtkWidget *					floating_toolbar_widget;
	GtkWidget *					overlay;
	GtkWidget *					revealer;
	GtkWidget *					overlay_ftb_overlay;
	GtkWidget *					overlay_ftb_fr;

	GtkWidget *					floating_toolbar_label;
	gdouble						floating_toolbar_opacity;

	/* Various delayed and timer event source ids */
	guint						acs_eventsourceid;              // timeout
	guint						spf_eventsourceid;              // idle
	guint						grab_retry_eventsourceid;       // timeout
	guint						delayed_grab_eventsourceid;
	guint						ftb_hide_eventsource;           // timeout
	guint						tar_eventsource;                // timeout
	guint						hidetb_eventsource;             // timeout
	guint						dwp_eventsourceid;              // timeout

	GtkWidget *					toolbar;
	GtkWidget *					grid;

	/* Toolitems that need to be handled */
	GtkButton*					toolitem_menu;
	GtkButton *					toolitem_autofit;
	GtkButton *					toolitem_fullscreen;
	GtkButton *					toolitem_switch_page;
	GtkButton *					toolitem_dynres;
	GtkButton *					toolitem_scale;
	GtkButton *					toolitem_grab;
	GtkButton *					toolitem_multimon;
	GtkButton *					toolitem_preferences;
	GtkButton *					toolitem_tools;
	GtkButton *					toolitem_new;
	GtkButton *					toolitem_duplicate;
	GtkButton *					toolitem_screenshot;
	GtkWidget *					fullscreen_option_button;
	GtkWidget *					fullscreen_scaler_button;
	GtkWidget *					scaler_option_button;
	


	GtkWidget *					pin_button;
	gboolean					pin_down;

	gboolean					sticky;

	/* Flag to turn off toolbar signal handling when toolbar is
	 * reconfiguring, usually due to a tab switch */
	gboolean					toolbar_is_reconfiguring;

	/* This is the current view mode, i.e. VIEWPORT_FULLSCREEN_MODE,
	 * as saved on the "viwemode" profile preference file */
	gint						view_mode;

	/* Status variables used when in fullscreen mode. Needed
	 * to restore a fullscreen mode after coming from scrolled */
	gint						fss_view_mode;
	/* Status variables used when in scrolled window mode. Needed
	 * to restore a scrolled window mode after coming from fullscreen */
	gint						ss_width, ss_height;
	gboolean					ss_maximized;
	GdkMonitor*					active_monitor;

	gboolean					kbcaptured;
	gboolean					pointer_captured;
	gboolean					hostkey_activated;
	gboolean					hostkey_used;

	gboolean					pointer_entered;

	RemminaConnectionWindowOnDeleteConfirmMode	on_delete_confirm_mode;
};

typedef struct _RemminaConnectionObject {
	RemminaConnectionWindow *	cnnwin;
	RemminaFile *			remmina_file;

	GtkWidget *			proto;
	GtkWidget *			aspectframe;
	GtkWidget *			viewport;

	GtkWidget *			scrolled_container;

	gboolean			plugin_can_scale;

	gboolean			connected;
	gboolean			dynres_unlocked;
	GtkWidget *			toolbar_menu;
	GtkWidget *			preference_menu;
	GMenu *				connections_menu;
	GSimpleActionGroup*	action_group;

	gulong				deferred_open_size_allocate_handler;
} RemminaConnectionObject;

enum {
	TOOLBARPLACE_SIGNAL,
	LAST_SIGNAL
};

static guint rcw_signals[LAST_SIGNAL] =
{ 0 };

static RemminaConnectionWindow *rcw_create_scrolled(gint width, gint height, gboolean maximize);
static RemminaConnectionWindow *rcw_create_fullscreen(GtkWindow *old, gint view_mode);
static gboolean rcw_hostkey_func(RemminaProtocolWidget *gp, guint keyval, gboolean release);
static GtkWidget *rco_create_tab_page(RemminaConnectionObject *cnnobj);
static GtkWidget *rco_create_tab_label(RemminaConnectionObject *cnnobj);

void rcw_grab_focus(RemminaConnectionWindow *cnnwin);
static GtkWidget *rcw_create_toolbar(RemminaConnectionWindow *cnnwin, gint mode);
static void rcw_place_toolbar(GtkBox *toolbar, GtkGrid *grid, GtkWidget *sibling, int toolbar_placement);
static void rco_update_toolbar(RemminaConnectionObject *cnnobj);
static void rcw_keyboard_grab(RemminaConnectionWindow *cnnwin);
static void rcw_run_feature(GSimpleAction *action, GVariant *param, gpointer data);
static void rcw_toolbar_menu_on_launch_item(GSimpleAction *action, GVariant *param, gpointer data);
static void rcw_handle_keystrokes(GSimpleAction *action, GVariant *param, gpointer data);
static void rco_switch_page_activate(GSimpleAction *action, GVariant *param, gpointer data);
static GtkWidget *rcw_append_new_page(RemminaConnectionWindow *cnnwin, RemminaConnectionObject *cnnobj);
void rcw_toolbar_preferences_check(RemminaConnectionObject *cnnobj, GSimpleActionGroup* actions, const RemminaProtocolFeature *feature,
	const gchar *domain, gboolean enabled);
void rcw_toolbar_preferences_radio(RemminaConnectionObject *cnnobj, RemminaFile *remminafile,
	 GSimpleActionGroup* actions, const RemminaProtocolFeature *feature, const gchar *domain, gboolean enabled);

static GActionEntry rcw_actions[] = {
	{ "feature",	 rcw_run_feature,	 NULL, NULL, NULL },
	{ "keystrokes",	 rcw_handle_keystrokes,	 NULL, NULL, NULL },
	{ "launch",	 rcw_toolbar_menu_on_launch_item,	 NULL, NULL, NULL },
	{ "switch",	 rco_switch_page_activate,	 NULL, NULL, NULL },
};

static void rcw_ftb_drag_begin(GtkWidget *widget, GtkDragSource *context, gpointer user_data);

// static const GtkTargetEntry dnd_targets_ftb[] =
// {
// 	{
// 		(char *)"text/x-remmina-ftb",
// 		GTK_TARGET_SAME_APP | GTK_TARGET_OTHER_WIDGET,
// 		0
// 	},
// };

// static const GtkTargetEntry dnd_targets_tb[] =
// {
// 	{
// 		(char *)"text/x-remmina-tb",
// 		GTK_TARGET_SAME_APP,
// 		0
// 	},
// }; TODO GTK4 figure out TargetEntry

static void rcw_class_init(RemminaConnectionWindowClass *klass)
{
	TRACE_CALL(__func__);
	GtkCssProvider *provider;

	provider = gtk_css_provider_new();

	/* It’s important to remove padding, border and shadow from GtkViewport or
	 * we will never know its internal area size, because GtkViweport::viewport_get_view_allocation,
	 * which returns the internal size of the GtkViewport, is private and we cannot access it */

#if GTK_CHECK_VERSION(3, 14, 0)
	gtk_css_provider_load_from_data(provider,
					"#remmina-cw-viewport, #remmina-cw-aspectframe {\n"
					"  padding:0;\n"
					"  border:0;\n"
					"  background-color: black;\n"
					"}\n"
					"GtkDrawingArea {\n"
					"}\n"
					// "GtkToolbar {\n"
					// "  -GtkWidget-window-dragging: 0;\n"
					// "}\n"
					"#remmina-connection-window-fullscreen {\n"
					"  border-color: black;\n"
					"}\n"
					"#remmina-small-button {\n"
					"  outline-offset: 0;\n"
					"  outline-width: 0;\n"
					"  padding: 0;\n"
					"  border: 0;\n"
					"}\n"
					"#remmina-pin-button {\n"
					"  outline-offset: 0;\n"
					"  outline-width: 0;\n"
					"  padding: 2px;\n"
					"  border: 0;\n"
					"}\n"
					"#remmina-tab-page {\n"
					"  background-color: black;\n"
					"}\n"
					"#remmina-scrolled-container {\n"
					"}\n"
					"#remmina-scrolled-container.undershoot {\n"
					"  background: none;\n"
					"}\n"
					"#remmina-tab-page {\n"
					"}\n"
					"#ftbbox-upper {\n"
					"  background-color: white;\n"
					"  color: black;\n"
					"  border-style: none solid solid solid;\n"
					"  border-width: 1px;\n"
					"  border-radius: 4px;\n"
					"  padding: 0px;\n"
					"}\n"
					"#ftbbox-lower {\n"
					"  background-color: white;\n"
					"  color: black;\n"
					"  border-style: solid solid none solid;\n"
					"  border-width: 1px;\n"
					"  border-radius: 4px;\n"
					"  padding: 0px;\n"
					"}\n"
					"#ftb-handle {\n"
					"}\n"
					".message_panel {\n"
					"  border: 0px solid;\n"
					"  padding: 20px 20px 20px 20px;\n"
					"}\n"
					".message_panel entry {\n"
					"  background-image: none;\n"
					"  border-width: 4px;\n"
					"  border-radius: 8px;\n"
					"}\n"
					".message_panel .title_label {\n"
					"  font-size: 2em; \n"
					"}\n"
					, -1);

#else
	gtk_css_provider_load_from_data(provider,
					"#remmina-cw-viewport, #remmina-cw-aspectframe {\n"
					"  padding:0;\n"
					"  border:0;\n"
					"  background-color: black;\n"
					"}\n"
					"#remmina-cw-message-panel {\n"
					"}\n"
					"GtkDrawingArea {\n"
					"}\n"
					"GtkToolbar {\n"
					"  -GtkWidget-window-dragging: 0;\n"
					"}\n"
					"#remmina-connection-window-fullscreen {\n"
					"  border-color: black;\n"
					"}\n"
					"#remmina-small-button {\n"
					"  -GtkWidget-focus-padding: 0;\n"
					"  -GtkWidget-focus-line-width: 0;\n"
					"  padding: 0;\n"
					"  border: 0;\n"
					"}\n"
					"#remmina-pin-button {\n"
					"  -GtkWidget-focus-padding: 0;\n"
					"  -GtkWidget-focus-line-width: 0;\n"
					"  padding: 2px;\n"
					"  border: 0;\n"
					"}\n"
					"#remmina-scrolled-container {\n"
					"}\n"
					"#remmina-scrolled-container.undershoot {\n"
					"  background: none\n"
					"}\n"
					"#remmina-tab-page {\n"
					"}\n"
					"#ftbbox-upper {\n"
					"  border-style: none solid solid solid;\n"
					"  border-width: 1px;\n"
					"  border-radius: 4px;\n"
					"  padding: 0px;\n"
					"}\n"
					"#ftbbox-lower {\n"
					"  border-style: solid solid none solid;\n"
					"  border-width: 1px;\n"
					"  border-radius: 4px;\n"
					"  padding: 0px;\n"
					"}\n"
					"#ftb-handle {\n"
					"}\n"

					, -1);
#endif

	gtk_style_context_add_provider_for_display(gdk_display_get_default(),
						  GTK_STYLE_PROVIDER(provider),
						  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION); // TODO GTK4

	g_object_unref(provider);

	/* Define a signal used to notify all rcws of toolbar move */
	rcw_signals[TOOLBARPLACE_SIGNAL] = g_signal_new("toolbar-place", G_TYPE_FROM_CLASS(klass),
							G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, G_STRUCT_OFFSET(RemminaConnectionWindowClass, toolbar_place), NULL, NULL,
							g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static RemminaConnectionObject *rcw_get_cnnobj_at_page(RemminaConnectionWindow *cnnwin, gint npage)
{
	GtkWidget *po;

	if (!cnnwin->priv->notebook)
		return NULL;
	po = gtk_notebook_get_nth_page(GTK_NOTEBOOK(cnnwin->priv->notebook), npage);
	return g_object_get_data(G_OBJECT(po), "cnnobj");
}

static RemminaConnectionObject *rcw_get_visible_cnnobj(RemminaConnectionWindow *cnnwin)
{
	gint np;

	if (cnnwin != NULL && cnnwin->priv != NULL && cnnwin->priv->notebook != NULL) {
		np = gtk_notebook_get_current_page(GTK_NOTEBOOK(cnnwin->priv->notebook));
		if (np < 0)
			return NULL;
		return rcw_get_cnnobj_at_page(cnnwin, np);
	} else {
		return NULL;
	}
}

static RemminaScaleMode get_current_allowed_scale_mode(RemminaConnectionObject *cnnobj, gboolean *dynres_avail, gboolean *scale_avail)
{
	TRACE_CALL(__func__);
	RemminaScaleMode scalemode;
	gboolean plugin_has_dynres, plugin_can_scale;

	scalemode = remmina_protocol_widget_get_current_scale_mode(REMMINA_PROTOCOL_WIDGET(cnnobj->proto));

	plugin_has_dynres = remmina_protocol_widget_query_feature_by_type(REMMINA_PROTOCOL_WIDGET(cnnobj->proto),
									  REMMINA_PROTOCOL_FEATURE_TYPE_DYNRESUPDATE);

	plugin_can_scale = remmina_protocol_widget_query_feature_by_type(REMMINA_PROTOCOL_WIDGET(cnnobj->proto),
									 REMMINA_PROTOCOL_FEATURE_TYPE_SCALE);

	/* Forbid scalemode REMMINA_PROTOCOL_WIDGET_SCALE_MODE_DYNRES when not possible */
	if ((!plugin_has_dynres) && scalemode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_DYNRES)
		scalemode = REMMINA_PROTOCOL_WIDGET_SCALE_MODE_NONE;

	/* Forbid scalemode REMMINA_PROTOCOL_WIDGET_SCALE_MODE_SCALED when not possible */
	if (!plugin_can_scale && scalemode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_SCALED)
		scalemode = REMMINA_PROTOCOL_WIDGET_SCALE_MODE_NONE;

	if (scale_avail)
		*scale_avail = plugin_can_scale;
	if (dynres_avail)
		*dynres_avail = (plugin_has_dynres && cnnobj->dynres_unlocked);

	return scalemode;
}

static void rco_disconnect_current_page(RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);

	/* Disconnects the connection which is currently in view in the notebook */
	remmina_protocol_widget_close_connection(REMMINA_PROTOCOL_WIDGET(cnnobj->proto));
}

static void rcw_kp_ungrab(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);

	GtkNative* native = gtk_widget_get_native((GTK_WIDGET(cnnwin)));

	GdkSurface *surface = gtk_native_get_surface(native);
	gdk_toplevel_restore_system_shortcuts(surface);


// 	if (cnnwin->priv->grab_retry_eventsourceid) {
// 		g_source_remove(cnnwin->priv->grab_retry_eventsourceid);
// 		cnnwin->priv->grab_retry_eventsourceid = 0;
// 	}
// 	if (cnnwin->priv->delayed_grab_eventsourceid) {
// 		g_source_remove(cnnwin->priv->delayed_grab_eventsourceid);
// 		cnnwin->priv->delayed_grab_eventsourceid = 0;
// 	}


// 	if (!cnnwin->priv->kbcaptured && !cnnwin->priv->pointer_captured)
// 		return;

// #if DEBUG_KB_GRABBING
// 	printf("DEBUG_KB_GRABBING: --- ungrabbing\n");
// #endif



// #if GTK_CHECK_VERSION(3, 20, 0)
// 	/* We can use gtk_seat_grab()/_ungrab() only after GTK 3.24 */
// 	//gdk_seat_ungrab(seat);
// #else
// 	// if (keyboard != NULL) {
// 	// 	if (gdk_device_get_source(keyboard) != GDK_SOURCE_KEYBOARD)
// 	// 		keyboard = gdk_device_get_associated_device(keyboard);
// 	// 	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
// 	// 	gdk_device_ungrab(keyboard, GDK_CURRENT_TIME);
// 	// 	G_GNUC_END_IGNORE_DEPRECATIONS
// 	// }
// #endif
// 	cnnwin->priv->kbcaptured = FALSE;
// 	cnnwin->priv->pointer_captured = FALSE;
}

static gboolean rcw_keyboard_grab_retry(gpointer user_data)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindow *cnnwin = (RemminaConnectionWindow *)user_data;

#if DEBUG_KB_GRABBING
	printf("%s retry grab\n", __func__);
#endif
	rcw_keyboard_grab(cnnwin);
	cnnwin->priv->grab_retry_eventsourceid = 0;
	return G_SOURCE_REMOVE;
}


static void rcw_pointer_grab(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	/* This function in Wayland is useless and generates a spurious leave-notify event.
	 * Should we remove it ? https://gitlab.gnome.org/GNOME/mutter/-/issues/2450#note_1588081 */
// #if GTK_CHECK_VERSION(3, 20, 0)
// 	GdkSeat *seat;
// 	GdkDisplay *display;
// 	//GdkGrabStatus ggs;


// 	if (cnnwin->priv->pointer_captured) {
// #if DEBUG_KB_GRABBING
// 		printf("DEBUG_KB_GRABBING: pointer_captured is true, it should not\n");
// #endif
// 		return;
// 	}

// 	display = gtk_widget_get_display(GTK_WIDGET(cnnwin));
// 	seat = gdk_display_get_default_seat(display);
// 	ggs = gdk_seat_grab(seat, gtk_widget_get_window(GTK_WIDGET(cnnwin)),
// 			    GDK_SEAT_CAPABILITY_ALL_POINTING, TRUE, NULL, NULL, NULL, NULL);
// 	if (ggs != GDK_GRAB_SUCCESS) {
// #if DEBUG_KB_GRABBING
// 		printf("DEBUG_KB_GRABBING: GRAB of POINTER failed. GdkGrabStatus: %d\n", (int)ggs);
// #endif
// 	} else {
// 		cnnwin->priv->pointer_captured = TRUE;
// 	}

// #endif TODO GTK$
}

static void rcw_run_feature(GSimpleAction *action, GVariant *param, gpointer data){
	TRACE_CALL(__func__);
	RemminaProtocolFeature *feature;
	RemminaProtocolWidget* proto;

	if(data == NULL){
		return;
	}

	feature = (RemminaProtocolFeature *)g_object_get_data(G_OBJECT(action), "feature-type");
	proto = (RemminaProtocolWidget *)g_object_get_data(G_OBJECT(action), "proto");
	remmina_protocol_widget_call_feature_by_ref(proto, feature);
}

static void rcw_handle_keystrokes(GSimpleAction *action, GVariant *param, gpointer data){
	TRACE_CALL(__func__);
	gchar* keystrokes;

	if(data == NULL){
		return;
	}

	keystrokes = (gchar*)g_object_get_data(G_OBJECT(action), "keystrokes");
	remmina_protocol_widget_send_keystrokes(REMMINA_PROTOCOL_WIDGET(data), keystrokes);
}


void rcw_grab_notification_check(GObject* self, GParamSpec* pspec, gpointer user_data)
{
	RemminaConnectionWindow *cnnwin = (RemminaConnectionWindow *)user_data;

	if (strcmp(g_param_spec_get_name(pspec), "shortcuts-inhibited") == 0) {
		g_object_get (GDK_TOPLEVEL (self), "shortcuts-inhibited", &(cnnwin->priv->kbcaptured), NULL);
		
	}
			
}

static void rcw_keyboard_grab(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);


	if (cnnwin->priv->kbcaptured) {
#if DEBUG_KB_GRABBING
		printf("DEBUG_KB_GRABBING: %s not grabbing because already grabbed.\n", __func__);
#endif
		return;
	}

	GtkNative* native = gtk_widget_get_native((GTK_WIDGET(cnnwin)));

	GdkSurface *surface = gtk_native_get_surface(native);
	g_signal_connect(G_OBJECT(surface), "notify", G_CALLBACK(rcw_grab_notification_check), cnnwin);
	gdk_toplevel_inhibit_system_shortcuts(surface, NULL);

// #if DEBUG_KB_GRABBING
// 		printf("DEBUG_KB_GRABBING: profile asks for grabbing, let’s try.\n");
// #endif
// 		/* Up to GTK version 3.20 we can grab the keyboard with gdk_device_grab().
// 		 * in GTK 3.20 gdk_seat_grab() should be used instead of gdk_device_grab().
// 		 * There is a bug in GTK up to 3.22: When gdk_device_grab() fails
// 		 * the widget is hidden:
// 		 * https://gitlab.gnome.org/GNOME/gtk/commit/726ad5a5ae7c4f167e8dd454cd7c250821c400ab
// 		 * The bugfix will be released with GTK 3.24.
// 		 * Also please note that the newer gdk_seat_grab() is still calling gdk_device_grab().
// 		 *
// 		 * Warning: gdk_seat_grab() will call XGrabKeyboard() or XIGrabDevice()
// 		 * which in turn will generate a core X input event FocusOut and FocusIn
// 		 * but not Xinput2 events.
// 		 * In some cases, GTK is unable to neutralize FocusIn and FocusOut core
// 		 * events (ie: i3wm+Plasma with GDK_CORE_DEVICE_EVENTS=1 because detail=NotifyNonlinear
// 		 * instead of detail=NotifyAncestor/detail=NotifyInferior)
// 		 * Receiving a FocusOut event for Remmina at this time will cause an infinite loop.
// 		 * Therefore is important for GTK to use Xinput2 instead of core X events
// 		 * by unsetting GDK_CORE_DEVICE_EVENTS
// 		 */
// #if GTK_CHECK_VERSION(3, 20, 0)
// 		ggs = gdk_seat_grab(seat, gtk_widget_get_window(GTK_WIDGET(cnnwin)),
// 				    GDK_SEAT_CAPABILITY_KEYBOARD, TRUE, NULL, NULL, NULL, NULL);
// #else
// 		ggs = gdk_device_grab(keyboard, gtk_widget_get_window(GTK_WIDGET(cnnwin)), GDK_OWNERSHIP_WINDOW,
// 				      TRUE, GDK_KEY_PRESS | GDK_KEY_RELEASE, NULL, GDK_CURRENT_TIME);
// #endif
// 		if (ggs != GDK_GRAB_SUCCESS) {
// #if DEBUG_KB_GRABBING
// 			printf("GRAB of keyboard failed.\n");
// #endif
// 			/* Reschedule grabbing in half a second if not already done */
// 			if (cnnwin->priv->grab_retry_eventsourceid == 0)
// 				cnnwin->priv->grab_retry_eventsourceid = g_timeout_add(500, (GSourceFunc)rcw_keyboard_grab_retry, cnnwin);
// 		} else {
// #if DEBUG_KB_GRABBING
// 			printf("Keyboard grabbed\n");
// #endif
// 			if (cnnwin->priv->grab_retry_eventsourceid != 0) {
// 				g_source_remove(cnnwin->priv->grab_retry_eventsourceid);
// 				cnnwin->priv->grab_retry_eventsourceid = 0;
// 			}
// 			cnnwin->priv->kbcaptured = TRUE;
// 		}
// 	} else {
// 		rcw_kp_ungrab(cnnwin);
// 	} 
//TODO GTK4
}

static void rcw_close_all_connections(RemminaConnectionWindow *cnnwin)
{
	RemminaConnectionWindowPriv *priv = cnnwin->priv;
	GtkNotebook *notebook = GTK_NOTEBOOK(priv->notebook);
	GtkWidget *w;
	RemminaConnectionObject *cnnobj;
	gint i, n;

	if (GTK_IS_WIDGET(notebook)) {
		n = gtk_notebook_get_n_pages(notebook);
		for (i = n - 1; i >= 0; i--) {
			w = gtk_notebook_get_nth_page(notebook, i);
			cnnobj = (RemminaConnectionObject *)g_object_get_data(G_OBJECT(w), "cnnobj");
			/* Do close the connection on this tab */
			remmina_protocol_widget_close_connection(REMMINA_PROTOCOL_WIDGET(cnnobj->proto));
		}
	}
}

void rcw_delete_response( GtkDialog* self, gint response_id, gpointer user_data){
	gtk_window_destroy(GTK_WINDOW(self));
	if (response_id != GTK_RESPONSE_YES){
		return;
	}
	rcw_close_all_connections(user_data);
	
}

gboolean rcw_delete(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnwin->priv;
	GtkNotebook *notebook = GTK_NOTEBOOK(priv->notebook);
	GtkWidget *dialog;
	gint i, n, nopen;

	if (!REMMINA_IS_CONNECTION_WINDOW(cnnwin))
		return TRUE;

	if (cnnwin->priv->on_delete_confirm_mode != RCW_ONDELETE_NOCONFIRM) {
		n = gtk_notebook_get_n_pages(notebook);
		nopen = 0;
		/* count all non-closed connections */
		for(i = 0; i < n; i ++) {
			RemminaConnectionObject *cnnobj = rcw_get_cnnobj_at_page(cnnwin, i);
			if (!remmina_protocol_widget_is_closed((RemminaProtocolWidget *)cnnobj->proto))
				nopen ++;
		}
		if (nopen > 1) {
			dialog = gtk_message_dialog_new(GTK_WINDOW(cnnwin), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
							GTK_BUTTONS_YES_NO,
							_("Are you sure you want to close %i active connections in the current window?"), nopen);
			gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
			g_signal_connect(dialog, "response", G_CALLBACK(rcw_delete_response), cnnwin);
			gtk_widget_show(dialog);
		}
		else if (nopen == 1) {
			if (remmina_pref.confirm_close) {
				dialog = gtk_message_dialog_new(GTK_WINDOW(cnnwin), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
								GTK_BUTTONS_YES_NO,
								_("Are you sure you want to close this last active connection?"));
				gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
				g_signal_connect(dialog, "response", G_CALLBACK(rcw_delete_response), cnnwin);
				gtk_widget_show(dialog);
			}
		}
	}
	

	return TRUE;
}

static gboolean rcw_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	TRACE_CALL(__func__);
	rcw_delete(RCW(widget));
	return TRUE;
}

static void rcw_destroy(GtkWidget *widget, gpointer data)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv;
	RemminaConnectionWindow *cnnwin;

	if (!REMMINA_IS_CONNECTION_WINDOW(widget))
		return;

	cnnwin = (RemminaConnectionWindow *)widget;
	priv = cnnwin->priv;

	if (priv->kbcaptured)
		rcw_kp_ungrab(cnnwin);

	if (priv->acs_eventsourceid) {
		g_source_remove(priv->acs_eventsourceid);
		priv->acs_eventsourceid = 0;
	}
	if (priv->spf_eventsourceid) {
		g_source_remove(priv->spf_eventsourceid);
		priv->spf_eventsourceid = 0;
	}
	if (priv->grab_retry_eventsourceid) {
		g_source_remove(priv->grab_retry_eventsourceid);
		priv->grab_retry_eventsourceid = 0;
	}
	if (cnnwin->priv->delayed_grab_eventsourceid) {
		g_source_remove(cnnwin->priv->delayed_grab_eventsourceid);
		cnnwin->priv->delayed_grab_eventsourceid = 0;
	}
	if (priv->ftb_hide_eventsource) {
		g_source_remove(priv->ftb_hide_eventsource);
		priv->ftb_hide_eventsource = 0;
	}
	if (priv->tar_eventsource) {
		g_source_remove(priv->tar_eventsource);
		priv->tar_eventsource = 0;
	}
	if (priv->hidetb_eventsource) {
		g_source_remove(priv->hidetb_eventsource);
		priv->hidetb_eventsource = 0;
	}
	if (priv->dwp_eventsourceid) {
		g_source_remove(priv->dwp_eventsourceid);
		priv->dwp_eventsourceid = 0;
	}

	/* There is no need to destroy priv->floating_toolbar_widget,
	 * because it’s our child and will be destroyed automatically */

	cnnwin->priv = NULL;
	g_free(priv);
}

gboolean rcw_notify_widget_toolbar_placement(GtkWidget *widget, gpointer data)
{
	TRACE_CALL(__func__);
	GType rcwtype;

	rcwtype = rcw_get_type();
	if (G_TYPE_CHECK_INSTANCE_TYPE(widget, rcwtype)) {
		g_signal_emit_by_name(G_OBJECT(widget), "toolbar-place");
		return TRUE;
	}
	return FALSE;
}

static gboolean rcw_tb_drag_failed(GtkWidget *widget, GtkDragSource *context,
				  gpointer user_data)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv;
	RemminaConnectionWindow *cnnwin;


	cnnwin = (RemminaConnectionWindow *)user_data;
	priv = cnnwin->priv;

	if (priv->toolbar)
		gtk_widget_show(GTK_WIDGET(priv->toolbar));

	return TRUE;
}

static gboolean rcw_tb_drag_drop(GtkWidget *widget, GtkDragSource *context,
				 gint x, gint y, guint time, gpointer user_data)
{
	TRACE_CALL(__func__);
	GtkAllocation wa;
	gint new_toolbar_placement;
	RemminaConnectionWindowPriv *priv;
	RemminaConnectionWindow *cnnwin;

	cnnwin = (RemminaConnectionWindow *)user_data;
	priv = cnnwin->priv;

	gtk_widget_get_allocation(widget, &wa);

	if (wa.width * y >= wa.height * x) {
		if (wa.width * y > wa.height * (wa.width - x))
			new_toolbar_placement = TOOLBAR_PLACEMENT_BOTTOM;
		else
			new_toolbar_placement = TOOLBAR_PLACEMENT_LEFT;
	} else {
		if (wa.width * y > wa.height * (wa.width - x))
			new_toolbar_placement = TOOLBAR_PLACEMENT_RIGHT;
		else
			new_toolbar_placement = TOOLBAR_PLACEMENT_TOP;
	}

	//gtk_drag_finish(context, TRUE, TRUE, time); TODO GTK4

	if (new_toolbar_placement != remmina_pref.toolbar_placement) {
		/* Save new position */
		remmina_pref.toolbar_placement = new_toolbar_placement;
		remmina_pref_save();

		/* Signal all windows that the toolbar must be moved */
		remmina_widget_pool_foreach(rcw_notify_widget_toolbar_placement, NULL);
	}
	if (priv->toolbar)
		gtk_widget_show(GTK_WIDGET(priv->toolbar));

	return TRUE;
}

static void rcw_tb_drag_begin(GtkWidget *widget, GtkDragSource *context, gpointer user_data)
{
	TRACE_CALL(__func__);

	cairo_surface_t *surface;
	cairo_t *cr;
	GtkAllocation wa;
	double dashes[] = { 10 };

	gtk_widget_get_allocation(widget, &wa);

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 16, 16);
	cr = cairo_create(surface);
	cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
	cairo_set_line_width(cr, 4);
	cairo_set_dash(cr, dashes, 1, 0);
	cairo_rectangle(cr, 0, 0, 16, 16);
	cairo_stroke(cr);
	cairo_destroy(cr);

	gtk_widget_hide(widget);

	//gtk_drag_set_icon_surface(context, surface); TODO GTK4
}

void rcw_update_toolbar_opacity(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnwin->priv;
	RemminaConnectionObject *cnnobj;

	cnnobj = rcw_get_visible_cnnobj(cnnwin);
	if (!cnnobj) return;

	priv->floating_toolbar_opacity = (1.0 - TOOLBAR_OPACITY_MIN) / ((gdouble)TOOLBAR_OPACITY_LEVEL)
					 * ((gdouble)(TOOLBAR_OPACITY_LEVEL - remmina_file_get_int(cnnobj->remmina_file, "toolbar_opacity", 0)))
					 + TOOLBAR_OPACITY_MIN;
	if (priv->floating_toolbar_widget)
		gtk_widget_set_opacity(GTK_WIDGET(priv->overlay_ftb_overlay), priv->floating_toolbar_opacity);
}

static gboolean rcw_floating_toolbar_make_invisible(gpointer data)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = (RemminaConnectionWindowPriv *)data;

	gtk_widget_set_opacity(GTK_WIDGET(priv->overlay_ftb_overlay), 0.0);
	priv->ftb_hide_eventsource = 0;
	return G_SOURCE_REMOVE;
}

static void rcw_floating_toolbar_show(RemminaConnectionWindow *cnnwin, gboolean show)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnwin->priv;

	if (priv->floating_toolbar_widget == NULL)
		return;

	if (show || priv->pin_down) {
		/* Make the FTB no longer transparent, in case we have an hidden toolbar */
		rcw_update_toolbar_opacity(cnnwin);
		/* Remove outstanding hide events, if not yet active */
		if (priv->ftb_hide_eventsource) {
			g_source_remove(priv->ftb_hide_eventsource);
			priv->ftb_hide_eventsource = 0;
		}
	} else {
		/* If we are hiding and the toolbar must be made invisible, schedule
		 * a later toolbar hide */
		if (remmina_pref.fullscreen_toolbar_visibility == FLOATING_TOOLBAR_VISIBILITY_INVISIBLE)
			if (priv->ftb_hide_eventsource == 0)
				priv->ftb_hide_eventsource = g_timeout_add(1000, rcw_floating_toolbar_make_invisible, priv);
	}

	gtk_revealer_set_reveal_child(GTK_REVEALER(priv->revealer), show || priv->pin_down);
}

static void rco_get_desktop_size(RemminaConnectionObject *cnnobj, gint *width, gint *height)
{
	TRACE_CALL(__func__);
	RemminaProtocolWidget *gp = REMMINA_PROTOCOL_WIDGET(cnnobj->proto);


	*width = remmina_protocol_widget_get_width(gp);
	*height = remmina_protocol_widget_get_height(gp);
	if (*width == 0) {
		/* Before connecting we do not have real remote width/height,
		 * so we ask profile values */
		*width = remmina_protocol_widget_get_profile_remote_width(gp);
		*height = remmina_protocol_widget_get_profile_remote_height(gp);
	}
}

void rco_set_scrolled_policy(RemminaScaleMode scalemode, GtkScrolledWindow *scrolled_window)
{
	TRACE_CALL(__func__);

	gtk_scrolled_window_set_policy(scrolled_window,
				       scalemode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_SCALED ? GTK_POLICY_NEVER : GTK_POLICY_AUTOMATIC,
				       scalemode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_SCALED ? GTK_POLICY_NEVER : GTK_POLICY_AUTOMATIC);
}

static GtkWidget *rco_create_scrolled_container(RemminaScaleMode scalemode, int view_mode)
{
	GtkWidget *scrolled_container;

	if (view_mode == VIEWPORT_FULLSCREEN_MODE) {
		scrolled_container = remmina_scrolled_viewport_new();
	} else {
		scrolled_container = gtk_scrolled_window_new();
		rco_set_scrolled_policy(scalemode, GTK_SCROLLED_WINDOW(scrolled_container));
		//gtk_container_set_border_width(GTK_CONTAINER(scrolled_container), 0);
		gtk_widget_set_vexpand(scrolled_container, TRUE);
		gtk_widget_set_hexpand(scrolled_container, TRUE);
		gtk_widget_set_focusable(scrolled_container, FALSE);
	}

	gtk_widget_set_name(scrolled_container, "remmina-scrolled-container");
	gtk_widget_show(scrolled_container);

	return scrolled_container;
}

gboolean rcw_toolbar_autofit_restore(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);

	RemminaConnectionWindowPriv *priv = cnnwin->priv;
	RemminaConnectionObject *cnnobj;
	gint dwidth, dheight;
	GtkAllocation nba, ca, ta;

	cnnwin->priv->tar_eventsource = 0;

	if (priv->toolbar_is_reconfiguring){
		return G_SOURCE_REMOVE;
	}
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return FALSE;

	if (cnnobj->connected && GTK_IS_SCROLLED_WINDOW(cnnobj->scrolled_container)) {
		rco_get_desktop_size(cnnobj, &dwidth, &dheight);
		gtk_widget_get_allocation(GTK_WIDGET(priv->notebook), &nba);
		gtk_widget_get_allocation(cnnobj->scrolled_container, &ca);
		gtk_widget_get_allocation(priv->toolbar, &ta);
		if (remmina_pref.toolbar_placement == TOOLBAR_PLACEMENT_LEFT ||
		    remmina_pref.toolbar_placement == TOOLBAR_PLACEMENT_RIGHT){
			gtk_window_set_default_size(GTK_WINDOW(cnnobj->cnnwin), MAX(1, dwidth + ta.width + nba.width - ca.width),
					  MAX(1, dheight + nba.height - ca.height));
		}
		else{
			gtk_window_unmaximize(GTK_WINDOW(cnnobj->cnnwin));
			gtk_window_set_default_size(GTK_WINDOW(cnnobj->cnnwin), MAX(1, dwidth + nba.width - ca.width),
					  MAX(1, dheight + ta.height + nba.height - ca.height));
		}
	}
	if (GTK_IS_SCROLLED_WINDOW(cnnobj->scrolled_container)) {
		RemminaScaleMode scalemode = get_current_allowed_scale_mode(cnnobj, NULL, NULL);
		rco_set_scrolled_policy(scalemode, GTK_SCROLLED_WINDOW(cnnobj->scrolled_container));
	}

	return G_SOURCE_REMOVE;
}

static void rcw_toolbar_autofit(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;
	if (cnnwin->priv->toolbar_is_reconfiguring){
		return;
	}
		
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) {
		return;
	}

	if (GTK_IS_SCROLLED_WINDOW(cnnobj->scrolled_container)) {
			gtk_window_unfullscreen(GTK_WINDOW(cnnwin)); //TODO GTK4
		if (gtk_window_is_fullscreen(GTK_WINDOW(cnnwin))){
			gtk_window_unfullscreen(GTK_WINDOW(cnnwin)); //TODO GTK4
		}

		/* It’s tricky to make the toolbars disappear automatically, while keeping scrollable.
		 * Please tell me if you know a better way to do this */
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cnnobj->scrolled_container), GTK_POLICY_NEVER, GTK_POLICY_NEVER);

		cnnwin->priv->tar_eventsource = g_timeout_add(200, (GSourceFunc)rcw_toolbar_autofit_restore, cnnwin);
	}
}






#ifdef GDK_WINDOWING_WAYLAND
// Below code queries the Wayland backend to determine montior geometry

struct wl_ctx {
  struct wl_list outputs;
  GdkRectangle *rect;
};

struct result {
  struct wl_ctx *wl_ctx;
  struct wl_output *output;
  struct wl_list link;
};

static void output_handle_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width,
                                	int32_t physical_height, int32_t subpixel,  const char *make, const char *model,
                                	int32_t output_transform)
{
	struct result *out = (struct result *)data;
	out->wl_ctx->rect->height = physical_height;
	out->wl_ctx->rect->width = physical_width;
	out->wl_ctx->rect->x = x;
	out->wl_ctx->rect->y = y;
}

static void output_handle_mode(void *data, struct wl_output *wl_output,uint32_t flags, int32_t width, 
							int32_t height, int32_t refresh) {}
static void output_handle_done(void *data, struct wl_output *wl_output) {}
static void output_handle_scale(void *data, struct wl_output *wl_output, int32_t scale) {}

static const struct wl_output_listener output_listener = 
{
	output_handle_geometry, 
	output_handle_mode, 
	output_handle_done, 
	output_handle_scale,
};

static void global_registry_handler(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version) {
	if (!strcmp(interface, "wl_output")) {
		struct wl_ctx *wl_ctx = (struct wl_ctx *)data;
		struct result *output = malloc(sizeof(struct result));
		output->wl_ctx = wl_ctx;
		output->output = wl_registry_bind(registry, id, &wl_output_interface, version);
		wl_list_insert(&wl_ctx->outputs, &output->link);
		wl_output_add_listener(output->output, &output_listener, output);
  	}
}

static void global_registry_remover(void *data, struct wl_registry *registry, uint32_t id) {}

static const struct wl_registry_listener registry_listener = {global_registry_handler, global_registry_remover};

#endif


void rco_get_monitor_geometry(RemminaConnectionObject *cnnobj, GdkRectangle *sz)
{
	TRACE_CALL(__func__);

	/* Fill sz with the monitor (or workarea) size and position
	 * of the monitor (or workarea) where cnnobj->cnnwin is located */

	GdkRectangle monitor_geometry;
	GdkDisplay *display;
	GdkMonitor *monitor;

	sz->x = sz->y = sz->width = sz->height = 17;

	if (!cnnobj)
		return;
	if (!cnnobj->cnnwin)
		return;
	if (!gtk_widget_is_visible(GTK_WIDGET(cnnobj->cnnwin)))
		return;

	display = gtk_widget_get_display(GTK_WIDGET(cnnobj->cnnwin));
	GtkNative* native = gtk_widget_get_native((GTK_WIDGET(cnnobj->cnnwin)));
	GdkSurface *surface = gtk_native_get_surface(native);
	monitor = gdk_display_get_monitor_at_surface(display, surface);


	gdk_monitor_get_geometry(monitor, sz);
// #ifdef GDK_WINDOWING_WAYLAND
// 		struct wl_display *wl_display = wl_display_connect(NULL);
// 		if (wl_display == NULL){
// 			REMMINA_DEBUG("failed to connect to wl display");
// 			return;
// 		}
// 		struct wl_registry* wl_registry = wl_display_get_registry(wl_display);

// 		struct wl_ctx wl_ctx;
// 		wl_list_init(&wl_ctx.outputs);
// 		wl_ctx.rect = sz;
		
// 		wl_registry_add_listener(wl_registry, &registry_listener, &wl_ctx);
// 		wl_display_dispatch(wl_display);
// 		wl_display_roundtrip(wl_display);

// 		struct result *out, *tmp;
// 		wl_list_for_each_safe(out, tmp, &wl_ctx.outputs, link) {
// 			wl_output_destroy(out->output);
// 			wl_list_remove(&out->link);
// 			free(out);
// 		}
// 		wl_registry_destroy(wl_registry);
// 		wl_display_disconnect(wl_display);
// #endif


	// int monitor_scale_factor = gdk_monitor_get_scale_factor(monitor);
	// monitor_geometry.width *= monitor_scale_factor;
	// monitor_geometry.height *= monitor_scale_factor;
	//TODO GTK4 get workarea for x11 backgrounds

}

static void rco_check_resize(RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);
	gboolean scroll_required = FALSE;

	GdkRectangle monitor_geometry;
	gint rd_width, rd_height;
	// gint bordersz;
	gint scalemode;

	scalemode = remmina_protocol_widget_get_current_scale_mode(REMMINA_PROTOCOL_WIDGET(cnnobj->proto));

	/* Get remote destkop size */
	rco_get_desktop_size(cnnobj, &rd_width, &rd_height);

	/* Get our monitor size */
	rco_get_monitor_geometry(cnnobj, &monitor_geometry);

	if (!remmina_protocol_widget_get_expand(REMMINA_PROTOCOL_WIDGET(cnnobj->proto)) &&
	    (monitor_geometry.width < rd_width || monitor_geometry.height < rd_height) &&
	    scalemode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_NONE)
		scroll_required = TRUE;

	switch (cnnobj->cnnwin->priv->view_mode) {
	case SCROLLED_FULLSCREEN_MODE:
		gtk_window_set_default_size(GTK_WINDOW(cnnobj->cnnwin), monitor_geometry.width, monitor_geometry.height);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cnnobj->scrolled_container),
					       (scroll_required ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER),
					       (scroll_required ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER));
		break;

	case VIEWPORT_FULLSCREEN_MODE:
		// bordersz = scroll_required ? SCROLL_BORDER_SIZE : 0;
		gtk_window_set_default_size(GTK_WINDOW(cnnobj->cnnwin), monitor_geometry.width, monitor_geometry.height);
		if (REMMINA_IS_SCROLLED_VIEWPORT(cnnobj->scrolled_container))
			/* Put a border around Notebook content (RemminaScrolledViewpord), so we can
			 * move the mouse over the border to scroll */
			//gtk_container_set_border_width(GTK_CONTAINER(cnnobj->scrolled_container), bordersz);

		break;

	case SCROLLED_WINDOW_MODE:
		if (remmina_file_get_int(cnnobj->remmina_file, "viewmode", UNDEFINED_MODE) == UNDEFINED_MODE) {
			/* ToDo: is this really needed ? When ? */
			gtk_window_set_default_size(GTK_WINDOW(cnnobj->cnnwin),
						    MIN(rd_width, monitor_geometry.width), MIN(rd_height, monitor_geometry.height));
			if (rd_width >= monitor_geometry.width || rd_height >= monitor_geometry.height) {
				gtk_window_maximize(GTK_WINDOW(cnnobj->cnnwin));
				remmina_file_set_int(cnnobj->remmina_file, "window_maximize", TRUE);
			} else {
				rcw_toolbar_autofit(NULL, cnnobj->cnnwin);
				remmina_file_set_int(cnnobj->remmina_file, "window_maximize", FALSE);
			}
		} else {
			if (remmina_file_get_int(cnnobj->remmina_file, "window_maximize", FALSE))
				gtk_window_maximize(GTK_WINDOW(cnnobj->cnnwin));
		}
		break;

	default:
		break;
	}
}

static void rcw_set_tooltip(GtkWidget *item, const gchar *tip, guint key1, guint key2)
{
	TRACE_CALL(__func__);
	gchar *s1;
	gchar *s2;

	if (remmina_pref.hostkey && key1) {
		if (key2)
			s1 = g_strdup_printf(" (%s + %s,%s)", gdk_keyval_name(remmina_pref.hostkey),
					     gdk_keyval_name(gdk_keyval_to_upper(key1)), gdk_keyval_name(gdk_keyval_to_upper(key2)));
		else if (key1 == remmina_pref.hostkey)
			s1 = g_strdup_printf(" (%s)", gdk_keyval_name(remmina_pref.hostkey));
		else
			s1 = g_strdup_printf(" (%s + %s)", gdk_keyval_name(remmina_pref.hostkey),
					     gdk_keyval_name(gdk_keyval_to_upper(key1)));
	} else {
		s1 = NULL;
	}
	s2 = g_strdup_printf("%s%s", tip, s1 ? s1 : "");
	gtk_widget_set_tooltip_text(item, s2);
	g_free(s2);
	g_free(s1);
}

static void remmina_protocol_widget_update_alignment(RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);
	RemminaScaleMode scalemode;
	gboolean scaledexpandedmode;
	int rdwidth, rdheight;
	gfloat aratio;

	if (!cnnobj->plugin_can_scale) {
		/* If we have a plugin that cannot scale,
		 * (i.e. SFTP plugin), then we expand proto */
		gtk_widget_set_halign(GTK_WIDGET(cnnobj->proto), GTK_ALIGN_FILL);
		gtk_widget_set_valign(GTK_WIDGET(cnnobj->proto), GTK_ALIGN_FILL);
	} else {
		/* Plugin can scale */

		scalemode = get_current_allowed_scale_mode(cnnobj, NULL, NULL);
		scaledexpandedmode = remmina_protocol_widget_get_expand(REMMINA_PROTOCOL_WIDGET(cnnobj->proto));

		/* Check if we need aspectframe and create/destroy it accordingly */
		if (scalemode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_SCALED && !scaledexpandedmode) {
			/* We need an aspectframe as a parent of proto */
			rdwidth = remmina_protocol_widget_get_width(REMMINA_PROTOCOL_WIDGET(cnnobj->proto));
			rdheight = remmina_protocol_widget_get_height(REMMINA_PROTOCOL_WIDGET(cnnobj->proto));
			aratio = (gfloat)rdwidth / (gfloat)rdheight;
			if (!cnnobj->aspectframe) {
				/* We need a new aspectframe */
				cnnobj->aspectframe = gtk_aspect_frame_new(0.5, 0.5, aratio, FALSE);
				gtk_widget_set_name(cnnobj->aspectframe, "remmina-cw-aspectframe");
				g_object_ref(cnnobj->proto);
				gtk_viewport_set_child(GTK_VIEWPORT(cnnobj->viewport), cnnobj->aspectframe);
				gtk_aspect_frame_set_child(GTK_ASPECT_FRAME(cnnobj->aspectframe), cnnobj->proto);
				g_object_unref(cnnobj->proto);
				gtk_widget_show(cnnobj->aspectframe);
				if (cnnobj != NULL && cnnobj->cnnwin != NULL && cnnobj->cnnwin->priv->notebook != NULL)
					rcw_grab_focus(cnnobj->cnnwin);
			} else {
				//gtk_aspect_frame_set(GTK_ASPECT_FRAME(cnnobj->aspectframe), 0.5, 0.5, aratio, FALSE); TODO GTK4
				gtk_aspect_frame_set_xalign(GTK_ASPECT_FRAME(cnnobj->aspectframe), 0.5);
				gtk_aspect_frame_set_yalign(GTK_ASPECT_FRAME(cnnobj->aspectframe), 0.5);
				gtk_aspect_frame_set_ratio(GTK_ASPECT_FRAME(cnnobj->aspectframe), aratio);
				gtk_aspect_frame_set_obey_child(GTK_ASPECT_FRAME(cnnobj->aspectframe), FALSE);
			}
		} else {
			/* We do not need an aspectframe as a parent of proto */
			if (cnnobj->aspectframe) {
				/* We must remove the old aspectframe reparenting proto to viewport */
				g_object_ref(cnnobj->aspectframe);
				g_object_ref(cnnobj->proto);
				//gtk_container_remove(GTK_CONTAINER(cnnobj->aspectframe), cnnobj->proto);
				//gtk_container_remove(GTK_CONTAINER(cnnobj->viewport), cnnobj->aspectframe);
				g_object_unref(cnnobj->aspectframe);
				cnnobj->aspectframe = NULL;
				gtk_viewport_set_child(GTK_VIEWPORT(cnnobj->viewport), cnnobj->proto);
				g_object_unref(cnnobj->proto);
				if (cnnobj != NULL && cnnobj->cnnwin != NULL && cnnobj->cnnwin->priv->notebook != NULL)
					rcw_grab_focus(cnnobj->cnnwin);
			}
		}

		if (scalemode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_SCALED || scalemode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_DYNRES) {
			/* We have a plugin that can be scaled, and the scale button
			 * has been pressed. Give it the correct WxH maintaining aspect
			 * ratio of remote destkop size */
			gtk_widget_set_halign(GTK_WIDGET(cnnobj->proto), GTK_ALIGN_FILL);
			gtk_widget_set_valign(GTK_WIDGET(cnnobj->proto), GTK_ALIGN_FILL);
		} else {
			/* Plugin can scale, but no scaling is active. Ensure that we have
			 * aspectframe with a ratio of 1 */
			gtk_widget_set_halign(GTK_WIDGET(cnnobj->proto), GTK_ALIGN_CENTER);
			gtk_widget_set_valign(GTK_WIDGET(cnnobj->proto), GTK_ALIGN_CENTER);
		}
	}
}

static void nb_set_current_page(GtkNotebook *notebook, GtkWidget *page)
{
	gint np, i;

	np = gtk_notebook_get_n_pages(notebook);
	for (i = 0; i < np; i++) {
		if (gtk_notebook_get_nth_page(notebook, i) == page) {
			gtk_notebook_set_current_page(notebook, i);
			break;
		}
	}
}

static void nb_migrate_message_panels(GtkWidget *frompage, GtkWidget *topage)
{
	/* Migrate a single connection tab from a notebook to another one */
	// GList *lst, *l;

	/* Reparent message panels */
	// lst = gtk_container_get_children(GTK_CONTAINER(frompage));
	// for (l = lst; l != NULL; l = l->next) {
	// 	if (REMMINA_IS_MESSAGE_PANEL(l->data)) {
	// 		g_object_ref(l->data);
	// 		gtk_container_remove(GTK_CONTAINER(frompage), GTK_WIDGET(l->data));
	// 		gtk_box_append(GTK_BOX(topage), GTK_WIDGET(l->data));
	// 		g_object_unref(l->data);
	// 		//gtk_box_reorder_child(GTK_BOX(topage), GTK_WIDGET(l->data), 0);
	// 	}
	// }
	// g_list_free(lst);

}

static void rcw_migrate(RemminaConnectionWindow *from, RemminaConnectionWindow *to)
{
	/* Migrate a complete notebook from a window to another */

	gchar *tag;
	gint cp, np, i;
	GtkNotebook *from_notebook;
	GtkWidget *frompage, *newpage, *old_scrolled_container;
	RemminaConnectionObject *cnnobj;
	RemminaScaleMode scalemode;

	/* Migrate TAG */
	tag = g_strdup((gchar *)g_object_get_data(G_OBJECT(from), "tag"));
	g_object_set_data_full(G_OBJECT(to), "tag", tag, (GDestroyNotify)g_free);

	/* Migrate notebook content */
	from_notebook = from->priv->notebook;
	if (from_notebook && GTK_IS_NOTEBOOK(from_notebook)) {

		cp = gtk_notebook_get_current_page(from_notebook);
		np = gtk_notebook_get_n_pages(from_notebook);
		/* Create pages on dest notebook and migrate
		 * page content */
		for (i = 0; i < np; i++) {
			frompage = gtk_notebook_get_nth_page(from_notebook, i);
			cnnobj = g_object_get_data(G_OBJECT(frompage), "cnnobj");

			/* Migrate actions */
			gtk_widget_insert_action_group(GTK_WIDGET(to), "rcw", G_ACTION_GROUP(cnnobj->action_group));

			/* A scrolled container must be recreated, because it can be different on the new window/page
			  depending on view_mode */
			scalemode = get_current_allowed_scale_mode(cnnobj, NULL, NULL);
			old_scrolled_container = cnnobj->scrolled_container;
			cnnobj->scrolled_container = rco_create_scrolled_container(scalemode, to->priv->view_mode);

			newpage = rcw_append_new_page(to, cnnobj);

			nb_migrate_message_panels(frompage, newpage);

			/* Reparent the viewport (which is inside scrolled_container) to the new page */
			g_object_ref(cnnobj->viewport);
			if (GTK_IS_SCROLLED_WINDOW(cnnobj->scrolled_container)){
				gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(old_scrolled_container), NULL);
				gtk_widget_unparent(cnnobj->viewport);
				gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(cnnobj->scrolled_container), cnnobj->viewport);
				g_object_unref(cnnobj->viewport);
			}
			else{
				gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(old_scrolled_container), NULL);
				gtk_widget_unparent(cnnobj->viewport);
				gtk_box_append(GTK_BOX(cnnobj->scrolled_container), cnnobj->viewport);
				g_object_unref(cnnobj->viewport);
			}
		}

		/* Remove all the pages from source notebook */
		for (i = np - 1; i >= 0; i--)
			gtk_notebook_remove_page(from_notebook, i);
		gtk_notebook_set_current_page(to->priv->notebook, cp);

	}
}

static void rcw_switch_viewmode(RemminaConnectionWindow *cnnwin, int newmode)
{
	gboolean is_maximized;
	RemminaConnectionWindow *newwin;
	gint old_width, old_height;
	int old_mode;

	old_mode = cnnwin->priv->view_mode;
	if (old_mode == newmode)
		return;

	if (newmode == VIEWPORT_FULLSCREEN_MODE || newmode == SCROLLED_FULLSCREEN_MODE) {
		if (old_mode == SCROLLED_WINDOW_MODE) {
			/* We are leaving SCROLLED_WINDOW_MODE, save W,H, and maximized
			 * status before self destruction of cnnwin */
			gtk_window_get_default_size(GTK_WINDOW(cnnwin), &old_width, &old_height);
			is_maximized = gtk_window_is_maximized(GTK_WINDOW(cnnwin));
		}
		newwin = rcw_create_fullscreen(GTK_WINDOW(cnnwin), cnnwin->priv->fss_view_mode);
		rcw_migrate(cnnwin, newwin);
		gtk_widget_show(GTK_WIDGET(newwin));
		GtkWindowGroup *wingrp = gtk_window_group_new();
		gtk_window_group_add_window(wingrp, GTK_WINDOW(newwin));
		gtk_window_set_transient_for(GTK_WINDOW(newwin), NULL);

		if (old_mode == SCROLLED_WINDOW_MODE) {
			newwin->priv->ss_maximized = is_maximized;
			newwin->priv->ss_width = old_width;
			newwin->priv->ss_height = old_height;
		}
	} else {
		newwin = rcw_create_scrolled(cnnwin->priv->ss_width, cnnwin->priv->ss_height,
					     cnnwin->priv->ss_maximized);
		rcw_migrate(cnnwin, newwin);
		if (old_mode == VIEWPORT_FULLSCREEN_MODE || old_mode == SCROLLED_FULLSCREEN_MODE)
			/* We are leaving a FULLSCREEN mode, save some parameters
			 * status before self destruction of cnnwin */
			newwin->priv->fss_view_mode = old_mode;
	}

	/* Prevent unreleased hostkey from old window to be released here */
	newwin->priv->hostkey_used = TRUE;
}


static void rcw_toolbar_fullscreen(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);

	RemminaConnectionObject *cnnobj;

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;

	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	RemminaProtocolWidget *gp = REMMINA_PROTOCOL_WIDGET(cnnobj->proto);

	if (remmina_protocol_widget_get_multimon(gp) >= 1) {
		REMMINA_DEBUG("Fullscreen on all monitor");
		//gdk_window_set_fullscreen_mode(gtk_widget_get_window(GTK_WIDGET(toggle)), GDK_FULLSCREEN_ON_ALL_MONITORS);
	} else {
		REMMINA_DEBUG("Fullscreen on one monitor");
	}

	if ((toggle != NULL && toggle == GTK_WIDGET(cnnwin->priv->toolitem_fullscreen))) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle))) {
			if (remmina_protocol_widget_get_multimon(gp) >= 1)
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cnnwin->priv->toolitem_multimon), TRUE);
			rcw_switch_viewmode(cnnwin, cnnwin->priv->fss_view_mode);
		} else {
			rcw_switch_viewmode(cnnwin, SCROLLED_WINDOW_MODE);
		}
	} else
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cnnwin->priv->toolitem_multimon))) {
		rcw_switch_viewmode(cnnwin, cnnwin->priv->fss_view_mode);
	} else {
		rcw_switch_viewmode(cnnwin, SCROLLED_WINDOW_MODE);
	}
}

static void rco_viewport_fullscreen_mode(GtkWidget *widget, RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindow *newwin;

	// if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
	// 	return;
	cnnobj->cnnwin->priv->fss_view_mode = VIEWPORT_FULLSCREEN_MODE;
	newwin = rcw_create_fullscreen(GTK_WINDOW(cnnobj->cnnwin), VIEWPORT_FULLSCREEN_MODE);
	rcw_migrate(cnnobj->cnnwin, newwin);
	gtk_widget_show(GTK_WIDGET(newwin));
	GtkWindowGroup *wingrp = gtk_window_group_new();
	gtk_window_group_add_window(wingrp, GTK_WINDOW(newwin));
	gtk_window_set_transient_for(GTK_WINDOW(newwin), NULL);
}

static void rco_scrolled_fullscreen_mode(GtkWidget *widget, RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindow *newwin;

	// if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
	// 	return;
	cnnobj->cnnwin->priv->fss_view_mode = SCROLLED_FULLSCREEN_MODE;
	newwin = rcw_create_fullscreen(GTK_WINDOW(cnnobj->cnnwin), SCROLLED_FULLSCREEN_MODE);
	rcw_migrate(cnnobj->cnnwin, newwin);
	gtk_widget_show(GTK_WIDGET(newwin));
	GtkWindowGroup *wingrp = gtk_window_group_new();
	gtk_window_group_add_window(wingrp, GTK_WINDOW(newwin));
	gtk_window_set_transient_for(GTK_WINDOW(newwin), NULL);
}

static void rcw_fullscreen_option_popdown(GtkWidget *widget, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnwin->priv;

	priv->sticky = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->fullscreen_option_button), FALSE);
	rcw_floating_toolbar_show(cnnwin, FALSE);
}

void rcw_toolbar_fullscreen_option(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;
	GtkWidget *menu;
	GtkToggleButton *menuitem;

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;

	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)))
		return;

	cnnwin->priv->sticky = TRUE;

	menu = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	menuitem = (GtkToggleButton*)gtk_toggle_button_new_with_label( _("Viewport fullscreen mode"));
	if (cnnwin->priv->view_mode == VIEWPORT_FULLSCREEN_MODE)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(menuitem), TRUE);
	g_signal_connect(G_OBJECT(menuitem), "toggled", G_CALLBACK(rco_viewport_fullscreen_mode), cnnobj);


	if (cnnwin->priv->view_mode == SCROLLED_FULLSCREEN_MODE)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(menuitem), TRUE);
	g_signal_connect(G_OBJECT(menuitem), "toggled", G_CALLBACK(rco_scrolled_fullscreen_mode), cnnobj);

	g_signal_connect(G_OBJECT(menu), "deactivate", G_CALLBACK(rcw_fullscreen_option_popdown), cnnwin);

// #if GTK_CHECK_VERSION(3, 22, 0)
// 	gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(toggle),
// 				 GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
// #else
// 	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, remmina_public_popup_position, cnnwin->priv->toolitem_fullscreen, 0,
// 		       gtk_get_current_event_time());
// #endif
}


static void rcw_scaler_option_popdown(GtkWidget *widget, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnwin->priv;

	if (priv->toolbar_is_reconfiguring)
		return;
	priv->sticky = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->scaler_option_button), FALSE);
	rcw_floating_toolbar_show(cnnwin, FALSE);
}

static void rcw_scaler_expand(GtkWidget *widget, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;

	// if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
	// 	return;
	cnnobj = rcw_get_visible_cnnobj(cnnwin);
	if (!cnnobj)
		return;
	remmina_protocol_widget_set_expand(REMMINA_PROTOCOL_WIDGET(cnnobj->proto), TRUE);
	remmina_file_set_int(cnnobj->remmina_file, "scaler_expand", TRUE);
	remmina_protocol_widget_update_alignment(cnnobj);
}
static void rcw_scaler_keep_aspect(GtkWidget *widget, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;

	// if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
	// 	return;
	cnnobj = rcw_get_visible_cnnobj(cnnwin);
	if (!cnnobj)
		return;

	remmina_protocol_widget_set_expand(REMMINA_PROTOCOL_WIDGET(cnnobj->proto), FALSE);
	remmina_file_set_int(cnnobj->remmina_file, "scaler_expand", FALSE);
	remmina_protocol_widget_update_alignment(cnnobj);
}

// create properly formatted action name based on menu label
static void rcw_create_action_names(char *name, char *str, const char *label, char *group){
	strcpy(name, label);
	strcpy(str, "rcw.");
	//replace white_space with _
	char* ptr = name;
	while(*ptr){
		if (*ptr == ' ' || *ptr == '(' || *ptr == ')' || *ptr == '-'){
			*ptr = '_';
		}
		ptr++;
	}
	if (strcmp(group, "") == 0){
		strcat(str, name);
	}
	else{
		strcat(str, group);
		strcat(str, "::");
		strcat(str, name);
	}
}

static void rcw_toolbar_scaler_option(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv;
	RemminaConnectionObject *cnnobj;
	GtkWidget *menu;
	GtkWidget *menuitem;
	gboolean scaler_expand;

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;

	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;
	priv = cnnwin->priv;

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)))
		return;

	scaler_expand = remmina_protocol_widget_get_expand(REMMINA_PROTOCOL_WIDGET(cnnobj->proto));

	priv->sticky = TRUE;

	menu = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);;

	menuitem = gtk_toggle_button_new_with_label(_("Keep aspect ratio when scaled"));
	gtk_widget_show(menuitem);
	//gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	//group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
	if (!scaler_expand)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(menuitem), TRUE);
	g_signal_connect(G_OBJECT(menuitem), "toggled", G_CALLBACK(rcw_scaler_keep_aspect), cnnwin);

	menuitem = gtk_toggle_button_new_with_label(("Fill client window when scaled")); //TODO GTK4
	gtk_widget_show(menuitem);
	//gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	if (scaler_expand)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(menuitem), TRUE);
	g_signal_connect(G_OBJECT(menuitem), "toggled", G_CALLBACK(rcw_scaler_expand), cnnwin);

	g_signal_connect(G_OBJECT(menu), "deactivate", G_CALLBACK(rcw_scaler_option_popdown), cnnwin);

// #if GTK_CHECK_VERSION(3, 22, 0)
// 	gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(toggle),
// 				 GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
// #else
// 	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, remmina_public_popup_position, priv->toolitem_scale, 0,
// 		       gtk_get_current_event_time());
// #endif
}

void rco_switch_page_activate(GSimpleAction *action, GVariant *param, gpointer data)
{
	TRACE_CALL(__func__);
	gint page_num;
	RemminaConnectionObject* cnnobj;
	

	if(data == NULL){
			return;
		}

	

	page_num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), "new-page-num"));
	cnnobj = (RemminaConnectionObject *)g_object_get_data(G_OBJECT(action), "cnnobj");

	RemminaConnectionWindowPriv *priv = cnnobj->cnnwin->priv;
	gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook), page_num);

	// RemminaConnectionWindowPriv *priv = cnnobj->cnnwin->priv;
	// gint page_num;

	// page_num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), "new-page-num"));
	// gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook), page_num);
}

void rcw_toolbar_switch_page_popdown(GtkWidget *widget, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnwin->priv;

	priv->sticky = FALSE;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->toolitem_switch_page), FALSE);
	rcw_floating_toolbar_show(cnnwin, FALSE);
}

static void rcw_toolbar_switch_page(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);

	RemminaConnectionWindowPriv *priv = cnnwin->priv;
	RemminaConnectionObject *cnnobj, *cur_cnnobj;
	GtkPopoverMenu* popover_menu;
	GtkWidget *menu;
	gint i, n, cur;

	if (priv->toolbar_is_reconfiguring)
		return;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)))
		return;

	priv->sticky = TRUE;

	menu = g_menu_new();
	cur = gtk_notebook_get_current_page(GTK_NOTEBOOK(priv->notebook));
	cur_cnnobj = rcw_get_cnnobj_at_page(cnnobj->cnnwin, cur);
	n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(priv->notebook));
	for (i = 0; i < n; i++) {
		// if (i != cur){
		cnnobj = rcw_get_cnnobj_at_page(cnnobj->cnnwin, i);
		
		char name[80];
		char detailed_action[80];
		char label[80];
		char* trail = "switch";
		
		char* label_base = remmina_file_get_string(cnnobj->remmina_file, "name");
		strcpy(label, label_base);
		strcat(label, trail);
		rcw_create_action_names(name, detailed_action, label, "");

		// REMMINA_DEBUG("name: %s", name);
		// REMMINA_DEBUG("label: %s", label);
		// REMMINA_DEBUG("label_base: %s", label_base);
		// REMMINA_DEBUG("detailed_action: %s", detailed_action);

		GSimpleAction *action = g_simple_action_new (g_strdup(name), NULL);
		GMenuItem* menuitem = g_menu_item_new(label_base, detailed_action);
		//save these to be accessed in callback 
		g_object_set_data(G_OBJECT(action), "cnnobj", (gpointer)cnnobj);
		g_object_set_data(G_OBJECT(action), "new-page-num", GINT_TO_POINTER(i));

		g_signal_connect (action, "activate", G_CALLBACK (rco_switch_page_activate), menuitem);
		g_action_map_add_action (G_ACTION_MAP (cur_cnnobj->action_group), G_ACTION (action));
		
		
		g_menu_append_item(G_MENU(menu), menuitem);


		// }
			
	}


	popover_menu = (GtkPopoverMenu*)gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
	gtk_widget_set_parent(GTK_WIDGET(popover_menu), toggle);
	gtk_popover_popup(GTK_POPOVER(popover_menu));

	g_signal_connect(G_OBJECT(popover_menu), "closed", G_CALLBACK(rcw_toolbar_switch_page_popdown),
			 cnnwin);

// #if GTK_CHECK_VERSION(3, 22, 0)
// 	gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(toggle),
// 				 GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
// #else
// 	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, remmina_public_popup_position, widget, 0, gtk_get_current_event_time());
// #endif
}

void rco_update_toolbar_autofit_button(RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnobj->cnnwin->priv;
	GtkButton *toolitem;
	RemminaScaleMode sc;

	toolitem = priv->toolitem_autofit;
	if (toolitem) {
		if (priv->view_mode != SCROLLED_WINDOW_MODE) {
			gtk_widget_set_sensitive(GTK_WIDGET(toolitem), FALSE);
		} else {
			sc = get_current_allowed_scale_mode(cnnobj, NULL, NULL);
			gtk_widget_set_sensitive(GTK_WIDGET(toolitem), sc == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_NONE);
		}
	}
}

static void rco_change_scalemode(RemminaConnectionObject *cnnobj, gboolean bdyn, gboolean bscale)
{
	RemminaScaleMode scalemode;
	RemminaConnectionWindowPriv *priv = cnnobj->cnnwin->priv;

	if (bdyn)
		scalemode = REMMINA_PROTOCOL_WIDGET_SCALE_MODE_DYNRES;
	else if (bscale)
		scalemode = REMMINA_PROTOCOL_WIDGET_SCALE_MODE_SCALED;
	else
		scalemode = REMMINA_PROTOCOL_WIDGET_SCALE_MODE_NONE;

	remmina_protocol_widget_set_current_scale_mode(REMMINA_PROTOCOL_WIDGET(cnnobj->proto), scalemode);
	remmina_file_set_int(cnnobj->remmina_file, "scale", scalemode);
	gtk_widget_set_sensitive(GTK_WIDGET(priv->scaler_option_button), scalemode == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_SCALED);
	rco_update_toolbar_autofit_button(cnnobj);

	remmina_protocol_widget_call_feature_by_type(REMMINA_PROTOCOL_WIDGET(cnnobj->proto),
						     REMMINA_PROTOCOL_FEATURE_TYPE_SCALE, 0);

	if (cnnobj->cnnwin->priv->view_mode != SCROLLED_WINDOW_MODE)
		rco_check_resize(cnnobj);
	if (GTK_IS_SCROLLED_WINDOW(cnnobj->scrolled_container)) {
		rco_set_scrolled_policy(scalemode, GTK_SCROLLED_WINDOW(cnnobj->scrolled_container));
	}
}

static void rcw_toolbar_dynres(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	gboolean bdyn, bscale;
	RemminaConnectionObject *cnnobj;

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	if (cnnobj->connected) {
		bdyn = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));
		bscale = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cnnobj->cnnwin->priv->toolitem_scale));

		if (bdyn && bscale) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cnnobj->cnnwin->priv->toolitem_scale), FALSE);
			bscale = FALSE;
		}

		rco_change_scalemode(cnnobj, bdyn, bscale);
	}
}

static void rcw_toolbar_scaled_mode(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	gboolean bdyn, bscale;
	RemminaConnectionObject *cnnobj;

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	bdyn = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cnnobj->cnnwin->priv->toolitem_dynres));
	bscale = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));

	if (bdyn && bscale) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cnnobj->cnnwin->priv->toolitem_dynres), FALSE);
		bdyn = FALSE;
	}

	rco_change_scalemode(cnnobj, bdyn, bscale);
}

static void rcw_create_toolbar_connection_menu(GSimpleActionGroup* actions, RemminaConnectionWindow *cnnwin){
	RemminaConnectionObject *cnnobj;
	GMenu *menu;
	GMenuItem *menuitem;
	gchar filename[MAX_PATH_LEN];
	GDir *dir;
	gchar *remmina_data_dir;

	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	menu = cnnobj->connections_menu = g_menu_new();

	// get all groups
	remmina_data_dir = remmina_file_get_datadir();
	dir = g_dir_open(remmina_data_dir, 0, NULL);
	char* groups = remmina_file_manager_get_groups();

	// create a sub_menu for each group
	GHashTable* group_map = g_hash_table_new(g_int_hash, g_int_equal); //TODO GTK4 clean up
	char* group_name = strtok(groups, ",");
	while (group_name != NULL){
		GMenu* submenu = g_menu_new();
		g_menu_append_item(menu, g_menu_item_new_submenu(group_name, G_MENU_MODEL(submenu)));
		g_hash_table_insert(group_map, group_name, submenu);
		group_name = strtok(NULL, ",");
	}
	
	if (dir != NULL) {
		/* Iterate all remote desktop profiles */
		const gchar* name;
		
		while ((name = g_dir_read_name(dir)) != NULL) {
			const gchar* group = NULL;
			if (!g_str_has_suffix(name, ".remmina"))
				continue;
			g_snprintf(filename, sizeof(filename), "%s/%s", remmina_data_dir, name);

			//get info about the connection
			GKeyFile* gkeyfile = g_key_file_new();
			if (!g_key_file_load_from_file(gkeyfile, filename, G_KEY_FILE_NONE, NULL)) {
				g_key_file_free(gkeyfile);
				continue;
			}
			name = g_key_file_get_string(gkeyfile, "remmina", "name", NULL);
			group = g_key_file_get_string(gkeyfile, "remmina", "group", NULL);
			g_key_file_free(gkeyfile);
			if (name == NULL){
				continue;
			}
			char detailed_action[80];
			char new_name[80];

			rcw_create_action_names(new_name, detailed_action, name, "");

			GSimpleAction *action = g_simple_action_new (new_name, NULL);
			menuitem = g_menu_item_new(name, detailed_action);
			if (menuitem != NULL) {
				//add to group submenu
				if (group == NULL || group[0] == '\0'){
					g_menu_append_item(menu, G_MENU_ITEM(menuitem));
				}
				else{
					
					GMenu* submenu = g_hash_table_lookup(group_map, group);
					if (submenu != NULL){
						g_menu_append_item(submenu,menuitem);
					}
				}
				
				//save these to be accessed in callback 
				char* saved_filename = g_strdup(filename);//(char*) malloc(sizeof(filename));
				g_object_set_data(G_OBJECT(action), "filename", (gpointer)saved_filename);

				g_signal_connect (action, "activate", G_CALLBACK (rcw_toolbar_menu_on_launch_item), menuitem);
				g_action_map_add_action (G_ACTION_MAP (actions), G_ACTION (action));
			}
		}
		g_dir_close(dir);
	}
	g_free(remmina_data_dir);
}


static void rcw_create_toolbar_actions(GSimpleActionGroup* actions, RemminaConnectionWindow *cnnwin){
	RemminaConnectionObject *cnnobj;
	const RemminaProtocolFeature *feature;
	const gchar *domain;
	gboolean enabled;
	gchar **keystrokes;
	gchar **keystroke_values;
	gint i;
	const char* label;

	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;
	gboolean pref_separator = FALSE;


	domain = remmina_protocol_widget_get_domain(REMMINA_PROTOCOL_WIDGET(cnnobj->proto));
	cnnobj->toolbar_menu = (GtkWidget*)g_menu_new();
	cnnobj->preference_menu = (GtkWidget*)g_menu_new();
	for (feature = remmina_protocol_widget_get_features(REMMINA_PROTOCOL_WIDGET(cnnobj->proto)); feature && feature->type;
	     feature++) {
		if (feature->type == REMMINA_PROTOCOL_FEATURE_TYPE_TOOL){
			if (feature->opt1)
				label = g_dgettext(domain, (const gchar *)feature->opt1);

			enabled = remmina_protocol_widget_query_feature_by_ref(REMMINA_PROTOCOL_WIDGET(cnnobj->proto), feature);
			if (enabled) {
				char name[80];
				char detailed_action[80];
				rcw_create_action_names(name, detailed_action, label, "");

				GSimpleAction *action = g_simple_action_new (name, NULL);
				GMenuItem* menuitem = g_menu_item_new(label, detailed_action);
				//save these to be accessed in callback 
				g_object_set_data(G_OBJECT(action), "feature-type", (gpointer)feature);
				g_object_set_data(G_OBJECT(action), "proto", (gpointer)cnnobj->proto);

				g_signal_connect (action, "activate", G_CALLBACK (rcw_run_feature), menuitem);
				g_action_map_add_action (G_ACTION_MAP (actions), G_ACTION (action));
				
				g_menu_append_item(G_MENU(cnnobj->toolbar_menu), menuitem);
			}
		}
		else if (feature->type == REMMINA_PROTOCOL_FEATURE_TYPE_PREF){
			if (pref_separator){
				
			}
			enabled = remmina_protocol_widget_query_feature_by_ref(REMMINA_PROTOCOL_WIDGET(cnnobj->proto), feature);
			switch (GPOINTER_TO_INT(feature->opt1)) {
				case REMMINA_PROTOCOL_FEATURE_PREF_RADIO:
					rcw_toolbar_preferences_radio(cnnobj, cnnobj->remmina_file, actions, feature,
									domain, enabled);
					pref_separator = TRUE;
					break;
				case REMMINA_PROTOCOL_FEATURE_PREF_CHECK:
					rcw_toolbar_preferences_check(cnnobj, actions, feature,
									(const gchar*)domain, (gboolean)enabled);
					break;
			}
		}
		
		else{
			continue;
		}
			

		
		
	}

	/* If the plugin accepts keystrokes include the keystrokes menu */
	if (remmina_protocol_widget_plugin_receives_keystrokes(REMMINA_PROTOCOL_WIDGET(cnnobj->proto))) {
		/* Get the registered keystrokes list */
		keystrokes = g_strsplit(remmina_pref.keystrokes, STRING_DELIMITOR, -1);
		if (g_strv_length(keystrokes)) {
			/* Add a keystrokes submenu */
			GMenu* submenu = g_menu_new();
			/* Add each registered keystroke */
			for (i = 0; i < g_strv_length(keystrokes); i++) {
				keystroke_values = g_strsplit(keystrokes[i], STRING_DELIMITOR2, -1);
				if (g_strv_length(keystroke_values) > 1) {
					/* Add the keystroke if no description was available */
					char name[80];
					strcpy(name, keystroke_values[0]);
					char detailed_action[80];
					strcpy(detailed_action, "rcw.");
					char* ptr = name;
					while(*ptr){
						if (*ptr == ' '){
							*ptr = '_';
						}
						ptr++;
					}
					strcat(detailed_action, name);
					GSimpleAction *action = g_simple_action_new (name, NULL);

					GMenuItem* menuitem = g_menu_item_new(
						g_strdup(keystroke_values[strlen(keystroke_values[0]) ? 0 : 1]), detailed_action);

					g_object_set_data(G_OBJECT(action), "keystrokes", g_strdup(keystroke_values[1]));
					g_signal_connect(G_OBJECT(action), "activate",
								 G_CALLBACK(rcw_handle_keystrokes),
								 REMMINA_PROTOCOL_WIDGET(cnnobj->proto));

					g_action_map_add_action (G_ACTION_MAP (actions), G_ACTION (action));
					g_menu_append_item(submenu, menuitem);
				}
				g_strfreev(keystroke_values);
			}

			// menuitem = gtk_button_new_with_label(_("Send clipboard content as keystrokes"));
			// static gchar k_tooltip[] =
			// 	N_("CAUTION: Pasted text will be sent as a sequence of key-codes as if typed on your local keyboard.\n"
			// 	"\n"
			// 	"  • For best results use same keyboard settings for both, client and server.\n"
			// 	"\n"
			// 	"  • If client-keyboard is different from server-keyboard the received text can contain wrong or erroneous characters.\n"
			// 	"\n"
			// 	"  • Unicode characters and other special characters that can't be translated to local key-codes won’t be sent to the server.\n"
			// 	"\n");
			// gtk_widget_set_tooltip_text(menuitem, k_tooltip);
			// //gtk_menu_shell_append(GTK_MENU_SHELL(submenu_keystrokes), menuitem);
			// g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
			// 			 G_CALLBACK(remmina_protocol_widget_send_clipboard),
			// 			 REMMINA_PROTOCOL_WIDGET(cnnobj->proto));
			// gtk_widget_show(menuitem);
			g_menu_append_submenu(G_MENU(cnnobj->toolbar_menu), "Keystrokes", G_MENU_MODEL(submenu));

		}
		g_strfreev(keystrokes);
	}
}






static void rcw_toolbar_multi_monitor_mode(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;

	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle))) {
		REMMINA_DEBUG("Saving multimon as 1");
		remmina_file_set_int(cnnobj->remmina_file, "multimon", 1);
		remmina_file_save(cnnobj->remmina_file);
		remmina_protocol_widget_call_feature_by_type(REMMINA_PROTOCOL_WIDGET(cnnobj->proto),
							     REMMINA_PROTOCOL_FEATURE_TYPE_MULTIMON, 0);
		if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cnnwin->priv->toolitem_fullscreen)))
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cnnwin->priv->toolitem_fullscreen), TRUE);
	} else {
		REMMINA_DEBUG("Saving multimon as 0");
		remmina_file_set_int(cnnobj->remmina_file, "multimon", 0);
		remmina_file_save(cnnobj->remmina_file);
		rcw_toolbar_fullscreen(NULL, cnnwin);
	}
}

static void rcw_toolbar_open_main(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;

	remmina_exec_command(REMMINA_COMMAND_MAIN, NULL);
}

static void rcw_toolbar_preferences_popdown(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	cnnobj->cnnwin->priv->sticky = FALSE;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cnnobj->cnnwin->priv->toolitem_preferences), FALSE);
	rcw_floating_toolbar_show(cnnwin, FALSE);
}

void rcw_toolbar_menu_popdown(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnwin->priv;

	if (priv->toolbar_is_reconfiguring)
		return;

	priv->sticky = FALSE;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->toolitem_menu), FALSE);
	rcw_floating_toolbar_show(cnnwin, FALSE);
}

void rcw_toolbar_tools_popdown(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnwin->priv;

	if (priv->toolbar_is_reconfiguring)
		return;

	priv->sticky = FALSE;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->toolitem_tools), FALSE);
	rcw_floating_toolbar_show(cnnwin, FALSE);
}

static void rco_call_protocol_feature_radio(GSimpleAction* action, GVariant *data)
{
	TRACE_CALL(__func__);
	RemminaProtocolFeature *feature;
	const gchar* value;
	RemminaConnectionObject* cnnobj;

	feature = (RemminaProtocolFeature *)g_object_get_data(G_OBJECT(action), "feature-type");
	cnnobj = (RemminaConnectionObject *)g_object_get_data(G_OBJECT(action), "cnnobj");
	value = g_variant_get_string(data, NULL);

	remmina_file_set_string(cnnobj->remmina_file, (const gchar *)feature->opt2, (const gchar *)value);
	remmina_protocol_widget_call_feature_by_ref(REMMINA_PROTOCOL_WIDGET(cnnobj->proto), feature);
	g_action_change_state(G_ACTION(action), data);
}

static void rco_call_protocol_feature_check(GSimpleAction* action, gpointer *data)
{
	TRACE_CALL(__func__);
	RemminaProtocolFeature *feature;
	gboolean value;
	RemminaConnectionObject* cnnobj;

	
	value = g_variant_get_boolean(g_action_get_state(G_ACTION(action)));
	gboolean new_state = value;

	
	feature = (RemminaProtocolFeature *)g_object_get_data(G_OBJECT(action), "feature-type");
	cnnobj = (RemminaConnectionObject *)g_object_get_data(G_OBJECT(action), "cnnobj");

	remmina_file_set_int(cnnobj->remmina_file, (const gchar *)feature->opt2, !value);
	g_action_change_state(G_ACTION(action), g_variant_new_boolean(!new_state));
	remmina_protocol_widget_call_feature_by_ref(REMMINA_PROTOCOL_WIDGET(cnnobj->proto), feature);
}


void rcw_toolbar_preferences_radio(RemminaConnectionObject *cnnobj, RemminaFile *remminafile,
				   GSimpleActionGroup* actions, const RemminaProtocolFeature *feature, const gchar *domain, gboolean enabled)
{
	TRACE_CALL(__func__);
	GtkWidget *menuitem;
	gint i;
	const gchar **list;
	const gchar *value;
	GVariantType* variant_type = g_variant_type_new("s");

	value = remmina_file_get_string(remminafile, (const gchar *)feature->opt2);
	list = (const gchar **)feature->opt3;

	
	for (i = 0; list[i]; i += 2) {
		menuitem = gtk_toggle_button_new_with_label(g_dgettext(domain, list[i + 1]));
		const char* label = g_dgettext(domain, list[i + 1]);

		if (enabled) {
			char name[80];
			char detailed_action[80];
			rcw_create_action_names(name, detailed_action, label, "radio");

			GSimpleAction *action = g_simple_action_new_stateful ("radio", variant_type, g_variant_new_string(name));

			GMenuItem* menuitem = g_menu_item_new(label, detailed_action);
			//save these to be accessed in callback 
			g_object_set_data(G_OBJECT(action), "feature-type", (gpointer)feature);
			g_object_set_data(G_OBJECT(action), "cnnobj", (gpointer)cnnobj);

			g_signal_connect (action, "activate", G_CALLBACK (rco_call_protocol_feature_radio), menuitem);
			g_action_map_add_action (G_ACTION_MAP (actions), G_ACTION (action));
			
			g_menu_append_item(G_MENU(cnnobj->preference_menu), menuitem);
			if (value && g_strcmp0(list[i], value) == 0){
				g_action_change_state(G_ACTION(action), g_variant_new_string("name"));
			}
		} 
		else {
			gtk_widget_set_sensitive(menuitem, FALSE);
		}
	}
	g_variant_type_free(variant_type);
}

void rcw_toolbar_preferences_check(RemminaConnectionObject *cnnobj,
				   GSimpleActionGroup* actions, const RemminaProtocolFeature *feature,
				   const gchar *domain, gboolean enabled)
{
	TRACE_CALL(__func__);
	GtkWidget *menuitem;

	menuitem = gtk_toggle_button_new_with_label(g_dgettext(domain, (const gchar *)feature->opt3));
	gtk_widget_show(menuitem);

	if (enabled) {
		gboolean initial_value = remmina_file_get_int(cnnobj->remmina_file, feature->opt2, 0);
		GVariant* variant = g_variant_new_boolean(initial_value);

		const char* label = g_dgettext(domain, (const gchar *)feature->opt3);
		char name[80];
		char detailed_action[80];
		rcw_create_action_names(name, detailed_action, label, "");

		GSimpleAction *action = g_simple_action_new_stateful (name, NULL, variant);

		GMenuItem* menuitem = g_menu_item_new(label, detailed_action);
		//save these to be accessed in callback 
		g_object_set_data(G_OBJECT(action), "feature-type", (gpointer)feature);
		g_object_set_data(G_OBJECT(action), "cnnobj", (gpointer)cnnobj);

		g_signal_connect (action, "activate", G_CALLBACK (rco_call_protocol_feature_check), NULL);
		g_action_map_add_action (G_ACTION_MAP (actions), G_ACTION (action));
		
		g_menu_append_item(G_MENU(cnnobj->preference_menu), menuitem);

	} 
	else {
		gtk_widget_set_sensitive(menuitem, FALSE);
	}
}

static void rcw_toolbar_preferences(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv;
	RemminaConnectionObject *cnnobj;
	GtkPopoverMenu* popover_menu;


	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;
	priv = cnnobj->cnnwin->priv;

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)))
		return;

	priv->sticky = TRUE;


	popover_menu = (GtkPopoverMenu *)gtk_popover_menu_new_from_model(G_MENU_MODEL(cnnobj->preference_menu));
	gtk_widget_set_parent(GTK_WIDGET(popover_menu), toggle);
	gtk_popover_popup(GTK_POPOVER(popover_menu));
	
	g_signal_connect(G_OBJECT(popover_menu), "closed", G_CALLBACK(rcw_toolbar_preferences_popdown), cnnwin);



	//TODO GTK4 handle menu separator
}

static void rcw_toolbar_menu_on_launch_item(GSimpleAction *action, GVariant *variant, gpointer data)
{
	TRACE_CALL(__func__);
	gchar *s;

	s = (gchar*)g_object_get_data(G_OBJECT(action), "filename");

	remmina_exec_command(REMMINA_COMMAND_CONNECT, s);
	g_free(s);
}

static void rcw_toolbar_menu(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv;
	RemminaConnectionObject *cnnobj;
	GtkPopoverMenu* popover_menu;

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;
	REMMINA_DEBUG("Clicked the menu");
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;
	priv = cnnobj->cnnwin->priv;

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)))
		return;

	priv->sticky = TRUE;


	popover_menu = (GtkPopoverMenu*)gtk_popover_menu_new_from_model(G_MENU_MODEL(cnnobj->connections_menu));
	gtk_widget_set_parent(GTK_WIDGET(popover_menu), toggle);
	gtk_popover_popup(GTK_POPOVER(popover_menu));
	
	g_signal_connect(G_OBJECT(popover_menu), "closed", G_CALLBACK(rcw_toolbar_menu_popdown), cnnwin);

	// g_signal_connect(G_OBJECT(menu), "launch-item", G_CALLBACK(rcw_toolbar_menu_on_launch_item), NULL);
	// menuitem = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	// gtk_widget_show(menuitem);


	//gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
// #if GTK_CHECK_VERSION(3, 22, 0)
// 	gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(toggle),
// 				 GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
// #else
// 	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, remmina_public_popup_position, widget, 0, gtk_get_current_event_time());
// #endif
// 	g_signal_connect(G_OBJECT(menu), "deactivate", G_CALLBACK(rcw_toolbar_menu_popdown), cnnwin);
}





static void rcw_toolbar_tools(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv;
	RemminaConnectionObject *cnnobj;
	GtkPopoverMenu* popover_menu;


	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;
	priv = cnnobj->cnnwin->priv;

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)))
		return;

	priv->sticky = TRUE;


	popover_menu = (GtkPopoverMenu*)gtk_popover_menu_new_from_model(G_MENU_MODEL(cnnobj->toolbar_menu));
	gtk_widget_set_parent(GTK_WIDGET(popover_menu), toggle);
	gtk_popover_popup(GTK_POPOVER(popover_menu));
	
	g_signal_connect(G_OBJECT(popover_menu), "closed", G_CALLBACK(rcw_toolbar_tools_popdown), cnnwin);

}

static void rcw_toolbar_duplicate(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);

	RemminaConnectionObject *cnnobj;

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	remmina_file_save(cnnobj->remmina_file);

	remmina_exec_command(REMMINA_COMMAND_CONNECT, cnnobj->remmina_file->filename);
}

static void rcw_toolbar_screenshot(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);

	GdkPixbuf *screenshot;
	GdkSurface *active_window;
	cairo_t *cr;
	gint width, height;
	GString *pngstr;
	gchar *pngname;
	GtkWidget *dialog;
	RemminaProtocolWidget *gp;
	RemminaPluginScreenshotData rpsd;
	RemminaConnectionObject *cnnobj;
	cairo_surface_t *srcsurface;
	cairo_format_t cairo_format;
	cairo_surface_t *surface;
	int stride;

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	GDateTime *date = g_date_time_new_now_utc();

	// We will take a screenshot of the currently displayed RemminaProtocolWidget.
	gp = REMMINA_PROTOCOL_WIDGET(cnnobj->proto);

	gchar *denyclip = remmina_pref_get_value("deny_screenshot_clipboard");

	REMMINA_DEBUG("deny_screenshot_clipboard is set to %s", denyclip);

	// GdkClipboard *c = gdk_display_get_clipboard(gdk_display_get_default());

	// Ask the plugin if it can give us a screenshot
	if (remmina_protocol_widget_plugin_screenshot(gp, &rpsd)) {
		// Good, we have a screenshot from the plugin !

		REMMINA_DEBUG("Screenshot from plugin: w=%d h=%d bpp=%d bytespp=%d\n",
			      rpsd.width, rpsd.height, rpsd.bitsPerPixel, rpsd.bytesPerPixel);

		width = rpsd.width;
		height = rpsd.height;

		if (rpsd.bitsPerPixel == 32)
			cairo_format = CAIRO_FORMAT_ARGB32;
		else if (rpsd.bitsPerPixel == 24)
			cairo_format = CAIRO_FORMAT_RGB24;
		else
			cairo_format = CAIRO_FORMAT_RGB16_565;

		stride = cairo_format_stride_for_width(cairo_format, width);

		srcsurface = cairo_image_surface_create_for_data(rpsd.buffer, cairo_format, width, height, stride);
		// Transfer the PixBuf in the main clipboard selection
		if (denyclip && (g_strcmp0(denyclip, "true"))){
			// gtk_clipboard_set_image(c, gdk_pixbuf_get_from_surface(
			// 				srcsurface, 0, 0, width, height));
		}
		surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
		cr = cairo_create(surface);
		cairo_set_source_surface(cr, srcsurface, 0, 0);
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		cairo_paint(cr);
		cairo_surface_destroy(srcsurface);

		free(rpsd.buffer);
	} else {
		// The plugin is not releasing us a screenshot, just try to catch one via GTK

		/* Warn the user if image is distorted */
		if (cnnobj->plugin_can_scale &&
		    get_current_allowed_scale_mode(cnnobj, NULL, NULL) == REMMINA_PROTOCOL_WIDGET_SCALE_MODE_SCALED) {
			dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							_("Turn off scaling to avoid screenshot distortion."));
			g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(gtk_window_destroy), NULL);
			gtk_widget_show(dialog);
		}

		// Get the screenshot.
		GtkNative* native = gtk_widget_get_native((GTK_WIDGET(gp)));
		GdkSurface *window = gtk_native_get_surface(native);
		active_window = window;
		// width = gdk_window_get_width(gtk_widget_get_window(GTK_WIDGET(cnnobj->cnnwin)));
		width = gdk_surface_get_width(active_window);
		// height = gdk_window_get_height(gtk_widget_get_window(GTK_WIDGET(cnnobj->cnnwin)));
		height = gdk_surface_get_height(active_window);

		//screenshot = gdk_pixbuf_get_from_window(active_window, 0, 0, width, height);
		if (screenshot == NULL)
			g_print("gdk_pixbuf_get_from_window failed\n");

		// Transfer the PixBuf in the main clipboard selection
		// if (denyclip && (g_strcmp0(denyclip, "true")))
		// 	gtk_clipboard_set_image(c, screenshot);
		// Prepare the destination Cairo surface.
		surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
		cr = cairo_create(surface);

		// Copy the source pixbuf to the surface and paint it.
		gdk_cairo_set_source_pixbuf(cr, screenshot, 0, 0);
		cairo_paint(cr);

		// Deallocate screenshot pixbuf
		g_object_unref(screenshot);
	}

	//home/antenore/Pictures/remmina_%p_%h_%Y  %m %d-%H%M%S.png pngname
	//home/antenore/Pictures/remmina_st_  _2018 9 24-151958.240374.png

	pngstr = g_string_new(g_strdup_printf("%s/%s.png",
					      remmina_pref.screenshot_path,
					      remmina_pref.screenshot_name));
	remmina_utils_string_replace_all(pngstr, "%p",
					 remmina_file_get_string(cnnobj->remmina_file, "name"));
	remmina_utils_string_replace_all(pngstr, "%h",
					 remmina_file_get_string(cnnobj->remmina_file, "server"));
	remmina_utils_string_replace_all(pngstr, "%Y",
					 g_strdup_printf("%d", g_date_time_get_year(date)));
	remmina_utils_string_replace_all(pngstr, "%m", g_strdup_printf("%02d",
								       g_date_time_get_month(date)));
	remmina_utils_string_replace_all(pngstr, "%d",
					 g_strdup_printf("%02d", g_date_time_get_day_of_month(date)));
	remmina_utils_string_replace_all(pngstr, "%H",
					 g_strdup_printf("%02d", g_date_time_get_hour(date)));
	remmina_utils_string_replace_all(pngstr, "%M",
					 g_strdup_printf("%02d", g_date_time_get_minute(date)));
	remmina_utils_string_replace_all(pngstr, "%S",
					 g_strdup_printf("%02d", g_date_time_get_second(date)));
	g_date_time_unref(date);
	pngname = g_string_free(pngstr, FALSE);

	cairo_surface_write_to_png(surface, pngname);

	/* send a desktop notification */
	if (g_file_test(pngname, G_FILE_TEST_EXISTS))
		remmina_public_send_notification("remmina-screenshot-is-ready-id", _("Screenshot taken"), pngname);

	//Clean up and return.
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

static void rcw_toolbar_minimize(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;

	rcw_floating_toolbar_show(cnnwin, FALSE);
	gtk_window_minimize(GTK_WINDOW(cnnwin));
}

static void rcw_toolbar_disconnect(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;
	rco_disconnect_current_page(cnnobj);
}

static void rcw_toolbar_grab(GtkWidget *toggle, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	gboolean capture;
	RemminaConnectionObject *cnnobj;

	if (cnnwin->priv->toolbar_is_reconfiguring)
		return;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	capture = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));
	
	if (cnnobj->connected){
		remmina_file_set_int(cnnobj->remmina_file, "keyboard_grab", capture);
	}

	if (capture && cnnobj->connected) {
		
#if DEBUG_KB_GRABBING
		printf("DEBUG_KB_GRABBING: Grabbing for button\n");
#endif
		rcw_keyboard_grab(cnnobj->cnnwin);
		if (cnnobj->cnnwin->priv->pointer_entered)
			rcw_pointer_grab(cnnobj->cnnwin);
	} else {
		rcw_kp_ungrab(cnnobj->cnnwin);
	}

	rco_update_toolbar(cnnobj);
}

static GtkWidget *
rcw_create_toolbar(RemminaConnectionWindow *cnnwin, gint mode)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnwin->priv;
	RemminaConnectionObject *cnnobj;
	GtkBox *toolbar;
	GtkWidget *toolitem;
	GtkWidget *widget;
	GtkWidget *arrow;
	GdkDisplay *display;
	gint n_monitors;

	display = gdk_display_get_default();
	n_monitors = g_list_model_get_n_items(gdk_display_get_monitors(display));

	cnnobj = rcw_get_visible_cnnobj(cnnwin);

	priv->toolbar_is_reconfiguring = TRUE;

	toolbar = (GtkBox*)gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	/* Main actions */

	/* Menu */
	toolitem = gtk_toggle_button_new();
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "view-more-symbolic");
	//gtk_button_set_label(GTK_BUTTON(toolitem), _("_Menu"));
	gtk_widget_set_tooltip_text(toolitem, _("Menu"));
	gtk_box_append((toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	g_signal_connect(G_OBJECT(toolitem), "toggled", G_CALLBACK(rcw_toolbar_menu), cnnwin);
	priv->toolitem_menu = GTK_BUTTON(toolitem);

	/* Open Main window */
	toolitem = gtk_button_new_with_label("Open Remmina Main window");
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "go-home-symbolic");
	gtk_widget_set_tooltip_text(toolitem, _("Open the Remmina main window"));
	gtk_box_append((toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	g_signal_connect(G_OBJECT(toolitem), "clicked", G_CALLBACK(rcw_toolbar_open_main), cnnwin);

	priv->toolitem_new = GTK_BUTTON(toolitem);

	/* Duplicate session */
	toolitem = gtk_button_new_with_label("Duplicate connection");
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-duplicate-symbolic");
	gtk_widget_set_tooltip_text(toolitem, _("Duplicate current connection"));
	gtk_box_append((toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	g_signal_connect(G_OBJECT(toolitem), "clicked", G_CALLBACK(rcw_toolbar_duplicate), cnnwin);
	if (!cnnobj)
		gtk_widget_set_sensitive(GTK_WIDGET(toolitem), FALSE);

	priv->toolitem_duplicate = GTK_BUTTON(toolitem);

	/* Separator */
	toolitem = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_append((toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));

	/* Auto-Fit */
	toolitem = gtk_button_new();
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-fit-window-symbolic");
	rcw_set_tooltip(GTK_WIDGET(toolitem), _("Resize the window to fit in remote resolution"),
			remmina_pref.shortcutkey_autofit, 0);
	g_signal_connect(G_OBJECT(toolitem), "clicked", G_CALLBACK(rcw_toolbar_autofit), cnnwin);
	priv->toolitem_autofit = GTK_BUTTON(toolitem);
	gtk_box_append((toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));


	/* Fullscreen toggle */
	toolitem = gtk_toggle_button_new();
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-fullscreen-symbolic");
	rcw_set_tooltip(GTK_WIDGET(toolitem), _("Toggle fullscreen mode"),
			remmina_pref.shortcutkey_fullscreen, 0);
	gtk_box_append((toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	priv->toolitem_fullscreen = GTK_BUTTON(toolitem);
	if (kioskmode) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolitem), FALSE);
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolitem), mode != SCROLLED_WINDOW_MODE);
		g_signal_connect(G_OBJECT(toolitem), "clicked", G_CALLBACK(rcw_toolbar_fullscreen), cnnwin);
	}

	/* Fullscreen drop-down options */
	toolitem = gtk_button_new();
	gtk_widget_show(GTK_WIDGET(toolitem));
	gtk_box_append((toolbar), toolitem);
	widget = gtk_toggle_button_new();
	gtk_widget_show(widget);
	//gtk_container_set_border_width(GTK_CONTAINER(widget), 0);
	//gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(GTK_WIDGET(widget), FALSE);
	if (remmina_pref.small_toolbutton)
		gtk_widget_set_name(widget, "remmina-small-button");

#else
	gtk_button_set_focus_on_click(GTK_BUTTON(widget), FALSE);
#endif
	gtk_button_set_child(GTK_BUTTON(toolitem), widget);

#if GTK_CHECK_VERSION(3, 14, 0)
	arrow = gtk_image_new_from_icon_name("org.remmina.Remmina-pan-down-symbolic");
#else
	arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
#endif
	gtk_widget_show(arrow);
	gtk_button_set_child(GTK_BUTTON(widget), arrow);
	g_signal_connect(G_OBJECT(widget), "toggled", G_CALLBACK(rcw_toolbar_fullscreen_option), cnnwin);
	priv->fullscreen_option_button = widget;
	if (mode == SCROLLED_WINDOW_MODE)
		gtk_widget_set_sensitive(GTK_WIDGET(widget), FALSE);

	/* Multi monitor */
	if (n_monitors > 1) {
		toolitem = gtk_toggle_button_new();
		gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-multi-monitor-symbolic");
		rcw_set_tooltip(GTK_WIDGET(toolitem), _("Multi monitor"),
				remmina_pref.shortcutkey_multimon, 0);
		gtk_box_append(GTK_BOX(toolbar), toolitem);
		gtk_widget_show(GTK_WIDGET(toolitem));
		g_signal_connect(G_OBJECT(toolitem), "toggled", G_CALLBACK(rcw_toolbar_multi_monitor_mode), cnnwin);
		priv->toolitem_multimon = GTK_BUTTON(toolitem);
		if (!cnnobj)
			gtk_widget_set_sensitive(GTK_WIDGET(toolitem), FALSE);
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolitem),
							  remmina_file_get_int(cnnobj->remmina_file, "multimon", FALSE));
	}

	/* Dynamic Resolution Update */
	toolitem = gtk_toggle_button_new();
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-dynres-symbolic");
	rcw_set_tooltip(GTK_WIDGET(toolitem), _("Toggle dynamic resolution update"),
			remmina_pref.shortcutkey_dynres, 0);
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	g_signal_connect(G_OBJECT(toolitem), "toggled", G_CALLBACK(rcw_toolbar_dynres), cnnwin);
	priv->toolitem_dynres = GTK_BUTTON(toolitem);

	/* Scaler button */
	toolitem = gtk_toggle_button_new();
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-scale-symbolic");
	rcw_set_tooltip(GTK_WIDGET(toolitem), _("Toggle scaled mode"), remmina_pref.shortcutkey_scale, 0);
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	g_signal_connect(G_OBJECT(toolitem), "toggled", G_CALLBACK(rcw_toolbar_scaled_mode), cnnwin);
	priv->toolitem_scale = GTK_BUTTON(toolitem);

	/* Scaler aspect ratio dropdown menu */
	toolitem = gtk_button_new();
	gtk_widget_show(GTK_WIDGET(toolitem));
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	widget = gtk_toggle_button_new();
	gtk_widget_show(widget);
	//gtk_container_set_border_width(GTK_CONTAINER(widget), 0);
	//gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(GTK_WIDGET(widget), FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(widget), FALSE);
#endif
	if (remmina_pref.small_toolbutton)
		gtk_widget_set_name(widget, "remmina-small-button");
	gtk_button_set_child(GTK_BUTTON(toolitem), widget);
#if GTK_CHECK_VERSION(3, 14, 0)
	arrow = gtk_image_new_from_icon_name("org.remmina.Remmina-pan-down-symbolic");
#else
	arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
#endif
	gtk_widget_show(arrow);
	gtk_button_set_child(GTK_BUTTON(widget), arrow);
	g_signal_connect(G_OBJECT(widget), "toggled", G_CALLBACK(rcw_toolbar_scaler_option), cnnwin);
	priv->scaler_option_button = widget;

	/* Separator */
	toolitem = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));

	/* Switch tabs */
	toolitem = gtk_toggle_button_new();
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-switch-page-symbolic");
	rcw_set_tooltip(GTK_WIDGET(toolitem), _("Switch tab pages"), remmina_pref.shortcutkey_prevtab,
			remmina_pref.shortcutkey_nexttab);
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	g_signal_connect(G_OBJECT(toolitem), "toggled", G_CALLBACK(rcw_toolbar_switch_page), cnnwin);
	priv->toolitem_switch_page = GTK_BUTTON(toolitem);

	/* Grab keyboard button */
	toolitem = gtk_toggle_button_new();
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-keyboard-symbolic");
	rcw_set_tooltip(GTK_WIDGET(toolitem), _("Grab all keyboard events"),
			remmina_pref.shortcutkey_grab, 0);
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	g_signal_connect(G_OBJECT(toolitem), "toggled", G_CALLBACK(rcw_toolbar_grab), cnnwin);
	priv->toolitem_grab = GTK_BUTTON(toolitem);
	if (!cnnobj)
		gtk_widget_set_sensitive(GTK_WIDGET(toolitem), FALSE);
	else {
		const gchar *protocol = remmina_file_get_string(cnnobj->remmina_file, "protocol");
		if (g_strcmp0(protocol, "SFTP") == 0 || g_strcmp0(protocol, "SSH") == 0)
			gtk_widget_set_sensitive(GTK_WIDGET(toolitem), FALSE);
	}

	/* Preferences */
	toolitem = gtk_toggle_button_new();
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-preferences-system-symbolic");
	gtk_widget_set_tooltip_text(toolitem, _("Preferences"));
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	g_signal_connect(G_OBJECT(toolitem), "toggled", G_CALLBACK(rcw_toolbar_preferences), cnnwin);
	priv->toolitem_preferences = GTK_BUTTON(toolitem);

	/* Tools */
	toolitem = gtk_toggle_button_new();
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-system-run-symbolic");
	gtk_widget_set_tooltip_text(toolitem, _("Tools"));
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	g_signal_connect(G_OBJECT(toolitem), "toggled", G_CALLBACK(rcw_toolbar_tools), cnnwin);
	priv->toolitem_tools = GTK_BUTTON(toolitem);

	/* Separator */
	toolitem = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));

	toolitem = gtk_button_new_with_label( "_Screenshot");
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-camera-photo-symbolic");
	rcw_set_tooltip(GTK_WIDGET(toolitem), _("Screenshot"), remmina_pref.shortcutkey_screenshot, 0);
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	g_signal_connect(G_OBJECT(toolitem), "clicked", G_CALLBACK(rcw_toolbar_screenshot), cnnwin);
	priv->toolitem_screenshot = GTK_BUTTON(toolitem);

	/* Separator */
	toolitem = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));

	/* Minimize */
	toolitem = gtk_button_new_with_label("_Bottom");
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-go-bottom-symbolic");
	rcw_set_tooltip(GTK_WIDGET(toolitem), _("Minimize window"), remmina_pref.shortcutkey_minimize, 0);
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	g_signal_connect(G_OBJECT(toolitem), "clicked", G_CALLBACK(rcw_toolbar_minimize), cnnwin);
	if (kioskmode)
		gtk_widget_set_sensitive(GTK_WIDGET(toolitem), FALSE);

	/* Disconnect */
	toolitem = gtk_button_new_with_label("_Disconnect");
	gtk_button_set_icon_name(GTK_BUTTON(toolitem), "org.remmina.Remmina-disconnect-symbolic");
	rcw_set_tooltip(GTK_WIDGET(toolitem), _("Disconnect"), remmina_pref.shortcutkey_disconnect, 0);
	gtk_box_append(GTK_BOX(toolbar), toolitem);
	gtk_widget_show(GTK_WIDGET(toolitem));
	g_signal_connect(G_OBJECT(toolitem), "clicked", G_CALLBACK(rcw_toolbar_disconnect), cnnwin);

	priv->toolbar_is_reconfiguring = FALSE;
	return GTK_WIDGET(toolbar);
}

static void rcw_place_toolbar(GtkBox *toolbar, GtkGrid *grid, GtkWidget *sibling, int toolbar_placement)
{
	/* Place the toolbar inside the grid and set its orientation */

	if (toolbar_placement == TOOLBAR_PLACEMENT_LEFT || toolbar_placement == TOOLBAR_PLACEMENT_RIGHT)
		gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar), GTK_ORIENTATION_VERTICAL);
	else
		gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar), GTK_ORIENTATION_HORIZONTAL);


	switch (toolbar_placement) {
	case TOOLBAR_PLACEMENT_TOP:
		gtk_widget_set_hexpand(GTK_WIDGET(toolbar), TRUE);
		gtk_widget_set_vexpand(GTK_WIDGET(toolbar), FALSE);
		gtk_grid_attach_next_to(GTK_GRID(grid), GTK_WIDGET(toolbar), sibling, GTK_POS_TOP, 1, 1);
		break;
	case TOOLBAR_PLACEMENT_RIGHT:
		gtk_widget_set_vexpand(GTK_WIDGET(toolbar), TRUE);
		gtk_widget_set_hexpand(GTK_WIDGET(toolbar), FALSE);
		gtk_grid_attach_next_to(GTK_GRID(grid), GTK_WIDGET(toolbar), sibling, GTK_POS_RIGHT, 1, 1);
		break;
	case TOOLBAR_PLACEMENT_BOTTOM:
		gtk_widget_set_hexpand(GTK_WIDGET(toolbar), TRUE);
		gtk_widget_set_vexpand(GTK_WIDGET(toolbar), FALSE);
		gtk_grid_attach_next_to(GTK_GRID(grid), GTK_WIDGET(toolbar), sibling, GTK_POS_BOTTOM, 1, 1);
		break;
	case TOOLBAR_PLACEMENT_LEFT:
		gtk_widget_set_vexpand(GTK_WIDGET(toolbar), TRUE);
		gtk_widget_set_hexpand(GTK_WIDGET(toolbar), FALSE);
		gtk_grid_attach_next_to(GTK_GRID(grid), GTK_WIDGET(toolbar), sibling, GTK_POS_LEFT, 1, 1);
		break;
	}
}

static void rco_update_toolbar(RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnobj->cnnwin->priv;
	GtkWidget *toolitem;
	gboolean bval, dynres_avail, scale_avail;
	gboolean test_floating_toolbar;
	RemminaScaleMode scalemode;

	priv->toolbar_is_reconfiguring = TRUE;

	rco_update_toolbar_autofit_button(cnnobj);

	toolitem = GTK_WIDGET(priv->toolitem_switch_page);
	if (kioskmode)
		bval = FALSE;
	else
		bval = (gtk_notebook_get_n_pages(GTK_NOTEBOOK(priv->notebook)) > 1);
	gtk_widget_set_sensitive(GTK_WIDGET(toolitem), bval);

	if (cnnobj->remmina_file->filename)
		gtk_widget_set_sensitive(GTK_WIDGET(priv->toolitem_duplicate), TRUE);
	else
		gtk_widget_set_sensitive(GTK_WIDGET(priv->toolitem_duplicate), FALSE);

	scalemode = get_current_allowed_scale_mode(cnnobj, &dynres_avail, &scale_avail);
	gtk_widget_set_sensitive(GTK_WIDGET(priv->toolitem_dynres), dynres_avail && cnnobj->connected);
	gtk_widget_set_sensitive(GTK_WIDGET(priv->toolitem_scale), scale_avail && cnnobj->connected);

	switch (scalemode) {
	case REMMINA_PROTOCOL_WIDGET_SCALE_MODE_NONE:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->toolitem_dynres), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->toolitem_scale), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(priv->scaler_option_button), FALSE);
		break;
	case REMMINA_PROTOCOL_WIDGET_SCALE_MODE_SCALED:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->toolitem_dynres), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->toolitem_scale), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(priv->scaler_option_button), TRUE && cnnobj->connected);
		break;
	case REMMINA_PROTOCOL_WIDGET_SCALE_MODE_DYNRES:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->toolitem_dynres), TRUE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->toolitem_scale), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(priv->scaler_option_button), FALSE);
		break;
	}

	/* REMMINA_PROTOCOL_FEATURE_TYPE_MULTIMON */
	toolitem = (GtkWidget*)priv->toolitem_multimon;
	if (toolitem) {
		gint hasmultimon = remmina_protocol_widget_query_feature_by_type(REMMINA_PROTOCOL_WIDGET(cnnobj->proto),
										 REMMINA_PROTOCOL_FEATURE_TYPE_MULTIMON);

		gtk_widget_set_sensitive(GTK_WIDGET(toolitem), cnnobj->connected);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolitem),
						  remmina_file_get_int(cnnobj->remmina_file, "multimon", FALSE));
		gtk_widget_set_sensitive(GTK_WIDGET(toolitem), hasmultimon);
	}

	toolitem = (GtkWidget*)priv->toolitem_grab;
	gtk_widget_set_sensitive(GTK_WIDGET(toolitem), cnnobj->connected);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolitem),
					  remmina_file_get_int(cnnobj->remmina_file, "keyboard_grab", FALSE));
	const gchar *protocol = remmina_file_get_string(cnnobj->remmina_file, "protocol");
	if (g_strcmp0(protocol, "SFTP") == 0 || g_strcmp0(protocol, "SSH") == 0) {
		gtk_widget_set_sensitive(GTK_WIDGET(toolitem), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolitem), FALSE);
		remmina_file_set_int(cnnobj->remmina_file, "keyboard_grab", FALSE);
	}

	toolitem = (GtkWidget*)priv->toolitem_preferences;
	gtk_widget_set_sensitive(GTK_WIDGET(toolitem), cnnobj->connected);
	bval = remmina_protocol_widget_query_feature_by_type(REMMINA_PROTOCOL_WIDGET(cnnobj->proto),
							     REMMINA_PROTOCOL_FEATURE_TYPE_PREF);
	gtk_widget_set_sensitive(GTK_WIDGET(toolitem), bval && cnnobj->connected);

	toolitem = (GtkWidget*)priv->toolitem_tools;
	bval = remmina_protocol_widget_query_feature_by_type(REMMINA_PROTOCOL_WIDGET(cnnobj->proto),
							     REMMINA_PROTOCOL_FEATURE_TYPE_TOOL);
	gtk_widget_set_sensitive(GTK_WIDGET(toolitem), bval && cnnobj->connected);

	gtk_widget_set_sensitive(GTK_WIDGET(priv->toolitem_screenshot), cnnobj->connected);

	gtk_window_set_title(GTK_WINDOW(cnnobj->cnnwin), remmina_file_get_string(cnnobj->remmina_file, "name"));

	test_floating_toolbar = (priv->floating_toolbar_widget != NULL);

	if (test_floating_toolbar) {
		const gchar *str = remmina_file_get_string(cnnobj->remmina_file, "name");
		const gchar *format;
		GdkRGBA rgba;
		gchar *bg;

		bg = g_strdup(remmina_pref.grab_color);
		if (!gdk_rgba_parse(&rgba, bg)) {
			REMMINA_DEBUG("%s cannot be parsed as a color", bg);
			bg = g_strdup("#00FF00");
		} else {
			REMMINA_DEBUG("Using %s as background color", bg);
		}

		if (remmina_file_get_int(cnnobj->remmina_file, "keyboard_grab", FALSE)) {
			if (remmina_pref_get_boolean("grab_color_switch")) {
				// gtk_widget_override_background_color(priv->overlay_ftb_fr, GTK_STATE_NORMAL, &rgba);
				format = g_strconcat("<span bgcolor=\"", bg, "\" size=\"large\"><b>(G: ON) - \%s</b></span>", NULL);
			} else {
				// gtk_widget_override_background_color(priv->overlay_ftb_fr, GTK_STATE_NORMAL, NULL);
				format = "<big><b>(G: ON) - \%s</b></big>";
			}
		} else {
			// gtk_widget_override_background_color(priv->overlay_ftb_fr, GTK_STATE_NORMAL, NULL); TODO GTK4
			format = "<big><b>(G:OFF) - \%s</b></big>";
		}
		gchar *markup;

		markup = g_markup_printf_escaped(format, str);
		gtk_label_set_markup(GTK_LABEL(priv->floating_toolbar_label), markup);
		g_free(markup);
		g_free(bg);
	}

	priv->toolbar_is_reconfiguring = FALSE;
}

static void rcw_set_toolbar_visibility(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnwin->priv;

	if (priv->view_mode == SCROLLED_WINDOW_MODE) {
		if (remmina_pref.hide_connection_toolbar)
			gtk_widget_hide(priv->toolbar);
		else
			gtk_widget_show(priv->toolbar);
	}
}

#if DEBUG_KB_GRABBING
static void print_crossing_event(GdkEventCrossing *event) {
	printf("DEBUG_KB_GRABBING: --- Crossing event detail: ");
	switch (event->detail) {
	case GDK_NOTIFY_ANCESTOR: printf("GDK_NOTIFY_ANCESTOR"); break;
	case GDK_NOTIFY_VIRTUAL: printf("GDK_NOTIFY_VIRTUAL"); break;
	case GDK_NOTIFY_NONLINEAR: printf("GDK_NOTIFY_NONLINEAR"); break;
	case GDK_NOTIFY_NONLINEAR_VIRTUAL: printf("GDK_NOTIFY_NONLINEAR_VIRTUAL"); break;
	case GDK_NOTIFY_UNKNOWN: printf("GDK_NOTIFY_UNKNOWN"); break;
	case GDK_NOTIFY_INFERIOR: printf("GDK_NOTIFY_INFERIOR"); break;
	default: printf("unknown");
	}
	printf("\n");
	printf("DEBUG_KB_GRABBING: --- Crossing event mode=");
	switch (event->mode) {
	case GDK_CROSSING_NORMAL: printf("GDK_CROSSING_NORMAL"); break;
	case GDK_CROSSING_GRAB: printf("GDK_CROSSING_GRAB"); break;
	case GDK_CROSSING_UNGRAB: printf("GDK_CROSSING_UNGRAB"); break;
	case GDK_CROSSING_GTK_GRAB: printf("GDK_CROSSING_GTK_GRAB"); break;
	case GDK_CROSSING_GTK_UNGRAB: printf("GDK_CROSSING_GTK_UNGRAB"); break;
	case GDK_CROSSING_STATE_CHANGED: printf("GDK_CROSSING_STATE_CHANGED"); break;
	case GDK_CROSSING_TOUCH_BEGIN: printf("GDK_CROSSING_TOUCH_BEGIN"); break;
	case GDK_CROSSING_TOUCH_END: printf("GDK_CROSSING_TOUCH_END"); break;
	case GDK_CROSSING_DEVICE_SWITCH: printf("GDK_CROSSING_DEVICE_SWITCH"); break;
	default: printf("unknown");
	}
	printf("\n");
}
#endif

static gboolean rcw_floating_toolbar_on_enter(GtkWidget* self, gdouble x, gdouble y,
					      RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	rcw_floating_toolbar_show(cnnwin, TRUE);
	return TRUE;
}

static gboolean rcw_floating_toolbar_on_leave(GtkEventController *event_controller,
					      RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	GdkEvent* event = gtk_event_controller_get_current_event(event_controller);
	if (event == NULL){
		return FALSE;
	} //TODO determine why nulls occur
	if (gdk_crossing_event_get_detail(event) != GDK_NOTIFY_INFERIOR) 
		rcw_floating_toolbar_show(cnnwin, FALSE);
	return TRUE;
}


static gboolean rcw_on_enter_notify_event(GtkWidget *widget, GdkCrossingEvent *event,
					  gpointer user_data)
{
	TRACE_CALL(__func__);
#if DEBUG_KB_GRABBING
	printf("DEBUG_KB_GRABBING: enter-notify-event on rcw received\n");
	print_crossing_event(event);
#endif
	return FALSE;
}



static gboolean rcw_on_leave_notify_event(GtkEventControllerMotion *event_controller, 
					  gpointer user_data)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindow *cnnwin = (RemminaConnectionWindow *)user_data;
	GdkEvent* event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(event_controller));
	if (event == NULL){
		return FALSE;
	}
#if DEBUG_KB_GRABBING
	printf("DEBUG_KB_GRABBING: leave-notify-event on rcw received\n");
	print_crossing_event(event);
#endif

	if (gdk_crossing_event_get_mode(event) != GDK_CROSSING_NORMAL && gdk_crossing_event_get_mode(event) != GDK_CROSSING_UNGRAB) {
#if DEBUG_KB_GRABBING
	printf("DEBUG_KB_GRABBING:   ignored because mode is not GDK_CROSSING_NORMAL GDK_CROSSING_UNGRAB\n");
#endif
		return FALSE;
	}

	if (cnnwin->priv->delayed_grab_eventsourceid) {
		g_source_remove(cnnwin->priv->delayed_grab_eventsourceid);
		cnnwin->priv->delayed_grab_eventsourceid = 0;
	}

	/* Workaround for https://gitlab.gnome.org/GNOME/mutter/-/issues/2450#note_1586570 */
	if (gdk_crossing_event_get_mode(event) != GDK_CROSSING_UNGRAB) {
		rcw_kp_ungrab(cnnwin);
	} else {
#if DEBUG_KB_GRABBING
		printf("DEBUG_KB_GRABBING:   not ungrabbing, this event seems to be an unwanted event from GTK\n");
#endif
	}

	return FALSE;
}


static gboolean rco_leave_protocol_widget(GtkEventController *event_controller,
					  RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);
	GdkEvent* event = gtk_event_controller_get_current_event(event_controller);
	if (event == NULL){
		return FALSE;
	}
#if DEBUG_KB_GRABBING
	printf("DEBUG_KB_GRABBING: received leave event on RCO.\n");
	print_crossing_event(event);
#endif

	if (cnnobj->cnnwin->priv->delayed_grab_eventsourceid) {
		g_source_remove(cnnobj->cnnwin->priv->delayed_grab_eventsourceid);
		cnnobj->cnnwin->priv->delayed_grab_eventsourceid = 0;
	}

	cnnobj->cnnwin->priv->pointer_entered = FALSE;

	/* Ungrab only if the leave is due to normal mouse motion and not to an inferior */
	if (gdk_crossing_event_get_mode(event) == GDK_CROSSING_NORMAL && gdk_crossing_event_get_detail(event) != GDK_NOTIFY_INFERIOR)
		rcw_kp_ungrab(cnnobj->cnnwin);

	return FALSE;
}


gboolean rco_enter_protocol_widget(GtkWidget *event_controller,
				   gdouble x, gdouble y, RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);
	gboolean active;
	GdkEvent* event = gtk_event_controller_get_current_event((GtkEventController*)event_controller);
// 	if (event == NULL){
// 		return FALSE;
// 	}
// #if DEBUG_KB_GRABBING
// 	printf("DEBUG_KB_GRABBING: %s: enter on protocol widget event received\n", __func__);
// 	print_crossing_event(event);
// #endif

// 	RemminaConnectionWindowPriv *priv = cnnobj->cnnwin->priv;
// 	if (!priv->sticky && gdk_crossing_event_get_mode(event) == GDK_CROSSING_NORMAL)
// 		rcw_floating_toolbar_show(cnnobj->cnnwin, FALSE);

// 	priv->pointer_entered = TRUE;

// 	if (gdk_crossing_event_get_mode(event) == GDK_CROSSING_UNGRAB) {
// 		// Someone steal our grab, take note and do not attempt to regrab
// 		cnnobj->cnnwin->priv->kbcaptured = FALSE;
// 		cnnobj->cnnwin->priv->pointer_captured = FALSE;
// 		return FALSE;
// 	}TODO GTK4

	/* Check if we need grabbing */
	active = gtk_window_is_active(GTK_WINDOW(cnnobj->cnnwin));
	if (remmina_file_get_int(cnnobj->remmina_file, "keyboard_grab", FALSE) && active) {
		rcw_keyboard_grab(cnnobj->cnnwin);
		rcw_pointer_grab(cnnobj->cnnwin);
	}

	return FALSE;
}

static gboolean focus_in_delayed_grab(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);

#if DEBUG_KB_GRABBING
	printf("DEBUG_KB_GRABBING:   %s\n", __func__);
#endif
	if (cnnwin->priv->pointer_entered) {
#if DEBUG_KB_GRABBING
		printf("DEBUG_KB_GRABBING:   delayed requesting kb and pointer grab, because of pointer inside\n");
#endif
		rcw_keyboard_grab(cnnwin);
		rcw_pointer_grab(cnnwin);
	}
#if DEBUG_KB_GRABBING
	else {
		printf("DEBUG_KB_GRABBING:   %s not grabbing because pointer_entered is false\n", __func__);
	}
#endif
	cnnwin->priv->delayed_grab_eventsourceid = 0;
	return G_SOURCE_REMOVE;
}

static void rcw_focus_in(RemminaConnectionWindow *cnnwin)
{
	/* This function is the default signal handler for focus-in-event,
	 * but can also be called after a window focus state change event
	 * from rcw_state_event(). So expect to be called twice
	 * when cnnwin gains the focus */

	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;

	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	if (cnnobj && cnnobj->connected && remmina_file_get_int(cnnobj->remmina_file, "keyboard_grab", FALSE)) {
#if DEBUG_KB_GRABBING
		printf("DEBUG_KB_GRABBING: Received focus in on rcw, grabbing enabled: requesting kb grab, delayed\n");
#endif
		if (cnnwin->priv->delayed_grab_eventsourceid == 0)
			cnnwin->priv->delayed_grab_eventsourceid = g_timeout_add(300, (GSourceFunc)focus_in_delayed_grab, cnnwin);
	}
#if DEBUG_KB_GRABBING
	else {
		printf("DEBUG_KB_GRABBING: Received focus in on rcw, but a condition will prevent to grab\n");
	}
#endif
}

static void rcw_focus_out(RemminaConnectionWindow *cnnwin)
{
	/* This function is the default signal handler for focus-out-event,
	 * but can also be called after a window focus state change event
	 * from rcw_state_event(). So expect to be called twice
	 * when cnnwin loses the focus */

	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;

	// rcw_kp_ungrab(cnnwin);

	cnnwin->priv->hostkey_activated = FALSE;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	if (REMMINA_IS_SCROLLED_VIEWPORT(cnnobj->scrolled_container))
		remmina_scrolled_viewport_remove_motion(REMMINA_SCROLLED_VIEWPORT(cnnobj->scrolled_container));

	if (cnnobj->proto && cnnobj->scrolled_container)
		remmina_protocol_widget_call_feature_by_type(REMMINA_PROTOCOL_WIDGET(cnnobj->proto),
							     REMMINA_PROTOCOL_FEATURE_TYPE_UNFOCUS, 0);
}

static gboolean
rcw_floating_toolbar_hide(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnwin->priv;

	priv->hidetb_eventsource = 0;
	rcw_floating_toolbar_show(cnnwin, FALSE);
	return G_SOURCE_REMOVE;
}

static gboolean rcw_floating_toolbar_on_scroll(GtkWidget *widget, gdouble dx, gdouble dy, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;

	// int opacity;

	cnnobj = rcw_get_visible_cnnobj(cnnwin);
	if (!cnnobj)
		return TRUE;

	// opacity = remmina_file_get_int(cnnobj->remmina_file, "toolbar_opacity", 0);
// 	switch (gdk_scroll_event_get_direction(event)) {
// 	case GDK_SCROLL_UP:
// 		if (opacity > 0) {
// 			remmina_file_set_int(cnnobj->remmina_file, "toolbar_opacity", opacity - 1);
// 			rcw_update_toolbar_opacity(cnnwin);
// 			return TRUE;
// 		}
// 		break;
// 	case GDK_SCROLL_DOWN:
// 		if (opacity < TOOLBAR_OPACITY_LEVEL) {
// 			remmina_file_set_int(cnnobj->remmina_file, "toolbar_opacity", opacity + 1);
// 			rcw_update_toolbar_opacity(cnnwin);
// 			return TRUE;
// 		}
// 		break;
// #if GTK_CHECK_VERSION(3, 4, 0)
// 	case GDK_SCROLL_SMOOTH:
// 		int x = 0;
// 		int y = 0;
// 		//dk_scroll_event_get_delta(event, x, y);
// 		if (y < 0 && opacity > 0) {
// 			remmina_file_set_int(cnnobj->remmina_file, "toolbar_opacity", opacity - 1);
// 			rcw_update_toolbar_opacity(cnnwin);
// 			return TRUE;
// 		}
// 		if (y > 0 && opacity < TOOLBAR_OPACITY_LEVEL) {
// 			remmina_file_set_int(cnnobj->remmina_file, "toolbar_opacity", opacity + 1);
// 			rcw_update_toolbar_opacity(cnnwin);
// 			return TRUE;
// 		}
// 		break;
// #endif
// 	default:
// 		break;
// 	} TODO GTK4
	return TRUE;
}

static gboolean rcw_after_configure_scrolled(gpointer user_data)
{
	TRACE_CALL(__func__);
	gint width, height;
	//GdkWindowState s;
	gint ipg, npages;
	RemminaConnectionWindow *cnnwin;

	cnnwin = (RemminaConnectionWindow *)user_data;

	if (!cnnwin || !cnnwin->priv)
		return FALSE;

	//s = gdk_window_get_state(gtk_widget_get_window(GTK_WIDGET(cnnwin)));


	/* Changed window_maximize, window_width and window_height for all
	 * connections inside the notebook */
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(cnnwin->priv->notebook));
	for (ipg = 0; ipg < npages; ipg++) {
		RemminaConnectionObject *cnnobj;
		cnnobj = g_object_get_data(
			G_OBJECT(gtk_notebook_get_nth_page(GTK_NOTEBOOK(cnnwin->priv->notebook), ipg)),
			"cnnobj");
		if (gtk_window_is_maximized(GTK_WINDOW(cnnwin))) {
			remmina_file_set_int(cnnobj->remmina_file, "window_maximize", TRUE);
		} else {
			gtk_window_get_default_size(GTK_WINDOW(cnnobj->cnnwin), &width, &height);
			remmina_file_set_int(cnnobj->remmina_file, "window_width", width);
			remmina_file_set_int(cnnobj->remmina_file, "window_height", height);
			remmina_file_set_int(cnnobj->remmina_file, "window_maximize", FALSE);
		}
	}
	cnnwin->priv->acs_eventsourceid = 0;
	return FALSE;
}

static gboolean rcw_on_configure(GtkWidget *widget,
				 gpointer data)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindow *cnnwin;
	RemminaConnectionObject *cnnobj;

	if (!REMMINA_IS_CONNECTION_WINDOW(widget))
		return FALSE;

	cnnwin = (RemminaConnectionWindow *)widget;

	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return FALSE;

	if (cnnwin->priv->acs_eventsourceid) {
		g_source_remove(cnnwin->priv->acs_eventsourceid);
		cnnwin->priv->acs_eventsourceid = 0;
	}
	GtkNative* native = gtk_widget_get_native((GTK_WIDGET(cnnwin)));
	if ( gtk_native_get_surface(native)
	    && cnnwin->priv->view_mode == SCROLLED_WINDOW_MODE)
		/* Under GNOME Shell we receive this configure_event BEFORE a window
		 * is really unmaximized, so we must read its new state and dimensions
		 * later, not now */
		cnnwin->priv->acs_eventsourceid = g_timeout_add(500, rcw_after_configure_scrolled, cnnwin);

	if (cnnwin->priv->view_mode != SCROLLED_WINDOW_MODE)
		/* Notify window of change so that scroll border can be hidden or shown if needed */
		rco_check_resize(cnnobj);
	return FALSE;
}

static void rcw_update_pin(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	if (cnnwin->priv->pin_down)
		gtk_button_set_icon_name(GTK_BUTTON(cnnwin->priv->pin_button), "org.remmina.Remmina-pin-down-symbolic");
	else
		gtk_button_set_icon_name(GTK_BUTTON(cnnwin->priv->pin_button), "org.remmina.Remmina-pin-up-symbolic");
}

static void rcw_toolbar_pin(GtkWidget *widget, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	remmina_pref.toolbar_pin_down = cnnwin->priv->pin_down = !cnnwin->priv->pin_down;
	remmina_pref_save();
	rcw_update_pin(cnnwin);
}

static void rcw_create_floating_toolbar(RemminaConnectionWindow *cnnwin, gint mode)
{
	TRACE_CALL(__func__);

	RemminaConnectionWindowPriv *priv = cnnwin->priv;
	GtkWidget *ftb_widget;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *pinbutton;
	GtkWidget *tb;


	/* A widget to be used for GtkOverlay for GTK >= 3.10 */
	ftb_widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show(vbox);

	gtk_box_append(GTK_BOX(ftb_widget), vbox);

	tb = rcw_create_toolbar(cnnwin, mode);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show(hbox);


	/* The pin button */
	pinbutton = gtk_button_new();
	gtk_widget_show(pinbutton);
	gtk_box_append(GTK_BOX(hbox), pinbutton);
	//gtk_button_set_relief(GTK_BUTTON(pinbutton), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(GTK_WIDGET(pinbutton), FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(pinbutton), FALSE);
#endif
	gtk_widget_set_name(pinbutton, "remmina-pin-button");
	g_signal_connect(G_OBJECT(pinbutton), "clicked", G_CALLBACK(rcw_toolbar_pin), cnnwin);
	priv->pin_button = pinbutton;
	priv->pin_down = remmina_pref.toolbar_pin_down;
	rcw_update_pin(cnnwin);


	label = gtk_label_new("");
	gtk_label_set_max_width_chars(GTK_LABEL(label), 50);
	gtk_widget_show(label);

	gtk_box_append(GTK_BOX(hbox), label);

	priv->floating_toolbar_label = label;

	if (remmina_pref.floating_toolbar_placement == FLOATING_TOOLBAR_PLACEMENT_BOTTOM) {
		gtk_box_append(GTK_BOX(vbox), hbox);
		gtk_box_append(GTK_BOX(vbox), tb);
	} else {
		gtk_box_append(GTK_BOX(vbox), tb);
		gtk_box_append(GTK_BOX(vbox), hbox);
	}

	priv->floating_toolbar_widget = ftb_widget;
	gtk_widget_show(ftb_widget);
}

static void rcw_toolbar_place_signal(RemminaConnectionWindow *cnnwin, gpointer data)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv;

	priv = cnnwin->priv;
	/* Detach old toolbar widget and reattach in new position in the grid */
	if (priv->toolbar && priv->grid) {
		g_object_ref(priv->toolbar);
		//gtk_container_remove(GTK_CONTAINER(priv->grid), priv->toolbar);
		rcw_place_toolbar(GTK_BOX(priv->toolbar), GTK_GRID(priv->grid), GTK_WIDGET(priv->notebook), remmina_pref.toolbar_placement);
		g_object_unref(priv->toolbar);
	}
}


static void rcw_init(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv;

	priv = g_new0(RemminaConnectionWindowPriv, 1);
	cnnwin->priv = priv;

	priv->view_mode = SCROLLED_WINDOW_MODE;
	if (kioskmode && kioskmode == TRUE)
		priv->view_mode = VIEWPORT_FULLSCREEN_MODE;

	priv->floating_toolbar_opacity = 1.0;
	priv->kbcaptured = FALSE;
	priv->pointer_captured = FALSE;
	priv->pointer_entered = FALSE;
	priv->fss_view_mode = VIEWPORT_FULLSCREEN_MODE;
	priv->ss_width = 640;
	priv->ss_height = 480;
	priv->ss_maximized = FALSE;

	remmina_widget_pool_register(GTK_WIDGET(cnnwin));
}

static gboolean rcw_focus_in_event(GtkEventControllerFocus *widget, gpointer user_data)
{
	TRACE_CALL(__func__);
#if DEBUG_KB_GRABBING
	printf("DEBUG_KB_GRABBING: RCW focus-in-event received\n");
#endif
	rcw_focus_in((RemminaConnectionWindow *)user_data);
	return FALSE;
}

static gboolean rcw_focus_out_event(GtkEventControllerFocus *widget, gpointer user_data)
{
	TRACE_CALL(__func__);
#if DEBUG_KB_GRABBING
	printf("DEBUG_KB_GRABBING: RCW focus-out-event received\n");
#endif
	rcw_focus_out((RemminaConnectionWindow *)user_data);
	return FALSE;
}


static gboolean rcw_state_event(GtkWidget *widget, gpointer user_data)
{
	TRACE_CALL(__func__);

	if (!REMMINA_IS_CONNECTION_WINDOW(widget))
		return FALSE;

#if DEBUG_KB_GRABBING
	printf("DEBUG_KB_GRABBING: window-state-event received\n");
#endif

	// if (event->changed_mask & GDK_WINDOW_STATE_FOCUSED) {
	// 	if (event->new_window_state & GDK_WINDOW_STATE_FOCUSED)
	// 		rcw_focus_in((RemminaConnectionWindow *)widget);
	// 	else
	// 		rcw_focus_out((RemminaConnectionWindow *)widget);
	// } TODO GTK4

	return FALSE;
}

static gboolean rcw_map_event(GdkSurface *surface, gpointer data)
{
	TRACE_CALL(__func__);

	GtkWidget* widget = (GtkWidget*)gtk_native_get_for_surface(surface);
	if (widget == NULL){
		return FALSE;
	}

	RemminaConnectionWindow *cnnwin = (RemminaConnectionWindow *)widget;
	RemminaConnectionObject *cnnobj;
	RemminaProtocolWidget *gp;

	if (cnnwin->priv->toolbar_is_reconfiguring) return FALSE;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return FALSE;

	gp = REMMINA_PROTOCOL_WIDGET(cnnobj->proto);
	REMMINA_DEBUG("Mapping: %s", gtk_widget_get_name(widget));
	if (remmina_protocol_widget_map_event(gp))
		REMMINA_DEBUG("Called plugin mapping function");
	return FALSE;
}

static gboolean rcw_unmap_event(GdkSurface *surface, gpointer data)
{
	TRACE_CALL(__func__);

	GtkWidget* widget = (GtkWidget*)gtk_native_get_for_surface(surface);
	if (widget == NULL){
		return FALSE;
	}

	RemminaConnectionWindow *cnnwin = (RemminaConnectionWindow *)widget;
	RemminaConnectionObject *cnnobj;
	RemminaProtocolWidget *gp;

	if (cnnwin->priv->toolbar_is_reconfiguring) return FALSE;
	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return FALSE;

	gp = REMMINA_PROTOCOL_WIDGET(cnnobj->proto);
	REMMINA_DEBUG("Unmapping: %s", gtk_widget_get_name(widget));
	if (remmina_protocol_widget_unmap_event(gp))
		REMMINA_DEBUG("Called plugin mapping function");
	return FALSE;
}

static gboolean rcw_map_event_fullscreen(GtkWidget *widget,  gpointer data)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;
	RemminaConnectionWindow* cnnwin = (RemminaConnectionWindow*)data;

	REMMINA_DEBUG("Mapping: %s", gtk_widget_get_name(GTK_WIDGET(cnnwin)));

	if (!REMMINA_IS_CONNECTION_WINDOW(cnnwin)) {
		REMMINA_DEBUG("Remmina Connection Window undefined, cannot go fullscreen");
		return FALSE;
	}

	cnnobj = rcw_get_visible_cnnobj((RemminaConnectionWindow *)cnnwin);
	if (!cnnobj) {
		REMMINA_DEBUG("Remmina Connection Object undefined, cannot go fullscreen");
		return FALSE;
	}

	RemminaProtocolWidget *gp = REMMINA_PROTOCOL_WIDGET(cnnobj->proto);

	if (!gp)
		REMMINA_DEBUG("Remmina Protocol Widget undefined, cannot go fullscreen");

	if (remmina_protocol_widget_get_multimon(gp) >= 1) {
		REMMINA_DEBUG("Fullscreen on all monitor");
		// gdk_window_set_fullscreen_mode(gtk_widget_get_window(widget), GDK_FULLSCREEN_ON_ALL_MONITORS);
		// gdk_window_fullscreen(gtk_widget_get_window(widget)); //TODO GTK4
		return TRUE;
	} else {
		REMMINA_DEBUG("Fullscreen on one monitor");
	}



	GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(cnnwin));	
	GtkNative* native = gtk_widget_get_native((GTK_WIDGET(cnnwin)));
	GdkSurface* surface = gtk_native_get_surface(native);
	GdkMonitor* monitor = gdk_display_get_monitor_at_surface(display, surface);

#if GTK_CHECK_VERSION(3, 18, 0)
	if (remmina_pref.fullscreen_on_auto) {
		if (cnnwin->priv->active_monitor == NULL){
			gtk_window_fullscreen(GTK_WINDOW(cnnwin));
		}
		else{
			gtk_window_fullscreen_on_monitor(GTK_WINDOW(cnnwin), cnnwin->priv->active_monitor);
		}
	} else {
		REMMINA_DEBUG("Fullscreen managed by WM or by the user, as per settings");
		gtk_window_fullscreen(GTK_WINDOW(cnnwin));
	}
#else
	REMMINA_DEBUG("Cannot fullscreen on a specific monitor, feature available from GTK 3.18");
	gtk_window_fullscreen(GTK_WINDOW(widget));
#endif

	if (remmina_protocol_widget_map_event(gp))
		REMMINA_DEBUG("Called plugin mapping function");

	return FALSE;
}


void rcw_property_notification_check(GObject* self, GParamSpec* pspec, gpointer user_data)
{

	if (strcmp(g_param_spec_get_name(pspec), "mapped") == 0) {
		GdkSurface* surface = GDK_SURFACE(self);
		if (gdk_surface_get_mapped(surface)){
			if (user_data != NULL){
				rcw_map_event_fullscreen(GTK_WIDGET(surface), user_data);
			}
			else{
				rcw_map_event(surface, user_data);
			}
			
		}
		else{
			rcw_unmap_event(surface, user_data);
		}
	}

	if (strcmp(g_param_spec_get_name(pspec), "default-width") == 0 || strcmp(g_param_spec_get_name(pspec), "default-height") == 0) {
		rcw_on_configure(GTK_WIDGET(self), user_data);
	}	
				
	if (strcmp(g_param_spec_get_name(pspec), "maximized") == 0 || strcmp(g_param_spec_get_name(pspec), "fullscreened")== 0) {
		rcw_state_event(GTK_WIDGET(self), user_data);
	}
			
	//map-event (fullscreen)
}






static RemminaConnectionWindow *
rcw_new(gboolean fullscreen, GdkMonitor* full_screen_target_monitor)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindow *cnnwin;

	cnnwin = RCW(g_object_new(REMMINA_TYPE_CONNECTION_WINDOW, NULL));
	cnnwin->priv->on_delete_confirm_mode = RCW_ONDELETE_CONFIRM_IF_2_OR_MORE;
	cnnwin->priv->active_monitor = full_screen_target_monitor;

	g_signal_connect(G_OBJECT(cnnwin), "notify", G_CALLBACK(rcw_property_notification_check), NULL);		

	//gtk_container_set_border_width(GTK_CONTAINER(cnnwin), 0);
	g_signal_connect(G_OBJECT(cnnwin), "toolbar-place", G_CALLBACK(rcw_toolbar_place_signal), NULL);

	g_signal_connect(G_OBJECT(cnnwin), "close-request", G_CALLBACK(rcw_delete_event), NULL);
	g_signal_connect(G_OBJECT(cnnwin), "destroy", G_CALLBACK(rcw_destroy), NULL);

	/* Under Xorg focus-in-event and focus-out-event don’t work when keyboard is grabbed
	 * via gdk_device_grab. So we listen for window-state-event to detect focus in and focus out.
	 * But we must also listen focus-in-event and focus-out-event because some
	 * window managers missing _NET_WM_STATE_FOCUSED hint, does not update the window state
	 * in case of focus change */

	GtkEventControllerFocus* focus_event_controller = (GtkEventControllerFocus* )gtk_event_controller_focus_new();
	gtk_widget_add_controller(GTK_WIDGET(cnnwin), GTK_EVENT_CONTROLLER(focus_event_controller));
	g_signal_connect(focus_event_controller, "enter", G_CALLBACK(rcw_focus_in_event), cnnwin);
	g_signal_connect(focus_event_controller, "leave", G_CALLBACK(rcw_focus_out_event), cnnwin);


	GtkEventControllerMotion* motion_event_controller = (GtkEventControllerMotion*)gtk_event_controller_motion_new();
	gtk_widget_add_controller(GTK_WIDGET(cnnwin), GTK_EVENT_CONTROLLER(motion_event_controller));
	g_signal_connect(motion_event_controller, "enter", G_CALLBACK(rcw_on_enter_notify_event), cnnwin);
	g_signal_connect(motion_event_controller, "leave", G_CALLBACK(rcw_on_leave_notify_event), cnnwin);

	return cnnwin;
}

/* This function will be called for the first connection. A tag is set to the window so that
 * other connections can determine if whether a new tab should be append to the same window
 */
static void rcw_update_tag(RemminaConnectionWindow *cnnwin, RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);
	gchar *tag;

	switch (remmina_pref.tab_mode) {
	case REMMINA_TAB_BY_GROUP:
		tag = g_strdup(remmina_file_get_string(cnnobj->remmina_file, "group"));
		break;
	case REMMINA_TAB_BY_PROTOCOL:
		tag = g_strdup(remmina_file_get_string(cnnobj->remmina_file, "protocol"));
		break;
	default:
		tag = NULL;
		break;
	}
	g_object_set_data_full(G_OBJECT(cnnwin), "tag", tag, (GDestroyNotify)g_free);
}

void rcw_grab_focus(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;

	if (!(cnnobj = rcw_get_visible_cnnobj(cnnwin))) return;

	if (GTK_IS_WIDGET(cnnobj->proto))
		remmina_protocol_widget_grab_focus(REMMINA_PROTOCOL_WIDGET(cnnobj->proto));
}

static GtkWidget *nb_find_page_by_cnnobj(GtkNotebook *notebook, RemminaConnectionObject *cnnobj)
{
	gint i, np;
	GtkWidget *found_page, *pg;

	if (cnnobj == NULL || cnnobj->cnnwin == NULL || cnnobj->cnnwin->priv == NULL)
		return NULL;
	found_page = NULL;
	np = gtk_notebook_get_n_pages(cnnobj->cnnwin->priv->notebook);
	for (i = 0; i < np; i++) {
		pg = gtk_notebook_get_nth_page(cnnobj->cnnwin->priv->notebook, i);
		if (g_object_get_data(G_OBJECT(pg), "cnnobj") == cnnobj) {
			found_page = pg;
			break;
		}
	}

	return found_page;
}


void rco_closewin(RemminaProtocolWidget *gp)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj = gp->cnnobj;
	GtkWidget *page_to_remove;


	if (cnnobj && cnnobj->scrolled_container && REMMINA_IS_SCROLLED_VIEWPORT(cnnobj->scrolled_container)) {
		REMMINA_DEBUG("deleting motion");
		remmina_scrolled_viewport_remove_motion(REMMINA_SCROLLED_VIEWPORT(cnnobj->scrolled_container));
	}

	if (cnnobj && cnnobj->cnnwin) {
		page_to_remove = nb_find_page_by_cnnobj(cnnobj->cnnwin->priv->notebook, cnnobj);
		if (page_to_remove) {
			gtk_notebook_remove_page(
				cnnobj->cnnwin->priv->notebook,
				gtk_notebook_page_num(cnnobj->cnnwin->priv->notebook, page_to_remove));
			/* Invalidate pointers to objects destroyed by page removal */
			cnnobj->aspectframe = NULL;
			cnnobj->viewport = NULL;
			cnnobj->scrolled_container = NULL;
			/* we cannot invalidate cnnobj->proto, because it can be already been
			 * detached from the widget hierarchy in rco_on_disconnect() */
		}
	}
	if (cnnobj) {
		cnnobj->remmina_file = NULL;
		g_free(cnnobj);
		gp->cnnobj = NULL;
	}

	remmina_application_condexit(REMMINA_CONDEXIT_ONDISCONNECT);
}

void rco_on_close_button_clicked(GtkButton *button, RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);
	if (REMMINA_IS_PROTOCOL_WIDGET(cnnobj->proto)) {
		if (!remmina_protocol_widget_is_closed((RemminaProtocolWidget *)cnnobj->proto))
			remmina_protocol_widget_close_connection(REMMINA_PROTOCOL_WIDGET(cnnobj->proto));
		else
			rco_closewin((RemminaProtocolWidget *)cnnobj->proto);
	}
}

static GtkWidget *rco_create_tab_label(RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);
	GtkWidget *hbox;
	GtkWidget *widget;
	GtkButton *button;

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_show(hbox);

	widget = gtk_image_new_from_icon_name(remmina_file_get_icon_name(cnnobj->remmina_file));
	gtk_widget_show(widget);
	gtk_box_append(GTK_BOX(hbox), widget);

	widget = gtk_label_new(remmina_file_get_string(cnnobj->remmina_file, "name"));
	gtk_widget_set_valign(widget, GTK_ALIGN_CENTER);
	gtk_widget_set_halign(widget, GTK_ALIGN_CENTER);

	gtk_box_append(GTK_BOX(hbox), widget);

	button = (GtkButton*)gtk_button_new();      // The "x" to close the tab
	//gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(GTK_WIDGET(widget), FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(button), FALSE);
#endif
	gtk_widget_set_name(GTK_WIDGET(button), "remmina-small-button");

	widget = gtk_image_new_from_icon_name("window-close");
	gtk_button_set_child(button, widget);

	gtk_box_append(GTK_BOX(hbox), GTK_WIDGET(button));

	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(rco_on_close_button_clicked), cnnobj);


	return hbox;
}

static GtkWidget *rco_create_tab_page(RemminaConnectionObject *cnnobj)
{
	GtkWidget *page;

	page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(page, "remmina-tab-page");


	return page;
}

static GtkWidget *rcw_append_new_page(RemminaConnectionWindow *cnnwin, RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);
	GtkWidget *page, *label;
	GtkNotebook *notebook;

	notebook = cnnwin->priv->notebook;

	page = rco_create_tab_page(cnnobj);
	g_object_set_data(G_OBJECT(page), "cnnobj", cnnobj);
	label = rco_create_tab_label(cnnobj);

	cnnobj->cnnwin = cnnwin;

	gtk_notebook_append_page(notebook, page, label);
	gtk_notebook_set_tab_reorderable(notebook, page, TRUE);
	gtk_notebook_set_tab_detachable(notebook, page, TRUE);
	/* This trick prevents the tab label from being focused */
	gtk_widget_set_can_focus(gtk_widget_get_parent(label), FALSE);

	if (gtk_widget_get_parent(cnnobj->scrolled_container) != NULL)
		printf("REMMINA WARNING in %s: scrolled_container already has a parent\n", __func__);
	gtk_box_append(GTK_BOX(page), cnnobj->scrolled_container);

	gtk_widget_show(page);

	return page;
}


static void rcw_update_notebook(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	GtkNotebook *notebook;
	gint n;

	notebook = GTK_NOTEBOOK(cnnwin->priv->notebook);

	switch (cnnwin->priv->view_mode) {
	case SCROLLED_WINDOW_MODE:
		n = gtk_notebook_get_n_pages(notebook);
		gtk_notebook_set_show_tabs(notebook, remmina_pref.always_show_tab ? TRUE : n > 1);
		gtk_notebook_set_show_border(notebook, remmina_pref.always_show_tab ? TRUE : n > 1);
		break;
	default:
		gtk_notebook_set_show_tabs(notebook, FALSE);
		gtk_notebook_set_show_border(notebook, FALSE);
		break;
	}
}

static gboolean rcw_on_switch_page_finalsel(gpointer user_data)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv;
	RemminaConnectionObject *cnnobj;

	if (!user_data)
		return FALSE;

	cnnobj = (RemminaConnectionObject *)user_data;
	if (!cnnobj->cnnwin)
		return FALSE;

	priv = cnnobj->cnnwin->priv;

	if (GTK_IS_WIDGET(cnnobj->cnnwin)) {
		rcw_floating_toolbar_show(cnnobj->cnnwin, TRUE);
		if (!priv->hidetb_eventsource)
			priv->hidetb_eventsource = g_timeout_add(TB_HIDE_TIME_TIME, (GSourceFunc)
								 rcw_floating_toolbar_hide, cnnobj->cnnwin);
		rco_update_toolbar(cnnobj);
		rcw_grab_focus(cnnobj->cnnwin);
		if (priv->view_mode != SCROLLED_WINDOW_MODE)
			rco_check_resize(cnnobj);
	}
	priv->spf_eventsourceid = 0;
	return FALSE;
}

static void rcw_on_switch_page(GtkNotebook *notebook, GtkWidget *newpage, guint page_num,
			       RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindowPriv *priv = cnnwin->priv;
	RemminaConnectionObject *cnnobj_newpage;

	cnnobj_newpage = g_object_get_data(G_OBJECT(newpage), "cnnobj");
	if (priv->spf_eventsourceid)
		g_source_remove(priv->spf_eventsourceid);
	priv->spf_eventsourceid = g_idle_add(rcw_on_switch_page_finalsel, cnnobj_newpage);
}

static void rcw_on_page_added(GtkNotebook *notebook, GtkWidget *child, guint page_num,
			      RemminaConnectionWindow *cnnwin)
{
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(cnnwin->priv->notebook)) > 0)
		rcw_update_notebook(cnnwin);
}

static void rcw_on_page_removed(GtkNotebook *notebook, GtkWidget *child, guint page_num,
				RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(cnnwin->priv->notebook)) <= 0)
		gtk_window_destroy(GTK_WINDOW(cnnwin));

}

static GtkNotebook *
rcw_on_notebook_create_window(GtkNotebook *notebook, GtkWidget *page, gpointer data)
{
	/* This signal callback is called by GTK when a detachable tab is dropped on the root window
	 * or in an existing window */

	TRACE_CALL(__func__);
	RemminaConnectionWindow *srccnnwin;
	RemminaConnectionWindow *dstcnnwin;
	RemminaConnectionObject *cnnobj;
	GdkSurface *surface;
	gchar *srctag;
	gint width, height;



	cnnobj = (RemminaConnectionObject *)g_object_get_data(G_OBJECT(page), "cnnobj");
	GtkNative* native = gtk_widget_get_native((GTK_WIDGET(cnnobj->cnnwin)));
	surface = gtk_native_get_surface(native);

	srccnnwin = RCW(gtk_widget_get_root(GTK_WIDGET(notebook)));
	dstcnnwin = RCW(remmina_widget_pool_find_by_window(REMMINA_TYPE_CONNECTION_WINDOW, surface));

	if (srccnnwin == dstcnnwin)
		return NULL;

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(srccnnwin->priv->notebook)) == 1 && !dstcnnwin)
		return NULL;

	

	if (!dstcnnwin) {
		/* Drop is directed to a new rcw: create a new scrolled window to accommodate
		 * the dropped connectionand move our cnnobj there. Width and
		 * height of the new window are cloned from the current window */
		srctag = (gchar *)g_object_get_data(G_OBJECT(srccnnwin), "tag");
		gtk_window_get_default_size(GTK_WINDOW(srccnnwin), &width, &height);
		dstcnnwin = rcw_create_scrolled(width, height, FALSE);  // New dropped window is never maximized
		g_object_set_data_full(G_OBJECT(dstcnnwin), "tag", g_strdup(srctag), (GDestroyNotify)g_free);
		/* when returning, GTK will move the whole tab to the new notebook.
		 * Prepare cnnobj to be hosted in the new cnnwin */
		cnnobj->cnnwin = dstcnnwin;
	} else {
		cnnobj->cnnwin = dstcnnwin;
	}

	remmina_protocol_widget_set_hostkey_func(REMMINA_PROTOCOL_WIDGET(cnnobj->proto),
						 (RemminaHostkeyFunc)rcw_hostkey_func);

	return GTK_NOTEBOOK(cnnobj->cnnwin->priv->notebook);
}

static GtkNotebook *
rcw_create_notebook(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	GtkNotebook *notebook;

	notebook = GTK_NOTEBOOK(gtk_notebook_new());

	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
	gtk_widget_show(GTK_WIDGET(notebook));

	g_signal_connect(G_OBJECT(notebook), "create-window", G_CALLBACK(rcw_on_notebook_create_window), NULL);
	g_signal_connect(G_OBJECT(notebook), "switch-page", G_CALLBACK(rcw_on_switch_page), cnnwin);
	g_signal_connect(G_OBJECT(notebook), "page-added", G_CALLBACK(rcw_on_page_added), cnnwin);
	g_signal_connect(G_OBJECT(notebook), "page-removed", G_CALLBACK(rcw_on_page_removed), cnnwin);
	gtk_widget_set_focusable(GTK_WIDGET(notebook), FALSE);
	return notebook;
}

/* Create a scrolled toplevel window */
static RemminaConnectionWindow *rcw_create_scrolled(gint width, gint height, gboolean maximize)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindow *cnnwin;
	GtkWidget *grid;
	GtkWidget *toolbar;
	GtkNotebook *notebook;
	GtkSettings *settings = gtk_settings_get_default();

	cnnwin = rcw_new(FALSE, NULL);
	gtk_widget_realize(GTK_WIDGET(cnnwin));
	g_signal_connect(gtk_native_get_surface(GTK_NATIVE(cnnwin)), "notify", G_CALLBACK(rcw_property_notification_check), NULL);

	gtk_window_set_default_size(GTK_WINDOW(cnnwin), width, height);
	g_object_set(settings, "gtk-application-prefer-dark-theme", remmina_pref.dark_theme, NULL);

	/* Create the toolbar */
	toolbar = rcw_create_toolbar(cnnwin, SCROLLED_WINDOW_MODE);

	/* Create the notebook */
	notebook = rcw_create_notebook(cnnwin);

	/* Create the grid container for toolbars+notebook and populate it */
	grid = gtk_grid_new();


	gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(notebook), 0, 0, 1, 1);

	gtk_widget_set_hexpand(GTK_WIDGET(notebook), TRUE);
	gtk_widget_set_vexpand(GTK_WIDGET(notebook), TRUE);

	rcw_place_toolbar(GTK_BOX(toolbar), GTK_GRID(grid), GTK_WIDGET(notebook), remmina_pref.toolbar_placement);

	gtk_window_set_child(GTK_WINDOW(cnnwin), grid);

	/* Add drag capabilities to the toolbar */
	//gtk_drag_source_set(GTK_WIDGET(toolbar), GDK_BUTTON1_MASK,
			    //dnd_targets_tb, sizeof dnd_targets_tb / sizeof *dnd_targets_tb, GDK_ACTION_MOVE); TODO GTK4
	// g_signal_connect_after(GTK_WIDGET(toolbar), "drag-begin", G_CALLBACK(rcw_tb_drag_begin), NULL);
	// g_signal_connect(GTK_WIDGET(toolbar), "drag-failed", G_CALLBACK(rcw_tb_drag_failed), cnnwin);

	/* Add drop capabilities to the drop/dest target for the toolbar (the notebook) */
	// gtk_drag_dest_set(GTK_WIDGET(notebook), GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT,
	// 		  dnd_targets_tb, sizeof dnd_targets_tb / sizeof *dnd_targets_tb, GDK_ACTION_MOVE); TODO GTK4
	//gtk_drag_dest_set_track_motion(GTK_WIDGET(notebook), TRUE); TODO GTK4
	// g_signal_connect(GTK_WIDGET(notebook), "drag-drop", G_CALLBACK(rcw_tb_drag_drop), cnnwin);

	cnnwin->priv->view_mode = SCROLLED_WINDOW_MODE;
	cnnwin->priv->toolbar = toolbar;
	cnnwin->priv->grid = grid;
	cnnwin->priv->notebook = notebook;
	cnnwin->priv->ss_width = width;
	cnnwin->priv->ss_height = height;
	cnnwin->priv->ss_maximized = maximize;

	/* The notebook and all its child must be realized now, or a reparent will
	 * call unrealize() and will destroy a GtkSocket */
	gtk_widget_show(grid);
	gtk_widget_show(GTK_WIDGET(cnnwin));
	GtkWindowGroup *wingrp = gtk_window_group_new();

	gtk_window_group_add_window(wingrp, GTK_WINDOW(cnnwin));
	gtk_window_set_transient_for(GTK_WINDOW(cnnwin), NULL);

	if (maximize)
		gtk_window_maximize(GTK_WINDOW(cnnwin));


	rcw_set_toolbar_visibility(cnnwin);

	return cnnwin;
}

static void rcw_create_overlay_ftb_overlay(RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);

	GtkWidget *revealer;
	RemminaConnectionWindowPriv *priv;

	priv = cnnwin->priv;

	if (priv->overlay_ftb_overlay != NULL) {
		gtk_window_destroy(GTK_WINDOW(priv->overlay_ftb_overlay));
		priv->overlay_ftb_overlay = NULL;
		priv->revealer = NULL;
	}
	if (priv->overlay_ftb_fr != NULL) {
		gtk_window_destroy(priv->overlay_ftb_fr);
		priv->overlay_ftb_fr = NULL;
	}

	rcw_create_floating_toolbar(cnnwin, priv->fss_view_mode);

	priv->overlay_ftb_overlay = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	//gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);

	GtkWidget *handle = gtk_drawing_area_new();

	gtk_widget_set_size_request(handle, 4, 4);
	gtk_widget_set_name(handle, "ftb-handle");

	revealer = gtk_revealer_new();

	gtk_widget_set_halign(GTK_WIDGET(priv->overlay_ftb_overlay), GTK_ALIGN_CENTER);

	if (remmina_pref.floating_toolbar_placement == FLOATING_TOOLBAR_PLACEMENT_BOTTOM) {
		gtk_box_append(GTK_BOX(vbox), handle);
		gtk_box_append(GTK_BOX(vbox), revealer);
		gtk_revealer_set_transition_type(GTK_REVEALER(revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
		gtk_widget_set_valign(GTK_WIDGET(priv->overlay_ftb_overlay), GTK_ALIGN_END);
	} else {
		gtk_box_append(GTK_BOX(vbox), revealer);
		gtk_box_append(GTK_BOX(vbox), handle);
		gtk_revealer_set_transition_type(GTK_REVEALER(revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
		gtk_widget_set_valign(GTK_WIDGET(priv->overlay_ftb_overlay), GTK_ALIGN_START);
	}


	gtk_revealer_set_child(GTK_REVEALER(revealer), priv->floating_toolbar_widget);
	gtk_widget_set_halign(GTK_WIDGET(revealer), GTK_ALIGN_CENTER);
	gtk_widget_set_valign(GTK_WIDGET(revealer), GTK_ALIGN_START);

	priv->revealer = revealer;

	GtkWidget *fr;

	fr = gtk_frame_new(NULL);
	priv->overlay_ftb_fr = fr;
	gtk_box_append(GTK_BOX(priv->overlay_ftb_overlay), fr);
	gtk_frame_set_child(GTK_FRAME(fr), vbox);

	gtk_widget_show(vbox);
	gtk_widget_show(revealer);
	gtk_widget_show(handle);
	gtk_widget_show(priv->overlay_ftb_overlay);
	gtk_widget_show(fr);

	if (remmina_pref.floating_toolbar_placement == FLOATING_TOOLBAR_PLACEMENT_BOTTOM)
		gtk_widget_set_name(fr, "ftbbox-lower");
	else
		gtk_widget_set_name(fr, "ftbbox-upper");

	gtk_overlay_add_overlay(GTK_OVERLAY(priv->overlay), priv->overlay_ftb_overlay);

	rcw_floating_toolbar_show(cnnwin, TRUE);

	GtkEventControllerMotion* motion_event_controller = (GtkEventControllerMotion*)gtk_event_controller_motion_new();
	// gtk_event_controller_set_propagation_phase(motion_event_controller, GTK_PHASE_CAPTURE);
	gtk_widget_add_controller(GTK_WIDGET(priv->overlay_ftb_overlay), (GtkEventController*)motion_event_controller);

	GtkEventControllerScroll* scroll_event_controller = (GtkEventControllerScroll*)gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_NONE);
	// gtk_event_controller_set_propagation_phase(scroll_event_controller, GTK_PHASE_CAPTURE);
	gtk_widget_add_controller(GTK_WIDGET(priv->overlay_ftb_overlay), (GtkEventController*)scroll_event_controller);

	g_signal_connect(motion_event_controller, "enter", G_CALLBACK(rcw_floating_toolbar_on_enter), cnnwin);
	g_signal_connect(motion_event_controller, "leave", G_CALLBACK(rcw_floating_toolbar_on_leave), cnnwin);
	g_signal_connect(scroll_event_controller, "scroll", G_CALLBACK(rcw_floating_toolbar_on_scroll), cnnwin);
// 	gtk_widget_add_events(
// 		GTK_WIDGET(priv->overlay_ftb_overlay),
// 		GDK_SCROLL_MASK
// #if GTK_CHECK_VERSION(3, 4, 0)
// 		| GDK_SMOOTH_SCROLL_MASK
// #endif
// 		); TODO GTK4

	/* Add drag and drop capabilities to the source */
	// gtk_drag_source_set(GTK_WIDGET(priv->overlay_ftb_overlay), GDK_BUTTON1_MASK,
	// 		    dnd_targets_ftb, sizeof dnd_targets_ftb / sizeof *dnd_targets_ftb, GDK_ACTION_MOVE); TODO GTK4
	// g_signal_connect_after(GTK_WIDGET(priv->overlay_ftb_overlay), "drag-begin", G_CALLBACK(rcw_ftb_drag_begin), cnnwin);

	if (remmina_pref.fullscreen_toolbar_visibility == FLOATING_TOOLBAR_VISIBILITY_DISABLE)
		/* toolbar in fullscreenmode disabled, hide everything */
		gtk_widget_hide(fr);
}


static gboolean rcw_ftb_drag_drop(GtkWidget *widget, GtkDragSource *context,
				  gint x, gint y, guint time, RemminaConnectionWindow *cnnwin)
{
	TRACE_CALL(__func__);
	GtkAllocation wa;
	gint new_floating_toolbar_placement;
	RemminaConnectionObject *cnnobj;

	gtk_widget_get_allocation(widget, &wa);

	if (y >= wa.height / 2)
		new_floating_toolbar_placement = FLOATING_TOOLBAR_PLACEMENT_BOTTOM;
	else
		new_floating_toolbar_placement = FLOATING_TOOLBAR_PLACEMENT_TOP;

	//gtk_drag_finish(context, TRUE, TRUE, time); TODO GTK4

	if (new_floating_toolbar_placement != remmina_pref.floating_toolbar_placement) {
		/* Destroy and recreate the FTB */
		remmina_pref.floating_toolbar_placement = new_floating_toolbar_placement;
		remmina_pref_save();
		rcw_create_overlay_ftb_overlay(cnnwin);
		cnnobj = rcw_get_visible_cnnobj(cnnwin);
		if (cnnobj) rco_update_toolbar(cnnobj);
	}

	return TRUE;
}

static void rcw_ftb_drag_begin(GtkWidget *widget, GtkDragSource *context, gpointer user_data)
{
	TRACE_CALL(__func__);

	cairo_surface_t *surface;
	cairo_t *cr;
	GtkAllocation wa;
	double dashes[] = { 10 };

	gtk_widget_get_allocation(widget, &wa);

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, wa.width, wa.height);
	cr = cairo_create(surface);
	cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
	cairo_set_line_width(cr, 2);
	cairo_set_dash(cr, dashes, 1, 0);
	cairo_rectangle(cr, 0, 0, wa.width, wa.height);
	cairo_stroke(cr);
	cairo_destroy(cr);

	//gtk_drag_set_icon_surface(context, surface); TODO GTK4
}

RemminaConnectionWindow *rcw_create_fullscreen(GtkWindow *old, gint view_mode)
{
	TRACE_CALL(__func__);
	RemminaConnectionWindow *cnnwin;
	GtkNotebook *notebook;

#if GTK_CHECK_VERSION(3, 22, 0)
	gint n_monitors;
	gint i;
	GdkMonitor *old_monitor = NULL;
	GdkDisplay *old_display;
	GdkSurface *old_window;
#endif
	gint full_screen_target_monitor;

	full_screen_target_monitor = FULL_SCREEN_TARGET_MONITOR_UNDEFINED;
	if (old) {
#if GTK_CHECK_VERSION(3, 22, 0)
		old_window = gtk_native_get_surface(gtk_widget_get_native(GTK_WIDGET(old)));
		old_display = gtk_widget_get_display(GTK_WIDGET(old));
		old_monitor = gdk_display_get_monitor_at_surface(old_display, old_window);

#else
	GtkNative* native = gtk_widget_get_native((GTK_WIDGET(cnnwin)));
	GdkSurface *window = gtk_native_get_surface(native);
		full_screen_target_monitor = gdk_screen_get_monitor_at_window(
			gdk_screen_get_default(),
			window));
#endif
	}
	if (full_screen_target_monitor == 0){
		full_screen_target_monitor = FULL_SCREEN_TARGET_MONITOR_ZERO;
	}
	cnnwin = rcw_new(TRUE, old_monitor);
	gtk_widget_set_name(GTK_WIDGET(cnnwin), "remmina-connection-window-fullscreen");
	gtk_widget_realize(GTK_WIDGET(cnnwin));
		g_signal_connect(G_OBJECT(gtk_native_get_surface(GTK_NATIVE(cnnwin))), "notify", G_CALLBACK(rcw_property_notification_check), cnnwin);

	if (!view_mode)
		view_mode = VIEWPORT_FULLSCREEN_MODE;

	notebook = rcw_create_notebook(cnnwin);

	cnnwin->priv->overlay = gtk_overlay_new();
	gtk_window_set_child(GTK_WINDOW(cnnwin), cnnwin->priv->overlay);
	gtk_overlay_set_child(GTK_OVERLAY(cnnwin->priv->overlay), GTK_WIDGET(notebook));
	gtk_widget_show(GTK_WIDGET(cnnwin->priv->overlay));

	cnnwin->priv->notebook = notebook;
	cnnwin->priv->view_mode = view_mode;
	cnnwin->priv->fss_view_mode = view_mode;

	/* Create the floating toolbar */
	rcw_create_overlay_ftb_overlay(cnnwin);
	/* Add drag and drop capabilities to the drop/dest target for floating toolbar */
	// gtk_drag_dest_set(GTK_WIDGET(cnnwin->priv->overlay), GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT,
	// 		  dnd_targets_ftb, sizeof dnd_targets_ftb / sizeof *dnd_targets_ftb, GDK_ACTION_MOVE); TODO GTK4
	//gtk_drag_dest_set_track_motion(GTK_WIDGET(cnnwin->priv->notebook), TRUE); TODO GTK4
	// g_signal_connect(GTK_WIDGET(cnnwin->priv->overlay), "drag-drop", G_CALLBACK(rcw_ftb_drag_drop), cnnwin);




	return cnnwin;
}

static gboolean rcw_hostkey_func(RemminaProtocolWidget *gp, guint keyval, gboolean release)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj = gp->cnnobj;
	RemminaConnectionWindowPriv *priv = cnnobj->cnnwin->priv;
	const RemminaProtocolFeature *feature;
	gint i;

	if (release) {
		if (remmina_pref.hostkey && keyval == remmina_pref.hostkey) {
			priv->hostkey_activated = FALSE;
			if (priv->hostkey_used)
				/* hostkey pressed + something else */
				return TRUE;
		}
		/* If hostkey is released without pressing other keys, we should execute the
		 * shortcut key which is the same as hostkey. Be default, this is grab/ungrab
		 * keyboard */
		else if (priv->hostkey_activated) {
			/* Trap all key releases when hostkey is pressed */
			/* hostkey pressed + something else */
			return TRUE;
		} else {
			/* Any key pressed, no hostkey */
			return FALSE;
		}
	} else if (remmina_pref.hostkey && keyval == remmina_pref.hostkey) {
		/** @todo Add callback for hostname transparent overlay #832 */
		priv->hostkey_activated = TRUE;
		priv->hostkey_used = FALSE;
		return TRUE;
	} else if (!priv->hostkey_activated) {
		/* Any key pressed, no hostkey */
		return FALSE;
	}

	priv->hostkey_used = TRUE;
	keyval = gdk_keyval_to_lower(keyval);
	if (keyval == GDK_KEY_Up || keyval == GDK_KEY_Down
	    || keyval == GDK_KEY_Left || keyval == GDK_KEY_Right) {
		GtkAdjustment *adjust;
		int pos;

		if (cnnobj->connected && GTK_IS_SCROLLED_WINDOW(cnnobj->scrolled_container)) {
			if (keyval == GDK_KEY_Up || keyval == GDK_KEY_Down)
				adjust = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(cnnobj->scrolled_container));
			else
				adjust = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(cnnobj->scrolled_container));

			if (keyval == GDK_KEY_Up || keyval == GDK_KEY_Left)
				pos = 0;
			else
				pos = gtk_adjustment_get_upper(adjust);

			gtk_adjustment_set_value(adjust, pos);
			if (keyval == GDK_KEY_Up || keyval == GDK_KEY_Down)
				gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(cnnobj->scrolled_container), adjust);
			else
				gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(cnnobj->scrolled_container), adjust);
		} else if (REMMINA_IS_SCROLLED_VIEWPORT(cnnobj->scrolled_container)) {
			RemminaScrolledViewport *gsv;
			GtkWidget *child;
			GdkSurface *gsvsurf;
			gint sz;
			GtkAdjustment *adj;
			gdouble value;

			// if (!GTK_IS_BIN(cnnobj->scrolled_container))
			// 	return FALSE;

			gsv = REMMINA_SCROLLED_VIEWPORT(cnnobj->scrolled_container);
			child = gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(gsv));
			if (!GTK_IS_VIEWPORT(child))
				return FALSE;

			GtkNative* native = gtk_widget_get_native((GTK_WIDGET(gsv)));
			GdkSurface *surface = gtk_native_get_surface(native);
			gsvsurf = surface;
			if (!gsv)
				return FALSE;

			if (keyval == GDK_KEY_Up || keyval == GDK_KEY_Down) {
				sz = gdk_surface_get_height(gsvsurf) + 2; // Add 2px of black scroll border
				adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(child));
			} else {
				sz = gdk_surface_get_height(gsvsurf) + 2; // Add 2px of black scroll border
				adj = gtk_scrollable_get_hadjustment(GTK_SCROLLABLE(child));
			}

			if (keyval == GDK_KEY_Up || keyval == GDK_KEY_Left)
				value = 0;
			else
				value = gtk_adjustment_get_upper(GTK_ADJUSTMENT(adj)) - (gdouble)sz + 2.0;

			gtk_adjustment_set_value(GTK_ADJUSTMENT(adj), value);
		}
	}

	if (keyval == remmina_pref.shortcutkey_fullscreen && !extrahardening) {
		switch (priv->view_mode) {
		case SCROLLED_WINDOW_MODE:
			rcw_switch_viewmode(cnnobj->cnnwin, priv->fss_view_mode);
			break;
		case SCROLLED_FULLSCREEN_MODE:
		case VIEWPORT_FULLSCREEN_MODE:
			rcw_switch_viewmode(cnnobj->cnnwin, SCROLLED_WINDOW_MODE);
			break;
		default:
			break;
		}
	} else if (keyval == remmina_pref.shortcutkey_autofit && !extrahardening) {
		if (priv->toolitem_autofit && gtk_widget_is_sensitive(GTK_WIDGET(priv->toolitem_autofit)))
			rcw_toolbar_autofit(GTK_WIDGET(gp), cnnobj->cnnwin);
	} else if (keyval == remmina_pref.shortcutkey_nexttab && !extrahardening) {
		i = gtk_notebook_get_current_page(GTK_NOTEBOOK(priv->notebook)) + 1;
		if (i >= gtk_notebook_get_n_pages(GTK_NOTEBOOK(priv->notebook)))
			i = 0;
		gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook), i);
	} else if (keyval == remmina_pref.shortcutkey_prevtab && !extrahardening) {
		i = gtk_notebook_get_current_page(GTK_NOTEBOOK(priv->notebook)) - 1;
		if (i < 0)
			i = gtk_notebook_get_n_pages(GTK_NOTEBOOK(priv->notebook)) - 1;
		gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook), i);
	} else if (keyval == remmina_pref.shortcutkey_clipboard && !extrahardening) {
		if (remmina_protocol_widget_plugin_receives_keystrokes(REMMINA_PROTOCOL_WIDGET(cnnobj->proto))) {
			remmina_protocol_widget_send_clipboard((RemminaProtocolWidget*)cnnobj->proto, G_OBJECT(cnnobj->proto));
		}
	} else if (keyval == remmina_pref.shortcutkey_scale && !extrahardening) {
		if (gtk_widget_is_sensitive(GTK_WIDGET(priv->toolitem_scale))) {
			gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(priv->toolitem_scale),
				!gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(
						priv->toolitem_scale)));
		}
	} else if (keyval == remmina_pref.shortcutkey_grab && !extrahardening) {
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(priv->toolitem_grab),
			!gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(
					priv->toolitem_grab)));
	} else if (keyval == remmina_pref.shortcutkey_minimize && !extrahardening) {
		rcw_toolbar_minimize(GTK_WIDGET(gp),
				     cnnobj->cnnwin);
	} else if (keyval == remmina_pref.shortcutkey_viewonly && !extrahardening) {
		remmina_file_set_int(cnnobj->remmina_file, "viewonly",
				     (remmina_file_get_int(cnnobj->remmina_file, "viewonly", 0)
				      == 0) ? 1 : 0);
	} else if (keyval == remmina_pref.shortcutkey_screenshot && !extrahardening) {
		rcw_toolbar_screenshot(GTK_WIDGET(gp),
				       cnnobj->cnnwin);
	} else if (keyval == remmina_pref.shortcutkey_disconnect && !extrahardening) {
		rco_disconnect_current_page(cnnobj);
	} else if (keyval == remmina_pref.shortcutkey_toolbar && !extrahardening) {
		if (priv->view_mode == SCROLLED_WINDOW_MODE) {
			remmina_pref.hide_connection_toolbar =
				!remmina_pref.hide_connection_toolbar;
			rcw_set_toolbar_visibility(cnnobj->cnnwin);
		}
	} else {
		for (feature =
			     remmina_protocol_widget_get_features(
				     REMMINA_PROTOCOL_WIDGET(
					     cnnobj->proto));
		     feature && feature->type;
		     feature++) {
			if (feature->type
			    == REMMINA_PROTOCOL_FEATURE_TYPE_TOOL
			    && GPOINTER_TO_UINT(
				    feature->opt3)
			    == keyval) {
				remmina_protocol_widget_call_feature_by_ref(
					REMMINA_PROTOCOL_WIDGET(
						cnnobj->proto),
					feature);
				break;
			}
		}
	}
	/* If a keypress makes the current cnnobj to move to another window,
	 * priv is now invalid. So we can no longer use priv here */
	cnnobj->cnnwin->priv->hostkey_activated = FALSE;

	/* Trap all keypresses when hostkey is pressed */
	return TRUE;
}

static RemminaConnectionWindow *rcw_find(RemminaFile *remminafile)
{
	TRACE_CALL(__func__);
	const gchar *tag;

	switch (remmina_pref.tab_mode) {
	case REMMINA_TAB_BY_GROUP:
		tag = remmina_file_get_string(remminafile, "group");
		break;
	case REMMINA_TAB_BY_PROTOCOL:
		tag = remmina_file_get_string(remminafile, "protocol");
		break;
	case REMMINA_TAB_ALL:
		tag = NULL;
		break;
	case REMMINA_TAB_NONE:
	default:
		return NULL;
	}
	return RCW(remmina_widget_pool_find(REMMINA_TYPE_CONNECTION_WINDOW, tag));
}

gboolean rcw_delayed_window_present(gpointer user_data)
{
	RemminaConnectionWindow *cnnwin = (RemminaConnectionWindow *)user_data;

	if (cnnwin) {
		gtk_window_present_with_time(GTK_WINDOW(cnnwin), (guint32)(g_get_monotonic_time() / 1000));
		rcw_grab_focus(cnnwin);
	}
	cnnwin->priv->dwp_eventsourceid = 0;
	return G_SOURCE_REMOVE;
}

void rco_on_connect(RemminaProtocolWidget *gp, RemminaConnectionObject *cnnobj)
{
	TRACE_CALL(__func__);

	REMMINA_DEBUG("Connect signal emitted");
	GSimpleActionGroup *actions;
	/* This signal handler is called by a plugin when it’s correctly connected
	 * (and authenticated) */

	cnnobj->connected = TRUE;

	remmina_protocol_widget_set_hostkey_func(REMMINA_PROTOCOL_WIDGET(cnnobj->proto),
						 (RemminaHostkeyFunc)rcw_hostkey_func);

	/** Remember recent list for quick connect, and save the current date
	 * in the last_used field.
	 */
	if (remmina_file_get_filename(cnnobj->remmina_file) == NULL)
		remmina_pref_add_recent(remmina_file_get_string(cnnobj->remmina_file, "protocol"),
					remmina_file_get_string(cnnobj->remmina_file, "server"));
	REMMINA_DEBUG("We save the last successful connection date");
	//remmina_file_set_string(cnnobj->remmina_file, "last_success", last_success);
	remmina_file_state_last_success(cnnobj->remmina_file);
	//REMMINA_DEBUG("Last connection made on %s.", last_success);

	REMMINA_DEBUG("Saving credentials");
	/* Save credentials */
	remmina_file_save(cnnobj->remmina_file);

	if (cnnobj->cnnwin->priv->floating_toolbar_widget)
		gtk_widget_show(cnnobj->cnnwin->priv->floating_toolbar_widget);

	actions = g_simple_action_group_new();	
	g_action_map_add_action_entries(G_ACTION_MAP(actions), rcw_actions, G_N_ELEMENTS(rcw_actions), cnnobj);
	gtk_widget_insert_action_group(GTK_WIDGET(cnnobj->cnnwin), "rcw", G_ACTION_GROUP(actions));
	rcw_create_toolbar_actions(actions, cnnobj->cnnwin);
	rcw_create_toolbar_connection_menu(actions, cnnobj->cnnwin);
	cnnobj->action_group = actions;

	rco_update_toolbar(cnnobj);

	REMMINA_DEBUG("Trying to present the window");
	/* Try to present window */
	cnnobj->cnnwin->priv->dwp_eventsourceid = g_timeout_add(200, rcw_delayed_window_present, (gpointer)cnnobj->cnnwin);
}

static void cb_lasterror_confirmed(void *cbdata, int btn)
{
	TRACE_CALL(__func__);
	rco_closewin((RemminaProtocolWidget *)cbdata);
}

void rco_on_disconnect(RemminaProtocolWidget *gp, gpointer data)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj = gp->cnnobj;
	RemminaConnectionWindowPriv *priv = cnnobj->cnnwin->priv;
	GtkWidget *pparent;

	REMMINA_DEBUG("Disconnect signal received on RemminaProtocolWidget");
	/* Detach the protocol widget from the notebook now, or we risk that a
	 * window delete will destroy cnnobj->proto before we complete disconnection.
	 */
	pparent = gtk_widget_get_parent(cnnobj->proto);
	if (pparent != NULL) {
		g_object_ref(cnnobj->proto);
		//gtk_container_remove(GTK_CONTAINER(pparent), cnnobj->proto);
	}

	cnnobj->connected = FALSE;

	if (remmina_pref.save_view_mode) {
		if (cnnobj->cnnwin)
			remmina_file_set_int(cnnobj->remmina_file, "viewmode", cnnobj->cnnwin->priv->view_mode);
		remmina_file_save(cnnobj->remmina_file);
	}

	rcw_kp_ungrab(cnnobj->cnnwin);
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON(priv->toolitem_grab),
		FALSE);

	if (remmina_protocol_widget_has_error(gp)) {
		/* We cannot close window immediately, but we must show a message panel */
		RemminaMessagePanel *mp;
		/* Destroy scrolled_container (and viewport) and all its children the plugin created
		 * on it, so they will not receive GUI signals */
		if (cnnobj->scrolled_container) {
			gtk_widget_unparent(cnnobj->scrolled_container);
			//g_object_unref(cnnobj->scrolled_container);
			cnnobj->scrolled_container = NULL;
		}
		cnnobj->viewport = NULL;
		mp = remmina_message_panel_new();
		remmina_message_panel_setup_message(mp, remmina_protocol_widget_get_error_message(gp), cb_lasterror_confirmed, gp);
		rco_show_message_panel(gp->cnnobj, mp);
		REMMINA_DEBUG("Could not disconnect");
	} else {
		rco_closewin(gp);
		REMMINA_DEBUG("Disconnected");
	}
}

void rco_on_desktop_resize(RemminaProtocolWidget *gp, gpointer data)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj = gp->cnnobj;

	if (cnnobj && cnnobj->cnnwin && cnnobj->cnnwin->priv->view_mode != SCROLLED_WINDOW_MODE)
		rco_check_resize(cnnobj);
}

void rco_on_update_align(RemminaProtocolWidget *gp, gpointer data)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj = gp->cnnobj;

	remmina_protocol_widget_update_alignment(cnnobj);
}

void rco_on_lock_dynres(RemminaProtocolWidget *gp, gpointer data)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj = gp->cnnobj;

	cnnobj->dynres_unlocked = FALSE;
	rco_update_toolbar(cnnobj);
}

void rco_on_unlock_dynres(RemminaProtocolWidget *gp, gpointer data)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj = gp->cnnobj;

	cnnobj->dynres_unlocked = TRUE;
	rco_update_toolbar(cnnobj);
}

gboolean rcw_open_from_filename(const gchar *filename)
{
	TRACE_CALL(__func__);
	RemminaFile *remminafile;
	GtkWidget *dialog;

	remminafile = remmina_file_manager_load_file(filename);
	if (remminafile) {
		if (remmina_file_get_int (remminafile, "profile-lock", FALSE)
				&& remmina_unlock_new(remmina_main_get_window()) == 0)
			return FALSE;
		rcw_open_from_file(remminafile);
		return TRUE;
	} else {
		dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						_("The file “%s” is corrupted, unreadable, or could not be found."), filename);
		g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(gtk_window_destroy), NULL);
		gtk_widget_show(dialog);
		remmina_widget_pool_register(dialog);
		return FALSE;
	}
}

static gboolean open_connection_last_stage(gpointer user_data)
{
	RemminaProtocolWidget *gp = (RemminaProtocolWidget *)user_data;

	/* Now we have an allocated size for our RemminaProtocolWidget. We can proceed with the connection */
	remmina_protocol_widget_update_remote_resolution(gp);
	remmina_protocol_widget_open_connection(gp);
	rco_check_resize(gp->cnnobj);

	return FALSE;
}

static void rpw_size_allocated_on_connection(GtkWidget *w, gpointer user_data)
{
	RemminaProtocolWidget *gp = (RemminaProtocolWidget *)w;

	/* Disconnect signal handler to avoid to be called again after a normal resize */
	// g_signal_handler_disconnect(w, gp->cnnobj->deferred_open_size_allocate_handler);

	/* Allow extra 100 ms for size allocation (do we really need it?) */
	g_timeout_add(100, open_connection_last_stage, gp);

	return;
}

void rcw_open_from_file(RemminaFile *remminafile)
{
	TRACE_CALL(__func__);
	rcw_open_from_file_full(remminafile, NULL, NULL, NULL);
}

static void set_label_selectable(gpointer data, gpointer user_data)
{
	TRACE_CALL(__func__);
	GtkWidget *widget = GTK_WIDGET(data);

	if (GTK_IS_LABEL(widget))
		gtk_label_set_selectable(GTK_LABEL(widget), TRUE);
}

/**
 * @brief These define the response id's of the
 *	  gtksocket-is-not-available-warning-dialog buttons.
 */
enum GTKSOCKET_NOT_AVAIL_RESPONSE_TYPE {
	GTKSOCKET_NOT_AVAIL_RESPONSE_OPEN_BROWSER = 0,
	GTKSOCKET_NOT_AVAIL_RESPONSE_NUM
};

/**
 * @brief Gets called if the user interacts with the
 *	  gtksocket-is-not-available-warning-dialog
 */
static void rcw_gtksocket_not_available_dialog_response(GtkDialog *			self,
							gint				response_id,
							RemminaConnectionObject *	cnnobj)
{
	TRACE_CALL(__func__);

	// GError *error = NULL;

	if (response_id == GTKSOCKET_NOT_AVAIL_RESPONSE_OPEN_BROWSER) {
		// gtk_show_uri_on_window(
		// 	NULL,
		// 	// TRANSLATORS: This should be a link to the Remmina wiki page:
		// 	// TRANSLATORS: 'GtkSocket feature is not available'.
		// 	_("https://gitlab.com/Remmina/Remmina/-/wikis/GtkSocket-feature-is-not-available-in-a-Wayland-session"),
		// 	GDK_CURRENT_TIME, &error
		// 	); TODO GTK4
	}

	// Close the current page since it's useless without GtkSocket.
	// The user would need to manually click the close button.
	if (cnnobj) rco_disconnect_current_page(cnnobj);

	gtk_window_destroy(GTK_WINDOW(self));
}

GtkWidget *rcw_open_from_file_full(RemminaFile *remminafile, GCallback disconnect_cb, gpointer data, guint *handler)
{
	TRACE_CALL(__func__);
	RemminaConnectionObject *cnnobj;

	gint ret;
	GtkWidget *dialog;
	GtkWidget *newpage;
	gint width, height;
	gboolean maximize;
	gint view_mode;
	const gchar *msg;
	RemminaScaleMode scalemode;

	if (disconnect_cb) {
		g_print("disconnect_cb is deprecated inside rcw_open_from_file_full() and should be null\n");
		return NULL;
	}


	/* Create the RemminaConnectionObject */
	cnnobj = g_new0(RemminaConnectionObject, 1);
	cnnobj->remmina_file = remminafile;

	/* Create the RemminaProtocolWidget */
	cnnobj->proto = remmina_protocol_widget_new();
	gtk_widget_set_hexpand(cnnobj->proto, TRUE);
	gtk_widget_set_vexpand(cnnobj->proto, TRUE);
	remmina_protocol_widget_setup((RemminaProtocolWidget *)cnnobj->proto, remminafile, cnnobj);
	if (remmina_protocol_widget_has_error((RemminaProtocolWidget *)cnnobj->proto)) {
		GtkWindow *wparent;
		wparent = remmina_main_get_window();
		msg = remmina_protocol_widget_get_error_message((RemminaProtocolWidget *)cnnobj->proto);
		dialog = gtk_message_dialog_new(wparent, GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", msg);
		//gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_window_destroy(GTK_WINDOW(dialog));
		/* We should destroy cnnobj->proto and cnnobj now… TODO: Fix this leak */
		return NULL;
	}



	/* Set a name for the widget, for CSS selector */
	gtk_widget_set_name(GTK_WIDGET(cnnobj->proto), "remmina-protocol-widget");

	gtk_widget_set_halign(GTK_WIDGET(cnnobj->proto), GTK_ALIGN_FILL);
	gtk_widget_set_valign(GTK_WIDGET(cnnobj->proto), GTK_ALIGN_FILL);

	if (data)
		g_object_set_data(G_OBJECT(cnnobj->proto), "user-data", data);

	view_mode = remmina_file_get_int(cnnobj->remmina_file, "viewmode", 0);
	if (kioskmode)
		view_mode = VIEWPORT_FULLSCREEN_MODE;
	gint ismultimon = remmina_file_get_int(cnnobj->remmina_file, "multimon", 0);

	if (ismultimon)
		view_mode = VIEWPORT_FULLSCREEN_MODE;

	if (fullscreen)
		view_mode = VIEWPORT_FULLSCREEN_MODE;

	/* Create the viewport to make the RemminaProtocolWidget scrollable */
	cnnobj->viewport = gtk_viewport_new(NULL, NULL);
	gtk_widget_set_name(cnnobj->viewport, "remmina-cw-viewport");
	gtk_widget_show(cnnobj->viewport);
	//gtk_container_set_border_width(GTK_CONTAINER(cnnobj->viewport), 0);
	//gtk_viewport_set_shadow_type(GTK_VIEWPORT(cnnobj->viewport), GTK_SHADOW_NONE);

	/* Create the scrolled container */
	scalemode = get_current_allowed_scale_mode(cnnobj, NULL, NULL);
	cnnobj->scrolled_container = rco_create_scrolled_container(scalemode, view_mode);

	if (GTK_IS_SCROLLED_WINDOW(cnnobj->scrolled_container)){
		gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(cnnobj->scrolled_container), cnnobj->viewport);
	}
	else{
		gtk_box_append(GTK_BOX(cnnobj->scrolled_container), cnnobj->viewport);
	}
	
	/* Determine whether the plugin can scale or not. If the plugin can scale and we do
	 * not want to expand, then we add a GtkAspectFrame to maintain aspect ratio during scaling */
	cnnobj->plugin_can_scale = remmina_plugin_manager_query_feature_by_type(REMMINA_PLUGIN_TYPE_PROTOCOL,
										remmina_file_get_string(remminafile, "protocol"),
										REMMINA_PROTOCOL_FEATURE_TYPE_SCALE);

	cnnobj->aspectframe = NULL;
	gtk_viewport_set_child(GTK_VIEWPORT(cnnobj->viewport), cnnobj->proto);

	/* Determine whether this connection will be put on a new window
	 * or in an existing one */
	cnnobj->cnnwin = rcw_find(remminafile);
	if (!cnnobj->cnnwin) {
		/* Connection goes on a new toplevel window */
		switch (view_mode) {
		case SCROLLED_FULLSCREEN_MODE:
		case VIEWPORT_FULLSCREEN_MODE:
			cnnobj->cnnwin = rcw_create_fullscreen(NULL, view_mode);
			rcw_update_tag(cnnobj->cnnwin, cnnobj);
			rcw_append_new_page(cnnobj->cnnwin, cnnobj);
			GtkWindowGroup *wingrp = gtk_window_group_new();
			gtk_window_group_add_window(wingrp, GTK_WINDOW(cnnobj->cnnwin));
			gtk_window_set_transient_for(GTK_WINDOW(cnnobj->cnnwin), NULL);
			break;
		case SCROLLED_WINDOW_MODE:
		default:
			width = remmina_file_get_int(cnnobj->remmina_file, "window_width", 640);
			height = remmina_file_get_int(cnnobj->remmina_file, "window_height", 480);
			maximize = remmina_file_get_int(cnnobj->remmina_file, "window_maximize", FALSE) ? TRUE : FALSE;
			cnnobj->cnnwin = rcw_create_scrolled(width, height, maximize);
			rcw_update_tag(cnnobj->cnnwin, cnnobj);
			rcw_append_new_page(cnnobj->cnnwin, cnnobj);
			break;
		}
		
	} else {
		newpage = rcw_append_new_page(cnnobj->cnnwin, cnnobj);
		gtk_window_present(GTK_WINDOW(cnnobj->cnnwin));
		nb_set_current_page(cnnobj->cnnwin->priv->notebook, newpage);
	}

	// Do not call remmina_protocol_widget_update_alignment(cnnobj); here or cnnobj->proto will not fill its parent size
	// and remmina_protocol_widget_update_remote_resolution() cannot autodetect available space

	gtk_widget_show(cnnobj->proto);

	GtkEventControllerMotion* motion_event_controller = (GtkEventControllerMotion*)gtk_event_controller_motion_new();
	gtk_event_controller_set_propagation_phase(motion_event_controller, GTK_PHASE_CAPTURE);
	gtk_widget_add_controller(GTK_WIDGET(cnnobj->proto), GTK_EVENT_CONTROLLER(motion_event_controller));


	g_signal_connect(G_OBJECT(cnnobj->proto), "connect", G_CALLBACK(rco_on_connect), cnnobj);
	g_signal_connect(G_OBJECT(cnnobj->proto), "disconnect", G_CALLBACK(rco_on_disconnect), NULL);
	g_signal_connect(G_OBJECT(cnnobj->proto), "desktop-resize", G_CALLBACK(rco_on_desktop_resize), NULL);
	g_signal_connect(G_OBJECT(cnnobj->proto), "update-align", G_CALLBACK(rco_on_update_align), NULL);
	g_signal_connect(G_OBJECT(cnnobj->proto), "lock-dynres", G_CALLBACK(rco_on_lock_dynres), NULL);
	g_signal_connect(G_OBJECT(cnnobj->proto), "unlock-dynres", G_CALLBACK(rco_on_unlock_dynres), NULL);
	g_signal_connect(motion_event_controller, "enter", G_CALLBACK(rco_enter_protocol_widget), cnnobj);
	g_signal_connect(motion_event_controller, "leave", G_CALLBACK(rco_leave_protocol_widget), cnnobj);

	if (!remmina_pref.save_view_mode)
		remmina_file_set_int(cnnobj->remmina_file, "viewmode", remmina_pref.default_mode);


	/* If it is a GtkSocket plugin and X11 is not available, we inform the
	 * user and close the connection */
	ret = remmina_plugin_manager_query_feature_by_type(REMMINA_PLUGIN_TYPE_PROTOCOL,
							   remmina_file_get_string(remminafile, "protocol"),
							   REMMINA_PROTOCOL_FEATURE_TYPE_GTKSOCKET);
	if (ret && !remmina_gtksocket_available()) {
		gchar *title = _("Warning: This plugin requires GtkSocket, but this "
				 "feature is unavailable in a Wayland session.");

		gchar *err_msg =
			// TRANSLATORS: This should be a link to the Remmina wiki page:
			// 'GtkSocket feature is not available'.
			_("Plugins relying on GtkSocket can't run in a "
			  "Wayland session.\nFor more info and a possible "
			  "workaround, please visit the Remmina wiki at:\n\n"
			  "https://gitlab.com/Remmina/Remmina/-/wikis/GtkSocket-feature-is-not-available-in-a-Wayland-session");

		dialog = gtk_message_dialog_new(
			GTK_WINDOW(cnnobj->cnnwin),
			GTK_DIALOG_MODAL,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_OK,
			"%s", title);

		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s",
							 err_msg);
		gtk_dialog_add_button(GTK_DIALOG(dialog), _("Open in web browser"),
				      GTKSOCKET_NOT_AVAIL_RESPONSE_OPEN_BROWSER);

		REMMINA_CRITICAL(g_strdup_printf("%s\n%s", title, err_msg));

		g_signal_connect(G_OBJECT(dialog),
				 "response",
				 G_CALLBACK(rcw_gtksocket_not_available_dialog_response),
				 cnnobj);

		// Make Text selectable. Usefull because of the link in the text.
		// GtkWidget *area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dialog));
		// GtkWidget *box = (GtkWidget *)area;
		// GList *children = gtk_container_get_children(box);
		// g_list_foreach(children, set_label_selectable, NULL);
		// g_list_free(children);

		gtk_widget_show(dialog);

		return NULL;    /* Should we destroy something before returning? */
	}

	if (cnnobj->cnnwin->priv->floating_toolbar_widget)
		gtk_widget_show(cnnobj->cnnwin->priv->floating_toolbar_widget);

	if (remmina_protocol_widget_has_error((RemminaProtocolWidget *)cnnobj->proto)) {
		printf("OK, an error occurred in initializing the protocol plugin before connecting. The error is %s.\n"
		       "TODO: Put this string as an error to show", remmina_protocol_widget_get_error_message((RemminaProtocolWidget *)cnnobj->proto));
		return cnnobj->proto;
	}


	/* GTK window setup is done here, and we are almost ready to call remmina_protocol_widget_open_connection().
	 * But size has not yet been allocated by GTK
	 * to the widgets. If we are in RES_USE_INITIAL_WINDOW_SIZE resolution mode or scale is REMMINA_PROTOCOL_WIDGET_SCALE_MODE_DYNRES,
	 * we should wait for a size allocation from GTK for cnnobj->proto
	 * before connecting */



	//Seems to have changed in GTK4. Is it still necessary?
	// cnnobj->deferred_open_size_allocate_handler = g_signal_connect(G_OBJECT(cnnobj->proto), "size-allocate", G_CALLBACK(rpw_size_allocated_on_connection), NULL);
	//rpw_size_allocated_on_connection(cnnobj->proto, NULL);
	open_connection_last_stage(cnnobj->proto);

	return cnnobj->proto;
}

GtkWindow *rcw_get_gtkwindow(RemminaConnectionObject *cnnobj)
{
	return &cnnobj->cnnwin->window;
}
GtkWidget *rcw_get_gtkviewport(RemminaConnectionObject *cnnobj)
{
	return cnnobj->viewport;
}

void rcw_set_delete_confirm_mode(RemminaConnectionWindow *cnnwin, RemminaConnectionWindowOnDeleteConfirmMode mode)
{
	TRACE_CALL(__func__);
	cnnwin->priv->on_delete_confirm_mode = mode;
}

/**
 * Deletes a RemminaMessagePanel from the current cnnobj
 * and if it was visible, make visible the last remaining one.
 */
void rco_destroy_message_panel(RemminaConnectionObject *cnnobj, RemminaMessagePanel *mp)
{
	TRACE_CALL(__func__);
	RemminaMessagePanel *lastPanel;
	gboolean was_visible;
	GtkWidget *page;
	GtkWidget* child;
	page = nb_find_page_by_cnnobj(cnnobj->cnnwin->priv->notebook, cnnobj);


	child = gtk_widget_get_first_child(page);
	while(child != NULL){
		if ((RemminaMessagePanel *)child == mp){
				break;
		}
		child = gtk_widget_get_next_sibling(child);
	}

	if (child == NULL) {
		printf("Remmina: Warning. There was a request to destroy a RemminaMessagePanel that is not on the page\n");
		return;
	}
	was_visible = gtk_widget_is_visible(GTK_WIDGET(mp));
	gtk_box_remove(GTK_BOX(page), GTK_WIDGET(mp));

	/* And now, show the last remaining message panel, if needed */
	if (was_visible) {


		lastPanel = NULL;
		child = gtk_widget_get_first_child(page);
		while(child != NULL){
			if  (G_TYPE_CHECK_INSTANCE_TYPE(child, REMMINA_TYPE_MESSAGE_PANEL)){
					lastPanel = (RemminaMessagePanel*)child;
			}
			child = gtk_widget_get_next_sibling(child);
		}

		if (lastPanel)
			gtk_widget_show(GTK_WIDGET(lastPanel));
	}
}

/**
 * Each cnnobj->page can have more than one RemminaMessagePanel, but 0 or 1 are visible.
 *
 * This function adds a RemminaMessagePanel to cnnobj->page, move it to top,
 * and makes it the only visible one.
 */
void rco_show_message_panel(RemminaConnectionObject *cnnobj, RemminaMessagePanel *mp)
{
	TRACE_CALL(__func__);
	// GList *childs, *cc;
	GtkWidget *page, *child;

	/* Hides all RemminaMessagePanels childs of cnnobj->page */
	page = nb_find_page_by_cnnobj(cnnobj->cnnwin->priv->notebook, cnnobj);
	// childs = gtk_container_get_children(GTK_CONTAINER(page));
	// cc = g_list_first(childs);
	// while (cc != NULL) {
	// 	if (G_TYPE_CHECK_INSTANCE_TYPE(cc->data, REMMINA_TYPE_MESSAGE_PANEL))
	// 		gtk_widget_hide(GTK_WIDGET(cc->data));
	// 	cc = g_list_next(cc);
	// }
	// g_list_free(childs);

	child = gtk_widget_get_first_child(page);
	while(child != NULL){
		if (G_TYPE_CHECK_INSTANCE_TYPE(child, REMMINA_TYPE_MESSAGE_PANEL)){
				gtk_widget_hide(child);
		}
		child = gtk_widget_get_next_sibling(child);
	}

	/* Add the new message panel at the top of cnnobj->page */
	gtk_box_append(GTK_BOX(page), GTK_WIDGET(mp));
	gtk_box_reorder_child_after(GTK_BOX(page), GTK_WIDGET(mp), NULL);

	/* Show the message panel */
	gtk_widget_show(GTK_WIDGET(mp));

	/* Focus the correct field of the RemminaMessagePanel */
	remmina_message_panel_focus_auth_entry(mp);
}
