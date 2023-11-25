/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2016 Nick Mainsbridge <mainsbridge@gmail.com>
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
#include <utility>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include "canvas/rectangle.h"
#include "canvas/line_set.h"

#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/operations.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"
#include "ardour/evoral_types_convert.h"

#include "gui_thread.h"
#include "midi_region_view.h"
#include "midi_streamview.h"
#include "midi_time_axis.h"
#include "midi_util.h"
#include "public_editor.h"
#include "region_selection.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Editing;

MidiStreamView::MidiStreamView (MidiTimeAxisView& tv)
	: StreamView (tv)
	, MidiViewBackground (_canvas_group)
	, _updates_suspended (false)
{
	/* use a dedicated group for MIDI regions (on top of the grid and lines) */
	_region_group = new ArdourCanvas::Container (_canvas_group);
	_region_group->raise_to_top ();
	_region_group->set_render_with_alpha (UIConfiguration::instance().modifier ("region alpha").a());

	/* put the note lines in the timeaxisview's group, so it
	   can be put below ghost regions from MIDI underlays
	*/
	_note_lines->Event.connect(
		sigc::bind(sigc::mem_fun(_trackview.editor(),
		                         &PublicEditor::canvas_stream_view_event),
		           _note_lines, &_trackview));


	color_handler ();

	UIConfiguration::instance().ColorsChanged.connect(sigc::mem_fun(*this, &MidiStreamView::color_handler));
	UIConfiguration::instance().ParameterChanged.connect(sigc::mem_fun(*this, &MidiStreamView::parameter_changed));
}

MidiStreamView::~MidiStreamView ()
{
}

void
MidiStreamView::parameter_changed (string const & param)
{
	StreamView::parameter_changed (param);
}

RegionView*
MidiStreamView::create_region_view (std::shared_ptr<Region> r, bool /*wfd*/, bool recording)
{
	std::shared_ptr<MidiRegion> region = std::dynamic_pointer_cast<MidiRegion> (r);

	if (region == 0) {
		return 0;
	}

	RegionView* region_view = NULL;
	if (recording) {
		region_view = new MidiRegionView (
			_region_group, _trackview.editor(), _trackview, region,
			_samples_per_pixel, region_color, recording,
			TimeAxisViewItem::Visibility(TimeAxisViewItem::ShowFrame));
	} else {
		region_view = new MidiRegionView (_region_group, _trackview.editor(), _trackview, region,
		                                  _samples_per_pixel, region_color);
	}

	region_view->init (false);

	return region_view;
}

RegionView*
MidiStreamView::add_region_view_internal (std::shared_ptr<Region> r, bool wait_for_data, bool recording)
{
	std::shared_ptr<MidiRegion> region = std::dynamic_pointer_cast<MidiRegion> (r);

	if (!region) {
		return 0;
	}

	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->region() == r) {

			/* great. we already have a MidiRegionView for this Region. use it again. */

			(*i)->set_valid (true);

			display_region (dynamic_cast<MidiRegionView*>(*i), wait_for_data);

			return 0;
		}
	}

	MidiRegionView* region_view = dynamic_cast<MidiRegionView*> (create_region_view (r, wait_for_data, recording));
	if (region_view == 0) {
		return 0;
	}

	region_views.push_front (region_view);

	{
		RegionView::DisplaySuspender ds (*region_view, false);

		display_region (region_view, wait_for_data);

		/* fit note range if we are importing */
		if (_trackview.session()->operation_in_progress (Operations::insert_file)) {
			/* this will call display_region() */
			set_note_range (ContentsRange);
		}
	}

	/* catch regionview going away */
	std::weak_ptr<Region> wr (region); // make this explicit
	region->DropReferences.connect (*this, invalidator (*this), boost::bind (&MidiStreamView::remove_region_view, this, wr), gui_context());

	RegionViewAdded (region_view);

	return region_view;
}

void
MidiStreamView::display_region (MidiRegionView* region_view, bool)
{
	if (!region_view) {
		return;
	}

	RegionView::DisplaySuspender ds (*region_view, false);

	region_view->set_height (child_height());

	std::shared_ptr<MidiSource> source (region_view->midi_region()->midi_source(0));

	if (!source) {
		error << _("attempt to display MIDI region with no source") << endmsg;
		return;
	}

	if (!source->model()) {
		error << _("attempt to display MIDI region with no model") << endmsg;
		return;
	}

	_range_dirty = update_data_note_range (source->model()->lowest_note(), source->model()->highest_note());

	// Display region contents
	region_view->display_model (source->model());
}


void
MidiStreamView::display_track (std::shared_ptr<Track> tr)
{
	StreamView::display_track (tr);

	draw_note_lines();

	NoteRangeChanged(); /* EMIT SIGNAL*/
}

void
MidiStreamView::update_contents_metrics(std::shared_ptr<Region> r)
{
	std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion>(r);

	if (mr) {
		Source::ReaderLock lm (mr->midi_source(0)->mutex());
		_range_dirty = update_data_note_range (mr->model()->lowest_note(), mr->model()->highest_note());
	}
}

void
MidiStreamView::set_layer_display (LayerDisplay d)
{

//revert this change for now.  Although stacked view is weirdly implemented wrt the "scroomer", it is still necessary to manage layered midi regions.
//	if (d != Overlaid) {
//		return;
//	}

	StreamView::set_layer_display (d);
	for (auto& rv : region_views) {
		rv->set_frame_color ();
	}
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

	vector<RegionView::DisplaySuspender> vds;

	// Flag region views as invalid and disable drawing
	for (i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_valid (false);
		vds.push_back (RegionView::DisplaySuspender (**i, false));
	}

	// Add and display region views, and flag them as valid
	_trackview.track()->playlist()->foreach_region (sigc::hide_return (sigc::mem_fun (*this, &StreamView::add_region_view)));

	// Stack regions by layer, and remove invalid regions
	layer_regions();

	// Update note range (not regions which are correct) and draw note lines
	apply_note_range (_lowest_note, _highest_note, false);
}

void
MidiStreamView::apply_note_range_to_children ()
{
	if (!_updates_suspended) {
		for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
			((MidiRegionView*)(*i))->apply_note_range(_lowest_note, _highest_note);
		}
	}
}

void
MidiStreamView::setup_rec_box ()
{
	// cerr << _trackview.name() << " streamview SRB\n";

	if (!_trackview.session()->transport_stopped_or_stopping() &&
	    (_trackview.session()->transport_rolling() || _trackview.session()->get_record_enabled())) {

		if (!rec_active &&
		    _trackview.session()->record_status() == Session::Recording &&
		    _trackview.track()->rec_enable_control()->get_value()) {

			if (UIConfiguration::instance().get_show_waveforms_while_recording() && rec_regions.size() == rec_rects.size()) {

				/* add a new region, but don't bother if they set show-waveforms-while-recording mid-record */

				MidiRegion::SourceList sources;

				rec_data_ready_connections.drop_connections ();

				sources.push_back (_trackview.midi_track()->write_source());

				// handle multi

				timepos_t start;
				if (rec_regions.size() > 0) {
					start = rec_regions.back().first->start()
						+ timepos_t (_trackview.track()->get_captured_samples (rec_regions.size() - 1));
				}

				if (!rec_regions.empty()) {
					MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (rec_regions.back().second);
					mrv->end_write ();
				}

				PropertyList plist;

				plist.add (ARDOUR::Properties::start, start);
				plist.add (ARDOUR::Properties::length, timepos_t (Temporal::Beats::ticks (1)));
				/* Just above we're setting this nascent region's length to 1 tick.  I think this is so
				   that the RegionView gets created with a non-zero width, as apparently creating a
				   RegionView with a zero width causes it never to be displayed (there is a warning in
				   TimeAxisViewItem::init about this). We don't want to use 1 sample since that results
				   in zero length musical time duration.
				*/
				plist.add (ARDOUR::Properties::name, string());
				plist.add (ARDOUR::Properties::layer, 0);

				std::shared_ptr<MidiRegion> region (std::dynamic_pointer_cast<MidiRegion>
				                                      (RegionFactory::create (sources, plist, false)));
				if (region) {

					/* MIDI regions should likely not be positioned using audio time, but this is
					 * just a rec-region, so we don't really care
					 */

					region->set_start (timepos_t (_trackview.track()->current_capture_start() - _trackview.track()->get_capture_start_sample (0)));
					region->set_position (timepos_t (_trackview.track()->current_capture_start ()));

					RegionView* rv = add_region_view_internal (region, false, true);
					MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (rv);
					mrv->begin_write ();

					/* rec region will be destroyed in setup_rec_box */
					rec_regions.push_back (make_pair (region, rv));

					/* we add the region later */
					setup_new_rec_layer_time (region);
				} else {
					error << _("failed to create MIDI region") << endmsg;
				}
			}

			/* start a new rec box */

			create_rec_box (_trackview.midi_track()->current_capture_start(), 0);

		} else if (rec_active &&
		           (_trackview.session()->record_status() != Session::Recording ||
		            !_trackview.track()->rec_enable_control()->get_value())) {
			screen_update_connection.disconnect();
			rec_active = false;
			rec_updating = false;
		}

	} else {

		// cerr << "\tNOT rolling, rec_rects = " << rec_rects.size() << " rec_regions = " << rec_regions.size() << endl;
		cleanup_rec_box ();

	}
}

void
MidiStreamView::color_handler ()
{
	MidiViewBackground::color_handler ();

	_region_group->set_render_with_alpha (UIConfiguration::instance().modifier ("region alpha").a());

	if (_trackview.is_midi_track()) {
		canvas_rect->set_fill_color (UIConfiguration::instance().color_mod ("midi track base", "midi track base"));
	} else {
		canvas_rect->set_fill_color (UIConfiguration::instance().color ("midi bus base"));
	}

	for (auto & rv : region_views) {
		rv->color_handler ();
	}
}

void
MidiStreamView::update_rec_box ()
{
	StreamView::update_rec_box ();

	if (rec_regions.empty()) {
		return;
	}

	/* Update the region being recorded to reflect where we currently are */
	std::shared_ptr<ARDOUR::Region> region = rec_regions.back().first;
	region->set_length (timecnt_t (_trackview.track()->current_capture_end () - _trackview.track()->current_capture_start()));

	MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (rec_regions.back().second);
	mrv->extend_active_notes ();
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
	apply_note_range_to_children ();

	_canvas_group->redraw ();
}

struct RegionPositionSorter {
	bool operator() (RegionView* a, RegionView* b) {
		return a->region()->position() < b->region()->position();
	}
};

bool
MidiStreamView::paste (timepos_t const & pos, const Selection& selection, PasteContext& ctx)
{
	/* Paste into the first region which starts on or before pos.  Only called when
	   using an internal editing tool. */

	if (region_views.empty()) {
		return false;
	}

	region_views.sort (RegionView::PositionOrder());

	list<RegionView*>::const_iterator prev = region_views.begin ();

	for (list<RegionView*>::const_iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->region()->position() > pos) {
			break;
		}
		prev = i;
	}

	std::shared_ptr<Region> r = (*prev)->region ();

	/* If *prev doesn't cover pos, it's no good */
	if (r->position() > pos || ((r->position() + r->length()) < pos)) {
		return false;
	}

	MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (*prev);
	return mrv ? mrv->paste(pos, selection, ctx) : false;
}

void
MidiStreamView::get_regions_with_selected_data (RegionSelection& rs)
{
	for (list<RegionView*>::const_iterator i = region_views.begin(); i != region_views.end(); ++i) {
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (*i);

		if (!mrv) {
			continue;
		}

		if (!mrv->selection().empty()) {
			rs.add (*i);
		}
	}
}

