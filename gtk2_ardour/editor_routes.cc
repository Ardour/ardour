/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#include "editor_routes.h"

#include "pbd/i18n.h"

EditorRoutes::EditorRoutes ()
{
	init ();
}

void
EditorRoutes::init ()
{
	setup_col (append_toggle (_columns.visible, _columns.noop_true, sigc::mem_fun (*this, &EditorRoutes::on_tv_visible_changed)), S_("Visible|V"), _("Track/Bus visible ?"));
	setup_col (append_toggle (_columns.trigger, _columns.is_track, sigc::mem_fun (*this, &EditorRoutes::on_tv_trigger_changed)),  S_("Cues|C"), _("Visible on Cues window ?"));
	setup_col (append_toggle (_columns.active, _columns.activatable, sigc::mem_fun (*this, &EditorRoutes::on_tv_active_changed)), S_("Active|A"),  _("Track/Bus active ?"));

	append_col_input_active ();
	append_col_rec_enable ();
	append_col_rec_safe ();
	append_col_mute ();
	append_col_solo ();
}
