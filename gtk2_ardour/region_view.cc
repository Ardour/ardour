/*
    Copyright (C) 2001-2006 Paul Davis

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
#include <algorithm>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include "ardour/playlist.h"
#include "ardour/session.h"

#include "canvas/polygon.h"
#include "canvas/debug.h"
#include "canvas/pixbuf.h"
#include "canvas/text.h"
#include "canvas/line.h"

#include "ardour_ui.h"
#include "global_signals.h"
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

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace Gtk;
using namespace ArdourCanvas;

static const int32_t sync_mark_width = 9;

PBD::Signal1<void,RegionView*> RegionView::RegionViewGoingAway;

RegionView::RegionView (ArdourCanvas::Group*              parent,
                        TimeAxisView&                     tv,
                        boost::shared_ptr<ARDOUR::Region> r,
                        double                            spu,
                        Gdk::Color const &                basic_color,
			bool                              automation)
	: TimeAxisViewItem (r->name(), *parent, tv, spu, basic_color, r->position(), r->length(), false, automation,
			    TimeAxisViewItem::Visibility (TimeAxisViewItem::ShowNameText|
							  TimeAxisViewItem::ShowNameHighlight| TimeAxisViewItem::ShowFrame))
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
	, _region_relative_time_converter(r->session().tempo_map(), r->position())
	, _source_relative_time_converter(r->session().tempo_map(), r->position() - r->start())
{
	GhostRegion::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&RegionView::remove_ghost, this, _1), gui_context());
}

RegionView::RegionView (const RegionView& other)
	: sigc::trackable(other)
	, TimeAxisViewItem (other)
        , _silence_text (0)
	, _region_relative_time_converter(other.region_relative_time_converter())
	, _source_relative_time_converter(other.source_relative_time_converter())
{
	/* derived concrete type will call init () */

	_region = other._region;
	current_visible_sync_position = other.current_visible_sync_position;
	valid = false;
	_pixel_width = other._pixel_width;

	GhostRegion::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&RegionView::remove_ghost, this, _1), gui_context());
}

RegionView::RegionView (const RegionView& other, boost::shared_ptr<Region> other_region)
	: sigc::trackable(other)
	, TimeAxisViewItem (other)
        , _silence_text (0)
	, _region_relative_time_converter(other_region->session().tempo_map(), other_region->position())
	, _source_relative_time_converter(other_region->session().tempo_map(), other_region->position() - other_region->start())
{
	/* this is a pseudo-copy constructor used when dragging regions
	   around on the canvas.
	*/

	/* derived concrete type will call init () */

	_region = other_region;
	current_visible_sync_position = other.current_visible_sync_position;
	valid = false;
	_pixel_width = other._pixel_width;

	GhostRegion::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&RegionView::remove_ghost, this, _1), gui_context());
}

RegionView::RegionView (ArdourCanvas::Group*         parent,
                        TimeAxisView&                tv,
                        boost::shared_ptr<ARDOUR::Region> r,
                        double                       spu,
                        Gdk::Color const &           basic_color,
			bool                         recording,
                        TimeAxisViewItem::Visibility visibility)
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
	, _region_relative_time_converter(r->session().tempo_map(), r->position())
	, _source_relative_time_converter(r->session().tempo_map(), r->position() - r->start())
{
}

void
RegionView::init (Gdk::Color const & basic_color, bool wfd)
{
	editor        = 0;
	valid         = true;
	in_destructor = false;
	wait_for_data = wfd;
	sync_mark     = 0;
	sync_line     = 0;
	sync_mark     = 0;
	sync_line     = 0;

	compute_colors (basic_color);

	if (name_highlight) {
		name_highlight->set_data ("regionview", this);
		name_highlight->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_region_view_name_highlight_event), name_highlight, this));

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
	}

	if (name_text) {
		name_text->set_data ("regionview", this);
		name_text->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_region_view_name_event), name_text, this));
	}

	if (wfd) {
		_enable_display = true;
	}

	set_height (trackview.current_height());

	_region->PropertyChanged.connect (*this, invalidator (*this), boost::bind (&RegionView::region_changed, this, _1), gui_context());

	set_colors ();

	ColorsChanged.connect (sigc::mem_fun (*this, &RegionView::color_handler));

	/* XXX sync mark drag? */
}

RegionView::~RegionView ()
{
	in_destructor = true;

	for (vector<GhostRegion*>::iterator g = ghosts.begin(); g != ghosts.end(); ++g) {
		delete *g;
	}

	for (list<ArdourCanvas::Rectangle*>::iterator i = _coverage_frames.begin (); i != _coverage_frames.end (); ++i) {
		delete *i;
	}

        drop_silent_frames ();

	delete editor;
}

bool
RegionView::canvas_group_event (GdkEvent* event)
{
	return trackview.editor().canvas_region_view_event (event, group, this);
}

void
RegionView::set_silent_frames (const AudioIntervalResult& silences, double /*threshold*/)
{
        framecnt_t shortest = max_framecnt;

	/* remove old silent frames */
        drop_silent_frames ();

        if (silences.empty()) {
                return;
        }

        uint32_t const color = ARDOUR_UI::config()->get_canvasvar_Silence();

	for (AudioIntervalResult::const_iterator i = silences.begin(); i != silences.end(); ++i) {

		ArdourCanvas::Rectangle* cr = new ArdourCanvas::Rectangle (group);
		cr->set_ignore_events (true);
		_silent_frames.push_back (cr);

		/* coordinates for the rect are relative to the regionview origin */

		cr->set_x0 (trackview.editor().sample_to_pixel (i->first - _region->start()));
		cr->set_x1 (trackview.editor().sample_to_pixel (i->second - _region->start()));
		cr->set_y0 (1);
		cr->set_y1 (_height - 2);
		cr->set_outline (false);
		cr->set_fill_color (color);

		shortest = min (shortest, i->second - i->first);
	}

	/* Find shortest audible segment */
        framecnt_t shortest_audible = max_framecnt;

	framecnt_t s = _region->start();
	for (AudioIntervalResult::const_iterator i = silences.begin(); i != silences.end(); ++i) {
		framecnt_t const dur = i->first - s;
		if (dur > 0) {
			shortest_audible = min (shortest_audible, dur);
		}

		s = i->second;
	}

	framecnt_t const dur = _region->start() + _region->length() - 1 - s;
	if (dur > 0) {
		shortest_audible = min (shortest_audible, dur);
	}

        _silence_text = new ArdourCanvas::Text (group);
	_silence_text->set_ignore_events (true);
        _silence_text->set_font_description (get_font_for_style (N_("SilenceText")));
        _silence_text->set_color (ARDOUR_UI::config()->get_canvasvar_SilenceText());

        /* both positions are relative to the region start offset in source */

        _silence_text->set_x_position (trackview.editor().sample_to_pixel (silences.front().first - _region->start()) + 10.0);
        _silence_text->set_y_position (20.0);

        double ms = (float) shortest/_region->session().frame_rate();

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

        if (shortest_audible != max_framepos) {
                /* ms are now in seconds */
                double ma = (float) shortest_audible / _region->session().frame_rate();
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
	if (what_changed.contains (ARDOUR::Properties::sync_position)) {
		region_sync_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::locked)) {
		region_locked ();
	}
	if (what_changed.contains (ARDOUR::Properties::locked)) {
		/* name will show locked status */
		region_renamed ();
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

	if (what_changed.contains (ARDOUR::Properties::position)) {
		set_position (_region->position(), 0);
		_region_relative_time_converter.set_origin_b (_region->position());
	}

	if (what_changed.contains (ARDOUR::Properties::start) || what_changed.contains (ARDOUR::Properties::position)) {
		_source_relative_time_converter.set_origin_b (_region->position() - _region->start());
	}

	PropertyChange s_and_l;
	s_and_l.add (ARDOUR::Properties::start);
	s_and_l.add (ARDOUR::Properties::length);

	if (what_changed.contains (s_and_l)) {

		set_duration (_region->length(), 0);

		unit_length = _region->length() / samples_per_pixel;

 		for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {

 			(*i)->set_duration (unit_length);

 		}
	}
}

void
RegionView::reset_width_dependent_items (double pixel_width)
{
	TimeAxisViewItem::reset_width_dependent_items (pixel_width);
	_pixel_width = pixel_width;
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
RegionView::set_position (framepos_t pos, void* /*src*/, double* ignored)
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
		(*i)->set_duration (_region->length() / fpp);
	}

	region_sync_changed ();
}

bool
RegionView::set_duration (framecnt_t frames, void *src)
{
	if (!TimeAxisViewItem::set_duration (frames, src)) {
		return false;
	}

	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		(*i)->set_duration (_region->length() / samples_per_pixel);
	}

	return true;
}

void
RegionView::set_colors ()
{
	TimeAxisViewItem::set_colors ();

	if (sync_mark) {
		/* XXX: make these colours themable */
		sync_mark->set_fill_color (RGBA_TO_UINT (0, 255, 0, 255));
		sync_line->set_outline_color (RGBA_TO_UINT (0, 255, 0, 255));
	}
}

void
RegionView::set_frame_color ()
{
	if (_region->opaque()) {
		fill_opacity = 130;
	} else {
		fill_opacity = 60;
	}

	TimeAxisViewItem::set_frame_color ();
}

void
RegionView::fake_set_opaque (bool yn)
{
       if (yn) {
               fill_opacity = 130;
       } else {
               fill_opacity = 60;
       }

       set_frame_color ();
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

	if (_region->locked()) {
		str += '>';
		str += _region->name();
		str += '<';
	} else if (_region->position_locked()) {
		str += '{';
		str += _region->name();
		str += '}';
	} else if (_region->video_locked()) {
		str += '[';
		str += _region->name();
		str += ']';
	} else {
		str = _region->name();
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
	reset_width_dependent_items (_pixel_width);
}

void
RegionView::region_sync_changed ()
{
	int sync_dir;
	framecnt_t sync_offset;

	sync_offset = _region->sync_offset (sync_dir);

	if (sync_offset == 0) {
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
		sync_mark->set_fill_color (RGBA_TO_UINT(0,255,0,255));    // FIXME make a themeable colour

		sync_line = new ArdourCanvas::Line (group);
		CANVAS_DEBUG_NAME (sync_line, string_compose ("sync mark for %1", get_item_name()));
		sync_line->set_outline_color (RGBA_TO_UINT(0,255,0,255)); // FIXME make a themeable colour
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

			double offset = sync_offset / samples_per_pixel;
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

	get_canvas_group()->move (ArdourCanvas::Duple (x_delta, y_delta));

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

uint32_t
RegionView::get_fill_color ()
{
	return fill_color;
}

void
RegionView::set_height (double h)
{
	TimeAxisViewItem::set_height(h);

	if (sync_line) {
		Points points;
		int sync_dir;
		framecnt_t sync_offset;
		sync_offset = _region->sync_offset (sync_dir);
		double offset = sync_offset / samples_per_pixel;

		sync_line->set (
			ArdourCanvas::Duple (offset, 0),
			ArdourCanvas::Duple (offset, h - NAME_HIGHLIGHT_SIZE)
			);
	}

	for (list<ArdourCanvas::Rectangle*>::iterator i = _coverage_frames.begin(); i != _coverage_frames.end(); ++i) {
		(*i)->set_y1 (h + 1);
	}

	for (list<ArdourCanvas::Rectangle*>::iterator i = _silent_frames.begin(); i != _silent_frames.end(); ++i) {
		(*i)->set_y1 (h + 1);
	}

}

/** Remove old coverage frames and make new ones, if we're in a LayerDisplay mode
 *  which uses them. */
void
RegionView::update_coverage_frames (LayerDisplay d)
{
	/* remove old coverage frames */
	for (list<ArdourCanvas::Rectangle*>::iterator i = _coverage_frames.begin (); i != _coverage_frames.end (); ++i) {
		delete *i;
	}

	_coverage_frames.clear ();

	if (d != Stacked) {
		/* don't do coverage frames unless we're in stacked mode */
		return;
	}

	boost::shared_ptr<Playlist> pl (_region->playlist ());
	if (!pl) {
		return;
	}

	framepos_t const position = _region->first_frame ();
	framepos_t t = position;
	framepos_t const end = _region->last_frame ();

	ArdourCanvas::Rectangle* cr = 0;
	bool me = false;

	/* the color that will be used to show parts of regions that will not be heard */
	uint32_t const non_playing_color = ARDOUR_UI::config()->get_canvasvar_CoveredRegion ();

	while (t < end) {

		t++;

		/* is this region is on top at time t? */
		bool const new_me = (pl->top_unmuted_region_at (t) == _region);

		/* finish off any old rect, if required */
		if (cr && me != new_me) {
			cr->set_x1 (trackview.editor().sample_to_pixel (t - position));
		}

		/* start off any new rect, if required */
		if (cr == 0 || me != new_me) {
			cr = new ArdourCanvas::Rectangle (group);
			_coverage_frames.push_back (cr);
			cr->set_x0 (trackview.editor().sample_to_pixel (t - position));
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
		me = new_me;
	}

	if (cr) {
		/* finish off the last rectangle */
		cr->set_x1 (trackview.editor().sample_to_pixel (end - position));
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
RegionView::trim_front (framepos_t new_bound, bool no_overlap)
{
	if (_region->locked()) {
		return false;
	}

	RouteTimeAxisView& rtv = dynamic_cast<RouteTimeAxisView&> (trackview);
	double const speed = rtv.track()->speed ();

	framepos_t const pre_trim_first_frame = _region->first_frame();

	_region->trim_front ((framepos_t) (new_bound * speed));

	if (no_overlap) {
		// Get the next region on the left of this region and shrink/expand it.
		boost::shared_ptr<Playlist> playlist (_region->playlist());
		boost::shared_ptr<Region> region_left = playlist->find_next_region (pre_trim_first_frame, End, 0);

		bool regions_touching = false;

		if (region_left != 0 && (pre_trim_first_frame == region_left->last_frame() + 1)) {
			regions_touching = true;
		}

		// Only trim region on the left if the first frame has gone beyond the left region's last frame.
		if (region_left != 0 &&	(region_left->last_frame() > _region->first_frame() || regions_touching)) {
			region_left->trim_end (_region->first_frame() - 1);
		}
	}

	region_changed (ARDOUR::bounds_change);

	return (pre_trim_first_frame != _region->first_frame());  //return true if we actually changed something
}

bool
RegionView::trim_end (framepos_t new_bound, bool no_overlap)
{
	if (_region->locked()) {
		return false;
	}

	RouteTimeAxisView& rtv = dynamic_cast<RouteTimeAxisView&> (trackview);
	double const speed = rtv.track()->speed ();

	framepos_t const pre_trim_last_frame = _region->last_frame();

	_region->trim_end ((framepos_t) (new_bound * speed));

	if (no_overlap) {
		// Get the next region on the right of this region and shrink/expand it.
		boost::shared_ptr<Playlist> playlist (_region->playlist());
		boost::shared_ptr<Region> region_right = playlist->find_next_region (pre_trim_last_frame, Start, 1);

		bool regions_touching = false;

		if (region_right != 0 && (pre_trim_last_frame == region_right->first_frame() - 1)) {
			regions_touching = true;
		}

		// Only trim region on the right if the last frame has gone beyond the right region's first frame.
		if (region_right != 0 && (region_right->first_frame() < _region->last_frame() || regions_touching)) {
			region_right->trim_front (_region->last_frame() + 1);
		}

		region_changed (ARDOUR::bounds_change);

	} else {
		region_changed (PropertyChange (ARDOUR::Properties::length));
	}

	return (pre_trim_last_frame != _region->last_frame());  //return true if we actually changed something
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
RegionView::move_contents (frameoffset_t distance)
{
	if (_region->locked()) {
		return;
	}
	_region->move_start (distance);
	region_changed (PropertyChange (ARDOUR::Properties::start));
}

/** Snap a frame offset within our region using the current snap settings.
 *  @param x Frame offset from this region's position.
 *  @return Snapped frame offset from this region's position.
 */
frameoffset_t
RegionView::snap_frame_to_frame (frameoffset_t x) const
{
	PublicEditor& editor = trackview.editor();

	/* x is region relative, convert it to global absolute frames */
	framepos_t const session_frame = x + _region->position();

	/* try a snap in either direction */
	framepos_t frame = session_frame;
	editor.snap_to (frame, 0);

	/* if we went off the beginning of the region, snap forwards */
	if (frame < _region->position ()) {
		frame = session_frame;
		editor.snap_to (frame, 1);
	}

	/* back to region relative */
	return frame - _region->position();
}
