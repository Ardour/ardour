/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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
#include <algorithm>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include "temporal/tempo.h"

#include "ardour/playlist.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/source.h"

#include "gtkmm2ext/colors.h"

#include "canvas/arrow.h"
#include "canvas/polygon.h"
#include "canvas/debug.h"
#include "canvas/pixbuf.h"
#include "canvas/text.h"
#include "canvas/line.h"

#include "streamview.h"
#include "region_view.h"
#include "automation_region_view.h"
#include "route_time_axis.h"
#include "public_editor.h"
#include "region_editor.h"
#include "ghostregion.h"
#include "route_time_axis.h"
#include "ui_config.h"
#include "utils.h"
#include "rgb_macros.h"
#include "gui_thread.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Editing;
using namespace Gtk;
using namespace ArdourCanvas;

static const int32_t sync_mark_width = 9;

PBD::Signal1<void,RegionView*> RegionView::RegionViewGoingAway;

RegionView::RegionView (ArdourCanvas::Container*          parent,
                        TimeAxisView&                     tv,
                        boost::shared_ptr<ARDOUR::Region> r,
                        double                            spu,
                        uint32_t                          basic_color,
                        bool                              automation)
	: TimeAxisViewItem (r->name(), *parent, tv, spu, basic_color, r->position(), r->length(), false, automation,
			    (automation ? TimeAxisViewItem::ShowFrame :
			     TimeAxisViewItem::Visibility ((UIConfiguration::instance().get_show_region_name() ? TimeAxisViewItem::ShowNameText : 0) |
							   TimeAxisViewItem::ShowNameHighlight| TimeAxisViewItem::ShowFrame)))
	, _region (r)
	, sync_mark(0)
	, sync_line(0)
	, editor(0)
	, current_visible_sync_position(0.0)
	, valid(false)
	, _enable_display(false)
	, _pixel_width(1.0)
	, in_destructor(false)
	, wait_for_data(false)
	, _silence_text (0)
	, _xrun_markers_visible (false)
	, _cue_markers_visible (false)
{
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &RegionView::parameter_changed));

	for (SourceList::const_iterator s = _region->sources().begin(); s != _region->sources().end(); ++s) {
		(*s)->CueMarkersChanged.connect (*this, invalidator (*this), boost::bind (&RegionView::update_cue_markers, this), gui_context());
	}
}

RegionView::RegionView (const RegionView& other)
	: sigc::trackable(other)
	, TimeAxisViewItem (other)
	, _silence_text (0)
	, _xrun_markers_visible (false)
	, _cue_markers_visible (false)
{
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &RegionView::parameter_changed));

	/* derived concrete type will call init () */

	_region = other._region;
	current_visible_sync_position = other.current_visible_sync_position;
	valid = false;
	_pixel_width = other._pixel_width;

	for (SourceList::const_iterator s = _region->sources().begin(); s != _region->sources().end(); ++s) {
		(*s)->CueMarkersChanged.connect (*this, invalidator (*this), boost::bind (&RegionView::update_cue_markers, this), gui_context());
	}
}

RegionView::RegionView (const RegionView& other, boost::shared_ptr<Region> other_region)
	: sigc::trackable(other)
	, TimeAxisViewItem (other)
	, _silence_text (0)
	, _xrun_markers_visible (false)
	, _cue_markers_visible (false)
{
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &RegionView::parameter_changed));

	/* derived concrete type will call init () */
	/* this is a pseudo-copy constructor used when dragging regions
	   around on the canvas.
	*/

	/* derived concrete type will call init () */

	_region = other_region;
	current_visible_sync_position = other.current_visible_sync_position;
	valid = false;
	_pixel_width = other._pixel_width;

	for (SourceList::const_iterator s = _region->sources().begin(); s != _region->sources().end(); ++s) {
		(*s)->CueMarkersChanged.connect (*this, invalidator (*this), boost::bind (&RegionView::update_cue_markers, this), gui_context());
	}
}


RegionView::RegionView (ArdourCanvas::Container*          parent,
                        TimeAxisView&                     tv,
                        boost::shared_ptr<ARDOUR::Region> r,
                        double                            spu,
                        uint32_t                          basic_color,
                        bool                              recording,
                        TimeAxisViewItem::Visibility      visibility)
	: TimeAxisViewItem (r->name(), *parent, tv, spu, basic_color, r->position(), r->length(), recording, false, visibility)
	, _region (r)
	, sync_mark(0)
	, sync_line(0)
	, editor(0)
	, current_visible_sync_position(0.0)
	, valid(false)
	, _enable_display(false)
	, _pixel_width(1.0)
	, in_destructor(false)
	, wait_for_data(false)
	, _silence_text (0)
{
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &RegionView::parameter_changed));

	for (SourceList::const_iterator s = _region->sources().begin(); s != _region->sources().end(); ++s) {
		(*s)->CueMarkersChanged.connect (*this, invalidator (*this), boost::bind (&RegionView::update_cue_markers, this), gui_context());
	}
}

void
RegionView::init (bool wfd)
{
	editor        = 0;
	valid         = true;
	in_destructor = false;
	wait_for_data = wfd;
	sync_mark     = 0;
	sync_line     = 0;
	sync_mark     = 0;
	sync_line     = 0;

	if (name_highlight) {
		name_highlight->set_data ("regionview", this);
		name_highlight->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_region_view_name_highlight_event), name_highlight, this));
	}

	if (frame_handle_start) {
		frame_handle_start->set_data ("regionview", this);
		frame_handle_start->set_data ("isleft", (void*) 1);
		frame_handle_start->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_frame_handle_event), frame_handle_start, this));
		frame_handle_start->raise_to_top();
	}

	if (frame_handle_end) {
		frame_handle_end->set_data ("regionview", this);
		frame_handle_end->set_data ("isleft", (void*) 0);
		frame_handle_end->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_frame_handle_event), frame_handle_end, this));
		frame_handle_end->raise_to_top();
	}

	if (name_text) {
		name_text->set_data ("regionview", this);
		name_text->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_region_view_name_event), name_text, this));
	}

	XrunPositions xrp;
	_region->captured_xruns (xrp, true);
	int arrow_size = (int)(7.0 * UIConfiguration::instance ().get_ui_scale ()) & ~1;
	for (XrunPositions::const_iterator x = xrp.begin (); x != xrp.end (); ++x) {
		ArdourCanvas::Arrow* canvas_item = new ArdourCanvas::Arrow(group);
		canvas_item->set_color (UIConfiguration::instance().color ("neutral:background"));
		canvas_item->set_show_head (1, true);
		canvas_item->set_show_head (0, false);
		canvas_item->set_head_width (1, arrow_size);
		canvas_item->set_head_height (1, arrow_size);
		canvas_item->set_y0 (arrow_size);
		canvas_item->set_y1 (arrow_size);
		canvas_item->raise_to_top ();
		canvas_item->hide ();
		_xrun_markers.push_back (make_pair(*x, canvas_item));
	}

	_xrun_markers_visible = false;
	update_xrun_markers ();

	_cue_markers_visible = false;
	update_cue_markers ();

	if (wfd) {
		_enable_display = true;
	}

	/* derived class calls set_height () including RegionView::set_height() in ::init() */
	//set_height (trackview.current_height());

	_region->PropertyChanged.connect (*this, invalidator (*this), boost::bind (&RegionView::region_changed, this, _1), gui_context());

	/* derived class calls set_colors () including RegionView::set_colors() in ::init() */
	//set_colors ();
	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &RegionView::color_handler));

	/* XXX sync mark drag? */
}

RegionView::~RegionView ()
{
	in_destructor = true;

	for (vector<GhostRegion*>::iterator g = ghosts.begin(); g != ghosts.end(); ++g) {
		delete *g;
	}

	for (list<ArdourCanvas::Rectangle*>::iterator i = _coverage_frame.begin (); i != _coverage_frame.end (); ++i) {
		delete *i;
	}

	for (list<std::pair<samplepos_t, ArdourCanvas::Arrow*> >::iterator i = _xrun_markers.begin(); i != _xrun_markers.end(); ++i) {
		delete ((*i).second);
	}

	for (ViewCueMarkers::iterator c = _cue_markers.begin(); c != _cue_markers.end(); ++c) {
		delete (*c);
	}

	drop_silent_frames ();

	delete editor;
}

bool
RegionView::canvas_group_event (GdkEvent* event)
{
	if (!in_destructor) {
		return trackview.editor().canvas_region_view_event (event, group, this);
	}
	return false;
}

void
RegionView::set_silent_frames (const AudioIntervalResult& silences, double /*threshold*/)
{
	samplecnt_t shortest = max_samplecnt;

	/* remove old silent frames */
	drop_silent_frames ();

	if (silences.empty()) {
		return;
	}

	uint32_t const color = UIConfiguration::instance().color_mod ("silence", "silence");

	for (AudioIntervalResult::const_iterator i = silences.begin(); i != silences.end(); ++i) {

		ArdourCanvas::Rectangle* cr = new ArdourCanvas::Rectangle (group);
		cr->set_ignore_events (true);
		_silent_frames.push_back (cr);

		/* coordinates for the rect are relative to the regionview origin */

		cr->set_x0 (trackview.editor().sample_to_pixel (i->first - _region->start_sample()));
		cr->set_x1 (trackview.editor().sample_to_pixel (i->second - _region->start_sample()));
		cr->set_y0 (1);
		cr->set_y1 (_height - 2);
		cr->set_outline (false);
		cr->set_fill_color (color);

		shortest = min (shortest, i->second - i->first);
	}

	/* Find shortest audible segment */
	samplecnt_t shortest_audible = max_samplecnt;

	samplecnt_t s = _region->start_sample();
	for (AudioIntervalResult::const_iterator i = silences.begin(); i != silences.end(); ++i) {
		samplecnt_t const dur = i->first - s;
		if (dur > 0) {
			shortest_audible = min (shortest_audible, dur);
		}

		s = i->second;
	}

	samplecnt_t const dur = _region->start_sample() + _region->length_samples() - 1 - s;
	if (dur > 0) {
		shortest_audible = min (shortest_audible, dur);
	}

	_silence_text = new ArdourCanvas::Text (group);
	_silence_text->set_ignore_events (true);
	_silence_text->set_font_description (get_font_for_style (N_("SilenceText")));
	_silence_text->set_color (UIConfiguration::instance().color ("silence text"));

	/* both positions are relative to the region start offset in source */

	_silence_text->set_x_position (trackview.editor().sample_to_pixel (silences.front().first - _region->start_sample()) + 10.0);
	_silence_text->set_y_position (20.0);

	double ms = (float) shortest/_region->session().sample_rate();

	/* ms are now in seconds */

	char const * sunits;

	if (ms >= 60.0) {
		sunits = _("minutes");
		ms /= 60.0;
	} else if (ms < 1.0) {
		sunits = _("msecs");
		ms *= 1000.0;
	} else {
		sunits = _("secs");
	}

	string text = string_compose (ngettext ("%1 silent segment", "%1 silent segments", silences.size()), silences.size())
		+ ", "
		+ string_compose (_("shortest = %1 %2"), ms, sunits);

	if (shortest_audible != max_samplepos) {
		/* ms are now in seconds */
		double ma = (float) shortest_audible / _region->session().sample_rate();
		char const * aunits;

		if (ma >= 60.0) {
			aunits = _("minutes");
			ma /= 60.0;
		} else if (ma < 1.0) {
			aunits = _("msecs");
			ma *= 1000.0;
		} else {
			aunits = _("secs");
		}

		text += string_compose (_("\n  (shortest audible segment = %1 %2)"), ma, aunits);
	}

	_silence_text->set (text);
}

void
RegionView::hide_silent_frames ()
{
	for (list<ArdourCanvas::Rectangle*>::iterator i = _silent_frames.begin (); i != _silent_frames.end (); ++i) {
		(*i)->hide ();
	}
	_silence_text->hide();
}

void
RegionView::drop_silent_frames ()
{
	for (list<ArdourCanvas::Rectangle*>::iterator i = _silent_frames.begin (); i != _silent_frames.end (); ++i) {
		delete *i;
	}
	_silent_frames.clear ();

	delete _silence_text;
	_silence_text = 0;
}

gint
RegionView::_lock_toggle (ArdourCanvas::Item*, GdkEvent* ev, void* arg)
{
	switch (ev->type) {
	case GDK_BUTTON_RELEASE:
		static_cast<RegionView*>(arg)->lock_toggle ();
		return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

void
RegionView::lock_toggle ()
{
	_region->set_locked (!_region->locked());
}

void
RegionView::region_changed (const PropertyChange& what_changed)
{
	ENSURE_GUI_THREAD (*this, &RegionView::region_changed, what_changed);

	if (what_changed.contains (ARDOUR::bounds_change)) {
		region_resized (what_changed);
		region_sync_changed ();
		update_cue_markers ();
	}
	if (what_changed.contains (ARDOUR::Properties::muted)) {
		region_muted ();
	}
	if (what_changed.contains (ARDOUR::Properties::opaque)) {
		region_opacity ();
	}
	if (what_changed.contains (ARDOUR::Properties::name)) {
		region_renamed ();
	}
	if (what_changed.contains (ARDOUR::Properties::time_domain)) {
		region_renamed ();
	}
	if (what_changed.contains (ARDOUR::Properties::sync_position)) {
		region_sync_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::locked)) {
		region_locked ();
	}
}

void
RegionView::region_locked ()
{
	/* name will show locked status */
	region_renamed ();
}

void
RegionView::region_resized (const PropertyChange& what_changed)
{
	double unit_length;

	/* position is stored in length */

	if (what_changed.contains (ARDOUR::Properties::length)) {
		set_position (_region->position(), 0);
	}

	PropertyChange s_and_l;
	s_and_l.add (ARDOUR::Properties::start);
	s_and_l.add (ARDOUR::Properties::length);

	if (what_changed.contains (s_and_l)) {

		set_duration (_region->length(), 0);

		unit_length = _region->length_samples() / samples_per_pixel;

		for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
			(*i)->set_duration (unit_length);
		}

		update_xrun_markers ();
	}
}

void
RegionView::reset_width_dependent_items (double pixel_width)
{
	TimeAxisViewItem::reset_width_dependent_items (pixel_width);
	_pixel_width = pixel_width;

	if (_xrun_markers_visible) {
		const samplepos_t start = _region->start_sample();
		for (list<std::pair<samplepos_t, ArdourCanvas::Arrow*> >::iterator i = _xrun_markers.begin(); i != _xrun_markers.end(); ++i) {
			float x_pos = trackview.editor().sample_to_pixel (i->first - start);
			i->second->set_x (x_pos);
		}
	}
}

void
RegionView::update_xrun_markers ()
{
	const bool show_xruns_markers = UIConfiguration::instance().get_show_region_xrun_markers();
	if (_xrun_markers_visible == show_xruns_markers && !_xrun_markers_visible) {
		return;
	}

	const samplepos_t start = _region->start_sample();
	const samplepos_t length = _region->length_samples();
	for (list<std::pair<samplepos_t, ArdourCanvas::Arrow*> >::iterator i = _xrun_markers.begin(); i != _xrun_markers.end(); ++i) {
		float x_pos = trackview.editor().sample_to_pixel (i->first - start);
		i->second->set_x (x_pos);
		if (show_xruns_markers && (i->first >= start && i->first < start + length)) {
			i->second->show ();
		} else  {
			i->second->hide ();
		}
	}
	_xrun_markers_visible = show_xruns_markers;
}

RegionView::ViewCueMarker::~ViewCueMarker ()
{
	delete view_marker;
}

void
RegionView::update_cue_markers ()
{
	const bool show_cue_markers = UIConfiguration::instance().get_show_region_cue_markers();

	if (_cue_markers_visible == show_cue_markers && !_cue_markers_visible) {
		return;
	}

	const timepos_t start = region()->start();
	const timepos_t end = region()->start() + region()->length();
	const Gtkmm2ext::SVAModifier alpha = UIConfiguration::instance().modifier (X_("region mark"));
	const uint32_t color = Gtkmm2ext::HSV (UIConfiguration::instance().color ("region mark")).mod (alpha).color();

	/* We assume that if the region has multiple sources, any of them will
	 * be appropriate as the origin of cue markers. We use the first one.
	 */

	boost::shared_ptr<Source> source = region()->source (0);
	CueMarkers const & model_markers (source->cue_markers());

	/* Remove any view markers that are no longer present in the model cue
	 * marker set
	 */

	for (ViewCueMarkers::iterator v = _cue_markers.begin(); v != _cue_markers.end(); ) {
		if (model_markers.find ((*v)->model_marker) == model_markers.end()) {
			delete *v;
			v = _cue_markers.erase (v);
		} else {
			++v;
		}
	}

	/* now check all the model markers and make sure we have view markers
	 * for them. Note that because we use Source::cue_markers() above the
	 * set will contain markers stamped with absolute, not region-relative,
	 * timestamps and some of them may be outside the Region.
	 */

	for (CueMarkers::const_iterator c = model_markers.begin(); c != model_markers.end(); c++) {

		ViewCueMarkers::iterator existing = _cue_markers.end();

		for (ViewCueMarkers::iterator v = _cue_markers.begin(); v != _cue_markers.end(); ++v) {
			if ((*v)->model_marker == *c) {
				existing = v;
				break;
			}
		}

		if (existing == _cue_markers.end()) {

			if (c->position() < start || c->position() >= end) {
				/* not within this region */
				continue;
			}

			/* Create a new ViewCueMarker */

			ArdourMarker* mark = new ArdourMarker (trackview.editor(), *group, color , c->text(), ArdourMarker::RegionCue, timepos_t (start.distance (c->position())), true, this);
			mark->set_points_color (color);
			mark->set_show_line (true);
			/* make sure the line has a clean end, before the frame
			   of the region view
			*/
			mark->set_line_height (trackview.current_height() - (1.5 * UIConfiguration::instance ().get_ui_scale ()));
			mark->the_item().raise_to_top ();

			if (show_cue_markers) {
				mark->show ();
			} else  {
				mark->hide ();
			}

			_cue_markers.push_back (new ViewCueMarker (mark, *c));

		} else {

			if (c->position() < start || c->position() >= end) {
				/* not within this region */
				delete (*existing);
				_cue_markers.erase (existing);
				continue;
			}

			/* Move and control visibility for an existing ViewCueMarker */

			if (show_cue_markers) {
				(*existing)->view_marker->show ();
			} else  {
				(*existing)->view_marker->hide ();
			}

			(*existing)->view_marker->set_position (timepos_t (start.distance (c->position())));
			(*existing)->view_marker->the_item().raise_to_top ();
		}
	}

	_cue_markers_visible = show_cue_markers;
}

void
RegionView::region_muted ()
{
	set_frame_color ();
	region_renamed ();
}

void
RegionView::region_opacity ()
{
	set_frame_color ();
}

void
RegionView::raise_to_top ()
{
	_region->raise_to_top ();
}

void
RegionView::lower_to_bottom ()
{
	_region->lower_to_bottom ();
}

bool
RegionView::set_position (timepos_t const & pos, void* /*src*/, double* ignored)
{
	double delta;
	bool ret;

	if (!(ret = TimeAxisViewItem::set_position (pos, this, &delta))) {
		return false;
	}

	if (ignored) {
		*ignored = delta;
	}

	if (delta) {
		for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
			(*i)->group->move (ArdourCanvas::Duple (delta, 0.0));
		}
	}

	return ret;
}

void
RegionView::set_samples_per_pixel (double fpp)
{
	TimeAxisViewItem::set_samples_per_pixel (fpp);

	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		(*i)->set_samples_per_pixel (fpp);
		(*i)->set_duration (_region->length_samples() / fpp);
	}

	region_sync_changed ();
}

bool
RegionView::set_duration (timecnt_t const & dur, void *src)
{
	if (!TimeAxisViewItem::set_duration (dur, src)) {
		return false;
	}

	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		(*i)->set_duration (_region->length_samples() / samples_per_pixel);
	}

	return true;
}

void
RegionView::set_colors ()
{
	TimeAxisViewItem::set_colors ();
	set_sync_mark_color ();

	const Gtkmm2ext::SVAModifier alpha = UIConfiguration::instance().modifier (X_("region mark"));
	const uint32_t color = Gtkmm2ext::HSV (UIConfiguration::instance().color ("region mark")).mod (alpha).color();

	for (ViewCueMarkers::iterator c = _cue_markers.begin(); c != _cue_markers.end(); ++c) {
		(*c)->view_marker->set_color_rgba (color);
	}
}

void
RegionView::parameter_changed (std::string const& p)
{
	if (p == "show-region-xrun-markers") {
		update_xrun_markers ();
	} else if (p == "show-region-cue-markers") {
		update_cue_markers ();
	}
}

void
RegionView::set_sync_mark_color ()
{
	if (sync_mark) {
		Gtkmm2ext::Color c = UIConfiguration::instance().color ("sync mark");
		sync_mark->set_fill_color (c);
		sync_mark->set_outline_color (c);
		sync_line->set_outline_color (c);
	}
}

uint32_t
RegionView::get_fill_color () const
{
	Gtkmm2ext::Color f = TimeAxisViewItem::get_fill_color();
	char const *modname;

	if (_region->opaque() && ( !_dragging && !_region->muted () )) {
		modname = "opaque region base";
	} else {
		modname = "transparent region base";
	}

	return Gtkmm2ext::HSV(f).mod (UIConfiguration::instance().modifier (modname)).color ();
}

void
RegionView::show_region_editor ()
{
	if (editor == 0) {
		editor = new RegionEditor (trackview.session(), region());
	}

	editor->present ();
	editor->show_all();
}

void
RegionView::hide_region_editor()
{
	if (editor) {
		editor->hide_all ();
	}
}

std::string
RegionView::make_name () const
{
	std::string str;

	// XXX nice to have some good icons for this
	if (_region->position_time_domain() == Temporal::BeatTime) {
		str += "\u266B"; // BEAMED EIGHTH NOTES
	}

	if (_region->locked()) {
		str += "\u2629"; // CROSS OF JERUSALEM
		str += _region->name();
	} else if (_region->position_locked()) {
		str += "\u21B9"; // LEFTWARDS ARROW TO BAR OVER RIGHTWARDS ARROW TO BAR
		str += _region->name();
	} else if (_region->video_locked()) {
		str += '[';
		str += _region->name();
		str += ']';
	} else {
		str += _region->name();
	}

	if (_region->muted()) {
		str = string ("!") + str;
	}

	return str;
}

void
RegionView::region_renamed ()
{
	std::string str = make_name ();

	set_item_name (str, this);
	set_name_text (str);
}

void
RegionView::region_sync_changed ()
{
	int sync_dir;
	timecnt_t sync_offset;

	sync_offset = _region->sync_offset (sync_dir);

	if (sync_offset.is_zero ()) {
		/* no need for a sync mark */
		if (sync_mark) {
			sync_mark->hide();
			sync_line->hide ();
		}
		return;
	}

	if (!sync_mark) {

		/* points set below */

		sync_mark = new ArdourCanvas::Polygon (group);
		CANVAS_DEBUG_NAME (sync_mark, string_compose ("sync mark for %1", get_item_name()));
		sync_line = new ArdourCanvas::Line (group);
		CANVAS_DEBUG_NAME (sync_line, string_compose ("sync mark for %1", get_item_name()));

		set_sync_mark_color ();
	}

	/* this has to handle both a genuine change of position, a change of samples_per_pixel
	   and a change in the bounds of the _region->
	 */

	if (sync_offset == 0) {

		/* no sync mark - its the start of the region */

		sync_mark->hide();
		sync_line->hide ();

	} else {

		if ((sync_dir < 0) || ((sync_dir > 0) && (sync_offset > _region->length()))) {

			/* no sync mark - its out of the bounds of the region */

			sync_mark->hide();
			sync_line->hide ();

		} else {

			/* lets do it */

			Points points;

			//points = sync_mark->property_points().get_value();

			double offset = trackview.editor().duration_to_pixels (sync_offset);
			points.push_back (ArdourCanvas::Duple (offset - ((sync_mark_width-1)/2), 1));
			points.push_back (ArdourCanvas::Duple (offset + ((sync_mark_width-1)/2), 1));
			points.push_back (ArdourCanvas::Duple (offset, sync_mark_width - 1));
			points.push_back (ArdourCanvas::Duple (offset - ((sync_mark_width-1)/2), 1));
			sync_mark->set (points);
			sync_mark->show ();

			sync_line->set (ArdourCanvas::Duple (offset, 0), ArdourCanvas::Duple (offset, trackview.current_height() - NAME_HIGHLIGHT_SIZE));
			sync_line->show ();
		}
	}
}

void
RegionView::move (double x_delta, double y_delta)
{
	if (!_region->can_move() || (x_delta == 0 && y_delta == 0)) {
		return;
	}

	/* items will not prevent Item::move() moving
	 * them to a negative x-axis coordinate, which
	 * is legal, but we don't want that here.
	 */

	ArdourCanvas::Item *item = get_canvas_group ();

	if (item->position().x + x_delta < 0) {
		x_delta = -item->position().x; /* move it to zero */
	}

	item->move (ArdourCanvas::Duple (x_delta, y_delta));

	/* note: ghosts never leave their tracks so y_delta for them is always zero */

	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		(*i)->group->move (ArdourCanvas::Duple (x_delta, 0.0));
	}
}

void
RegionView::remove_ghost_in (TimeAxisView& tv)
{
	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		if (&(*i)->trackview == &tv) {
			delete *i;
			break;
		}
	}
}

void
RegionView::remove_ghost (GhostRegion* ghost)
{
	if (in_destructor) {
		return;
	}

	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		if (*i == ghost) {
			ghosts.erase (i);
			break;
		}
	}
}

void
RegionView::set_height (double h)
{
	TimeAxisViewItem::set_height(h);

	if (sync_line) {
		Points points;
		int sync_dir;
		timecnt_t sync_offset;
		sync_offset = _region->sync_offset (sync_dir);
		double offset = trackview.editor().duration_to_pixels (sync_offset);

		sync_line->set (
			ArdourCanvas::Duple (offset, 0),
			ArdourCanvas::Duple (offset, h - NAME_HIGHLIGHT_SIZE)
			);
	}

	for (list<ArdourCanvas::Rectangle*>::iterator i = _coverage_frame.begin(); i != _coverage_frame.end(); ++i) {
		(*i)->set_y1 (h + 1);
	}

	for (list<ArdourCanvas::Rectangle*>::iterator i = _silent_frames.begin(); i != _silent_frames.end(); ++i) {
		(*i)->set_y1 (h + 1);
	}

	for (ViewCueMarkers::iterator v = _cue_markers.begin(); v != _cue_markers.end(); ++v) {
		(*v)->view_marker->set_line_height (h - (1.5 * UIConfiguration::instance().get_ui_scale()));
	}
}

/** Remove old coverage frame and make new ones, if we're in a LayerDisplay mode
 *  which uses them. */
void
RegionView::update_coverage_frame (LayerDisplay d)
{
	/* remove old coverage frame */
	for (list<ArdourCanvas::Rectangle*>::iterator i = _coverage_frame.begin (); i != _coverage_frame.end (); ++i) {
		delete *i;
	}

	_coverage_frame.clear ();

	if (d != Stacked) {
		/* don't do coverage frame unless we're in stacked mode */
		return;
	}

	boost::shared_ptr<Playlist> pl (_region->playlist ());
	if (!pl) {
		return;
	}

	timepos_t const position = _region->position ();
	timepos_t t (position);
	timepos_t const end = _region->nt_last ();

	ArdourCanvas::Rectangle* cr = 0;
	bool me = false;

	/* the color that will be used to show parts of regions that will not be heard */
	uint32_t const non_playing_color = UIConfiguration::instance().color_mod ("covered region", "covered region base");


	while (t < end) {

		t.increment();

		/* is this region is on top at time t? */
		bool const new_me = (pl->top_unmuted_region_at (t) == _region);
		/* finish off any old rect, if required */
		if (cr && me != new_me) {
			cr->set_x1 (trackview.editor().duration_to_pixels (position.distance (t)));
		}

		/* start off any new rect, if required */
		if (cr == 0 || me != new_me) {
			cr = new ArdourCanvas::Rectangle (group);
			_coverage_frame.push_back (cr);
			cr->set_x0 (trackview.editor().duration_to_pixels (position.distance (t)));
			cr->set_y0 (1);
			cr->set_y1 (_height + 1);
			cr->set_outline (false);
			cr->set_ignore_events (true);
			if (new_me) {
				cr->set_fill_color (UINT_RGBA_CHANGE_A (non_playing_color, 0));
			} else {
				cr->set_fill_color (non_playing_color);
			}
		}
		t = pl->find_next_region_boundary (t, 1);
		if (t.is_negative()) {
			break;
		}
		me = new_me;
	}

	t = pl->find_next_region_boundary (t, 1);

	if (cr) {
		/* finish off the last rectangle */
		cr->set_x1 (trackview.editor().duration_to_pixels (position.distance (end)));
	}

	if (frame_handle_start) {
		frame_handle_start->raise_to_top ();
	}

	if (frame_handle_end) {
		frame_handle_end->raise_to_top ();
	}

	if (name_highlight) {
		name_highlight->raise_to_top ();
	}

	if (name_text) {
		name_text->raise_to_top ();
	}
}

bool
RegionView::trim_front (timepos_t const & new_bound, bool no_overlap)
{
	if (_region->locked()) {
		return false;
	}

	if (_region->position() == new_bound) {
		return false;
	}

	timepos_t const pos = _region->position();

	_region->trim_front (new_bound);

	if (no_overlap) {
		/* Get the next region on the left of this region and shrink/expand it. */
		boost::shared_ptr<Playlist> playlist (_region->playlist());
		boost::shared_ptr<Region> region_left = playlist->find_next_region (pos, End, 0);

		bool regions_touching = false;

		if (region_left != 0 && (pos == region_left->end())) {
			regions_touching = true;
		}

		/* Only trim region on the left if the first sample has gone beyond the left region's last sample. */
		if (region_left != 0 && (region_left->nt_last() > _region->position() || regions_touching)) {
			region_left->trim_end (_region->position().decrement());
		}
	}

	region_changed (ARDOUR::bounds_change);

	return (pos != _region->position()); // return true if we actually changed something
}

bool
RegionView::trim_end (timepos_t const & new_bound, bool no_overlap)
{
	if (_region->locked()) {
		return false;
	}

	timepos_t const last = _region->nt_last();

	_region->trim_end (new_bound);

	if (no_overlap) {
		/* Get the next region on the right of this region and shrink/expand it. */
		boost::shared_ptr<Playlist> playlist (_region->playlist());
		boost::shared_ptr<Region> region_right = playlist->find_next_region (last, Start, 1);

		bool regions_touching = false;

		if (region_right != 0 && (last == region_right->position().decrement())) {
			regions_touching = true;
		}

		/* Only trim region on the right if the last sample has gone beyond the right region's first sample. */
		if (region_right != 0 && (region_right->position() < _region->nt_last() || regions_touching)) {
			region_right->trim_front (_region->end());
		}

		region_changed (ARDOUR::bounds_change);

	} else {
		region_changed (PropertyChange (ARDOUR::Properties::length));
	}

	return (last != _region->nt_last()); // return true if we actually changed something
}


void
RegionView::thaw_after_trim ()
{
	if (_region->locked()) {
		return;
	}

	_region->resume_property_changes ();
}


void
RegionView::move_contents (timecnt_t const & distance)
{
	if (_region->locked()) {
		return;
	}
	_region->move_start (distance);
	region_changed (PropertyChange (ARDOUR::Properties::start));
}


/** Snap a time offset within our region using the current snap settings.
 *  @param x Time offset from this region's position.
 *  @param ensure_snap whether to ignore snap_mode (in the case of SnapOff) and magnetic snap.
 *  Used when inverting snap mode logic with key modifiers, or snap distance calculation.
 *  @return Snapped time offset from this region's position.
 */
timepos_t
RegionView::snap_region_time_to_region_time (timepos_t const & x, bool ensure_snap) const
{
	PublicEditor& editor = trackview.editor();
	/* x is region relative, convert it to global absolute time */
	timepos_t const session_pos = _region->position() + x;

	/* try a snap in either direction */
	timepos_t snapped = session_pos;
	editor.snap_to (snapped, Temporal::RoundNearest, SnapToAny_Visual, ensure_snap);

	/* if we went off the beginning of the region, snap forwards */
	if (snapped < _region->position ()) {
		snapped = session_pos;
		editor.snap_to (snapped, Temporal::RoundUpAlways, SnapToAny_Visual, ensure_snap);
	}

	/* back to region relative */
	return _region->region_relative_position (snapped);
}

timecnt_t
RegionView::region_relative_distance (timecnt_t const & duration, Temporal::TimeDomain domain)
{
	return Temporal::TempoMap::use()->convert_duration (duration, _region->position(), domain);
}

timecnt_t
RegionView::source_relative_distance (timecnt_t const & duration, Temporal::TimeDomain domain)
{
	return Temporal::TempoMap::use()->convert_duration (duration, _region->source_position(), domain);
}

void
RegionView::update_visibility ()
{
	/* currently only the name visibility can be changed dynamically */

	if (UIConfiguration::instance().get_show_region_name()) {
		visibility = Visibility (visibility | ShowNameText);
	} else {
		visibility = Visibility (visibility & ~ShowNameText);
	}

	manage_name_text ();
}

void
RegionView::set_selected (bool yn)
{
	_region->set_selected_for_solo(yn);
	TimeAxisViewItem::set_selected(yn);
}

void
RegionView::maybe_raise_cue_markers ()
{
	for (ViewCueMarkers::iterator v = _cue_markers.begin(); v != _cue_markers.end(); ++v) {
		(*v)->view_marker->the_item().raise_to_top ();
	}
}

CueMarker
RegionView::find_model_cue_marker (ArdourMarker* m)
{
	for (ViewCueMarkers::iterator v = _cue_markers.begin(); v != _cue_markers.end(); ++v) {
		if ((*v)->view_marker == m) {
			return (*v)->model_marker;
		}
	}

	return CueMarker (string(), timepos_t()); /* empty string signifies invalid */
}

void
RegionView::drop_cue_marker (ArdourMarker* m)
{
	for (ViewCueMarkers::iterator v = _cue_markers.begin(); v != _cue_markers.end(); ++v) {
		if ((*v)->view_marker == m) {
			delete m;
			_cue_markers.erase (v);
			return;
		}
	}
}
