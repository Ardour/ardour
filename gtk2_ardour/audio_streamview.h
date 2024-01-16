/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
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

#ifndef __ardour_audio_streamview_h__
#define __ardour_audio_streamview_h__

#include <cmath>
#include <list>
#include <memory>
#include <map>


#include "ardour/location.h"
#include "point_selection.h"
#include "editing.h"
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
	~AudioStreamView ();

	int     set_amplitude_above_axis (gdouble app);
	gdouble get_amplitude_above_axis () { return _amplitude_above_axis; }

	void show_all_fades ();
	void hide_all_fades ();

	std::pair<std::list<AudioRegionView*>, std::list<AudioRegionView*> > hide_xfades_with (std::shared_ptr<ARDOUR::AudioRegion> ar);

	RegionView* create_region_view (std::shared_ptr<ARDOUR::Region>, bool, bool);
	void set_selected_points (PointSelection&);

	void reload_waves ();

	void set_layer_display (LayerDisplay);

	ArdourCanvas::Container* region_canvas () const { return _region_group; }

private:
	void setup_rec_box ();
	void rec_peak_range_ready (samplepos_t start, ARDOUR::samplecnt_t cnt, std::weak_ptr<ARDOUR::Source> src);
	void update_rec_regions (ARDOUR::samplepos_t, ARDOUR::samplecnt_t);

	RegionView* add_region_view_internal (std::shared_ptr<ARDOUR::Region>, bool wait_for_waves, bool recording = false);
	void remove_audio_region_view (std::shared_ptr<ARDOUR::AudioRegion> );

	void redisplay_track ();

	void color_handler ();

	double _amplitude_above_axis;

	std::map<std::shared_ptr<ARDOUR::Source>, bool> rec_data_ready_map;

	ArdourCanvas::Container* _region_group;

	bool outline_region;
};

#endif /* __ardour_audio_streamview_h__ */
