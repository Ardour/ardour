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

    $Id$
*/

#include <cmath>
#include <algorithm>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include <ardour/playlist.h>
#include <ardour/audioregion.h>
#include <ardour/audiosource.h>
#include <ardour/audio_diskstream.h>

#include "taperegionview.h"
#include "audio_time_axis.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;

TapeAudioRegionView::TapeAudioRegionView (ArdourCanvas::Group *parent, AudioTimeAxisView &tv, 
					  AudioRegion& r, 
					  double spu, 
					  Gdk::Color& basic_color)

	: AudioRegionView (parent, tv, r, spu, basic_color, 
			   TimeAxisViewItem::Visibility (TimeAxisViewItem::ShowNameHighlight|
							 TimeAxisViewItem::ShowFrame|
							 TimeAxisViewItem::HideFrameLR|
							 TimeAxisViewItem::FullWidthNameHighlight))
{
}

void
TapeAudioRegionView::init (double amplitude_above_axis, Gdk::Color& basic_color, bool wfw)
{
	XMLNode *node;

	editor = 0;
	valid = true;
	in_destructor = false;
	_amplitude_above_axis = amplitude_above_axis;
	zero_line = 0;
	wait_for_waves = wfw;
	_height = 0;

	_flags = 0;

	if ((node = region.extra_xml ("GUI")) != 0) {
		set_flags (node);
	} else {
		_flags = WaveformVisible;
		store_flags ();
	}

	fade_in_handle = 0;
	fade_out_handle = 0;
	gain_line = 0;
	sync_mark = 0;

	compute_colors (basic_color);

	create_waves ();

	name_highlight->set_data ("regionview", this);

	reset_width_dependent_items ((double) region.length() / samples_per_unit);

	set_height (trackview.height);

	region_muted ();
	region_resized (BoundsChanged);
	set_waveview_data_src();
	region_locked ();

	/* no events, no state changes */

	set_colors ();

	// ColorChanged.connect (mem_fun (*this, &AudioRegionView::color_handler));

	/* every time the wave data changes and peaks are ready, redraw */

	
	for (uint32_t n = 0; n < region.n_channels(); ++n) {
		region.source(n).PeaksReady.connect (bind (mem_fun(*this, &TapeAudioRegionView::update), n));
	}
	
}

TapeAudioRegionView::~TapeAudioRegionView()
{
}

void
TapeAudioRegionView::update (uint32_t n)
{
	/* check that all waves are build and ready */

	if (!tmp_waves.empty()) {
		return;
	}

	ENSURE_GUI_THREAD (bind (mem_fun(*this, &TapeAudioRegionView::update), n));

	/* this triggers a cache invalidation and redraw in the waveview */

	waves[n]->property_data_src() = &region;
}

void
TapeAudioRegionView::set_frame_color ()
{
	fill_opacity = 255;
	TimeAxisViewItem::set_frame_color ();
}
