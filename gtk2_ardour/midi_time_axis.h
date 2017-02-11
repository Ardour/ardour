/*
    Copyright (C) 2006 Paul Davis

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

#ifndef __ardour_midi_time_axis_h__
#define __ardour_midi_time_axis_h__

#include <list>

#include <gtkmm/table.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/checkmenuitem.h>

#include "gtkmm2ext/selector.h"

#include "ardour/types.h"
#include "ardour/region.h"

#include "ardour_dialog.h"
#include "ardour_dropdown.h"
#include "route_ui.h"
#include "enums.h"
#include "route_time_axis.h"
#include "midi_streamview.h"

namespace MIDI {
namespace Name {
class MasterDeviceNames;
class CustomDeviceMode;
struct PatchPrimaryKey;
}
}

namespace ARDOUR {
	class Session;
	class RouteGroup;
	class Processor;
	class Location;
	class MidiPlaylist;
}

namespace Evoral {
	template<typename Time> class Note;
}

class PublicEditor;
class MidiStreamView;
class MidiScroomer;
class PianoRollHeader;
class StepEntry;
class StepEditor;
class MidiChannelSelectorWindow;

#define NO_MIDI_NOTE 0xff

class MidiTimeAxisView : public RouteTimeAxisView
{
public:
	MidiTimeAxisView (PublicEditor&, ARDOUR::Session*, ArdourCanvas::Canvas& canvas);
	virtual ~MidiTimeAxisView ();

	void set_route (boost::shared_ptr<ARDOUR::Route>);

	MidiStreamView* midi_view();

	void set_height (uint32_t, TrackHeightMode m = OnlySelf);

	boost::shared_ptr<ARDOUR::MidiRegion> add_region (ARDOUR::framepos_t, ARDOUR::framecnt_t, bool);

	void show_all_automation (bool apply_to_selection = false);
	void show_existing_automation (bool apply_to_selection = false);
	void create_automation_child (const Evoral::Parameter& param, bool show);

	bool paste (ARDOUR::framepos_t, const Selection&, PasteContext& ctx, const int32_t sub_num);

	ARDOUR::NoteMode  note_mode() const { return _note_mode; }
	ARDOUR::ColorMode color_mode() const { return _color_mode; }

	boost::shared_ptr<MIDI::Name::MasterDeviceNames> get_device_names();
	boost::shared_ptr<MIDI::Name::CustomDeviceMode> get_device_mode();

	void update_range();

	Gtk::CheckMenuItem* automation_child_menu_item (Evoral::Parameter);

	StepEditor* step_editor() { return _step_editor; }
	void check_step_edit ();

	void first_idle ();
	void set_note_highlight (uint8_t note);

	uint8_t get_channel_for_add () const;

	void get_per_region_note_selection (std::list<std::pair<PBD::ID, std::set<boost::shared_ptr<Evoral::Note<Evoral::Beats> > > > >&);

protected:
	void start_step_editing ();
	void stop_step_editing ();
	void processors_changed (ARDOUR::RouteProcessorChange);

private:
	sigc::signal<void, std::string, std::string>  _midi_patch_settings_changed;

	void setup_midnam_patches ();
	void update_patch_selector ();
	void drop_instrument_ref ();
	PBD::ScopedConnectionList midnam_connection;

	void start_scroomer_update ();
	void stop_scroomer_update ();
	sigc::connection _note_range_changed_connection;

	void model_changed(const std::string& model);
	void custom_device_mode_changed(const std::string& mode);

	void append_extra_display_menu_items ();
	void build_automation_action_menu (bool);
	Gtk::Menu* build_note_mode_menu();
	Gtk::Menu* build_color_mode_menu();
	Gtk::Menu* build_patch_menu();

	void set_note_mode (ARDOUR::NoteMode mode, bool apply_to_selection = false);
	void set_color_mode (ARDOUR::ColorMode, bool force = false, bool redisplay = true, bool apply_to_selection = false);
	void set_note_range (MidiStreamView::VisibleNoteRange range, bool apply_to_selection = false);
	void on_patch_menu_selected (int chn, const MIDI::Name::PatchPrimaryKey& key);

	void route_active_changed ();
	void note_range_changed ();
	void contents_height_changed ();

	void update_control_names ();

	bool                         _ignore_signals;
	MidiScroomer*                _range_scroomer;
	PianoRollHeader*             _piano_roll_header;
	ARDOUR::NoteMode             _note_mode;
	Gtk::RadioMenuItem*          _note_mode_item;
	Gtk::RadioMenuItem*          _percussion_mode_item;
	ARDOUR::ColorMode            _color_mode;
	Gtk::RadioMenuItem*          _meter_color_mode_item;
	Gtk::RadioMenuItem*          _channel_color_mode_item;
	Gtk::RadioMenuItem*          _track_color_mode_item;
	Gtk::VBox                    _midi_controls_box;
	MidiChannelSelectorWindow*   _channel_selector;
	ArdourDropdown               _midnam_model_selector;
	ArdourDropdown               _midnam_custom_device_mode_selector;

	Gtk::CheckMenuItem*          _step_edit_item;
	Gtk::Menu*                    default_channel_menu;

	void change_all_channel_tracks_visibility (bool yn, Evoral::Parameter param);
	void add_basic_parameter_menu_item (Gtk::Menu_Helpers::MenuList& items, const std::string& label, Evoral::Parameter param);
	void add_channel_command_menu_item (Gtk::Menu_Helpers::MenuList& items, const std::string& label, ARDOUR::AutomationType auto_type, uint8_t cmd);

	Gtk::Menu* controller_menu;
	Gtk::Menu* poly_pressure_menu;

	void add_single_channel_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items, int ctl, const std::string& name);
	void add_multi_channel_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items, int ctl, const std::string& name);
	void build_controller_menu ();
	void toggle_channel_selector ();
	void channel_selector_hidden ();
	void set_channel_mode (ARDOUR::ChannelMode, uint16_t);

	void set_note_selection (uint8_t note);
	void add_note_selection (uint8_t note);
	void extend_note_selection (uint8_t note);
	void toggle_note_selection (uint8_t note);
	void set_note_selection_region_view (RegionView* rv, uint8_t note, uint16_t chn_mask);
	void add_note_selection_region_view (RegionView* rv, uint8_t note, uint16_t chn_mask);
	void extend_note_selection_region_view (RegionView*, uint8_t note, uint16_t chn_mask);
	void toggle_note_selection_region_view (RegionView*, uint8_t note, uint16_t chn_mask);
	void get_per_region_note_selection_region_view (RegionView*, std::list<std::pair<PBD::ID, std::set<boost::shared_ptr<Evoral::Note<Evoral::Beats> > > > >&);

	void ensure_step_editor ();

	/** parameter -> menu item map for the channel command items */
	ParameterMenuMap _channel_command_menu_map;
	/** parameter -> menu item map for the controller menu */
	ParameterMenuMap _controller_menu_map;

	StepEditor* _step_editor;
};

#endif /* __ardour_midi_time_axis_h__ */
