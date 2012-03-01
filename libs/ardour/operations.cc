/*
    Copyright (C) 2011 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <glib.h>
#include "ardour/operations.h"
#include "i18n.h"

GQuark Operations::capture           = g_quark_from_static_string (_("capture"));
GQuark Operations::paste             = g_quark_from_static_string (_("paste"));
GQuark Operations::duplicate_region  = g_quark_from_static_string (_("duplicate region"));
GQuark Operations::insert_file       = g_quark_from_static_string (_("insert file"));
GQuark Operations::insert_region     = g_quark_from_static_string (_("insert region"));
GQuark Operations::drag_region_brush = g_quark_from_static_string (_("drag region brush"));
GQuark Operations::region_drag       = g_quark_from_static_string (_("region drag"));
GQuark Operations::selection_grab    = g_quark_from_static_string (_("selection grab"));
GQuark Operations::region_fill       = g_quark_from_static_string (_("region fill"));
GQuark Operations::fill_selection    = g_quark_from_static_string (_("fill selection"));
GQuark Operations::create_region     = g_quark_from_static_string (_("create region"));
GQuark Operations::region_copy       = g_quark_from_static_string (_("region copy"));
GQuark Operations::fixed_time_region_copy = g_quark_from_static_string (_("fixed time region copy"));
