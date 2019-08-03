/*
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2016 Paul Davis <paul@linuxaudiosystems.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <glib.h>
#include "ardour/operations.h"
#include "pbd/i18n.h"

GQuark Operations::capture;
GQuark Operations::paste;
GQuark Operations::duplicate_region;
GQuark Operations::insert_file;
GQuark Operations::insert_region;
GQuark Operations::drag_region_brush;
GQuark Operations::region_drag;
GQuark Operations::selection_grab;
GQuark Operations::region_fill;
GQuark Operations::fill_selection;
GQuark Operations::create_region;
GQuark Operations::region_copy;
GQuark Operations::fixed_time_region_copy;

void
Operations::make_operations_quarks ()
{
	Operations::capture           = g_quark_from_static_string (_("capture"));
	Operations::paste             = g_quark_from_static_string (_("paste"));
	Operations::duplicate_region  = g_quark_from_static_string (_("duplicate region"));
	Operations::insert_file       = g_quark_from_static_string (_("insert file"));
	Operations::insert_region     = g_quark_from_static_string (_("insert region"));
	Operations::drag_region_brush = g_quark_from_static_string (_("drag region brush"));
	Operations::region_drag       = g_quark_from_static_string (_("region drag"));
	Operations::selection_grab    = g_quark_from_static_string (_("selection grab"));
	Operations::region_fill       = g_quark_from_static_string (_("region fill"));
	Operations::fill_selection    = g_quark_from_static_string (_("fill selection"));
	Operations::create_region     = g_quark_from_static_string (_("create region"));
	Operations::region_copy       = g_quark_from_static_string (_("region copy"));
	Operations::fixed_time_region_copy = g_quark_from_static_string (_("fixed time region copy"));
}
