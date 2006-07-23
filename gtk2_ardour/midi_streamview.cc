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

#include <cmath>
#include <cassert>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include <ardour/midi_playlist.h>
#include <ardour/midi_region.h>
#include <ardour/midi_source.h>
#include <ardour/midi_diskstream.h>
#include <ardour/midi_track.h>
//#include <ardour/playlist_templates.h>
#include <ardour/source.h>

#include "midi_streamview.h"
#include "regionview.h"
//#include "midi_regionview.h"
#include "midi_time_axis.h"
#include "canvas-simplerect.h"
#include "region_selection.h"
#include "selection.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "utils.h"
#include "color.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;

MidiStreamView::MidiStreamView (MidiTimeAxisView& tv)
	: StreamView (tv)
{
	region_color = _trackview.color();

	if (tv.is_midi_track())
		stream_base_color = color_map[cMidiTrackBase];
	else
		stream_base_color = color_map[cMidiBusBase];

	/* set_position() will position the group */

	canvas_group = new ArdourCanvas::Group(*_trackview.canvas_display);

	canvas_rect = new ArdourCanvas::SimpleRect (*canvas_group);
	canvas_rect->property_x1() = 0.0;
	canvas_rect->property_y1() = 0.0;
	canvas_rect->property_x2() = 1000000.0;
	canvas_rect->property_y2() = (double) tv.height;
	canvas_rect->property_outline_color_rgba() = color_map[cMidiTrackOutline];
	canvas_rect->property_outline_what() = (guint32) (0x1|0x2|0x8);  // outline ends and bottom 
	canvas_rect->property_fill_color_rgba() = stream_base_color;

	canvas_rect->signal_event().connect (bind (mem_fun (_trackview.editor, &PublicEditor::canvas_stream_view_event), canvas_rect, &_trackview));

	_samples_per_unit = _trackview.editor.get_current_zoom();

	if (_trackview.is_midi_track()) {
		_trackview.midi_track()->DiskstreamChanged.connect (mem_fun (*this, &MidiStreamView::diskstream_changed));
		_trackview.session().TransportStateChange.connect (mem_fun (*this, &MidiStreamView::transport_changed));
		_trackview.get_diskstream()->RecordEnableChanged.connect (mem_fun (*this, &MidiStreamView::rec_enable_changed));
		_trackview.session().RecordStateChanged.connect (mem_fun (*this, &MidiStreamView::sess_rec_enable_changed));
	} 

	rec_updating = false;
	rec_active = false;
	use_rec_regions = tv.editor.show_waveforms_recording ();

	ColorChanged.connect (mem_fun (*this, &MidiStreamView::color_handler));
}

MidiStreamView::~MidiStreamView ()
{
	undisplay_diskstream ();
	delete canvas_group;
}


void
MidiStreamView::add_region_view_internal (Region *r, bool wait_for_waves)
{
#if 0
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &MidiStreamView::add_region_view), r));

	MidiRegion* region = dynamic_cast<MidiRegion*> (r);

	if (region == 0) {
		return;
	}

	MidiRegionView *region_view;
	list<RegionView *>::iterator i;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		if (&(*i)->region() == r) {
			
			/* great. we already have a MidiRegionView for this Region. use it again. */

			(*i)->set_valid (true);
			return;
		}
	}
	
	/* FIXME 
	switch (_trackview.midi_track()->mode()) {
	case Normal:
		region_view = new MidiRegionView (canvas_group, _trackview, *region, 
						   _samples_per_unit, region_color);
		break;
	case Destructive:
		region_view = new TapeMidiRegionView (canvas_group, _trackview, *region, 
						       _samples_per_unit, region_color);
		break;
	}
	*/
	region_view = new MidiRegionView (canvas_group, _trackview, *region, 
					   _samples_per_unit, region_color);

	region_view->init (region_color, wait_for_waves);
	region_views.push_front (region_view);
	
	/* catch regionview going away */

	region->GoingAway.connect (mem_fun (*this, &MidiStreamView::remove_region_view));
	
	RegionViewAdded (region_view);
#endif
}

// FIXME: code duplication with AudioStreamVIew
void
MidiStreamView::redisplay_diskstream ()
{
	list<RegionView *>::iterator i, tmp;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_valid (false);
	}

	if (_trackview.is_midi_track()) {
		_trackview.get_diskstream()->playlist()->foreach_region (static_cast<StreamView*>(this), &StreamView::add_region_view);
	}

	for (i = region_views.begin(); i != region_views.end(); ) {
		tmp = i;
		tmp++;

		if (!(*i)->is_valid()) {
			delete *i;
			region_views.erase (i);
		} 

		i = tmp;
	}

	/* now fix layering */

	playlist_modified ();
}


void
MidiStreamView::setup_rec_box ()
{
#if 0
	// cerr << _trackview.name() << " streamview SRB\n";

	if (_trackview.session().transport_rolling()) {

		// cerr << "\trolling\n";

		if (!rec_active && 
		    _trackview.session().record_status() == Session::Recording && 
		    _trackview.get_diskstream()->record_enabled()) {

			if (_trackview.midi_track()->mode() == Normal && use_rec_regions && rec_regions.size() == rec_rects.size()) {

				/* add a new region, but don't bother if they set use_rec_regions mid-record */

				MidiRegion::SourceList sources;

				// FIXME
				MidiDiskstream* ads = dynamic_cast<MidiDiskstream*>(_trackview.get_diskstream());
				assert(ads);

				// handle multi
				
				jack_nframes_t start = 0;
				if (rec_regions.size() > 0) {
					start = rec_regions.back()->start() + _trackview.get_diskstream()->get_captured_frames(rec_regions.size()-1);
				}
				
				MidiRegion * region = new MidiRegion(sources, start, 1 , "", 0, (Region::Flag)(Region::DefaultFlags | Region::DoNotSaveState), false);
				region->set_position (_trackview.session().transport_frame(), this);
				rec_regions.push_back (region);
				/* catch it if it goes away */
				region->GoingAway.connect (mem_fun (*this, &MidiStreamView::remove_rec_region));

				/* we add the region later */
			}
			
			/* start a new rec box */

			MidiTrack* at;

			at = _trackview.midi_track(); /* we know what it is already */
			MidiDiskstream& ds = at->midi_diskstream();
			jack_nframes_t frame_pos = ds.current_capture_start ();
			gdouble xstart = _trackview.editor.frame_to_pixel (frame_pos);
			gdouble xend;
			uint32_t fill_color;

			switch (_trackview.midi_track()->mode()) {
			case Normal:
				xend = xstart;
				fill_color = color_map[cRecordingRectFill];
				break;

			case Destructive:
				xend = xstart + 2;
				fill_color = color_map[cRecordingRectFill];
				/* make the recording rect translucent to allow
				   the user to see the peak data coming in, etc.
				*/
				fill_color = UINT_RGBA_CHANGE_A (fill_color, 120);
				break;
			}
			
			ArdourCanvas::SimpleRect * rec_rect = new Gnome::Canvas::SimpleRect (*canvas_group);
			rec_rect->property_x1() = xstart;
			rec_rect->property_y1() = 1.0;
			rec_rect->property_x2() = xend;
			rec_rect->property_y2() = (double) _trackview.height - 1;
			rec_rect->property_outline_color_rgba() = color_map[cRecordingRectOutline];
			rec_rect->property_fill_color_rgba() = fill_color;
			
			RecBoxInfo recbox;
			recbox.rectangle = rec_rect;
			recbox.start = _trackview.session().transport_frame();
			recbox.length = 0;
			
			rec_rects.push_back (recbox);
			
			screen_update_connection.disconnect();
			screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (mem_fun (*this, &MidiStreamView::update_rec_box));	
			rec_updating = true;
			rec_active = true;

		} else if (rec_active &&
			   (_trackview.session().record_status() != Session::Recording ||
			    !_trackview.get_diskstream()->record_enabled())) {

			screen_update_connection.disconnect();
			rec_active = false;
			rec_updating = false;

		}
		
	} else {

		// cerr << "\tNOT rolling, rec_rects = " << rec_rects.size() << " rec_regions = " << rec_regions.size() << endl;

		if (!rec_rects.empty() || !rec_regions.empty()) {

			/* disconnect rapid update */
			screen_update_connection.disconnect();

			for (list<sigc::connection>::iterator prc = peak_ready_connections.begin(); prc != peak_ready_connections.end(); ++prc) {
				(*prc).disconnect();
			}
			peak_ready_connections.clear();

			rec_updating = false;
			rec_active = false;
			last_rec_peak_frame = 0;
			
			/* remove temp regions */
			for (list<Region*>::iterator iter=rec_regions.begin(); iter != rec_regions.end(); )
			{
				list<Region*>::iterator tmp;

				tmp = iter;
				++tmp;

				/* this will trigger the remove_region_view */
				delete *iter;

				iter = tmp;
			}
			
			rec_regions.clear();

			// cerr << "\tclear " << rec_rects.size() << " rec rects\n";
		

			/* transport stopped, clear boxes */
			for (vector<RecBoxInfo>::iterator iter=rec_rects.begin(); iter != rec_rects.end(); ++iter) {
				RecBoxInfo &rect = (*iter);
				delete rect.rectangle;
			}
			
			rec_rects.clear();
			
		}
	}
#endif
}

void
MidiStreamView::update_rec_regions ()
{
#if 0
	if (use_rec_regions) {

		uint32_t n = 0;

		for (list<Region*>::iterator iter = rec_regions.begin(); iter != rec_regions.end(); n++) {

			list<Region*>::iterator tmp;

			tmp = iter;
			++tmp;

			if (!canvas_item_visible (rec_rects[n].rectangle)) {
				/* rect already hidden, this region is done */
				iter = tmp;
				continue;
			}
			
			// FIXME
			MidiRegion * region = dynamic_cast<MidiRegion*>(*iter);
			assert(region);

			jack_nframes_t origlen = region->length();

			if (region == rec_regions.back() && rec_active) {

				if (last_rec_peak_frame > region->start()) {

					jack_nframes_t nlen = last_rec_peak_frame - region->start();

					if (nlen != region->length()) {

						region->freeze ();
						region->set_position (_trackview.get_diskstream()->get_capture_start_frame(n), this);
						region->set_length (nlen, this);
						region->thaw ("updated");

						if (origlen == 1) {
							/* our special initial length */
							add_region_view_internal (region, false);
						}

						/* also update rect */
						ArdourCanvas::SimpleRect * rect = rec_rects[n].rectangle;
						gdouble xend = _trackview.editor.frame_to_pixel (region->position() + region->length());
						rect->property_x2() = xend;
					}
				}

			} else {

				jack_nframes_t nlen = _trackview.get_diskstream()->get_captured_frames(n);

				if (nlen != region->length()) {

					if (region->source(0).length() >= region->start() + nlen) {

						region->freeze ();
						region->set_position (_trackview.get_diskstream()->get_capture_start_frame(n), this);
						region->set_length (nlen, this);
						region->thaw ("updated");
						
						if (origlen == 1) {
							/* our special initial length */
							add_region_view_internal (region, false);
						}
						
						/* also hide rect */
						ArdourCanvas::Item * rect = rec_rects[n].rectangle;
						rect->hide();

					}
				}
			}

			iter = tmp;
		}
	}
#endif
}

void
MidiStreamView::color_handler (ColorID id, uint32_t val)
{
	switch (id) {
	case cMidiTrackBase:
		if (_trackview.is_midi_track()) {
			canvas_rect->property_fill_color_rgba() = val;
		} 
		break;
	case cMidiBusBase:
		if (!_trackview.is_midi_track()) {
			canvas_rect->property_fill_color_rgba() = val;
		}
		break;
	case cMidiTrackOutline:
		canvas_rect->property_outline_color_rgba() = val;
		break;

	default:
		break;
	}
}
