/*
    Copyright (C) 2015-2016 Nil Geisweiller

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
#include "midi_time_axis.h"

#include "midi_tracker_pattern.h"
#include "automation_tracker_pattern.h"

namespace Evoral {
	template<typename Time> class Note;
};

namespace MIDI {
namespace Name {
class MasterDeviceNames;
class CustomDeviceMode;
}
}

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class MidiTrack;
	class Session;
	class Route;
	class Processor;
};

class AutomationTimeAxisView;

// Maximum number of note and automation tracks. Temporary limit before a
// dedicated widget is created to replace Gtk::TreeModel::ColumnRecord

// Maximum number of note tracks in the midi tracker editor
#define MAX_NUMBER_OF_NOTE_TRACKS 64

// Maximum number of automation trackts in the midi tracker editor
#define MAX_NUMBER_OF_AUTOMATION_TRACKS 64

// Test if element is in container
template<typename C>
bool is_in(const typename C::value_type& element, const C& container)
{
	return container.find(element) != container.end();
}

// Test if key is in map
template<typename M>
bool is_key_in(const typename M::key_type& key, const M& map)
{
	return map.find(key) != map.end();
}

class MidiTrackerEditor : public ArdourWindow
{
  public:
	typedef Evoral::Note<Evoral::Beats> NoteType;

	MidiTrackerEditor(ARDOUR::Session*, MidiTimeAxisView*, boost::shared_ptr<ARDOUR::Route>, boost::shared_ptr<ARDOUR::MidiRegion>, boost::shared_ptr<ARDOUR::MidiTrack>);
	~MidiTrackerEditor();

  private:

	///////////////////
	// Automation	 //
	///////////////////

	struct ProcessorAutomationNode {
		Evoral::Parameter                         what;
		Gtk::CheckMenuItem*                       menu_item;
		// corresponding column index. If set to 0 then undetermined yet
		size_t                                    column;
		MidiTrackerEditor&                        parent;

		ProcessorAutomationNode (Evoral::Parameter w, Gtk::CheckMenuItem* mitem, MidiTrackerEditor& p)
		    : what (w), menu_item (mitem), column(0), parent (p) {}

	    ~ProcessorAutomationNode ();
	};

	struct ProcessorAutomationInfo {
	    boost::shared_ptr<ARDOUR::Processor>  processor;
	    bool                                  valid;
	    Gtk::Menu*                            menu;
	    std::vector<ProcessorAutomationNode*> columns;

	    ProcessorAutomationInfo (boost::shared_ptr<ARDOUR::Processor> i)
		    : processor (i), valid (true), menu (0) {}

	    ~ProcessorAutomationInfo ();
	};

	/** Information about all automatable processor parameters that apply to
	 *  this route.  The Amp processor is not included in this list.
	 */
	std::list<ProcessorAutomationInfo*> processor_automation;

	// Column index of the first automation
	size_t automation_col_offset;

	// List of column indices currently unassigned to an automation
	std::set<size_t> available_automation_columns;

	// Map column index to automation parameter
	std::map<size_t, Evoral::Parameter> col2param;

	// Keep track of all visible automation columns
	std::set<size_t> visible_automation_columns;

	// Map Parameter to AutomationControl
	typedef std::map<Evoral::Parameter, boost::shared_ptr<ARDOUR::AutomationControl> > Parameter2AutomationControl;
	Parameter2AutomationControl param2actrl;

	// Map column index to automation track index
	std::map<size_t, size_t> col2autotrack;

	ArdourButton                 automation_button;
	Gtk::Menu                    subplugin_menu;
	Gtk::Menu*                   automation_action_menu;
	Gtk::Menu*                   controller_menu;

	typedef std::map<Evoral::Parameter, Gtk::CheckMenuItem*> ParameterMenuMap;
	/** parameter -> menu item map for the main automation menu */
	ParameterMenuMap _main_automation_menu_map;  // TODO: implement!
	/** parameter -> menu item map for the plugin automation menu */
	ParameterMenuMap _subplugin_menu_map;
	/** parameter -> menu item map for the channel command items */
	ParameterMenuMap _channel_command_menu_map;
	/** parameter -> menu item map for the controller menu */
	ParameterMenuMap _controller_menu_map;

	size_t gain_column;
	size_t trim_column; // TODO: support audio tracks
	size_t mute_column;
	std::vector<size_t> pan_columns;

	Gtk::CheckMenuItem* gain_automation_item;
	Gtk::CheckMenuItem* trim_automation_item;
	Gtk::CheckMenuItem* mute_automation_item;
	Gtk::CheckMenuItem* pan_automation_item;

	ProcessorAutomationNode* find_processor_automation_node (boost::shared_ptr<ARDOUR::Processor> processor, Evoral::Parameter what);

	// Assign an automation parameter to a column and return the corresponding
	// column index
	size_t add_automation_column (const Evoral::Parameter& param);
	void add_processor_automation_column (boost::shared_ptr<ARDOUR::Processor> processor, const Evoral::Parameter& what);

	void build_param2actrl ();

	virtual void show_all_automation ();
	virtual void show_existing_automation ();
	virtual void hide_all_automation ();

	void setup_processor_menu_and_curves ();
	void add_processor_to_subplugin_menu (boost::weak_ptr<ARDOUR::Processor>);
	void processor_menu_item_toggled (MidiTrackerEditor::ProcessorAutomationInfo*, MidiTrackerEditor::ProcessorAutomationNode*);
	void build_automation_action_menu ();
	void add_channel_command_menu_item (Gtk::Menu_Helpers::MenuList& items, const std::string& label, ARDOUR::AutomationType auto_type, uint8_t cmd);
	void change_all_channel_tracks_visibility (bool yn, Evoral::Parameter param);
	void toggle_automation_track (const Evoral::Parameter& param);
	void build_controller_menu ();
	boost::shared_ptr<MIDI::Name::MasterDeviceNames> get_device_names();
	void add_single_channel_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items, int ctl, const std::string& name);
	void add_multi_channel_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items, int ctl, const std::string& name);

	// Return true if the gain column is visible
	bool is_gain_visible ();
	bool is_mute_visible ();
	bool is_pan_visible ();
	void update_gain_column_visibility ();
	void update_trim_column_visibility ();
	void update_mute_column_visibility ();
	void update_pan_columns_visibility ();

	// Show/hide gain, mute and pan
	void show_all_main_automations ();
	void show_existing_main_automations ();
	void hide_main_automations ();

	////////////////////////////
	// Other (to sort out)	  //
	////////////////////////////

	struct MidiTrackerModelColumns : public Gtk::TreeModel::ColumnRecord {
		MidiTrackerModelColumns()
		{
			// The background color differs when the row is on beats and
			// bars. This is to keep track of it.
			add (_background_color);
			add (time);
			for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS; i++) {
				add (note_name[i]);
				add (_note_foreground_color[i]);
				add (channel[i]);
				add (_channel_foreground_color[i]);
				add (velocity[i]);
				add (_velocity_foreground_color[i]);
				add (delay[i]);
				add (_delay_foreground_color[i]);
				add (_note[i]);		// We keep that around to play the note
			}
			for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS; i++) {
				add (automation[i]);
				add (_automation[i]);
				add (_automation_foreground_color[i]);
				add (automation_delay[i]);
				add (_automation_delay_foreground_color[i]);
			}
		};
		Gtk::TreeModelColumn<std::string> _background_color;
		Gtk::TreeModelColumn<std::string> time;
		Gtk::TreeModelColumn<std::string> note_name[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _note_foreground_color[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> channel[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _channel_foreground_color[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> velocity[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _velocity_foreground_color[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> delay[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _delay_foreground_color[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<boost::shared_ptr<NoteType> > _note[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> automation[MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<ARDOUR::AutomationList::iterator> _automation[MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<std::string> _automation_foreground_color[MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<std::string> automation_delay[MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<std::string> _automation_delay_foreground_color[MAX_NUMBER_OF_AUTOMATION_TRACKS];
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

	MidiTimeAxisView* midi_time_axis_view;
	boost::shared_ptr<ARDOUR::Route> route;

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
	ArdourButton                 visible_note_button;
	ArdourButton                 visible_channel_button;
	ArdourButton                 visible_velocity_button;
	ArdourButton                 visible_delay_button;
	bool                         visible_note;
	bool                         visible_channel;
	bool                         visible_velocity;
	bool                         visible_delay;

	boost::shared_ptr<ARDOUR::MidiRegion> region;
	boost::shared_ptr<ARDOUR::MidiTrack>  track;
	boost::shared_ptr<ARDOUR::MidiModel>  midi_model;

	MidiTrackerPattern* mtp;
	AutomationTrackerPattern* atp;

	/** connection used to connect to model's ContentChanged signal */
	PBD::ScopedConnection content_connection;

	void build_beats_per_row_menu ();

	void register_actions ();

	bool visible_note_press (GdkEventButton*);
	bool visible_channel_press (GdkEventButton*);
	bool visible_velocity_press (GdkEventButton*);
	bool visible_delay_press (GdkEventButton*);
	void redisplay_visible_note ();
	void redisplay_visible_channel ();
	void redisplay_visible_velocity ();
	void redisplay_visible_delay ();
	void redisplay_visible_automation ();
	void redisplay_visible_automation_delay ();
	void automation_click ();

	void setup_tooltips ();
	void setup_toolbar ();
	void setup_pattern ();
	void setup_scroller ();
	void redisplay_model ();

	bool is_midi_track () const;
	boost::shared_ptr<ARDOUR::MidiTrack> midi_track() const;

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
