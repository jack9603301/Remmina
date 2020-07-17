/*
 * Remmina - The GTK+ Remote Desktop Client
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

#include "rdp_plugin.h"
#include "rdp_monitor.h"

/* https://github.com/adlocode/xfwm4/blob/1d21be9ffc0fa1cea91905a07d1446c5227745f4/common/xfwm-common.c */

void remmina_rdp_monitor_get (rfContext *rfi, gchar **monitorids, guint32 *maxwidth, guint32 *maxheight)
{
	TRACE_CALL(__func__);

	gint n_monitors;
	gint scale;
	gint count;
	gboolean primary_found = FALSE;
	/* TODO (max monitors * 2) */
	static gchar buffer[256];
	GdkMonitor *monitor;
	GdkDisplay *display = gdk_display_get_default ();

	GdkRectangle geometry = { 0, 0, 0, 0 };
	GdkRectangle tempgeom = { 0, 0, 0, 0 };
	GdkRectangle destgeom = { 0, 0, 0, 0 };

	n_monitors = gdk_display_get_n_monitors(display);

	for (gint i = 0; i < n_monitors; ++i) {
		monitor = gdk_display_get_monitor(display, i);
		/* If the desktop env in use doesn't have the working area concept
		 * gdk_monitor_get_workarea will return the monitor geometry*/
		REMMINA_PLUGIN_DEBUG("Monitor n %d", i);
		gdk_monitor_get_workarea (monitor, &geometry);
		/* geometry contain the application geometry, to obtain the real one
		 * we must multiply by the scale factor */
		scale = gdk_monitor_get_scale_factor (monitor);
		geometry.width *= scale;
		geometry.height *= scale;
		REMMINA_PLUGIN_DEBUG("Monitor n %d scale: %d", i, scale);
		REMMINA_PLUGIN_DEBUG("Monitor n %d width: %d", i, geometry.width);
		REMMINA_PLUGIN_DEBUG("Monitor n %d x: %d", i, geometry.x);
		REMMINA_PLUGIN_DEBUG("Monitor n %d y: %d", i, geometry.y);
		rfi->settings->MonitorDefArray[i].x = geometry.x;
		rfi->settings->MonitorDefArray[i].y = geometry.x;
		rfi->settings->MonitorDefArray[i].width = geometry.width;
		rfi->settings->MonitorDefArray[i].height = geometry.height;
		rfi->settings->MonitorDefArray[i].orig_screen = i;
		if (gdk_monitor_is_primary(monitor)) {
			rfi->settings->MonitorDefArray[i].is_primary = TRUE;
			rfi->settings->MonitorLocalShiftX = rfi->settings->MonitorDefArray[i].x;
			rfi->settings->MonitorLocalShiftY = rfi->settings->MonitorDefArray[i].y;
			REMMINA_PLUGIN_DEBUG ("Primary monitor found with id: %d", i);
			primary_found = TRUE;
		}
		if (!primary_found) {
			rfi->settings->MonitorDefArray[0].is_primary = TRUE;
			rfi->settings->MonitorLocalShiftX = rfi->settings->MonitorDefArray[0].x;
			rfi->settings->MonitorLocalShiftY = rfi->settings->MonitorDefArray[0].y;
		}
		if (buffer[0] == '\0')
			g_sprintf (buffer, "%d", i);
		else
			g_sprintf(buffer, "%s,%d", buffer, i);
		REMMINA_PLUGIN_DEBUG("Monitor IDs buffer: %s", buffer);
#if 0
		else
			*maxwidth = MIN(*maxwidth, geometry.width);
		if (i == 0)
			*maxheight = geometry.height;
		else
			*maxheight = MIN(*maxheight, geometry.height);
		REMMINA_PLUGIN_DEBUG("maxw and maxh: %ux%u", *maxwidth, *maxheight);
#endif
		gdk_rectangle_union(&tempgeom, &geometry, &destgeom);
		//tempgeom = destgeom;
		memcpy(&tempgeom, &destgeom, sizeof tempgeom);
		count++;
	}
	rfi->settings->MonitorCount = count;
	}
	rfi->settings->MonitorCount = count;
	*maxwidth = destgeom.width;
	*maxheight = destgeom.height;
	REMMINA_PLUGIN_DEBUG("maxw and maxh: %ux%u", *maxwidth, *maxheight);
	if (n_monitors > 1)
		rfi->settings->SupportMonitorLayoutPdu = TRUE;
	*monitorids = g_strdup(buffer);
}

