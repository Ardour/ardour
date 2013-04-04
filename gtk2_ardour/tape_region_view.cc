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

#include <cmath>
#include <algorithm>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include "ardour/audioregion.h"
#include "ardour/audiosource.h"

#include "tape_region_view.h"
#include "audio_time_axis.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;

const TimeAxisViewItem::Visibility TapeAudioRegionView::default_tape_visibility
	= TimeAxisViewItem::Visibility (
		TimeAxisViewItem::ShowNameHighlight |
		TimeAxisViewItem::ShowNameText |
		TimeAxisViewItem::ShowFrame |
		TimeAxisViewItem::HideFrameRight |
		TimeAxisViewItem::FullWidthNameHighlight);

TapeAudioRegionView::TapeAudioRegionView (ArdourCanvas::Group *parent, RouteTimeAxisView &tv,
					  boost::shared_ptr<AudioRegion> r,
					  double spu,
					  Gdk::Color const & basic_color)

	: AudioRegionView (parent, tv, r, spu, basic_color, false,
			   TimeAxisViewItem::Visibility ((r->position() != 0) ? default_tape_visibility :
							 TimeAxisViewItem::Visibility (default_tape_visibility|TimeAxisViewItem::HideFrameLeft)))
{
}

void
TapeAudioRegionView::init (Gdk::Color const & basic_color, bool /*wfw*/)
{
	/* never wait for data: always just create the waves, connect once and then
	   we'll update whenever we need to.
	*/

	AudioRegionView::init(basic_color, false);

	/* every time the wave data changes and peaks are ready, redraw */

	for (uint32_t n = 0; n < audio_region()->n_channels(); ++n) {
		audio_region()->audio_source(n)->PeaksReady.connect (*this, invalidator (*this), boost::bind (&TapeAudioRegionView::update, this, n), gui_context());
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

	ENSURE_GUI_THREAD (*this, &TapeAudioRegionView::update, n);
	waves[n]->rebuild ();
}

void
TapeAudioRegionView::set_frame_color ()
{
	fill_opacity = 255;
	AudioRegionView::set_frame_color ();
}
