/*
    Copyright (C) 2001, 2006 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_midi_streamview_h__
#define __ardour_midi_streamview_h__

#include <list>
#include <cmath>

#include <ardour/location.h>
#include "enums.h"
#include "streamview.h"
#include "time_axis_view_item.h"
#include "route_time_axis.h"
#include "canvas.h"

namespace Gdk {
	class Color;
}

namespace ARDOUR {
	class Route;
	class Diskstream;
	class Crossfade;
	class PeakData;
	class MidiRegion;
	class Source;
	class MidiModel;
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

	void set_selected_regionviews (RegionSelection&);
	void get_selectables (jack_nframes_t start, jack_nframes_t end, list<Selectable* >&);
	void get_inverted_selectables (Selection&, list<Selectable* >& results);

	enum VisibleNoteRange {
		FullRange,
		ContentsRange
	};

	Gtk::Adjustment note_range_adjustment;

	VisibleNoteRange note_range() { return _range; }
	void set_note_range(VisibleNoteRange r);
	void set_note_range(uint8_t lowest, uint8_t highest);

	uint8_t lowest_note()  const { return (_range == FullRange) ? 0 : _lowest_note; }
	uint8_t highest_note() const { return (_range == FullRange) ? 127 : _highest_note; }
	
	void update_bounds(uint8_t note_num);
	
	void redisplay_diskstream ();
	
	inline double contents_height() const
		{ return (_trackview.height - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 2); }
	
	inline double note_to_y(uint8_t note) const
		{ return contents_height()
			- (note + 1 - _lowest_note) * note_height() + 1; }
	
	inline uint8_t y_to_note(double y) const
		{ return (uint8_t)((contents_height() - y - 1)
				/ contents_height() * (double)contents_note_range())
				+ _lowest_note; }
	
	inline double note_height() const
		{ return contents_height() / (double)contents_note_range(); }
	
	inline uint8_t contents_note_range() const
		{ return _highest_note - _lowest_note + 1; }
	
	sigc::signal<void> NoteRangeChanged;

  private:
	void setup_rec_box ();
	void rec_data_range_ready (jack_nframes_t start, jack_nframes_t dur, boost::weak_ptr<ARDOUR::Source> src); 
	void update_rec_regions (boost::shared_ptr<ARDOUR::MidiModel> data, jack_nframes_t start, jack_nframes_t dur);
	
	RegionView* add_region_view_internal (boost::shared_ptr<ARDOUR::Region>, bool wait_for_waves);
	void        display_region(MidiRegionView* region_view, bool load_model);
	void        display_diskstream (boost::shared_ptr<ARDOUR::Diskstream> ds);
	
	void update_contents_y_position_and_height ();
	void draw_note_lines();

	void color_handler ();

	void note_range_adjustment_changed();

	VisibleNoteRange          _range;
	double                    _range_sum_cache;
	uint8_t                   _lowest_note;
	uint8_t                   _highest_note;
	ArdourCanvas::Lineset*    _note_lines;
};

#endif /* __ardour_midi_streamview_h__ */
