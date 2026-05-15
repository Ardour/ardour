/*
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2012 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include "ardour/types.h"

#include "midi_view.h"
#include "pianoroll.h"

class VelocityDisplay;
class PianorollAutomationLine;

namespace ArdourCanvas {
	class Box;
}

class PianorollMidiView : public MidiView
{
  public:
	PianorollMidiView (std::shared_ptr<ARDOUR::MidiTrack> mt,
	             ArdourCanvas::Item&         parent,
	             ArdourCanvas::Item&         noscroll_parent,
	             EditingContext&             ec,
	             MidiViewBackground&         bg
		);

	~PianorollMidiView();

	void set_region (std::shared_ptr<ARDOUR::MidiRegion>);
	void set_samples_per_pixel (double);
	void set_height (double);

	void clear_events ();
	void clear_ghost_events();
	void ghosts_model_changed();
	void ghosts_view_changed();
	void ghost_remove_note (NoteBase*);
	void ghost_add_note (NoteBase*);
	void ghost_sync_selection (NoteBase*);

	ArdourCanvas::Item* drag_group() const;

	std::list<SelectableOwner*> selectable_owners();
	MergeableLine* make_merger ();

	bool automation_rb_click (GdkEvent*, Temporal::timepos_t const &, Evoral::Parameter);
	bool velocity_rb_click (GdkEvent*, Temporal::timepos_t const &);
	void line_drag_click (GdkEvent*, Temporal::timepos_t const &);

	void point_selection_changed ();
	void clear_selection ();

	sigc::signal<void> AutomationStateChange;

	void set_overlay_text (std::string const &);
	void hide_overlay_text ();
	void show_overlay_text ();

	void cut_copy_clear (::Selection& selection, Editing::CutCopyOp);

	void add_automation_lane (Evoral::Parameter const &, Pianoroll::AutomationLane& lane_parent);
	void remove_automation_lane (Evoral::Parameter const &, Pianoroll::AutomationLane& lane_parent);
	void set_active_automation (Evoral::Parameter const &);
	void remove_all_automation ();
	void swap_automation_channel (int);
	void clear_automation_lane (Evoral::Parameter const &);

	void partition_height ();

	void get_selectables (Evoral::Parameter const & param, Temporal::timepos_t const & start, Temporal::timepos_t  const & end, double x, double y, std::list<Selectable*>& sl, bool within = false);
	void set_sensitive (bool yn);

	XMLNode* automation_state () const;
	void set_automation_state (XMLNode const &);

	void color_handler ();

  protected:
	bool scroll (GdkEventScroll* ev);

	ArdourCanvas::Item* _noscroll_parent;
	ArdourCanvas::Text* overlay_text;

	typedef std::shared_ptr<PianorollAutomationLine>  CueAutomationLine;
	typedef std::shared_ptr<ARDOUR::AutomationControl>  CueAutomationControl;

	struct AutomationLane {

		AutomationLane (CueAutomationControl ctl, CueAutomationLine ln, bool vis, Pianoroll::AutomationLane& par)
			: control (ctl), line (ln), velocity_display (nullptr), parent (par) {}
		AutomationLane (VelocityDisplay& vdisp, bool vis, Pianoroll::AutomationLane& par)
			: control (nullptr), line (nullptr), velocity_display (&vdisp), parent (par) {}

		~AutomationLane();

		CueAutomationControl control;
		CueAutomationLine line;
		VelocityDisplay* velocity_display;
		Pianoroll::AutomationLane& parent;

		void set_sensitive (bool);
		void set_height (double);
	};

	typedef std::map<Evoral::Parameter, AutomationLane*> CueAutomationMap;
	CueAutomationMap automation_map;

	AutomationLane* automation_lane_by_param (Evoral::Parameter const &);
	Evoral::Parameter active_automation_parameter;

	std::shared_ptr<Temporal::TempoMap const> tempo_map;
	ArdourCanvas::Rectangle* event_rect;

	void update_sustained (Note *);
	void update_hit (Hit *);

	double _height;

	bool midi_canvas_group_event (GdkEvent*);
	bool automation_group_event (GdkEvent*);
	Gtkmm2ext::Color line_color_for (Evoral::Parameter const &);

	void reset_width_dependent_items (double pixel_width);

	void cut_copy_clear_one (AutomationLine& line, ::Selection& selection, Editing::CutCopyOp op);
	void cut_copy_points (Editing::CutCopyOp op, Temporal::timepos_t const & earliest_time);

	sigc::connection er_connection;
	sigc::connection parent_connection;
};
