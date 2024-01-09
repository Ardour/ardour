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

#ifndef __gtk_ardour_midi_region_view_h__
#define __gtk_ardour_midi_region_view_h__

#include <string>
#include <vector>
#include <stdint.h>

#include "pbd/signals.h"

#include "ardour/midi_model.h"
#include "ardour/types.h"

#include "editing.h"
#include "region_view.h"
#include "midi_time_axis.h"
#include "midi_view.h"
#include "time_axis_view_item.h"
#include "automation_line.h"
#include "enums.h"

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class Filter;
};

namespace MIDI {
	namespace Name {
		struct PatchPrimaryKey;
	};
};

class SysEx;
class Note;
class Hit;
class MidiTimeAxisView;
class NoteBase;
class GhostRegion;
class AutomationTimeAxisView;
class AutomationRegionView;
class MidiCutBuffer;
class MidiListEditor;
class EditNoteDialog;
class PatchChange;
class ItemCounts;
class CursorContext;
class VelocityGhostRegion;
class EditingContext;

class MidiRegionView : public RegionView, public MidiView
{
public:
	typedef Evoral::Note<Temporal::Beats> NoteType;
	typedef Evoral::Sequence<Temporal::Beats>::Notes Notes;

	MidiRegionView (ArdourCanvas::Container*              parent,
	                EditingContext&,
	                RouteTimeAxisView&                    tv,
	                std::shared_ptr<ARDOUR::MidiRegion> r,
	                double                                samples_per_pixel,
	                uint32_t                              basic_color);

	MidiRegionView (ArdourCanvas::Container*              parent,
	                EditingContext&,
	                RouteTimeAxisView&                    tv,
	                std::shared_ptr<ARDOUR::MidiRegion> r,
	                double                                samples_per_pixel,
	                uint32_t                              basic_color,
	                bool                                  recording,
	                Visibility                            visibility);


	MidiRegionView (const MidiRegionView& other);
	MidiRegionView (const MidiRegionView& other, std::shared_ptr<ARDOUR::MidiRegion>);

	~MidiRegionView ();

	void init (bool wfd);
	bool display_is_enabled() const;

	void set_selected (bool yn);

	const std::shared_ptr<ARDOUR::MidiRegion> midi_region() const;

	inline MidiTimeAxisView* midi_view() const
	{ return dynamic_cast<MidiTimeAxisView*>(&trackview); }

	inline MidiStreamView* midi_stream_view() const
	{ return midi_view()->midi_view(); }

	void set_height (double);

	inline ARDOUR::ColorMode color_mode() const { return midi_view()->color_mode(); }

	std::string get_modifier_name() const;

	GhostRegion* add_ghost (TimeAxisView&);

	ARDOUR::InstrumentInfo& instrument_info() const;

	double height() const;
	void redisplay (bool);

  protected:
	void reset_width_dependent_items (double pixel_width);
	void parameter_changed (std::string const & p);
	uint32_t get_fill_color() const;
	void color_handler ();
	void region_resized (const PBD::PropertyChange&);
	bool canvas_group_event (GdkEvent*);

  private:

	friend class MidiRubberbandSelectDrag;
	friend class MidiVerticalSelectDrag;
	friend class NoteDrag;
	friend class NoteCreateDrag;
	friend class HitCreateDrag;
	friend class MidiGhostRegion;

	friend class EditNoteDialog;

	void clear_ghost_events() {}
	void ghosts_model_changed() {}
	void ghosts_view_changed();
	void ghost_remove_note (NoteBase*) {}
	void ghost_add_note (NoteBase*) {}
	void ghost_sync_selection (NoteBase*);

	bool motion (GdkEventMotion*);
	bool scroll (GdkEventScroll*);
	bool button_press (GdkEventButton*);
	bool button_release (GdkEventButton*);
	bool enter_notify (GdkEventCrossing*);
	bool leave_notify (GdkEventCrossing*);

	void enter_internal (uint32_t state);
	void leave_internal ();
	void mouse_mode_changed ();

	double contents_height() const { return (_height - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 2); }

	void connect_to_diskstream ();
};


#endif /* __gtk_ardour_midi_region_view_h__ */
