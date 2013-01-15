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

#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/operations.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"

#include "ardour_ui.h"
#include "canvas-simplerect.h"
#include "global_signals.h"
#include "gui_thread.h"
#include "lineset.h"
#include "midi_region_view.h"
#include "midi_streamview.h"
#include "midi_time_axis.h"
#include "midi_util.h"
#include "public_editor.h"
#include "region_selection.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "simplerect.h"
#include "utils.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;

MidiStreamView::MidiStreamView (MidiTimeAxisView& tv)
	: StreamView (tv)
	, note_range_adjustment(0.0f, 0.0f, 0.0f)
	, _range_dirty(false)
	, _range_sum_cache(-1.0)
	, _lowest_note(60)
	, _highest_note(71)
	, _data_note_min(60)
	, _data_note_max(71)
	, _note_lines (0)
	, _updates_suspended (false)
{
	/* use a group dedicated to MIDI underlays. Audio underlays are not in this group. */
	midi_underlay_group = new ArdourCanvas::Group (*_canvas_group);
	midi_underlay_group->lower_to_bottom();

	/* put the note lines in the timeaxisview's group, so it
	   can be put below ghost regions from MIDI underlays*/
	_note_lines = new ArdourCanvas::LineSet(*_canvas_group,
	                                        ArdourCanvas::LineSet::Horizontal);

	_note_lines->property_x1() = 0;
	_note_lines->property_y1() = 0;
	_note_lines->property_x2() = DBL_MAX;
	_note_lines->property_y2() = 0;

	_note_lines->signal_event().connect(
		sigc::bind(sigc::mem_fun(_trackview.editor(),
		                         &PublicEditor::canvas_stream_view_event),
		           _note_lines, &_trackview));

	_note_lines->lower_to_bottom();

	color_handler ();

	ColorsChanged.connect(sigc::mem_fun(*this, &MidiStreamView::color_handler));

	note_range_adjustment.set_page_size(_highest_note - _lowest_note);
	note_range_adjustment.set_value(_lowest_note);

	note_range_adjustment.signal_value_changed().connect(
		sigc::mem_fun(*this, &MidiStreamView::note_range_adjustment_changed));
}

MidiStreamView::~MidiStreamView ()
{
}

RegionView*
MidiStreamView::create_region_view (boost::shared_ptr<Region> r, bool /*wfd*/, bool)
{
	boost::shared_ptr<MidiRegion> region = boost::dynamic_pointer_cast<MidiRegion> (r);

	if (region == 0) {
		return 0;
	}

	RegionView* region_view = new MidiRegionView (_canvas_group, _trackview, region,
	                                              _samples_per_unit, region_color);

	region_view->init (region_color, false);

	return region_view;
}

RegionView*
MidiStreamView::add_region_view_internal (boost::shared_ptr<Region> r, bool wfd, bool recording)
{
	boost::shared_ptr<MidiRegion> region = boost::dynamic_pointer_cast<MidiRegion> (r);

	if (region == 0) {
		return 0;
	}

	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->region() == r) {

			/* great. we already have a MidiRegionView for this Region. use it again. */

			(*i)->set_valid (true);

			display_region(dynamic_cast<MidiRegionView*>(*i), wfd);

			return 0;
		}
	}

	MidiRegionView* region_view = dynamic_cast<MidiRegionView*> (create_region_view (r, wfd, recording));
	if (region_view == 0) {
		return 0;
	}

	region_views.push_front (region_view);

	if (_trackview.editor().internal_editing()) {
		region_view->hide_rect ();
	} else {
		region_view->show_rect ();
	}

	/* display events and find note range */
	display_region (region_view, wfd);

	/* fit note range if we are importing */
	if (_trackview.session()->operation_in_progress (Operations::insert_file)) {
		set_note_range (ContentsRange);
	}

	/* catch regionview going away */
	boost::weak_ptr<Region> wr (region); // make this explicit
	region->DropReferences.connect (*this, invalidator (*this), boost::bind (&MidiStreamView::remove_region_view, this, wr), gui_context());

	RegionViewAdded (region_view);

	return region_view;
}

void
MidiStreamView::display_region(MidiRegionView* region_view, bool load_model)
{
	if (!region_view) {
		return;
	}

	region_view->enable_display(true);

	boost::shared_ptr<MidiSource> source(region_view->midi_region()->midi_source(0));

	if (load_model) {
		source->load_model();
	}

	_range_dirty = update_data_note_range(
		source->model()->lowest_note(),
		source->model()->highest_note());

	// Display region contents
	region_view->set_height (child_height());
	region_view->display_model(source->model());
}

void
MidiStreamView::display_track (boost::shared_ptr<Track> tr)
{
	StreamView::display_track (tr);

	draw_note_lines();

	NoteRangeChanged();
}

void
MidiStreamView::update_contents_metrics(boost::shared_ptr<Region> r)
{
	boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(r);
	if (mr) {
		mr->midi_source(0)->load_model();
		_range_dirty = update_data_note_range(
			mr->model()->lowest_note(),
			mr->model()->highest_note());
	}
}

bool
MidiStreamView::update_data_note_range(uint8_t min, uint8_t max)
{
	bool dirty = false;
	if (min < _data_note_min) {
		_data_note_min = min;
		dirty = true;
	}
	if (max > _data_note_max) {
		_data_note_max = max;
		dirty = true;
	}
	return dirty;
}

void
MidiStreamView::redisplay_track ()
{
	if (!_trackview.is_midi_track()) {
		return;
	}

	list<RegionView*>::iterator i;

	// Load models if necessary, and find note range of all our contents
	_range_dirty = false;
	_data_note_min = 127;
	_data_note_max = 0;
	_trackview.track()->playlist()->foreach_region(
		sigc::mem_fun (*this, &StreamView::update_contents_metrics));

	// No notes, use default range
	if (!_range_dirty) {
		_data_note_min = 60;
		_data_note_max = 71;
	}

	// Flag region views as invalid and disable drawing
	for (i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_valid(false);
		(*i)->enable_display(false);
	}

	// Add and display region views, and flag them as valid
	_trackview.track()->playlist()->foreach_region(
		sigc::hide_return (sigc::mem_fun (*this, &StreamView::add_region_view)));

	// Stack regions by layer, and remove invalid regions
	layer_regions();

	// Update note range (not regions which are correct) and draw note lines
	apply_note_range(_lowest_note, _highest_note, false);
}


void
MidiStreamView::update_contents_height ()
{
	StreamView::update_contents_height();
	_note_lines->property_y2() = child_height ();

	apply_note_range (lowest_note(), highest_note(), true);
}

void
MidiStreamView::draw_note_lines()
{
	if (!_note_lines || _updates_suspended) {
		return;
	}

	double y;
	double prev_y = contents_height();
	uint32_t color;

	_note_lines->clear();

	if (child_height() < 140 || note_height() < 3) {
		/* track is too small for note lines, or there are too many */
		return;
	}

	for (int i = lowest_note(); i <= highest_note(); ++i) {
		y = floor(note_to_y(i));

		_note_lines->add_line(prev_y, 1.0, ARDOUR_UI::config()->canvasvar_PianoRollBlackOutline.get());

		switch (i % 12) {
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

		if (i == highest_note()) {
			_note_lines->add_line(y, prev_y - y, color);
		} else {
			_note_lines->add_line(y + 1.0, prev_y - y - 1.0, color);
		}

		prev_y = y;
	}
}

void
MidiStreamView::set_note_range(VisibleNoteRange r)
{
	if (r == FullRange) {
		_lowest_note = 0;
		_highest_note = 127;
	} else {
		_lowest_note = _data_note_min;
		_highest_note = _data_note_max;
	}

	apply_note_range(_lowest_note, _highest_note, true);
}

void
MidiStreamView::apply_note_range(uint8_t lowest, uint8_t highest, bool to_region_views)
{
	_highest_note = highest;
	_lowest_note = lowest;

	int const max_note_height = 20;  // This should probably be based on text size...
	int const range = _highest_note - _lowest_note;
	int const pixels_per_note = floor (child_height () / range);

	/* do not grow note height beyond 10 pixels */
	if (pixels_per_note > max_note_height) {

		int const available_note_range = floor (child_height() / max_note_height);
		int additional_notes = available_note_range - range;

		/* distribute additional notes to higher and lower ranges, clamp at 0 and 127 */
		for (int i = 0; i < additional_notes; i++){

			if (i % 2 && _highest_note < 127){
				_highest_note++;
			}
			else if (i % 2) {
				_lowest_note--;
			}
			else if (_lowest_note > 0){
				_lowest_note--;
			}
			else {
				_highest_note++;
			}
		}
	}

	note_range_adjustment.set_page_size(_highest_note - _lowest_note);
	note_range_adjustment.set_value(_lowest_note);

	draw_note_lines();

	if (to_region_views) {
		apply_note_range_to_regions ();
	}

	NoteRangeChanged();
}

void
MidiStreamView::apply_note_range_to_regions ()
{
	if (!_updates_suspended) {
		for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
			((MidiRegionView*)(*i))->apply_note_range(_lowest_note, _highest_note);
		}
	}
}

void
MidiStreamView::update_note_range(uint8_t note_num)
{
	assert(note_num <= 127);
	_data_note_min = min(_data_note_min, note_num);
	_data_note_max = max(_data_note_max, note_num);
}

void
MidiStreamView::setup_rec_box ()
{
	// cerr << _trackview.name() << " streamview SRB\n";

	if (_trackview.session()->transport_rolling()) {

		if (!rec_active &&
		    _trackview.session()->record_status() == Session::Recording &&
		    _trackview.track()->record_enabled()) {

			if (Config->get_show_waveforms_while_recording() && rec_regions.size() == rec_rects.size()) {

				/* add a new region, but don't bother if they set show-waveforms-while-recording mid-record */

				MidiRegion::SourceList sources;

				rec_data_ready_connections.drop_connections ();

				sources.push_back (_trackview.midi_track()->write_source());

				// handle multi

				framepos_t start = 0;
				if (rec_regions.size() > 0) {
					start = rec_regions.back().first->start()
						+ _trackview.track()->get_captured_frames(rec_regions.size()-1);
				}

				if (!rec_regions.empty()) {
					MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (rec_regions.back().second);
					mrv->end_write ();
				}

				PropertyList plist;

				plist.add (ARDOUR::Properties::start, start);
				plist.add (ARDOUR::Properties::length, 1);
				/* Just above we're setting this nascent region's length to 1.  I think this
				   is so that the RegionView gets created with a non-zero width, as apparently
				   creating a RegionView with a zero width causes it never to be displayed
				   (there is a warning in TimeAxisViewItem::init about this).  However, we
				   must also set length_beats to something non-zero, otherwise the frame length
				   of 1 causes length_beats to be set to some small quantity << 1.  Then
				   when the position is set up below, this length_beats is used to recompute
				   length using BeatsFramesConverter::to, which is slightly innacurate for small
				   beats values because it converts floating point beats to bars, beats and
				   integer ticks.  The upshot of which being that length gets set back to 0,
				   meaning no region view is ever seen, meaning no MIDI notes during record (#3820).
				*/
				plist.add (ARDOUR::Properties::length_beats, 1);
				plist.add (ARDOUR::Properties::name, string());
				plist.add (ARDOUR::Properties::layer, 0);

				boost::shared_ptr<MidiRegion> region (boost::dynamic_pointer_cast<MidiRegion>
				                                      (RegionFactory::create (sources, plist, false)));

				assert(region);
				region->set_start (_trackview.track()->current_capture_start() - _trackview.track()->get_capture_start_frame (0));
				region->set_position (_trackview.track()->current_capture_start());
				RegionView* rv = add_region_view_internal (region, false);
				MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (rv);
				mrv->begin_write ();

				rec_regions.push_back (make_pair (region, rv));

				// rec regions are destroyed in setup_rec_box

				/* we add the region later */

				setup_new_rec_layer_time (region);
			}

			/* start a new rec box */

			boost::shared_ptr<MidiTrack> mt = _trackview.midi_track(); /* we know what it is already */
			framepos_t const frame_pos = mt->current_capture_start ();
			gdouble const xstart = _trackview.editor().frame_to_pixel (frame_pos);
			gdouble const xend = xstart;
			uint32_t fill_color;

			assert(_trackview.midi_track()->mode() == Normal);

			fill_color = ARDOUR_UI::config()->canvasvar_RecordingRect.get();

			ArdourCanvas::SimpleRect * rec_rect = new Gnome::Canvas::SimpleRect (*_canvas_group);
			rec_rect->property_x1() = xstart;
			rec_rect->property_y1() = 1.0;
			rec_rect->property_x2() = xend;
			rec_rect->property_y2() = (double) _trackview.current_height() - 1;
			rec_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_RecordingRect.get();
			rec_rect->property_fill_color_rgba() = fill_color;
			rec_rect->lower_to_bottom();

			RecBoxInfo recbox;
			recbox.rectangle = rec_rect;
			recbox.start = _trackview.session()->transport_frame();
			recbox.length = 0;

			rec_rects.push_back (recbox);

			screen_update_connection.disconnect();
			screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (
				sigc::mem_fun (*this, &MidiStreamView::update_rec_box));
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
MidiStreamView::color_handler ()
{
	draw_note_lines ();

	if (_trackview.is_midi_track()) {
		canvas_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiTrackBase.get();
	} else {
		canvas_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiBusBase.get();;
	}
}

void
MidiStreamView::note_range_adjustment_changed()
{
	double sum = note_range_adjustment.get_value() + note_range_adjustment.get_page_size();
	int lowest = (int) floor(note_range_adjustment.get_value());
	int highest;

	if (sum == _range_sum_cache) {
		//cerr << "cached" << endl;
		highest = (int) floor(sum);
	} else {
		//cerr << "recalc" << endl;
		highest = lowest + (int) floor(note_range_adjustment.get_page_size());
		_range_sum_cache = sum;
	}

	if (lowest == _lowest_note && highest == _highest_note) {
		return;
	}

	//cerr << "note range adjustment changed: " << lowest << " " << highest << endl;
	//cerr << "  val=" << v_zoom_adjustment.get_value() << " page=" << v_zoom_adjustment.get_page_size() << " sum=" << v_zoom_adjustment.get_value() + v_zoom_adjustment.get_page_size() << endl;

	_lowest_note = lowest;
	_highest_note = highest;
	apply_note_range(lowest, highest, true);
}

void
MidiStreamView::update_rec_box ()
{
	StreamView::update_rec_box ();

	if (rec_regions.empty()) {
		return;
	}

	/* Update the region being recorded to reflect where we currently are */
	boost::shared_ptr<ARDOUR::Region> region = rec_regions.back().first;
	region->set_length (_trackview.track()->current_capture_end () - _trackview.track()->current_capture_start());

	MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (rec_regions.back().second);
	mrv->extend_active_notes ();
}

uint8_t
MidiStreamView::y_to_note (double y) const
{
	int const n = ((contents_height() - y - 1) / contents_height() * (double)contents_note_range())
		+ lowest_note();

	if (n < 0) {
		return 0;
	} else if (n > 127) {
		return 127;
	}

	return n;
}

/** Suspend updates to the regions' note ranges and our
 *  note lines until resume_updates() is called.
 */
void
MidiStreamView::suspend_updates ()
{
	_updates_suspended = true;
}

/** Resume updates to region note ranges and note lines,
 *  and update them now.
 */
void
MidiStreamView::resume_updates ()
{
	_updates_suspended = false;

	draw_note_lines ();
	apply_note_range_to_regions ();
}

void
MidiStreamView::leave_internal_edit_mode ()
{
	StreamView::leave_internal_edit_mode ();
	for (RegionViewList::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (*i);
		assert (mrv);
		mrv->clear_selection ();
	}
}
