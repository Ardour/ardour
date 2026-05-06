/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <map>

#include "pbd/timer.h"

#include <ytkmm/adjustment.h>
#include <ytkmm/radiotoolbutton.h>

#include "canvas/ruler.h"
#include "widgets/eventboxext.h"

#include "cue_editor.h"

namespace Gtk {
	class Widget;
	class HScrollbar;
}

namespace ArdourCanvas {
	class Box;
	class Button;
	class Canvas;
	class Container;
	class GtkCanvasViewport;
	class Icon;
	class PianoRollHeader;
	class ScrollGroup;
	class Widget;
}

namespace ArdourWidgets {
	class ArdourButton;
	class MetaButton;
}

class CrossCursor;
class PianorollMidiView;
class PianorollMidiBackground;
class MidiInspector;

struct ControllerControls : public Gtk::HBox {
	ControllerControls (int num, std::string const & name, Gtk::RadioButtonGroup& group);
	~ControllerControls();

	ArdourWidgets::ArdourButton* show_hide_button;
	ArdourWidgets::ArdourButton* edit_button;
	Gtk::Label name;
	int number;

	bool showing() const;
	bool editing() const;

	sigc::signal<void> show_clicked;
	sigc::signal<void> edit_clicked;

	void set_showing (bool);
	void set_editing (bool);
};

class Pianoroll : public CueEditor
{
  public:
	enum EditingPolicy {
		AllViews,
		ActiveView,
	};


	Pianoroll (std::string const & name, bool with_transport_controls = false, bool expandable = false);
	~Pianoroll ();

	Gtk::Widget& contents ();

	samplecnt_t current_page_samples() const;

	void set_editing_policy (EditingPolicy);
	EditingPolicy editing_policy() const { return _editing_policy; }

	void get_per_region_note_selection (std::list<std::pair<PBD::ID, std::set<std::shared_ptr<Evoral::Note<Temporal::Beats> > > > >&) const {}

	Temporal::Beats get_grid_type_as_beats (bool& success, Temporal::timepos_t const & position) const;
	Temporal::Beats get_draw_length_as_beats (bool& success, Temporal::timepos_t const & position) const;

	bool canvas_note_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_velocity_base_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_velocity_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_control_point_event (GdkEvent* event, ArdourCanvas::Item*, ControlPoint*);
	bool canvas_cue_start_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_cue_end_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_bg_event (GdkEvent* event, ArdourCanvas::Item*);
	bool automation_group_event (GdkEvent* event, Evoral::Parameter);
	bool automation_close_event (GdkEvent* event, Evoral::Parameter);
	bool automation_clear_event (GdkEvent* event, Evoral::Parameter);

	int32_t get_grid_beat_divisions (Editing::GridType gt) const { return 1; }
	int32_t get_grid_music_divisions (Editing::GridType gt) const { return 1; }

	std::shared_ptr<ARDOUR::MidiTrack> midi_track() const;

	void add_region (std::shared_ptr<ARDOUR::Region>, std::shared_ptr<ARDOUR::MidiTrack>);
	void replace_region (std::shared_ptr<ARDOUR::Region>, std::shared_ptr<ARDOUR::MidiTrack>);
	void remove_regions ();
	void remove_region (std::shared_ptr<ARDOUR::Region>);
	void set_region (std::shared_ptr<ARDOUR::Region>);

	double max_extents_scale() const { return 1.2; }
	void set_samples_per_pixel (samplecnt_t);

	void set_mouse_mode (Editing::MouseMode, bool force = false);

	void midi_action (void (MidiView::*method)());

	std::list<SelectableOwner*> selectable_owners();
	void select_all_within (Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, std::list<SelectableOwner*> const &, ARDOUR::SelectionOperation, bool);

	Gdk::Cursor* which_track_cursor () const;
	Gdk::Cursor* which_mode_cursor () const;
	Gdk::Cursor* which_trim_cursor (bool left_side) const;
	Gdk::Cursor* which_canvas_cursor (ItemType type) const;

	void set_visible_channel (int chan);
	int visible_channel () const { return _visible_channel; }

	void note_mode_clicked();
	ARDOUR::NoteMode note_mode() const;
	void note_mode_chosen (ARDOUR::NoteMode);

	void set_trigger_start (Temporal::timepos_t const &);
	void set_trigger_end (Temporal::timepos_t const &);
	void set_trigger_length (Temporal::timecnt_t const &);
	void set_trigger_bounds (Temporal::timepos_t const &, Temporal::timepos_t const &);

	void delete_ ();
	void paste (float times, bool from_context_menu);
	void keyboard_paste ();
	void cut_copy (Editing::CutCopyOp);

	PianorollMidiView* midi_view() const { return _active_view; }
	void set_session (ARDOUR::Session*);
	bool allow_trim_cursors () const;

	void shift_contents (Temporal::timepos_t const &, bool model);
	void make_a_region();

	ARDOUR::InstrumentInfo* instrument_info() const;

	void set_show_source (bool);
	Temporal::timepos_t source_to_timeline (Temporal::timepos_t const & source_pos) const;

	void set_layered_automation (bool);
	void set_note_highlight (uint8_t note);

	void apply_note_range (uint8_t lowest, uint8_t highest);

	void note_entered ();
	void note_left ();

	struct AutomationLane {
		AutomationLane (Evoral::Parameter const &, Pianoroll const &, ArdourCanvas::Item*, uint32_t nth);
		~AutomationLane ();

		double height() const { return group->height(); }
		void deduce_color (uint32_t nth);

		ArdourCanvas::Rectangle* group;
		ArdourCanvas::Text* label;
		ArdourCanvas::Icon* close_x;
		ArdourCanvas::Button* clear_button;
	};

	void add_automation_lane (Evoral::Parameter const & param);
	void remove_automation_lane (Evoral::Parameter const & param);
	void clear_automation_lane (Evoral::Parameter const & param);

	bool get_midi_chord (int root_pitch, std::vector<int>& pitches) const;

	void midi_view_selection_changed ();

  protected:
	Temporal::timepos_t snap_to_grid (Temporal::timepos_t const & start,
	                                  Temporal::RoundMode   direction,
	                                  ARDOUR::SnapPref    gpref) const;

	void snap_to_internal (Temporal::timepos_t& first,
	                       Temporal::RoundMode    direction = Temporal::RoundNearest,
	                       ARDOUR::SnapPref     gpref = ARDOUR::SnapToAny_Visual,
	                       bool                 ensure_snap = false) const;

	bool button_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_handler_1 (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_handler_2 (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_dispatch (GdkEventButton*);
	bool button_release_dispatch (GdkEventButton*);
	bool motion_handler (ArdourCanvas::Item*, GdkEvent*, bool from_autoscroll = false);
	bool enter_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool leave_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool key_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool key_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);

	void escape ();
	void session_going_away ();

	void set_color_mode (ARDOUR::ColorMode);
	ARDOUR::ColorMode color_mode() const { return _color_mode; }

	void motion_track (ArdourCanvas::Duple const &);
	std::string parameter_name (Evoral::Parameter const &) const;

 private:
	ArdourCanvas::Ruler*     bbt_ruler;
	ArdourCanvas::Rectangle* tempo_bar;
	ArdourCanvas::Rectangle* meter_bar;
	ArdourCanvas::PianoRollHeader* prh;
	EditingPolicy _editing_policy;
	ARDOUR::ColorMode _color_mode;

	ArdourWidgets::ArdourButton size_button;
	ArdourWidgets::ArdourButton automation_button;
	bool expandable;
	void toggle_size();

	ArdourWidgets::ArdourDropdown region_dropdown;
	void rebuild_region_dropdown ();

	ArdourWidgets::ArdourDropdown policy_dropdown;
	ArdourWidgets::ArdourDropdown colors_dropdown;

	bool no_toggle;
	void toggle_automation (Evoral::Parameter param);
	void add_single_controller_item (Gtk::Menu_Helpers::MenuList&, int, const std::string&);
	void add_multi_controller_item (Gtk::Menu_Helpers::MenuList&, uint16_t, int, const std::string&);

	typedef std::map<Evoral::Parameter, AutomationLane*> AutomationLanes;
	AutomationLanes automation_lanes;

	PianorollMidiBackground* bg;

	typedef std::map<std::shared_ptr<ARDOUR::Region>, PianorollMidiView*> RegionMidiViewMap;
	RegionMidiViewMap region_view_map;
	void region_going_away (std::weak_ptr<ARDOUR::Region> region);

	PianorollMidiView* _active_view;

	void build_canvas ();
	void canvas_allocate (Gtk::Allocation);
	void build_lower_toolbar ();
	void build_cc_menu (ArdourWidgets::MetaButton*);

	bool canvas_enter_leave (GdkEventCrossing* ev);

	class BBTMetric : public ArdourCanvas::Ruler::Metric
	{
	  public:
		BBTMetric (Pianoroll& ec) : context (&ec) {}

		void get_marks (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, int maxchars) const {
			context->metric_get_bbt (marks, lower, upper, maxchars);
		}

	  private:
		Pianoroll* context;
	};

	BBTMetric bbt_metric;

	PBD::ScopedConnectionList view_connections;
	void maybe_update ();
	void trigger_prop_change (PBD::PropertyChange const &);

	void unset_region ();
	void unset_trigger ();

	void bindings_changed ();

	bool idle_data_captured ();

	bool user_automation_active_button_click (GdkEventButton* ev, ArdourWidgets::MetaButton* mb);
	void user_automation_show_button_click (GdkEventButton* ev, ArdourWidgets::MetaButton* metabutton);

	void automation_active_button_click (Evoral::ParameterType type, int id);
	void automation_show_button_click (Evoral::ParameterType type, int id);

	int _visible_channel;

	sigc::signal<void> NoteModeChanged;

	void point_selection_changed ();

	bool ignore_channel_changes;
	void visible_channel_changed ();

	void map_transport_state ();

	void update_tempo_based_rulers ();
	void update_rulers() { update_tempo_based_rulers (); }

	Gtk::Menu _region_context_menu;
	void popup_region_context_menu (ArdourCanvas::Item* item, GdkEvent* event);

	void set_note_selection (uint8_t note);
	void add_note_selection (uint8_t note);
	void extend_note_selection (uint8_t note);
	void toggle_note_selection (uint8_t note);

	void pack_inner (Gtk::Box&);
	void pack_outer (Gtk::Box&);

	void begin_write ();
	void end_write ();

	void manage_possible_header (Gtk::Allocation& alloc);

	void show_count_in (std::string const &);
	void hide_count_in ();

	void instant_save ();
	void parameter_changed (std::string param);
	void set_from_rsu (RegionUISettings&);

	Gtk::Menu* get_single_region_context_menu ();
	MidiViews midiviews_from_region_selection (RegionSelection const &) const;

	void setup_colors ();
	void update_pitch_colors ();

	CrossCursor* xcursor;

	void partition_height ();
	Evoral::Parameter automation_by_y (double y);

	MidiInspector* midi_inspector;
	void replace_chord (std::vector<int> intervals);
	void invert_selected_chord (bool up);
	void drop_selected_chord (std::vector<int> which_notes);

	ARDOUR::Quantize* get_quantize_op ();
	sigc::connection selection_connection;

	Gtk::Menu* build_automation_menu ();
	void automation_button_clicked ();
	void show_automation_for_all ();
	static void build_midi_controller_name_map ();
	static std::map<std::string,std::string> controller_name_map;
};
