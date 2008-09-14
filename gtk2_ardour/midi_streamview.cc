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
#include <ardour/smf_source.h>
#include <ardour/region_factory.h>

#include "midi_streamview.h"
#include "region_view.h"
#include "midi_region_view.h"
#include "midi_time_axis.h"
#include "canvas-simplerect.h"
#include "region_selection.h"
#include "selection.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "utils.h"
#include "simplerect.h"
#include "lineset.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;

MidiStreamView::MidiStreamView (MidiTimeAxisView& tv)
	: StreamView (tv)
	, note_range_adjustment(0.0f, 0.0f, 0.0f)
	, _range(ContentsRange)
	, _range_sum_cache(-1.0)
	, _lowest_note(60)
	, _highest_note(60)
{
	if (tv.is_track())
		stream_base_color = ARDOUR_UI::config()->canvasvar_MidiTrackBase.get();
	else
		stream_base_color = ARDOUR_UI::config()->canvasvar_MidiBusBase.get();

	use_rec_regions = tv.editor.show_waveforms_recording ();

	/* use a group dedicated to MIDI underlays. Audio underlays are not in this group. */
	midi_underlay_group = new ArdourCanvas::Group (*canvas_group);
	midi_underlay_group->lower_to_bottom();

	/* put the note lines in the timeaxisview's group, so it 
	   can be put below ghost regions from MIDI underlays*/
	_note_lines = new ArdourCanvas::Lineset(*canvas_group, ArdourCanvas::Lineset::Horizontal);

	_note_lines->property_x1() = 0;
	_note_lines->property_y1() = 0;
	_note_lines->property_x2() = trackview().editor.frame_to_pixel (max_frames);
	_note_lines->property_y2() = 0;

	_note_lines->signal_event().connect (bind (mem_fun (_trackview.editor, &PublicEditor::canvas_stream_view_event), _note_lines, &_trackview));
	_note_lines->lower_to_bottom();

	note_range_adjustment.signal_value_changed().connect (mem_fun (*this, &MidiStreamView::note_range_adjustment_changed));
	ColorsChanged.connect(mem_fun(*this, &MidiStreamView::draw_note_lines));
}

MidiStreamView::~MidiStreamView ()
{
}


RegionView*
MidiStreamView::add_region_view_internal (boost::shared_ptr<Region> r, bool wfd, bool recording)
{
	boost::shared_ptr<MidiRegion> region = boost::dynamic_pointer_cast<MidiRegion> (r);

	if (region == 0) {
		return NULL;
	}

	MidiRegionView *region_view;
	list<RegionView *>::iterator i;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->region() == r) {
			
			/* great. we already have a MidiRegionView for this Region. use it again. */

			(*i)->set_valid (true);
			(*i)->enable_display(wfd);
			display_region(dynamic_cast<MidiRegionView*>(*i), wfd);

			return NULL;
		}
	}
	
	region_view = new MidiRegionView (canvas_group, _trackview, region, 
			_samples_per_unit, region_color);
		
	region_view->init (region_color, false);
	region_views.push_front (region_view);
	
	/* follow global waveform setting */

	if (wfd) {
		region_view->enable_display(true);
		region_view->midi_region()->midi_source(0)->load_model();
	}

	/* display events and find note range */
	display_region(region_view, wfd);

	/* always display at least 1 octave range */
	_highest_note = max(_highest_note, static_cast<uint8_t>(_lowest_note + 11));

	/* catch regionview going away */
	region->GoingAway.connect (bind (mem_fun (*this, &MidiStreamView::remove_region_view), region));
	
	RegionViewAdded (region_view);

	return region_view;
}

void
MidiStreamView::display_region(MidiRegionView* region_view, bool load_model)
{
	if ( ! region_view)
		return;

	boost::shared_ptr<MidiSource> source(region_view->midi_region()->midi_source(0));

	if (load_model)
		source->load_model();

	// Find our note range
	if (source->model())
		for (size_t i=0; i < source->model()->n_notes(); ++i)
			update_bounds(source->model()->note_at(i)->note());
	
	// Display region contents
	region_view->display_model(source->model());
}

void
MidiStreamView::display_diskstream (boost::shared_ptr<Diskstream> ds)
{
	StreamView::display_diskstream(ds);
	draw_note_lines();
	NoteRangeChanged();
}

// FIXME: code duplication with AudioStreamView
void
MidiStreamView::redisplay_diskstream ()
{
	list<RegionView *>::iterator i, tmp;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->enable_display(true); // FIXME: double display, remove
		(*i)->set_valid (false);
		
		/* FIXME: slow.  MidiRegionView needs a find_note_range method
		 * that finds the range without wasting time drawing the events */

		// Load model if it isn't already, to get note range
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
		mrv->midi_region()->midi_source(0)->load_model();
	}
	
	if (_trackview.is_midi_track()) {
		_trackview.get_diskstream()->playlist()->foreach_region (
				static_cast<StreamView*>(this), &StreamView::add_region_view);
	}

	/* Always display at least one octave */
	if (_highest_note == 127) {
		if (_lowest_note > (127 - 11)) {
			_lowest_note = 127 - 11;
		}
	} else if (_highest_note < _lowest_note + 11) {
		_highest_note = _lowest_note + 11;
	}
	
	RegionViewList copy;
	
	/* Place regions */
	for (i = region_views.begin(); i != region_views.end(); ) {
		tmp = i;
		tmp++;

		if (!(*i)->is_valid()) {
			delete *i;
			region_views.erase (i);
			i = tmp;
			continue;
		} else {
			(*i)->enable_display(true);
			(*i)->set_y_position_and_height(0, height); // apply note range
		}
		
		/* Sort regionviews by layer so that when we call region_layered ()
		   the canvas layering works out (in non-stacked mode). */

		if (copy.size() == 0) {
			copy.push_front((*i));
			i = tmp;
			continue;
		}

		RegionViewList::iterator k = copy.begin();
		RegionViewList::iterator l = copy.end();
		l--;

		if ((*i)->region()->layer() <= (*k)->region()->layer()) {
			copy.push_front((*i));
			i = tmp;
			continue;
		} else if ((*i)->region()->layer() >= (*l)->region()->layer()) {
			copy.push_back((*i));
			i = tmp;
			continue;
		}

		for (RegionViewList::iterator j = copy.begin(); j != copy.end(); ++j) {
			if ((*j)->region()->layer() >= (*i)->region()->layer()) {
				copy.insert(j, (*i));
				break;
			}
		}

		i = tmp;
	}
	
	/* Fix canvas layering */
	for (RegionViewList::iterator j = copy.begin(); j != copy.end(); ++j) {
		(*j)->enable_display(true);
		(*j)->set_height (height);
		region_layered (*j);
	}
	
	/* Update note range and draw note lines */
	note_range_adjustment.set_page_size(_highest_note - _lowest_note);
	note_range_adjustment.set_value(_lowest_note);
	NoteRangeChanged();
	draw_note_lines();
}


void
MidiStreamView::update_contents_y_position_and_height ()
{
	StreamView::update_contents_y_position_and_height();
	_note_lines->property_y2() = height;
	draw_note_lines();
}
	
void
MidiStreamView::draw_note_lines()
{
	double y;
	double prev_y = contents_height();
	uint32_t color;

	_note_lines->clear();

	for(int i = _lowest_note; i <= _highest_note; ++i) {
		y = floor(note_to_y(i));
		
		_note_lines->add_line(prev_y, 1.0, ARDOUR_UI::config()->canvasvar_PianoRollBlackOutline.get());

		switch(i % 12) {
		case 1:
		case 3:
		case 6:
		case 8:
		case 10:
			color = ARDOUR_UI::config()->canvasvar_PianoRollBlack.get();
			break;
		default:
			color = ARDOUR_UI::config()->canvasvar_PianoRollWhite.get();
			break;
		}

		if(i == _highest_note) {
			_note_lines->add_line(y, prev_y - y, color);
		}
		else {
			_note_lines->add_line(y + 1.0, prev_y - y - 1.0, color);
		}

		prev_y = y;
	}
}
	

void
MidiStreamView::set_note_range(VisibleNoteRange r)
{
	_range = r;
	if (r == FullRange) {
		_lowest_note = 0;
		_highest_note = 127;
	} else {
		_lowest_note = 60;
		_highest_note = 60;
	}
	redisplay_diskstream();
}

void
MidiStreamView::set_note_range(uint8_t lowest, uint8_t highest) {
	if(_range == ContentsRange) {
		_lowest_note = lowest;
		_highest_note = highest;

		list<RegionView *>::iterator i;
		for (i = region_views.begin(); i != region_views.end(); ++i) {
			(*i)->set_y_position_and_height(0, height); // apply note range
		}
	}

	draw_note_lines();
	NoteRangeChanged();
}
	
void 
MidiStreamView::update_bounds(uint8_t note_num)
{
	_lowest_note = min(_lowest_note, note_num);
	_highest_note = max(_highest_note, note_num);
}


void
MidiStreamView::setup_rec_box ()
{
	// cerr << _trackview.name() << " streamview SRB\n";

	if (_trackview.session().transport_rolling()) {

		if (!rec_active && 
		    _trackview.session().record_status() == Session::Recording && 
		    _trackview.get_diskstream()->record_enabled()) {

			if (use_rec_regions && rec_regions.size() == rec_rects.size()) {

				/* add a new region, but don't bother if they set use_rec_regions mid-record */

				MidiRegion::SourceList sources;
				
				for (list<sigc::connection>::iterator prc = rec_data_ready_connections.begin(); prc != rec_data_ready_connections.end(); ++prc) {
					(*prc).disconnect();
				}
				rec_data_ready_connections.clear();

				// FIXME
				boost::shared_ptr<MidiDiskstream> mds = boost::dynamic_pointer_cast<MidiDiskstream>(_trackview.get_diskstream());
				assert(mds);

				sources.push_back(mds->write_source());
				
				rec_data_ready_connections.push_back (mds->write_source()->ViewDataRangeReady.connect (bind (mem_fun (*this, &MidiStreamView::rec_data_range_ready), boost::weak_ptr<Source>(mds->write_source())))); 

				// handle multi
				
				jack_nframes_t start = 0;
				if (rec_regions.size() > 0) {
					start = rec_regions.back().first->position() + _trackview.get_diskstream()->get_captured_frames(rec_regions.size()-1);
				}
				
				boost::shared_ptr<MidiRegion> region (boost::dynamic_pointer_cast<MidiRegion>
					(RegionFactory::create (sources, start, 1 , "", 0, (Region::Flag)(Region::DefaultFlags | Region::DoNotSaveState), false)));
				assert(region);
				region->set_position (_trackview.session().transport_frame(), this);
				rec_regions.push_back (make_pair(region, (RegionView*)0));
				
				// rec regions are destroyed in setup_rec_box

				/* we add the region later */
			}
			
			/* start a new rec box */

			boost::shared_ptr<MidiTrack> mt = _trackview.midi_track(); /* we know what it is already */
			boost::shared_ptr<MidiDiskstream> ds = mt->midi_diskstream();
			jack_nframes_t frame_pos = ds->current_capture_start ();
			gdouble xstart = _trackview.editor.frame_to_pixel (frame_pos);
			gdouble xend;
			uint32_t fill_color;

			assert(_trackview.midi_track()->mode() == Normal);
			
			xend = xstart;
			fill_color = ARDOUR_UI::config()->canvasvar_RecordingRect.get();
			
			ArdourCanvas::SimpleRect * rec_rect = new Gnome::Canvas::SimpleRect (*canvas_group);
			rec_rect->property_x1() = xstart;
			rec_rect->property_y1() = 1.0;
			rec_rect->property_x2() = xend;
			rec_rect->property_y2() = (double) _trackview.current_height() - 1;
			rec_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_RecordingRect.get();
			rec_rect->property_fill_color_rgba() = fill_color;
			rec_rect->lower_to_bottom();
			
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

			for (list<sigc::connection>::iterator prc = rec_data_ready_connections.begin(); prc != rec_data_ready_connections.end(); ++prc) {
				(*prc).disconnect();
			}
			rec_data_ready_connections.clear();

			rec_updating = false;
			rec_active = false;
			
			/* remove temp regions */
			
			for (list<pair<boost::shared_ptr<Region>,RegionView*> >::iterator iter = rec_regions.begin(); iter != rec_regions.end();) {
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
MidiStreamView::update_rec_regions (boost::shared_ptr<MidiModel> data, nframes_t start, nframes_t dur)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &MidiStreamView::update_rec_regions), data, start, dur));

	if (use_rec_regions) {

		uint32_t n = 0;
		bool     update_range = false;

		for (list<pair<boost::shared_ptr<Region>,RegionView*> >::iterator iter = rec_regions.begin(); iter != rec_regions.end(); n++) {

			list<pair<boost::shared_ptr<Region>,RegionView*> >::iterator tmp;

			tmp = iter;
			++tmp;
			
			boost::shared_ptr<MidiRegion> region = boost::dynamic_pointer_cast<MidiRegion>(iter->first);
			if (!region || !iter->second) {
				iter = tmp;
				continue;
			}
			
			if (!canvas_item_visible (rec_rects[n].rectangle)) {
				/* rect already hidden, this region is done */
				iter = tmp;
				continue;
			}
			
			nframes_t origlen = region->length();
			
			if (region == rec_regions.back().first && rec_active) {

				if (start >= region->midi_source(0)->timeline_position()) {
				
					nframes_t nlen = start + dur - region->position();

					if (nlen != region->length()) {
					
						region->freeze ();
						region->set_position (_trackview.get_diskstream()->get_capture_start_frame(n), this);
						region->set_length (start + dur - region->position(), this);
						region->thaw ("updated");
						
						if (origlen == 1) {
							/* our special initial length */
							iter->second = add_region_view_internal (region, false);
							((MidiRegionView*)iter->second)->begin_write();
						}

						/* also update rect */
						ArdourCanvas::SimpleRect * rect = rec_rects[n].rectangle;
						gdouble xend = _trackview.editor.frame_to_pixel (region->position() + region->length());
						rect->property_x2() = xend;

						/* draw events */
						MidiRegionView* mrv = (MidiRegionView*)iter->second;
						for (size_t i=0; i < data->n_notes(); ++i) {

							// FIXME: slooooooooow!

							const boost::shared_ptr<Note> note = data->note_at(i);
							
							if (note->duration() > 0 && note->end_time() + region->position() > start)
								mrv->resolve_note(note->note(), note->end_time());

							if (note->time() + region->position() < start)
								continue;

							if (note->time() + region->position() > start + dur)
								break;

							mrv->add_note(note);

							if (note->note() < _lowest_note) {
								_lowest_note = note->note();
								update_range = true;
							} else if (note->note() > _highest_note) {
								_highest_note = note->note();
								update_range = true;
							}
						}
						
						mrv->extend_active_notes();
					}
				}

			} else {
				
				nframes_t nlen = _trackview.get_diskstream()->get_captured_frames(n);

				if (nlen != region->length()) {

					if (region->source(0)->length() >= region->position() + nlen) {

						region->freeze ();
						region->set_position (_trackview.get_diskstream()->get_capture_start_frame(n), this);
						region->set_length (nlen, this);
						region->thaw ("updated");
						
						if (origlen == 1) {
							/* our special initial length */
							iter->second = add_region_view_internal (region, false);
						}
						
						/* also hide rect */
						ArdourCanvas::Item * rect = rec_rects[n].rectangle;
						rect->hide();

					}
				}
			}

			iter = tmp;
		}

		if (update_range)
			update_contents_y_position_and_height();
	}
}

void
MidiStreamView::rec_data_range_ready (jack_nframes_t start, jack_nframes_t dur, boost::weak_ptr<Source> weak_src)
{
	// this is called from the butler thread for now
	
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &MidiStreamView::rec_data_range_ready), start, dur, weak_src));
	
	boost::shared_ptr<SMFSource> src (boost::dynamic_pointer_cast<SMFSource>(weak_src.lock()));
	
	this->update_rec_regions (src->model(), start, dur);
}

void
MidiStreamView::color_handler ()
{

	//case cMidiTrackBase:
	if (_trackview.is_midi_track()) {
		//canvas_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiTrackBase.get();
	} 

	//case cMidiBusBase:
	if (!_trackview.is_midi_track()) {
		//canvas_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiBusBase.get();;
	}
}

void
MidiStreamView::note_range_adjustment_changed() {
	double sum = note_range_adjustment.get_value() + note_range_adjustment.get_page_size();
	int lowest = (int) floor(note_range_adjustment.get_value());
	int highest;

	if(sum == _range_sum_cache) {
		//cerr << "cached" << endl;
		highest = (int) floor(sum);
	}
	else {
		//cerr << "recalc" << endl;
		highest = lowest + (int) floor(note_range_adjustment.get_page_size());
		_range_sum_cache = sum;
	}

	if(lowest == lowest_note() && highest == highest_note()) {
		return;
	}

	//cerr << "note range changed: " << lowest << " " << highest << endl;
	//cerr << "  val=" << v_zoom_adjustment.get_value() << " page=" << v_zoom_adjustment.get_page_size() << " sum=" << v_zoom_adjustment.get_value() + v_zoom_adjustment.get_page_size() << endl;

	set_note_range(lowest, highest);
}
