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

class MidiStreamView : public StreamView
{
public:
	MidiStreamView (MidiTimeAxisView&);
	~MidiStreamView ();

	void get_inverted_selectables (Selection&, std::list<Selectable* >& results);
	void get_regions_with_selected_data (RegionSelection&);

	enum VisibleNoteRange {
		FullRange,
		ContentsRange
	};

	Gtk::Adjustment note_range_adjustment;
	ArdourCanvas::Container* midi_underlay_group;

	void set_note_range(VisibleNoteRange r);

	inline uint8_t lowest_note()  const { return _lowest_note; }
	inline uint8_t highest_note() const { return _highest_note; }

	void update_note_range(uint8_t note_num);

	void set_layer_display (LayerDisplay);
	//bool can_change_layer_display() const { return false; } // revert this change for now.  Although stacked view is weirdly implemented wrt the "scroomer", it is still necessary to be able to manage layered regions.
	void redisplay_track ();

	inline double contents_height() const {
		return (child_height() - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 2);
	}

	inline double note_to_y(uint8_t note) const {
		return contents_height() - (note + 1 - lowest_note()) * note_height() + 1;
	}

	uint8_t y_to_note(double y) const;

	inline double note_height() const {
		return contents_height() / (double)contents_note_range();
	}

	inline uint8_t contents_note_range() const {
		return highest_note() - lowest_note() + 1;
	}

	sigc::signal<void> NoteRangeChanged;

	RegionView* create_region_view (boost::shared_ptr<ARDOUR::Region>, bool, bool);

	bool paste (Temporal::timepos_t const & pos, const Selection& selection, PasteContext& ctx);

	void apply_note_range(uint8_t lowest, uint8_t highest, bool to_region_views);

	void suspend_updates ();
	void resume_updates ();

protected:
	void setup_rec_box ();
	void update_rec_box ();

private:

	RegionView* add_region_view_internal (
			boost::shared_ptr<ARDOUR::Region>,
			bool wait_for_waves,
			bool recording = false);

	void display_region(MidiRegionView* region_view, bool load_model);
	void display_track (boost::shared_ptr<ARDOUR::Track> tr);

	void update_contents_height ();

	void draw_note_lines();
	bool update_data_note_range(uint8_t min, uint8_t max);
	void update_contents_metrics(boost::shared_ptr<ARDOUR::Region> r);

	void color_handler ();

	void note_range_adjustment_changed();
	void apply_note_range_to_regions ();

	bool                      _range_dirty;
	double                    _range_sum_cache;
	uint8_t                   _lowest_note;   ///< currently visible
	uint8_t                   _highest_note;  ///< currently visible
	uint8_t                   _data_note_min; ///< in data
	uint8_t                   _data_note_max; ///< in data
	ArdourCanvas::LineSet*    _note_lines;
	/** true if updates to the note lines and regions are currently suspended */
	bool                      _updates_suspended;
};

#endif /* __ardour_midi_streamview_h__ */
