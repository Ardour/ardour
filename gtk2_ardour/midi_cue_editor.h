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

#ifndef __gtk_ardour_midi_cue_editor_h__
#define __gtk_ardour_midi_cue_editor_h__

#include <gtkmm/adjustment.h>

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
}

class MidiCueView;
class CueMidiBackground;

class MidiCueEditor : public CueEditor
{
  public:
	MidiCueEditor ();
	~MidiCueEditor ();

	ArdourCanvas::Container* get_trackview_group () const { return data_group; }
	ArdourCanvas::Container* get_noscroll_group() const { return no_scroll_group; }
	Gtk::Widget& viewport();
	Gtk::Widget& toolbox ();

	double visible_canvas_width() const { return _visible_canvas_width; }
	samplecnt_t current_page_samples() const;

	void get_per_region_note_selection (std::list<std::pair<PBD::ID, std::set<std::shared_ptr<Evoral::Note<Temporal::Beats> > > > >&) const {}

	Temporal::Beats get_grid_type_as_beats (bool& success, Temporal::timepos_t const & position) const { return Temporal::Beats (1, 0); }
	Temporal::Beats get_draw_length_as_beats (bool& success, Temporal::timepos_t const & position) const { return Temporal::Beats (1, 0); }

	bool canvas_note_event (GdkEvent* event, ArdourCanvas::Item*);

	int32_t get_grid_beat_divisions (Editing::GridType gt) const { return 1; }
	int32_t get_grid_music_divisions (Editing::GridType gt, uint32_t event_state) const { return 1; }

	void set_region (std::shared_ptr<ARDOUR::MidiTrack>, std::shared_ptr<ARDOUR::MidiRegion>);

	ArdourCanvas::ScrollGroup* get_hscroll_group () const { return h_scroll_group; }
	ArdourCanvas::ScrollGroup* get_cursor_scroll_group () const { return cursor_scroll_group; }

	void set_samples_per_pixel (samplecnt_t);

	void set_mouse_mode (Editing::MouseMode, bool force = false);
	void step_mouse_mode (bool next);
	Editing::MouseMode current_mouse_mode () const;
	bool internal_editing() const;

	double timebar_height;
	size_t n_timebars;

	ArdourCanvas::GtkCanvasViewport* get_canvas_viewport() const;
	ArdourCanvas::GtkCanvas* get_canvas() const;

	int set_state (const XMLNode&, int version);
	XMLNode& get_state () const;

	void maybe_autoscroll (bool, bool, bool);
	bool autoscroll_active() const;

  protected:
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

 private:
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
	ArdourCanvas::Container* time_line_group;
	ArdourCanvas::Ruler*     bbt_ruler;
	ArdourCanvas::Rectangle* tempo_bar;
	ArdourCanvas::Rectangle* meter_bar;
	ArdourCanvas::PianoRollHeader* prh;

	ArdourCanvas::Rectangle* transport_loop_range_rect;

	Gtk::VBox     _toolbox;

	CueMidiBackground* bg;
	MidiCueView* view;

	void build_canvas ();
	void canvas_allocate (Gtk::Allocation);

	RegionSelection region_selection();

	bool canvas_enter_leave (GdkEventCrossing* ev);

	void metric_get_bbt (std::vector<ArdourCanvas::Ruler::Mark>&, samplepos_t, samplepos_t, gint);

	class BBTMetric : public ArdourCanvas::Ruler::Metric
	{
	  public:
		BBTMetric (MidiCueEditor& ec) : context (&ec) {}

		void get_marks (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, int maxchars) const {
			context->metric_get_bbt (marks, lower, upper, maxchars);
		}

	  private:
		MidiCueEditor* context;
	};

	BBTMetric bbt_metric;

	bool canvas_pre_event (GdkEvent*);
	void setup_toolbar ();

	/* autoscrolling */

	bool autoscroll_canvas ();
	void start_canvas_autoscroll (bool allow_horiz, bool allow_vert, const ArdourCanvas::Rect& boundary);
	void stop_canvas_autoscroll ();

	void visual_changer (const VisualChange&);
};


#endif /* __gtk_ardour_midi_cue_editor_h__ */
