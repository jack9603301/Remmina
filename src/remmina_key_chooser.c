/*
 * Remmina - The GTK+ Remote Desktop Client
 * Copyright (C) 2009-2010 Vic Lee
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

#include <glib/gi18n.h>
#include "remmina_key_chooser.h"
#include "remmina_public.h"
#include "remmina/remmina_trace_calls.h"

/* Handle key-presses on the GtkEventBox */
static gboolean remmina_key_chooser_dialog_on_key_press(GtkEventControllerKey* self,
  guint keyval,
  guint keycode,
  GdkModifierType state, RemminaKeyChooserArguments *arguments)
{
	TRACE_CALL(__func__);
	if (!arguments->use_modifiers || !gdk_key_event_is_modifier(gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(self)))) {
		arguments->val = remmina_key_chooser_get_value(keyval, state);
		gtk_dialog_response(arguments->dialog, keyval == GDK_KEY_Escape ? GTK_RESPONSE_CANCEL : GTK_RESPONSE_OK);
		
	}
	return TRUE;
}

static void remmina_key_chooser_dialog_on_response(GtkDialog* self,
  gint response_id,
  gpointer user_data)
{
	RemminaKeyChooserArguments* args = user_data;
	if (response_id == GTK_RESPONSE_REJECT ) {
		gtk_button_set_label(GTK_BUTTON(args->widget), KEY_CHOOSER_NONE);
	}
	else if(response_id == GTK_RESPONSE_OK ){
		gtk_button_set_label(GTK_BUTTON(args->widget), args->val);
	}
	gtk_window_destroy(GTK_WINDOW(self)); 
  }

/* Show a key chooser dialog and return the keyval for the selected key */
void remmina_key_chooser_new(GtkWindow *parent_window, gboolean use_modifiers, GtkWidget *widget)
{
	TRACE_CALL(__func__);
	GtkBuilder *builder = remmina_public_gtk_builder_new_from_resource ("/org/remmina/Remmina/src/../data/ui/remmina_key_chooser.glade");
	GtkDialog *dialog;
	RemminaKeyChooserArguments *arguments;
	arguments = g_new0(RemminaKeyChooserArguments, 1);
	arguments->use_modifiers = use_modifiers;
	arguments->widget = widget;
	arguments->val = gtk_button_get_label(GTK_BUTTON(widget));


	/* Setup the dialog */
	dialog = GTK_DIALOG(gtk_builder_get_object(builder, "KeyChooserDialog"));
	arguments->dialog = dialog;
	gtk_window_set_transient_for(GTK_WINDOW(dialog), parent_window);
	/* Connect the GtkEventBox signal */
	GtkEventControllerKey* key_event_controller = (GtkEventControllerKey*)gtk_event_controller_key_new();
	// GtkBox* eventbox_key_chooser = GTK_BOX(gtk_builder_get_object(builder, "eventbox_key_chooser"));
	gtk_widget_add_controller(GTK_WIDGET(dialog), GTK_EVENT_CONTROLLER(key_event_controller));
	gtk_window_set_modal(GTK_WINDOW(dialog), true);

	g_signal_connect(key_event_controller, "key-released",
		G_CALLBACK(remmina_key_chooser_dialog_on_key_press), arguments);
	g_signal_connect(dialog, "response",
		G_CALLBACK(remmina_key_chooser_dialog_on_response), arguments);
	gtk_widget_show(dialog);
}

/* Get the uppercase character value of a keyval */
gchar* remmina_key_chooser_get_value(guint keyval, guint state)
{
	TRACE_CALL(__func__);

	if (!keyval)
		return g_strdup(KEY_CHOOSER_NONE);

	return g_strdup_printf("%s%s%s%s%s%s%s",
		(state & GDK_SHIFT_MASK) ? KEY_MODIFIER_SHIFT : "",
		(state & GDK_CONTROL_MASK) ? KEY_MODIFIER_CTRL : "",
		(state & GDK_ALT_MASK) ? KEY_MODIFIER_ALT : "",
		(state & GDK_SUPER_MASK) ? KEY_MODIFIER_SUPER : "",
		(state & GDK_HYPER_MASK) ? KEY_MODIFIER_HYPER : "",
		(state & GDK_META_MASK) ? KEY_MODIFIER_META : "",
		gdk_keyval_name(gdk_keyval_to_upper(keyval)));
}

/* Get the keyval of a (lowercase) character value */
guint remmina_key_chooser_get_keyval(const gchar *value)
{
	TRACE_CALL(__func__);
	gchar *patterns[] =
	{
		KEY_MODIFIER_SHIFT,
		KEY_MODIFIER_CTRL,
		KEY_MODIFIER_ALT,
		KEY_MODIFIER_SUPER,
		KEY_MODIFIER_HYPER,
		KEY_MODIFIER_META,
		NULL
	};
	gint i;
	gchar *tmpvalue;
	gchar *newvalue;
	guint keyval;

	if (g_strcmp0(value, KEY_CHOOSER_NONE) == 0)
		return 0;

	/* Remove any modifier text before to get the keyval */
	newvalue = g_strdup(value);
	for (i = 0; i < g_strv_length(patterns); i++) {
		tmpvalue = remmina_public_str_replace(newvalue, patterns[i], "");
		g_free(newvalue);
		newvalue = g_strdup(tmpvalue);
		g_free(tmpvalue);
	}
	keyval = gdk_keyval_to_lower(gdk_keyval_from_name(newvalue));
	g_free(newvalue);
	return keyval;
}
