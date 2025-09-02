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

class StartBoundaryRect;
class EndBoundaryRect;

class AudioClipEditor :  public CueEditor
{
public:
	AudioClipEditor (std::string const &, bool with_transport = false);
	~AudioClipEditor ();

	void canvas_allocate (Gtk::Allocation&);

	Gtk::Widget& contents ();

	void set_trigger (ARDOUR::TriggerReference&);
	void set_region (std::shared_ptr<ARDOUR::Region> r);
	void region_changed (const PBD::PropertyChange& what_changed);

	bool key_press (GdkEventKey*);

	/* EditingContext API. As of July 2025, we do not implement most of
	 * these
	 */

	bool button_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_handler_1 (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_handler_2 (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_dispatch (GdkEventButton*) { return true; }
	bool button_release_dispatch (GdkEventButton*) { return true; }

	bool motion_handler (ArdourCanvas::Item*, GdkEvent*, bool from_autoscroll = false);

	bool enter_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool leave_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool key_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) { return true; }
	bool key_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) { return true; }

	bool canvas_note_event (GdkEvent* event, ArdourCanvas::Item*) { return true; }
	bool canvas_velocity_base_event (GdkEvent* event, ArdourCanvas::Item*) { return true; }
	bool canvas_velocity_event (GdkEvent* event, ArdourCanvas::Item*) { return true; }
	bool canvas_control_point_event (GdkEvent* event, ArdourCanvas::Item*, ControlPoint*) { return true; }
	bool canvas_bg_event (GdkEvent* event, ArdourCanvas::Item*) { return true; }

	samplecnt_t current_page_samples() const;
	void set_samples_per_pixel (samplecnt_t);

	Gdk::Cursor* which_track_cursor () const { return nullptr; }
	Gdk::Cursor* which_mode_cursor () const { return nullptr; }
	Gdk::Cursor* which_trim_cursor (bool left_side) const { return nullptr; }
	Gdk::Cursor* which_canvas_cursor (ItemType type) const;

	Temporal::timepos_t snap_to_grid (Temporal::timepos_t const & start, Temporal::RoundMode direction, ARDOUR::SnapPref gpref) const { return start; }
	void snap_to_internal (Temporal::timepos_t& first, Temporal::RoundMode direction = Temporal::RoundNearest, ARDOUR::SnapPref gpref = ARDOUR::SnapToAny_Visual, bool ensure_snap = false) const {}

	void select_all_within (Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, std::list<SelectableOwner*> const &, ARDOUR::SelectionOperation, bool) {}
	void get_per_region_note_selection (std::list<std::pair<PBD::ID, std::set<std::shared_ptr<Evoral::Note<Temporal::Beats> > > > >&) const {}
	void get_regionviews_by_id (PBD::ID const id, RegionSelection & regions) const {}

	void point_selection_changed () {}
	void delete_ () {}
	void paste (float times, bool from_context_menu) {}
	void keyboard_paste () {}
	void cut_copy (Editing::CutCopyOp) {}

	void maybe_update ();

	bool idle_data_captured () { return false; }

 private:
	ArdourCanvas::Container*         line_container;
	StartBoundaryRect*               start_line;
	EndBoundaryRect*                 end_line;
	ArdourCanvas::Line*              loop_line;
	ArdourCanvas::Container*         ruler_container;
	ArdourCanvas::Ruler*             main_ruler;

	class ClipMetric : public ArdourCanvas::Ruler::Metric
	{
	  public:
		ClipMetric (AudioClipEditor & ac) : ace (ac) {
			units_per_pixel = 1;
		}

		void get_marks (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, int maxchars) const;

	  private:
		AudioClipEditor & ace;

	};

	ClipMetric*                            clip_metric;
	std::vector<ArdourWaveView::WaveView*> waves;
	double                                 non_wave_height;
	samplepos_t                            left_origin;
	double                                 scroll_fraction;

	void scroll_left ();
	void scrol_right ();

	bool event_handler (GdkEvent* ev);
	bool start_line_event_handler (GdkEvent* ev, StartBoundaryRect*);
	bool end_line_event_handler (GdkEvent* ev, EndBoundaryRect*);
	void drop_waves ();
	void set_wave_heights ();
	void set_spp_from_length (ARDOUR::samplecnt_t);
	void set_waveform_colors ();
	void set_colors ();
	void position_lines ();
	void scroll_changed ();

	PBD::ScopedConnection state_connection;

	void build_canvas ();
	void build_lower_toolbar ();
	void pack_inner (Gtk::Box&);
	void pack_outer (Gtk::Box&);

	bool canvas_enter_leave (GdkEventCrossing* ev);

	void begin_write ();
	void end_write ();

	void show_count_in (std::string const &);
	void hide_count_in ();

	void unset (bool trigger_too);
	void load_shared_bindings ();

	void compute_fixed_ruler_scale ();
	void update_fixed_rulers ();

	void update_rulers () { update_fixed_rulers(); }
	void set_action_defaults ();
};
