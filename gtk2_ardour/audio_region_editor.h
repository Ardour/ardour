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

#ifndef __gtk_ardour_audio_region_edit_h__
#define __gtk_ardour_audio_region_edit_h__

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
#include "region_editor.h"

namespace ARDOUR {
	class AudioRegion;
	class Session;
}

class AudioRegionView;

class AudioRegionEditor : public RegionEditor
{
  public:
	AudioRegionEditor (ARDOUR::Session&, boost::shared_ptr<ARDOUR::AudioRegion>, AudioRegionView& rv);
	~AudioRegionEditor ();

  private:
	boost::shared_ptr<ARDOUR::AudioRegion> _region;
	AudioRegionView& _region_view;

	void connect_editor_events ();

	Gtk::Label name_label;
	Gtk::Entry name_entry;
	Gtk::HBox  name_hbox;

	Gtk::HBox  top_row_hbox;
	Gtk::HBox  top_row_button_hbox;

	Gtk::ToggleButton  audition_button;

	Gtk::HBox  lower_hbox;

	Gtk::Table time_table;

	Gtk::Label position_label;
	Gtk::Label end_label;
	Gtk::Label length_label;
	Gtk::Label sync_label;
	Gtk::Label start_label;
	Gtk::Alignment position_alignment;
	Gtk::Alignment end_alignment;
	Gtk::Alignment length_alignment;
	Gtk::Alignment sync_alignment;
	Gtk::Alignment start_alignment;

	AudioClock position_clock;
	AudioClock end_clock;
	AudioClock length_clock;
	AudioClock sync_offset_clock;
	AudioClock start_clock;

	Gtk::HSeparator sep3;
	Gtk::VSeparator sep1;
	Gtk::VSeparator sep2;

	void region_changed (ARDOUR::Change);
	void bounds_changed (ARDOUR::Change);
	void name_changed ();

	void audition_state_changed (bool);

	void activation ();

	void name_entry_changed ();
	void position_clock_changed ();
	void end_clock_changed ();
	void length_clock_changed ();

	void audition_button_toggled ();

	gint bpressed (GdkEventButton* ev, Gtk::SpinButton* but, void (AudioRegionEditor::*pmf)());
	gint breleased (GdkEventButton* ev, Gtk::SpinButton* but, void (AudioRegionEditor::*pmf)());

	bool spin_arrow_grab;
};

#endif /* __gtk_ardour_audio_region_edit_h__ */
