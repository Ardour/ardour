/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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

#include <ytkmm/box.h>
#include <ytkmm/label.h>
#include <ytkmm/table.h>

#include "pbd/history_owner.h"

#include "ardour/ardour.h"
#include "ardour/session_handle.h"
#include "ardour/triggerbox.h"
#include "ardour/types.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/cairo_packer.h"

#include "widgets/ardour_button.h"

#include "canvas/canvas.h"
#include "canvas/container.h"
#include "canvas/line.h"
#include "canvas/rectangle.h"
#include "canvas/ruler.h"
#include "canvas/scroll_group.h"

#include "audio_clock.h"
#include "cue_editor.h"

namespace ARDOUR
{
	class Session;
	class Location;
	class Trigger;
}

namespace ArdourCanvas
{
	class Text;
	class Polygon;
}

namespace ArdourWaveView
{
	class WaveView;
}

class AudioClipEditor :  public CueEditor
{
public:
	AudioClipEditor (std::string const &, bool with_transport);
	~AudioClipEditor ();

	void canvas_allocate (Gtk::Allocation&);

	Gtk::Widget& viewport();
	Gtk::Widget& contents ();

	void set_trigger (ARDOUR::TriggerReference&);
	void set_region (std::shared_ptr<ARDOUR::AudioRegion>);
	void set_region (std::shared_ptr<ARDOUR::Region> r);
	void region_changed (const PBD::PropertyChange& what_changed);

	double      sample_to_pixel (ARDOUR::samplepos_t);
	samplepos_t pixel_to_sample (double);

	bool key_press (GdkEventKey*);
	bool minsec_ruler_event (GdkEvent*);

	/* EditingContext API. As of July 2025, we do not implement most of
	 * these
	 */

	bool button_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) { return true; }
	bool button_press_handler_1 (ArdourCanvas::Item*, GdkEvent*, ItemType) { return true; }
	bool button_press_handler_2 (ArdourCanvas::Item*, GdkEvent*, ItemType) { return true; }
	bool button_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) { return true; }
	bool button_press_dispatch (GdkEventButton*) { return true; }
	bool button_release_dispatch (GdkEventButton*) { return true; }
	bool motion_handler (ArdourCanvas::Item*, GdkEvent*, bool from_autoscroll = false) { return true; }
	bool enter_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) { return true; }
	bool leave_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) { return true; }
	bool key_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) { return true; }
	bool key_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) { return true; }

	bool canvas_note_event (GdkEvent* event, ArdourCanvas::Item*) { return true; }
	bool canvas_velocity_base_event (GdkEvent* event, ArdourCanvas::Item*) { return true; }
	bool canvas_velocity_event (GdkEvent* event, ArdourCanvas::Item*) { return true; }
	bool canvas_control_point_event (GdkEvent* event, ArdourCanvas::Item*, ControlPoint*) { return true; }
	bool canvas_bg_event (GdkEvent* event, ArdourCanvas::Item*) { return true; }

	ArdourCanvas::Container* get_trackview_group () const;
	ArdourCanvas::Container* get_noscroll_group() const;
	ArdourCanvas::ScrollGroup* get_hscroll_group () const;
	ArdourCanvas::ScrollGroup* get_cursor_scroll_group () const;

	samplecnt_t current_page_samples() const;
	double visible_canvas_width() const;
	void set_samples_per_pixel (samplecnt_t);

	ArdourCanvas::GtkCanvasViewport* get_canvas_viewport() const { return const_cast<ArdourCanvas::GtkCanvasViewport*> (&_viewport); }
	ArdourCanvas::GtkCanvas* get_canvas() const { return &canvas; }

	std::pair<Temporal::timepos_t,Temporal::timepos_t> max_zoom_extent() const;

	Gdk::Cursor* which_track_cursor () const { return nullptr; }
	Gdk::Cursor* which_mode_cursor () const { return nullptr; }
	Gdk::Cursor* which_trim_cursor (bool left_side) const { return nullptr; }
	Gdk::Cursor* which_canvas_cursor (ItemType type) const { return nullptr; }

	RegionSelection region_selection();

	Temporal::timepos_t snap_to_grid (Temporal::timepos_t const & start, Temporal::RoundMode direction, ARDOUR::SnapPref gpref) const { return start; }
	void snap_to_internal (Temporal::timepos_t& first, Temporal::RoundMode direction = Temporal::RoundNearest, ARDOUR::SnapPref gpref = ARDOUR::SnapToAny_Visual, bool ensure_snap = false) const {}

	void select_all_within (Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, std::list<SelectableOwner*> const &, ARDOUR::SelectionOperation, bool) {}
	void get_per_region_note_selection (std::list<std::pair<PBD::ID, std::set<std::shared_ptr<Evoral::Note<Temporal::Beats> > > > >&) const {}
	void get_regionviews_by_id (PBD::ID const id, RegionSelection & regions) const {}
	void maybe_autoscroll (bool, bool, bool from_headers) {};
	void stop_canvas_autoscroll () {}
	void redisplay_grid (bool immediate_redraw) {};
	void instant_save() {};

	void point_selection_changed () {}
	void step_mouse_mode (bool next);
	void mouse_mode_toggled (Editing::MouseMode);
	void delete_ () {}
	void paste (float times, bool from_context_menu) {}
	void keyboard_paste () {}
	void cut_copy (Editing::CutCopyOp) {}

	void register_actions() {}
	void visual_changer (const VisualChange&) {}
	void build_zoom_focus_menu () {}

private:
	ArdourCanvas::GtkCanvasViewport _viewport;
	ArdourCanvas::GtkCanvas&         canvas;

	ArdourCanvas::Container*         line_container;
	ArdourCanvas::Line*              start_line;
	ArdourCanvas::Line*              end_line;
	ArdourCanvas::Line*              loop_line;
	ArdourCanvas::Container*         ruler_container;
	ArdourCanvas::Ruler*             minsec_ruler;

	class ClipBBTMetric : public ArdourCanvas::Ruler::Metric
	{
	  public:
		ClipBBTMetric (ARDOUR::TriggerReference tr) : tref (tr) {
			units_per_pixel = 1;
		}

		void get_marks (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, int maxchars) const;

	  private:
		ARDOUR::TriggerReference tref;

	};

	ClipBBTMetric*                         clip_metric;
	std::vector<ArdourWaveView::WaveView*> waves;
	double                                 non_wave_height;
	samplepos_t                            left_origin;
	double                                 scroll_fraction;
	std::shared_ptr<ARDOUR::AudioRegion> audio_region;

	void scroll_left ();
	void scrol_right ();

	enum LineType {
		StartLine,
		EndLine,
		LoopLine,
	};

	bool event_handler (GdkEvent* ev);
	bool line_event_handler (GdkEvent* ev, ArdourCanvas::Line*);
	void drop_waves ();
	void set_wave_heights ();
	void set_spp_from_length (ARDOUR::samplecnt_t);
	void set_waveform_colors ();
	void set_colors ();
	void position_lines ();
	void scroll_changed ();

	class LineDrag
	{
	public:
		LineDrag (AudioClipEditor&, ArdourCanvas::Line&);

		void begin (GdkEventButton*);
		void end (GdkEventButton*);
		void motion (GdkEventMotion*);

	private:
		AudioClipEditor&    editor;
		ArdourCanvas::Line& line;
	};

	friend class LineDrag;
	LineDrag* current_line_drag;

	PBD::ScopedConnection state_connection;

	void build_canvas ();
	void build_lower_toolbar ();
	void pack_inner (Gtk::Box&);
	void pack_outer (Gtk::Box&);

	bool canvas_enter_leave (GdkEventCrossing* ev);
};
