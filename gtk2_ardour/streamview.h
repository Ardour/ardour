/*
    Copyright (C) 2001 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_streamview_h__
#define __ardour_streamview_h__

#include <list>
#include <map>
#include <cmath>

#include <gtkmm.h>
#include <libgnomecanvasmm/libgnomecanvasmm.h>

#include <ardour/location.h>
#include "enums.h"
#include "simplerect.h"

namespace ARDOUR {
	class Route;
	class DiskStream;
	class Crossfade;
	class PeakData;
	class AudioRegion;
	class Source;
}

struct RecBoxInfo {
        Gnome::Canvas::SimpleRect* rectangle;
	jack_nframes_t start;
	jack_nframes_t length;
};

class PublicEditor;
class Selectable;
class AudioTimeAxisView;
class AudioRegionView;
class AudioRegionSelection;
class CrossfadeView;
class Selection;

class StreamView : public sigc::trackable
{
  public:
	StreamView (AudioTimeAxisView&);
	~StreamView ();

	void set_waveform_shape (WaveformShape);

	AudioTimeAxisView& trackview() { return _trackview; }

	void set_zoom_all();

	int set_height (gdouble);
	int set_position (gdouble x, gdouble y);

	int set_samples_per_unit (gdouble spp);
	gdouble get_samples_per_unit () { return _samples_per_unit; }

	int set_amplitude_above_axis (gdouble app);
	gdouble get_amplitude_above_axis () { return _amplitude_above_axis; }

	void set_show_waveforms (bool yn);
	void set_show_waveforms_recording (bool yn) { use_rec_regions = yn; }

	Gnome::Canvas::Item* canvas_item() { return canvas_group; }

	sigc::signal<void,AudioRegionView*> AudioRegionViewAdded;

	enum ColorTarget {
		RegionColor,
		StreamBaseColor
	};

	void apply_color (Gdk::Color&, ColorTarget t);
	void set_selected_regionviews (AudioRegionSelection&);
	void get_selectables (jack_nframes_t start, jack_nframes_t end, list<Selectable* >&);
	void get_inverted_selectables (Selection&, list<Selectable* >& results);
	Gdk::Color get_region_color () const { return region_color; }

	void foreach_regionview (sigc::slot<void,AudioRegionView*> slot);
	void foreach_crossfadeview (void (CrossfadeView::*pmf)(void));

	void attach ();
	
	void region_layered (AudioRegionView*);
	
	AudioRegionView* find_view (const ARDOUR::AudioRegion&);

	void show_all_xfades ();
	void hide_all_xfades ();
	void hide_xfades_involving (AudioRegionView&);
	void reveal_xfades_involving (AudioRegionView&);

  private:
	AudioTimeAxisView& _trackview;

	Gnome::Canvas::Group* canvas_group;
	Gnome::Canvas::SimpleRect* canvas_rect; /* frame around the whole thing */

	typedef list<AudioRegionView* > AudioRegionViewList;
	AudioRegionViewList region_views;

	typedef list<CrossfadeView*> CrossfadeViewList;
	CrossfadeViewList crossfade_views;

	double _samples_per_unit;
	double _amplitude_above_axis;

	sigc::connection screen_update_connection;
	vector<RecBoxInfo> rec_rects;
	list<ARDOUR::AudioRegion* > rec_regions;
	bool rec_updating;
	bool rec_active;
	bool use_rec_regions;
	list<sigc::connection> peak_ready_connections;
	jack_nframes_t last_rec_peak_frame;
	map<ARDOUR::Source*, bool> rec_peak_ready_map;
	
	void update_rec_box ();
	void transport_changed();
	void rec_enable_changed(void*  src = 0);
	void sess_rec_enable_changed();
	void setup_rec_box ();
	void rec_peak_range_ready (jack_nframes_t start, jack_nframes_t cnt, ARDOUR::Source* src); 
	void update_rec_regions ();
	
	void add_region_view (ARDOUR::Region*);
	void add_region_view_internal (ARDOUR::Region*, bool wait_for_waves);
	void remove_region_view (ARDOUR::Region* );
	void remove_rec_region (ARDOUR::Region*);
	void remove_audio_region_view (ARDOUR::AudioRegion* );
	void remove_audio_rec_region (ARDOUR::AudioRegion*);

	void display_diskstream (ARDOUR::DiskStream* );
	void undisplay_diskstream ();
	void redisplay_diskstream ();
	void diskstream_changed (void* );
	void playlist_state_changed (ARDOUR::Change);
	void playlist_changed (ARDOUR::DiskStream* );
	void playlist_modified ();

	bool crossfades_visible;
	void add_crossfade (ARDOUR::Crossfade*);
	void remove_crossfade (ARDOUR::Crossfade*);

	/* XXX why are these different? */
	
	Gdk::Color region_color;
	uint32_t stream_base_color;

	vector<sigc::connection> playlist_connections;
	sigc::connection playlist_change_connection;
};

#endif /* __ardour_streamview_h__ */
