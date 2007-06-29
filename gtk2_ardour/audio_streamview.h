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

#ifndef __ardour_audio_streamview_h__
#define __ardour_audio_streamview_h__

#include <list>
#include <map>
#include <cmath>

#include <boost/weak_ptr.hpp>

#include <ardour/location.h>
#include "enums.h"
#include "simplerect.h"
#include "streamview.h"

namespace Gdk {
	class Color;
}

namespace ARDOUR {
	class Route;
	class Diskstream;
	class Crossfade;
	class PeakData;
	class AudioRegion;
	class Source;
}

class PublicEditor;
class Selectable;
class AudioTimeAxisView;
class AudioRegionView;
class RegionSelection;
class CrossfadeView;
class Selection;

class AudioStreamView : public StreamView
{
  public:
	AudioStreamView (AudioTimeAxisView&);
	~AudioStreamView ();

	void set_waveform_shape (WaveformShape);
	WaveformShape get_waveform_shape () const { return _waveform_shape; }
	void set_waveform_scale (WaveformScale);
	WaveformScale get_waveform_scale () const { return _waveform_scale; }

	int set_samples_per_unit (gdouble spp);

	int     set_amplitude_above_axis (gdouble app);
	gdouble get_amplitude_above_axis () { return _amplitude_above_axis; }

	void set_show_waveforms (bool yn);
	void set_show_waveforms_recording (bool yn) { use_rec_regions = yn; }

	void foreach_crossfadeview (void (CrossfadeView::*pmf)(void));

	void show_all_xfades ();
	void hide_all_xfades ();
	void hide_xfades_involving (AudioRegionView&);
	void reveal_xfades_involving (AudioRegionView&);

  private:
	void setup_rec_box ();
	void rec_peak_range_ready (nframes_t start, nframes_t cnt, boost::weak_ptr<ARDOUR::Source> src); 
	void update_rec_regions ();
	
	RegionView* add_region_view_internal (boost::shared_ptr<ARDOUR::Region>, bool wait_for_waves);
	void remove_region_view (boost::weak_ptr<ARDOUR::Region> );
	void remove_audio_region_view (boost::shared_ptr<ARDOUR::AudioRegion> );

	void undisplay_diskstream ();
	void redisplay_diskstream ();
	void playlist_modified_weak (boost::weak_ptr<ARDOUR::Diskstream>);
	void playlist_modified (boost::shared_ptr<ARDOUR::Diskstream>);
	void playlist_changed (boost::shared_ptr<ARDOUR::Diskstream>);

	void add_crossfade (boost::shared_ptr<ARDOUR::Crossfade>);
	void add_crossfade_weak (boost::weak_ptr<ARDOUR::Crossfade>);
	void remove_crossfade (boost::shared_ptr<ARDOUR::Region>);

	void color_handler ();

	void update_contents_y_position_and_height ();
	
	double _amplitude_above_axis;
	
	typedef list<CrossfadeView*> CrossfadeViewList;
	CrossfadeViewList crossfade_views;
	bool              crossfades_visible;

	WaveformShape     _waveform_shape;
	WaveformScale     _waveform_scale;
	
	map<boost::shared_ptr<ARDOUR::Source>, bool> rec_data_ready_map;
};

#endif /* __ardour_audio_streamview_h__ */
