/*
    Copyright (C) 2001 Paul Davis 

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

    $Id$
*/

#include <pbd/memento_command.h>

#include <ardour/audioregion.h>
#include <ardour/playlist.h>
#include <ardour/utils.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/stop_signal.h>
#include <cmath>

#include "audio_region_editor.h"
#include "audio_region_view.h"
#include "ardour_ui.h"
#include "utils.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace sigc;
using namespace std;

AudioRegionEditor::AudioRegionEditor (Session& s, boost::shared_ptr<AudioRegion> r, AudioRegionView& rv)
	: RegionEditor (s),
	  _region (r),
	  _region_view (rv),
	  name_label (_("NAME:")),
	  audition_button (_("play")),
	  time_table (3, 2),
	  start_clock ("AudioRegionEditorClock", true),
	  end_clock ("AudioRegionEditorClock", true),
	  length_clock ("AudioRegionEditorClock", true, true),
	  sync_offset_clock ("AudioRegionEditorClock", true, true)

{
	start_clock.set_session (&_session);
	end_clock.set_session (&_session);
	length_clock.set_session (&_session);

	name_entry.set_name ("AudioRegionEditorEntry");
	name_label.set_name ("AudioRegionEditorLabel");

	name_hbox.set_spacing (5);
	name_hbox.pack_start (name_label, false, false);
	name_hbox.pack_start (name_entry, false, false);

	ARDOUR_UI::instance()->tooltips().set_tip (audition_button, _("audition this region"));

	audition_button.unset_flags (Gtk::CAN_FOCUS);
	
	audition_button.set_events (audition_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));

	top_row_button_hbox.set_border_width (5);
	top_row_button_hbox.set_spacing (5);
	top_row_button_hbox.set_homogeneous (false);
	top_row_button_hbox.pack_end (audition_button, false, false);
	
	top_row_hbox.pack_start (name_hbox, true, true);
	top_row_hbox.pack_end (top_row_button_hbox, true, true);

	start_label.set_name ("AudioRegionEditorLabel");
	start_label.set_text (_("START:"));
	end_label.set_name ("AudioRegionEditorLabel");
	end_label.set_text (_("END:"));
	length_label.set_name ("AudioRegionEditorLabel");
	length_label.set_text (_("LENGTH:"));
	
	time_table.set_col_spacings (2);
	time_table.set_row_spacings (5);
	time_table.set_border_width (5);

	start_alignment.set (1.0, 0.5);
	end_alignment.set (1.0, 0.5);
	length_alignment.set (1.0, 0.5);

	start_alignment.add (start_label);
	end_alignment.add (end_label);
	length_alignment.add (length_label);

	time_table.attach (start_alignment, 0, 1, 0, 1, Gtk::FILL, Gtk::FILL);
	time_table.attach (start_clock, 1, 2, 0, 1, Gtk::FILL, Gtk::FILL);

	time_table.attach (end_alignment, 0, 1, 1, 2, Gtk::FILL, Gtk::FILL);
	time_table.attach (end_clock, 1, 2, 1, 2, Gtk::FILL, Gtk::FILL);

	time_table.attach (length_alignment, 0, 1, 2, 3, Gtk::FILL, Gtk::FILL);
	time_table.attach (length_clock, 1, 2, 2, 3, Gtk::FILL, Gtk::FILL);

	lower_hbox.pack_start (time_table, true, true);
	lower_hbox.pack_start (sep1, false, false);
	lower_hbox.pack_start (sep2, false, false);

	get_vbox()->pack_start (top_row_hbox, true, true);
	get_vbox()->pack_start (sep3, false, false);
	get_vbox()->pack_start (lower_hbox, true, true);

	set_name ("AudioRegionEditorWindow");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), static_cast<Window *> (this)));

	string title = _("ardour: region ");
	title += _region->name();
	set_title (title);

	show_all();

	name_changed ();
	bounds_changed (Change (StartChanged|LengthChanged|PositionChanged));

	_region->StateChanged.connect (mem_fun(*this, &AudioRegionEditor::region_changed));
	
	spin_arrow_grab = false;
	
	connect_editor_events ();
}

AudioRegionEditor::~AudioRegionEditor ()
{
}

void
AudioRegionEditor::region_changed (Change what_changed)
{
	if (what_changed & NameChanged) {
		name_changed ();
	}
	if (what_changed & BoundsChanged) {
		bounds_changed (what_changed);
	}
}

gint 
AudioRegionEditor::bpressed (GdkEventButton* ev, Gtk::SpinButton* but, void (AudioRegionEditor::*pmf)())
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
AudioRegionEditor::breleased (GdkEventButton* ev, Gtk::SpinButton* but, void (AudioRegionEditor::*pmf)())
{
	if (spin_arrow_grab) {
		(this->*pmf)();
		spin_arrow_grab = false;
	}
	return FALSE;
}

void
AudioRegionEditor::connect_editor_events ()
{
	name_entry.signal_changed().connect (mem_fun(*this, &AudioRegionEditor::name_entry_changed));

	start_clock.ValueChanged.connect (mem_fun(*this, &AudioRegionEditor::start_clock_changed));
	end_clock.ValueChanged.connect (mem_fun(*this, &AudioRegionEditor::end_clock_changed));
	length_clock.ValueChanged.connect (mem_fun(*this, &AudioRegionEditor::length_clock_changed));

	audition_button.signal_toggled().connect (mem_fun(*this, &AudioRegionEditor::audition_button_toggled));
	_session.AuditionActive.connect (mem_fun(*this, &AudioRegionEditor::audition_state_changed));
}

void
AudioRegionEditor::start_clock_changed ()
{
	_session.begin_reversible_command (_("change region start position"));

	boost::shared_ptr<Playlist> pl = _region->playlist();

	if (pl) {
		XMLNode &before = pl->get_state();
		_region->set_position (start_clock.current_time(), this);
		XMLNode &after = pl->get_state();
		_session.add_command(new MementoCommand<Playlist>(*pl, &before, &after));
	}

	_session.commit_reversible_command ();
}

void
AudioRegionEditor::end_clock_changed ()
{
	_session.begin_reversible_command (_("change region end position"));

	boost::shared_ptr<Playlist> pl = _region->playlist();
	
	if (pl) {
		XMLNode &before = pl->get_state();
		_region->trim_end (end_clock.current_time(), this);
		XMLNode &after = pl->get_state();
		_session.add_command(new MementoCommand<Playlist>(*pl, &before, &after));
	}

	_session.commit_reversible_command ();

	end_clock.set (_region->position() + _region->length(), true);
}

void
AudioRegionEditor::length_clock_changed ()
{
	nframes_t frames = length_clock.current_time();
	
	_session.begin_reversible_command (_("change region length"));
	
	boost::shared_ptr<Playlist> pl = _region->playlist();

	if (pl) {
		XMLNode &before = pl->get_state();
		_region->trim_end (_region->position() + frames, this);
		XMLNode &after = pl->get_state();
		_session.add_command(new MementoCommand<Playlist>(*pl, &before, &after));
	}

	_session.commit_reversible_command ();

	length_clock.set (_region->length());
}

void
AudioRegionEditor::audition_button_toggled ()
{
	if (audition_button.get_active()) {
		_session.audition_region (_region);
	} else {
		_session.cancel_audition ();
	}
}

void
AudioRegionEditor::name_changed ()
{
	if (name_entry.get_text() != _region->name()) {
		name_entry.set_text (_region->name());
	}
}

void
AudioRegionEditor::bounds_changed (Change what_changed)
{
	if (what_changed & Change ((PositionChanged|LengthChanged))) {
		start_clock.set (_region->position(), true);
		end_clock.set (_region->position() + _region->length(), true);
		length_clock.set (_region->length(), true);
	}
}

void
AudioRegionEditor::activation ()
{
	
}	

void
AudioRegionEditor::name_entry_changed ()
{
	if (name_entry.get_text() != _region->name()) {
		_region->set_name (name_entry.get_text());
	}
}

void
AudioRegionEditor::audition_state_changed (bool yn)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &AudioRegionEditor::audition_state_changed), yn));

	if (!yn) {
		audition_button.set_active (false);
	}
}

