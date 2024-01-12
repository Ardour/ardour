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

#include "cue_editor.h"

namespace Gtk {
	class Widget;
}

namespace ArdourCanvas {
	class Canvas;
	class Container;
	class GtkCanvasViewport;
	class ScrollGroup;
}

class MidiView;
class CueMidiBackground;

class MidiCueEditor : public CueEditor
{
  public:
	MidiCueEditor ();
	~MidiCueEditor ();

	ArdourCanvas::Container* get_noscroll_group() const { return no_scroll_group; }
	Gtk::Widget& viewport();

	double visible_canvas_width() const { return _visible_canvas_width; }
	samplecnt_t current_page_samples() const;

	void get_per_region_note_selection (std::list<std::pair<PBD::ID, std::set<std::shared_ptr<Evoral::Note<Temporal::Beats> > > > >&) const {}

	Temporal::Beats get_grid_type_as_beats (bool& success, Temporal::timepos_t const & position) const { return Temporal::Beats (1, 0); }
	Temporal::Beats get_draw_length_as_beats (bool& success, Temporal::timepos_t const & position) const { return Temporal::Beats (1, 0); }

	bool canvas_note_event (GdkEvent* event, ArdourCanvas::Item*);

	int32_t get_grid_beat_divisions (Editing::GridType gt) const { return 1; }
	int32_t get_grid_music_divisions (Editing::GridType gt, uint32_t event_state) const { return 1; }

	void apply_midi_note_edit_op (ARDOUR::MidiOperator& op, const RegionSelection& rs);
	PBD::Command* apply_midi_note_edit_op_to_region (ARDOUR::MidiOperator& op, MidiRegionView& mrv);

	void set_region (std::shared_ptr<ARDOUR::MidiTrack>, std::shared_ptr<ARDOUR::MidiRegion>);

  protected:
	Temporal::timepos_t snap_to_grid (Temporal::timepos_t const & start,
	                                  Temporal::RoundMode   direction,
	                                  ARDOUR::SnapPref    gpref) const;

	void snap_to_internal (Temporal::timepos_t& first,
	                       Temporal::RoundMode    direction = Temporal::RoundNearest,
	                       ARDOUR::SnapPref     gpref = ARDOUR::SnapToAny_Visual,
	                       bool                 ensure_snap = false) const;

 private:
	Gtk::Adjustment vertical_adjustment;
	Gtk::Adjustment horizontal_adjustment;
	ArdourCanvas::GtkCanvasViewport* _canvas_viewport;
	ArdourCanvas::Canvas* _canvas;

	ArdourCanvas::Container* tempo_group;

	/* The group containing all other groups that are scrolled vertically
	   and horizontally.
	*/
	ArdourCanvas::ScrollGroup* hv_scroll_group;

	/* The group containing all other groups that are scrolled horizontally ONLY
	*/
	ArdourCanvas::ScrollGroup* h_scroll_group;

	/* Scroll group for cursors, scrolled horizontally, above everything else
	*/
	ArdourCanvas::ScrollGroup* cursor_scroll_group;

	/* The group containing all trackviews. */
	ArdourCanvas::Container* no_scroll_group;

	/* The group containing all trackviews. */
	ArdourCanvas::Container* _trackview_group;
	ArdourCanvas::Container* global_rect_group;
	ArdourCanvas::Container* time_line_group;

	ArdourCanvas::Rectangle* transport_loop_range_rect;

	CueMidiBackground* bg;
	MidiView* view;

	void build_canvas ();
	void canvas_allocate (Gtk::Allocation);
};


#endif /* __gtk_ardour_midi_cue_editor_h__ */
