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

#include <gtkmm/table.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/checkmenuitem.h>

#include <gtkmm2ext/selector.h>
#include <list>

#include "ardour/types.h"
#include "ardour/region.h"

#include "ardour_dialog.h"
#include "route_ui.h"
#include "enums.h"
#include "route_time_axis.h"
#include "canvas.h"
#include "midi_streamview.h"
#include "midi_channel_selector.h"

namespace ARDOUR {
	class Session;
	class RouteGroup;
	class Processor;
	class Location;
	class MidiPlaylist;
}

class PublicEditor;
class MidiStreamView;
class MidiScroomer;
class PianoRollHeader;

class MidiTimeAxisView : public RouteTimeAxisView
{
  public:
 	MidiTimeAxisView (PublicEditor&, ARDOUR::Session&, boost::shared_ptr<ARDOUR::Route>, ArdourCanvas::Canvas& canvas);
 	virtual ~MidiTimeAxisView ();

	MidiStreamView* midi_view();

	/* overridden from parent to store display state */
	guint32 show_at (double y, int& nth, Gtk::VBox *parent);
	void set_height (uint32_t);
	void hide ();

	boost::shared_ptr<ARDOUR::Region> add_region (nframes64_t pos);

	void show_all_automation ();
	void show_existing_automation ();
	void add_cc_track ();
	void add_parameter_track (const Evoral::Parameter& param);
	void create_automation_child (const Evoral::Parameter& param, bool show);

	ARDOUR::NoteMode  note_mode() const { return _note_mode; }
	ARDOUR::ColorMode color_mode() const { return _color_mode; }

	void update_range();

	sigc::signal<void, ARDOUR::ChannelMode, uint16_t>& signal_channel_mode_changed() {
		return _channel_selector.mode_changed;
	}

	sigc::signal<void, std::string, std::string>& signal_midi_patch_settings_changed() {
		return _midi_patch_settings_changed;
	}

	void start_step_editing ();
	void stop_step_editing ();
	void check_step_edit ();
	void step_edit_rest ();

  private:
	sigc::signal<void, std::string, std::string>  _midi_patch_settings_changed;

	void model_changed();
	void custom_device_mode_changed();

	void append_extra_display_menu_items ();
	void build_automation_action_menu ();
	Gtk::Menu* build_note_mode_menu();
	Gtk::Menu* build_color_mode_menu();

	void set_note_mode (ARDOUR::NoteMode mode);
	void set_color_mode(ARDOUR::ColorMode mode);
	void set_note_range(MidiStreamView::VisibleNoteRange range);

	void route_active_changed ();

	void add_insert_to_subplugin_menu (ARDOUR::Processor *);

	bool                         _ignore_signals;
	Gtk::Menu                    _subplugin_menu;
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
	MidiMultipleChannelSelector  _channel_selector;
	Gtk::ComboBoxText            _model_selector;
	Gtk::ComboBoxText            _custom_device_mode_selector;

	Gtk::CheckMenuItem*          _step_edit_item;
	Gtk::CheckMenuItem*          _midi_thru_item;
	Gtk::Menu*                    default_channel_menu;

	nframes64_t step_edit_insert_position;
	Evoral::MusicalTime step_edit_beat_pos;
	boost::shared_ptr<ARDOUR::Region> step_edit_region;
	MidiRegionView* step_edit_region_view;

	Gtk::Menu* build_def_channel_menu();
	void set_default_channel (int);
	void toggle_midi_thru ();
};

#endif /* __ardour_midi_time_axis_h__ */

