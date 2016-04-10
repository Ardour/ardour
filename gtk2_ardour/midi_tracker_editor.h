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

#include "gtkmm2ext/bindings.h"

#include "evoral/types.hpp"

#include "ardour/session_handle.h"

#include "ardour_dropdown.h"
#include "ardour_window.h"
#include "editing.h"

#include "midi_tracker_matrix.h"

namespace Evoral {
	template<typename Time> class Note;
};

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class MidiTrack;
	class Session;
};

// Maximum number of tracks in the midi tracker editor
#define MAX_NUMBER_OF_TRACKS 256

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
			for (size_t i = 0; i < MAX_NUMBER_OF_TRACKS; ++i) {
				add (note_name[i]);
				add (channel[i]);
				add (velocity[i]);
				add (delay[i]);
				add (_note[i]);		// We keep that around to play the note
			}
			add (_color);		// The background color differs when the row is
								// on beats and bars. This is to keep track of
								// it.
		};
		Gtk::TreeModelColumn<std::string> time;
		Gtk::TreeModelColumn<std::string> note_name[MAX_NUMBER_OF_TRACKS];
		Gtk::TreeModelColumn<std::string> channel[MAX_NUMBER_OF_TRACKS];
		Gtk::TreeModelColumn<std::string> velocity[MAX_NUMBER_OF_TRACKS];
		Gtk::TreeModelColumn<std::string> delay[MAX_NUMBER_OF_TRACKS];
		Gtk::TreeModelColumn<boost::shared_ptr<NoteType> > _note[MAX_NUMBER_OF_TRACKS];
		Gtk::TreeModelColumn<std::string> _color;
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

	// If the resolution isn't fine enough and multiple notes do not fit in the
	// same row, then this string is printed.
	static const std::string undefined_str;

	MidiTrackerModelColumns      columns;
	Glib::RefPtr<Gtk::ListStore> model;
	Gtk::TreeView                view;
	Gtk::ScrolledWindow          scroller;
	Gtk::TreeModel::Path         edit_path;
	int                          edit_column;
	Gtk::CellRendererText*       editing_renderer;
	Gtk::CellEditable*           editing_editable;
	Gtk::Table                   buttons;
	Gtk::HBox                    toolbar;
	Gtk::VBox                    vbox;
	Gtkmm2ext::ActionMap         myactions;
	ArdourDropdown               beats_per_row_selector;
	std::vector<std::string>     beats_per_row_strings;
	uint8_t                      rows_per_beat;
	ArdourButton                 visible_blank_button;
	ArdourButton                 visible_note_button;
	ArdourButton                 visible_channel_button;
	ArdourButton                 visible_velocity_button;
	ArdourButton                 visible_delay_button;
	bool                         visible_blank;
	bool                         visible_note;
	bool                         visible_channel;
	bool                         visible_velocity;
	bool                         visible_delay;
	ArdourButton                 automation_button;
	Gtk::Menu                    subplugin_menu;
	Gtk::Menu*                   automation_action_menu;

	typedef std::map<Evoral::Parameter, Gtk::CheckMenuItem*> ParameterMenuMap;
	/** parameter -> menu item map for the main automation menu */
	ParameterMenuMap _main_automation_menu_map;

	boost::shared_ptr<ARDOUR::MidiRegion> region;
	boost::shared_ptr<ARDOUR::MidiTrack>  track;
	boost::shared_ptr<ARDOUR::MidiModel>  midi_model;

	MidiTrackerMatrix* mtm;

	/** connection used to connect to model's ContentChanged signal */
	PBD::ScopedConnection content_connection;

	void build_beats_per_row_menu ();
	void build_automation_action_menu ();

	void register_actions ();

	bool visible_blank_press (GdkEventButton*);
	bool visible_note_press (GdkEventButton*);
	bool visible_channel_press (GdkEventButton*);
	bool visible_velocity_press (GdkEventButton*);
	bool visible_delay_press (GdkEventButton*);
	void redisplay_visible_note ();
	void redisplay_visible_channel ();
	void redisplay_visible_velocity ();
	void redisplay_visible_delay ();
	void automation_click ();

	virtual void show_all_automation ();
	virtual void show_existing_automation ();
	virtual void hide_all_automation ();

	void setup_tooltips ();
	void setup_toolbar ();
	void setup_matrix ();
	void setup_scroller ();
	void redisplay_model ();

	// Beats per row corresponds to a SnapType. I could have user an integer
	// directly but I prefer to use the SnapType to be more consistent with the
	// main editor.
	void set_beats_per_row_to (Editing::SnapType);
	void beats_per_row_selection_done (Editing::SnapType);
	Glib::RefPtr<Gtk::RadioAction> beats_per_row_action (Editing::SnapType);
	void beats_per_row_chosen (Editing::SnapType);

	// Make it up for the lack of C++11 support
	template<typename T> std::string to_string(const T& v)
	{
		std::stringstream ss;
		ss << v;
		return ss.str();
	}
};

#endif /* __ardour_gtk2_midi_tracker_editor_h_ */
