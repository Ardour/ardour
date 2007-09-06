/*
    Copyright (C) 2001-2007 Paul Davis 

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
#include <cassert>
#include <utility>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include <ardour/midi_playlist.h>
#include <ardour/midi_region.h>
#include <ardour/midi_source.h>
#include <ardour/midi_diskstream.h>
#include <ardour/midi_track.h>
#include <ardour/midi_events.h>
#include <ardour/smf_source.h>
#include <ardour/region_factory.h>

#include "automation_streamview.h"
#include "region_view.h"
#include "automation_region_view.h"
#include "automation_time_axis.h"
#include "canvas-simplerect.h"
#include "region_selection.h"
#include "selection.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "utils.h"
#include "simplerect.h"
#include "simpleline.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;

AutomationStreamView::AutomationStreamView (AutomationTimeAxisView& tv)
	: StreamView (*dynamic_cast<RouteTimeAxisView*>(tv.get_parent()),
			new ArdourCanvas::Group(*tv.canvas_display))
	, _controller(tv.controller())
	, _automation_view(tv)
{
	//canvas_rect->property_fill_color_rgba() = stream_base_color;
	canvas_rect->property_outline_color_rgba() = RGBA_BLACK;
	canvas_rect->lower(2);

	use_rec_regions = tv.editor.show_waveforms_recording ();
}

AutomationStreamView::~AutomationStreamView ()
{
}


RegionView*
AutomationStreamView::add_region_view_internal (boost::shared_ptr<Region> region, bool wfd)
{
	if ( ! region) {
		cerr << "No region" << endl;
		return NULL;
	}

	if (wfd) {
		boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(region);
		if (mr)
			mr->midi_source()->load_model();
	}

	const boost::shared_ptr<AutomationControl> control = region->control(_controller->controllable()->parameter());

	if ( ! control) {
		cerr << "No " << _controller->controllable()->parameter().to_string()
			<< " for " << region->name() << endl;
		return NULL;
	}

	const boost::shared_ptr<AutomationList> list = control->list();

	AutomationRegionView *region_view;
	std::list<RegionView *>::iterator i;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->region() == region) {
			
			/* great. we already have a MidiRegionView for this Region. use it again. */

			(*i)->set_valid (true);
			(*i)->enable_display(wfd);
			display_region(dynamic_cast<AutomationRegionView*>(*i));

			return NULL;
		}
	}
	
	region_view = new AutomationRegionView (canvas_group, _automation_view, region, list,
			_samples_per_unit, region_color);
		
	region_view->init (region_color, false);
	region_views.push_front (region_view);
	
	/* follow global waveform setting */

	if (wfd) {
		region_view->enable_display(true);
		//region_view->midi_region()->midi_source(0)->load_model();
	}

	/* display events */
	display_region(region_view);

	/* catch regionview going away */
	region->GoingAway.connect (bind (mem_fun (*this, &AutomationStreamView::remove_region_view), region));
	
	RegionViewAdded (region_view);

	return region_view;
}

void
AutomationStreamView::display_region(AutomationRegionView* region_view)
{
	region_view->line().reset();
	region_view->raise();
}

void
AutomationStreamView::redisplay_diskstream ()
{
	list<RegionView *>::iterator i, tmp;

	for (i = region_views.begin(); i != region_views.end(); ++i)
		(*i)->set_valid (false);
	
	if (_trackview.is_track()) {
		_trackview.get_diskstream()->playlist()->foreach_region (static_cast<StreamView*>(this), &StreamView::add_region_view);
	}

	for (i = region_views.begin(); i != region_views.end(); ) {
		tmp = i;
		tmp++;

		if (!(*i)->is_valid()) {
			delete *i;
			region_views.erase (i);
		} else {
			(*i)->enable_display(true);
			(*i)->set_y_position_and_height(0, height);
		}

		i = tmp;
	}
	
	/* now fix layering */

	for (RegionViewList::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		region_layered (*i);
	}
}


void
AutomationStreamView::setup_rec_box ()
{
}

void
AutomationStreamView::update_rec_regions (nframes_t start, nframes_t dur)
{
}

void
AutomationStreamView::rec_data_range_ready (jack_nframes_t start, jack_nframes_t dur)
{
	// this is called from the butler thread for now
	
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &AutomationStreamView::rec_data_range_ready), start, dur));
	
	this->update_rec_regions (start, dur);
}

void
AutomationStreamView::color_handler ()
{
	/*if (_trackview.is_midi_track()) {
		canvas_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiTrackBase.get();
	} 

	if (!_trackview.is_midi_track()) {
		canvas_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiBusBase.get();;
	}*/
}

