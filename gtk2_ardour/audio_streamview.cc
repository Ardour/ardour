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

#include <ardour/audioplaylist.h>
#include <ardour/audioregion.h>
#include <ardour/audiofilesource.h>
#include <ardour/audio_diskstream.h>
#include <ardour/audio_track.h>
#include <ardour/playlist_templates.h>
#include <ardour/source.h>
#include <ardour/region_factory.h>

#include "audio_streamview.h"
#include "audio_region_view.h"
#include "tape_region_view.h"
#include "audio_time_axis.h"
#include "canvas-waveview.h"
#include "canvas-simplerect.h"
#include "region_selection.h"
#include "selection.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "crossfade_view.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "utils.h"
#include "color.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;

AudioStreamView::AudioStreamView (AudioTimeAxisView& tv)
	: StreamView (tv)
{
	crossfades_visible = true;

	if (tv.is_track())
		stream_base_color = color_map[cAudioTrackBase];
	else
		stream_base_color = color_map[cAudioBusBase];
	
	canvas_rect->property_fill_color_rgba() = stream_base_color;
	canvas_rect->property_outline_color_rgba() = color_map[cAudioTrackOutline];

	_amplitude_above_axis = 1.0;

	use_rec_regions = tv.editor.show_waveforms_recording ();
}

AudioStreamView::~AudioStreamView ()
{
}

int
AudioStreamView::set_height (gdouble h)
{
	/* limit the values to something sane-ish */
	if (h < 10.0 || h > 1000.0) {
		return -1;
	}

	StreamView::set_height(h);

	for (CrossfadeViewList::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		(*i)->set_height (h);
	}

	return 0;
}

int 
AudioStreamView::set_samples_per_unit (gdouble spp)
{
	StreamView::set_samples_per_unit(spp);

	for (CrossfadeViewList::iterator xi = crossfade_views.begin(); xi != crossfade_views.end(); ++xi) {
		(*xi)->set_samples_per_unit (spp);
	}

	return 0;
}

int 
AudioStreamView::set_amplitude_above_axis (gdouble app)
{
	RegionViewList::iterator i;

	if (app < 1.0) {
		return -1;
	}

	_amplitude_above_axis = app;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv)
			arv->set_amplitude_above_axis (app);
	}

	return 0;
}

void
AudioStreamView::add_region_view_internal (boost::shared_ptr<Region> r, bool wait_for_waves)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &AudioStreamView::add_region_view), r));

	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (r);

	if (region == 0) {
		return;
	}

	AudioRegionView *region_view;
	list<RegionView *>::iterator i;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->region() == r) {
			
			/* great. we already have a AudioRegionView for this Region. use it again. */

			(*i)->set_valid (true);
			return;
		}
	}
	
	switch (_trackview.audio_track()->mode()) {
	case Normal:
		region_view = new AudioRegionView (canvas_group, _trackview, region, 
						   _samples_per_unit, region_color);
		break;
	case Destructive:
		region_view = new TapeAudioRegionView (canvas_group, _trackview, region, 
						       _samples_per_unit, region_color);
		break;
	}

	region_view->init (region_color, wait_for_waves);
	region_view->set_amplitude_above_axis(_amplitude_above_axis);
	region_views.push_front (region_view);
	
	/* follow global waveform setting */

	region_view->set_waveform_visible(_trackview.editor.show_waveforms());

	/* catch regionview going away */

	region->GoingAway.connect (bind (mem_fun (*this, &AudioStreamView::remove_region_view), region));
	
	RegionViewAdded (region_view);
}

void
AudioStreamView::remove_region_view (boost::shared_ptr<Region> r)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &AudioStreamView::remove_region_view), r));

	for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end();) {
		list<CrossfadeView*>::iterator tmp;
		
		tmp = i;
		++tmp;
		
		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(r);
		if (ar && (*i)->crossfade.involves (ar)) {
			delete *i;
			crossfade_views.erase (i);
		}
		
		i = tmp;
	}

	StreamView::remove_region_view(r);
}

void
AudioStreamView::undisplay_diskstream ()
{
	StreamView::undisplay_diskstream();

	for (CrossfadeViewList::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		delete *i;
	}

	crossfade_views.clear ();
}

void
AudioStreamView::playlist_modified ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &AudioStreamView::playlist_modified));

	StreamView::playlist_modified();
	
	/* if the playlist is modified, make sure xfades are on top and all the regionviews are stacked 
	   correctly.
	*/

	for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		(*i)->get_canvas_group()->raise_to_top();
	}
}

void
AudioStreamView::playlist_changed (boost::shared_ptr<Diskstream> ds)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &AudioStreamView::playlist_changed), ds));

	StreamView::playlist_changed(ds);

	AudioPlaylist* apl = dynamic_cast<AudioPlaylist*>(ds->playlist());
	if (apl)
		playlist_connections.push_back (apl->NewCrossfade.connect (mem_fun (*this, &AudioStreamView::add_crossfade)));
}

void
AudioStreamView::add_crossfade (Crossfade *crossfade)
{
	AudioRegionView* lview = 0;
	AudioRegionView* rview = 0;

	ENSURE_GUI_THREAD (bind (mem_fun (*this, &AudioStreamView::add_crossfade), crossfade));

	/* first see if we already have a CrossfadeView for this Crossfade */

	for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		if (&(*i)->crossfade == crossfade) {
			if (!crossfades_visible) {
				(*i)->hide();
			} else {
				(*i)->show ();
			}
			(*i)->set_valid (true);
			return;
		}
	}

	/* create a new one */

	for (list<RegionView *>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		AudioRegionView* arv = dynamic_cast<AudioRegionView*>(*i);

		if (!lview && arv && (arv->region() == crossfade->out())) {
			lview = arv;
		}
		if (!rview && arv && (arv->region() == crossfade->in())) {
			rview = arv;
		}
	}

	CrossfadeView *cv = new CrossfadeView (_trackview.canvas_display,
					       _trackview,
					       *crossfade,
					       _samples_per_unit,
					       region_color,
					       *lview, *rview);

	crossfade->Invalidated.connect (mem_fun (*this, &AudioStreamView::remove_crossfade));
	crossfade_views.push_back (cv);

	if (!crossfades_visible) {
		cv->hide ();
	}
}

void
AudioStreamView::remove_crossfade (Crossfade *xfade)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &AudioStreamView::remove_crossfade), xfade));

	for (list<CrossfadeView*>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		if (&(*i)->crossfade == xfade) {
			delete *i;
			crossfade_views.erase (i);
			break;
		}
	}
}

void
AudioStreamView::redisplay_diskstream ()
{
	list<RegionView *>::iterator i, tmp;
	list<CrossfadeView*>::iterator xi, tmpx;

	
	for (i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_valid (false);
	}

	for (xi = crossfade_views.begin(); xi != crossfade_views.end(); ++xi) {
		(*xi)->set_valid (false);
		if ((*xi)->visible()) {
			(*xi)->show ();
		}
	}

	if (_trackview.is_audio_track()) {
		_trackview.get_diskstream()->playlist()->foreach_region (static_cast<StreamView*>(this), &StreamView::add_region_view);
		AudioPlaylist* apl = dynamic_cast<AudioPlaylist*>(_trackview.get_diskstream()->playlist());
		if (apl)
			apl->foreach_crossfade (this, &AudioStreamView::add_crossfade);
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

	for (xi = crossfade_views.begin(); xi != crossfade_views.end();) {
		tmpx = xi;
		tmpx++;

		if (!(*xi)->valid()) {
			delete *xi;
			crossfade_views.erase (xi);
		}

		xi = tmpx;
	}

	/* now fix layering */

	playlist_modified ();
}

void
AudioStreamView::set_show_waveforms (bool yn)
{
	for (list<RegionView *>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv)
			arv->set_waveform_visible (yn);
	}
}

void
AudioStreamView::set_waveform_shape (WaveformShape shape)
{
	for (RegionViewList::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv)
			arv->set_waveform_shape (shape);
	}
}		
		
void
AudioStreamView::setup_rec_box ()
{
	// cerr << _trackview.name() << " streamview SRB\n";

	if (_trackview.session().transport_rolling()) {

		// cerr << "\trolling\n";

		if (!rec_active && 
		    _trackview.session().record_status() == Session::Recording && 
		    _trackview.get_diskstream()->record_enabled()) {

			if (_trackview.audio_track()->mode() == Normal && use_rec_regions && rec_regions.size() == rec_rects.size()) {

				/* add a new region, but don't bother if they set use_rec_regions mid-record */

				SourceList sources;

				for (list<sigc::connection>::iterator prc = rec_data_ready_connections.begin(); prc != rec_data_ready_connections.end(); ++prc) {
					(*prc).disconnect();
				}
				rec_data_ready_connections.clear();
					
				// FIXME
				boost::shared_ptr<AudioDiskstream> ads = boost::dynamic_pointer_cast<AudioDiskstream>(_trackview.get_diskstream());
				assert(ads);

				for (uint32_t n=0; n < ads->n_channels().get(DataType::AUDIO); ++n) {
					boost::shared_ptr<AudioFileSource> src = boost::static_pointer_cast<AudioFileSource> (ads->write_source (n));
					if (src) {
						sources.push_back (src);
						rec_data_ready_connections.push_back (src->PeakRangeReady.connect (bind (mem_fun (*this, &AudioStreamView::rec_peak_range_ready), src))); 
					}
				}

				// handle multi
				
				jack_nframes_t start = 0;
				if (rec_regions.size() > 0) {
					start = rec_regions.back()->start() + _trackview.get_diskstream()->get_captured_frames(rec_regions.size()-1);
				}
				
				boost::shared_ptr<AudioRegion> region (boost::dynamic_pointer_cast<AudioRegion>
								       (RegionFactory::create (sources, start, 1 , "", 0, (Region::Flag)(Region::DefaultFlags | Region::DoNotSaveState), false)));
				region->set_position (_trackview.session().transport_frame(), this);
				rec_regions.push_back (region);
				/* catch it if it goes away */
				region->GoingAway.connect (bind (mem_fun (*this, &AudioStreamView::remove_rec_region), region));

				/* we add the region later */
			}
			
			/* start a new rec box */

			AudioTrack* at;

			at = _trackview.audio_track(); /* we know what it is already */
			boost::shared_ptr<AudioDiskstream> ds = at->audio_diskstream();
			jack_nframes_t frame_pos = ds->current_capture_start ();
			gdouble xstart = _trackview.editor.frame_to_pixel (frame_pos);
			gdouble xend;
			uint32_t fill_color;

			switch (_trackview.audio_track()->mode()) {
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
			screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (mem_fun (*this, &AudioStreamView::update_rec_box));	
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

			for (list<sigc::connection>::iterator prc = rec_data_ready_connections.begin(); prc != rec_data_ready_connections.end(); ++prc) {
				(*prc).disconnect();
			}
			rec_data_ready_connections.clear();

			rec_updating = false;
			rec_active = false;
			last_rec_data_frame = 0;
			
			/* remove temp regions */
			
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
}

void
AudioStreamView::foreach_crossfadeview (void (CrossfadeView::*pmf)(void))
{
	for (list<CrossfadeView*>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		((*i)->*pmf) ();
	}
}

void
AudioStreamView::rec_peak_range_ready (jack_nframes_t start, jack_nframes_t cnt, boost::shared_ptr<Source> src)
{
	// this is called from the peak building thread

	ENSURE_GUI_THREAD(bind (mem_fun (*this, &AudioStreamView::rec_peak_range_ready), start, cnt, src));
	
	if (rec_data_ready_map.size() == 0 || start+cnt > last_rec_data_frame) {
		last_rec_data_frame = start + cnt;
	}

	rec_data_ready_map[src] = true;

	if (rec_data_ready_map.size() == _trackview.get_diskstream()->n_channels().get(DataType::AUDIO)) {
		this->update_rec_regions ();
		rec_data_ready_map.clear();
	}
}

void
AudioStreamView::update_rec_regions ()
{
	if (use_rec_regions) {

		uint32_t n = 0;

		for (list<boost::shared_ptr<Region> >::iterator iter = rec_regions.begin(); iter != rec_regions.end(); n++) {

			list<boost::shared_ptr<Region> >::iterator tmp;

			tmp = iter;
			++tmp;

			if (!canvas_item_visible (rec_rects[n].rectangle)) {
				/* rect already hidden, this region is done */
				iter = tmp;
				continue;
			}
			
			// FIXME
			boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion>(*iter);
			assert(region);

			jack_nframes_t origlen = region->length();

			if (region == rec_regions.back() && rec_active) {

				if (last_rec_data_frame > region->start()) {

					jack_nframes_t nlen = last_rec_data_frame - region->start();

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

					if (region->source(0)->length() >= region->start() + nlen) {

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
}

void
AudioStreamView::show_all_xfades ()
{
	foreach_crossfadeview (&CrossfadeView::show);
	crossfades_visible = true;
}

void
AudioStreamView::hide_all_xfades ()
{
	foreach_crossfadeview (&CrossfadeView::hide);
	crossfades_visible = false;
}

void
AudioStreamView::hide_xfades_involving (AudioRegionView& rv)
{
	for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		if ((*i)->crossfade.involves (rv.audio_region())) {
			(*i)->fake_hide ();
		}
	}
}

void
AudioStreamView::reveal_xfades_involving (AudioRegionView& rv)
{
	for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		if ((*i)->crossfade.involves (rv.audio_region()) && (*i)->visible()) {
			(*i)->show ();
		}
	}
}

void
AudioStreamView::color_handler (ColorID id, uint32_t val)
{
	switch (id) {
	case cAudioTrackBase:
		if (_trackview.is_track()) {
			canvas_rect->property_fill_color_rgba() = val;
		} 
		break;
	case cAudioBusBase:
		if (!_trackview.is_track()) {
			canvas_rect->property_fill_color_rgba() = val;
		}
		break;
	case cAudioTrackOutline:
		canvas_rect->property_outline_color_rgba() = val;
		break;

	default:
		break;
	}
}

