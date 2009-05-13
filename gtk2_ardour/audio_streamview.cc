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
#include <utility>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/audiofilesource.h"
#include "ardour/audio_diskstream.h"
#include "ardour/audio_track.h"
#include "ardour/playlist_templates.h"
#include "ardour/source.h"
#include "ardour/region_factory.h"
#include "ardour/profile.h"

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

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;

AudioStreamView::AudioStreamView (AudioTimeAxisView& tv)
	: StreamView (tv)
{
	crossfades_visible = true;
	_waveform_scale = LinearWaveform;
	_waveform_shape = Traditional;
	color_handler ();
	_amplitude_above_axis = 1.0;

	use_rec_regions = tv.editor().show_waveforms_recording ();
}

AudioStreamView::~AudioStreamView ()
{
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

RegionView*
AudioStreamView::add_region_view_internal (boost::shared_ptr<Region> r, bool wait_for_waves, bool recording)
{
	AudioRegionView *region_view = 0;
	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (r);

	if (region == 0) {
		return NULL;
	}

//	if(!recording){
//		for (list<RegionView *>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
//			if ((*i)->region() == r) {
//				cerr << "audio_streamview in add_region_view_internal region found" << endl;
				/* great. we already have a AudioRegionView for this Region. use it again. */
				
//				(*i)->set_valid (true);

				// this might not be necessary
//				AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);

//				if (arv) {
//					arv->set_waveform_scale (_waveform_scale);
//					arv->set_waveform_shape (_waveform_shape);
//				}
					
//				return NULL;
//			}
//		}
//	}

	switch (_trackview.audio_track()->mode()) {
	
	case NonLayered:
	case Normal:
		if (recording) {
			region_view = new AudioRegionView (canvas_group, _trackview, region, 
					_samples_per_unit, region_color, recording, TimeAxisViewItem::Visibility(
							TimeAxisViewItem::ShowFrame | TimeAxisViewItem::HideFrameRight));
		} else {
			region_view = new AudioRegionView (canvas_group, _trackview, region, 
					_samples_per_unit, region_color);
		}
		break;
	case Destructive:
		region_view = new TapeAudioRegionView (canvas_group, _trackview, region, 
				_samples_per_unit, region_color);
		break;
	default:
		fatal << string_compose (_("programming error: %1"), "illegal track mode in ::add_region_view_internal") << endmsg;
		/*NOTREACHED*/

	}

	region_view->init (region_color, wait_for_waves);
	region_view->set_amplitude_above_axis(_amplitude_above_axis);
	region_view->set_height (child_height ());
	region_views.push_front (region_view);

	/* if its the special single-sample length that we use for rec-regions, make it 
	   insensitive to events 
	*/

	if (region->length() == 1) {
		region_view->set_sensitive (false);
	}

	/* if this was the first one, then lets query the waveform scale and shape.
	   otherwise, we set it to the current value */
	   
	if (region_views.size() == 1) {

		if (region_view->waveform_logscaled()) {
			_waveform_scale = LogWaveform;
		} else {
			_waveform_scale = LinearWaveform;
		}

		if (region_view->waveform_rectified()) {
			_waveform_shape = Rectified;
		} else {
			_waveform_shape = Traditional;
		}
	}
	else {
		region_view->set_waveform_scale(_waveform_scale);
		region_view->set_waveform_shape(_waveform_shape);
	}
	
	/* follow global waveform setting */
	region_view->set_waveform_visible(_trackview.editor().show_waveforms());

	/* catch regionview going away */
	region->GoingAway.connect (bind (mem_fun (*this, &AudioStreamView::remove_region_view), boost::weak_ptr<Region> (r)));

	RegionViewAdded (region_view);

	return region_view;
}

void
AudioStreamView::remove_region_view (boost::weak_ptr<Region> weak_r)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &AudioStreamView::remove_region_view), weak_r));

	boost::shared_ptr<Region> r (weak_r.lock());

	if (!r) {
		return;
	}

	if (!_trackview.session().deletion_in_progress()) {

		for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end();) {
			list<CrossfadeView*>::iterator tmp;
			
			tmp = i;
			++tmp;
			
			boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(r);
			if (ar && (*i)->crossfade->involves (ar)) {
				delete *i;
				crossfade_views.erase (i);
			}
			
			i = tmp;
		}
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
AudioStreamView::playlist_modified_weak (boost::weak_ptr<Diskstream> ds)
{
	boost::shared_ptr<Diskstream> sp (ds.lock());
	if (sp) {
		playlist_modified (sp);
	}
}

void
AudioStreamView::playlist_modified (boost::shared_ptr<Diskstream> ds)
{
	/* we do not allow shared_ptr<T> to be bound to slots */
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &AudioStreamView::playlist_modified_weak), ds));

	StreamView::playlist_modified (ds);
	
	/* make sure xfades are on top and all the regionviews are stacked correctly. */

	for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		(*i)->get_canvas_group()->raise_to_top();
	}
}

void
AudioStreamView::playlist_changed_weak (boost::weak_ptr<Diskstream> ds)
{
	boost::shared_ptr<Diskstream> sp (ds.lock());
	if (sp) {
		playlist_changed (sp);
	}
}

void
AudioStreamView::playlist_changed (boost::shared_ptr<Diskstream> ds)
{
	ENSURE_GUI_THREAD (bind (
			mem_fun (*this, &AudioStreamView::playlist_changed_weak),
			boost::weak_ptr<Diskstream> (ds)));

	StreamView::playlist_changed(ds);

	boost::shared_ptr<AudioPlaylist> apl = boost::dynamic_pointer_cast<AudioPlaylist>(ds->playlist());
	if (apl) {
		playlist_connections.push_back (apl->NewCrossfade.connect (
				mem_fun (*this, &AudioStreamView::add_crossfade)));
	}
}

void
AudioStreamView::add_crossfade_weak (boost::weak_ptr<Crossfade> crossfade)
{
	boost::shared_ptr<Crossfade> sp (crossfade.lock());

	if (!sp) {
		return;
	}

	add_crossfade (sp);
}

void
AudioStreamView::add_crossfade (boost::shared_ptr<Crossfade> crossfade)
{
	AudioRegionView* lview = 0;
	AudioRegionView* rview = 0;

	/* we do not allow shared_ptr<T> to be bound to slots */
	
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &AudioStreamView::add_crossfade_weak), boost::weak_ptr<Crossfade> (crossfade)));

	/* first see if we already have a CrossfadeView for this Crossfade */

	for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		if ((*i)->crossfade == crossfade) {

			if (!crossfades_visible || _layer_display == Stacked) {
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

	CrossfadeView *cv = new CrossfadeView (_trackview.canvas_display (),
					       _trackview,
					        crossfade,
					       _samples_per_unit,
					       region_color,
					       *lview, *rview);
	cv->set_valid (true);
	crossfade->Invalidated.connect (mem_fun (*this, &AudioStreamView::remove_crossfade));
	crossfade_views.push_back (cv);
	if (!Config->get_xfades_visible() || !crossfades_visible || _layer_display == Stacked) {
		cv->hide ();
	}
}

void
AudioStreamView::remove_crossfade (boost::shared_ptr<Region> r)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &AudioStreamView::remove_crossfade), r));

	boost::shared_ptr<Crossfade> xfade = boost::dynamic_pointer_cast<Crossfade> (r);

	for (list<CrossfadeView*>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		if ((*i)->crossfade == xfade) {
			delete *i;
			crossfade_views.erase (i);
			break;
		}
	}
}

void
AudioStreamView::redisplay_diskstream ()
{
	list<RegionView *>::iterator i;
	list<CrossfadeView*>::iterator xi, tmpx;

	// Flag region views as invalid and disable drawing
	for (i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_valid (false);
		(*i)->enable_display (false);
	}

	// Flag crossfade views as invalid
	for (xi = crossfade_views.begin(); xi != crossfade_views.end(); ++xi) {
		(*xi)->set_valid (false);
		if ((*xi)->visible() && _layer_display != Stacked) {
			(*xi)->show ();
		}
	}

	// Add and display region and crossfade views, and flag them as valid

	if (_trackview.is_audio_track()) {
		_trackview.get_diskstream()->playlist()->foreach_region(
				static_cast<StreamView*>(this),
				&StreamView::add_region_view);

		boost::shared_ptr<AudioPlaylist> apl = boost::dynamic_pointer_cast<AudioPlaylist>(
				_trackview.get_diskstream()->playlist());
		if (apl)
			apl->foreach_crossfade (this, &AudioStreamView::add_crossfade);
	}
	
	// Remove invalid crossfade views
	for (xi = crossfade_views.begin(); xi != crossfade_views.end();) {
		tmpx = xi;
		tmpx++;

		if (!(*xi)->valid()) {
			delete *xi;
			crossfade_views.erase (xi);
		}

		xi = tmpx;
	}

	// Stack regions by layer, and remove invalid regions
	layer_regions();
}

void
AudioStreamView::set_show_waveforms (bool yn)
{
	for (list<RegionView *>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			arv->set_waveform_visible (yn);
		}
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
	_waveform_shape = shape;
}		

void
AudioStreamView::set_waveform_scale (WaveformScale scale)
{
	for (RegionViewList::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) 
			arv->set_waveform_scale (scale);
	}
	_waveform_scale = scale;
}		

void
AudioStreamView::setup_rec_box ()
{
	//cerr << _trackview.name() << " streamview SRB region_views.size() = " << region_views.size() << endl;

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

				for (uint32_t n=0; n < ads->n_channels().n_audio(); ++n) {
					boost::shared_ptr<AudioFileSource> src = boost::static_pointer_cast<AudioFileSource> (ads->write_source (n));
					if (src) {
						sources.push_back (src);
						
						rec_data_ready_connections.push_back (src->PeakRangeReady.connect (bind
							(mem_fun (*this, &AudioStreamView::rec_peak_range_ready), boost::weak_ptr<Source>(src)))); 
					}
				}

				// handle multi
				
				nframes_t start = 0;
				if (rec_regions.size() > 0) {
					start = rec_regions.back().first->start() + _trackview.get_diskstream()->get_captured_frames(rec_regions.size()-1);
				}
				
				boost::shared_ptr<AudioRegion> region (boost::dynamic_pointer_cast<AudioRegion>
								       (RegionFactory::create (sources, start, 1 , "", 0, (Region::Flag)(Region::DefaultFlags | Region::DoNotSaveState), false)));
				assert(region);
				region->set_position (_trackview.session().transport_frame(), this);
				rec_regions.push_back (make_pair(region, (RegionView*)0));
			}
			
			/* start a new rec box */

			boost::shared_ptr<AudioTrack> at;

			at = _trackview.audio_track(); /* we know what it is already */
			boost::shared_ptr<AudioDiskstream> ds = at->audio_diskstream();
			nframes_t frame_pos = ds->current_capture_start ();
			gdouble xstart = _trackview.editor().frame_to_pixel (frame_pos);
			gdouble xend;
			uint32_t fill_color;

			switch (_trackview.audio_track()->mode()) {
			case Normal:
			case NonLayered:
				xend = xstart;
				fill_color = ARDOUR_UI::config()->canvasvar_RecordingRect.get();
				break;

			case Destructive:
				xend = xstart + 2;
				fill_color = ARDOUR_UI::config()->canvasvar_RecordingRect.get();
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
			rec_rect->property_y2() = child_height ();
			rec_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_TimeAxisFrame.get();
			rec_rect->property_outline_what() = 0x1 | 0x2 | 0x4 | 0x8;
			rec_rect->property_fill_color_rgba() = fill_color;
			rec_rect->lower_to_bottom();
			
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
			
			/* remove temp regions */

			for (list<pair<boost::shared_ptr<Region>,RegionView*> >::iterator iter = rec_regions.begin(); iter != rec_regions.end(); ) {
				list<pair<boost::shared_ptr<Region>,RegionView*> >::iterator tmp;

				tmp = iter;
				++tmp;

				(*iter).first->drop_references ();

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
}

void
AudioStreamView::foreach_crossfadeview (void (CrossfadeView::*pmf)(void))
{
	for (list<CrossfadeView*>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		((*i)->*pmf) ();
	}
}

void
AudioStreamView::rec_peak_range_ready (nframes_t start, nframes_t cnt, boost::weak_ptr<Source> weak_src)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &AudioStreamView::rec_peak_range_ready), start, cnt, weak_src));
	
	boost::shared_ptr<Source> src (weak_src.lock());

	if (!src) {
		return; 
	}

	// this is called from the peak building thread
	
	if (rec_data_ready_map.size() == 0 || start+cnt > last_rec_data_frame) {
		last_rec_data_frame = start + cnt;
	}
	
	rec_data_ready_map[src] = true;
	
	if (rec_data_ready_map.size() == _trackview.get_diskstream()->n_channels().n_audio()) {
		this->update_rec_regions ();
		rec_data_ready_map.clear();
	}
}

void
AudioStreamView::update_rec_regions ()
{
	if (use_rec_regions) {
		uint32_t n = 0;

		for (list<pair<boost::shared_ptr<Region>,RegionView*> >::iterator iter = rec_regions.begin(); iter != rec_regions.end(); n++) {

			list<pair<boost::shared_ptr<Region>,RegionView*> >::iterator tmp;

			tmp = iter;
			++tmp;

			if (!canvas_item_visible (rec_rects[n].rectangle)) {
				/* rect already hidden, this region is done */
				iter = tmp;
				continue;
			}
			
			boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion>(iter->first);
			if (!region) {
				continue;
			}

			nframes_t origlen = region->length();

			if (region == rec_regions.back().first && rec_active) {

				if (last_rec_data_frame > region->start()) {

					nframes_t nlen = last_rec_data_frame - region->start();

					if (nlen != region->length()) {

						region->freeze ();
						region->set_position (_trackview.get_diskstream()->get_capture_start_frame(n), this);
						region->set_length (nlen, this);
						region->thaw ("updated");

						if (origlen == 1) {
							/* our special initial length */
							add_region_view_internal (region, false, true);
						}

						/* also update rect */
						ArdourCanvas::SimpleRect * rect = rec_rects[n].rectangle;
						gdouble xend = _trackview.editor().frame_to_pixel (region->position() + region->length());
						rect->property_x2() = xend;
					}
				}

			} else {

				nframes_t nlen = _trackview.get_diskstream()->get_captured_frames(n);

				if (nlen != region->length()) {

					if (region->source_length(0) >= region->start() + nlen) {

						region->freeze ();
						region->set_position (_trackview.get_diskstream()->get_capture_start_frame(n), this);
						region->set_length (nlen, this);
						region->thaw ("updated");
						
						if (origlen == 1) {
							/* our special initial length */
							add_region_view_internal (region, false, true);
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
AudioStreamView::show_all_fades ()
{
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			arv->set_fade_visibility (true);
		}
	}
}

void
AudioStreamView::hide_all_fades ()
{
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			arv->set_fade_visibility (false);
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
		if ((*i)->crossfade->involves (rv.audio_region())) {
			(*i)->fake_hide ();
		}
	}
}

void
AudioStreamView::reveal_xfades_involving (AudioRegionView& rv)
{
	for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		if ((*i)->crossfade->involves (rv.audio_region()) && (*i)->visible() && _layer_display != Stacked) {
			(*i)->show ();
		}
	}
}

void
AudioStreamView::color_handler ()
{
	//case cAudioTrackBase:
	if (_trackview.is_track()) {
		canvas_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_AudioTrackBase.get();
	} 

	//case cAudioBusBase:
	if (!_trackview.is_track()) {
		if (Profile->get_sae() && _trackview.route()->is_master()) {
			canvas_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_AudioMasterBusBase.get();
		} else {
			canvas_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_AudioBusBase.get();
		}
	}
}

void
AudioStreamView::update_contents_height ()
{
	StreamView::update_contents_height ();
	
	for (CrossfadeViewList::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		if (_layer_display == Overlaid) {
			(*i)->show ();
			(*i)->set_height (height);
		} else {
			(*i)->hide ();
		}
	}
}
