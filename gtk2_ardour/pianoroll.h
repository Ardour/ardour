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

#include <ytkmm/adjustment.h>

#include "canvas/ruler.h"

#include "cue_editor.h"

namespace Gtk {
	class Widget;
}

namespace ArdourCanvas {
	class Box;
	class Canvas;
	class Container;
	class GtkCanvasViewport;
	class PianoRollHeader;
	class ScrollGroup;
	class Widget;
}

namespace ArdourWidgets {
	class ArdourButton;
	class MetaButton;
}

class PianorollMidiView;
class CueMidiBackground;

class Pianoroll : public CueEditor
{
  public:
	Pianoroll (std::string const & name);
	~Pianoroll ();

	ArdourCanvas::Container* get_trackview_group () const { return data_group; }
	ArdourCanvas::Container* get_noscroll_group() const { return no_scroll_group; }
	Gtk::Widget& viewport();
	Gtk::Widget& contents ();

	double visible_canvas_width() const { return _visible_canvas_width; }
	samplecnt_t current_page_samples() const;

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

	int32_t get_grid_beat_divisions (Editing::GridType gt) const { return 1; }
	int32_t get_grid_music_divisions (Editing::GridType gt, uint32_t event_state) const { return 1; }

	void set (ARDOUR::TriggerReference&);
	void set_region (std::shared_ptr<ARDOUR::MidiRegion>);
	void set_track (std::shared_ptr<ARDOUR::MidiTrack>);

	ArdourCanvas::ScrollGroup* get_hscroll_group () const { return h_scroll_group; }
	ArdourCanvas::ScrollGroup* get_cursor_scroll_group () const { return cursor_scroll_group; }

	double max_extents_scale() const { return 1.2; }
	void set_samples_per_pixel (samplecnt_t);

	void set_mouse_mode (Editing::MouseMode, bool force = false);
	void step_mouse_mode (bool next);
	Editing::MouseMode current_mouse_mode () const;
	bool internal_editing() const;

	void box_rec_enable_change (ARDOUR::TriggerBox const &);
	void trigger_rec_enable_change (ARDOUR::Trigger const &);

	double timebar_height;
	size_t n_timebars;

	ArdourCanvas::GtkCanvasViewport* get_canvas_viewport() const;
	ArdourCanvas::GtkCanvas* get_canvas() const;

	int set_state (const XMLNode&, int version);
	XMLNode& get_state () const;

	void maybe_autoscroll (bool, bool, bool);
	bool autoscroll_active() const;

	void midi_action (void (MidiView::*method)());

	std::list<SelectableOwner*> selectable_owners();
	void select_all_within (Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, std::list<SelectableOwner*> const &, ARDOUR::SelectionOperation, bool);

	Gdk::Cursor* which_track_cursor () const;
	Gdk::Cursor* which_mode_cursor () const;
	Gdk::Cursor* which_trim_cursor (bool left_side) const;
	Gdk::Cursor* which_canvas_cursor (ItemType type) const;

	void set_visible_channel (int chan);
	void note_mode_clicked();
	ARDOUR::NoteMode note_mode() const { return _note_mode; }
	void set_note_mode (ARDOUR::NoteMode);

	void set_trigger_start (Temporal::timepos_t const &);
	void set_trigger_end (Temporal::timepos_t const &);
	void set_trigger_length (Temporal::timecnt_t const &);
	void set_trigger_bounds (Temporal::timepos_t const &, Temporal::timepos_t const &);

	void full_zoom_clicked();
	void zoom_to_show (Temporal::timecnt_t const &);

	void delete_ ();
	void paste (float times, bool from_context_menu);
	void keyboard_paste ();
	void cut_copy (Editing::CutCopyOp);

  protected:
	void load_bindings ();
	void register_actions ();

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

	void mouse_mode_toggled (Editing::MouseMode);

	void escape ();
	void on_samples_per_pixel_changed ();

 private:
	ARDOUR::TriggerReference ref;
	std::shared_ptr<ARDOUR::MidiTrack> _track;
	ArdourCanvas::GtkCanvasViewport* _canvas_viewport;
	ArdourCanvas::GtkCanvas* _canvas;

	/* The group containing all other groups that are scrolled vertically
	   and horizontally.
	*/
	ArdourCanvas::ScrollGroup* hv_scroll_group;

	/* The group containing all other groups that are scrolled horizontally ONLY
	*/
	ArdourCanvas::ScrollGroup* h_scroll_group;
	ArdourCanvas::ScrollGroup* v_scroll_group;

	/* Scroll group for cursors, scrolled horizontally, above everything else
	*/
	ArdourCanvas::ScrollGroup* cursor_scroll_group;

	ArdourCanvas::Container* global_rect_group;
	ArdourCanvas::Container* no_scroll_group;
	ArdourCanvas::Container* data_group;
	ArdourCanvas::Ruler*     bbt_ruler;
	ArdourCanvas::Rectangle* tempo_bar;
	ArdourCanvas::Rectangle* meter_bar;
	ArdourCanvas::PianoRollHeader* prh;

	ArdourCanvas::Rectangle* transport_loop_range_rect;

	Gtk::EventBox _contents;
	Gtk::VBox     _toolbox;

	Gtk::HBox                    button_bar;
	ArdourWidgets::ArdourButton* velocity_button;
	ArdourWidgets::ArdourButton* bender_button;
	ArdourWidgets::ArdourButton* pressure_button;
	ArdourWidgets::ArdourButton* expression_button;
	ArdourWidgets::ArdourButton* modulation_button;
	ArdourWidgets::MetaButton* cc_dropdown1;
	ArdourWidgets::MetaButton* cc_dropdown2;
	ArdourWidgets::MetaButton* cc_dropdown3;

	typedef std::map<ArdourWidgets::ArdourButton*,Evoral::Parameter> ParameterButtonMap;
	ParameterButtonMap parameter_button_map;
	void rebuild_parameter_button_map ();

	CueMidiBackground* bg;
	PianorollMidiView* view;

	void build_canvas ();
	void canvas_allocate (Gtk::Allocation);
	void build_upper_toolbar ();
	void build_lower_toolbar ();

	RegionSelection region_selection();

	bool canvas_enter_leave (GdkEventCrossing* ev);

	void metric_get_bbt (std::vector<ArdourCanvas::Ruler::Mark>&, samplepos_t, samplepos_t, gint);

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

	bool canvas_pre_event (GdkEvent*);

	/* autoscrolling */

	bool autoscroll_canvas ();
	void start_canvas_autoscroll (bool allow_horiz, bool allow_vert, const ArdourCanvas::Rect& boundary);
	void stop_canvas_autoscroll ();

	sigc::connection _update_connection;
	PBD::ScopedConnectionList object_connections;
	void maybe_update ();
	void trigger_prop_change (PBD::PropertyChange const &);

	void unset ();

	void visual_changer (const VisualChange&);
	void bindings_changed ();

	void data_captured (samplecnt_t);
	bool idle_data_captured ();
	std::atomic<int> idle_update_queued;
	PBD::ScopedConnectionList capture_connections;
	samplecnt_t data_capture_duration;

	bool user_automation_button_event (GdkEventButton* ev, ArdourWidgets::MetaButton* mb);
	bool automation_button_event (GdkEventButton*, Evoral::ParameterType type, int id);
	bool automation_button_click (Evoral::ParameterType type, int id, ARDOUR::SelectionOperation);
	void automation_led_click (GdkEventButton*, Evoral::ParameterType type, int id);
	void user_led_click (GdkEventButton* ev, ArdourWidgets::MetaButton* metabutton);

	int _visible_channel;

	ARDOUR::NoteMode _note_mode;
	sigc::signal<void> NoteModeChanged;

	void automation_state_changed ();

	void build_zoom_focus_menu ();

	std::pair<Temporal::timepos_t,Temporal::timepos_t> max_zoom_extent() const;

	void point_selection_changed ();
	bool enter (GdkEventCrossing*);

	bool zoom_in_allocate;

	ArdourWidgets::ArdourButton rec_enable_button;
	void rec_enable_clicked ();
	Gtk::Adjustment bar_adjustment;
	Gtk::SpinButton bar_spinner;
	Gtk::Label length_label;
	Gtk::HBox   rec_box;

	bool rec_button_press (GdkEventButton*);
	void rec_enable_change ();
	void blink_rec_enable (bool);
	sigc::connection rec_blink_connection;

	void add_single_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items, int ctl, const std::string& name, ArdourWidgets::MetaButton*);
	void add_multi_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items, uint16_t channels, int ctl, const std::string& name, ArdourWidgets::MetaButton*);
	void reset_user_cc_choice (std::string, Evoral::Parameter param, ArdourWidgets::MetaButton*);

};
