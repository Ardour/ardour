/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_midi_streamview_h__
#define __ardour_midi_streamview_h__

#include <list>
#include <cmath>

#include "ardour/location.h"
#include "enums.h"
#include "midi_view_background.h"
#include "streamview.h"
#include "time_axis_view_item.h"
#include "route_time_axis.h"

namespace Gdk {
	class Color;
}

namespace ARDOUR {
	class Crossfade;
	class MidiModel;
	class MidiRegion;
	class Route;
	class Source;
	struct PeakData;
}

namespace ArdourCanvas {
	class LineSet;
}

class PublicEditor;
class Selectable;
class MidiTimeAxisView;
class MidiRegionView;
class RegionSelection;
class CrossfadeView;
class Selection;

class MidiStreamView : public StreamView, public MidiViewBackground
{
public:
	MidiStreamView (MidiTimeAxisView&);
	~MidiStreamView ();

	void get_inverted_selectables (Selection&, std::list<Selectable* >& results);
	void get_regions_with_selected_data (RegionSelection&);

	void set_layer_display (LayerDisplay);
	//bool can_change_layer_display() const { return false; } // revert this change for now.  Although stacked view is weirdly implemented wrt the "scroomer", it is still necessary to be able to manage layered regions.
	void redisplay_track ();

	double contents_height() const {
		return (child_height() - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 2);
	}

	double y_position () const;

	RegionView* create_region_view (std::shared_ptr<ARDOUR::Region>, bool, bool);

	bool paste (Temporal::timepos_t const & pos, const Selection& selection, PasteContext& ctx);

	void suspend_updates ();
	void resume_updates ();

	ArdourCanvas::Container* region_canvas () const { return _region_group; }

	void parameter_changed (std::string const &);
	uint8_t get_preferred_midi_channel () const;
	void record_layer_check (std::shared_ptr<ARDOUR::Region>, samplepos_t);
	void set_note_highlight (bool);

protected:
	void setup_rec_box ();
	void update_rec_box ();
	bool updates_suspended() const { return _updates_suspended; }

	ArdourCanvas::Container* _region_group;

private:
	RegionView* add_region_view_internal (
			std::shared_ptr<ARDOUR::Region>,
			bool wait_for_waves,
			bool recording = false);

	void display_region(MidiRegionView* region_view, bool load_model);
	void display_track (std::shared_ptr<ARDOUR::Track> tr);
	void update_contents_height ();
	void update_contents_metrics (std::shared_ptr<ARDOUR::Region> r);
	void color_handler ();
	void apply_note_range_to_children ();

	/** true if updates to the note lines and regions are currently suspended */
	bool _updates_suspended;
};

#endif /* __ardour_midi_streamview_h__ */
