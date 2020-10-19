/*
 * Copyright (C) 2015-2016 Colin Fletcher <colin.m.fletcher@googlemail.com>
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

#include "ardour_dialog.h"
#include "public_editor.h"
#include "editing.h"
#include "audio_clock.h"

class InsertRemoveTimeDialog : public ArdourDialog
{
public:
	InsertRemoveTimeDialog (PublicEditor &, bool remove = false);

	Editing::InsertTimeOption intersected_region_action ();
	bool all_playlists () const;
	bool move_glued () const;
	bool move_markers () const;
	bool move_glued_markers () const;
	bool move_locked_markers () const;
	bool move_tempos () const;
	Temporal::timepos_t position () const;
	Temporal::timecnt_t distance () const;

private:
	void move_markers_toggled ();
	void doit ();

	PublicEditor& _editor;
	Gtk::ComboBoxText _intersected_combo;
	Gtk::CheckButton _all_playlists;
	Gtk::CheckButton _move_glued;
	Gtk::CheckButton _move_markers;
	Gtk::CheckButton _move_glued_markers;
	Gtk::CheckButton _move_locked_markers;
	Gtk::CheckButton _move_tempos;
	Gtk::Label tempo_label;
	AudioClock duration_clock;
	AudioClock position_clock;
};
