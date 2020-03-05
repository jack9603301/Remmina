/*
 * Remmina - The GTK+ Remote Desktop Client
 * Copyright (C) 2009 - Vic Lee
 * Copyright (C) 2014-2015 Antenore Gatta, Fabio Castelli, Giovanni Panozzo
 * Copyright (C) 2016-2020 Antenore Gatta, Giovanni Panozzo
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

#pragma once

#include "remmina_file.h"
#include "remmina_message_panel.h"
//Comment in production
//#define NDEBUG
#include <assert.h>
G_BEGIN_DECLS

#define   REMMINA_TYPE_CONNECTION_WINDOW            (rcw_get_type())
#define   RCW(obj)                                  (G_TYPE_CHECK_INSTANCE_CAST((obj), REMMINA_TYPE_CONNECTION_WINDOW, RemminaConnectionWindow))
#define   RCW_CLASS(klass)                          (G_TYPE_CHECK_CLASS_CAST((klass), REMMINA_TYPE_CONNECTION_WINDOW, RemminaConnectionWindowClass))
#define   REMMINA_IS_CONNECTION_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), REMMINA_TYPE_CONNECTION_WINDOW))
#define   REMMINA_IS_CONNECTION_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), REMMINA_TYPE_CONNECTION_WINDOW))
#define   RCW_GET_CLASS(obj)                        (G_TYPE_INSTANCE_GET_CLASS((obj), REMMINA_TYPE_CONNECTION_WINDOW, RemminaConnectionWindowClass))
#define   MONITOR_NAME_MAX                                20

typedef struct _RemminaConnectionWindowPriv RemminaConnectionWindowPriv;

typedef struct _RemminaConnectionWindow {
	GtkWindow			window;
	RemminaConnectionWindowPriv *	priv;
} RemminaConnectionWindow;

typedef struct _RemminaConnectionWindowClass {
	GtkWindowClass parent_class;
	void (*toolbar_place)(RemminaConnectionWindow *gp);
} RemminaConnectionWindowClass;

typedef struct _RemminaConnectionObject RemminaConnectionObject;

typedef enum {
	RCW_ONDELETE_CONFIRM_IF_2_OR_MORE	= 0,
	RCW_ONDELETE_NOCONFIRM			= 1
} RemminaConnectionWindowOnDeleteConfirmMode;

GType rcw_get_type(void)
G_GNUC_CONST;

/* Open a new connection window for a .remmina file */
gboolean rcw_open_from_filename(const gchar *filename);
/* Open a new connection window for a given RemminaFile struct. The struct will be freed after the call */
void rcw_open_from_file(RemminaFile *remminafile);
gboolean rcw_delete(RemminaConnectionWindow *cnnwin);
void rcw_set_delete_confirm_mode(RemminaConnectionWindow *cnnwin, RemminaConnectionWindowOnDeleteConfirmMode mode);
GtkWidget *rcw_open_from_file_full(RemminaFile *remminafile, GCallback disconnect_cb, gpointer data, guint *handler);

void rco_destroy_message_panel(RemminaConnectionObject *cnnobj, RemminaMessagePanel *mp);
void rco_show_message_panel(RemminaConnectionObject *cnnobj, RemminaMessagePanel *mp);
void rco_get_monitor_geometry(RemminaConnectionObject *cnnobj, GdkRectangle *sz);

/**
 * @brief Each monitor has it own values saved here
 * @param width The size the proto have to bee scaled to fit into the local window
 * @param height The size the proto have to bee scaled to fit into the local window
 * @param monitorHorizontalSaved saves the horizontal position the user typed in needed to calculate horizontalPositioning
 * @param monitorVerticalSaved saves the vertical position the user typed in needed to calculate verticalPositioning
 * @param horizontalPositioning how much do you have to move the monitor
 * @param verticalPositioning how much do you have to move the monitor
 * @param monitorNumber which number the current Monitor has Deprecated ?
 * @param endX On which coordinate the monitor end(usefull for the next monitor in line) Deprecated ?
 * @param remoteMonitorPartWidth Size of the remoteMonitor
 * @param remoteMonitorPartHeight Size of the remoteMonitor
 * @param active should the monitor be listed in the dropdown menu
 * @param name of the monitor
 * @param builder for the individual change window
 * @param dialog for the individual change window
 * @param widthButton GtkSpinButton where you can change the width in the gui
 * @param heightButton GtkSpinButton where you can change the height in the gui
 * @param monitorNameEntry Text entry box to change monitor name
 * @param monitorActivateButton set the active attribute
 * @param okButton saves the settings in temporary vector
 * @param cancelButton cancels chenged settings
 * @param monitorHorizontalButton sets the horizontal start position of the mon
 * @param monitorVerticalButton sets the vertical start position of mon
 * @param monitorButton saves the widget from the createMonitor dialog and saves it to change the name after change in monitorSelection
 */
struct Monitors {
    float width;
    float height, monitorHorizontalSaved, monitorVerticalSaved;
    float horizontalPositioning, verticalPositioning;
    int   remoteMonitorPartWidth, remoteMonitorPartHeight, active;
    char *name;
	GtkBuilder *builder;
	GtkDialog  *dialog;
	GtkWidget *widthButton, *heightButton, *monitorNameEntry, *monitorActivateButton, *okButton, *cancelButton,
	*monitorHorizontalButton, *monitorVerticalButton, *monitorButton;
};

//Self written std::vector
/**
 * @brief This saves the monitors dynamically
 * @arg Monitors saves the monitors
 * @arg the number of monitors
 */
struct Array{
    struct  Monitors *monitor;
    size_t length;
};

/**
 * @brief creates a vector
 * @param size how many elements should be created
 * @return returns the vector
 */
struct Array* initialize_array(size_t size);
/**
 * @brief Adds a monitor to the Vector
 * @param vector where to add the monitor
 * @param monitors one mon that will be added
 */
void add_monitors(struct Array *vector, struct Monitors *monitors);
/**
 * @brief Vector will be copied in other
 * @param vector will be copied
 * @param copy will be overwritten
 */
void copy_array(struct Array *vector, struct Array *copy);
/**
 * @brief changes a single monitor
 * @param ar the vector in which the mon should be changed
 * @param index the monitor which should be changed
 * @param name the new name
 * @param width the new width
 * @param height the new height
 * @param active the new active status
 * @param horizontalPosition the new position
 * @param verticalPosition
 */
void change_monitor(struct Array *ar, size_t index, char *name, int width, int height, int active, int horizontalPosition, int verticalPosition);
/**
 * @brief deletes a specific element
 * @param vector in which the element should be deleted
 * @param number index of deleted element
 */
void delete_elements(struct Array *vector, size_t number);
/**
 * @brief temporary monitor list with dimensions
 */
struct tempMonitorList{
    int xDimension, yDimension;
    struct Array *ar;
};
/**
 * @brief The settings for all Monitors
 * @param remoteWidth The width of all Monitors together
 * @param remoteHeight The Height of all Monitors together
 * @param localWidth The width of the current window
 * @param localHeight The height of the current window
 * @param monitors How many monitors are registered
 * @param actualMonitor The monitor that is used at the moment
 * @param widthFactor The factor on which the remote size must be scaled for the window
 * @param heightFactor The factor on which the remote size must be scaled for the window
 * @param ar The vector of the existing monitors
 * @param useMonitorWidget The widget will be saved here to easily activate and deactivate it
 * @param flowBox this is the container where the monitors are in as a monitorWall in gui
 * @param yDimensionButton here the yDimension will be saved to size the wall
 * @param xDimensionButton here the xDimension will be saved to size the wall
 * @param remmina_file here settings for the connection will be saved as a hash table
 * @param dialog for the managing window
 * @param builder for the managing window
 * @param aspect This is for saving if the last monitor was a aspect or a fixed
 * @param temp where the temporary monitors will be saved before saving them with the ok Button of the managing window
 */
struct StaticMonitor {
    float remoteWidth;
    float remoteHeight;
    float localWidth;
    float localHeight;
    int monitors;
    int actualMonitor;
    float widthFactor;
    float heightFactor;
    struct Array *ar;
    GtkWidget *useMonitorWidget, *flowBox, *yDimensionButton,
            *xDimensionButton;
    RemminaFile *remmina_file;
    GtkDialog *dialog;
    GtkBuilder *builder;
    int aspect; // 1=True 0=False This says if the last displayed monitor was a aspect or a fixed
    struct tempMonitorList temp;
    RemminaConnectionObject *cnnobj;

};


struct StaticMonitor *monitorStruct;
void rco_change_scalemode(RemminaConnectionObject *cnnobj, gboolean bdyn, gboolean bscale);

/**
 * @brief This function create the settings for a specific monitor which can than be selected by the user
 * @param name of the monitor
 * @param remoteMonitorPartWidth the width of one of the remote monitors
 * @param remoteMonitorPartHeight the height of one of the remote monitors
 * @param active if the monitor should be shown in dropdown
 * @param horizontalPositioning where to start drawing of monitor
 * @param verticalPositioning where to start drawing of monitor
 */
void create_multi_monitor(char *name, float remoteMonitorPartWidth, float remoteMonitorPartHeight, int active,
                          float horizontalPositioning, float verticalPositioning);

/**
 * @brief This struct is necessary to give two arguments in one callback to change the monitor with buttons! It will called when you select one monitor
 */
struct Daten{
    RemminaConnectionObject *cnnobj;
    int monitor;
};
/**
 * @brief Fill/update the variables of the monitorStruct
 * @param remoteWidth remote width of the whole desktop
 * @param remoteHeight remote height of the whole desktop
 * @param localWidth width of the local monitor
 * @param localHeight height of the local monitor
*/
void fill_static_multi_monitor(float remoteWidth, float remoteHeight, float localWidth, float localHeight);

/**
 * @brief Create the fixed container and save it in cnnobj->aspectframe
 * @param cnnobj The RemminaConnectionObject struct which saves cnnwin and some graphical Container
 * @param width The width of the local window
 * @param height The height of the local window
 */
void initialize_fixed(RemminaConnectionObject *cnnobj, gint width, gint height);

/**
 * @brief Create the aspectframe container in aspectframe
 * @param cnnobj The RemminaConnectionObject struct which saves cnnwin and some graphical Container
 * @param aratio The Resolution of the remote Monitor
 */
void initialize_aspectframe(RemminaConnectionObject *cnnobj, gfloat aratio);

/**
 * @brief Updates the fixed container this will be need, if the local monitor changes or the user want another monitor
 * @param cnnobj The RemminaConnectionObject struct which saves cnnwin and some graphical Container
 */
void update_fixed(const RemminaConnectionObject *cnnobj);

/**
 * @brief This is a callback function which sets which monitor the user want to see
 * @param widget This parameter is required for the callback function, but will not be used
 * @param daten This function gets in the struct which monitor the user want and the RemminaConnectionObject
 */
void use_monitor(GtkWidget *widget, struct Daten *daten);

/**
 * @brief Initialize the monitorStruct with default values
 */
void define_structs();
/**
 * @brief the ok button of the managing window has been clicked
 */
void ok_button_clicked();
/**
 * @brief the cancel button of the managing window has been clicked
 */
void cancel_button_clicked();

void
write_file(char *name, int width, int height, int active, float horizontalPositioning, float verticalPositioning,
           int index, RemminaFile *remmina_file);

void tree_selection_changed_cb(GtkTreeSelection *selection, gpointer data);
/**
 * @brief a button for a monitor has been clicked in the managing window
 * @param widget gives the widget itself, but isn't used
 * @param monitorNumber the number of the selected monitor
 */
void monitor_selection_clicked(GtkWidget *widget, int *monitorNumber);

void rcw_screen_option_popdown(GtkWidget *widget, RemminaConnectionWindow *cnnwin);
/**
 * @brief removes the aspect frame container
 * @param cnnobj the RemminaConnectionObject
 */
void remove_aspectframe(RemminaConnectionObject *cnnobj);
void remmina_protocol_widget_update_alignment(RemminaConnectionObject *cnnobj);
#define MESSAGE_PANEL_SPINNER 0x00000001
#define MESSAGE_PANEL_OKBUTTON 0x00000002

G_END_DECLS
