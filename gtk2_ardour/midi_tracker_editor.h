/*
    Copyleft (C) 2015 Nil Geisweiller

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

#ifndef __ardour_gtk2_midi_tracker_editor_h_
#define __ardour_gtk2_midi_tracker_editor_h_

#include <gtkmm/treeview.h>
#include <gtkmm/table.h>
#include <gtkmm/box.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>

#include "evoral/types.hpp"

#include "ardour/session_handle.h"

#include "ardour_window.h"

namespace Evoral {
	template<typename Time> class Note;
};

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class MidiTrack;
	class Session;
};

class MidiTrackerEditor : public ArdourWindow
{
  public:
	typedef Evoral::Note<Evoral::Beats> NoteType;

	MidiTrackerEditor(ARDOUR::Session*, boost::shared_ptr<ARDOUR::MidiRegion>,
		       boost::shared_ptr<ARDOUR::MidiTrack>);
	~MidiTrackerEditor();

  private:
	struct MidiTrackerModelColumns : public Gtk::TreeModel::ColumnRecord {
		MidiTrackerModelColumns()
		{
			add (time);
			add (note_name);
			add (channel);
			add (velocity);
			add (delay);
			add (_note);		// We keep that around to play the note
		};
		Gtk::TreeModelColumn<std::string> time;
		Gtk::TreeModelColumn<std::string> note_name;
		Gtk::TreeModelColumn<uint8_t>     channel;
		Gtk::TreeModelColumn<uint8_t>     velocity;
		Gtk::TreeModelColumn<int16_t>     delay;
		Gtk::TreeModelColumn<boost::shared_ptr<NoteType> > _note;
	};

	enum tracker_columns {
		TIME_COLNUM,
		NOTE_COLNUM,
		CHANNEL_COLNUM,
		VELOCITY_COLNUM,
		DELAY_COLNUM,
		TRACKER_COLNUM_COUNT
	};

	static const std::string note_off_str;
	
	MidiTrackerModelColumns      columns;
	Glib::RefPtr<Gtk::ListStore> model;
	Gtk::TreeView                view;
	Gtk::ScrolledWindow          scroller;
	Gtk::TreeModel::Path         edit_path;
	int                          edit_column;
	Gtk::CellRendererText*       editing_renderer;
	Gtk::CellEditable*           editing_editable;
	Gtk::Table                   buttons;
	Gtk::VBox                    vbox;
	Gtk::ToggleButton            sound_notes_button;

	boost::shared_ptr<ARDOUR::MidiRegion> region;
	boost::shared_ptr<ARDOUR::MidiTrack>  track;

	/** connection used to connect to model's ContentChanged signal */
	PBD::ScopedConnection content_connection;

	void edited (const std::string&, const std::string&);
	void editing_started (Gtk::CellEditable*, const std::string& path, int);
	void editing_canceled ();
	void stop_editing (bool cancelled = false);

	void redisplay_model ();

	bool key_press (GdkEventKey* ev);
	bool key_release (GdkEventKey* ev);
	bool scroll_event (GdkEventScroll*);

	void delete_selected_note ();
	void selection_changed ();
};

#endif /* __ardour_gtk2_midi_tracker_editor_h_ */
