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

#include "pbd/stacktrace.h"

#include "ardour/audioregion.h"
#include "ardour/audiofilesource.h"
#include "ardour/audio_track.h"
#include "ardour/region_factory.h"
#include "ardour/profile.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"

#include "canvas/rectangle.h"

#include "audio_streamview.h"
#include "audio_region_view.h"
#include "tape_region_view.h"
#include "audio_time_axis.h"
#include "region_selection.h"
#include "selection.h"
#include "public_editor.h"
#include "ardour_ui.h"
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
	color_handler ();
	_amplitude_above_axis = 1.0;
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
AudioStreamView::create_region_view (boost::shared_ptr<Region> r, bool wait_for_waves, bool recording)
{
	AudioRegionView *region_view = 0;
	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (r);

	if (region == 0) {
		return 0;
	}

	switch (_trackview.audio_track()->mode()) {

	case NonLayered:
	case Normal:
		if (recording) {
			region_view = new AudioRegionView (_canvas_group, _trackview, region,
							   _samples_per_pixel, region_color, recording, TimeAxisViewItem::Visibility(
								   TimeAxisViewItem::ShowFrame |
								   TimeAxisViewItem::HideFrameRight |
								   TimeAxisViewItem::HideFrameLeft |
								   TimeAxisViewItem::HideFrameTB));
		} else {
			region_view = new AudioRegionView (_canvas_group, _trackview, region,
					_samples_per_pixel, region_color);
		}
		break;
	case Destructive:
		region_view = new TapeAudioRegionView (_canvas_group, _trackview, region,
				_samples_per_pixel, region_color);
		break;
	default:
		fatal << string_compose (_("programming error: %1"), "illegal track mode in ::add_region_view_internal") << endmsg;
		/*NOTREACHED*/

	}

	region_view->init (region_color, wait_for_waves);
	region_view->set_amplitude_above_axis(_amplitude_above_axis);
	region_view->set_height (child_height ());

	/* if its the special single-sample length that we use for rec-regions, make it
	   insensitive to events
	*/

	if (region->length() == 1) {
		region_view->set_sensitive (false);
	}

	return region_view;
}

RegionView*
AudioStreamView::add_region_view_internal (boost::shared_ptr<Region> r, bool wait_for_waves, bool recording)
{
	RegionView *region_view = create_region_view (r, wait_for_waves, recording);

	if (region_view == 0) {
		return 0;
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

	region_views.push_front (region_view);

        if (_trackview.editor().internal_editing()) {
                region_view->hide_rect ();
        } else {
                region_view->show_rect ();
        }

	/* catch region going away */

	r->DropReferences.connect (*this, invalidator (*this), boost::bind (&AudioStreamView::remove_region_view, this, boost::weak_ptr<Region> (r)), gui_context());

	RegionViewAdded (region_view);

	return region_view;
}

void
AudioStreamView::redisplay_track ()
{
	list<RegionView *>::iterator i;

	// Flag region views as invalid and disable drawing
	for (i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_valid (false);
		(*i)->enable_display (false);
	}

	// Add and display views, and flag them as valid
	if (_trackview.is_audio_track()) {
		_trackview.track()->playlist()->foreach_region(
			sigc::hide_return (sigc::mem_fun (*this, &StreamView::add_region_view))
			);
	}

	// Stack regions by layer, and remove invalid regions
	layer_regions();
}

void
AudioStreamView::setup_rec_box ()
{
	//cerr << _trackview.name() << " streamview SRB region_views.size() = " << region_views.size() << endl;

	if (_trackview.session()->transport_rolling()) {

		// cerr << "\trolling\n";

		if (!rec_active &&
		    _trackview.session()->record_status() == Session::Recording &&
		    _trackview.track()->record_enabled()) {
			if (_trackview.audio_track()->mode() == Normal && Config->get_show_waveforms_while_recording() && rec_regions.size() == rec_rects.size()) {

				/* add a new region, but don't bother if they set show-waveforms-while-recording mid-record */

				SourceList sources;

				rec_data_ready_connections.drop_connections ();
				boost::shared_ptr<AudioTrack> tr = _trackview.audio_track();

				for (uint32_t n = 0; n < tr->n_channels().n_audio(); ++n) {
					boost::shared_ptr<AudioFileSource> src = tr->write_source (n);
					if (src) {
						sources.push_back (src);
						src->PeakRangeReady.connect (rec_data_ready_connections,
						                             invalidator (*this),
						                             boost::bind (&AudioStreamView::rec_peak_range_ready, this, _1, _2, boost::weak_ptr<Source>(src)),
						                             gui_context());
					}
				}

				// handle multi

				framepos_t start = 0;
				if (rec_regions.size() > 0) {
					start = rec_regions.back().first->start()
							+ _trackview.track()->get_captured_frames(rec_regions.size()-1);
				}

				PropertyList plist;

				plist.add (Properties::start, start);
				plist.add (Properties::length, 1);
				plist.add (Properties::name, string());
				plist.add (Properties::layer, 0);

				boost::shared_ptr<AudioRegion> region (
					boost::dynamic_pointer_cast<AudioRegion>(RegionFactory::create (sources, plist, false)));

				assert(region);
				region->set_position (_trackview.session()->transport_frame());
				rec_regions.push_back (make_pair(region, (RegionView*) 0));
			}

			/* start a new rec box */

			boost::shared_ptr<AudioTrack> at;

			at = _trackview.audio_track(); /* we know what it is already */
			framepos_t const frame_pos = at->current_capture_start ();
			gdouble xstart = _trackview.editor().sample_to_pixel (frame_pos);
			gdouble xend = xstart; /* keeps gcc optimized happy, really set in switch() below */
			uint32_t fill_color;

			switch (_trackview.audio_track()->mode()) {
			case Normal:
			case NonLayered:
				xend = xstart;
				fill_color = ARDOUR_UI::config()->get_canvasvar_RecordingRect();
				break;

			case Destructive:
				xend = xstart + 2;
				fill_color = ARDOUR_UI::config()->get_canvasvar_RecordingRect();
				/* make the recording rect translucent to allow
				   the user to see the peak data coming in, etc.
				*/
				fill_color = UINT_RGBA_CHANGE_A (fill_color, 120);
				break;
			}

			ArdourCanvas::Rectangle * rec_rect = new ArdourCanvas::Rectangle (_canvas_group);
			rec_rect->set_x0 (xstart);
			rec_rect->set_y0 (1);
			rec_rect->set_x1 (xend);
			rec_rect->set_y1 (child_height ());
			rec_rect->set_outline_what (0);
			rec_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_TimeAxisFrame());
			rec_rect->set_fill_color (fill_color);
			rec_rect->lower_to_bottom();

			RecBoxInfo recbox;
			recbox.rectangle = rec_rect;
			recbox.start = _trackview.session()->transport_frame();
			recbox.length = 0;

			rec_rects.push_back (recbox);

			screen_update_connection.disconnect();
			screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (
					sigc::mem_fun (*this, &AudioStreamView::update_rec_box));
			rec_updating = true;
			rec_active = true;

		} else if (rec_active &&
			   (_trackview.session()->record_status() != Session::Recording ||
			    !_trackview.track()->record_enabled())) {
			screen_update_connection.disconnect();
			rec_active = false;
			rec_updating = false;
		}

	} else {

		// cerr << "\tNOT rolling, rec_rects = " << rec_rects.size() << " rec_regions = " << rec_regions.size() << endl;

		if (!rec_rects.empty() || !rec_regions.empty()) {

			/* disconnect rapid update */
			screen_update_connection.disconnect();
			rec_data_ready_connections.drop_connections ();
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
AudioStreamView::rec_peak_range_ready (framepos_t start, framecnt_t cnt, boost::weak_ptr<Source> weak_src)
{
	ENSURE_GUI_THREAD (*this, &AudioStreamView::rec_peak_range_ready, start, cnt, weak_src)

	boost::shared_ptr<Source> src (weak_src.lock());

	if (!src) {
		return;
	}

	// this is called from the peak building thread

	if (rec_data_ready_map.size() == 0 || start + cnt > last_rec_data_frame) {
		last_rec_data_frame = start + cnt;
	}

	rec_data_ready_map[src] = true;

	if (rec_data_ready_map.size() == _trackview.track()->n_channels().n_audio()) {
		update_rec_regions (start, cnt);
		rec_data_ready_map.clear();
	}
}

void
AudioStreamView::update_rec_regions (framepos_t start, framecnt_t cnt)
{
	if (!Config->get_show_waveforms_while_recording ()) {
		return;
	}

	uint32_t n = 0;

	for (list<pair<boost::shared_ptr<Region>,RegionView*> >::iterator iter = rec_regions.begin(); iter != rec_regions.end(); n++) {

		list<pair<boost::shared_ptr<Region>,RegionView*> >::iterator tmp = iter;
		++tmp;

		assert (n < rec_rects.size());

		if (!rec_rects[n].rectangle->visible()) {
			/* rect already hidden, this region is done */
			iter = tmp;
			continue;
		}

		boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion>(iter->first);

		if (!region) {
			iter = tmp;
			continue;
		}

		framecnt_t origlen = region->length();

		if (region == rec_regions.back().first && rec_active) {

			if (last_rec_data_frame > region->start()) {

				framecnt_t nlen = last_rec_data_frame - region->start();

				if (nlen != region->length()) {

					region->suspend_property_changes ();
					region->set_position (_trackview.track()->get_capture_start_frame(n));
					region->set_length (nlen);
					region->resume_property_changes ();

					if (origlen == 1) {
						/* our special initial length */
						add_region_view_internal (region, false, true);
						setup_new_rec_layer_time (region);
					}

					check_record_layers (region, (region->position() - region->start() + start + cnt));

					/* also update rect */
					ArdourCanvas::Rectangle * rect = rec_rects[n].rectangle;
					gdouble xend = _trackview.editor().sample_to_pixel (region->position() + region->length());
					rect->set_x1 (xend);
				}

			} else {

				framecnt_t nlen = _trackview.track()->get_captured_frames(n);

				if (nlen != region->length()) {

					if (region->source_length(0) >= region->start() + nlen) {

						region->suspend_property_changes ();
						region->set_position (_trackview.track()->get_capture_start_frame(n));
						region->set_length (nlen);
						region->resume_property_changes ();

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
		}

		iter = tmp;
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

/** Hide xfades for regions that overlap ar.
 *  @return Pair of lists; first is the AudioRegionViews that start xfades were hidden for,
 *  second is the AudioRegionViews that end xfades were hidden for.
 */
pair<list<AudioRegionView*>, list<AudioRegionView*> >
AudioStreamView::hide_xfades_with (boost::shared_ptr<AudioRegion> ar)
{
	list<AudioRegionView*> start_hidden;
	list<AudioRegionView*> end_hidden;
	
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			switch (arv->region()->coverage (ar->position(), ar->last_frame())) {
			case Evoral::OverlapNone:
				break;
			default:
				if (arv->start_xfade_visible ()) {
					start_hidden.push_back (arv);
				}
				if (arv->end_xfade_visible ()) {
					end_hidden.push_back (arv);
				}
				arv->hide_xfades ();
				break;
			}
		}
	}

	return make_pair (start_hidden, end_hidden);
}

void
AudioStreamView::color_handler ()
{
	//case cAudioTrackBase:
	if (_trackview.is_track()) {
		canvas_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_AudioTrackBase());
	}

	//case cAudioBusBase:
	if (!_trackview.is_track()) {
		if (Profile->get_sae() && _trackview.route()->is_master()) {
			canvas_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_AudioMasterBusBase());
		} else {
			canvas_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_AudioBusBase());
		}
	}
}
