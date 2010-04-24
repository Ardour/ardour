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

*/

#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"

#include "ardour/session.h"
#include "ardour/audioregion.h"
#include "ardour/playlist.h"
#include "ardour/utils.h"
#include "ardour/dB.h"
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
using namespace std;
using namespace Gtkmm2ext;

AudioRegionEditor::AudioRegionEditor (Session* s, boost::shared_ptr<AudioRegion> r, AudioRegionView& rv)
	: RegionEditor (s),
	  _region (r),
	  _region_view (rv),
	  name_label (_("Name:")),
	  audition_button (_("Play")),
	  _table (8, 2),
	  position_clock (X_("regionposition"), true, X_("AudioRegionEditorClock"), true, false),
	  end_clock (X_("regionend"), true, X_("AudioRegionEditorClock"), true, false),
	  length_clock (X_("regionlength"), true, X_("AudioRegionEditorClock"), true, false, true),
	  sync_offset_relative_clock (X_("regionsyncoffsetrelative"), true, X_("AudioRegionEditorClock"), true, false),
	  sync_offset_absolute_clock (X_("regionsyncoffsetabsolute"), true, X_("AudioRegionEditorClock"), true, false),
	  /* XXX cannot file start yet */
	  start_clock (X_("regionstart"), true, X_("AudioRegionEditorClock"), false, false),
	  gain_adjustment(accurate_coefficient_to_dB(_region->scale_amplitude()), -40.0, +40.0, 0.1, 1.0, 0)	  

{
	position_clock.set_session (_session);
	end_clock.set_session (_session);
	length_clock.set_session (_session);
	sync_offset_relative_clock.set_session (_session);
	sync_offset_absolute_clock.set_session (_session);
	start_clock.set_session (_session);

	ARDOUR_UI::instance()->set_tip (audition_button, _("audition this region"));

	audition_button.unset_flags (Gtk::CAN_FOCUS);

	audition_button.set_events (audition_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));

	name_entry.set_name ("AudioRegionEditorEntry");
	name_label.set_name ("AudioRegionEditorLabel");
	position_label.set_name ("AudioRegionEditorLabel");
	position_label.set_text (_("Position:"));
	end_label.set_name ("AudioRegionEditorLabel");
	end_label.set_text (_("End:"));
	length_label.set_name ("AudioRegionEditorLabel");
	length_label.set_text (_("Length:"));
	sync_relative_label.set_name ("AudioRegionEditorLabel");
	sync_relative_label.set_text (_("Sync point (relative to region):"));
	sync_absolute_label.set_name ("AudioRegionEditorLabel");
	sync_absolute_label.set_text (_("Sync point (absolute):"));
	start_label.set_name ("AudioRegionEditorLabel");
	start_label.set_text (_("File start:"));

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
	gain_label.set_alignment (1, 0.5);

	Gtk::HBox* nb = Gtk::manage (new Gtk::HBox);
	nb->set_spacing (6);
	nb->pack_start (name_entry);
	nb->pack_start (audition_button);

	_table.attach (name_label, 0, 1, 0, 1, Gtk::FILL, Gtk::FILL);
	_table.attach (*nb, 1, 2, 0, 1, Gtk::FILL, Gtk::FILL);

	_table.attach (position_label, 0, 1, 1, 2, Gtk::FILL, Gtk::FILL);
	_table.attach (position_clock, 1, 2, 1, 2, Gtk::FILL, Gtk::FILL);

 	_table.attach (end_label, 0, 1, 2, 3, Gtk::FILL, Gtk::FILL);
	_table.attach (end_clock, 1, 2, 2, 3, Gtk::FILL, Gtk::FILL);
	
 	_table.attach (length_label, 0, 1, 3, 4, Gtk::FILL, Gtk::FILL);
	_table.attach (length_clock, 1, 2, 3, 4, Gtk::FILL, Gtk::FILL);
	
 	_table.attach (sync_relative_label, 0, 1, 4, 5, Gtk::FILL, Gtk::FILL);
 	_table.attach (sync_offset_relative_clock, 1, 2, 4, 5, Gtk::FILL, Gtk::FILL);
 
 	_table.attach (sync_absolute_label, 0, 1, 5, 6, Gtk::FILL, Gtk::FILL);
 	_table.attach (sync_offset_absolute_clock, 1, 2, 5, 6, Gtk::FILL, Gtk::FILL);

 	_table.attach (start_label, 0, 1, 6, 7, Gtk::FILL, Gtk::FILL);
 	_table.attach (start_clock, 1, 2, 6, 7, Gtk::FILL, Gtk::FILL);

	Gtk::HBox* gb = Gtk::manage (new Gtk::HBox);
	gb->set_spacing (6);
	gb->pack_start (gain_entry);
	gb->pack_start (*Gtk::manage (new Gtk::Label (_("dB"))), false, false);

	gain_label.set_name ("AudioRegionEditorLabel");
	gain_label.set_text (_("Region gain:"));
	gain_entry.configure (gain_adjustment, 0.0, 1);
	_table.attach (gain_label, 0, 1, 7, 8, Gtk::FILL, Gtk::FILL);
	_table.attach (*gb, 1, 2, 7, 8, Gtk::FILL, Gtk::FILL);
	
	get_vbox()->pack_start (_table, true, true);

	add_button (Gtk::Stock::CLOSE, Gtk::RESPONSE_ACCEPT);

	set_name ("AudioRegionEditorWindow");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	signal_delete_event().connect (sigc::bind (sigc::ptr_fun (just_hide_it), static_cast<Window *> (this)));
	signal_response().connect (sigc::mem_fun (*this, &AudioRegionEditor::handle_response));

	set_title (string_compose (_("Region '%1'"), _region->name()));

	show_all();

	name_changed ();

	PropertyChange change;

	change.add (ARDOUR::Properties::start);
	change.add (ARDOUR::Properties::length);
	change.add (ARDOUR::Properties::position);
	change.add (ARDOUR::Properties::sync_position);

	bounds_changed (change);

	gain_changed ();

	_region->PropertyChanged.connect (state_connection, invalidator (*this), ui_bind (&AudioRegionEditor::region_changed, this, _1), gui_context());

	spin_arrow_grab = false;

	connect_editor_events ();
}

AudioRegionEditor::~AudioRegionEditor ()
{
}

void
AudioRegionEditor::region_changed (const PBD::PropertyChange& what_changed)
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

	if (what_changed.contains (ARDOUR::Properties::scale_amplitude)) {
		gain_changed ();
	}
}

gint
AudioRegionEditor::bpressed (GdkEventButton* ev, Gtk::SpinButton* /*but*/, void (AudioRegionEditor::*/*pmf*/)())
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
AudioRegionEditor::breleased (GdkEventButton* /*ev*/, Gtk::SpinButton* /*but*/, void (AudioRegionEditor::*pmf)())
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
	name_entry.signal_changed().connect (sigc::mem_fun(*this, &AudioRegionEditor::name_entry_changed));

	position_clock.ValueChanged.connect (sigc::mem_fun(*this, &AudioRegionEditor::position_clock_changed));
	end_clock.ValueChanged.connect (sigc::mem_fun(*this, &AudioRegionEditor::end_clock_changed));
	length_clock.ValueChanged.connect (sigc::mem_fun(*this, &AudioRegionEditor::length_clock_changed));
	sync_offset_absolute_clock.ValueChanged.connect (sigc::mem_fun (*this, &AudioRegionEditor::sync_offset_absolute_clock_changed));
	sync_offset_relative_clock.ValueChanged.connect (sigc::mem_fun (*this, &AudioRegionEditor::sync_offset_relative_clock_changed));
	gain_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &AudioRegionEditor::gain_adjustment_changed));

	audition_button.signal_toggled().connect (sigc::mem_fun(*this, &AudioRegionEditor::audition_button_toggled));

	_session->AuditionActive.connect (audition_connection, invalidator (*this), ui_bind (&AudioRegionEditor::audition_state_changed, this, _1), gui_context());
}

void
AudioRegionEditor::position_clock_changed ()
{
	_session->begin_reversible_command (_("change region start position"));

	boost::shared_ptr<Playlist> pl = _region->playlist();

	if (pl) {
		_region->clear_history ();
		_region->set_position (position_clock.current_time(), this);
		_session->add_command(new StatefulDiffCommand (_region));
	}

	_session->commit_reversible_command ();
}

void
AudioRegionEditor::end_clock_changed ()
{
	_session->begin_reversible_command (_("change region end position"));

	boost::shared_ptr<Playlist> pl = _region->playlist();

	if (pl) {
                _region->clear_history ();
		_region->trim_end (end_clock.current_time(), this);
		_session->add_command(new StatefulDiffCommand (_region));
	}

	_session->commit_reversible_command ();

	end_clock.set (_region->position() + _region->length() - 1, true);
}

void
AudioRegionEditor::length_clock_changed ()
{
	nframes_t frames = length_clock.current_time();

	_session->begin_reversible_command (_("change region length"));

	boost::shared_ptr<Playlist> pl = _region->playlist();

	if (pl) {
                _region->clear_history ();
		_region->trim_end (_region->position() + frames - 1, this);
		_session->add_command(new StatefulDiffCommand (_region));
	}

	_session->commit_reversible_command ();

	length_clock.set (_region->length());
}

void
AudioRegionEditor::gain_changed ()
{
	float const region_gain_dB = accurate_coefficient_to_dB (_region->scale_amplitude());
	if (region_gain_dB != gain_adjustment.get_value()) {
		gain_adjustment.set_value(region_gain_dB);
	}
}

void
AudioRegionEditor::gain_adjustment_changed ()
{
	float const gain = dB_to_coefficient (gain_adjustment.get_value());
	if (_region->scale_amplitude() != gain) {
		_region->set_scale_amplitude (gain);
	}
}

void
AudioRegionEditor::audition_button_toggled ()
{
	if (audition_button.get_active()) {
		_session->audition_region (_region);
	} else {
		_session->cancel_audition ();
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
AudioRegionEditor::bounds_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::position) && what_changed.contains (ARDOUR::Properties::length)) {
		position_clock.set (_region->position(), true);
		end_clock.set (_region->position() + _region->length() - 1, true);
		length_clock.set (_region->length(), true);
	} else if (what_changed.contains (ARDOUR::Properties::position)) {
		position_clock.set (_region->position(), true);
		end_clock.set (_region->position() + _region->length() - 1, true);
	} else if (what_changed.contains (ARDOUR::Properties::length)) {
		end_clock.set (_region->position() + _region->length() - 1, true);
		length_clock.set (_region->length(), true);
	}

	if (what_changed.contains (ARDOUR::Properties::sync_position) || what_changed.contains (ARDOUR::Properties::position)) {
		int dir;
		nframes_t off = _region->sync_offset (dir);
		if (dir == -1) {
			off = -off;
		}

		if (what_changed.contains (ARDOUR::Properties::sync_position)) {
			sync_offset_relative_clock.set (off, true);
		}

		sync_offset_absolute_clock.set (off + _region->position (), true);
	}

	if (what_changed.contains (ARDOUR::Properties::start)) {
		start_clock.set (_region->start(), true);
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
	ENSURE_GUI_THREAD (*this, &AudioRegionEditor::audition_state_changed, yn)

	if (!yn) {
		audition_button.set_active (false);
	}
}

void
AudioRegionEditor::sync_offset_absolute_clock_changed ()
{
	_session->begin_reversible_command (_("change region sync point"));

        _region->clear_history ();
	_region->set_sync_position (sync_offset_absolute_clock.current_time());
	_session->add_command (new StatefulDiffCommand (_region));
	
	_session->commit_reversible_command ();
}

void
AudioRegionEditor::sync_offset_relative_clock_changed ()
{
	_session->begin_reversible_command (_("change region sync point"));

        _region->clear_history ();
	_region->set_sync_position (sync_offset_relative_clock.current_time() + _region->position ());
	_session->add_command (new StatefulDiffCommand (_region));
	
	_session->commit_reversible_command ();
}

bool
AudioRegionEditor::on_delete_event (GdkEventAny* ev)
{
	PropertyChange change;

	change.add (ARDOUR::Properties::start);
	change.add (ARDOUR::Properties::length);
	change.add (ARDOUR::Properties::position);
	change.add (ARDOUR::Properties::sync_position);

	bounds_changed (change);

	return RegionEditor::on_delete_event (ev);
}

void
AudioRegionEditor::handle_response (int)
{
	hide ();
}
