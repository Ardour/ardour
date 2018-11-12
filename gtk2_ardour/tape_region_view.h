/*
    Copyright (C) 2006 Paul Davis

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

#ifndef __gtk_ardour_tape_audio_region_view_h__
#define __gtk_ardour_tape_audio_region_view_h__

#include <vector>

#include "audio_region_view.h"

class TapeAudioRegionView : public AudioRegionView
{
public:
	TapeAudioRegionView (ArdourCanvas::Container *,
	                     RouteTimeAxisView&,
	                     boost::shared_ptr<ARDOUR::AudioRegion>,
	                     double initial_samples_per_pixel,
	                     uint32_t base_color);
	~TapeAudioRegionView ();

protected:
	void init (bool wait_for_waves);

	void update (uint32_t n);

	static const TimeAxisViewItem::Visibility default_tape_visibility;
};

#endif /* __gtk_ardour_tape_audio_region_view_h__ */
