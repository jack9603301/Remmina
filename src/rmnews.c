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
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>
#include "remmina_pref.h"
#include "remmina_scheduler.h"
#include "rmnews.h"
#include "remmina/remmina_trace_calls.h"

/* Timers */
#define RMNEWS_CHECK_1ST_MS 3000
#define RMNEWS_CHECK_INTERVAL_MS 12000
#define RMNEWS_INTERVAL_SEC    26784

void rmnews_get_news(gboolean show_only)
{
	TRACE_CALL(__func__);

	g_print ("Gathering news\n");

}

static gboolean rmnews_periodic_check(gpointer user_data)
{
	TRACE_CALL(__func__);
	GTimeVal t;
	glong next;

	next = remmina_pref.periodic_usage_stats_last_sent + RMNEWS_INTERVAL_SEC;
	g_get_current_time(&t);
	if (t.tv_sec > next || (t.tv_sec < remmina_pref.periodic_usage_stats_last_sent && t.tv_sec > 1514764800)) {
		g_print("Testing news\n");
		rmnews_get_news(FALSE);
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
