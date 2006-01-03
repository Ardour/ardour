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

#ifndef __gtk_ardour_region_edit_h__
#define __gtk_ardour_region_edit_h__

#include <map>

#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/box.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/button.h>
#include <gtkmm/arrow.h>
#include <gtkmm/frame.h>
#include <gtkmm/table.h>
#include <gtkmm/alignment.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/separator.h>
#include <gtkmm/spinbutton.h>

#include <libgnomecanvas/libgnomecanvas.h>
#include <sigc++/signal.h>

#include "audio_clock.h"
#include "ardour_dialog.h"

namespace ARDOUR {
	class AudioRegion;
	class Session;
}

class AudioRegionView;

class AudioRegionEditor : public ArdourDialog
{
  public:
	AudioRegionEditor (ARDOUR::Session&, ARDOUR::AudioRegion&, AudioRegionView& rv);
	~AudioRegionEditor ();

  private:
	ARDOUR::Session& _session;
	ARDOUR::AudioRegion& _region;
	AudioRegionView& _region_view;

	void connect_editor_events ();

	Gtk::Label name_label;
	Gtk::Entry name_entry;
	Gtk::HBox  name_hbox;

	Gtk::HBox  top_row_hbox;
	Gtk::HBox  top_row_button_hbox;

	Gtk::ToggleButton lock_button;
	Gtk::ToggleButton mute_button;
	Gtk::ToggleButton opaque_button;
	Gtk::ToggleButton envelope_active_button;
	Gtk::ToggleButton envelope_view_button;

	Gtk::Button       raise_button;
	Gtk::Arrow        raise_arrow;
	Gtk::Button       lower_button;
	Gtk::Arrow        lower_arrow;
	Gtk::Frame        layer_frame;
	Gtk::Label        layer_value_label;
	Gtk::Label        layer_label;
	Gtk::HBox         layer_hbox;

	Gtk::ToggleButton  audition_button;

	Gtk::HBox  lower_hbox;
	
	Gtk::Table time_table;

	Gtk::Label start_label;
	Gtk::Label end_label;
	Gtk::Label length_label;
	Gtk::Alignment start_alignment;
	Gtk::Alignment end_alignment;
	Gtk::Alignment length_alignment;

	AudioClock start_clock;
	AudioClock end_clock;
	AudioClock length_clock;
	AudioClock sync_offset_clock;

	Gtk::Table  envelope_loop_table;
	Gtk::Button loop_button;
	Gtk::Label  loop_label;
	Gtk::Label  envelope_label;

	Gtk::Table fade_in_table;
	Gtk::Label fade_in_label;
	Gtk::Alignment fade_in_label_align;
	Gtk::Label fade_in_active_button_label;
	Gtk::ToggleButton fade_in_active_button;
	Gtk::Label fade_in_length_label;

	Gtk::Adjustment fade_in_length_adjustment;
	Gtk::SpinButton fade_in_length_spinner;

	Gtk::Table fade_out_table;
	Gtk::Label fade_out_label;
	Gtk::Alignment fade_out_label_align;
	Gtk::Label fade_out_active_button_label;
	Gtk::ToggleButton fade_out_active_button;
	Gtk::Label fade_out_length_label;

	Gtk::Adjustment fade_out_length_adjustment;
	Gtk::SpinButton fade_out_length_spinner;

	Gtk::HSeparator sep3;
	Gtk::VSeparator sep1;
	Gtk::VSeparator sep2;

	void region_changed (ARDOUR::Change);
	void bounds_changed (ARDOUR::Change);
	void name_changed ();
	void opacity_changed ();
	void mute_changed ();
	void envelope_active_changed ();
	void lock_changed ();
	void layer_changed ();

	void fade_in_length_adjustment_changed ();
	void fade_out_length_adjustment_changed ();
	void fade_in_changed ();
	void fade_out_changed ();
	void audition_state_changed (bool);

	void activation ();

	void name_entry_changed ();
	void start_clock_changed ();
	void end_clock_changed ();
	void length_clock_changed ();

	gint envelope_active_button_press (GdkEventButton *);
	gint envelope_active_button_release (GdkEventButton *);

	void audition_button_toggled ();
	void envelope_view_button_toggled ();
	void lock_button_clicked ();
	void mute_button_clicked ();
	void opaque_button_clicked ();
	void raise_button_clicked ();
	void lower_button_clicked ();

	void fade_in_active_toggled ();
	void fade_out_active_toggled ();
	void fade_in_active_changed ();
	void fade_out_active_changed ();

	void fade_in_realized ();
	void fade_out_realized ();

	void start_editing_fade_in ();
	void start_editing_fade_out ();
	void stop_editing_fade_in ();
	void stop_editing_fade_out ();

	gint bpressed (GdkEventButton* ev, Gtk::SpinButton* but, void (AudioRegionEditor::*pmf)());
	gint breleased (GdkEventButton* ev, Gtk::SpinButton* but, void (AudioRegionEditor::*pmf)());

	bool spin_arrow_grab;
};

#endif /* __gtk_ardour_region_edit_h__ */
