/*
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2015 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <cmath>

#include <gtkmm/listviewtext.h>
#include <gtkmm/stock.h>

#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"

#include "widgets/tooltips.h"

#include "ardour/region.h"
#include "ardour/session.h"
#include "ardour/source.h"

#include "ardour_ui.h"
#include "clock_group.h"
#include "main_clock.h"
#include "gui_thread.h"
#include "region_editor.h"
#include "public_editor.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace Gtkmm2ext;

RegionEditor::RegionEditor (Session* s, boost::shared_ptr<Region> r)
	: ArdourDialog (_("Region"))
	, _table (9, 2)
	, _table_row (0)
	, _region (r)
	, name_label (_("Name:"))
	, audition_button (_("Audition"))
	, _clock_group (new ClockGroup)
	, position_clock (X_("regionposition"), true, "", true, false)
	, end_clock (X_("regionend"), true, "", true, false)
	, length_clock (X_("regionlength"), true, "", true, false, true)
	, sync_offset_relative_clock (X_("regionsyncoffsetrelative"), true, "", true, false)
	, sync_offset_absolute_clock (X_("regionsyncoffsetabsolute"), true, "", true, false)
	  /* XXX cannot file start yet */
	, start_clock (X_("regionstart"), true, "", false, false)
	, _sources (1)
{
	set_session (s);

	_clock_group->set_clock_mode (ARDOUR_UI::instance()->primary_clock->mode());
	ARDOUR_UI::instance()->primary_clock->mode_changed.connect (sigc::mem_fun (*this, &RegionEditor::set_clock_mode_from_primary));

	_clock_group->add (position_clock);
	_clock_group->add (end_clock);
	_clock_group->add (length_clock);
	_clock_group->add (sync_offset_relative_clock);
	_clock_group->add (sync_offset_absolute_clock);
	_clock_group->add (start_clock);

	position_clock.set_session (_session);
	end_clock.set_session (_session);
	length_clock.set_session (_session);
	sync_offset_relative_clock.set_session (_session);
	sync_offset_absolute_clock.set_session (_session);
	start_clock.set_session (_session);

	ArdourWidgets::set_tooltip (audition_button, _("audition this region"));

	audition_button.unset_flags (Gtk::CAN_FOCUS);

	audition_button.set_events (audition_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));

	name_entry.set_name ("RegionEditorEntry");
	name_label.set_name ("RegionEditorLabel");
	position_label.set_name ("RegionEditorLabel");
	position_label.set_text (_("Position:"));
	end_label.set_name ("RegionEditorLabel");
	end_label.set_text (_("End:"));
	length_label.set_name ("RegionEditorLabel");
	length_label.set_text (_("Length:"));
	sync_relative_label.set_name ("RegionEditorLabel");
	sync_relative_label.set_text (_("Sync point (relative to region):"));
	sync_absolute_label.set_name ("RegionEditorLabel");
	sync_absolute_label.set_text (_("Sync point (absolute):"));
	start_label.set_name ("RegionEditorLabel");
	start_label.set_text (_("File start:"));
	_sources_label.set_name ("RegionEditorLabel");

	if (_region->sources().size() > 1) {
		_sources_label.set_text (_("Sources:"));
	} else {
		_sources_label.set_text (_("Source:"));
	}

	_table.set_col_spacings (12);
	_table.set_row_spacings (6);
	_table.set_border_width (12);

	name_label.set_alignment (1, 0.5);
	position_label.set_alignment (1, 0.5);
	end_label.set_alignment (1, 0.5);
	length_label.set_alignment (1, 0.5);
	sync_relative_label.set_alignment (1, 0.5);
	sync_absolute_label.set_alignment (1, 0.5);
	start_label.set_alignment (1, 0.5);
	_sources_label.set_alignment (1, 0.5);

	Gtk::HBox* nb = Gtk::manage (new Gtk::HBox);
	nb->set_spacing (6);
	nb->pack_start (name_entry);
	nb->pack_start (audition_button, false, false);

	_table.attach (name_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (*nb, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (position_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (position_clock, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (end_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (end_clock, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (length_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (length_clock, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (sync_relative_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (sync_offset_relative_clock, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (sync_absolute_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (sync_offset_absolute_clock, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (start_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (start_clock, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (_sources_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (_sources, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	get_vbox()->pack_start (_table, true, true);

	add_button (Gtk::Stock::CLOSE, Gtk::RESPONSE_ACCEPT);

	set_name ("RegionEditorWindow");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	signal_response().connect (sigc::mem_fun (*this, &RegionEditor::handle_response));

	set_title (string_compose (_("Region '%1'"), _region->name()));

	for (uint32_t i = 0; i < _region->sources().size(); ++i) {
		_sources.append_text (_region->source(i)->name());
	}

	_sources.set_headers_visible (false);
	Gtk::CellRendererText* t = dynamic_cast<Gtk::CellRendererText*> (_sources.get_column_cell_renderer(0));
	assert (t);
	t->property_ellipsize() = Pango::ELLIPSIZE_END;

	show_all();

	name_changed ();

	PropertyChange change;

	change.add (ARDOUR::Properties::start);
	change.add (ARDOUR::Properties::length);
	change.add (ARDOUR::Properties::position);
	change.add (ARDOUR::Properties::sync_position);

	bounds_changed (change);

	_region->PropertyChanged.connect (state_connection, invalidator (*this), boost::bind (&RegionEditor::region_changed, this, _1), gui_context());

	spin_arrow_grab = false;

	connect_editor_events ();
}

RegionEditor::~RegionEditor ()
{
	delete _clock_group;
}

void
RegionEditor::set_clock_mode_from_primary ()
{
	_clock_group->set_clock_mode (ARDOUR_UI::instance()->primary_clock->mode());
}

void
RegionEditor::region_changed (const PBD::PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		name_changed ();
	}

	PropertyChange interesting_stuff;

	interesting_stuff.add (ARDOUR::Properties::position);
	interesting_stuff.add (ARDOUR::Properties::length);
	interesting_stuff.add (ARDOUR::Properties::start);
	interesting_stuff.add (ARDOUR::Properties::sync_position);

	if (what_changed.contains (interesting_stuff)) {
		bounds_changed (what_changed);
	}
}

gint
RegionEditor::bpressed (GdkEventButton* ev, Gtk::SpinButton* /*but*/, void (RegionEditor::*/*pmf*/)())
{
	switch (ev->button) {
	case 1:
	case 2:
	case 3:
		if (ev->type == GDK_BUTTON_PRESS) { /* no double clicks here */
			if (!spin_arrow_grab) {
				// GTK2FIX probably nuke the region editor
				// if ((ev->window == but->gobj()->panel)) {
				// spin_arrow_grab = true;
				// (this->*pmf)();
				// }
			}
		}
		break;
	default:
		break;
	}
	return FALSE;
}

gint
RegionEditor::breleased (GdkEventButton* /*ev*/, Gtk::SpinButton* /*but*/, void (RegionEditor::*pmf)())
{
	if (spin_arrow_grab) {
		(this->*pmf)();
		spin_arrow_grab = false;
	}
	return FALSE;
}

void
RegionEditor::connect_editor_events ()
{
	name_entry.signal_changed().connect (sigc::mem_fun(*this, &RegionEditor::name_entry_changed));

	position_clock.ValueChanged.connect (sigc::mem_fun(*this, &RegionEditor::position_clock_changed));
	end_clock.ValueChanged.connect (sigc::mem_fun(*this, &RegionEditor::end_clock_changed));
	length_clock.ValueChanged.connect (sigc::mem_fun(*this, &RegionEditor::length_clock_changed));
	sync_offset_absolute_clock.ValueChanged.connect (sigc::mem_fun (*this, &RegionEditor::sync_offset_absolute_clock_changed));
	sync_offset_relative_clock.ValueChanged.connect (sigc::mem_fun (*this, &RegionEditor::sync_offset_relative_clock_changed));

	audition_button.signal_toggled().connect (sigc::mem_fun(*this, &RegionEditor::audition_button_toggled));

	_session->AuditionActive.connect (audition_connection, invalidator (*this), boost::bind (&RegionEditor::audition_state_changed, this, _1), gui_context());
}

void
RegionEditor::position_clock_changed ()
{
	bool in_command = false;
	boost::shared_ptr<Playlist> pl = _region->playlist();

	if (pl) {
		PublicEditor::instance().begin_reversible_command (_("change region start position"));
		in_command = true;

		_region->clear_changes ();
		_region->set_position (position_clock.current_time());
		_session->add_command(new StatefulDiffCommand (_region));
	}

	if (in_command) {
		PublicEditor::instance().commit_reversible_command ();
	}
}

void
RegionEditor::end_clock_changed ()
{
	bool in_command = false;
	boost::shared_ptr<Playlist> pl = _region->playlist();

	if (pl) {
		PublicEditor::instance().begin_reversible_command (_("change region end position"));
		in_command = true;

		_region->clear_changes ();
		_region->trim_end (end_clock.current_time());
		_session->add_command(new StatefulDiffCommand (_region));
	}

	if (in_command) {
		PublicEditor::instance().commit_reversible_command ();
	}

	end_clock.set (_region->nt_last(), true);
}

void
RegionEditor::length_clock_changed ()
{
	timecnt_t len = length_clock.current_duration();
	bool in_command = false;
	boost::shared_ptr<Playlist> pl = _region->playlist();

	if (pl) {
		PublicEditor::instance().begin_reversible_command (_("change region length"));
		in_command = true;

		_region->clear_changes ();
		_region->trim_end (_region->nt_position() + len.decrement());
		_session->add_command(new StatefulDiffCommand (_region));
	}

	if (in_command) {
		PublicEditor::instance().commit_reversible_command ();
	}

	length_clock.set_duration (_region->nt_length());
}

void
RegionEditor::audition_button_toggled ()
{
	if (audition_button.get_active()) {
		_session->audition_region (_region);
	} else {
		_session->cancel_audition ();
	}
}

void
RegionEditor::name_changed ()
{
	if (name_entry.get_text() != _region->name()) {
		name_entry.set_text (_region->name());
	}
}

void
RegionEditor::bounds_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::position) && what_changed.contains (ARDOUR::Properties::length)) {
		position_clock.set (_region->nt_position(), true);
		end_clock.set (_region->nt_last(), true);
		length_clock.set_duration (_region->nt_length(), true);
	} else if (what_changed.contains (ARDOUR::Properties::position)) {
		position_clock.set (_region->nt_position(), true);
		end_clock.set (_region->nt_last(), true);
	} else if (what_changed.contains (ARDOUR::Properties::length)) {
		end_clock.set (_region->nt_last(), true);
		length_clock.set_duration (_region->nt_length(), true);
	}

	if (what_changed.contains (ARDOUR::Properties::sync_position) || what_changed.contains (ARDOUR::Properties::position)) {
		int dir;
		timecnt_t off = _region->sync_offset (dir);
		if (dir == -1) {
			off = -off;
		}

		if (what_changed.contains (ARDOUR::Properties::sync_position)) {
			sync_offset_relative_clock.set_duration (off, true);
		}

		sync_offset_absolute_clock.set (_region->nt_position () + off, true);
	}

	if (what_changed.contains (ARDOUR::Properties::start)) {
		start_clock.set (timepos_t (_region->nt_start()), true);
	}
}

void
RegionEditor::activation ()
{

}

void
RegionEditor::name_entry_changed ()
{
	if (name_entry.get_text() != _region->name()) {
		_region->set_name (name_entry.get_text());
	}
}

void
RegionEditor::audition_state_changed (bool yn)
{
	ENSURE_GUI_THREAD (*this, &RegionEditor::audition_state_changed, yn)

	if (!yn) {
		audition_button.set_active (false);
	}
}

void
RegionEditor::sync_offset_absolute_clock_changed ()
{
	PublicEditor::instance().begin_reversible_command (_("change region sync point"));

	_region->clear_changes ();
	_region->set_sync_position (sync_offset_absolute_clock.current_time());
	_session->add_command (new StatefulDiffCommand (_region));

	PublicEditor::instance().commit_reversible_command ();
}

void
RegionEditor::sync_offset_relative_clock_changed ()
{
	PublicEditor::instance().begin_reversible_command (_("change region sync point"));

	_region->clear_changes ();
	_region->set_sync_position (sync_offset_relative_clock.current_time() + _region->nt_position ());
	_session->add_command (new StatefulDiffCommand (_region));

	PublicEditor::instance().commit_reversible_command ();
}

bool
RegionEditor::on_delete_event (GdkEventAny*)
{
	PropertyChange change;

	change.add (ARDOUR::Properties::start);
	change.add (ARDOUR::Properties::length);
	change.add (ARDOUR::Properties::position);
	change.add (ARDOUR::Properties::sync_position);

	bounds_changed (change);

	return true;
}

void
RegionEditor::handle_response (int)
{
	hide ();
}
