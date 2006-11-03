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

#include <ardour/audioregion.h>
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
	  lock_button (_("lock")),
	  mute_button (_("mute")),
	  opaque_button (_("opaque")),
	  envelope_active_button(_("active")),
	  envelope_view_button(_("visible")),
	  raise_arrow (Gtk::ARROW_UP, Gtk::SHADOW_OUT),
	  lower_arrow (Gtk::ARROW_DOWN, Gtk::SHADOW_OUT),
	  layer_label (_("Layer")),
	  audition_button (_("play")),
	  time_table (3, 2),
	  start_clock ("AudioRegionEditorClock", true),
	  end_clock ("AudioRegionEditorClock", true),
	  length_clock ("AudioRegionEditorClock", true, true),
	  sync_offset_clock ("AudioRegionEditorClock", true, true),
	  envelope_loop_table (1, 3),
	  envelope_label (_("ENVELOPE"))

{
	start_clock.set_session (&_session);
	end_clock.set_session (&_session);
	length_clock.set_session (&_session);

	name_entry.set_name ("AudioRegionEditorEntry");
	name_label.set_name ("AudioRegionEditorLabel");

	name_hbox.set_spacing (5);
	name_hbox.pack_start (name_label, false, false);
	name_hbox.pack_start (name_entry, false, false);

	raise_button.add (raise_arrow);
	lower_button.add (lower_arrow);
	layer_frame.set_name ("BaseFrame");
	layer_frame.set_shadow_type (Gtk::SHADOW_IN);
	layer_frame.add (layer_value_label);
	layer_label.set_name ("AudioRegionEditorLabel");
	layer_value_label.set_name ("AudioRegionEditorLabel");
	Gtkmm2ext::set_size_request_to_display_given_text (layer_value_label, "99", 5, 2);

	layer_hbox.set_spacing (5);
	layer_hbox.pack_start (layer_label, false, false);
	layer_hbox.pack_start (layer_frame, false, false);
#if 0
	layer_hbox.pack_start (raise_button, false, false);
	layer_hbox.pack_start (lower_button, false, false);
#endif

	mute_button.set_name ("AudioRegionEditorToggleButton");
	opaque_button.set_name ("AudioRegionEditorToggleButton");
	lock_button.set_name ("AudioRegionEditorToggleButton");
	envelope_active_button.set_name ("AudioRegionEditorToggleButton");
	envelope_view_button.set_name ("AudioRegionEditorToggleButton");

	ARDOUR_UI::instance()->tooltips().set_tip (mute_button, _("mute this region"));
	ARDOUR_UI::instance()->tooltips().set_tip (opaque_button, _("regions underneath this one cannot be heard"));
	ARDOUR_UI::instance()->tooltips().set_tip (lock_button, _("prevent any changes to this region"));
	ARDOUR_UI::instance()->tooltips().set_tip (envelope_active_button, _("use the gain envelope during playback"));
	ARDOUR_UI::instance()->tooltips().set_tip (envelope_view_button, _("show the gain envelope"));
	ARDOUR_UI::instance()->tooltips().set_tip (audition_button, _("audition this region"));

	mute_button.unset_flags (Gtk::CAN_FOCUS);
	opaque_button.unset_flags (Gtk::CAN_FOCUS);
	lock_button.unset_flags (Gtk::CAN_FOCUS);
	envelope_active_button.unset_flags (Gtk::CAN_FOCUS);
	envelope_view_button.unset_flags (Gtk::CAN_FOCUS);
	audition_button.unset_flags (Gtk::CAN_FOCUS);
	
	mute_button.set_events (mute_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	opaque_button.set_events (opaque_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	lock_button.set_events (lock_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	envelope_active_button.set_events (envelope_active_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	envelope_view_button.set_events (envelope_view_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	audition_button.set_events (audition_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));

	top_row_button_hbox.set_border_width (5);
	top_row_button_hbox.set_spacing (5);
	top_row_button_hbox.set_homogeneous (false);
	top_row_button_hbox.pack_start (mute_button, false, false);
	top_row_button_hbox.pack_start (opaque_button, false, false);
	top_row_button_hbox.pack_start (lock_button, false, false);
	top_row_button_hbox.pack_start (layer_hbox, false, false, 5);
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

	envelope_label.set_name ("AudioRegionEditorLabel");

	envelope_loop_table.set_border_width (5);
	envelope_loop_table.set_row_spacings (2);
	envelope_loop_table.attach (envelope_label, 0, 1, 0, 1, Gtk::FILL, Gtk::FILL);
	envelope_loop_table.attach (envelope_active_button, 0, 1, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL);
	envelope_loop_table.attach (envelope_view_button, 0, 1, 2, 3, Gtk::FILL|Gtk::EXPAND, Gtk::FILL);

	lower_hbox.pack_start (time_table, true, true);
	lower_hbox.pack_start (sep1, false, false);
	lower_hbox.pack_start (envelope_loop_table, true, true);
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
	envelope_active_changed ();
	mute_changed ();
	opacity_changed ();
	lock_changed ();
	layer_changed ();

	XMLNode *node  = _region->extra_xml ("GUI");
	XMLProperty *prop = 0;
	bool showing_envelope = false;

	if (node && (prop = node->property ("envelope-visible")) != 0) {
		if (prop->value() == "yes") {
			showing_envelope = true;
		} 
	} 

	if (showing_envelope) {
		envelope_view_button.set_active (true);
	} else {
		envelope_view_button.set_active (false);
	}

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

	if (what_changed & Region::OpacityChanged) {
		opacity_changed ();
	}
	if (what_changed & Region::MuteChanged) {
		mute_changed ();
	}
	if (what_changed & Region::LockChanged) {
		lock_changed ();
	}
	if (what_changed & Region::LayerChanged) {
		layer_changed ();
	}

	if (what_changed & AudioRegion::EnvelopeActiveChanged) {
		envelope_active_changed ();
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

	envelope_active_button.signal_button_press_event().connect (mem_fun(*this, &AudioRegionEditor::envelope_active_button_press));
	envelope_active_button.signal_button_release_event().connect (mem_fun(*this, &AudioRegionEditor::envelope_active_button_release));
	audition_button.signal_toggled().connect (mem_fun(*this, &AudioRegionEditor::audition_button_toggled));
	envelope_view_button.signal_toggled().connect (mem_fun(*this, &AudioRegionEditor::envelope_view_button_toggled));
	lock_button.signal_clicked().connect (mem_fun(*this, &AudioRegionEditor::lock_button_clicked));
	mute_button.signal_clicked().connect (mem_fun(*this, &AudioRegionEditor::mute_button_clicked));
	opaque_button.signal_clicked().connect (mem_fun(*this, &AudioRegionEditor::opaque_button_clicked));
	raise_button.signal_clicked().connect (mem_fun(*this, &AudioRegionEditor::raise_button_clicked));
	lower_button.signal_clicked().connect (mem_fun(*this, &AudioRegionEditor::lower_button_clicked));
	_session.AuditionActive.connect (mem_fun(*this, &AudioRegionEditor::audition_state_changed));
}

void
AudioRegionEditor::start_clock_changed ()
{
	_region->set_position (start_clock.current_time(), this);
}

void
AudioRegionEditor::end_clock_changed ()
{
	_region->trim_end (end_clock.current_time(), this);

	end_clock.set (_region->position() + _region->length(), true);
}

void
AudioRegionEditor::length_clock_changed ()
{
	nframes_t frames = length_clock.current_time();
	_region->trim_end (_region->position() + frames, this);

	length_clock.set (_region->length());
}

gint
AudioRegionEditor::envelope_active_button_press(GdkEventButton *ev)
{
	return stop_signal (envelope_active_button, "button_press_event");
}

gint
AudioRegionEditor::envelope_active_button_release (GdkEventButton *ev)
{
	_region->set_envelope_active (!_region->envelope_active());
	return stop_signal (envelope_active_button, "button_release_event");
}

void
AudioRegionEditor::envelope_view_button_toggled ()
{
	bool visible = envelope_view_button.get_active ();

	_region_view.set_envelope_visible (visible);
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
AudioRegionEditor::raise_button_clicked ()
{
	_region->raise ();
}

void
AudioRegionEditor::lower_button_clicked ()
{
	_region->lower ();
}

void
AudioRegionEditor::opaque_button_clicked ()
{
	bool ractive = _region->opaque();

	if (opaque_button.get_active() != ractive) {
		_region->set_opaque (!ractive);
	}
}

void
AudioRegionEditor::mute_button_clicked ()
{
	bool ractive = _region->muted();

	if (mute_button.get_active() != ractive) {
		_region->set_muted (!ractive);
	}
}

void
AudioRegionEditor::lock_button_clicked ()
{
	bool ractive = _region->locked();

	if (lock_button.get_active() != ractive) {
		_region->set_locked (!ractive);
	}
}

void
AudioRegionEditor::layer_changed ()
{
	char buf[8];
	snprintf (buf, sizeof(buf), "%d", (int) _region->layer() + 1);
	layer_value_label.set_text (buf);
}

void
AudioRegionEditor::name_changed ()
{
	if (name_entry.get_text() != _region->name()) {
		name_entry.set_text (_region->name());
	}
}

void
AudioRegionEditor::lock_changed ()
{
	bool yn;

	if ((yn = _region->locked()) != lock_button.get_active()) {
		lock_button.set_active (yn);
	}

	start_clock.set_sensitive (!yn);
	end_clock.set_sensitive (!yn);
	length_clock.set_sensitive (!yn);
}

void
AudioRegionEditor::envelope_active_changed ()
{
	bool yn;

	if ((yn = _region->envelope_active()) != envelope_active_button.get_active()) {
		envelope_active_button.set_active (yn);
	}
}

void
AudioRegionEditor::opacity_changed ()
{
	bool yn;
	if ((yn = _region->opaque()) != opaque_button.get_active()) {
		opaque_button.set_active (yn);
	}
}

void
AudioRegionEditor::mute_changed ()
{
	bool yn;
	if ((yn = _region->muted()) != mute_button.get_active()) {
		mute_button.set_active (yn);
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

