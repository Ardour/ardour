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

#include "ardour/location.h"
#include "editing.h"
#include "simplerect.h"
#include "streamview.h"

namespace Gdk {
	class Color;
}

namespace ARDOUR {
	class AudioRegion;
	class Route;
	class Source;
	struct PeakData;
}

class PublicEditor;
class Selectable;
class AudioTimeAxisView;
class AudioRegionView;
class RegionSelection;
class Selection;

class AudioStreamView : public StreamView
{
  public:
	AudioStreamView (AudioTimeAxisView&);

	int     set_amplitude_above_axis (gdouble app);
	gdouble get_amplitude_above_axis () { return _amplitude_above_axis; }

	void set_show_waveforms (bool yn);

	void show_all_fades ();
	void hide_all_fades ();

	void hide_xfades_with (boost::shared_ptr<ARDOUR::AudioRegion> ar);

	RegionView* create_region_view (boost::shared_ptr<ARDOUR::Region>, bool, bool);

  private:
	void setup_rec_box ();
	void rec_peak_range_ready (framepos_t start, ARDOUR::framecnt_t cnt, boost::weak_ptr<ARDOUR::Source> src);
	void update_rec_regions (ARDOUR::framepos_t, ARDOUR::framecnt_t);

	RegionView* add_region_view_internal (boost::shared_ptr<ARDOUR::Region>, bool wait_for_waves, bool recording = false);
	void remove_audio_region_view (boost::shared_ptr<ARDOUR::AudioRegion> );

	void redisplay_track ();

	void color_handler ();

	void parameter_changed (std::string const &);
	void set_waveform_shape (ARDOUR::WaveformShape);
	void set_waveform_scale (ARDOUR::WaveformScale);

	double _amplitude_above_axis;

	std::map<boost::shared_ptr<ARDOUR::Source>, bool> rec_data_ready_map;

	bool outline_region;
};

#endif /* __ardour_audio_streamview_h__ */
