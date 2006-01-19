#include <cmath>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include <ardour/audioplaylist.h>
#include <ardour/audioregion.h>
#include <ardour/diskstream.h>
#include <ardour/audio_track.h>
#include <ardour/playlist_templates.h>
#include <ardour/source.h>

#include "streamview.h"
#include "regionview.h"
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

using namespace ARDOUR;
using namespace Editing;

StreamView::StreamView (AudioTimeAxisView& tv)
	: _trackview (tv)
{
	region_color = _trackview.color();
	crossfades_visible = true;

	if (tv.is_audio_track()) {
		/* TRACK */
		//stream_base_color = RGBA_TO_UINT (222,223,218,255);
		stream_base_color = color_map[cAudioTrackBase];
	} else {
		/* BUS */
		//stream_base_color = RGBA_TO_UINT (230,226,238,255);
		stream_base_color = color_map[cAudioBusBase];
	}

	/* set_position() will position the group */

	//GTK2FIX -- how to get the group? is the canvas display really a group?
	//canvas_group = gnome_canvas_item_new (GNOME_CANVAS_GROUP(_trackview.canvas_display),
	//			    gnome_canvas_group_get_type (),
	//			    NULL);
	canvas_group = new ArdourCanvas::Group(*_trackview.canvas_display);

	//canvas_rect = gnome_canvas_item_new (GNOME_CANVAS_GROUP(canvas_group),
	//			   gnome_canvas_simplerect_get_type(),
	//			   "x1", 0.0,
	//			   "y1", 0.0,
	//			   "x2", 1000000.0,
	//			   "y2", (double) tv.height,
	//			   "outline_color_rgba", color_map[cAudioTrackOutline],
	//			   /* outline ends and bottom */
	//			   "outline_what", (guint32) (0x1|0x2|0x8),
	//			   "fill_color_rgba", stream_base_color,
	//	]		   NULL);
	canvas_rect = new ArdourCanvas::SimpleRect (*canvas_group);
	canvas_rect->property_x1() = 0.0;
	canvas_rect->property_y1() = 0.0;
	canvas_rect->property_x2() = 1000000.0;
	canvas_rect->property_y2() = (double) tv.height;
	canvas_rect->property_outline_color_rgba() = color_map[cAudioTrackOutline];
	/* outline ends and bottom */
	canvas_rect->property_outline_what() = (guint32) (0x1|0x2|0x8);
	canvas_rect->property_fill_color_rgba() = stream_base_color;

	canvas_rect->signal_event().connect (bind (mem_fun (_trackview.editor, &PublicEditor::canvas_stream_view_event), canvas_rect, &_trackview));

	_samples_per_unit = _trackview.editor.get_current_zoom();
	_amplitude_above_axis = 1.0;

	if (_trackview.is_audio_track()) {
		_trackview.audio_track()->diskstream_changed.connect (mem_fun (*this, &StreamView::diskstream_changed));
		_trackview.session().TransportStateChange.connect (mem_fun (*this, &StreamView::transport_changed));
		_trackview.get_diskstream()->record_enable_changed.connect (mem_fun (*this, &StreamView::rec_enable_changed));
		_trackview.session().RecordStateChanged.connect (mem_fun (*this, &StreamView::sess_rec_enable_changed));
	} 

	rec_updating = false;
	rec_active = false;
	use_rec_regions = tv.editor.show_waveforms_recording ();
	last_rec_peak_frame = 0;
}

StreamView::~StreamView ()
{
	undisplay_diskstream ();
	delete canvas_group;
}

void
StreamView::attach ()
{
	if (_trackview.is_audio_track()) {
		display_diskstream (_trackview.get_diskstream());
	}
}

int
StreamView::set_position (gdouble x, gdouble y)

{
	canvas_group->property_x() = x;
	canvas_group->property_y() = y;
	return 0;
}

int
StreamView::set_height (gdouble h)
{
	/* limit the values to something sane-ish */

	if (h < 10.0 || h > 1000.0) {
		return -1;
	}

	canvas_rect->property_y2() = h;

	for (AudioRegionViewList::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_height (h);
	}

	for (CrossfadeViewList::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		(*i)->set_height (h);
	}

	for (vector<RecBoxInfo>::iterator i = rec_rects.begin(); i != rec_rects.end(); ++i) {
		RecBoxInfo &recbox = (*i);
		recbox.rectangle->property_y2() = h - 1.0;
	}

	return 0;
}

int 
StreamView::set_samples_per_unit (gdouble spp)
{
	AudioRegionViewList::iterator i;

	if (spp < 1.0) {
		return -1;
	}

	_samples_per_unit = spp;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_samples_per_unit (spp);
	}

	for (CrossfadeViewList::iterator xi = crossfade_views.begin(); xi != crossfade_views.end(); ++xi) {
		(*xi)->set_samples_per_unit (spp);
	}

	for (vector<RecBoxInfo>::iterator xi = rec_rects.begin(); xi != rec_rects.end(); ++xi) {
		RecBoxInfo &recbox = (*xi);
		
		gdouble xstart = _trackview.editor.frame_to_pixel ( recbox.start );
		gdouble xend = _trackview.editor.frame_to_pixel ( recbox.start + recbox.length );

		recbox.rectangle->property_x1() = xstart;
		recbox.rectangle->property_x2() = xend;
	}

	return 0;
}

int 
StreamView::set_amplitude_above_axis (gdouble app)

{
	AudioRegionViewList::iterator i;

	if (app < 1.0) {
		return -1;
	}

	_amplitude_above_axis = app;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_amplitude_above_axis (app);
	}

	return 0;
}

void
StreamView::add_region_view (Region *r)
{
	add_region_view_internal (r, true);
}

void
StreamView::add_region_view_internal (Region *r, bool wait_for_waves)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &StreamView::add_region_view), r));

	AudioRegion* region = dynamic_cast<AudioRegion*> (r);

	if (region == 0) {
		return;
	}

	AudioRegionView *region_view;
	list<AudioRegionView *>::iterator i;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		if (&(*i)->region == region) {
			
			/* great. we already have a AudioRegionView for this Region. use it again.
			 */

			(*i)->set_valid (true);
			return;
		}
	}

	region_view = new AudioRegionView (canvas_group,
					   _trackview,
					   *region,
					   _samples_per_unit,
					   _amplitude_above_axis, 
					   region_color, 
					   wait_for_waves);

	region_views.push_front (region_view);
	
	/* follow global waveform setting */

	region_view->set_waveform_visible(_trackview.editor.show_waveforms());

	/* catch regionview going away */

	region->GoingAway.connect (mem_fun (*this, &StreamView::remove_region_view));
	
	AudioRegionViewAdded (region_view);
}

void
StreamView::remove_region_view (Region *r)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &StreamView::remove_region_view), r));

	AudioRegion* ar = dynamic_cast<AudioRegion*> (r);

	if (ar == 0) {
		return;
	}

	for (list<AudioRegionView *>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if (&((*i)->region) == ar) {
			delete *i;
			region_views.erase (i);
			break;
		}
	}

	for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end();) {
		list<CrossfadeView*>::iterator tmp;
		
		tmp = i;
		++tmp;
		
		if ((*i)->crossfade.involves (*ar)) {
			delete *i;
			crossfade_views.erase (i);
		}
		
		i = tmp;
	}
}

void
StreamView::remove_rec_region (Region *r)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &StreamView::remove_rec_region), r));
	
	if (!Gtkmm2ext::UI::instance()->caller_is_gui_thread()) {
		fatal << "region deleted from non-GUI thread!" << endmsg;
		/*NOTREACHED*/
	} 

	AudioRegion* ar = dynamic_cast<AudioRegion*> (r);

	if (ar == 0) {
		return;
	}

	for (list<AudioRegion *>::iterator i = rec_regions.begin(); i != rec_regions.end(); ++i) {
		if (*i == ar) {
			rec_regions.erase (i);
			break;
		}
	}
}

void
StreamView::undisplay_diskstream ()
{
	for (AudioRegionViewList::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		delete *i;
	}

	for (CrossfadeViewList::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		delete *i;
	}

	region_views.clear();
	crossfade_views.clear ();
}

void
StreamView::display_diskstream (DiskStream *ds)
{
	playlist_change_connection.disconnect();
	playlist_changed (ds);
	playlist_change_connection = ds->PlaylistChanged.connect (bind (mem_fun (*this, &StreamView::playlist_changed), ds));
}

void
StreamView::playlist_modified ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &StreamView::playlist_modified));

	/* if the playlist is modified, make sure xfades are on top and all the regionviews are stacked 
	   correctly.
	*/

	for (AudioRegionViewList::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		region_layered (*i);
	}

	for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		(*i)->get_canvas_group()->raise_to_top();
	}
}

void
StreamView::playlist_changed (DiskStream *ds)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &StreamView::playlist_changed), ds));

	/* disconnect from old playlist */

	for (vector<sigc::connection>::iterator i = playlist_connections.begin(); i != playlist_connections.end(); ++i) {
		(*i).disconnect();
	}
	
	playlist_connections.clear();
	undisplay_diskstream ();

	/* draw it */

	redisplay_diskstream ();

	/* catch changes */

	playlist_connections.push_back (ds->playlist()->RegionAdded.connect (mem_fun (*this, &StreamView::add_region_view)));
	playlist_connections.push_back (ds->playlist()->RegionRemoved.connect (mem_fun (*this, &StreamView::remove_region_view)));
	playlist_connections.push_back (ds->playlist()->StateChanged.connect (mem_fun (*this, &StreamView::playlist_state_changed)));
	playlist_connections.push_back (ds->playlist()->Modified.connect (mem_fun (*this, &StreamView::playlist_modified)));
	playlist_connections.push_back (ds->playlist()->NewCrossfade.connect (mem_fun (*this, &StreamView::add_crossfade)));
}

void
StreamView::add_crossfade (Crossfade *crossfade)
{
	AudioRegionView* lview = 0;
	AudioRegionView* rview = 0;

	ENSURE_GUI_THREAD (bind (mem_fun (*this, &StreamView::add_crossfade), crossfade));

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

	for (list<AudioRegionView *>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if (!lview && &((*i)->region) == &crossfade->out()) {
			lview = *i;
		}
		if (!rview && &((*i)->region) == &crossfade->in()) {
			rview = *i;
		}
	}

	CrossfadeView *cv = new CrossfadeView (_trackview.canvas_display,
					       _trackview,
					       *crossfade,
					       _samples_per_unit,
					       region_color,
					       *lview, *rview);

	crossfade->Invalidated.connect (mem_fun (*this, &StreamView::remove_crossfade));
	crossfade_views.push_back (cv);

	if (!crossfades_visible) {
		cv->hide ();
	}
}

void
StreamView::remove_crossfade (Crossfade *xfade)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &StreamView::remove_crossfade), xfade));

	for (list<CrossfadeView*>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		if (&(*i)->crossfade == xfade) {
			delete *i;
			crossfade_views.erase (i);
			break;
		}
	}
}

void
StreamView::playlist_state_changed (Change ignored)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &StreamView::playlist_state_changed), ignored));

	redisplay_diskstream ();
}

void
StreamView::redisplay_diskstream ()
{
	list<AudioRegionView *>::iterator i, tmp;
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
		_trackview.get_diskstream()->playlist()->foreach_region (this, &StreamView::add_region_view);
		_trackview.get_diskstream()->playlist()->foreach_crossfade (this, &StreamView::add_crossfade);
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
StreamView::diskstream_changed (void *src_ignored)
{
	AudioTrack *at;

	if ((at = _trackview.audio_track()) != 0) {
		DiskStream& ds = at->disk_stream();
		/* XXX grrr: when will SigC++ allow me to bind references? */
		Gtkmm2ext::UI::instance()->call_slot (bind (mem_fun (*this, &StreamView::display_diskstream), &ds));
	} else {
		Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &StreamView::undisplay_diskstream));
	}
}

void
StreamView::apply_color (Gdk::Color& color, ColorTarget target)

{
	list<AudioRegionView *>::iterator i;

	switch (target) {
	case RegionColor:
		region_color = color;
		for (i = region_views.begin(); i != region_views.end(); ++i) {
			(*i)->set_color (region_color);
		}
		// stream_base_color = RGBA_TO_UINT (color.red/256, color.green/256, color.blue/256, 255);
		// gnome_canvas_item_set (canvas_rect, "fill_color_rgba", stream_base_color, NULL);
		break;
		
	case StreamBaseColor:
		// stream_base_color = RGBA_TO_UINT (color.red/256, color.green/256, color.blue/256, 255);
		// gnome_canvas_item_set (canvas_rect, "fill_color_rgba", stream_base_color, NULL);
		break;
	}
}

void
StreamView::set_show_waveforms (bool yn)
{
	for (list<AudioRegionView *>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
			(*i)->set_waveform_visible (yn);
	}
}

void
StreamView::set_selected_regionviews (AudioRegionSelection& regions)
{
	bool selected;

	for (list<AudioRegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		
		selected = false;
		
		for (AudioRegionSelection::iterator ii = regions.begin(); ii != regions.end(); ++ii) {
			if (*i == *ii) {
				selected = true;
			}
		}
		
		(*i)->set_selected (selected, this);
	}
}

void
StreamView::get_selectables (jack_nframes_t start, jack_nframes_t end, list<Selectable*>& results)
{
	for (list<AudioRegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->region.coverage(start, end) != OverlapNone) {
			results.push_back (*i);
		}
	}
}

void
StreamView::get_inverted_selectables (Selection& sel, list<Selectable*>& results)
{
	for (list<AudioRegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if (!sel.audio_regions.contains (*i)) {
			results.push_back (*i);
		}
	}
}

void
StreamView::set_waveform_shape (WaveformShape shape)
{
	for (AudioRegionViewList::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_waveform_shape (shape);
	}
}		
		
void
StreamView::region_layered (AudioRegionView* rv)
{
	rv->get_canvas_group()->lower_to_bottom();

	/* don't ever leave it at the bottom, since then it doesn't
	   get events - the  parent group does instead ...
	*/
	
	rv->get_canvas_group()->raise (rv->region.layer() + 1);
}

void
StreamView::rec_enable_changed (void *src)
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &StreamView::setup_rec_box));
}

void
StreamView::sess_rec_enable_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &StreamView::setup_rec_box));
}

void
StreamView::transport_changed()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &StreamView::setup_rec_box));
}

void
StreamView::setup_rec_box ()
{
	// cerr << _trackview.name() << " streamview SRB\n";

	if (_trackview.session().transport_rolling()) {

		// cerr << "\trolling\n";

		if (!rec_active
		    && _trackview.session().record_status() == Session::Recording
		    && _trackview.get_diskstream()->record_enabled()) {

			if (use_rec_regions && rec_regions.size() == rec_rects.size()) {
				/* add a new region, but don't bother if they set use_rec_regions mid-record */

				AudioRegion::SourceList sources;

				for (list<sigc::connection>::iterator prc = peak_ready_connections.begin(); prc != peak_ready_connections.end(); ++prc) {
					(*prc).disconnect();
				}
				peak_ready_connections.clear();
					
				for (uint32_t n=0; n < _trackview.get_diskstream()->n_channels(); ++n) {
					Source *src = (Source *) _trackview.get_diskstream()->write_source (n);
					if (src) {
						sources.push_back (src);
						peak_ready_connections.push_back (src->PeakRangeReady.connect (bind (mem_fun (*this, &StreamView::rec_peak_range_ready), src))); 
					}
				}

				// handle multi
				
				jack_nframes_t start = 0;
				if (rec_regions.size() > 0) {
					start = rec_regions.back()->start() + _trackview.get_diskstream()->get_captured_frames(rec_regions.size()-1);
				}
				
				AudioRegion * region = new AudioRegion(sources, start, 1 , "", 0, (Region::Flag)(Region::DefaultFlags | Region::DoNotSaveState), false);
				region->set_position (_trackview.session().transport_frame(), this);
				rec_regions.push_back (region);
				/* catch it if it goes away */
				region->GoingAway.connect (mem_fun (*this, &StreamView::remove_rec_region));

				/* we add the region later */
			}
			
			/* start a new rec box */

			AudioTrack* at;

			at = _trackview.audio_track(); /* we know what it is already */
			DiskStream& ds = at->disk_stream();
			jack_nframes_t frame_pos = ds.current_capture_start ();
			gdouble xstart = _trackview.editor.frame_to_pixel (frame_pos);
			gdouble xend = xstart;
			
			ArdourCanvas::SimpleRect * rec_rect = new Gnome::Canvas::SimpleRect (*canvas_group);
			rec_rect->property_x1() = xstart;
			rec_rect->property_y1() = 1.0;
			rec_rect->property_x2() = xend;
			rec_rect->property_y2() = (double) _trackview.height - 1;
			rec_rect->property_outline_color_rgba() = color_map[cRecordingRectOutline];
			rec_rect->property_fill_color_rgba() =  color_map[cRecordingRectFill];
			
			RecBoxInfo recbox;
			recbox.rectangle = rec_rect;
			recbox.start = _trackview.session().transport_frame();
			recbox.length = 0;
			
			rec_rects.push_back (recbox);
			
			screen_update_connection.disconnect();
			screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (mem_fun (*this, &StreamView::update_rec_box));	
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
			for (list<AudioRegion*>::iterator iter=rec_regions.begin(); iter != rec_regions.end(); )
			{
				list<AudioRegion*>::iterator tmp;

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
}


void
StreamView::update_rec_box ()
{
	/* only update the last box */
	if (rec_active && rec_rects.size() > 0) {
		RecBoxInfo & rect = rec_rects.back();
		jack_nframes_t at = _trackview.get_diskstream()->current_capture_end();
		
		rect.length = at - rect.start;

		gdouble xstart = _trackview.editor.frame_to_pixel ( rect.start );
		gdouble xend = _trackview.editor.frame_to_pixel ( at );

		rect.rectangle->property_x1() = xstart;
		rect.rectangle->property_x2() = xend;
	}
}

AudioRegionView*
StreamView::find_view (const AudioRegion& region)
{
	for (list<AudioRegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {

		if (&(*i)->region == &region) {
			return *i;
		}
	}
	return 0;
}
	
void
StreamView::foreach_regionview (sigc::slot<void,AudioRegionView*> slot)
{
	for (list<AudioRegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		slot (*i);
	}
}

void
StreamView::foreach_crossfadeview (void (CrossfadeView::*pmf)(void))
{
	for (list<CrossfadeView*>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		((*i)->*pmf) ();
	}
}

void
StreamView::rec_peak_range_ready (jack_nframes_t start, jack_nframes_t cnt, Source * src)
{
	// this is called from the peak building thread

	ENSURE_GUI_THREAD(bind (mem_fun (*this, &StreamView::rec_peak_range_ready), start, cnt, src));
	
	if (rec_peak_ready_map.size() == 0 || start+cnt > last_rec_peak_frame) {
		last_rec_peak_frame = start + cnt;
	}

	rec_peak_ready_map[src] = true;

	if (rec_peak_ready_map.size() == _trackview.get_diskstream()->n_channels()) {
		this->update_rec_regions ();
		rec_peak_ready_map.clear();
	}
}

void
StreamView::update_rec_regions ()
{
	if (use_rec_regions) {

		uint32_t n = 0;

		for (list<AudioRegion*>::iterator iter = rec_regions.begin(); iter != rec_regions.end(); n++) {

			list<AudioRegion*>::iterator tmp;

			tmp = iter;
			++tmp;

			if (!canvas_item_visible (rec_rects[n].rectangle)) {
				/* rect already hidden, this region is done */
				iter = tmp;
				continue;
			}
			
			AudioRegion * region = (*iter);
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
}

void
StreamView::show_all_xfades ()
{
	foreach_crossfadeview (&CrossfadeView::show);
	crossfades_visible = true;
}

void
StreamView::hide_all_xfades ()
{
	foreach_crossfadeview (&CrossfadeView::hide);
	crossfades_visible = false;
}

void
StreamView::hide_xfades_involving (AudioRegionView& rv)
{
	for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		if ((*i)->crossfade.involves (rv.region)) {
			(*i)->fake_hide ();
		}
	}
}

void
StreamView::reveal_xfades_involving (AudioRegionView& rv)
{
	for (list<CrossfadeView *>::iterator i = crossfade_views.begin(); i != crossfade_views.end(); ++i) {
		if ((*i)->crossfade.involves (rv.region) && (*i)->visible()) {
			(*i)->show ();
		}
	}
}
