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
#include <gtkmm/adjustment.h>
#include <gtkmm/separator.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/listviewtext.h>


#include "pbd/signals.h"

#include "audio_clock.h"
#include "ardour_dialog.h"
#include "region_editor.h"

namespace ARDOUR {
	class Region;
	class Session;
}

class ClockGroup;

class RegionEditor : public ArdourDialog
{
  public:
	RegionEditor (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Region>);
	virtual ~RegionEditor ();

  protected:
	virtual void region_changed (const PBD::PropertyChange&);

	Gtk::Table _table;
	int _table_row;

  private:
	boost::shared_ptr<ARDOUR::Region> _region;

	void connect_editor_events ();

	Gtk::Label name_label;
	Gtk::Entry name_entry;
	Gtk::ToggleButton audition_button;

	Gtk::Label position_label;
	Gtk::Label end_label;
	Gtk::Label length_label;
	Gtk::Label sync_relative_label;
	Gtk::Label sync_absolute_label;
	Gtk::Label start_label;

        ClockGroup* _clock_group;

	AudioClock position_clock;
	AudioClock end_clock;
	AudioClock length_clock;
	AudioClock sync_offset_relative_clock; ///< sync offset relative to the start of the region
	AudioClock sync_offset_absolute_clock; ///< sync offset relative to the start of the timeline
	AudioClock start_clock;

	PBD::ScopedConnection state_connection;
	PBD::ScopedConnection audition_connection;

	void bounds_changed (const PBD::PropertyChange&);
	void name_changed ();

	void audition_state_changed (bool);

	void activation ();

	void name_entry_changed ();
	void position_clock_changed ();
	void end_clock_changed ();
	void length_clock_changed ();
	void sync_offset_absolute_clock_changed ();
	void sync_offset_relative_clock_changed ();

	void audition_button_toggled ();

	gint bpressed (GdkEventButton* ev, Gtk::SpinButton* but, void (RegionEditor::*pmf)());
	gint breleased (GdkEventButton* ev, Gtk::SpinButton* but, void (RegionEditor::*pmf)());

	bool on_delete_event (GdkEventAny *);
	void handle_response (int);

	bool spin_arrow_grab;

	Gtk::Label _sources_label;
	Gtk::ListViewText _sources;
};

#endif /* __gtk_ardour_region_edit_h__ */
