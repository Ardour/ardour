/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <cmath>
#include <cassert>
#include <utility>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include "ardour/audioregion.h"
#include "ardour/audiofilesource.h"
#include "ardour/audio_track.h"
#include "ardour/record_enable_control.h"
#include "ardour/region_factory.h"
#include "ardour/profile.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"

#include "canvas/rectangle.h"

#include "audio_streamview.h"
#include "audio_region_view.h"
#include "audio_time_axis.h"
#include "region_selection.h"
#include "region_gain_line.h"
#include "selection.h"
#include "public_editor.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "ui_config.h"

#include "pbd/i18n.h"

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

	default:
		fatal << string_compose (_("programming error: %1"), "illegal track mode in ::create_region_view()") << endmsg;
		abort(); /*NOTREACHED*/

	}

	region_view->init (wait_for_waves);
	region_view->set_amplitude_above_axis(_amplitude_above_axis);
	region_view->set_height (child_height ());

	/* if its the special single-sample length that we use for rec-regions, make it
	   insensitive to events
	*/

	if (region->length_samples() == 1) {
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

	region_views.push_front (region_view);

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
AudioStreamView::reload_waves ()
{
	list<RegionView *>::iterator i;
	for (i = region_views.begin(); i != region_views.end(); ++i) {
		AudioRegionView* arv = dynamic_cast<AudioRegionView*> (*i);
		if (!arv) {
			continue;
		}
		arv->delete_waves();
		arv->create_waves();
	}
}

void
AudioStreamView::setup_rec_box ()
{
	//cerr << _trackview.name() << " streamview SRB region_views.size() = " << region_views.size() << endl;

	if (!_trackview.session()->transport_stopped_or_stopping() &&
	    (_trackview.session()->transport_rolling() || _trackview.session()->get_record_enabled())) {

		// cerr << "\trolling\n";

		if (!rec_active &&
		    _trackview.session()->record_status() == Session::Recording &&
		    _trackview.track()->rec_enable_control()->get_value()) {
			if (_trackview.audio_track()->mode() == Normal && UIConfiguration::instance().get_show_waveforms_while_recording() && rec_regions.size() == rec_rects.size()) {

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

				samplepos_t start = 0;
				if (rec_regions.size() > 0) {
					start = rec_regions.back().first->start_sample()
							+ _trackview.track()->get_captured_samples(rec_regions.size()-1);
				}

				PropertyList plist;

				plist.add (Properties::start, start);
				plist.add (Properties::length, 1);
				plist.add (Properties::name, string());
				plist.add (Properties::layer, 0);

				boost::shared_ptr<AudioRegion> region (
					boost::dynamic_pointer_cast<AudioRegion>(RegionFactory::create (sources, plist, false)));

				assert(region);
				region->set_position (timepos_t (_trackview.session()->transport_sample()));
				rec_regions.push_back (make_pair(region, (RegionView*) 0));
			}

			/* start a new rec box */

			boost::shared_ptr<AudioTrack> at = _trackview.audio_track();
			samplepos_t const sample_pos = at->current_capture_start ();

			create_rec_box(sample_pos, 0);

		} else if (rec_active &&
		           (_trackview.session()->record_status() != Session::Recording ||
		            !_trackview.track()->rec_enable_control()->get_value())) {
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
AudioStreamView::rec_peak_range_ready (samplepos_t start, samplecnt_t cnt, boost::weak_ptr<Source> weak_src)
{
	ENSURE_GUI_THREAD (*this, &AudioStreamView::rec_peak_range_ready, start, cnt, weak_src)

	boost::shared_ptr<Source> src (weak_src.lock());

	if (!src) {
		return;
	}

	// this is called from the peak building thread

	if (rec_data_ready_map.size() == 0 || start + cnt > last_rec_data_sample) {
		last_rec_data_sample = start + cnt;
	}

	rec_data_ready_map[src] = true;

	if (rec_data_ready_map.size() == _trackview.track()->n_channels().n_audio()) {
		update_rec_regions (start, cnt);
		rec_data_ready_map.clear();
	}
}

void
AudioStreamView::update_rec_regions (samplepos_t start, samplecnt_t cnt)
{
	if (!UIConfiguration::instance().get_show_waveforms_while_recording ()) {
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

		samplecnt_t origlen = region->length_samples();

		if (region == rec_regions.back().first && rec_active) {

			if (last_rec_data_sample > region->start_sample()) {

				samplecnt_t nlen = last_rec_data_sample - region->start_sample();

				if (nlen != region->length_samples()) {

					region->suspend_property_changes ();
					/* set non-musical position / length */
					region->set_position (timepos_t (_trackview.track()->get_capture_start_sample(n)));
					region->set_length (timecnt_t (nlen));
					region->resume_property_changes ();

					if (origlen == 1) {
						/* our special initial length */
						add_region_view_internal (region, false, true);
						setup_new_rec_layer_time (region);
					}

					check_record_layers (region, (region->position_sample() - region->start_sample() + start + cnt));

					/* also update rect */
					ArdourCanvas::Rectangle * rect = rec_rects[n].rectangle;
					gdouble xend = _trackview.editor().sample_to_pixel (region->position_sample() + region->length_samples());
					rect->set_x1 (xend);
				}

			} else {

				samplecnt_t nlen = _trackview.track()->get_captured_samples(n);

				if (nlen != region->length_samples()) {

					if (region->source_length(0) >= region->start_sample() + nlen) {

						region->suspend_property_changes ();
						region->set_position (timepos_t (_trackview.track()->get_capture_start_sample(n)));
						region->set_length (timecnt_t (nlen));
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
			switch (arv->region()->coverage (ar->nt_position(), ar->nt_last())) {
			case Temporal::OverlapNone:
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
		canvas_rect->set_fill_color (UIConfiguration::instance().color_mod ("audio track base", "audio track base"));
	}

	//case cAudioBusBase:
	if (!_trackview.is_track()) {
		canvas_rect->set_fill_color (UIConfiguration::instance().color_mod ("audio bus base", "audio bus base"));
	}
}

void
AudioStreamView::set_selected_points (PointSelection& points)
{
	for (list<RegionView *>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv && arv->get_gain_line ()) {
			arv->get_gain_line ()->set_selected_points (points);
		}
	}
}
