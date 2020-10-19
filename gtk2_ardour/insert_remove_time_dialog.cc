/*
 * Copyright (C) 2015-2016 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2016-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016 Ben Loftis <ben@harrisonconsoles.com>
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

#include <gtkmm/table.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/stock.h>
#include <gtkmm/alignment.h>
#include "insert_remove_time_dialog.h"
#include "audio_clock.h"
#include "ardour_ui.h"
#include "main_clock.h"
#include "pbd/i18n.h"

using namespace Gtk;
using namespace Editing;
using namespace ARDOUR;

InsertRemoveTimeDialog::InsertRemoveTimeDialog (PublicEditor& e, bool remove)
	: ArdourDialog (remove ? _("Remove Time") : _("Insert Time"))
	, _editor (e)
	, duration_clock ("insertTimeClock", true, "",
			true,   // editable
			false,  // follows_playhead
			true,   // duration
			false,  // with_info
			true    // accept_on_focus_out
		)
	, position_clock ("insertPosTimeClock", true, "",
			true,   // editable
			false,  // follows_playhead
			false,   // duration
			false,  // with_info
			true    // accept_on_focus_out
		)
{
	set_session (_editor.session ());

	get_vbox()->set_border_width (12);
	get_vbox()->set_spacing (4);

	Table* table = manage (new Table (2, 3));
	table->set_spacings (4);

	Label* time_label = manage (new Label (remove ? _("Remove Time starting at:") : _("Insert Time starting at:")));
	time_label->set_alignment (1, 0.5);
	table->attach (*time_label, 0, 1, 0, 1, FILL | EXPAND);
	position_clock.set_session (_session);
	position_clock.set_mode (ARDOUR_UI::instance()->primary_clock->mode());
	table->attach (position_clock, 1, 2, 0, 1);

	time_label = manage (new Label (remove ? _("Time to remove:") : _("Time to insert:")));
	time_label->set_alignment (1, 0.5);
	table->attach (*time_label, 0, 1, 1, 2, FILL | EXPAND);
	duration_clock.set_session (_session);
	duration_clock.set_mode (ARDOUR_UI::instance()->primary_clock->mode());
	table->attach (duration_clock, 1, 2, 1, 2);

	//if a Range is selected, assume the user wants to insert/remove the length of the range
	if ( _editor.get_selection().time.length() != 0 ) {
		position_clock.set (_editor.get_selection().time.start_time(), true);
		duration_clock.set (_editor.get_selection().time.end_time(), true,  timecnt_t (_editor.get_selection().time.start_time()));
		duration_clock.set_bbt_reference (_editor.get_selection().time.start_time());
	} else {
		timepos_t const pos = _editor.get_preferred_edit_position (EDIT_IGNORE_MOUSE);
		position_clock.set (pos, true);
		duration_clock.set_bbt_reference (pos);
		duration_clock.set (timepos_t());
	}

	if (!remove) {
		Label* intersected_label = manage (new Label (_("Intersected regions should:")));
		intersected_label->set_alignment (1, 0.5);
		table->attach (*intersected_label, 0, 1, 2, 3, FILL | EXPAND);
		_intersected_combo.append_text (_("stay in position"));
		_intersected_combo.append_text (_("move"));
		_intersected_combo.append_text (_("be split"));
		_intersected_combo.set_active (0);
		table->attach (_intersected_combo, 1, 2, 2, 3);
	}

	get_vbox()->pack_start (*table);

	_all_playlists.set_label (_("Apply to all playlists of the selected track(s)"));
	get_vbox()->pack_start (_all_playlists);

	_move_glued.set_label (_("Move glued-to-musical-time regions (MIDI regions)"));
	_move_glued.set_active();
	get_vbox()->pack_start (_move_glued);
	_move_markers.set_label (_("Move markers"));
	get_vbox()->pack_start (_move_markers);
	_move_markers.signal_toggled().connect (sigc::mem_fun (*this, &InsertRemoveTimeDialog::move_markers_toggled));
	_move_glued_markers.set_label (_("Move glued-to-musical-time markers"));
	_move_glued_markers.set_active();
	Alignment* indent = manage (new Alignment);
	indent->set_padding (0, 0, 12, 0);
	indent->add (_move_glued_markers);
	get_vbox()->pack_start (*indent);
	_move_locked_markers.set_label (_("Move locked markers"));
	indent = manage (new Alignment);
	indent->set_padding (0, 0, 12, 0);
	indent->add (_move_locked_markers);
	get_vbox()->pack_start (*indent);
	tempo_label.set_markup (_("Move tempo and meter changes\n<i>(may cause oddities in the tempo map)</i>"));
	HBox* tempo_box = manage (new HBox);
	tempo_box->set_spacing (6);
	tempo_box->pack_start (_move_tempos, false, false);
	tempo_box->pack_start (tempo_label, false, false);
	get_vbox()->pack_start (*tempo_box);

	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	Gtk::Button *btn = manage (new Gtk::Button (remove ? _("Remove time") : _("Insert time")));
	btn->signal_clicked().connect (sigc::mem_fun(*this, &InsertRemoveTimeDialog::doit));
	get_action_area()->pack_start (*btn);
	show_all ();

	move_markers_toggled ();
}

InsertTimeOption
InsertRemoveTimeDialog::intersected_region_action ()
{
	/* only setting this to keep GCC quiet */
	InsertTimeOption opt = LeaveIntersected;

	switch (_intersected_combo.get_active_row_number ()) {
	case 0:
		opt = LeaveIntersected;
		break;
	case 1:
		opt = MoveIntersected;
		break;
	case 2:
		opt = SplitIntersected;
		break;
	}

	return opt;
}

bool
InsertRemoveTimeDialog::all_playlists () const
{
	return _all_playlists.get_active ();
}

bool
InsertRemoveTimeDialog::move_glued () const
{
	return _move_glued.get_active ();
}

bool
InsertRemoveTimeDialog::move_tempos () const
{
	return _move_tempos.get_active ();
}

bool
InsertRemoveTimeDialog::move_markers () const
{
	return _move_markers.get_active ();
}

bool
InsertRemoveTimeDialog::move_glued_markers () const
{
	return _move_glued_markers.get_active ();
}

bool
InsertRemoveTimeDialog::move_locked_markers () const
{
	return _move_locked_markers.get_active ();
}

timepos_t
InsertRemoveTimeDialog::position () const
{
	return position_clock.current_time();
}

timecnt_t
InsertRemoveTimeDialog::distance () const
{
	return duration_clock.current_duration (position_clock.current_time());
}

void
InsertRemoveTimeDialog::doit ()
{
	if (distance () == 0) {
		Gtk::MessageDialog msg (*this, _("Invalid or zero duration entered. Please enter a valid duration"));
		msg.run ();
		return;
	}
	response (RESPONSE_OK);
}

void
InsertRemoveTimeDialog::move_markers_toggled ()
{
	_move_glued_markers.set_sensitive (_move_markers.get_active ());
	_move_locked_markers.set_sensitive (_move_markers.get_active ());
}
