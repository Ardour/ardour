/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2013 Michael Fisher <mfisher31@gmail.com>
 * Copyright (C) 2014-2019 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2015-2017 Tim Mayberry <mojofunk@gmail.com>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <stdint.h>
#include <algorithm>

#include "pbd/memento_command.h"
#include "pbd/basename.h"
#include "pbd/stateful_diff_command.h"

#include <gtkmm/stock.h>

#include "gtkmm2ext/utils.h"

#include "ardour/audioengine.h"
#include "ardour/audioregion.h"
#include "ardour/audio_track.h"
#include "ardour/dB.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/operations.h"
#include "ardour/profile.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"

#include "canvas/canvas.h"
#include "canvas/scroll_group.h"

#include "editor.h"
#include "pbd/i18n.h"
#include "bbt_marker_dialog.h"
#include "keyboard.h"
#include "audio_region_view.h"
#include "automation_region_view.h"
#include "midi_region_view.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "control_point.h"
#include "region_gain_line.h"
#include "editor_drag.h"
#include "audio_time_axis.h"
#include "midi_time_axis.h"
#include "selection.h"
#include "midi_selection.h"
#include "automation_time_axis.h"
#include "debug.h"
#include "editor_cursors.h"
#include "mouse_cursors.h"
#include "note_base.h"
#include "patch_change.h"
#include "ui_config.h"
#include "verbose_cursor.h"
#include "video_timeline.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;
using namespace ArdourCanvas;
using namespace Temporal;

using Gtkmm2ext::Keyboard;

double ControlPointDrag::_zero_gain_fraction = -1.0;

DragManager::DragManager (Editor* e)
	: _editor (e)
	, _ending (false)
	, _current_pointer_x (0.0)
	, _current_pointer_y (0.0)
	, _current_pointer_time (0)
	, _old_follow_playhead (false)
{
}

DragManager::~DragManager ()
{
	abort ();
}

/** Call abort for each active drag */
void
DragManager::abort ()
{
	_ending = true;

	for (list<Drag*>::const_iterator i = _drags.begin(); i != _drags.end(); ++i) {
		(*i)->abort ();
		delete *i;
	}

	if (!_drags.empty ()) {
		_editor->set_follow_playhead (_old_follow_playhead, false);
	}

	_drags.clear ();
	_editor->abort_reversible_command();

	_ending = false;
}

void
DragManager::add (Drag* d)
{
	d->set_manager (this);
	_drags.push_back (d);
}

void
DragManager::set (Drag* d, GdkEvent* e, Gdk::Cursor* c)
{
	d->set_manager (this);
	_drags.push_back (d);
	start_grab (e, c);
}

bool
DragManager::preview_video () const {
	for (list<Drag*>::const_iterator i = _drags.begin(); i != _drags.end(); ++i) {
		if ((*i)->preview_video ()) {
			return true;
		}
	}
	return false;
}

void
DragManager::start_grab (GdkEvent* e, Gdk::Cursor* c)
{
	/* Prevent follow playhead during the drag to be nice to the user */
	_old_follow_playhead = _editor->follow_playhead ();
	_editor->set_follow_playhead (false);

	_current_pointer_time = timepos_t (_editor->canvas_event_sample (e, &_current_pointer_x, &_current_pointer_y));

	for (list<Drag*>::const_iterator i = _drags.begin(); i != _drags.end(); ++i) {
		if ((*i)->grab_button() < 0) {
			(*i)->start_grab (e, c);
		}
	}
}

/** Call end_grab for each active drag.
 *  @return true if any drag reported movement having occurred.
 */
bool
DragManager::end_grab (GdkEvent* e)
{
	_ending = true;

	bool r = false;

	for (list<Drag*>::iterator i = _drags.begin(); i != _drags.end(); ) {
		list<Drag*>::iterator tmp = i;

		if ((*i)->grab_button() == (int) e->button.button) {
			bool const t = (*i)->end_grab (e);
			if (t) {
				r = true;
			}
			delete *i;
			tmp = _drags.erase (i);
		} else {
			++tmp;
		}

		i = tmp;
	}

	_ending = false;

	if (_drags.empty()) {
		_editor->set_follow_playhead (_old_follow_playhead, false);
	}

	return r;
}

void
DragManager::mark_double_click ()
{
	for (list<Drag*>::const_iterator i = _drags.begin(); i != _drags.end(); ++i) {
		(*i)->set_double_click (true);
	}
}

bool
DragManager::motion_handler (GdkEvent* e, bool from_autoscroll)
{
	bool r = false;

	/* calling this implies that we expect the event to have canvas
	 * coordinates
	 *
	 * Can we guarantee that this is true?
	 */

	_current_pointer_time = timepos_t (_editor->canvas_event_sample (e, &_current_pointer_x, &_current_pointer_y));

	for (list<Drag*>::iterator i = _drags.begin(); i != _drags.end(); ++i) {
		bool const t = (*i)->motion_handler (e, from_autoscroll);
		/* run all handlers; return true if at least one of them
		   returns true (indicating that the event has been handled).
		*/
		if (t) {
			r = true;
		}

	}

	return r;
}

bool
DragManager::have_item (ArdourCanvas::Item* i) const
{
	list<Drag*>::const_iterator j = _drags.begin ();
	while (j != _drags.end() && (*j)->item () != i) {
		++j;
	}

	return j != _drags.end ();
}

Drag::Drag (Editor* e, ArdourCanvas::Item* i, Temporal::TimeDomain td, bool trackview_only)
	: _editor (e)
	, _drags (0)
	, _item (i)
	, _pointer_offset (0)
	, _video_offset (0)
	, _preview_video (false)
	, _x_constrained (false)
	, _y_constrained (false)
	, _was_rolling (false)
	, _earliest_time_limit (0)
	, _trackview_only (trackview_only)
	, _move_threshold_passed (false)
	, _starting_point_passed (false)
	, _initially_vertical (false)
	, _was_double_click (false)
	, _grab_x (0.0)
	, _grab_y (0.0)
	, _last_pointer_x (0.0)
	, _last_pointer_y (0.0)
	, _snap_delta (0)
	, _constraint_pressed (false)
	, _time_domain (td)
	, _grab_button (-1)
{

}
timepos_t
Drag::pixel_to_time (double x) const
{
	samplepos_t p = _editor->pixel_to_sample (x);

	if (_time_domain == Temporal::AudioTime) {
		return timepos_t (p);
	}

	return timepos_t (timepos_t (p).beats ());
}

void
Drag::swap_grab (ArdourCanvas::Item* new_item, Gdk::Cursor* cursor, uint32_t /*time*/)
{
	_item->ungrab ();
	_item = new_item;

	if (!_cursor_ctx) {
		_cursor_ctx = CursorContext::create (*_editor, cursor);
	} else {
		_cursor_ctx->change (cursor);
	}

	_item->grab ();
}

void
Drag::start_grab (GdkEvent* event, Gdk::Cursor *cursor)
{
	/* we set up x/y dragging constraints on first move */
	_constraint_pressed = ArdourKeyboard::indicates_constraint (event->button.state);

	const samplepos_t pos = _editor->canvas_event_sample (event, &_grab_x, &_grab_y);

	if (_time_domain == Temporal::AudioTime) {
		_raw_grab_time = timepos_t (pos);
	} else {
		_raw_grab_time = timepos_t (timepos_t (pos).beats());
	}

	_grab_button = event->button.button;

	setup_pointer_offset ();
	setup_video_offset ();
	if (! UIConfiguration::instance ().get_preview_video_frame_on_drag ()) {
		_preview_video = false;
	}

	_grab_time = adjusted_time (_raw_grab_time, event);
	_last_pointer_time = _grab_time;
	_last_pointer_x = _grab_x;

	if (_trackview_only) {
		_grab_y = _grab_y - _editor->get_trackview_group()->canvas_origin().y;
	}

	_last_pointer_y = _grab_y;

	_item->grab ();

	if (!_editor->cursors()->is_invalid (cursor)) {
		/* CAIROCANVAS need a variant here that passes *cursor */
		_cursor_ctx = CursorContext::create (*_editor, cursor);
	}

	if (_editor->session() && _editor->session()->transport_rolling()) {
		_was_rolling = true;
	} else {
		_was_rolling = false;
	}

//	if ( UIConfiguration::instance().get_snap_to_region_start() || UIConfiguration::instance().get_snap_to_region_end() || UIConfiguration::instance().get_snap_to_region_sync() ) {
//		_editor->build_region_boundary_cache ();
//	}
}

/** Call to end a drag `successfully'.  Ungrabs item and calls
 *  subclass' finished() method.
 *
 *  @param event GDK event, or 0.
 *  @return true if some movement occurred, otherwise false.
 */
bool
Drag::end_grab (GdkEvent* event)
{
	_editor->stop_canvas_autoscroll ();

	_item->ungrab ();

	finished (event, _move_threshold_passed);

	_editor->verbose_cursor()->hide ();
	_cursor_ctx.reset();

	return _move_threshold_passed;
}

timepos_t
Drag::adjusted_time (timepos_t const & f, GdkEvent const * event, bool snap) const
{
	timepos_t pos (f.time_domain()); /* zero */

	if (f > _pointer_offset) {
		pos = f;
		pos.shift_earlier (_pointer_offset);
	}

	if (snap) {
		_editor->snap_to_with_modifier (pos, event);
	}

	return pos;
}

timepos_t
Drag::adjusted_current_time (GdkEvent const * event, bool snap) const
{
	return adjusted_time (_drags->current_pointer_time (), event, snap);
}

timecnt_t
Drag::snap_delta (guint state) const
{
	if (ArdourKeyboard::indicates_snap_delta (state)) {
		return _snap_delta;
	}

	return timecnt_t (_editor->default_time_domain());
}

double
Drag::current_pointer_x() const
{
	return _drags->current_pointer_x ();
}

double
Drag::current_pointer_y () const
{
	if (!_trackview_only) {
		return _drags->current_pointer_y ();
	}

	return _drags->current_pointer_y () - _editor->get_trackview_group()->canvas_origin().y;
}

void
Drag::setup_snap_delta (timepos_t const & pos)
{
	timepos_t snap (pos);
	_editor->snap_to (snap, Temporal::RoundNearest, ARDOUR::SnapToAny_Visual, true);
	_snap_delta = pos.distance (snap);
}

bool
Drag::motion_handler (GdkEvent* event, bool from_autoscroll)
{
	/* check to see if we have moved in any way that matters since the last motion event */
	if (_move_threshold_passed &&
	    (!x_movement_matters() || _last_pointer_x == current_pointer_x ()) &&
	    (!y_movement_matters() || _last_pointer_y == current_pointer_y ()) ) {
		return false;
	}

	pair<timecnt_t, int> const threshold = move_threshold ();

	bool const old_move_threshold_passed = _move_threshold_passed;

	if (!_move_threshold_passed) {

		bool const xp = (_raw_grab_time.distance (_drags->current_pointer_time ()).abs() >= threshold.first);
		bool const yp = (::fabs ((current_pointer_y () - _grab_y)) >= threshold.second);

		_move_threshold_passed = ((xp && x_movement_matters()) || (yp && y_movement_matters()));
	}

	if (active (_editor->mouse_mode) && _move_threshold_passed) {

		if (event->motion.state & Gdk::BUTTON1_MASK || event->motion.state & Gdk::BUTTON2_MASK) {

			if (old_move_threshold_passed != _move_threshold_passed) {

				/* just changed */

				if (fabs (current_pointer_y() - _grab_y) > fabs (current_pointer_x() - _grab_x)) {
					_initially_vertical = true;
				} else {
					_initially_vertical = false;
				}
				/** check constraints for this drag.
				 *  Note that the current convention is to use "contains" for
				 *  key modifiers during motion and "equals" when initiating a drag.
				 *  In this case we haven't moved yet, so "equals" applies here.
				 */
				if (Config->get_edit_mode() != Lock) {
					if (event->motion.state & Gdk::BUTTON2_MASK) {
						// if dragging with button2, the motion is x constrained, with constraint modifier it is y constrained
						if (_constraint_pressed) {
							_x_constrained = false;
							_y_constrained = true;
						} else {
							_x_constrained = true;
							_y_constrained = false;
						}
					} else if (_constraint_pressed) {
						// if dragging normally, the motion is constrained to the first direction of movement.
						if (_initially_vertical) {
							_x_constrained = true;
							_y_constrained = false;
						} else {
							_x_constrained = false;
							_y_constrained = true;
						}
					}
				} else {
					if (event->button.state & Gdk::BUTTON2_MASK) {
						_x_constrained = false;
					} else {
						_x_constrained = true;
					}
					_y_constrained = false;
				}
			}

			if (!from_autoscroll) {
				_editor->maybe_autoscroll (true, allow_vertical_autoscroll (), false);
			}

			if (!_editor->autoscroll_active() || from_autoscroll) {


				bool first_move = (_move_threshold_passed != old_move_threshold_passed) || from_autoscroll;

				motion (event, first_move && !_starting_point_passed);

				if (first_move && !_starting_point_passed) {
					_starting_point_passed = true;
				}

				_last_pointer_x = _drags->current_pointer_x ();
				_last_pointer_y = current_pointer_y ();
				_last_pointer_time = adjusted_current_time (event, false);
			}

			return true;
		}
	}

	return false;
}

/** Call to abort a drag.  Ungrabs item and calls subclass's aborted () */
void
Drag::abort ()
{
	if (_item) {
		_item->ungrab ();
	}

	aborted (_move_threshold_passed);

	_editor->stop_canvas_autoscroll ();
	_editor->verbose_cursor()->hide ();
}

void
Drag::show_verbose_cursor_time (timepos_t const & pos)
{
	_editor->verbose_cursor()->set_time (pos.samples());
	_editor->verbose_cursor()->show ();
}

void
Drag::show_verbose_cursor_duration (timepos_t const & start, timepos_t const & end, double /*xoffset*/)
{
	_editor->verbose_cursor()->set_duration (start.samples(), end.samples());
	_editor->verbose_cursor()->show ();
}

void
Drag::show_verbose_cursor_text (string const & text)
{
	_editor->verbose_cursor()->set (text);
	_editor->verbose_cursor()->show ();
}

void
Drag::show_view_preview (timepos_t const & pos)
{
	if (_preview_video) {
		ARDOUR_UI::instance()->video_timeline->manual_seek_video_monitor (pos.samples());
	}
}

boost::shared_ptr<Region>
Drag::add_midi_region (MidiTimeAxisView* view, bool commit)
{
	if (_editor->session()) {
		const timepos_t pos (grab_time());
		/* not that the frame rate used here can be affected by pull up/down which
		   might be wrong.
		*/

		timepos_t beats = timepos_t (grab_time().beats());
		cerr << "Adding region based on grab @ " << grab_time() << " = " << beats << " aka " << beats.beats() << endl;
		timecnt_t len = pos.distance (max (timepos_t::zero (Temporal::BeatTime), timepos_t (pos.beats() + Beats (1, 0))));
		return view->add_region (beats, len, commit);
	}

	return boost::shared_ptr<Region>();
}

struct TimeAxisViewStripableSorter {
	bool operator() (TimeAxisView* tav_a, TimeAxisView* tav_b) {
		boost::shared_ptr<ARDOUR::Stripable> const& a = tav_a->stripable ();
		boost::shared_ptr<ARDOUR::Stripable> const& b = tav_b->stripable ();
		return ARDOUR::Stripable::Sorter () (a, b);
	}
};

RegionDrag::RegionDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
#warning nutempo un-hardcode time-domain, see fa7e7a462c8
	: Drag (e, i, p && p->region () ? p->region()->position().time_domain() : Temporal::AudioTime)
	, _primary (p)
	, _ntracks (0)
{
	_editor->visible_order_range (&_visible_y_low, &_visible_y_high);

	/* Make a list of tracks to refer to during the drag; we include hidden tracks,
	   as some of the regions we are dragging may be on such tracks.
	*/

	TrackViewList track_views = _editor->track_views;
	track_views.sort (TimeAxisViewStripableSorter ());

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		_time_axis_views.push_back (*i);

		TimeAxisView::Children children_list = (*i)->get_child_list ();
		for (TimeAxisView::Children::iterator j = children_list.begin(); j != children_list.end(); ++j) {
			_time_axis_views.push_back (j->get());
		}
	}

	/* the list of views can be empty at this point if this is a region list-insert drag
	 */

	for (list<RegionView*>::const_iterator i = v.begin(); i != v.end(); ++i) {
		_views.push_back (DraggingView (*i, this, &(*i)->get_time_axis_view()));
	}

	RegionView::RegionViewGoingAway.connect (death_connection, invalidator (*this), boost::bind (&RegionDrag::region_going_away, this, _1), gui_context());
}

void
RegionDrag::region_going_away (RegionView* v)
{
	list<DraggingView>::iterator i = _views.begin ();
	while (i != _views.end() && i->view != v) {
		++i;
	}

	if (i != _views.end()) {
		_views.erase (i);
	}
}

/** Given a TimeAxisView, return the index of it into the _time_axis_views vector,
 *  or -1 if it is not found.
 */
int
RegionDrag::find_time_axis_view (TimeAxisView* t) const
{
	int i = 0;
	int const N = _time_axis_views.size ();
	while (i < N && _time_axis_views[i] != t) {
		++i;
	}

	if (i == N) {
		return -1;
	}

	return i;
}

void
RegionDrag::setup_video_offset ()
{
	if (_views.empty ()) {
		_preview_video = true;
		return;
	}
	timepos_t first_sync = _views.begin()->view->region()->sync_position ();
	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		first_sync = std::min (first_sync, i->view->region()->sync_position ());
	}
	_video_offset = raw_grab_time().distance (first_sync + _pointer_offset);
	_preview_video = true;
}

void
RegionDrag::add_stateful_diff_commands_for_playlists (PlaylistSet const & playlists)
{
	for (PlaylistSet::const_iterator i = playlists.begin(); i != playlists.end(); ++i) {
		StatefulDiffCommand* c = new StatefulDiffCommand (*i);
		if (!c->empty()) {
			_editor->session()->add_command (c);
		} else {
			delete c;
		}
	}
}


RegionSlipContentsDrag::RegionSlipContentsDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
{
	DEBUG_TRACE (DEBUG::Drags, "New RegionSlipContentsDrag\n");
}

void
RegionSlipContentsDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, _editor->cursors()->trimmer);
}

void
RegionSlipContentsDrag::motion (GdkEvent* event, bool first_move)
{
	if (first_move) {

		/*prepare reversible cmd*/
		_editor->begin_reversible_command (_("Slip Contents"));
		for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
			RegionView* rv = i->view;
			rv->region()->clear_changes ();

			rv->drag_start ();  //this allows the region to draw itself 'transparently' while we drag it
		}

	} else {
		for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
			RegionView* rv = i->view;
			timecnt_t slippage = adjusted_current_time(event, false).distance (last_pointer_time());
			rv->move_contents (slippage);
		}
		show_verbose_cursor_time (_primary->region()->start ());
	}
}

void
RegionSlipContentsDrag::finished (GdkEvent *, bool movement_occurred)
{
	if (movement_occurred) {
		/*finish reversible cmd*/
		for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
			RegionView* rv = i->view;
			_editor->session()->add_command (new StatefulDiffCommand (rv->region()));

			rv->drag_end ();
		}
		_editor->commit_reversible_command ();
	}
}

void
RegionSlipContentsDrag::aborted (bool movement_occurred)
{
	/* ToDo: revert to the original region properties */
	_editor->abort_reversible_command ();
}


RegionBrushDrag::RegionBrushDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
{
	DEBUG_TRACE (DEBUG::Drags, "New RegionBrushDrag\n");
	_y_constrained = true;
}

void
RegionBrushDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, _editor->cursors()->trimmer);
}

void
RegionBrushDrag::motion (GdkEvent* event, bool first_move)
{
	if (first_move) {
		_editor->begin_reversible_command (_("Region brush drag"));
		_already_pasted.insert(_primary->region()->position());
	} else {
		timepos_t snapped (adjusted_current_time (event, false));
		_editor->snap_to (snapped, RoundDownAlways, SnapToGrid_Scaled, false);
		if(_already_pasted.find(snapped) == _already_pasted.end()) {
			_editor->mouse_brush_insert_region (_primary, snapped);
			_already_pasted.insert(snapped);
		}
	}
}

void
RegionBrushDrag::finished (GdkEvent *, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	PlaylistSet modified_playlists;
	modified_playlists.insert(_primary->region()->playlist());
	add_stateful_diff_commands_for_playlists(modified_playlists);
	_editor->commit_reversible_command ();
	_already_pasted.clear();
}

void
RegionBrushDrag::aborted (bool movement_occurred)
{
	_already_pasted.clear();

	/* ToDo: revert to the original playlist properties */
	_editor->abort_reversible_command ();
}

RegionMotionDrag::RegionMotionDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
	, _ignore_video_lock (false)
	, _total_x_delta (0)
	, _last_pointer_time_axis_view (0)
	, _last_pointer_layer (0)
	, _ndropzone (0)
	, _pdropzone (0)
	, _ddropzone (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New RegionMotionDrag\n");
}

void
RegionMotionDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);
	setup_snap_delta (_last_position);

	show_verbose_cursor_time (_last_position);
	show_view_preview (_last_position + _video_offset);

	/* this conditional is required because drag-n-drop'ed regions end up
	 * here, and at this point they are not attached to a playlist.
	 */

	if (_editor->should_ripple() && _primary && _primary->region() && _primary->region()->playlist()) {
		_earliest_time_limit = _primary->region()->playlist()->find_prev_region_start (_primary->region()->position());
	}

	pair<TimeAxisView*, double> const tv = _editor->trackview_by_y_position (current_pointer_y ());
	if (tv.first) {
		_last_pointer_time_axis_view = find_time_axis_view (tv.first);
		assert(_last_pointer_time_axis_view >= 0);
		_last_pointer_layer = tv.first->layer_display() == Overlaid ? 0 : tv.second;
	}

	if (Keyboard::modifier_state_equals (event->button.state, Keyboard::ModifierMask (Keyboard::TertiaryModifier))) {
		_ignore_video_lock = true;
	}

	if (_editor->should_ripple()) {
		/* we do not drag across tracks when rippling or brushing */
		_y_constrained = true;
	}
}

double
RegionMotionDrag::compute_x_delta (GdkEvent const * event, Temporal::timepos_t & pending_region_position)
{
	/* compute the amount of pointer motion in samples, and where
	   the region would be if we moved it by that much.
	*/
	if (_x_constrained) {
		pending_region_position = _last_position;
		return 0.0;
	}

	pending_region_position = adjusted_time (_drags->current_pointer_time (), event, false);

	timecnt_t sync_offset;
	int32_t sync_dir;

	sync_offset = _primary->region()->sync_offset (sync_dir);

	/* we don't handle a sync point that lies before zero.
	 */
	if (sync_dir >= 0 || (sync_dir < 0 && pending_region_position >= sync_offset)) {

		timecnt_t const sd = snap_delta (event->button.state);
		timepos_t sync_snap (pending_region_position + (sync_offset * sync_dir) + sd);
		_editor->snap_to_with_modifier (sync_snap, event);
		if (sync_offset.is_zero () && sd.is_zero ()) {
			pending_region_position = sync_snap;
		} else {
			pending_region_position = _primary->region()->adjust_to_sync (sync_snap).earlier (sd);
		}
	} else {
		pending_region_position = _last_position;
	}

	if (pending_region_position > timepos_t::max (pending_region_position.time_domain()).earlier (_primary->region()->length())) {
		pending_region_position = _last_position;
	}

	if (!_earliest_time_limit.is_zero () && pending_region_position <= _earliest_time_limit) {
		pending_region_position = _earliest_time_limit;
		return 0.0;
	}

	double dx = 0;

	bool const x_move_allowed = !_x_constrained;

	if ((pending_region_position != _last_position) && x_move_allowed) {

		/* x movement since last time (in pixels) */
		dx = _editor->duration_to_pixels_unrounded (_last_position.distance (pending_region_position));

		/* total x movement */
		timecnt_t total_dx = timecnt_t (pixel_to_time (_total_x_delta + dx), grab_time());

		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			const timepos_t off = i->view->region()->position() + total_dx;
			if (off.is_negative()) {
				dx = dx - _editor->time_to_pixel_unrounded (off);
				pending_region_position = pending_region_position.earlier (timecnt_t (off, timepos_t (pending_region_position.time_domain())));
				break;
			}
		}
	}

	_editor->set_snapped_cursor_position (pending_region_position);

	return dx;
}

int
RegionDrag::apply_track_delta (const int start, const int delta, const int skip, const bool distance_only) const
{
	if (delta == 0) {
		return start;
	}

	const int tavsize  = _time_axis_views.size();
	const int dt = delta > 0 ? +1 : -1;
	int current  = start;
	int target   = start + delta - skip;

	assert (current < 0 || current >= tavsize || !_time_axis_views[current]->hidden());
	assert (skip == 0 || (skip < 0 && delta < 0) || (skip > 0 && delta > 0));

	while (current >= 0 && current != target) {
		current += dt;
		if (current < 0 && dt < 0) {
			break;
		}
		if (current >= tavsize && dt > 0) {
			break;
		}
		if (current < 0 || current >= tavsize) {
			continue;
		}

		RouteTimeAxisView const * rtav = dynamic_cast<RouteTimeAxisView const *> (_time_axis_views[current]);
		if (_time_axis_views[current]->hidden() || !rtav || !rtav->is_track()) {
			target += dt;
		}

		if (distance_only && current == start + delta) {
			break;
		}
	}
	return target;
}

bool
RegionMotionDrag::y_movement_allowed (int delta_track, double delta_layer, int skip_invisible) const
{
	if (_y_constrained) {
		return false;
	}

	const int tavsize  = _time_axis_views.size();
	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		int n = apply_track_delta (i->time_axis_view, delta_track, skip_invisible);
		assert (n < 0 || n >= tavsize || !_time_axis_views[n]->hidden());

		if (i->time_axis_view < 0 || i->time_axis_view >= tavsize) {
			/* already in the drop zone */
			if (delta_track >= 0) {
				/* downward motion - OK if others are still not in the dropzone */
				continue;
			}

		}

		if (n < 0) {
			/* off the top */
			return false;
		} else if (n >= tavsize) {
			/* downward motion into drop zone. That's fine. */
			continue;
		}

		RouteTimeAxisView const * to = dynamic_cast<RouteTimeAxisView const *> (_time_axis_views[n]);
		if (to == 0 || to->hidden() || !to->is_track() || to->track()->data_type() != i->view->region()->data_type()) {
			/* not a track, or the wrong type */
			return false;
		}

		double const l = i->layer + delta_layer;

		/* Note that we allow layer to be up to 0.5 below zero, as this is used by `Expanded'
		   mode to allow the user to place a region below another on layer 0.
		*/
		if (delta_track == 0 && (l < -0.5 || l >= int (to->view()->layers()))) {
			/* Off the top or bottom layer; note that we only refuse if the track hasn't changed.
			   If it has, the layers will be munged later anyway, so it's ok.
			*/
			return false;
		}
	}

	/* all regions being dragged are ok with this change */
	return true;
}

struct DraggingViewSorter {
	bool operator() (const DraggingView& a, const DraggingView& b) {
		return a.time_axis_view < b.time_axis_view;
	}
};

void
RegionMotionDrag::collect_ripple_views ()
{
	RegionSelection copy;

	_editor->get_regionviews_at_or_after (_primary->region()->position(), copy);

	for (RegionSelection::reverse_iterator i = copy.rbegin(); i != copy.rend(); ++i) {
		if (!_editor->selection->regions.contains (*i)) {
			_views.push_back (DraggingView (*i, this, &(*i)->get_time_axis_view()));
		}
	}

	_editor->get_markers_to_ripple (_primary->region()->playlist(), _primary->region()->position(), ripple_markers);
}

void
RegionMotionDrag::motion (GdkEvent* event, bool first_move)
{
	double delta_layer = 0;
	int delta_time_axis_view = 0;
	int current_pointer_time_axis_view = -1;

	assert (!_views.empty ());

	/* Note: time axis views in this method are often expressed as an index into the _time_axis_views vector */

	/* Find the TimeAxisView that the pointer is now over */
	const double cur_y = current_pointer_y ();
	pair<TimeAxisView*, double> const r = _editor->trackview_by_y_position (cur_y);
	TimeAxisView* tv = r.first;

	if (!tv && cur_y < 0) {
		/* above trackview area, autoscroll hasn't moved us since last time, nothing to do */
		return;
	}

	/* find drop-zone y-position */
	Coord last_track_bottom_edge;
	last_track_bottom_edge = 0;
	for (std::vector<TimeAxisView*>::reverse_iterator t = _time_axis_views.rbegin(); t != _time_axis_views.rend(); ++t) {
		if (!(*t)->hidden()) {
			last_track_bottom_edge = (*t)->canvas_display()->canvas_origin ().y + (*t)->effective_height();
			break;
		}
	}

	if (tv && tv->view()) {
		/* the mouse is over a track */
		double layer = r.second;

		if (first_move && tv->view()->layer_display() == Stacked) {
			tv->view()->set_layer_display (Expanded);
		}

		/* Here's the current pointer position in terms of time axis view and layer */
		current_pointer_time_axis_view = find_time_axis_view (tv);
		assert(current_pointer_time_axis_view >= 0);

		double const current_pointer_layer = tv->layer_display() == Overlaid ? 0 : layer;

		/* Work out the change in y */

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);
		if (!rtv || !rtv->is_track()) {
			/* ignore non-tracks early on. we can't move any regions on them */
		} else if (_last_pointer_time_axis_view < 0) {
			/* Was in the drop-zone, now over a track.
			 * Hence it must be an upward move (from the bottom)
			 *
			 * track_index is still -1, so delta must be set to
			 * move up the correct number of tracks from the bottom.
			 *
			 * This is necessary because steps may be skipped if
			 * the bottom-most track is not a valid target and/or
			 * if there are hidden tracks at the bottom.
			 * Hence the initial offset (_ddropzone) as well as the
			 * last valid pointer position (_pdropzone) need to be
			 * taken into account.
			 */
			delta_time_axis_view = current_pointer_time_axis_view - _time_axis_views.size () + _ddropzone - _pdropzone;
		} else {
			delta_time_axis_view = current_pointer_time_axis_view - _last_pointer_time_axis_view;
		}

		/* TODO needs adjustment per DraggingView,
		 *
		 * e.g. select one region on the top-layer of a track
		 * and one region which is at the bottom-layer of another track
		 * drag both.
		 *
		 * Indicated drop-zones and layering is wrong.
		 * and may infer additional layers on the target-track
		 * (depending how many layers the original track had).
		 *
		 * Or select two regions (different layers) on a same track,
		 * move across a non-layer track.. -> layering info is lost.
		 * on drop either of the regions may be on top.
		 *
		 * Proposed solution: screw it :) well,
		 *   don't use delta_layer, use an absolute value
		 *   1) remember DraggingView's layer  as float 0..1  [current layer / all layers of source]
		 *   2) calculate current mouse pos, y-pos inside track divided by height of mouse-over track.
		 *   3) iterate over all DraggingView, find the one that is over the track with most layers
		 *   4) proportionally scale layer to layers available on target
		 */
		delta_layer = current_pointer_layer - _last_pointer_layer;

	}
	/* for automation lanes, there is a TimeAxisView but no ->view()
	 * if (!tv) -> dropzone
	 */
	else if (!tv && cur_y >= 0 && _last_pointer_time_axis_view >= 0) {
		/* Moving into the drop-zone.. */
		delta_time_axis_view = _time_axis_views.size () - _last_pointer_time_axis_view;
		/* delta_time_axis_view may not be sufficient to move into the DZ
		 * the mouse may enter it, but it may not be a valid move due to
		 * constraints.
		 *
		 * -> remember the delta needed to move into the dropzone
		 */
		_ddropzone = delta_time_axis_view;
		/* ..but subtract hidden tracks (or routes) at the bottom.
		 * we silently move mover them
		 */
		_ddropzone -= apply_track_delta(_last_pointer_time_axis_view, delta_time_axis_view, 0, true)
			      - _time_axis_views.size();
	}
	else if (!tv && cur_y >= 0 && _last_pointer_time_axis_view < 0) {
		/* move around inside the zone.
		 * This allows to move further down until all regions are in the zone.
		 */
		const double ptr_y = cur_y + _editor->get_trackview_group()->canvas_origin().y;
		assert(ptr_y >= last_track_bottom_edge);
		assert(_ddropzone > 0);

		/* calculate mouse position in 'tracks' below last track. */
		const double dzi_h = TimeAxisView::preset_height (HeightNormal);
		uint32_t dzpos = _ddropzone + floor((1 + ptr_y - last_track_bottom_edge) / dzi_h);

		if (dzpos > _pdropzone && _ndropzone < _ntracks) {
			// move further down
			delta_time_axis_view =  dzpos - _pdropzone;
		} else if (dzpos < _pdropzone && _ndropzone > 0) {
			// move up inside the DZ
			delta_time_axis_view =  dzpos - _pdropzone;
		}
	}

	/* Work out the change in x */
	timepos_t pending_region_position;
	double const x_delta = compute_x_delta (event, pending_region_position);

	Temporal::Beats const last_pos_qn = _last_position.beats();
	Temporal::Beats const qn_delta = pending_region_position.beats() - last_pos_qn;

	_last_position = pending_region_position;

	/* calculate hidden tracks in current y-axis delta */
	int delta_skip = 0;
	if (_last_pointer_time_axis_view < 0 && _pdropzone > 0) {
		/* The mouse is more than one track below the dropzone.
		 * distance calculation is not needed (and would not work, either
		 * because the dropzone is "packed").
		 *
		 * Except when [partially] moving regions out of dropzone in a large step.
		 * (the mouse may or may not remain in the DZ)
		 * Hidden tracks at the bottom of the TAV need to be skipped.
		 *
		 * This also handles the case if the mouse entered the DZ
		 * in a large step (exessive delta), either due to fast-movement,
		 * autoscroll, laggy UI. _ddropzone copensates for that (see "move into dz" above)
		 */
		if (delta_time_axis_view < 0 && (int)_ddropzone - delta_time_axis_view >= (int)_pdropzone) {
			const int dt = delta_time_axis_view + (int)_pdropzone - (int)_ddropzone;
			assert(dt <= 0);
			delta_skip = apply_track_delta(_time_axis_views.size(), dt, 0, true)
				-_time_axis_views.size() - dt;
		}
	}
	else if (_last_pointer_time_axis_view < 0) {
		/* Moving out of the zone. Check for hidden tracks at the bottom. */
		delta_skip = apply_track_delta(_time_axis_views.size(), delta_time_axis_view, 0, true)
			     -_time_axis_views.size() - delta_time_axis_view;
	} else {
		/* calculate hidden tracks that are skipped by the pointer movement */
		delta_skip = apply_track_delta(_last_pointer_time_axis_view, delta_time_axis_view, 0, true)
			     - _last_pointer_time_axis_view
		             - delta_time_axis_view;
	}

	/* Verify change in y */
	if (!y_movement_allowed (delta_time_axis_view, delta_layer, delta_skip)) {
		/* this y movement is not allowed, so do no y movement this time */
		delta_time_axis_view = 0;
		delta_layer = 0;
		delta_skip = 0;
	}

	if (x_delta == 0 && (tv && tv->view() && delta_time_axis_view == 0) && delta_layer == 0 && !first_move) {
		/* haven't reached next snap point, and we're not switching
		   trackviews nor layers. nothing to do.
		*/
		return;
	}

	typedef map<boost::shared_ptr<Playlist>, double> PlaylistDropzoneMap;
	PlaylistDropzoneMap playlist_dropzone_map;
	_ndropzone = 0; // number of elements currently in the dropzone

	if (first_move) {
		/* sort views by time_axis.
		 * This retains track order in the dropzone, regardless
		 * of actual selection order
		 */
		_views.sort (DraggingViewSorter());

		/* count number of distinct tracks of all regions
		 * being dragged, used for dropzone.
		 */
		int prev_track = -1;
		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			if ((int) i->time_axis_view != prev_track) {
				prev_track = i->time_axis_view;
				++_ntracks;
			}
		}
#ifndef NDEBUG
		int spread =
			_views.back().time_axis_view -
			_views.front().time_axis_view;

		spread -= apply_track_delta (_views.front().time_axis_view, spread, 0, true)
		          -  _views.back().time_axis_view;

		printf("Dragging region(s) from %d different track(s), max dist: %d\n", _ntracks, spread);
#endif
	}

	if (x_delta) {
		for (vector<ArdourMarker*>::iterator m = ripple_markers.begin(); m != ripple_markers.end(); ++m) {
			(*m)->the_item().move (Duple (x_delta, 0.));
		}
	}

	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {

		RegionView* rv = i->view;
		double y_delta;

		y_delta = 0;

		if (rv->region()->locked() || (rv->region()->video_locked() && !_ignore_video_lock)) {
			continue;
		}

		if (first_move) {
			rv->drag_start ();

			/* reparent the regionview into a group above all
			 * others
			 */

			ArdourCanvas::Item* rvg = rv->get_canvas_group();
			Duple rv_canvas_offset = rvg->parent()->canvas_origin ();
			Duple dmg_canvas_offset = _editor->_drag_motion_group->canvas_origin ();
			rv->get_canvas_group()->reparent (_editor->_drag_motion_group);
			/* move the item so that it continues to appear at the
			   same location now that its parent has changed.
			   */
			rvg->move (rv_canvas_offset - dmg_canvas_offset);
		}

		/* If we have moved tracks, we'll fudge the layer delta so that the
		   region gets moved back onto layer 0 on its new track; this avoids
		   confusion when dragging regions from non-zero layers onto different
		   tracks.
		*/
		double this_delta_layer = delta_layer;
		if (delta_time_axis_view != 0) {
			this_delta_layer = - i->layer;
		}

		int this_delta_time_axis_view = apply_track_delta(i->time_axis_view, delta_time_axis_view, delta_skip) - i->time_axis_view;

		int track_index = i->time_axis_view + this_delta_time_axis_view;
		assert(track_index >= 0);

		if (track_index < 0 || track_index >= (int) _time_axis_views.size()) {
			/* Track is in the Dropzone */

			i->time_axis_view = track_index;
			assert(i->time_axis_view >= (int) _time_axis_views.size());
			if (cur_y >= 0) {

				double yposition = 0;
				PlaylistDropzoneMap::iterator pdz = playlist_dropzone_map.find (i->view->region()->playlist());
				rv->set_height (TimeAxisView::preset_height (HeightNormal));
				++_ndropzone;

				/* store index of each new playlist as a negative count, starting at -1 */

				if (pdz == playlist_dropzone_map.end()) {
					/* compute where this new track (which doesn't exist yet) will live
					   on the y-axis.
					*/
					yposition = last_track_bottom_edge; /* where to place the top edge of the regionview */

					/* How high is this region view ? */

					boost::optional<ArdourCanvas::Rect> obbox = rv->get_canvas_group()->bounding_box ();
					ArdourCanvas::Rect bbox;

					if (obbox) {
						bbox = obbox.get ();
					}

					last_track_bottom_edge += bbox.height();

					playlist_dropzone_map.insert (make_pair (i->view->region()->playlist(), yposition));

				} else {
					yposition = pdz->second;
				}

				/* values are zero or negative, hence the use of min() */
				y_delta = yposition - rv->get_canvas_group()->canvas_origin().y;
			}

			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(rv);
			if (mrv) {
				mrv->apply_note_range (60, 71, true);
			}
		} else {

			/* The TimeAxisView that this region is now over */
			TimeAxisView* current_tv = _time_axis_views[track_index];

			/* Ensure it is moved from stacked -> expanded if appropriate */
			if (current_tv->view()->layer_display() == Stacked) {
				current_tv->view()->set_layer_display (Expanded);
			}

			/* We're only allowed to go -ve in layer on Expanded views */
			if (current_tv->view()->layer_display() != Expanded && (i->layer + this_delta_layer) < 0) {
				this_delta_layer = - i->layer;
			}

			/* Set height */
			rv->set_height (current_tv->view()->child_height ());

			/* Update show/hidden status as the region view may have come from a hidden track,
			   or have moved to one.
			*/
			if (current_tv->hidden ()) {
				rv->get_canvas_group()->hide ();
			} else {
				rv->get_canvas_group()->show ();
			}

			/* Update the DraggingView */
			i->time_axis_view = track_index;
			i->layer += this_delta_layer;

			Duple track_origin;

			/* Get the y coordinate of the top of the track that this region is now over */
			track_origin = current_tv->canvas_display()->item_to_canvas (track_origin);

			/* And adjust for the layer that it should be on */
			StreamView* cv = current_tv->view ();
			switch (cv->layer_display ()) {
			case Overlaid:
				break;
			case Stacked:
				track_origin.y += (cv->layers() - i->layer - 1) * cv->child_height ();
				break;
			case Expanded:
				track_origin.y += (cv->layers() - i->layer - 0.5) * 2 * cv->child_height ();
				break;
			}

			/* need to get the parent of the regionview
			 * canvas group and get its position in
			 * equivalent coordinate space as the trackview
			 * we are now dragging over.
			 */

			y_delta = track_origin.y - rv->get_canvas_group()->canvas_origin().y;

			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(rv);
			if (mrv) {
				MidiStreamView* msv;
				if ((msv = dynamic_cast <MidiStreamView*> (current_tv->view())) != 0) {
					mrv->apply_note_range (msv->lowest_note(), msv->highest_note(), true);
				}
			}
		}

		/* Now move the region view */
		if (rv->region()->position_time_domain() == Temporal::BeatTime) {
			Temporal::Beats const last_qn = rv->get_position().beats();
			rv->set_position (timepos_t (last_qn + qn_delta), 0);
			rv->move (0, y_delta);
		} else {
			rv->move (x_delta, y_delta);
		}
	} /* foreach region */

	_total_x_delta += x_delta;

	if (x_delta != 0) {
		show_verbose_cursor_time (_last_position);
		show_view_preview (_last_position + _video_offset);
	}

	/* keep track of pointer movement */
	if (tv) {
		/* the pointer is currently over a time axis view */

		if (_last_pointer_time_axis_view < 0) {
			/* last motion event was not over a time axis view
			 * or last y-movement out of the dropzone was not valid
			 */
			int dtz = 0;
			if (delta_time_axis_view < 0) {
				/* in the drop zone, moving up */

				/* _pdropzone is the last known pointer y-axis position inside the DZ.
				 * We do not use negative _last_pointer_time_axis_view because
				 * the dropzone is "packed" (the actual track offset is ignored)
				 *
				 * As opposed to the actual number
				 * of elements in the dropzone (_ndropzone)
				 * _pdropzone is not constrained. This is necessary
				 * to allow moving multiple regions with y-distance
				 * into the DZ.
				 *
				 * There can be 0 elements in the dropzone,
				 * even though the drag-pointer is inside the DZ.
				 *
				 * example:
				 * [ Audio-track, Midi-track, Audio-track, DZ ]
				 * move regions from both audio tracks at the same time into the
				 * DZ by grabbing the region in the bottom track.
				 */
				assert(current_pointer_time_axis_view >= 0);
				dtz = std::min((int)_pdropzone, (int)_ddropzone - delta_time_axis_view);
				_pdropzone -= dtz;
			}

			/* only move out of the zone if the movement is OK */
			if (_pdropzone == 0 && delta_time_axis_view != 0) {
				assert(delta_time_axis_view < 0);
				_last_pointer_time_axis_view = current_pointer_time_axis_view;
				/* if all logic and maths are correct, there is no need to assign the 'current' pointer.
				 * the current position can be calculated as follows:
				 */
				// a well placed oofus attack can still throw this off.
				// likely auto-scroll related, printf() debugging may tell, commented out for now.
				//assert (current_pointer_time_axis_view == _time_axis_views.size() - dtz + _ddropzone + delta_time_axis_view);
			}
		} else {
			/* last motion event was also over a time axis view */
			_last_pointer_time_axis_view += delta_time_axis_view;
			assert(_last_pointer_time_axis_view >= 0);
		}

	} else {

		/* the pointer is not over a time axis view */
		assert ((delta_time_axis_view > 0) || (((int)_pdropzone) >= (delta_skip - delta_time_axis_view)));
		_pdropzone += delta_time_axis_view - delta_skip;
		_last_pointer_time_axis_view = -1; // <0 : we're in the zone, value does not matter.
	}

	_last_pointer_layer += delta_layer;
}

void
RegionMoveDrag::motion (GdkEvent* event, bool first_move)
{
	if (first_move && _editor->should_ripple() && !_copy) {
		collect_ripple_views ();
	}

	if (_copy && first_move) {
		if (_x_constrained) {
			_editor->begin_reversible_command (Operations::fixed_time_region_copy);
		} else {
			_editor->begin_reversible_command (Operations::region_copy);
		}

		/* duplicate the regionview(s) and region(s) */

		list<DraggingView> new_regionviews;

		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {

			RegionView* rv = i->view;
			AudioRegionView* arv = dynamic_cast<AudioRegionView*>(rv);
			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(rv);

			const boost::shared_ptr<const Region> original = rv->region();
			boost::shared_ptr<Region> region_copy;

			region_copy = RegionFactory::create (original, true);

			/* need to set this so that the drop zone code can work. This doesn't
			   actually put the region into the playlist, but just sets a weak pointer
			   to it.
			*/
			region_copy->set_playlist (original->playlist());

			RegionView* nrv;
			if (arv) {
				boost::shared_ptr<AudioRegion> audioregion_copy
				= boost::dynamic_pointer_cast<AudioRegion>(region_copy);

				nrv = new AudioRegionView (*arv, audioregion_copy);
			} else if (mrv) {
				boost::shared_ptr<MidiRegion> midiregion_copy
					= boost::dynamic_pointer_cast<MidiRegion>(region_copy);
				nrv = new MidiRegionView (*mrv, midiregion_copy);
			} else {
				continue;
			}

			nrv->get_canvas_group()->show ();
			new_regionviews.push_back (DraggingView (nrv, this, i->initial_time_axis_view));

			/* swap _primary to the copy */

			if (rv == _primary) {
				_primary = nrv;
			}

			/* ..and deselect the one we copied */

			rv->set_selected (false);
		}

		if (!new_regionviews.empty()) {

			/* reflect the fact that we are dragging the copies */

			_views = new_regionviews;

			swap_grab (new_regionviews.front().view->get_canvas_group (), 0, event ? event->motion.time : 0);
		}

	} else if (!_copy && first_move) {
		if (_x_constrained) {
			_editor->begin_reversible_command (_("fixed time region drag"));
		} else {
			_editor->begin_reversible_command (Operations::region_drag);
		}
	}
	RegionMotionDrag::motion (event, first_move);
}

void
RegionMotionDrag::finished (GdkEvent *, bool)
{
	for (vector<TimeAxisView*>::iterator i = _time_axis_views.begin(); i != _time_axis_views.end(); ++i) {
		if (!(*i)->view()) {
			continue;
		}

		if ((*i)->view()->layer_display() == Expanded) {
			(*i)->view()->set_layer_display (Stacked);
		}
	}
}

void
RegionMoveDrag::finished (GdkEvent* ev, bool movement_occurred)
{
	RegionMotionDrag::finished (ev, movement_occurred);

	if (!movement_occurred) {

		/* just a click */

		if (was_double_click() && !_views.empty()) {
			DraggingView dv = _views.front();
			_editor->edit_region (dv.view);
		}

		return;
	}

	assert (!_views.empty ());

	/* We might have hidden region views so that they weren't visible during the drag
	   (when they have been reparented).  Now everything can be shown again, as region
	   views are back in their track parent groups.
	*/
	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		i->view->get_canvas_group()->show ();
	}

	bool const changed_position = (_last_position != _primary->region()->position());
	bool changed_tracks;

	if (_views.front().time_axis_view >= (int) _time_axis_views.size()) {
		/* in the drop zone */
		changed_tracks = true;
	} else {

		if (_views.front().time_axis_view < 0) {
			if (&_views.front().view->get_time_axis_view()) {
				changed_tracks = true;
			} else {
				changed_tracks = false;
			}
		} else {
			changed_tracks = (_time_axis_views[_views.front().time_axis_view] != &_views.front().view->get_time_axis_view());
		}
	}

	if (_copy) {

		finished_copy (
			changed_position,
			changed_tracks,
			_last_position,
			ev->button.state
			);

	} else {

		finished_no_copy (
			changed_position,
			changed_tracks,
			_last_position,
			ev->button.state
			);

	}
}

RouteTimeAxisView*
RegionMoveDrag::create_destination_time_axis (boost::shared_ptr<Region> region, TimeAxisView* original)
{
	if (!ARDOUR_UI_UTILS::engine_is_running ()) {
		return NULL;
	}

	/* Add a new track of the correct type, and return the RouteTimeAxisView that is created to display the
	   new track.
	 */
	TimeAxisView* tav = 0;
	try {
		if (boost::dynamic_pointer_cast<AudioRegion> (region)) {
			list<boost::shared_ptr<AudioTrack> > audio_tracks;
			uint32_t output_chan = region->sources().size();
			if ((Config->get_output_auto_connect() & AutoConnectMaster) && _editor->session()->master_out()) {
				output_chan =  _editor->session()->master_out()->n_inputs().n_audio();
			}
			audio_tracks = _editor->session()->new_audio_track (region->sources().size(), output_chan, 0, 1, region->name(), PresentationInfo::max_order);
			tav =_editor->time_axis_view_from_stripable (audio_tracks.front());
		} else {
			ChanCount one_midi_port (DataType::MIDI, 1);
			list<boost::shared_ptr<MidiTrack> > midi_tracks;
			midi_tracks = _editor->session()->new_midi_track (one_midi_port, one_midi_port,
			                                                  Config->get_strict_io () || Profile->get_mixbus (),
			                                                  boost::shared_ptr<ARDOUR::PluginInfo>(),
			                                                  (ARDOUR::Plugin::PresetRecord*) 0,
			                                                  (ARDOUR::RouteGroup*) 0, 1, region->name(), PresentationInfo::max_order, Normal, true);
			tav = _editor->time_axis_view_from_stripable (midi_tracks.front());
		}

		if (tav) {
			tav->set_height (original->current_height());
		}
	} catch (...) {
		error << _("Could not create new track after region placed in the drop zone") << endmsg;
	}

	return dynamic_cast<RouteTimeAxisView*> (tav);
}

void
RegionMoveDrag::clear_draggingview_list ()
{
	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end();) {
		list<DraggingView>::const_iterator next = i;
		++next;
		delete i->view;
		i = next;
	}
	_views.clear();
}

void
RegionMoveDrag::finished_copy (bool const changed_position, bool const changed_tracks, timepos_t const & last_position, int32_t const ev_state)
{
	RegionSelection new_views;
	PlaylistSet modified_playlists;
	RouteTimeAxisView* new_time_axis_view = 0;

	timecnt_t const drag_delta = _last_position.distance (_primary->region()->position());
	RegionList ripple_exclude;

	/*x_contrained on the same track: this will just make a duplicate region in the same place: abort the operation */
	if (_x_constrained && !changed_tracks) {
		clear_draggingview_list();
		_editor->abort_reversible_command ();
		return;
	}

	typedef map<boost::shared_ptr<Playlist>, RouteTimeAxisView*> PlaylistMapping;
	PlaylistMapping playlist_mapping;

	/* determine boundaries of dragged regions, across all playlists */
	timepos_t extent_min = timepos_t::max(_primary->region()->position().time_domain());
	timepos_t extent_max;

	/* insert the regions into their (potentially) new (or existing) playlists */
	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {

		RouteTimeAxisView* dest_rtv = 0;

		if (i->view->region()->locked() || (i->view->region()->video_locked() && !_ignore_video_lock)) {
			continue;
		}

		timepos_t where;

		if (changed_position && !_x_constrained) {
			where = timepos_t (i->view->region()->position().earlier (drag_delta));
		} else {
			where = timepos_t (i->view->region()->position());
		}

		/* compute full extent of regions that we're going to insert */

		if (where < extent_min) {
			extent_min = where;
		}

		if (where + i->view->region()->length() > extent_max) {
			extent_max = where + i->view->region()->length();
		}

		if (i->time_axis_view < 0 || i->time_axis_view >= (int)_time_axis_views.size()) {
			/* dragged to drop zone */

			PlaylistMapping::iterator pm;

			if ((pm = playlist_mapping.find (i->view->region()->playlist())) == playlist_mapping.end()) {
				/* first region from this original playlist: create a new track */
				new_time_axis_view = create_destination_time_axis (i->view->region(), i->initial_time_axis_view);
				if(!new_time_axis_view) {
					Drag::abort();
					return;
				}
				playlist_mapping.insert (make_pair (i->view->region()->playlist(), new_time_axis_view));
				dest_rtv = new_time_axis_view;
			} else {
				/* we already created a new track for regions from this playlist, use it */
				dest_rtv = pm->second;
			}
		} else {
			/* destination time axis view is the one we dragged to */
			dest_rtv = dynamic_cast<RouteTimeAxisView*> (_time_axis_views[i->time_axis_view]);
		}

		if (dest_rtv != 0) {
			RegionView* new_view;
			if (i->view == _primary && !_x_constrained) {
				new_view = insert_region_into_playlist (i->view->region(), dest_rtv, i->layer, last_position,
									modified_playlists, true);
			} else {
				if (i->view->region()->position_time_domain() == Temporal::AudioTime) {
					new_view = insert_region_into_playlist (i->view->region(), dest_rtv, i->layer, where,
										modified_playlists);
				} else {
					new_view = insert_region_into_playlist (i->view->region(), dest_rtv, i->layer, where,
										modified_playlists, true);
				}
			}

			if (new_view != 0) {
				new_views.push_back (new_view);
				ripple_exclude.push_back (new_view->region());
			}
		}
	}

	/* in the past this was done in the main iterator loop; no need */
	clear_draggingview_list();

	for (PlaylistSet::iterator p = modified_playlists.begin(); p != modified_playlists.end(); ++p) {
		if (_editor->should_ripple()) {
			(*p)->ripple (extent_min, extent_min.distance (extent_max), &ripple_exclude);
		}
		(*p)->rdiff_and_add_command (_editor->session());
	}

	/* Ripple marks & ranges if appropriate */

	if (Config->get_edit_mode() == RippleAll) {
		_editor->ripple_marks (_primary->region()->playlist(), extent_min, extent_min.distance (extent_max));
	}

	/* If we've created new regions either by copying or moving
	   to a new track, we want to replace the old selection with the new ones
	*/

	if (new_views.size() > 0) {
		_editor->selection->set (new_views);
	}

	_editor->commit_reversible_command ();
}

void
RegionMoveDrag::finished_no_copy (
	bool const changed_position,
	bool const changed_tracks,
	timepos_t const & last_position,
	int32_t const ev_state
	)
{
	RegionSelection new_views;
	PlaylistSet modified_playlists;
	PlaylistSet frozen_playlists;
	set<RouteTimeAxisView*> views_to_update;
	RouteTimeAxisView* new_time_axis_view = 0;

	timecnt_t const drag_delta = last_position.distance (_primary->region()->position());
	RegionList ripple_exclude;

	typedef map<boost::shared_ptr<Playlist>, RouteTimeAxisView*> PlaylistMapping;
	PlaylistMapping playlist_mapping;

	/* determine earliest position affected by the drag, across all playlists */
	timepos_t extent_min = timepos_t::max (Temporal::AudioTime); /* NUTEMPO fix domain */

	std::set<boost::shared_ptr<const Region> > uniq;
	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ) {

		RegionView* rv = i->view;
		RouteTimeAxisView* dest_rtv = 0;

		if (rv->region()->locked() || (rv->region()->video_locked() && !_ignore_video_lock)) {
			++i;
			continue;
		}

		if (uniq.find (rv->region()) != uniq.end()) {
			/* prevent duplicate moves when selecting regions from shared playlists */
			++i;
			continue;
		}
		uniq.insert(rv->region());

		if (i->time_axis_view < 0 || i->time_axis_view >= (int)_time_axis_views.size()) {
			/* dragged to drop zone */

			PlaylistMapping::iterator pm;

			if ((pm = playlist_mapping.find (i->view->region()->playlist())) == playlist_mapping.end()) {
				/* first region from this original playlist: create a new track */
				new_time_axis_view = create_destination_time_axis (i->view->region(), i->initial_time_axis_view);
				if(!new_time_axis_view) { // New track creation failed
					Drag::abort();
					return;
				}
				playlist_mapping.insert (make_pair (i->view->region()->playlist(), new_time_axis_view));
				dest_rtv = new_time_axis_view;
			} else {
				/* we already created a new track for regions from this playlist, use it */
				dest_rtv = pm->second;
			}

		} else {
			/* destination time axis view is the one we dragged to */
			dest_rtv = dynamic_cast<RouteTimeAxisView*> (_time_axis_views[i->time_axis_view]);
		}

		assert (dest_rtv);

		double const dest_layer = i->layer;

		views_to_update.insert (dest_rtv);

		timepos_t where;

		if (changed_position && !_x_constrained) {
			where = rv->region()->position().earlier (drag_delta);
		} else {
			where = rv->region()->position();
		}

		/* compute full extent of regions that we're going to insert */

		if (rv->region()->position() < extent_min) {
			extent_min = rv->region()->position ();
		}

		cerr << "drag emin " << extent_min << " emax " << drag_delta << endl;

		if (changed_tracks) {

			/* insert into new playlist */
			RegionView* new_view;
			if (rv == _primary && !_x_constrained) {
				new_view = insert_region_into_playlist (
					RegionFactory::create (rv->region (), true), dest_rtv, dest_layer, last_position,modified_playlists, true);
			} else {
				if (rv->region()->position_time_domain() == Temporal::AudioTime) {

					new_view = insert_region_into_playlist (
						RegionFactory::create (rv->region (), true), dest_rtv, dest_layer, where, modified_playlists);
				} else {
					new_view = insert_region_into_playlist (
						RegionFactory::create (rv->region (), true), dest_rtv, dest_layer, where, modified_playlists, true);
				}
			}

			if (new_view == 0) {
				++i;
				continue;
			}

			new_views.push_back (new_view);

			/* remove from old playlist */

			/* the region that used to be in the old playlist is not
			   moved to the new one - we use a copy of it. as a result,
			   any existing editor for the region should no longer be
			   visible.
			*/
			rv->hide_region_editor();


			remove_region_from_playlist (rv->region(), i->initial_playlist, modified_playlists);

		} else {

			boost::shared_ptr<Playlist> playlist = dest_rtv->playlist();

			/* this movement may result in a crossfade being modified, or a layering change,
			   so we need to get undo data from the playlist as well as the region.
			*/

			pair<PlaylistSet::iterator, bool> r = modified_playlists.insert (playlist);
			if (r.second) {
				playlist->clear_changes ();
			}

			rv->region()->clear_changes ();

			/*
			   motion on the same track. plonk the previously reparented region
			   back to its original canvas group (its streamview).
			   No need to do anything for copies as they are fake regions which will be deleted.
			*/

			rv->get_canvas_group()->reparent (dest_rtv->view()->canvas_item());
			rv->get_canvas_group()->set_y_position (i->initial_y);
			rv->drag_end ();

			/* just change the model */
			if (dest_rtv->view()->layer_display() == Stacked || dest_rtv->view()->layer_display() == Expanded) {
				playlist->set_layer (rv->region(), dest_layer);
			}

			/* freeze playlist to avoid lots of relayering in the case of a multi-region drag */

			r = frozen_playlists.insert (playlist);

			if (r.second) {
				playlist->freeze ();
			}

			rv->region()->set_position (where);

			_editor->session()->add_command (new StatefulDiffCommand (rv->region()));
		}

		// ripple_exclude.push_back (i->view->region());

		if (changed_tracks) {

			/* OK, this is where it gets tricky. If the playlist was being used by >1 tracks, and the region
			   was selected in all of them, then removing it from a playlist will have removed all
			   trace of it from _views (i.e. there were N regions selected, we removed 1,
			   but since its the same playlist for N tracks, all N tracks updated themselves, removed the
			   corresponding regionview, and _views is now empty).

			   This could have invalidated any and all iterators into _views.

			   The heuristic we use here is: if the region selection is empty, break out of the loop
			   here. if the region selection is not empty, then restart the loop because we know that
			   we must have removed at least the region(view) we've just been working on as well as any
			   that we processed on previous iterations.

			   EXCEPT .... if we are doing a copy drag, then _views hasn't been modified and
			   we can just iterate.
			*/


			if (_views.empty()) {
				break;
			} else {
				i = _views.begin();
			}

		} else {
			++i;
		}
	}

	for (set<boost::shared_ptr<Playlist> >::iterator p = frozen_playlists.begin(); p != frozen_playlists.end(); ++p) {
		(*p)->thaw();
	}

	if (Config->get_edit_mode() == RippleAll) {
		_editor->ripple_marks (_primary->region()->playlist(), extent_min, -drag_delta);
	}

	/* If we've created new regions either by copying or moving
	   to a new track, we want to replace the old selection with the new ones
	*/

	if (new_views.size() > 0) {
		_editor->selection->set (new_views);
	}

	_editor->commit_reversible_command ();

	/* We have futzed with the layering of canvas items on our streamviews.
	   If any region changed layer, this will have resulted in the stream
	   views being asked to set up their region views, and all will be well.
	   If not, we might now have badly-ordered region views.  Ask the StreamViews
	   involved to sort themselves out, just in case.
	*/

	for (set<RouteTimeAxisView*>::iterator i = views_to_update.begin(); i != views_to_update.end(); ++i) {
		(*i)->view()->playlist_layered ((*i)->track ());
	}
}

/** Remove a region from a playlist, clearing the diff history of the playlist first if necessary.
 *  @param region Region to remove.
 *  @param playlist playlist To remove from.
 *  @param modified_playlists The playlist will be added to this if it is not there already; used to ensure
 *  that clear_changes () is only called once per playlist.
 */
void
RegionMoveDrag::remove_region_from_playlist (
	boost::shared_ptr<Region> region,
	boost::shared_ptr<Playlist> playlist,
	PlaylistSet& modified_playlists
	)
{
	pair<set<boost::shared_ptr<Playlist> >::iterator, bool> r = modified_playlists.insert (playlist);

	if (r.second) {
		playlist->clear_changes ();
	}

	/* XX NEED TO RIPPLE */

	playlist->remove_region (region);
}


/** Insert a region into a playlist, handling the recovery of the resulting new RegionView, and
 *  clearing the playlist's diff history first if necessary.
 *  @param region Region to insert.
 *  @param dest_rtv Destination RouteTimeAxisView.
 *  @param dest_layer Destination layer.
 *  @param where Destination position.
 *  @param modified_playlists The playlist will be added to this if it is not there already; used to ensure
 *  that clear_changes () is only called once per playlist.
 *  @return New RegionView, or 0 if no insert was performed.
 */
RegionView *
RegionMoveDrag::insert_region_into_playlist (
	boost::shared_ptr<Region> region,
	RouteTimeAxisView*        dest_rtv,
	layer_t                   dest_layer,
	timepos_t const &         where,
	PlaylistSet&              modified_playlists,
	bool                      for_music
	)
{
	boost::shared_ptr<Playlist> dest_playlist = dest_rtv->playlist ();
	if (!dest_playlist) {
		return 0;
	}

	/* arrange to collect the new region view that will be created as a result of our playlist insertion */
	_new_region_view = 0;
	sigc::connection c = dest_rtv->view()->RegionViewAdded.connect (sigc::mem_fun (*this, &RegionMoveDrag::collect_new_region_view));

	/* clear history for the playlist we are about to insert to, provided we haven't already done so */
	pair<PlaylistSet::iterator, bool> r = modified_playlists.insert (dest_playlist);

	if (r.second) {
		dest_playlist->clear_changes ();
		dest_playlist->clear_owned_changes ();
		/* cannot freeze because we need the new region announcements */
	}

	dest_playlist->add_region (region, where, 1.0, for_music);

	if (dest_rtv->view()->layer_display() == Stacked || dest_rtv->view()->layer_display() == Expanded) {
		dest_playlist->set_layer (region, dest_layer);
	}

	c.disconnect ();

	assert (_new_region_view);

	return _new_region_view;
}

void
RegionMoveDrag::collect_new_region_view (RegionView* rv)
{
	_new_region_view = rv;
}

void
RegionMoveDrag::aborted (bool movement_occurred)
{
	if (_copy) {
		clear_draggingview_list();
	} else {
		RegionMotionDrag::aborted (movement_occurred);
	}
}

void
RegionMotionDrag::aborted (bool)
{
	for (vector<TimeAxisView*>::iterator i = _time_axis_views.begin(); i != _time_axis_views.end(); ++i) {

		StreamView* sview = (*i)->view();

		if (sview) {
			if (sview->layer_display() == Expanded) {
				sview->set_layer_display (Stacked);
			}
		}
	}

	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		RegionView* rv = i->view;
		TimeAxisView* tv = &(rv->get_time_axis_view ());
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);
		assert (rtv);
		rv->get_canvas_group()->reparent (rtv->view()->canvas_item());
		rv->get_canvas_group()->set_y_position (0);
		rv->drag_end ();
		rv->move (-_total_x_delta, 0);
		rv->set_height (rtv->view()->child_height ());
	}

	for (vector<ArdourMarker*>::iterator m = ripple_markers.begin(); m != ripple_markers.end(); ++m) {
		(*m)->the_item().move (Duple (-_total_x_delta, 0.));
	}
}

/** @param b true to brush, otherwise false.
 *  @param c true to make copies of the regions being moved, otherwise false.
 */
RegionMoveDrag::RegionMoveDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v, bool c)
	: RegionMotionDrag (e, i, p, v)
	, _copy (c)
	, _new_region_view (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New RegionMoveDrag\n");

	_last_position = _primary->region()->position();
}

void
RegionMoveDrag::setup_pointer_offset ()
{
	_pointer_offset = timecnt_t (_last_position.distance (raw_grab_time()), _last_position);
}

RegionInsertDrag::RegionInsertDrag (Editor* e, boost::shared_ptr<Region> r, RouteTimeAxisView* v, timepos_t const & pos)
#warning nutempo pass pos.time_domain () up to Drag's c'tor
	: RegionMotionDrag (e, 0, 0, list<RegionView*> ())
{
	DEBUG_TRACE (DEBUG::Drags, "New RegionInsertDrag\n");

	assert ((boost::dynamic_pointer_cast<AudioRegion> (r) && dynamic_cast<AudioTimeAxisView*> (v)) ||
		(boost::dynamic_pointer_cast<MidiRegion> (r) && dynamic_cast<MidiTimeAxisView*> (v)));

	_primary = v->view()->create_region_view (r, false, false);

	_primary->get_canvas_group()->show ();
	_primary->set_position (pos, 0);
	_views.push_back (DraggingView (_primary, this, v));

	_last_position = pos;

	_item = _primary->get_canvas_group ();
}

void
RegionInsertDrag::finished (GdkEvent * event, bool)
{
	int pos = _views.front().time_axis_view;
	assert(pos >= 0 && pos < (int)_time_axis_views.size());

	RouteTimeAxisView* dest_rtv = dynamic_cast<RouteTimeAxisView*> (_time_axis_views[pos]);

	_primary->get_canvas_group()->reparent (dest_rtv->view()->canvas_item());
	_primary->get_canvas_group()->set_y_position (0);

	boost::shared_ptr<Playlist> playlist = dest_rtv->playlist();

	_editor->begin_reversible_command (Operations::insert_region);
	playlist->clear_changes ();
	playlist->clear_owned_changes ();
	_editor->snap_to_with_modifier (_last_position, event);

	playlist->add_region (_primary->region (), _last_position, 1.0, false);

	if (_editor->should_ripple()) {
		playlist->ripple (_last_position, _primary->region()->length(), _primary->region());
	} else {
		playlist->rdiff_and_add_command (_editor->session());
	}

	_editor->commit_reversible_command ();

	delete _primary;
	_primary = 0;
	_views.clear ();
}

void
RegionInsertDrag::aborted (bool)
{
	delete _primary;
	_primary = 0;
	_views.clear ();
}

/***
 * ripple mode...
 */

void
RegionRippleDrag::add_all_after_to_views(TimeAxisView *tav, timepos_t const & where, const RegionSelection &exclude, bool drag_in_progress)
{

	boost::shared_ptr<RegionList> rl = tav->playlist()->regions_with_start_within (TimelineRange (where, timepos_t::max (where.time_domain()), 0));

	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(tav);
	RegionSelection to_ripple;
	for (RegionList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if ((*i)->position() >= where) {
			to_ripple.push_back (rtv->view()->find_view(*i));
		}
	}

	for (RegionSelection::reverse_iterator i = to_ripple.rbegin(); i != to_ripple.rend(); ++i) {
		if (!exclude.contains (*i)) {
			// the selection has already been added to _views

			if (drag_in_progress) {
				// do the same things that RegionMotionDrag::motion does when
				// first_move is true, for the region views that we're adding
				// to _views this time

				(*i)->drag_start();
				ArdourCanvas::Item* rvg = (*i)->get_canvas_group();
				Duple rv_canvas_offset = rvg->item_to_canvas (Duple (0,0));
				Duple dmg_canvas_offset = _editor->get_drag_motion_group()->canvas_origin ();
				rvg->reparent (_editor->get_drag_motion_group());

				// we only need to move in the y direction
				Duple fudge = rv_canvas_offset - dmg_canvas_offset;
				fudge.x = 0;
				rvg->move (fudge);

			}
			_views.push_back (DraggingView (*i, this, tav));
		}
	}
}

void
RegionRippleDrag::remove_unselected_from_views(timecnt_t const & amount, bool move_regions)
{
	ThawList thawlist;

	for (std::list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ) {
		// we added all the regions after the selection

		std::list<DraggingView>::iterator to_erase = i++;
		if (!_editor->get_selection().regions.contains (to_erase->view)) {
			// restore the non-selected regions to their original playlist & positions,
			// and then ripple them back by the length of the regions that were dragged away
			// do the same things as RegionMotionDrag::aborted

			RegionView *rv = to_erase->view;
			TimeAxisView* tv = &(rv->get_time_axis_view ());
			RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);
			assert (rtv);

			// plonk them back onto their own track
			rv->get_canvas_group()->reparent(rtv->view()->canvas_item());
			rv->get_canvas_group()->set_y_position (0);
			rv->drag_end ();

			if (move_regions) {
				thawlist.add (rv->region ());
				// move the underlying region to match the view
				rv->region()->set_position (rv->region()->position() + amount);
			} else {
				// restore the view to match the underlying region's original position
#warning NUTEMPO ALERT paul test this code in 5.x /* how can this work ... amount used to be in samples but ::move() expect pixels */
				//rv->move(-amount, 0);	// second parameter is y delta - seems 0 is OK
			}

			rv->set_height (rtv->view()->child_height ());
			_views.erase (to_erase);
		}
	}
	thawlist.release ();
}

bool
RegionRippleDrag::y_movement_allowed (int delta_track, double delta_layer, int skip_invisible) const
{
	if (RegionMotionDrag::y_movement_allowed (delta_track, delta_layer, skip_invisible)) {
		if (delta_track) {
			return allow_moves_across_tracks;
		} else {
			return true;
		}
	}
	return false;
}

RegionRippleDrag::RegionRippleDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionMoveDrag (e, i, p, v, false)
{
	DEBUG_TRACE (DEBUG::Drags, "New RegionRippleDrag\n");
	// compute length of selection
	RegionSelection selected_regions = _editor->get_selection().regions;
	selection_length = selected_regions.start_time().distance (selected_regions.end_time());

	// Rippling across tracks disabled. Rippling on all tracks is the way to go in the future.
	allow_moves_across_tracks = false; // (selected_regions.playlists().size() == 1);
	prev_tav = NULL;

	exclude = new RegionList;
	for (RegionSelection::iterator i =selected_regions.begin(); i != selected_regions.end(); ++i) {
		exclude->push_back((*i)->region());
	}

	// also add regions before start of selection to exclude, to be consistent with how Mixbus does ripple
	RegionSelection copy;
	selected_regions.by_position(copy); // get selected regions sorted by position into copy

	std::set<boost::shared_ptr<ARDOUR::Playlist> > playlists = copy.playlists();
	std::set<boost::shared_ptr<ARDOUR::Playlist> >::const_iterator pi;

	bool need_time_domain = true;

	for (pi = playlists.begin(); pi != playlists.end(); ++pi) {
		// find ripple start point on each applicable playlist
		RegionView *first_selected_on_this_track = NULL;
		for (RegionSelection::iterator i = copy.begin(); i != copy.end(); ++i) {
			if ((*i)->region()->playlist() == (*pi)) {
				// region is on this playlist - it's the first, because they're sorted
				first_selected_on_this_track = *i;
				break;
			}
		}

		assert (first_selected_on_this_track); // we should always find the region in one of the playlists...

		TimeAxisView* tav = &first_selected_on_this_track->get_time_axis_view();

		if (need_time_domain) {
			RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (tav);
			prev_amount = timecnt_t (rtav->route()->time_domain());
			need_time_domain = false;
		}

		add_all_after_to_views (tav, first_selected_on_this_track->region()->position(), selected_regions, false);
	}

	if (allow_moves_across_tracks) {
		orig_tav = &(*selected_regions.begin())->get_time_axis_view();
		for (std::list<DraggingView>::const_iterator it = _views.begin(); it != _views.end(); ++it) {
			_orig_tav_ripples.push_back((*it).view->region());
		}
	} else {
		orig_tav = NULL;
	}

}

void
RegionRippleDrag::motion (GdkEvent* event, bool first_move)
{
	/* Which trackview is this ? */

	pair<TimeAxisView*, double> const tvp = _editor->trackview_by_y_position (current_pointer_y ());
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*> (tvp.first);

	/* The region motion is only processed if the pointer is over
	   an audio track.
	 */

	if (!tv || !tv->is_track()) {
		/* To make sure we hide the verbose canvas cursor when the mouse is
		   not held over an audiotrack.
		 */
		_editor->verbose_cursor()->hide ();
		return;
	}

	timepos_t where = adjusted_current_time (event);
	assert (!where.is_negative());
	timepos_t after;
	double delta = compute_x_delta (event, after);

	timecnt_t amount = timecnt_t (pixel_to_time (delta), grab_time());

	if (allow_moves_across_tracks) {
		// all the originally selected regions were on the same track

		timecnt_t adjust;

		if (prev_tav && tv != prev_tav) {
			// dragged onto a different track
			// remove the unselected regions from _views, restore them to their original positions
			// and add the regions after the drop point on the new playlist to _views instead.
			// undo the effect of rippling the previous playlist, and include the effect of removing
			// the dragged region(s) from this track

			remove_unselected_from_views (prev_amount, false);
			// ripple previous playlist according to the regions that have been removed onto the new playlist
			prev_tav->playlist()->ripple(prev_position, -selection_length, exclude);
			prev_amount = timecnt_t (tv->route()->time_domain());

			// move just the selected regions
			RegionMoveDrag::motion(event, first_move);

			// ensure that the ripple operation on the new playlist inserts selection_length time
			adjust = selection_length;
			// ripple the new current playlist
			tv->playlist()->ripple (where, amount+adjust, exclude);

			// add regions after point where drag entered this track to subsequent ripples
			add_all_after_to_views (tv, where, _editor->get_selection().regions, true);

		} else {
			// motion on same track
			RegionMoveDrag::motion(event, first_move);
		}
		prev_tav = tv;

		// remember what we've done to this playlist so we can undo it if the selection is dragged to another track
		prev_position = where;
	} else {
		// selection encompasses multiple tracks - just drag
		// cross-track drags are forbidden
		RegionMoveDrag::motion(event, first_move);
	}

	if (!_x_constrained) {
		prev_amount += amount;
	}

	_last_position = after;
}

void
RegionRippleDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {

		/* just a click */

		if (was_double_click() && !_views.empty()) {
			DraggingView dv = _views.front();
			_editor->edit_region (dv.view);
		}

		return;
	}

	_editor->begin_reversible_command(_("Ripple drag"));

	// remove the regions being rippled from the dragging view, updating them to
	// their new positions

	if (allow_moves_across_tracks) {
		if (orig_tav) {
			// if regions were dragged across tracks, we've rippled any later
			// regions on the track the regions were dragged off, so we need
			// to add the original track to the undo record
			orig_tav->playlist()->clear_changes();
			orig_tav->playlist()->clear_owned_changes();
			orig_tav->playlist()->freeze ();

			remove_unselected_from_views (prev_amount, true);

			std::list<boost::shared_ptr<Region> >::const_iterator it = _orig_tav_ripples.begin();
			for (; it != _orig_tav_ripples.end(); ++it) {
				const boost::shared_ptr<Region> r = *it;
				bool found = false;
				for (std::list<DraggingView>::const_iterator it = _views.begin(); it != _views.end(); ++it) {
					if (it->view->region() == r) {
						found = true;
						break;
					}
				}
				if (!found) {
					const timepos_t pos_after = r->position();
					const timepos_t pos_before = pos_after + selection_length;
					r->set_position(pos_before);
					r->clear_changes();
					r->set_position(pos_after);
				}
			}

			orig_tav->playlist()->thaw ();

			vector<Command*> cmds;
			orig_tav->playlist()->rdiff (cmds);
			_editor->session()->add_commands (cmds);
		}
		if (prev_tav && prev_tav != orig_tav) {
			prev_tav->playlist()->clear_changes();
			vector<Command*> cmds;
			prev_tav->playlist()->rdiff (cmds);
			_editor->session()->add_commands (cmds);
		}
	} else {
		// selection spanned multiple tracks - all will need adding to undo record

		std::set<boost::shared_ptr<ARDOUR::Playlist> > playlists = _editor->selection->regions.playlists();
		std::set<boost::shared_ptr<ARDOUR::Playlist> >::const_iterator pi;

		for (pi = playlists.begin(); pi != playlists.end(); ++pi) {
			(*pi)->clear_changes();
			(*pi)->clear_owned_changes();
			(*pi)->freeze();
		}

		remove_unselected_from_views (prev_amount, true);

		for (pi = playlists.begin(); pi != playlists.end(); ++pi) {
			(*pi)->thaw();
			vector<Command*> cmds;
			(*pi)->rdiff (cmds);
			_editor->session()->add_commands (cmds);
		}
	}

	// other modified playlists are added to undo by RegionMoveDrag::finished()
	RegionMoveDrag::finished (event, movement_occurred);
	_editor->commit_reversible_command();
}

void
RegionRippleDrag::aborted (bool movement_occurred)
{
	RegionMoveDrag::aborted (movement_occurred);
	_views.clear ();
}


RegionCreateDrag::RegionCreateDrag (Editor* e, ArdourCanvas::Item* i, TimeAxisView* v)
	: Drag (e, i, e->default_time_domain()),
	  _view (dynamic_cast<MidiTimeAxisView*> (v))
{
	DEBUG_TRACE (DEBUG::Drags, "New RegionCreateDrag\n");

	assert (_view);
}

void
RegionCreateDrag::motion (GdkEvent* event, bool first_move)
{
	if (first_move) {
		_editor->begin_reversible_command (_("create region"));
		_region = add_midi_region (_view, false);
		_view->playlist()->freeze ();
	} else {

		if (_region) {
			timepos_t const pos (adjusted_current_time (event).beats());

			if (pos <= grab_time()) {
				_region->set_initial_position (pos);
			}

			if (pos != grab_time()) {
				/* Force MIDI regions to use Beats ... for now */
				timecnt_t const len (grab_time().distance (pos).abs().beats());
				_region->set_length (len);
			}
		}
	}
}

void
RegionCreateDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		add_midi_region (_view, true);
	} else {
		_view->playlist()->thaw ();
		_editor->commit_reversible_command();
	}
}

void
RegionCreateDrag::aborted (bool)
{
	if (_region) {
		_view->playlist()->thaw ();
	}

	/* XXX */
}

NoteResizeDrag::NoteResizeDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i, Temporal::BeatTime)
	, region (0)
	, relative (false)
	, at_front (true)
	, _was_selected (false)
	, _snap_delta (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New NoteResizeDrag\n");
}

void
NoteResizeDrag::start_grab (GdkEvent* event, Gdk::Cursor* /*ignored*/)
{
	Gdk::Cursor* cursor;
	NoteBase* cnote = reinterpret_cast<NoteBase*> (_item->get_data ("notebase"));
	assert (cnote);
	float x_fraction = cnote->mouse_x_fraction ();

	if (x_fraction > 0.0 && x_fraction < 0.25) {
		cursor = _editor->cursors()->left_side_trim;
		at_front = true;
	} else  {
		cursor = _editor->cursors()->right_side_trim;
		at_front = false;
	}

	Drag::start_grab (event, cursor);

	region = &cnote->region_view();

	double temp;
	temp = region->snap_to_pixel (cnote->x0 (), true);
	_snap_delta = temp - cnote->x0 ();

	_item->grab ();

	if (event->motion.state & ArdourKeyboard::note_size_relative_modifier ()) {
		relative = false;
	} else {
		relative = true;
	}
	MidiRegionSelection ms = _editor->get_selection().midi_regions();

	if (ms.size() > 1) {
		/* has to be relative, may make no sense otherwise */
		relative = true;
	}

	if (!(_was_selected = cnote->selected())) {

		/* tertiary-click means extend selection - we'll do that on button release,
		   so don't add it here, because otherwise we make it hard to figure
		   out the "extend-to" range.
		*/

		bool extend = Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier);

		if (!extend) {
			bool add = Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier);

			if (add) {
				region->note_selected (cnote, true);
			} else {
				_editor->get_selection().clear_points();
				region->unique_select (cnote);
			}
		}
	}
}

void
NoteResizeDrag::motion (GdkEvent* event, bool first_move)
{
	MidiRegionSelection ms  = _editor->get_selection().midi_regions();
	if (first_move) {
		_editor->begin_reversible_command (_("resize notes"));

		for (MidiRegionSelection::iterator r = ms.begin(); r != ms.end(); ) {
			MidiRegionSelection::iterator next;
			next = r;
			++next;
			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*r);
			if (mrv) {
				mrv->begin_resizing (at_front);
			}
			r = next;
		}
	}

	for (MidiRegionSelection::iterator r = ms.begin(); r != ms.end(); ++r) {
		NoteBase* nb = reinterpret_cast<NoteBase*> (_item->get_data ("notebase"));
		assert (nb);
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*r);
		if (mrv) {
			double sd = 0.0;
			bool snap = true;
			bool apply_snap_delta = ArdourKeyboard::indicates_snap_delta (event->button.state);

			if (ArdourKeyboard::indicates_snap (event->button.state)) {
				if (_editor->snap_mode () != SnapOff) {
					snap = false;
				}
			} else {
				if (_editor->snap_mode () == SnapOff) {
					snap = false;
					/* inverted logic here - we;re in snapoff but we've pressed the snap delta modifier */
					if (apply_snap_delta) {
						snap = true;
					}
				}
			}

			if (apply_snap_delta) {
				sd = _snap_delta;
			}

			mrv->update_resizing (nb, at_front, _drags->current_pointer_x() - grab_x(), relative, sd, snap);
		}
	}
}

void
NoteResizeDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		/* no motion - select note */
		NoteBase* cnote = reinterpret_cast<NoteBase*> (_item->get_data ("notebase"));
		if (_editor->current_mouse_mode() == Editing::MouseContent ||
		    _editor->current_mouse_mode() == Editing::MouseDraw) {

			bool changed = false;

			if (_was_selected) {
				bool add = Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier);
				if (add) {
					region->note_deselected (cnote);
					changed = true;
				} else {
					_editor->get_selection().clear_points();
					region->unique_select (cnote);
					changed = true;
				}
			} else {
				bool extend = Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier);
				bool add = Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier);

				if (!extend && !add && region->selection_size() > 1) {
					_editor->get_selection().clear_points();
					region->unique_select (cnote);
					changed = true;
				} else if (extend) {
					region->note_selected (cnote, true, true);
					changed = true;
				} else {
					/* it was added during button press */
					changed = true;
				}
			}

			if (changed) {
				_editor->begin_reversible_selection_op(X_("Resize Select Note Release"));
				_editor->commit_reversible_selection_op();
			}
		}

		return;
	}

	MidiRegionSelection ms = _editor->get_selection().midi_regions();
	for (MidiRegionSelection::iterator r = ms.begin(); r != ms.end(); ++r) {
		NoteBase* nb = reinterpret_cast<NoteBase*> (_item->get_data ("notebase"));
		assert (nb);
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*r);
		double sd = 0.0;
		bool snap = true;
		bool apply_snap_delta = ArdourKeyboard::indicates_snap_delta (event->button.state);
		if (mrv) {
			if (ArdourKeyboard::indicates_snap (event->button.state)) {
				if (_editor->snap_mode () != SnapOff) {
					snap = false;
				}
			} else {
				if (_editor->snap_mode () == SnapOff) {
					snap = false;
					/* inverted logic here - we;re in snapoff but we've pressed the snap delta modifier */
					if (apply_snap_delta) {
						snap = true;
					}
				}
			}

			if (apply_snap_delta) {
				sd = _snap_delta;
			}

			mrv->commit_resizing (nb, at_front, _drags->current_pointer_x() - grab_x(), relative, sd, snap);
		}
	}

	_editor->commit_reversible_command ();
}

void
NoteResizeDrag::aborted (bool)
{
	MidiRegionSelection ms = _editor->get_selection().midi_regions();
	for (MidiRegionSelection::iterator r = ms.begin(); r != ms.end(); ++r) {
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*r);
		if (mrv) {
			mrv->abort_resizing ();
		}
	}
}

AVDraggingView::AVDraggingView (RegionView* v)
	: view (v)
{
	initial_position = v->region()->position_sample();
}

VideoTimeLineDrag::VideoTimeLineDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i, e->default_time_domain())
{
	DEBUG_TRACE (DEBUG::Drags, "New VideoTimeLineDrag\n");

	RegionSelection rs;
	TrackViewList empty;
	empty.clear();
	_editor->get_regions_after(rs, timepos_t::zero (Temporal::AudioTime), empty);
	std::list<RegionView*> views = rs.by_layer();

	_stuck = false;
	for (list<RegionView*>::iterator i = views.begin(); i != views.end(); ++i) {
		RegionView* rv = (*i);
		if (!rv->region()->video_locked()) {
			continue;
		}
		if (rv->region()->locked()) {
			_stuck = true;
		}
		_views.push_back (AVDraggingView (rv));
	}
}

void
VideoTimeLineDrag::start_grab (GdkEvent* event, Gdk::Cursor*)
{
	Drag::start_grab (event);
	if (_editor->session() == 0) {
		return;
	}

	if (Keyboard::modifier_state_equals (event->button.state, Keyboard::ModifierMask (Keyboard::TertiaryModifier))) {
		_stuck = false;
		_views.clear();
	}

	if (_stuck) {
		show_verbose_cursor_text (_("One or more Audio Regions\nare both Locked and\nLocked to Video.\nThe video cannot be moved."));
		return;
	}

	_startdrag_video_offset=ARDOUR_UI::instance()->video_timeline->get_offset();
	_max_backwards_drag = (
			  ARDOUR_UI::instance()->video_timeline->get_duration()
			+ ARDOUR_UI::instance()->video_timeline->get_offset()
			- ceil(ARDOUR_UI::instance()->video_timeline->get_apv())
			);

	for (list<AVDraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		if (i->initial_position < _max_backwards_drag || _max_backwards_drag < 0) {
			_max_backwards_drag = ARDOUR_UI::instance()->video_timeline->quantify_samples_to_apv (i->initial_position);
		}
	}
	DEBUG_TRACE (DEBUG::Drags, string_compose("VideoTimeLineDrag: max backwards-drag: %1\n", _max_backwards_drag));

	char buf[128];
	Timecode::Time timecode;
	_editor->session()->sample_to_timecode(abs(_startdrag_video_offset), timecode, true /* use_offset */, false /* use_subframes */ );
	snprintf (buf, sizeof (buf), "Video Start:\n%c%02" PRId32 ":%02" PRId32 ":%02" PRId32 ":%02" PRId32, (_startdrag_video_offset<0?'-':' '), timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
	show_verbose_cursor_text (buf);
}

void
VideoTimeLineDrag::motion (GdkEvent* event, bool first_move)
{
	if (_editor->session() == 0) {
		return;
	}
	if (ARDOUR_UI::instance()->video_timeline->is_offset_locked()) {
		return;
	}
	if (_stuck) {
		show_verbose_cursor_text (_("One or more Audio Regions\nare both Locked and\nLocked to Video.\nThe video cannot be moved."));
		return;
	}

	samplecnt_t dt = adjusted_current_time (event).samples() - raw_grab_time().samples() + _pointer_offset.samples();
	dt = ARDOUR_UI::instance()->video_timeline->quantify_samples_to_apv(_startdrag_video_offset+dt) - _startdrag_video_offset;

	if (_max_backwards_drag >= 0 && dt <= - _max_backwards_drag) {
		dt = - _max_backwards_drag;
	}

	ARDOUR_UI::instance()->video_timeline->set_offset(_startdrag_video_offset+dt);
	ARDOUR_UI::instance()->flush_videotimeline_cache(true);

	for (list<AVDraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		RegionView* rv = i->view;
		DEBUG_TRACE (DEBUG::Drags, string_compose("SHIFT REGION at %1 by %2\n", i->initial_position, dt));
		if (first_move) {
			rv->drag_start ();
			rv->region()->clear_changes ();
			rv->region()->suspend_property_changes();
		}
		rv->region()->set_position(timepos_t (i->initial_position + dt));
		rv->region_changed (ARDOUR::Properties::length);
	}

	const samplepos_t offset = ARDOUR_UI::instance()->video_timeline->get_offset();
	Timecode::Time timecode;
	Timecode::Time timediff;
	char buf[128];
	_editor->session()->sample_to_timecode(abs(offset), timecode, true /* use_offset */, false /* use_subframes */ );
	_editor->session()->sample_to_timecode(abs(dt), timediff, false /* use_offset */, false /* use_subframes */ );
	snprintf (buf, sizeof (buf),
			"%s\n%c%02" PRId32 ":%02" PRId32 ":%02" PRId32 ":%02" PRId32
			"\n%s\n%c%02" PRId32 ":%02" PRId32 ":%02" PRId32 ":%02" PRId32
			, _("Video Start:"),
				(offset<0?'-':' '), timecode.hours, timecode.minutes, timecode.seconds, timecode.frames
			, _("Diff:"),
				(dt<0?'-':' '), timediff.hours, timediff.minutes, timediff.seconds, timediff.frames
				);
	show_verbose_cursor_text (buf);
}

void
VideoTimeLineDrag::finished (GdkEvent * /*event*/, bool movement_occurred)
{
	if (ARDOUR_UI::instance()->video_timeline->is_offset_locked()) {
		return;
	}
	if (_stuck) {
		return;
	}

	if (!movement_occurred || ! _editor->session()) {
		return;
	}

	ARDOUR_UI::instance()->flush_videotimeline_cache(true);

	_editor->begin_reversible_command (_("Move Video"));

	XMLNode &before = ARDOUR_UI::instance()->video_timeline->get_state();
	ARDOUR_UI::instance()->video_timeline->save_undo();
	XMLNode &after = ARDOUR_UI::instance()->video_timeline->get_state();
	_editor->session()->add_command(new MementoCommand<VideoTimeLine>(*(ARDOUR_UI::instance()->video_timeline), &before, &after));

	for (list<AVDraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		i->view->drag_end();
		i->view->region()->resume_property_changes ();

		_editor->session()->add_command (new StatefulDiffCommand (i->view->region()));
	}

	_editor->session()->maybe_update_session_range(
		timepos_t (std::max(ARDOUR_UI::instance()->video_timeline->get_offset(), (sampleoffset_t) 0)),
		timepos_t (std::max(ARDOUR_UI::instance()->video_timeline->get_offset() + ARDOUR_UI::instance()->video_timeline->get_duration(), (sampleoffset_t) 0))
		);


	_editor->commit_reversible_command ();
}

void
VideoTimeLineDrag::aborted (bool)
{
	if (ARDOUR_UI::instance()->video_timeline->is_offset_locked()) {
		return;
	}
	ARDOUR_UI::instance()->video_timeline->set_offset(_startdrag_video_offset);
	ARDOUR_UI::instance()->flush_videotimeline_cache(true);

	for (list<AVDraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		i->view->region()->resume_property_changes ();
		i->view->region()->set_position(timepos_t (i->initial_position));
	}
}

TrimDrag::TrimDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v, bool preserve_fade_anchor)
	: RegionDrag (e, i, p, v)
	, _operation (StartTrim)
	, _preserve_fade_anchor (preserve_fade_anchor)
	, _jump_position_when_done (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New TrimDrag\n");
}

void
TrimDrag::start_grab (GdkEvent* event, Gdk::Cursor*)
{
	timepos_t const region_start = _primary->region()->position();
	timepos_t const region_end = _primary->region()->end();
	timecnt_t const region_length = _primary->region()->length();

	timepos_t const pf = adjusted_current_time (event);
	setup_snap_delta (region_start);

	/* These will get overridden for a point trim.*/
	if (pf < (region_start + region_length/2)) {
		/* closer to front */
		_operation = StartTrim;
		if (Keyboard::modifier_state_equals (event->button.state, ArdourKeyboard::trim_anchored_modifier ())) {
			Drag::start_grab (event, _editor->cursors()->anchored_left_side_trim);
		} else {
			Drag::start_grab (event, _editor->cursors()->left_side_trim);
		}
	} else {
		/* closer to end */
		_operation = EndTrim;
		if (Keyboard::modifier_state_equals (event->button.state, ArdourKeyboard::trim_anchored_modifier ())) {
			Drag::start_grab (event, _editor->cursors()->anchored_right_side_trim);
		} else {
			Drag::start_grab (event, _editor->cursors()->right_side_trim);
		}
	}

	/* jump trim disabled for now
	if (Keyboard::modifier_state_equals (event->button.state, Keyboard::trim_jump_modifier ())) {
		_jump_position_when_done = true;
	}
	*/

	switch (_operation) {
	case StartTrim:
		show_verbose_cursor_time (region_start);
		break;
	case EndTrim:
		show_verbose_cursor_duration (region_start, region_end);
		break;
	}
	show_view_preview (_operation == StartTrim ? region_start : region_end);

	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		i->view->region()->suspend_property_changes ();
	}
}

void
TrimDrag::motion (GdkEvent* event, bool first_move)
{
	RegionView* rv = _primary;

	pair<set<boost::shared_ptr<Playlist> >::iterator,bool> insert_result;
	timecnt_t delta;
	timepos_t adj_time = adjusted_time (_drags->current_pointer_time () + snap_delta (event->button.state), event, true);
	timecnt_t dt = raw_grab_time().distance (adj_time) + _pointer_offset - snap_delta (event->button.state);

	if (first_move) {

		string trim_type;

		switch (_operation) {
		case StartTrim:
			trim_type = "Region start trim";
			break;
		case EndTrim:
			trim_type = "Region end trim";
			break;
		default:
			assert(0);
			break;
		}

		_editor->begin_reversible_command (trim_type);

		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			RegionView* rv = i->view;
			rv->region()->playlist()->clear_owned_changes ();

			if (_operation == StartTrim) {
				rv->trim_front_starting ();
			}

			AudioRegionView* const arv = dynamic_cast<AudioRegionView*> (rv);

			if (arv) {
				arv->temporarily_hide_envelope ();
				arv->drag_start ();
			}

			boost::shared_ptr<Playlist> pl = rv->region()->playlist();
			insert_result = _editor->motion_frozen_playlists.insert (pl);

			if (insert_result.second) {
				pl->freeze();
			}

			MidiRegionView* const mrv = dynamic_cast<MidiRegionView*> (rv);
			/* a MRV start trim may change the source length. ensure we cover all playlists here */
			if (mrv && _operation == StartTrim) {
				vector<boost::shared_ptr<Playlist> > all_playlists;
				_editor->session()->playlists()->get (all_playlists);
				for (vector<boost::shared_ptr<Playlist> >::iterator x = all_playlists.begin(); x != all_playlists.end(); ++x) {

					if ((*x)->uses_source (rv->region()->source(0))) {
						insert_result = _editor->motion_frozen_playlists.insert (*x);
						if (insert_result.second) {
							(*x)->clear_owned_changes ();
							(*x)->freeze();
						}

					}
				}
			}
		}
	}

	bool non_overlap_trim = false;

	if (event && Keyboard::modifier_state_contains (event->button.state, ArdourKeyboard::trim_overlap_modifier ())) {
		non_overlap_trim = true;
	}

	/* contstrain trim to fade length */
	if (_preserve_fade_anchor) {

		/* fades are audio and always use AudioTime domain */

		samplecnt_t dts = dt.samples();

		switch (_operation) {
			case StartTrim:
				for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
					AudioRegionView* arv = dynamic_cast<AudioRegionView*> (i->view);
					if (!arv) continue;
					boost::shared_ptr<AudioRegion> ar (arv->audio_region());
					if (ar->locked()) continue;
					samplecnt_t len = ar->fade_in()->back()->when.samples();
					if (len < dts) dts = min(dts, len);
				}
				break;
			case EndTrim:
				for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
					AudioRegionView* arv = dynamic_cast<AudioRegionView*> (i->view);
					if (!arv) continue;
					boost::shared_ptr<AudioRegion> ar (arv->audio_region());
					if (ar->locked()) continue;
					samplecnt_t len = ar->fade_out()->back()->when.samples();
					if (len < -dts) dts = max(dts, -len);
				}
				break;
		}
	}

	switch (_operation) {
	case StartTrim:
		for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
			bool changed = i->view->trim_front (timepos_t (i->initial_position) + dt, non_overlap_trim);


			if (changed && _preserve_fade_anchor) {
				AudioRegionView* arv = dynamic_cast<AudioRegionView*> (i->view);
				if (arv) {
					boost::shared_ptr<AudioRegion> ar (arv->audio_region());
					samplecnt_t len = ar->fade_in()->back()->when.samples();
					samplecnt_t diff = ar->first_sample() - i->initial_position.samples();
					samplepos_t new_length = len - diff;
					i->anchored_fade_length = min (ar->length_samples(), new_length);
					//i->anchored_fade_length = ar->verify_xfade_bounds (new_length, true  /*START*/ );
					arv->reset_fade_in_shape_width (ar, i->anchored_fade_length, true);
				}
			}
		}
		break;

	case EndTrim:
		for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
			bool changed = i->view->trim_end (timepos_t (i->initial_end) + dt, non_overlap_trim);

			if (changed && _preserve_fade_anchor) {
				AudioRegionView* arv = dynamic_cast<AudioRegionView*> (i->view);
				if (arv) {
					boost::shared_ptr<AudioRegion> ar (arv->audio_region());
					samplecnt_t len = ar->fade_out()->back()->when.samples();
					samplecnt_t diff = 1 + ar->last_sample() - i->initial_end.samples();
					samplepos_t new_length = len + diff;
					i->anchored_fade_length = min (ar->length_samples(), new_length);
					//i->anchored_fade_length = ar->verify_xfade_bounds (new_length, false  /*END*/ );
					arv->reset_fade_out_shape_width (ar, i->anchored_fade_length, true);
				}
			}
		}
		break;

	}

	switch (_operation) {
	case StartTrim:
		show_verbose_cursor_time (rv->region()->position());
		break;
	case EndTrim:
		show_verbose_cursor_duration (rv->region()->position(), rv->region()->end());
		break;
	}
	show_view_preview ((_operation == StartTrim ? rv->region()->position() : rv->region()->end()));
}

void
TrimDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {
		motion (event, false);

		if (_operation == StartTrim) {
			for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
				{
					/* This must happen before the region's StatefulDiffCommand is created, as it may
					   `correct' (ahem) the region's _start from being negative to being zero.  It
					   needs to be zero in the undo record.
					*/
					i->view->trim_front_ending ();
				}
				if (_preserve_fade_anchor && i->anchored_fade_length) {
					AudioRegionView* arv = dynamic_cast<AudioRegionView*> (i->view);
					if (arv) {
						boost::shared_ptr<AudioRegion> ar (arv->audio_region());
						arv->reset_fade_in_shape_width (ar, i->anchored_fade_length);
						ar->set_fade_in_length(i->anchored_fade_length);
						ar->set_fade_in_active(true);
					}
				}
				if (_jump_position_when_done) {
					i->view->region()->set_position (timepos_t (i->initial_position));
				}
			}
		} else if (_operation == EndTrim) {
			for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
				if (_preserve_fade_anchor && i->anchored_fade_length) {
					AudioRegionView* arv = dynamic_cast<AudioRegionView*> (i->view);
					if (arv) {
						boost::shared_ptr<AudioRegion> ar (arv->audio_region());
						arv->reset_fade_out_shape_width (ar, i->anchored_fade_length);
						ar->set_fade_out_length(i->anchored_fade_length);
						ar->set_fade_out_active(true);
					}
				}
				if (_jump_position_when_done) {
					i->view->region()->set_position (timepos_t (i->initial_end).earlier (i->view->region()->length()));
				}
			}
		}

		if (!_editor->selection->selected (_primary)) {
			_primary->thaw_after_trim ();
		} else {
			for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
				i->view->thaw_after_trim ();
				i->view->enable_display (true);
			}
		}

		for (set<boost::shared_ptr<Playlist> >::iterator p = _editor->motion_frozen_playlists.begin(); p != _editor->motion_frozen_playlists.end(); ++p) {
			/* Trimming one region may affect others on the playlist, so we need
			   to get undo Commands from the whole playlist rather than just the
			   region.  Use motion_frozen_playlists (a set) to make sure we don't
			   diff a given playlist more than once.
			*/

			vector<Command*> cmds;
			(*p)->rdiff (cmds);
			_editor->session()->add_commands (cmds);
			(*p)->thaw ();
		}

		_editor->motion_frozen_playlists.clear ();
		_editor->commit_reversible_command();

	} else {
		/* no mouse movement */
		if (adjusted_current_time (event) != adjusted_time (_drags->current_pointer_time(), event, false)) {
			_editor->point_trim (event, adjusted_current_time (event));
		}
	}

	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		i->view->region()->resume_property_changes ();
	}
}

void
TrimDrag::aborted (bool movement_occurred)
{
	/* Our motion method is changing model state, so use the Undo system
	   to cancel.  Perhaps not ideal, as this will leave an Undo point
	   behind which may be slightly odd from the user's point of view.
	*/

	GdkEvent ev;
	memset (&ev, 0, sizeof (GdkEvent));
	finished (&ev, true);

	if (movement_occurred) {
		_editor->session()->undo (1);
	}

	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		i->view->region()->resume_property_changes ();
	}
}

void
TrimDrag::setup_pointer_offset ()
{
	list<DraggingView>::iterator i = _views.begin ();
	while (i != _views.end() && i->view != _primary) {
		++i;
	}

	if (i == _views.end()) {
		return;
	}


	switch (_operation) {
	case StartTrim:
		_pointer_offset = i->initial_position.distance (raw_grab_time());
		break;
	case EndTrim:
		_pointer_offset = i->initial_end.distance (raw_grab_time());
		break;
	}
}

MeterMarkerDrag::MeterMarkerDrag (Editor* e, ArdourCanvas::Item* i, bool c)
	: Drag (e, i, Temporal::BeatTime)
	, _marker (reinterpret_cast<MeterMarker*> (_item->get_data ("marker")))
	, _copy (c)
	, _old_grid_type (e->grid_type())
	, _old_snap_mode (e->snap_mode())
	, before_state (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New MeterMarkerDrag\n");
	assert (_marker);
}

void
MeterMarkerDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);
	show_verbose_cursor_time (adjusted_current_time (event));

	/* setup thread-local tempo map ptr as a writable copy */

	TempoMap::fetch_writable ();
}

void
MeterMarkerDrag::setup_pointer_offset ()
{
	_pointer_offset =  _marker->meter().time().distance (raw_grab_time());
}

void
MeterMarkerDrag::motion (GdkEvent* event, bool first_move)
{
	TempoMap::SharedPtr map (TempoMap::use());

	if (first_move) {
		// create a dummy marker to catch events, then hide it.

		char name[64];
		snprintf (name, sizeof(name), "%d/%d", _marker->meter().divisions_per_bar(), _marker->meter().note_value ());

		_marker = new MeterMarker (
			*_editor,
			*_editor->meter_group,
			UIConfiguration::instance().color ("meter marker"),
			name,
			_marker->meter()
			);

		/* use the new marker for the grab */
		swap_grab (&_marker->the_item(), 0, GDK_CURRENT_TIME);
		_marker->hide();

		/* get current state */
		before_state = &map->get_state();
		if (!_copy) {
			_editor->begin_reversible_command (_("move meter mark"));
		} else {

			timepos_t const pointer = adjusted_current_time (event, false);
			BBT_Time pointer_bbt = map->bbt_at (pointer);
			Temporal::MeterPoint const & meter = map->metric_at (pointer).meter();
			BBT_Time bbt = meter.bbt();

			/* we can't add a meter where one currently exists */
			if (bbt < pointer_bbt) {
				++bbt.bars;
			} else {
				--bbt.bars;
			}

			_editor->begin_reversible_command (_("copy meter mark"));

			timepos_t pos;

			if (map->time_domain() == AudioTime) {
				pos = timepos_t (map->sample_at (bbt));
			} else {
				pos = timepos_t (map->quarters_at (bbt));
			}

			_marker->reset_meter (map->set_meter (meter, pos));
		}

		/* only snap to bars. leave snap mode alone for audio locked meters.*/
		if (map->time_domain() != AudioTime) {
			_editor->set_grid_to (GridTypeBar);
			_editor->set_snap_mode (SnapMagnetic);
		}
	}

	if (_movable && (!first_move || !_copy)) {

		timepos_t pos;

		if (_editor->grid_musical()) {
			/* not useful to try to snap to a grid we're about to change */
			pos = adjusted_current_time (event, false);
		} else {
			pos = adjusted_current_time (event);
		}

		if (map->move_meter (_marker->meter(), pos, false)) {
			/* it was moved */
			_editor->tempo_map_changed ();
		}
		show_verbose_cursor_time (timepos_t (_marker->meter().beats()));
		_editor->set_snapped_cursor_position (timepos_t (_marker->meter().sample(_editor->session()->sample_rate())));
	}
}

void
MeterMarkerDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		/* reset thread local tempo map to the original state */
		TempoMap::abort_update ();
		if (was_double_click()) {
			_editor->edit_meter_marker (*_marker);
		}
		return;
	}

	/* reinstate old snap setting */
	_editor->set_grid_to (_old_grid_type);
	_editor->set_snap_mode (_old_snap_mode);

	TempoMap::SharedPtr map (TempoMap::use());

	XMLNode &after = TempoMap::use()->get_state();
	_editor->session()->add_command (new MementoCommand<Temporal::TempoMap> (new Temporal::TempoMap::MementoBinder(), before_state, &after));
	_editor->commit_reversible_command ();

	TempoMap::update (map);

	// delete the dummy marker we used for visual representation while moving.
	// a new visual marker will show up automatically.
	delete _marker;
}

void
MeterMarkerDrag::aborted (bool moved)
{
	/* reset thread local tempo map to the original state */
	TempoMap::abort_update ();

	_marker->set_position (_marker->meter().time());

	if (moved) {
		/* reinstate old snap setting */
		_editor->set_grid_to (_old_grid_type);
		_editor->set_snap_mode (_old_snap_mode);

		// delete the dummy marker we used for visual representation while moving.
		// a new visual marker will show up automatically.
		delete _marker;
	}

}

TempoMarkerDrag::TempoMarkerDrag (Editor* e, ArdourCanvas::Item* i, bool c)
	: Drag (e, i, Temporal::BeatTime)
	, _copy (c)
	, _grab_bpm (120.0, 4.0)
	, _before_state (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New TempoMarkerDrag\n");

	_marker = reinterpret_cast<TempoMarker*> (_item->get_data ("marker"));
	_real_section = &_marker->tempo();
	_movable = !TempoMap::use()->is_initial (_marker->tempo());
	_grab_bpm = Tempo (_real_section->note_types_per_minute(), _real_section->note_type(), _real_section->end_note_types_per_minute());
	_grab_qn = _real_section->beats();
	assert (_marker);
}

void
TempoMarkerDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);
	if (!_real_section->active()) {
		show_verbose_cursor_text (_("inactive"));
	} else {
		show_verbose_cursor_time (adjusted_current_time (event));

		/* setup thread-local tempo map ptr as a writable copy */

		_editor->begin_tempo_map_edit ();
	}
}

void
TempoMarkerDrag::setup_pointer_offset ()
{
	_pointer_offset =  _marker->tempo().time().distance (raw_grab_time());
}

void
TempoMarkerDrag::motion (GdkEvent* event, bool first_move)
{
	if (!_marker->tempo().active()) {
		return;
	}

	TempoMap::SharedPtr map (TempoMap::use());

	if (first_move) {
		/* get current state */
		_before_state = &map->get_state();
		_editor->begin_reversible_command (_("move tempo mark"));
	}

	if (ArdourKeyboard::indicates_constraint (event->button.state) && ArdourKeyboard::indicates_copy (event->button.state)) {
		double new_bpm = max (1.5, _grab_bpm.end_note_types_per_minute() + ((grab_y() - min (-1.0, current_pointer_y())) / 5.0));
		stringstream strs;
		map->change_tempo (const_cast<TempoPoint&>(_marker->tempo()), Tempo (_marker->tempo().note_types_per_minute(), _marker->tempo().note_type(), new_bpm));
		strs << "end:" << fixed << setprecision(3) << new_bpm;
		show_verbose_cursor_text (strs.str());

	} else if (ArdourKeyboard::indicates_constraint (event->button.state)) {
		/* use vertical movement to alter tempo .. should be log */
		double new_bpm = max (1.5, _grab_bpm.note_types_per_minute() + ((grab_y() - min (-1.0, current_pointer_y())) / 5.0));
		stringstream strs;
		map->change_tempo (const_cast<TempoPoint&>(_marker->tempo()), Tempo (new_bpm, _marker->tempo().note_type(), _marker->tempo().end_note_types_per_minute()));
		strs << "start:" << fixed << setprecision(3) << new_bpm;
		show_verbose_cursor_text (strs.str());

	} else if (_movable) {
		timepos_t pos = adjusted_current_time (event);
		map->move_tempo (_marker->tempo(), pos, false);
		show_verbose_cursor_time (_marker->tempo().time());
		_editor->mid_tempo_change ();
	}
}


void
TempoMarkerDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!_marker->tempo().active()) {
		return;
	}

	if (!movement_occurred) {
		/* reset the per-thread tempo map ptr back to the current
		 * official version
		 */

		_editor->abort_tempo_map_edit ();

		if (was_double_click()) {
			_editor->edit_tempo_marker (*_marker);
		}

		return;
	}

	/* push the current state of our writable map copy */

	_editor->commit_tempo_map_edit ();
	XMLNode &after = TempoMap::use()->get_state();

	_editor->session()->add_command (new MementoCommand<Temporal::TempoMap> (new Temporal::TempoMap::MementoBinder(), _before_state, &after));
	_editor->commit_reversible_command ();
}

void
TempoMarkerDrag::aborted (bool moved)
{
	/* reset the per-thread tempo map ptr back to the current
	 * official version
	 */

	_editor->abort_tempo_map_edit ();

	// _point->end_float ();
	_marker->set_position (timepos_t (_marker->tempo().beats()));

	if (moved) {
		// delete the dummy (hidden) marker we used for events while moving.
		delete _marker;
	}
}

BBTRulerDrag::BBTRulerDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i, Temporal::BeatTime)
	, _tempo (0)
	, _before_state (0)
	, _drag_valid (true)
{
	DEBUG_TRACE (DEBUG::Drags, "New BBTRulerDrag\n");

}

void
BBTRulerDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	TempoMap::fetch_writable ();

	Drag::start_grab (event, cursor);

	TempoMap::SharedPtr map (TempoMap::use());
	_tempo = const_cast<TempoPoint*> (&map->metric_at (raw_grab_time().beats()).tempo());

	if (adjusted_current_time (event, false) <= _tempo->time()) {
		_drag_valid = false;
		return;
	}

	_editor->tempo_curve_selected (_tempo, true);

	ostringstream sstr;
	if (_tempo->clamped()) {
		TempoPoint const * prev = map->previous_tempo (*_tempo);
		if (prev) {
			sstr << "end: " << fixed << setprecision(3) << prev->end_note_types_per_minute() << "\n";
		}
	}

	sstr << "start: " << fixed << setprecision(3) << _tempo->note_types_per_minute();
	show_verbose_cursor_text (sstr.str());
}

void
BBTRulerDrag::setup_pointer_offset ()
{
	TempoMap::SharedPtr map (TempoMap::use());
	/* get current state */
	_before_state = &map->get_state();

	_grab_qn = max (Beats(), raw_grab_time().beats());

	uint32_t divisions = _editor->get_grid_beat_divisions (_editor->grid_type());

	if (divisions == 0) {
		divisions = 4;
	}

	_grab_qn = _grab_qn.round_to_subdivision (divisions, Temporal::RoundDownAlways);
	_pointer_offset = timepos_t (_grab_qn).distance (raw_grab_time());
}

void
BBTRulerDrag::motion (GdkEvent* event, bool first_move)
{
	if (!_drag_valid) {
		return;
	}

	if (first_move) {
		_editor->begin_reversible_command (_("stretch tempo"));
	}

	TempoMap::SharedPtr map (TempoMap::use());
	timepos_t pf;

	if (_editor->grid_musical()) {
		pf = adjusted_current_time (event, false);
	} else {
		pf = adjusted_current_time (event);
	}

	if (ArdourKeyboard::indicates_constraint (event->button.state)) {
		/* adjust previous tempo to match pointer sample */
#warning NUTEMPO need to implement this
		// _editor->session()->tempo_map().gui_stretch_tempo (_tempo, map.sample_at (_grab_qn), pf, _grab_qn, map.quarters_at_sample (pf));
	}

	ostringstream sstr;
	if (_tempo->clamped()) {
		TempoPoint const * prev = map->previous_tempo (*_tempo);
		if (prev) {
			_editor->tempo_curve_selected (prev, true);
			sstr << "end: " << fixed << setprecision(3) << prev->end_note_types_per_minute() << "\n";
		}
	}
	sstr << "start: " << fixed << setprecision(3) << _tempo->note_types_per_minute();
	show_verbose_cursor_text (sstr.str());
}

void
BBTRulerDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!_drag_valid) {
		TempoMap::abort_update ();
		return;
	}

	TempoMap::SharedPtr map (TempoMap::use());

	if (!movement_occurred) {

		_editor->begin_reversible_command (_("add BBT marker"));
		/* position markers must always be positioned using audio time */

		BBTMarkerDialog* marker_dialog = new BBTMarkerDialog (timepos_t (grab_sample()));

		/* run this modally since we are finishing a drag and the drag object
		 * will be destroyed when we return from here
		 */

		int result = marker_dialog->run ();
		BBT_Time bbt;

		switch (result) {
		case RESPONSE_ACCEPT:
		case RESPONSE_OK:
			bbt = marker_dialog->bbt_value ();
			map->set_bartime (bbt, marker_dialog->position());
			delete marker_dialog;
			break;
		default:
			delete marker_dialog;
			TempoMap::abort_update ();
			return;
		}

		/* the map change is committed below */

	} else {

		_editor->tempo_curve_selected (_tempo, false);

		if (_tempo->clamped()) {

			TempoPoint const * prev_tempo = map->previous_tempo (*_tempo);

			if (prev_tempo) {
				_editor->tempo_curve_selected (prev_tempo, false);
			}
		}
	}

	TempoMap::update (map);

	XMLNode &after = TempoMap::use()->get_state();

	_editor->session()->add_command(new MementoCommand<TempoMap>(new Temporal::TempoMap::MementoBinder(), _before_state, &after));
	_editor->commit_reversible_command ();
}

void
BBTRulerDrag::aborted (bool moved)
{
	TempoMap::abort_update ();
}

#warning NUTEMPO fixme no tempo twist drag for now
#if 0

TempoTwistDrag::TempoTwistDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i, Temporal::BeatTime)
	, _grab_qn (0.0)
	, _grab_tempo (0.0)
	, _tempo (0)
	, _next_tempo (0)
	, _drag_valid (true)
	, _before_state (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New TempoTwistDrag\n");

}

void
TempoTwistDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);
	TempoMap& map (_editor->session()->tempo_map());
	/* get current state */
	_before_state = &map.get_state();
	_tempo = const_cast<TempoSection*> (&map.tempo_section_at_sample (raw_grab_sample()));

	if (_tempo->locked_to_meter()) {
		_drag_valid = false;
		return;
	}

	_next_tempo = map.next_tempo_section (_tempo);
	if (_next_tempo) {
		if (!map.next_tempo_section (_next_tempo)) {
			_drag_valid = false;
			finished (event, false);

			return;
		}
		_editor->tempo_curve_selected (_tempo, true);
		_editor->tempo_curve_selected (_next_tempo, true);

		ostringstream sstr;
		sstr << "start: " << fixed << setprecision(3) << _tempo->note_types_per_minute() << "\n";
		sstr << "end: " << fixed << setprecision(3) << _tempo->end_note_types_per_minute() << "\n";
		sstr << "start: " << fixed << setprecision(3) << _next_tempo->note_types_per_minute();
		show_verbose_cursor_text (sstr.str());
	} else {
		_drag_valid = false;
	}

	_grab_tempo = Tempo (_tempo->note_types_per_minute(), _tempo->note_type());
}

void
TempoTwistDrag::setup_pointer_offset ()
{
	TempoMap& map (_editor->session()->tempo_map());
	const double beat_at_sample = max (0.0, map.beat_at_sample (raw_grab_sample()));
	const uint32_t divisions = _editor->get_grid_beat_divisions (0);
	double beat = 0.0;

	if (divisions > 0) {
		beat = floor (beat_at_sample) + (floor (((beat_at_sample - floor (beat_at_sample)) * divisions)) / divisions);
	} else {
		/* while it makes some sense for the user to determine the division to 'grab',
		   grabbing a bar often leads to confusing results wrt the actual tempo section being altered
		   and the result over steep tempo curves. Use sixteenths.
		*/
		beat = floor (beat_at_sample) + (floor (((beat_at_sample - floor (beat_at_sample)) * 4)) / 4);
	}

	_grab_qn = map.quarters_at_beat (beat);

	_pointer_offset = raw_grab_sample() - map.sample_at_quarter_note (_grab_qn);

}

void
TempoTwistDrag::motion (GdkEvent* event, bool first_move)
{

	if (!_next_tempo || !_drag_valid) {
		return;
	}

	TempoMap& map (_editor->session()->tempo_map());

	if (first_move) {
		_editor->begin_reversible_command (_("twist tempo"));
	}

	samplepos_t pf;

	if (_editor->grid_musical()) {
		pf = adjusted_current_sample (event, false);
	} else {
		pf = adjusted_current_sample (event);
	}

	/* adjust this and the next tempi to match pointer sample */
	double new_bpm = max (1.5, _grab_tempo.note_types_per_minute() + ((grab_y() - min (-1.0, current_pointer_y())) / 5.0));
	_editor->session()->tempo_map().gui_twist_tempi (_tempo, new_bpm, map.sample_at_quarter_note (_grab_qn), pf);

	ostringstream sstr;
	sstr << "start: " << fixed << setprecision(3) << _tempo->note_types_per_minute() << "\n";
	sstr << "end: " << fixed << setprecision(3) << _tempo->end_note_types_per_minute() << "\n";
	sstr << "start: " << fixed << setprecision(3) << _next_tempo->note_types_per_minute();
	show_verbose_cursor_text (sstr.str());
}

void
TempoTwistDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred || !_drag_valid) {
		return;
	}

	_editor->tempo_curve_selected (_tempo, false);
	_editor->tempo_curve_selected (_next_tempo, false);

	TempoMap& map (_editor->session()->tempo_map());
	XMLNode &after = map.get_state();
	_editor->session()->add_command(new MementoCommand<TempoMap>(map, _before_state, &after));
	_editor->commit_reversible_command ();
}

void
TempoTwistDrag::aborted (bool moved)
{
	if (moved) {
		_editor->session()->tempo_map().set_state (*_before_state, Stateful::current_state_version);
	}
}
#endif

TempoEndDrag::TempoEndDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i, Temporal::BeatTime)
	, _tempo (0)
	, _before_state (0)
	, _drag_valid (true)
{
	DEBUG_TRACE (DEBUG::Drags, "New TempoEndDrag\n");
	TempoMarker* marker = reinterpret_cast<TempoMarker*> (_item->get_data ("marker"));
	_tempo = &marker->tempo();
	_grab_qn = _tempo->beats();
}

void
TempoEndDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	TempoMap::fetch_writable();

	TempoMap::SharedPtr tmap (TempoMap::use());

	/* get current state */
	_before_state = &tmap->get_state();
	if (_tempo->locked_to_meter()) {
		_drag_valid = false;
		return;
	}

	ostringstream sstr;

	TempoPoint const * prev = 0;
	if ((prev = tmap->previous_tempo (*_tempo)) != 0) {
		_editor->tempo_curve_selected (prev, true);
		const samplecnt_t sr = AudioEngine::instance()->sample_rate();
		sstr << "end: " << fixed << setprecision(3) << tmap->tempo_at (samples_to_superclock (_tempo->sample (sr) - 1, sr)).end_note_types_per_minute() << "\n";
	}

	if (_tempo->clamped()) {
		_editor->tempo_curve_selected (_tempo, true);
		sstr << "start: " << fixed << setprecision(3) << _tempo->note_types_per_minute();
	}

	show_verbose_cursor_text (sstr.str());
}

void
TempoEndDrag::setup_pointer_offset ()
{
	_pointer_offset = timepos_t (_grab_qn).distance (raw_grab_time());
}

void
TempoEndDrag::motion (GdkEvent* event, bool first_move)
{
	if (!_drag_valid) {
		return;
	}

	TempoMap::SharedPtr map (TempoMap::use());

	if (first_move) {
		_editor->begin_reversible_command (_("stretch end tempo"));
	}

	timepos_t const pos = adjusted_current_time (event, false);
#warning NUTEMPO implement this
	// map->gui_stretch_tempo_end (&map->tempo_section_at_sample (_tempo->sample() - 1), map.sample_at_quarter_note (_grab_qn), pf);

	ostringstream sstr;
	const samplecnt_t sr = AudioEngine::instance()->sample_rate();
	sstr << "end: " << fixed << setprecision(3) << map->tempo_at (samples_to_superclock (_tempo->sample (sr) - 1, sr)).end_note_types_per_minute() << "\n";

	if (_tempo->clamped()) {
		sstr << "start: " << fixed << setprecision(3) << _tempo->note_types_per_minute();
	}

	show_verbose_cursor_text (sstr.str());
}

void
TempoEndDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred || !_drag_valid) {
		TempoMap::abort_update ();
		return;
	}

	TempoMap::SharedPtr tmap (TempoMap::use());

	TempoMap::update (tmap);

	tmap = TempoMap::use ();

	XMLNode &after = tmap->get_state();
	_editor->session()->add_command(new MementoCommand<TempoMap>(new Temporal::TempoMap::MementoBinder(), _before_state, &after));
	_editor->commit_reversible_command ();

	TempoPoint const * prev = 0;

	if ((prev = tmap->previous_tempo (*_tempo)) != 0) {
		_editor->tempo_curve_selected (prev, false);
	}

	if (_tempo->clamped()) {
		_editor->tempo_curve_selected (_tempo, false);

	}
}

void
TempoEndDrag::aborted (bool moved)
{
	TempoMap::abort_update ();
}

CursorDrag::CursorDrag (Editor* e, EditorCursor& c, bool s)
	: Drag (e, &c.track_canvas_item(), e->default_time_domain(), false)
	, _cursor (c)
	, _stop (s)
	, _grab_zoom (0.0)
{
	DEBUG_TRACE (DEBUG::Drags, "New CursorDrag\n");
}

/** Do all the things we do when dragging the playhead to make it look as though
 *  we have located, without actually doing the locate (because that would cause
 *  the diskstream buffers to be refilled, which is too slow).
 */
void
CursorDrag::fake_locate (samplepos_t t)
{
	if (_editor->session () == 0) {
		return;
	}

	_editor->playhead_cursor ()->set_position (t);

	Session* s = _editor->session ();
	if (s->timecode_transmission_suspended ()) {
		samplepos_t const f = _editor->playhead_cursor ()->current_sample ();
		/* This is asynchronous so it will be sent "now"
		 */
		s->send_mmc_locate (f);
		/* These are synchronous and will be sent during the next
		   process cycle
		*/
		s->queue_full_time_code ();
		s->queue_song_position_pointer ();
	}

	show_verbose_cursor_time (timepos_t (t));
	_editor->UpdateAllTransportClocks (t);
}

void
CursorDrag::start_grab (GdkEvent* event, Gdk::Cursor* c)
{
	Drag::start_grab (event, c);

	setup_snap_delta (timepos_t (_editor->playhead_cursor()->current_sample()));

	_grab_zoom = _editor->samples_per_pixel;

	timepos_t where (timepos_t (_editor->canvas_event_sample (event)) + snap_delta (event->button.state));

	_editor->snap_to_with_modifier (where, event);

	_editor->_dragging_playhead = true;
	_editor->_control_scroll_target = where.samples();

	Session* s = _editor->session ();

	/* grab the track canvas item as well */

	_cursor.track_canvas_item().grab();

	if (s) {
		if (_was_rolling && _stop) {
			s->request_stop ();
		}

		if (s->is_auditioning()) {
			s->cancel_audition ();
		}


		if (AudioEngine::instance()->running()) {

			/* do this only if we're the engine is connected
			 * because otherwise this request will never be
			 * serviced and we'll busy wait forever. likewise,
			 * notice if we are disconnected while waiting for the
			 * request to be serviced.
			 */

			s->request_suspend_timecode_transmission ();
			while (AudioEngine::instance()->running() && !s->timecode_transmission_suspended ()) {
				/* twiddle our thumbs */
			}
		}
	}

	fake_locate (where.earlier (snap_delta (event->button.state)).samples());

	_last_mx = event->button.x;
	_last_my = event->button.y;
	_last_dx = 0;
	_last_y_delta = 0;
}

void
CursorDrag::motion (GdkEvent* event, bool)
{
	timepos_t where (timepos_t (_editor->canvas_event_sample (event)) + snap_delta (event->button.state));

	_editor->snap_to_with_modifier (where, event);

	if (where != last_pointer_time()) {
		fake_locate (where.earlier (snap_delta (event->button.state)).samples());;
	}

	//maybe do zooming, too, if the option is enabled
	if (UIConfiguration::instance ().get_use_time_rulers_to_zoom_with_vertical_drag () ) {

		//To avoid accidental zooming, the mouse must move exactly vertical, not diagonal, to trigger a zoom step
		//we use screen coordinates for this, not canvas-based grab_x
		double mx = event->button.x;
		double dx = fabs(mx - _last_mx);
		double my = event->button.y;
		double dy = fabs(my - _last_my);

		{
			//do zooming in windowed "steps" so it feels more reversible ?
			const int stepsize = 2;  //stepsize ==1  means "trigger on every pixel of movement"
			int y_delta = grab_y() - current_pointer_y();
			y_delta = y_delta / stepsize;

			//if all requirements are met, do the actual zoom
			const double scale = 1.2;
			if ( (dy>dx) && (_last_dx ==0) && (y_delta != _last_y_delta) ) {
				if ( _last_y_delta > y_delta ) {
					_editor->temporal_zoom_step_mouse_focus_scale (true, scale);
				} else {
					_editor->temporal_zoom_step_mouse_focus_scale (false, scale);
				}
				_last_y_delta = y_delta;
			}
		}

		_last_my = my;
		_last_mx = mx;
		_last_dx = dx;
	}
}

void
CursorDrag::finished (GdkEvent* event, bool movement_occurred)
{
	_editor->_dragging_playhead = false;

	_cursor.track_canvas_item().ungrab();

	if (!movement_occurred && _stop) {
		return;
	}

	motion (event, false);

	Session* s = _editor->session ();
	if (s) {
		_editor->_pending_locate_request = true;
		s->request_locate (_editor->playhead_cursor ()->current_sample (), _was_rolling ? MustRoll : RollIfAppropriate);
		s->request_resume_timecode_transmission ();
	}
}

void
CursorDrag::aborted (bool)
{
	_cursor.track_canvas_item().ungrab();

	if (_editor->_dragging_playhead) {
		_editor->session()->request_resume_timecode_transmission ();
		_editor->_dragging_playhead = false;
	}

	_editor->playhead_cursor()->set_position (adjusted_time (grab_time (), 0, false).samples());
}

FadeInDrag::FadeInDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
{
	DEBUG_TRACE (DEBUG::Drags, "New FadeInDrag\n");
}

void
FadeInDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	AudioRegionView* arv = dynamic_cast<AudioRegionView*> (_primary);
	boost::shared_ptr<AudioRegion> const r = arv->audio_region ();
	setup_snap_delta (r->position());

	show_verbose_cursor_duration (r->position(), r->position() + r->fade_in()->back()->when, 32);
	show_view_preview (r->position() + r->fade_in()->back()->when);
}

void
FadeInDrag::setup_pointer_offset ()
{
	AudioRegionView* arv = dynamic_cast<AudioRegionView*> (_primary);
	boost::shared_ptr<AudioRegion> const r = arv->audio_region ();
	_pointer_offset = (r->fade_in()->back()->when + r->position()).distance (raw_grab_time());
}

void
FadeInDrag::motion (GdkEvent* event, bool)
{

	timepos_t tpos (timepos_t (_editor->canvas_event_sample (event)) + snap_delta (event->button.state));
	_editor->snap_to_with_modifier (tpos, event);
	tpos.shift_earlier (snap_delta (event->button.state));

	samplepos_t pos = tpos.samples ();

	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (_primary->region ());

	samplecnt_t fade_length;

	if (pos < (region->position_sample() + 64)) {
		fade_length = 64; // this should be a minimum defined somewhere
	} else if (pos > region->position_sample() + region->length_samples() - region->fade_out()->back()->when.samples()) {
		fade_length = region->length_samples() - region->fade_out()->back()->when.samples() - 1;
	} else {
		fade_length = pos - region->position_sample();
	}

	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		tmp->reset_fade_in_shape_width (tmp->audio_region(), fade_length);
	}

	show_verbose_cursor_duration (region->position(), region->position() + timepos_t (fade_length), 32);
	show_view_preview (region->position() + timepos_t (fade_length));
}

void
FadeInDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	timepos_t tpos (timepos_t (_editor->canvas_event_sample (event)) + snap_delta (event->button.state));
	_editor->snap_to_with_modifier (tpos, event);
	tpos.shift_earlier (snap_delta (event->button.state));

	samplepos_t pos = tpos.samples ();
	samplecnt_t fade_length;

	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (_primary->region ());

	if (pos < (region->position_sample() + 64)) {
		fade_length = 64; // this should be a minimum defined somewhere
	} else if (pos >= region->position_sample() + region->length_samples() - region->fade_out()->back()->when.samples()) {
		fade_length = region->length_samples() - region->fade_out()->back()->when.samples() - 1;
	} else {
		fade_length = pos - region->position_sample();
	}

	bool in_command = false;

	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		boost::shared_ptr<AutomationList> alist = tmp->audio_region()->fade_in();
		XMLNode &before = alist->get_state();

		tmp->audio_region()->set_fade_in_length (fade_length);
		tmp->audio_region()->set_fade_in_active (true);

		if (!in_command) {
			_editor->begin_reversible_command (_("change fade in length"));
			in_command = true;
		}
		XMLNode &after = alist->get_state();
		_editor->session()->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	}

	if (in_command) {
		_editor->commit_reversible_command ();
	}
}

void
FadeInDrag::aborted (bool)
{
	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		tmp->reset_fade_in_shape_width (tmp->audio_region(), tmp->audio_region()->fade_in()->back()->when.samples());
	}
}

FadeOutDrag::FadeOutDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
{
	DEBUG_TRACE (DEBUG::Drags, "New FadeOutDrag\n");
}

void
FadeOutDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	AudioRegionView* arv = dynamic_cast<AudioRegionView*> (_primary);
	boost::shared_ptr<AudioRegion> r = arv->audio_region ();
	setup_snap_delta (r->nt_last());

	show_verbose_cursor_duration (r->nt_last().earlier (r->fade_out()->back()->when), r->nt_last());
	show_view_preview (r->fade_out()->back()->when);
}

void
FadeOutDrag::setup_pointer_offset ()
{
	AudioRegionView* arv = dynamic_cast<AudioRegionView*> (_primary);
	boost::shared_ptr<AudioRegion> r = arv->audio_region ();
	_pointer_offset = (r->position() + (r->length() - r->fade_out()->back()->when)).distance (raw_grab_time());
}

void
FadeOutDrag::motion (GdkEvent* event, bool)
{
	samplecnt_t fade_length;

	timepos_t tpos (timepos_t (_editor->canvas_event_sample (event)) + snap_delta (event->button.state));
	_editor->snap_to_with_modifier (tpos, event);
	tpos.shift_earlier (snap_delta (event->button.state));

	samplepos_t pos (tpos.samples());

	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (_primary->region ());

	if (pos > (region->last_sample() - 64)) {
		fade_length = 64; // this should really be a minimum fade defined somewhere
	} else if (pos <= region->position_sample() + region->fade_in()->back()->when.samples()) {
		fade_length = region->length_samples() - region->fade_in()->back()->when.samples() - 1;
	} else {
		fade_length = region->last_sample() - pos;
	}

	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		tmp->reset_fade_out_shape_width (tmp->audio_region(), fade_length);
	}

	show_verbose_cursor_duration (timepos_t (region->last_sample() - fade_length), region->nt_last());
	show_view_preview (timepos_t (region->last_sample() - fade_length));
}

void
FadeOutDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	timepos_t tpos (timepos_t (_editor->canvas_event_sample (event)) + snap_delta (event->button.state));
	_editor->snap_to_with_modifier (tpos, event);
	tpos.shift_earlier (snap_delta (event->button.state));

	samplepos_t pos (tpos.samples());

	samplecnt_t fade_length;

	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (_primary->region ());

	if (pos > (region->last_sample() - 64)) {
		fade_length = 64; // this should really be a minimum fade defined somewhere
	} else if (pos <= region->position_sample() + region->fade_in()->back()->when.samples()) {
		fade_length = region->length_samples() - region->fade_in()->back()->when.samples() - 1;
	} else {
		fade_length = region->last_sample() - pos;
	}

	bool in_command = false;

	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		boost::shared_ptr<AutomationList> alist = tmp->audio_region()->fade_out();
		XMLNode &before = alist->get_state();

		tmp->audio_region()->set_fade_out_length (fade_length);
		tmp->audio_region()->set_fade_out_active (true);

		if (!in_command) {
			_editor->begin_reversible_command (_("change fade out length"));
			in_command = true;
		}
		XMLNode &after = alist->get_state();
		_editor->session()->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	}

	if (in_command) {
		_editor->commit_reversible_command ();
	}
}

void
FadeOutDrag::aborted (bool)
{
	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		tmp->reset_fade_out_shape_width (tmp->audio_region(), tmp->audio_region()->fade_out()->back()->when.samples());
	}
}

MarkerDrag::MarkerDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i, e->default_time_domain())
	, _selection_changed (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New MarkerDrag\n");
	Gtk::Window* toplevel = _editor->current_toplevel();
	_marker = reinterpret_cast<ArdourMarker*> (_item->get_data ("marker"));

	assert (_marker);

	_points.push_back (ArdourCanvas::Duple (0, 0));

	_points.push_back (ArdourCanvas::Duple (0, toplevel ? physical_screen_height (toplevel->get_window()) : 900));
}

MarkerDrag::~MarkerDrag ()
{
	for (CopiedLocationInfo::iterator i = _copied_locations.begin(); i != _copied_locations.end(); ++i) {
		delete i->location;
	}
}

MarkerDrag::CopiedLocationMarkerInfo::CopiedLocationMarkerInfo (Location* l, ArdourMarker* m)
{
	location = new Location (*l);
	markers.push_back (m);
	move_both = false;
}

void
MarkerDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	bool is_start;

	Location *location = _editor->find_location_from_marker (_marker, is_start);

	update_item (location);

	// _drag_line->show();
	// _line->raise_to_top();

	if (is_start) {
		show_verbose_cursor_time (location->start());
	} else {
		show_verbose_cursor_time (location->end());
	}
	show_view_preview ((is_start ? location->start() : location->end()) + _video_offset);
	setup_snap_delta (timepos_t (is_start ? location->start() : location->end()));

	Selection::Operation op = ArdourKeyboard::selection_type (event->button.state);

	switch (op) {
	case Selection::Toggle:
		/* we toggle on the button release */
		break;
	case Selection::Set:
		if (!_editor->selection->selected (_marker)) {
			_editor->selection->set (_marker);
			_selection_changed = true;
		}
		break;
	case Selection::Extend:
	{
		Locations::LocationList ll;
		list<ArdourMarker*> to_add;
		timepos_t s, e;
		_editor->selection->markers.range (s, e);
		s = min (_marker->position(), s);
		e = max (_marker->position(), e);
		s = min (s, e);
		e = max (s, e);
		if (e < timepos_t::max (e.time_domain())) {
			e.increment();
		}
		_editor->session()->locations()->find_all_between (s, e, ll, Location::Flags (0));
		for (Locations::LocationList::iterator i = ll.begin(); i != ll.end(); ++i) {
			Editor::LocationMarkers* lm = _editor->find_location_markers (*i);
			if (lm) {
				if (lm->start) {
					to_add.push_back (lm->start);
				}
				if (lm->end) {
					to_add.push_back (lm->end);
				}
			}
		}
		if (!to_add.empty()) {
			_editor->selection->add (to_add);
			_selection_changed = true;
		}
		break;
	}
	case Selection::Add:
		_editor->selection->add (_marker);
		_selection_changed = true;

		break;
	}

	/* Set up copies for us to manipulate during the drag
	 */

	for (MarkerSelection::iterator i = _editor->selection->markers.begin(); i != _editor->selection->markers.end(); ++i) {

		Location* l = _editor->find_location_from_marker (*i, is_start);

		if (!l) {
			continue;
		}

		if (l->is_mark()) {
			_copied_locations.push_back (CopiedLocationMarkerInfo (l, *i));
		} else {
			/* range: check that the other end of the range isn't
			   already there.
			*/
			CopiedLocationInfo::iterator x;
			for (x = _copied_locations.begin(); x != _copied_locations.end(); ++x) {
				if (*(*x).location == *l) {
					break;
				}
			}
			if (x == _copied_locations.end()) {
				_copied_locations.push_back (CopiedLocationMarkerInfo (l, *i));
			} else {
				(*x).markers.push_back (*i);
				(*x).move_both = true;
			}
		}

	}
}

void
MarkerDrag::setup_pointer_offset ()
{
	bool is_start;
	Location *location = _editor->find_location_from_marker (_marker, is_start);
	_pointer_offset = (is_start ? location->start() : location->end()).distance (raw_grab_time());
}

void
MarkerDrag::setup_video_offset ()
{
	_video_offset = timecnt_t (Temporal::AudioTime);
	_preview_video = true;
}

void
MarkerDrag::motion (GdkEvent* event, bool)
{
	timecnt_t f_delta;
	bool is_start;
	bool move_both = false;
	Location *real_location;
	Location *copy_location = 0;
	timecnt_t const sd = snap_delta (event->button.state);

	timepos_t const newpos = adjusted_time (_drags->current_pointer_time () + sd, event, true).earlier (sd);
	timepos_t next = newpos;

	if (Keyboard::modifier_state_contains (event->button.state, ArdourKeyboard::push_points_modifier ())) {
		move_both = true;
	}

	CopiedLocationInfo::iterator x;

	/* find the marker we're dragging, and compute the delta */

	for (x = _copied_locations.begin(); x != _copied_locations.end(); ++x) {

		copy_location = (*x).location;

		if (find (x->markers.begin(), x->markers.end(), _marker) != x->markers.end()) {

			/* this marker is represented by this
			 * CopiedLocationMarkerInfo
			 */

			if ((real_location = _editor->find_location_from_marker (_marker, is_start)) == 0) {
				/* que pasa ?? */
				return;
			}

			if (real_location->is_mark()) {
				f_delta = copy_location->start().distance (newpos);
			} else {


				switch (_marker->type()) {
				case ArdourMarker::SessionStart:
				case ArdourMarker::RangeStart:
				case ArdourMarker::LoopStart:
				case ArdourMarker::PunchIn:
					f_delta = copy_location->start().distance (newpos);
					break;

				case ArdourMarker::SessionEnd:
				case ArdourMarker::RangeEnd:
				case ArdourMarker::LoopEnd:
				case ArdourMarker::PunchOut:
					f_delta = copy_location->end().distance (newpos);
					break;
				default:
					/* what kind of marker is this ? */
					return;
				}
			}

			break;
		}
	}

	if (x == _copied_locations.end()) {
		/* hmm, impossible - we didn't find the dragged marker */
		return;
	}

	/* now move them all */

	for (x = _copied_locations.begin(); x != _copied_locations.end(); ++x) {

		copy_location = x->location;

		if ((real_location = _editor->find_location_from_marker (x->markers.front(), is_start)) == 0) {
			continue;
		}

		if (real_location->locked()) {
			continue;
		}

		if (copy_location->is_mark()) {

			/* now move it */

			if (copy_location->is_cue_marker()) {
				/* cue marks should always move with snapping */
				timepos_t s (copy_location->start() + f_delta);
				_editor->snap_to (s, Temporal::RoundNearest, SnapToGrid_Scaled, true);
				copy_location->set_start (s, false);
			} else {
				copy_location->set_start (copy_location->start() + f_delta, false);
			}

		} else {

			timepos_t new_start = copy_location->start() + f_delta;
			timepos_t new_end = copy_location->end() + f_delta;

			if (is_start) { // start-of-range marker

				if (move_both || (*x).move_both) {
					copy_location->set_start (new_start, false);
					copy_location->set_end (new_end, false);
				} else if (new_start < copy_location->end()) {
					copy_location->set_start (new_start, false);
				} else if (newpos.is_positive()) {
					//_editor->snap_to (next, RoundUpAlways, true);
					copy_location->set_end (next, false);
					copy_location->set_start (newpos, false);
				}

			} else { // end marker

				if (move_both || (*x).move_both) {
					copy_location->set_end (new_end);
					copy_location->set_start (new_start, false);
				} else if (new_end > copy_location->start()) {
					copy_location->set_end (new_end, false);
				} else if (newpos.is_positive()) {
					//_editor->snap_to (next, RoundDownAlways, true);
					copy_location->set_start (next, false);
					copy_location->set_end (newpos, false);
				}
			}
		}

		update_item (copy_location);

		/* now lookup the actual GUI items used to display this
		 * location and move them to wherever the copy of the location
		 * is now. This means that the logic in ARDOUR::Location is
		 * still enforced, even though we are not (yet) modifying
		 * the real Location itself.
		 */

		Editor::LocationMarkers* lm = _editor->find_location_markers (real_location);

		if (lm) {
			lm->set_position (copy_location->start(), copy_location->end());
		}

	}

	assert (!_copied_locations.empty());

	show_verbose_cursor_time (newpos);
	show_view_preview (newpos + _video_offset);
	_editor->set_snapped_cursor_position (newpos);
}

void
MarkerDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {

		if (was_double_click()) {
			_editor->rename_marker (_marker);
			return;
		}

		/* just a click, do nothing but finish
		   off the selection process
		*/

		Selection::Operation op = ArdourKeyboard::selection_type (event->button.state);
		switch (op) {
		case Selection::Set:
			if (_editor->selection->selected (_marker) && _editor->selection->markers.size() > 1) {
				_editor->selection->set (_marker);
				_selection_changed = true;
			}
			break;

		case Selection::Toggle:
			/* we toggle on the button release, click only */
			_editor->selection->toggle (_marker);
			_selection_changed = true;

			break;

		case Selection::Extend:
		case Selection::Add:
			break;
		}

		if (_selection_changed) {
			_editor->begin_reversible_selection_op(X_("Select Marker Release"));
			_editor->commit_reversible_selection_op();
		}

		return;
	}

	XMLNode &before = _editor->session()->locations()->get_state();
	bool in_command = false;

	MarkerSelection::iterator i;
	CopiedLocationInfo::iterator x;
	bool is_start;

	for (i = _editor->selection->markers.begin(), x = _copied_locations.begin();
	     x != _copied_locations.end() && i != _editor->selection->markers.end();
	     ++i, ++x) {

		Location * location = _editor->find_location_from_marker (*i, is_start);

		if (location) {

			if (location->locked()) {
				continue;
			}
			if (!in_command) {
				_editor->begin_reversible_command ( _("move marker") );
				in_command = true;
			}
			if (location->is_mark()) {
				location->set_start (((*x).location)->start(), false);
			} else {
				location->set (((*x).location)->start(), ((*x).location)->end());
			}

			if (location->is_session_range()) {
				_editor->session()->set_session_range_is_free (false);
			}
		}
	}

	if (in_command) {
		XMLNode &after = _editor->session()->locations()->get_state();
		_editor->session()->add_command(new MementoCommand<Locations>(*(_editor->session()->locations()), &before, &after));
		_editor->commit_reversible_command ();
	}
}

void
MarkerDrag::aborted (bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	for (CopiedLocationInfo::iterator x = _copied_locations.begin(); x != _copied_locations.end(); ++x) {

		/* move all markers to their original location */


		for (vector<ArdourMarker*>::iterator m = x->markers.begin(); m != x->markers.end(); ++m) {

			bool is_start;
			Location * location = _editor->find_location_from_marker (*m, is_start);

			if (location) {
				(*m)->set_position (is_start ? location->start() : location->end());
			}
		}
	}
}

void
MarkerDrag::update_item (Location*)
{
	/* noop */
}

ControlPointDrag::ControlPointDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i, e->default_time_domain()) /* XXX NUTEMPO FIX TIME DOMAIN */
	, _fixed_grab_x (0.0)
	, _fixed_grab_y (0.0)
	, _cumulative_x_drag (0.0)
	, _cumulative_y_drag (0.0)
	, _pushing (false)
	, _final_index (0)
{
	if (_zero_gain_fraction < 0.0) {
		_zero_gain_fraction = gain_to_slider_position_with_max (dB_to_coefficient (0.0), Config->get_max_gain());
	}

	DEBUG_TRACE (DEBUG::Drags, "New ControlPointDrag\n");

	_point = reinterpret_cast<ControlPoint*> (_item->get_data ("control_point"));
	assert (_point);
}


void
ControlPointDrag::start_grab (GdkEvent* event, Gdk::Cursor* /*cursor*/)
{
	Drag::start_grab (event, _editor->cursors()->fader);

	// start the grab at the center of the control point so
	// the point doesn't 'jump' to the mouse after the first drag
	_fixed_grab_x = _point->get_x() + _editor->time_to_pixel_unrounded (timepos_t (_point->line().offset()));
	_fixed_grab_y = _point->get_y();

	samplepos_t s = _editor->pixel_to_sample (_fixed_grab_x);

	if (_editor->default_time_domain() == Temporal::AudioTime) {
		setup_snap_delta (timepos_t (s));
	} else {
		setup_snap_delta (timepos_t (timepos_t (s).beats()));
	}

	float const fraction = 1 - (_point->get_y() / _point->line().height());
	show_verbose_cursor_text (_point->line().get_verbose_cursor_string (fraction));

	_pushing = Keyboard::modifier_state_equals (event->button.state, ArdourKeyboard::push_points_modifier ());
}

void
ControlPointDrag::motion (GdkEvent* event, bool first_motion)
{
	double dx = _drags->current_pointer_x() - last_pointer_x();
	double dy = current_pointer_y() - last_pointer_y();
	bool need_snap = true;

	if (Keyboard::modifier_state_equals (event->button.state, ArdourKeyboard::fine_adjust_modifier ())) {
		dx *= 0.1;
		dy *= 0.1;
		need_snap = false;
	}

	/* coordinate in pixels relative to the start of the region (for region-based automation)
	   or track (for track-based automation) */
	double cx = _fixed_grab_x + _cumulative_x_drag + dx;
	double cy = _fixed_grab_y + _cumulative_y_drag + dy;

	// calculate zero crossing point. back off by .01 to stay on the
	// positive side of zero
	double const zero_gain_y = (1.0 - _zero_gain_fraction) * _point->line().height() - .01;

	if (!_point->can_slide ()) {
		cx = _fixed_grab_x;
	}
	if (_y_constrained) {
		cy = _fixed_grab_y;
	}

	_cumulative_x_drag = cx - _fixed_grab_x;
	_cumulative_y_drag = cy - _fixed_grab_y;

	cx = max (0.0, cx);
	cy = max (0.0, cy);
	cy = min ((double) _point->line().height(), cy);

	// make sure we hit zero when passing through
	if ((cy < zero_gain_y && (cy - dy) > zero_gain_y) || (cy > zero_gain_y && (cy - dy) < zero_gain_y)) {
		cy = zero_gain_y;
	}

	timepos_t cx_pos (timepos_t (pixel_to_time (cx)) + snap_delta (event->button.state));

	if (need_snap) {
		_editor->snap_to_with_modifier (cx_pos, event);
	}

	cx_pos.shift_earlier (snap_delta (event->button.state));
	cx_pos = min (cx_pos, _point->line().maximum_time() + _point->line().offset());

	float const fraction = 1.0 - (cy / _point->line().height());

	if (first_motion) {
		float const initial_fraction = 1.0 - (_fixed_grab_y / _point->line().height());
		_editor->begin_reversible_command (_("automation event move"));
		_point->line().start_drag_single (_point, _fixed_grab_x, initial_fraction);
	}
	pair<float, float> result;
	result = _point->line().drag_motion (_editor->time_to_pixel_unrounded (cx_pos), fraction, false, _pushing, _final_index);
	show_verbose_cursor_text (_point->line().get_verbose_cursor_relative_string (result.first, result.second));
}

void
ControlPointDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {

		/* just a click */
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::ModifierMask (Keyboard::TertiaryModifier))) {
			_editor->reset_point_selection ();
		}

	} else {
		_point->line().end_drag (_pushing, _final_index);
		_editor->commit_reversible_command ();
	}
}

void
ControlPointDrag::aborted (bool)
{
	_point->line().reset ();
}

bool
ControlPointDrag::active (Editing::MouseMode m)
{
	if (m == Editing::MouseDraw) {
		/* always active in mouse draw */
		return true;
	}

	/* otherwise active if the point is on an automation line (ie not if its on a region gain line) */
	return dynamic_cast<AutomationLine*> (&(_point->line())) != 0;
}

LineDrag::LineDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i, e->default_time_domain())
	, _line (0)
	, _fixed_grab_x (0.0)
	, _fixed_grab_y (0.0)
	, _cumulative_y_drag (0)
	, _before (0)
	, _after (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New LineDrag\n");
}

void
LineDrag::start_grab (GdkEvent* event, Gdk::Cursor* /*cursor*/)
{
	_line = reinterpret_cast<AutomationLine*> (_item->get_data ("line"));
	assert (_line);

	_item = &_line->grab_item ();

	/* need to get x coordinate in terms of parent (TimeAxisItemView)
	   origin, and ditto for y.
	*/

	double mx = event->button.x;
	double my = event->button.y;

	_line->grab_item().canvas_to_item (mx, my);

	samplecnt_t const sample_within_region = (samplecnt_t) floor (mx * _editor->samples_per_pixel);

	if (!_line->control_points_adjacent (sample_within_region, _before, _after)) {
		/* no adjacent points */
		return;
	}

	Drag::start_grab (event, _editor->cursors()->fader);

	/* store grab start in item sample */
	double const bx = _line->nth (_before)->get_x();
	double const ax = _line->nth (_after)->get_x();
	double const click_ratio = (ax - mx) / (ax - bx);

	double const cy = ((_line->nth (_before)->get_y() * click_ratio) + (_line->nth (_after)->get_y() * (1 - click_ratio)));

	_fixed_grab_x = mx;
	_fixed_grab_y = cy;

	double fraction = 1.0 - (cy / _line->height());

	show_verbose_cursor_text (_line->get_verbose_cursor_string (fraction));
}

void
LineDrag::motion (GdkEvent* event, bool first_move)
{
	double dy = current_pointer_y() - last_pointer_y();

	if (Keyboard::modifier_state_equals (event->button.state, ArdourKeyboard::fine_adjust_modifier ())) {
		dy *= 0.1;
	}

	double cy = _fixed_grab_y + _cumulative_y_drag + dy;

	_cumulative_y_drag = cy - _fixed_grab_y;

	cy = max (0.0, cy);
	cy = min ((double) _line->height(), cy);

	double const fraction = 1.0 - (cy / _line->height());
	uint32_t ignored;

	if (first_move) {
		float const initial_fraction = 1.0 - (_fixed_grab_y / _line->height());

		_editor->begin_reversible_command (_("automation range move"));
		_line->start_drag_line (_before, _after, initial_fraction);
	}

	/* we are ignoring x position for this drag, so we can just pass in anything */
	pair<float, float> result;

	result = _line->drag_motion (0, fraction, true, false, ignored);
	show_verbose_cursor_text (_line->get_verbose_cursor_relative_string (result.first, result.second));
}

void
LineDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {
		motion (event, false);
		_line->end_drag (false, 0);
		_editor->commit_reversible_command ();
	} else {
		/* add a new control point on the line */

		AutomationTimeAxisView* atv;

		if ((atv = dynamic_cast<AutomationTimeAxisView*>(_editor->clicked_axisview)) != 0) {
			timepos_t where = grab_time ();

			double cx = 0;
			double cy = _fixed_grab_y;

			_line->grab_item().item_to_canvas (cx, cy);

			atv->add_automation_event (event, where, cy, false);
		} else if (dynamic_cast<AudioTimeAxisView*>(_editor->clicked_axisview) != 0) {
			AudioRegionView* arv;

			if ((arv = dynamic_cast<AudioRegionView*>(_editor->clicked_regionview)) != 0) {
				arv->add_gain_point_event (&arv->get_gain_line()->grab_item(), event, false);
			}
		}
	}
}

void
LineDrag::aborted (bool)
{
	_line->reset ();
}

FeatureLineDrag::FeatureLineDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i, e->default_time_domain()),
	  _line (0),
	  _arv (0),
	  _region_view_grab_x (0.0),
	  _cumulative_x_drag (0),
	  _before (0.0),
	  _max_x (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New FeatureLineDrag\n");
}

void
FeatureLineDrag::start_grab (GdkEvent* event, Gdk::Cursor* /*cursor*/)
{
	Drag::start_grab (event);

	_line = reinterpret_cast<Line*> (_item);
	assert (_line);

	/* need to get x coordinate in terms of parent (AudioRegionView) origin. */

	double cx = event->button.x;
	double cy = event->button.y;

	_item->parent()->canvas_to_item (cx, cy);

	/* store grab start in parent sample */
	_region_view_grab_x = cx;

	_before = *(float*) _item->get_data ("position");

	_arv = reinterpret_cast<AudioRegionView*> (_item->get_data ("regionview"));

	_max_x = _editor->duration_to_pixels (_arv->get_duration());
}

void
FeatureLineDrag::motion (GdkEvent*, bool)
{
	double dx = _drags->current_pointer_x() - last_pointer_x();

	double cx = _region_view_grab_x + _cumulative_x_drag + dx;

	_cumulative_x_drag += dx;

	/* Clamp the min and max extent of the drag to keep it within the region view bounds */

	if (cx > _max_x){
		cx = _max_x;
	}
	else if(cx < 0){
		cx = 0;
	}

	boost::optional<ArdourCanvas::Rect> bbox = _line->bounding_box ();
	assert (bbox);
	_line->set (ArdourCanvas::Duple (cx, 2.0), ArdourCanvas::Duple (cx, bbox.get().height ()));

	float *pos = new float;
	*pos = cx;

	_line->set_data ("position", pos);

	_before = cx;
}

void
FeatureLineDrag::finished (GdkEvent*, bool)
{
	_arv = reinterpret_cast<AudioRegionView*> (_item->get_data ("regionview"));
	_arv->update_transient(_before, _before);
}

void
FeatureLineDrag::aborted (bool)
{
	//_line->reset ();
}

RubberbandSelectDrag::RubberbandSelectDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i, e->default_time_domain())
	, _vertical_only (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New RubberbandSelectDrag\n");
}

void
RubberbandSelectDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event);
	show_verbose_cursor_time (adjusted_current_time (event, UIConfiguration::instance().get_rubberbanding_snaps_to_grid()));
}

void
RubberbandSelectDrag::motion (GdkEvent* event, bool)
{
	timepos_t start;
	timepos_t end;
	double y1;
	double y2;
	timepos_t const pf = adjusted_current_time (event, UIConfiguration::instance().get_rubberbanding_snaps_to_grid());
	timepos_t grab (grab_time ());

	if (UIConfiguration::instance().get_rubberbanding_snaps_to_grid ()) {
		_editor->snap_to_with_modifier (grab, event, RoundNearest, SnapToGrid_Scaled);
	} else {
		grab = raw_grab_time ();
	}

	/* base start and end on initial click position */

	if (pf < grab) {
		start = pf;
		end = grab;
	} else {
		end = pf;
		start = grab;
	}

	if (current_pointer_y() < grab_y()) {
		y1 = current_pointer_y();
		y2 = grab_y();
	} else {
		y2 = current_pointer_y();
		y1 = grab_y();
	}

	if (start != end || y1 != y2) {

		double x1 = _editor->time_to_pixel (start);
		double x2 = _editor->time_to_pixel (end);
		const double min_dimension = 2.0;

		if (_vertical_only) {
			/* fixed 10 pixel width */
			x2 = x1 + 10;
		} else {
			if (x2 < x1) {
				x2 = min (x1 - min_dimension, x2);
			} else {
				x2 = max (x1 + min_dimension, x2);
			}
		}

		if (y2 < y1) {
			y2 = min (y1 - min_dimension, y2);
		} else {
			y2 = max (y1 + min_dimension, y2);
		}

		/* translate rect into item space and set */

		ArdourCanvas::Rect r (x1, y1, x2, y2);

		/* this drag is a _trackview_only == true drag, so the y1 and
		 * y2 (computed using current_pointer_y() and grab_y()) will be
		 * relative to the top of the trackview group). The
		 * rubberband rect has the same parent/scroll offset as the
		 * the trackview group, so we can use the "r" rect directly
		 * to set the shape of the rubberband.
		 */

		_editor->rubberband_rect->set (r);
		_editor->rubberband_rect->show();
		_editor->rubberband_rect->raise_to_top();

		show_verbose_cursor_time (pf);

		do_select_things (event, true);
	}
}

void
RubberbandSelectDrag::do_select_things (GdkEvent* event, bool drag_in_progress)
{
	timepos_t x1;
	timepos_t x2;
	timepos_t grab = grab_time ();
	timepos_t lpf = last_pointer_time ();

	if (!UIConfiguration::instance().get_rubberbanding_snaps_to_grid ()) {
		grab = raw_grab_time ();

		timepos_t pos (pixel_to_time (last_pointer_x()));

		if (_editor->default_time_domain() == Temporal::AudioTime) {
			lpf = pos;
		} else {
			lpf = timepos_t (pos.beats());
		}
	}

	if (grab < lpf) {
		x1 = grab;
		x2 = lpf;
	} else {
		x2 = grab;
		x1 = lpf;
	}

	double y1;
	double y2;

	if (current_pointer_y() < grab_y()) {
		y1 = current_pointer_y();
		y2 = grab_y();
	} else {
		y2 = current_pointer_y();
		y1 = grab_y();
	}

	select_things (event->button.state, x1, x2, y1, y2, drag_in_progress);
}

void
RubberbandSelectDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {

		motion (event, false);
		do_select_things (event, false);

	} else {

		/* just a click */

		bool do_deselect = true;
		MidiTimeAxisView* mtv;
		AutomationTimeAxisView* atv;

		if ((mtv = dynamic_cast<MidiTimeAxisView*>(_editor->clicked_axisview)) != 0) {
			/* MIDI track */
			if (_editor->selection->empty() && _editor->mouse_mode == MouseDraw) {
				/* nothing selected */
				add_midi_region (mtv, true);
				do_deselect = false;
			}
		} else if ((atv = dynamic_cast<AutomationTimeAxisView*>(_editor->clicked_axisview)) != 0) {
			timepos_t where = grab_time ();
			atv->add_automation_event (event, where, event->button.y, false);
			do_deselect = false;
		}

		/* do not deselect if Primary or Tertiary (toggle-select or
		 * extend-select are pressed.
		 */

		if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::PrimaryModifier) &&
		    !Keyboard::modifier_state_contains (event->button.state, Keyboard::TertiaryModifier) &&
		    do_deselect) {
			deselect_things ();
		}

	}

	_editor->rubberband_rect->hide();
}

void
RubberbandSelectDrag::aborted (bool)
{
	_editor->rubberband_rect->hide ();
}

TimeFXDrag::TimeFXDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, std::list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
{
	DEBUG_TRACE (DEBUG::Drags, "New TimeFXDrag\n");
	_preview_video = false;
}

void
TimeFXDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	_editor->get_selection().add (_primary);
	timepos_t where (_primary->region()->position());
	setup_snap_delta (_primary->region()->position());

	show_verbose_cursor_duration (where, adjusted_current_time (event), 0);
}

void
TimeFXDrag::motion (GdkEvent* event, bool)
{
	RegionView* rv = _primary;
	StreamView* cv = rv->get_time_axis_view().view ();
	pair<TimeAxisView*, double> const tv = _editor->trackview_by_y_position (grab_y());
	int layer = tv.first->layer_display() == Overlaid ? 0 : tv.second;
	int layers = tv.first->layer_display() == Overlaid ? 1 : cv->layers();
	timepos_t pf (_editor->canvas_event_time (event) + snap_delta (event->button.state));

	_editor->snap_to_with_modifier (pf, event);
	pf.shift_earlier (snap_delta (event->button.state));

	if (pf > rv->region()->position()) {
		rv->get_time_axis_view().show_timestretch (rv->region()->position(), pf, layers, layer);
	}

	show_verbose_cursor_duration (_primary->region()->position(), pf);
}

void
TimeFXDrag::finished (GdkEvent* event, bool movement_occurred)
{
	/* this may have been a single click, no drag. We still want the dialog
	   to show up in that case, so that the user can manually edit the
	   parameters for the timestretch.
	*/

	ratio_t ratio (1, 1);

	if (movement_occurred) {

		motion (event, false);

		_primary->get_time_axis_view().hide_timestretch ();

		timepos_t adjusted_pos = adjusted_current_time (event);

		if (adjusted_pos < _primary->region()->position()) {
			/* backwards drag of the left edge - not usable */
			return;
		}

		timecnt_t newlen = _primary->region()->position().distance (adjusted_pos);

		if (_primary->region()->length().time_domain() == Temporal::BeatTime) {
			ratio = ratio_t (newlen.ticks(), _primary->region()->length().ticks());
		} else {
			ratio = ratio_t (newlen.samples(), _primary->region()->length().samples());
		}

#ifndef USE_RUBBERBAND
		// Soundtouch uses fraction / 100 instead of normal (/ 1)
#warning NUTEMPO timefx request now uses a rational type so this needs revisiting
		if (_primary->region()->data_type() == DataType::AUDIO) {
			ratio = ((newlen - _primary->region()->length()) / newlen) * 100;
		}
#endif
	}

	if (!_editor->get_selection().regions.empty()) {
		/* primary will already be included in the selection, and edit
		   group shared editing will propagate selection across
		   equivalent regions, so just use the current region
		   selection.
		*/

		if (_editor->time_stretch (_editor->get_selection().regions, ratio) == -1) {
			error << _("An error occurred while executing time stretch operation") << endmsg;
		}
	}
}

void
TimeFXDrag::aborted (bool)
{
	_primary->get_time_axis_view().hide_timestretch ();
}

ScrubDrag::ScrubDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i, Temporal::AudioTime)
{
	DEBUG_TRACE (DEBUG::Drags, "New ScrubDrag\n");
}

void
ScrubDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event);
}

void
ScrubDrag::motion (GdkEvent* /*event*/, bool)
{
	_editor->scrub (adjusted_current_time (0, false).samples(), _drags->current_pointer_x ());
}

void
ScrubDrag::finished (GdkEvent* /*event*/, bool movement_occurred)
{
	if (movement_occurred && _editor->session()) {
		/* make sure we stop */
		_editor->session()->request_stop ();
	}
}

void
ScrubDrag::aborted (bool)
{
	/* XXX: TODO */
}

SelectionDrag::SelectionDrag (Editor* e, ArdourCanvas::Item* i, Operation o)
	: Drag (e, i, e->default_time_domain())
	, _operation (o)
	, _add (false)
	, _time_selection_at_start (!_editor->get_selection().time.empty())
{
	DEBUG_TRACE (DEBUG::Drags, "New SelectionDrag\n");

	if (_time_selection_at_start) {
		start_at_start = _editor->get_selection().time.start_time();
		end_at_start = _editor->get_selection().time.end_time();
	}
}

void
SelectionDrag::start_grab (GdkEvent* event, Gdk::Cursor*)
{
	if (_editor->session() == 0) {
		return;
	}

	Gdk::Cursor* cursor = MouseCursors::invalid_cursor();

	switch (_operation) {
	case CreateSelection:
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::CopyModifier)) {
			_add = true;
		} else {
			_add = false;
		}
		cursor = _editor->cursors()->selector;
		Drag::start_grab (event, cursor);
		break;

	case SelectionStartTrim:
		if (_editor->clicked_axisview) {
			_editor->clicked_axisview->order_selection_trims (_item, true);
		}
		Drag::start_grab (event, _editor->cursors()->left_side_trim);
		break;

	case SelectionEndTrim:
		if (_editor->clicked_axisview) {
			_editor->clicked_axisview->order_selection_trims (_item, false);
		}
		Drag::start_grab (event, _editor->cursors()->right_side_trim);
		break;

	case SelectionMove:
		Drag::start_grab (event, cursor);
		break;

	case SelectionExtend:
		Drag::start_grab (event, cursor);
		break;
	}

	if (_operation == SelectionMove) {
		show_verbose_cursor_time (_editor->selection->time[_editor->clicked_selection].start());
	} else {
		show_verbose_cursor_time (adjusted_current_time (event));
	}
}

void
SelectionDrag::setup_pointer_offset ()
{
	switch (_operation) {
	case CreateSelection:
		_pointer_offset = timecnt_t (_editor->default_time_domain());
		break;

	case SelectionStartTrim:
	case SelectionMove:
		_pointer_offset = _editor->selection->time[_editor->clicked_selection].start().distance (raw_grab_time());
		break;

	case SelectionEndTrim:
		_pointer_offset = _editor->selection->time[_editor->clicked_selection].end().distance (raw_grab_time());
		break;

	case SelectionExtend:
		break;
	}
}

void
SelectionDrag::motion (GdkEvent* event, bool first_move)
{
	timepos_t start;
	timepos_t end;
	timecnt_t length;
	timecnt_t distance;
	timepos_t start_mf;
	timepos_t const pending_position = adjusted_current_time (event);

	if (_operation != CreateSelection && pending_position == last_pointer_time()) {
		return;
	}

	if (first_move) {
		if (Config->get_edit_mode() == RippleAll && !Config->get_interview_editing()) {
			_editor->selection->set (_editor->get_track_views());
		}
		_track_selection_at_start = _editor->selection->tracks;
	}

	switch (_operation) {
	case CreateSelection:
	{
		timepos_t grab (grab_time());
		if (first_move) {
			grab = adjusted_current_time (event, false);
			if (grab < pending_position) {
				_editor->snap_to (grab, RoundDownMaybe);
			}  else {
				_editor->snap_to (grab, RoundUpMaybe);
			}
		}

		if (pending_position < grab) {
			start = pending_position;
			end = grab;
		} else {
			end = pending_position;
			start = grab;
		}

		/* first drag: Either add to the selection
		   or create a new selection
		*/

		if (first_move) {

			if (_add) {

				/* adding to the selection */
				_editor->set_selected_track_as_side_effect (Selection::Add);
				_editor->clicked_selection = _editor->selection->add (start, end);
				_add = false;

			} else {

				/* new selection */

				if (_editor->clicked_axisview && !_editor->selection->selected (_editor->clicked_axisview)) {
					_editor->set_selected_track_as_side_effect (Selection::Set);
				}

				_editor->clicked_selection = _editor->selection->set (start, end);
			}
		}

		//if user is selecting a range on an automation track, bail out here before we get to the grouped stuff,
		// because the grouped stuff will start working on tracks (routeTAVs), and end up removing this
		AutomationTimeAxisView *atest = dynamic_cast<AutomationTimeAxisView *>(_editor->clicked_axisview);
		if (atest) {
			_editor->selection->add (atest);
			break;
		}

		/* select all tracks within the rectangle that we've marked out so far */
		TrackViewList new_selection;
		TrackViewList& all_tracks (_editor->track_views);

		ArdourCanvas::Coord const top = grab_y();
		ArdourCanvas::Coord const bottom = current_pointer_y();

		if ((Config->get_edit_mode() != RippleAll || Config->get_interview_editing()) && top >= 0 && bottom >= 0) {

			//first, find the tracks that are covered in the y range selection
			for (TrackViewList::const_iterator i = all_tracks.begin(); i != all_tracks.end(); ++i) {
				if ((*i)->covered_by_y_range (top, bottom)) {
					new_selection.push_back (*i);
				}
			}

			//now compare our list with the current selection, and add as necessary
			//( NOTE: most mouse moves don't change the selection so we can't just SET it for every mouse move; it gets clunky )
			TrackViewList tracks_to_add;
			TrackViewList tracks_to_remove;
			vector<RouteGroup*> selected_route_groups;

			if (!first_move) {
				for (TrackViewList::const_iterator i = _editor->selection->tracks.begin(); i != _editor->selection->tracks.end(); ++i) {
					if (!new_selection.contains (*i) && !_track_selection_at_start.contains (*i)) {
						tracks_to_remove.push_back (*i);
					} else {
						RouteGroup* rg = (*i)->route_group();
						if (rg && rg->is_active() && rg->is_select()) {
							selected_route_groups.push_back (rg);
						}
					}
				}
			}

			for (TrackViewList::const_iterator i = new_selection.begin(); i != new_selection.end(); ++i) {
				if (!_editor->selection->tracks.contains (*i)) {
					tracks_to_add.push_back (*i);
					RouteGroup* rg = (*i)->route_group();

					if (rg && rg->is_active() && rg->is_select()) {
						selected_route_groups.push_back (rg);
					}
				}
			}

			_editor->selection->add (tracks_to_add);

			if (!tracks_to_remove.empty()) {

				/* check all these to-be-removed tracks against the
				 * possibility that they are selected by being
				 * in the same group as an approved track.
				 */

				for (TrackViewList::iterator i = tracks_to_remove.begin(); i != tracks_to_remove.end(); ) {
					RouteGroup* rg = (*i)->route_group();

					if (rg && find (selected_route_groups.begin(), selected_route_groups.end(), rg) != selected_route_groups.end()) {
						i = tracks_to_remove.erase (i);
					} else {
						++i;
					}
				}

				/* remove whatever is left */

				_editor->selection->remove (tracks_to_remove);
			}
		}
	}
	break;

	case SelectionStartTrim:

		end = _editor->selection->time[_editor->clicked_selection].end();

		if (pending_position > end) {
			start = end;
		} else {
			start = pending_position;
		}
		break;

	case SelectionEndTrim:

		start = _editor->selection->time[_editor->clicked_selection].start();

		if (pending_position < start) {
			end = start;
		} else {
			end = pending_position;
		}

		break;

	case SelectionMove:

		start = _editor->selection->time[_editor->clicked_selection].start();
		end = _editor->selection->time[_editor->clicked_selection].end();

		length = start.distance (end);;
		distance = start.distance (pending_position);
		start = pending_position;

		start_mf = start;
		_editor->snap_to (start_mf);

		end = start_mf + length;

		break;

	case SelectionExtend:
		break;
	}

	if (start != end) {
		switch (_operation) {
		case SelectionMove:
			if (_time_selection_at_start) {
				_editor->selection->move_time (distance);
			}
			break;
		default:
			_editor->selection->replace (_editor->clicked_selection, start, end);
		}
	}

	if (_operation == SelectionMove) {
		show_verbose_cursor_time(start);
	} else {
		show_verbose_cursor_time(pending_position);
	}
}

void
SelectionDrag::finished (GdkEvent* event, bool movement_occurred)
{
	Session* s = _editor->session();

	_editor->begin_reversible_selection_op (X_("Change Time Selection"));
	if (movement_occurred) {
		motion (event, false);
		/* XXX this is not object-oriented programming at all. ick */
		if (_editor->selection->time.consolidate()) {
			_editor->selection->TimeChanged ();
		}

		/* XXX what if its a music time selection? */
		if (s) {

			//if Follow Edits is on, maybe try to follow the range selection  ... also consider range-audition mode
			if ( !s->config.get_external_sync() && s->transport_rolling() ) {
				if ( s->solo_selection_active() ) {
					_editor->play_solo_selection(true);  //play the newly selected range, and move solos to match
				} else if ( UIConfiguration::instance().get_follow_edits() && s->get_play_range() ) {  //already rolling a selected range
					s->request_play_range (&_editor->selection->time, true);  //play the newly selected range
				}
			} else if ( !s->transport_rolling() && UIConfiguration::instance().get_follow_edits() ) {
				s->request_locate (_editor->get_selection().time.start_sample());
			}

			if (_editor->get_selection().time.length() != 0) {
				s->set_range_selection (_editor->get_selection().time.start_time(), _editor->get_selection().time.end_time ());
			} else {
				s->clear_range_selection ();
			}
		}

	} else {
		/* just a click, no pointer movement.
		 */

		if (was_double_click()) {
			if (UIConfiguration::instance().get_use_double_click_to_zoom_to_selection()) {
				_editor->temporal_zoom_selection (Both);
				return;
			}
		}

		if (_operation == SelectionExtend) {
			if (_time_selection_at_start) {
				timepos_t pos = adjusted_current_time (event, false);
				timepos_t start = min (pos, start_at_start);
				timepos_t end = max (pos, end_at_start);
				_editor->selection->set (start, end);
			}
		} else {
			if (Keyboard::modifier_state_equals (event->button.state, Keyboard::CopyModifier)) {
				if (_editor->clicked_selection) {
					_editor->selection->remove (_editor->clicked_selection);
				}
			} else {
				if (!_editor->clicked_selection) {
					_editor->selection->clear_time();
				}
			}
		}

		if (_editor->clicked_axisview && !_editor->selection->selected (_editor->clicked_axisview)) {
			_editor->selection->set (_editor->clicked_axisview);
		}

		if (s && s->get_play_range () && s->transport_rolling()) {
			s->request_stop (false, false);
		}

	}

	_editor->stop_canvas_autoscroll ();
	_editor->clicked_selection = 0;
	_editor->commit_reversible_selection_op ();
}

void
SelectionDrag::aborted (bool)
{
	/* XXX: TODO */
}

RangeMarkerBarDrag::RangeMarkerBarDrag (Editor* e, ArdourCanvas::Item* i, Operation o)
	: Drag (e, i, e->default_time_domain(), false),
	  _operation (o),
	  _copy (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New RangeMarkerBarDrag\n");

	_drag_rect = new ArdourCanvas::Rectangle (_editor->time_line_group,
						  ArdourCanvas::Rect (0.0, 0.0, 0.0,
						                      physical_screen_height (_editor->current_toplevel()->get_window())));
	_drag_rect->hide ();

	_drag_rect->set_fill_color (UIConfiguration::instance().color ("range drag rect"));
	_drag_rect->set_outline_color (UIConfiguration::instance().color ("range drag rect"));
}

RangeMarkerBarDrag::~RangeMarkerBarDrag()
{
	/* normal canvas items will be cleaned up when their parent group is deleted. But
	   this item is created as the child of a long-lived parent group, and so we
	   need to explicitly delete it.
	*/
	delete _drag_rect;
}

void
RangeMarkerBarDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	if (_editor->session() == 0) {
		return;
	}

	Gdk::Cursor* cursor = MouseCursors::invalid_cursor();

	if (!_editor->temp_location) {
		_editor->temp_location = new Location (*_editor->session());
	}

	switch (_operation) {
	case CreateSkipMarker:
	case CreateRangeMarker:
	case CreateTransportMarker:
	case CreateCDMarker:
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::CopyModifier)) {
			_copy = true;
		} else {
			_copy = false;
		}
		cursor = _editor->cursors()->selector;
		break;
	}

	Drag::start_grab (event, cursor);

	show_verbose_cursor_time (adjusted_current_time (event));
}

void
RangeMarkerBarDrag::motion (GdkEvent* event, bool first_move)
{
	timepos_t start;
	timepos_t end;
	ArdourCanvas::Rectangle *crect;

	switch (_operation) {
	case CreateSkipMarker:
		crect = _editor->range_bar_drag_rect;
		break;
	case CreateRangeMarker:
		crect = _editor->range_bar_drag_rect;
		break;
	case CreateTransportMarker:
		crect = _editor->transport_bar_drag_rect;
		break;
	case CreateCDMarker:
		crect = _editor->cd_marker_bar_drag_rect;
		break;
	default:
		error << string_compose (_("programming_error: %1"), "Error: unknown range marker op passed to Editor::drag_range_markerbar_op ()") << endmsg;
		return;
		break;
	}

	timepos_t const pf = adjusted_current_time (event);

	if (_operation == CreateSkipMarker || _operation == CreateRangeMarker || _operation == CreateTransportMarker || _operation == CreateCDMarker) {
		timepos_t grab (grab_time());
		_editor->snap_to (grab);

		if (pf < grab_time()) {
			start = pf;
			end = grab;
		} else {
			end = pf;
			start = grab;
		}

		/* first drag: Either add to the selection
		   or create a new selection.
		*/

		if (first_move) {

			_editor->temp_location->set (start, end);

			crect->show ();

			update_item (_editor->temp_location);
			_drag_rect->show();
			//_drag_rect->raise_to_top();

		}
	}

	if (start != end) {
		_editor->temp_location->set (start, end);

		double x1 = _editor->time_to_pixel (start);
		double x2 = _editor->time_to_pixel (end);
		crect->set_x0 (x1);
		crect->set_x1 (x2);

		update_item (_editor->temp_location);
	}

	show_verbose_cursor_time (pf);

}

void
RangeMarkerBarDrag::finished (GdkEvent* event, bool movement_occurred)
{
	Location * newloc = 0;
	string rangename;
	Location::Flags flags;

	if (movement_occurred) {
		motion (event, false);
		_drag_rect->hide();

		switch (_operation) {
		case CreateSkipMarker:
		case CreateRangeMarker:
		case CreateCDMarker:
		{
			XMLNode &before = _editor->session()->locations()->get_state();
			if (_operation == CreateSkipMarker) {
				_editor->begin_reversible_command (_("new skip marker"));
				_editor->session()->locations()->next_available_name(rangename,_("skip"));
				flags = Location::Flags (Location::IsRangeMarker | Location::IsSkip);
				_editor->range_bar_drag_rect->hide();
			} else if (_operation == CreateCDMarker) {
				_editor->session()->locations()->next_available_name(rangename, _("CD"));
				_editor->begin_reversible_command (_("new CD marker"));
				flags = Location::Flags (Location::IsRangeMarker | Location::IsCDMarker);
				_editor->cd_marker_bar_drag_rect->hide();
			} else {
				_editor->begin_reversible_command (_("new skip marker"));
				_editor->session()->locations()->next_available_name(rangename, _("unnamed"));
				flags = Location::IsRangeMarker;
				_editor->range_bar_drag_rect->hide();
			}

			newloc = new Location (*_editor->session(), _editor->temp_location->start(), _editor->temp_location->end(), rangename, flags);

			_editor->session()->locations()->add (newloc, true);
			XMLNode &after = _editor->session()->locations()->get_state();
			_editor->session()->add_command(new MementoCommand<Locations>(*(_editor->session()->locations()), &before, &after));
			_editor->commit_reversible_command ();
			break;
		}

		case CreateTransportMarker:
			// popup menu to pick loop or punch
			_editor->new_transport_marker_context_menu (&event->button, _item);
			break;
		}

	} else {

		/* just a click, no pointer movement. remember that context menu stuff was handled elsewhere */

		if (_operation == CreateTransportMarker) {

			/* didn't drag, so just locate */

			_editor->session()->request_locate (grab_sample());

		} else if (_operation == CreateCDMarker) {

			/* didn't drag, but mark is already created so do nothing */

		} else { /* operation == CreateRangeMarker || CreateSkipMarker */

			timepos_t start;
			timepos_t end;

			_editor->session()->locations()->marks_either_side (grab_time(), start, end);

			if (end == timepos_t::max (end.time_domain())) {
				end = _editor->session()->current_end ();
			}

			if (start == timepos_t::max (start.time_domain())) {
				start = _editor->session()->current_start ();
			}

			switch (_editor->mouse_mode) {
			case MouseObject:
				/* find the two markers on either side and then make the selection from it */
				_editor->select_all_within (start, end, 0.0f, FLT_MAX, _editor->track_views, Selection::Set, false);
				break;

			case MouseRange:
				/* find the two markers on either side of the click and make the range out of it */
				_editor->selection->set (start, end);
				break;

			default:
				break;
			}
		}
	}

	_editor->stop_canvas_autoscroll ();
}

void
RangeMarkerBarDrag::aborted (bool movement_occurred)
{
	if (movement_occurred) {
		_drag_rect->hide ();
	}
}

void
RangeMarkerBarDrag::update_item (Location* location)
{
	double const x1 = _editor->time_to_pixel (location->start());
	double const x2 = _editor->time_to_pixel (location->end());

	_drag_rect->set_x0 (x1);
	_drag_rect->set_x1 (x2);
}

NoteDrag::NoteDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i, Temporal::BeatTime)
	, _cumulative_dy (0)
	, _was_selected (false)
	, _copy (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New NoteDrag\n");

	_primary = reinterpret_cast<NoteBase*> (_item->get_data ("notebase"));
	assert (_primary);
	_region = &_primary->region_view ();
	_note_height = _region->midi_stream_view()->note_height ();
}

void
NoteDrag::setup_pointer_offset ()
{
	_pointer_offset = _region->region()->source_beats_to_absolute_time (_primary->note()->time()).distance (raw_grab_time());
}

void
NoteDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event);

	if (ArdourKeyboard::indicates_copy (event->button.state)) {
		_copy = true;
	} else {
		_copy = false;
	}

	setup_snap_delta (_region->region()->source_beats_to_absolute_time (_primary->note()->time ()));

	if (!(_was_selected = _primary->selected())) {

		/* tertiary-click means extend selection - we'll do that on button release,
		   so don't add it here, because otherwise we make it hard to figure
		   out the "extend-to" range.
		*/

		bool extend = Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier);

		if (!extend) {
			bool add = Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier);

			if (add) {
				_region->note_selected (_primary, true);
			} else {
				_editor->get_selection().clear_points();
				_region->unique_select (_primary);
			}
		}
	}
}

/** @return Current total drag x change in quarter notes */
Temporal::timecnt_t
NoteDrag::total_dx (GdkEvent * event) const
{
	if (_x_constrained) {
		return timecnt_t::zero (Temporal::BeatTime);
	}

	/* dx in as samples, because we can't do any better */
	timecnt_t const dx = timecnt_t (pixel_to_time (_drags->current_pointer_x() - grab_x()), timepos_t());

	/* primary note time in quarter notes */
	timepos_t const n_qn = _region->region()->source_beats_to_absolute_time (_primary->note()->time());

	/* new session relative time of the primary note (will be in beats)

	 * start from the note position, add the distance the drag has covered,
	 * and then the required (if any) snap distance
	 */
	timepos_t snap = n_qn + dx + snap_delta (event->button.state);

	/* possibly snap and return corresponding delta (will be in beats) */
	_editor->snap_to_with_modifier (snap, event);

	/* we are trying to return the delta on the x-axis (almost certain in
	 * beats), So now, having snapped etc., subtract the original note
	 * position and the snap delta, and we'll know the current dx.
	 */

	timecnt_t ret (snap.earlier (n_qn).earlier (snap_delta (event->button.state)), n_qn);

	/* prevent the earliest note being dragged earlier than the region's start position */
	if (_earliest + ret < _region->region()->start()) {
		ret -= (ret + _earliest) -  _region->region()->start();
	}

	return ret;
}

/** @return Current total drag y change in note number */
int8_t
NoteDrag::total_dy () const
{
	if (_y_constrained) {
		return 0;
	}

	double const y = _region->midi_view()->y_position ();
	/* new current note */
	uint8_t n = _region->y_to_note (current_pointer_y () - y);
	/* clamp */
	MidiStreamView* msv = _region->midi_stream_view ();
	n = max (msv->lowest_note(), n);
	n = min (msv->highest_note(), n);
	/* and work out delta */
	return n - _region->y_to_note (grab_y() - y);
}

void
NoteDrag::motion (GdkEvent * event, bool first_move)
{
	if (first_move) {
		_earliest = timepos_t (_region->earliest_in_selection());
		if (_copy) {
			/* make copies of all the selected notes */
			_primary = _region->copy_selection (_primary);
		}
	}

	/* Total change in x and y since the start of the drag */
	timecnt_t const dx_qn = total_dx (event);
	int8_t const dy = total_dy ();

	/* Now work out what we have to do to the note canvas items to set this new drag delta */
	timecnt_t const tdx = _x_constrained ? timecnt_t::zero (_cumulative_dx.time_domain()) : dx_qn - _cumulative_dx;
	double const tdy = _y_constrained ? 0 : -dy * _note_height - _cumulative_dy;

	if (!tdx.is_zero () || tdy) {
		_cumulative_dx = dx_qn;
		_cumulative_dy += tdy;

		int8_t note_delta = total_dy();

		if (!tdx.is_zero () || tdy) {
			if (_copy) {
				_region->move_copies (dx_qn, tdy, note_delta);
			} else {
				_region->move_selection (dx_qn, tdy, note_delta);
			}

			/* the new note value may be the same as the old one, but we
			 * don't know what that means because the selection may have
			 * involved more than one note and we might be doing something
			 * odd with them. so show the note value anyway, always.
			 */

			uint8_t new_note = min (max (_primary->note()->note() + note_delta, 0), 127);

			_region->show_verbose_cursor_for_new_note_value (_primary->note(), new_note);

			_editor->set_snapped_cursor_position( _region->region()->source_beats_to_absolute_time (_primary->note()->time()) );
		}
	}
}

void
NoteDrag::finished (GdkEvent* ev, bool moved)
{
	if (!moved) {
		/* no motion - select note */

		if (_editor->current_mouse_mode() == Editing::MouseContent ||
		    _editor->current_mouse_mode() == Editing::MouseDraw) {

			bool changed = false;

			if (_was_selected) {
				bool add = Keyboard::modifier_state_equals (ev->button.state, Keyboard::PrimaryModifier);
				if (add) {
					_region->note_deselected (_primary);
					changed = true;
				} else {
					_editor->get_selection().clear_points();
					_region->unique_select (_primary);
					changed = true;
				}
			} else {
				bool extend = Keyboard::modifier_state_equals (ev->button.state, Keyboard::TertiaryModifier);
				bool add = Keyboard::modifier_state_equals (ev->button.state, Keyboard::PrimaryModifier);

				if (!extend && !add && _region->selection_size() > 1) {
					_editor->get_selection().clear_points();
					_region->unique_select (_primary);
					changed = true;
				} else if (extend) {
					_region->note_selected (_primary, true, true);
					changed = true;
				} else {
					/* it was added during button press */
					changed = true;

				}
			}

			if (changed) {
				_editor->begin_reversible_selection_op(X_("Select Note Release"));
				_editor->commit_reversible_selection_op();
			}
		}
	} else {
		_region->note_dropped (_primary, total_dx (ev), total_dy(), _copy);
	}
}

void
NoteDrag::aborted (bool)
{
	/* XXX: TODO */
}

/** Make an AutomationRangeDrag for lines in an AutomationTimeAxisView */
AutomationRangeDrag::AutomationRangeDrag (Editor* editor, AutomationTimeAxisView* atv, list<TimelineRange> const & r)
	: Drag (editor, atv->base_item (), editor->default_time_domain()) /* XXX NUTEMPO FIX TIME DOMAIN */
	, _ranges (r)
	, _y_origin (atv->y_position())
	, _y_height (atv->effective_height()) // or atv->lines()->front()->height() ?!
	, _nothing_to_drag (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New AutomationRangeDrag\n");
	setup (atv->lines ());
}

/** Make an AutomationRangeDrag for region gain lines or MIDI controller regions */
AutomationRangeDrag::AutomationRangeDrag (Editor* editor, list<RegionView*> const & v, list<TimelineRange> const & r, double y_origin, double y_height)
	: Drag (editor, v.front()->get_canvas_group (), editor->default_time_domain()) /* XXX NUTEMPO FIX TIME DOMAIN */
	, _ranges (r)
	, _y_origin (y_origin)
	, _y_height (y_height)
	, _nothing_to_drag (false)
	, _integral (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New AutomationRangeDrag\n");

	list<boost::shared_ptr<AutomationLine> > lines;

	for (list<RegionView*>::const_iterator i = v.begin(); i != v.end(); ++i) {
		if (AudioRegionView* audio_view = dynamic_cast<AudioRegionView*>(*i)) {
			lines.push_back (audio_view->get_gain_line ());
		} else if (AutomationRegionView* automation_view = dynamic_cast<AutomationRegionView*>(*i)) {
			lines.push_back (automation_view->line ());
			_integral = true;
		} else {
			error << _("Automation range drag created for invalid region type") << endmsg;
		}
	}
	setup (lines);
}

/** @param lines AutomationLines to drag.
 *  @param offset Offset from the session start to the points in the AutomationLines.
 */
void
AutomationRangeDrag::setup (list<boost::shared_ptr<AutomationLine> > const & lines)
{
	/* find the lines that overlap the ranges being dragged */
	list<boost::shared_ptr<AutomationLine> >::const_iterator i = lines.begin ();
	while (i != lines.end ()) {
		list<boost::shared_ptr<AutomationLine> >::const_iterator j = i;
		++j;

		pair<timepos_t, timepos_t> r = (*i)->get_point_x_range ();

		//need a special detection for automation lanes (not region gain line)
		//TODO:  if we implement automation regions, this check can probably be removed
		AudioRegionGainLine *argl = dynamic_cast<AudioRegionGainLine*> ((*i).get());
		if (!argl) {
			//in automation lanes, the EFFECTIVE range should be considered 0->max_position (even if there is no line)
			r.first = Temporal::timepos_t ((*i)->the_list()->time_domain());
			r.second = Temporal::timepos_t::max ((*i)->the_list()->time_domain());
		}

		/* check this range against all the TimelineRanges that we are using */
		list<TimelineRange>::const_iterator k = _ranges.begin ();
		while (k != _ranges.end()) {
			if (k->coverage (r.first, r.second) != Temporal::OverlapNone) {
				break;
			}
			++k;
		}

		/* add it to our list if it overlaps at all */
		if (k != _ranges.end()) {
			Line n;
			n.line = *i;
			n.state = 0;
			n.range = r;
			_lines.push_back (n);
		}

		i = j;
	}

	/* Now ::lines contains the AutomationLines that somehow overlap our drag */
}

double
AutomationRangeDrag::y_fraction (double global_y) const
{
	return 1.0 - ((global_y - _y_origin) / _y_height);
}

double
AutomationRangeDrag::value (boost::shared_ptr<AutomationList> list, timepos_t const & x) const
{
	const double v = list->eval(x);
	return _integral ? rint(v) : v;
}

void
AutomationRangeDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	/* Get line states before we start changing things */
	for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
		i->state = &i->line->get_state ();
	}

	if (_ranges.empty()) {

		/* No selected time ranges: drag all points */
		for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
			uint32_t const N = i->line->npoints ();
			for (uint32_t j = 0; j < N; ++j) {
				i->points.push_back (i->line->nth (j));
			}
		}

	}

	if (_nothing_to_drag) {
		return;
	}
}

void
AutomationRangeDrag::motion (GdkEvent*, bool first_move)
{
	if (_nothing_to_drag && !first_move) {
		return;
	}

	if (first_move) {
		_editor->begin_reversible_command (_("automation range move"));

		if (!_ranges.empty()) {

			/* add guard points */
			for (list<TimelineRange>::const_iterator i = _ranges.begin(); i != _ranges.end(); ++i) {

				timepos_t const half = (i->start() + i->end()) / 2;

				for (list<Line>::iterator j = _lines.begin(); j != _lines.end(); ++j) {
					if (j->range.first > i->start() || j->range.second < i->start()) {
						continue;
					}

					boost::shared_ptr<AutomationList> the_list = j->line->the_list ();

					/* j is the line that this audio range starts in; fade into it;
					 * 64 samples length plucked out of thin air.
					 */

					timepos_t a = i->start() + timepos_t (64);
					if (a > half) {
						a = half;
					}

					/* convert from absolute time into time
					 * relative to the line origin
					*/

					timepos_t p (j->line->get_origin().distance (i->start()).beats());
					timepos_t q (j->line->get_origin().distance (a).beats());

					/* XXX arguably ControlList::editor_add() should do this */

					p.set_time_domain (the_list->time_domain());
					q.set_time_domain (the_list->time_domain());

					XMLNode &before = the_list->get_state();
					bool const add_p = the_list->editor_add (p, value (the_list, p), false);
					bool const add_q = the_list->editor_add (q, value (the_list, q), false);

					if (add_p || add_q) {
						_editor->session()->add_command (
							new MementoCommand<AutomationList>(*the_list.get (), &before, &the_list->get_state()));
					}
				}

				/* same thing for the end */
				for (list<Line>::iterator j = _lines.begin(); j != _lines.end(); ++j) {

					if (j->range.first > i->end() || j->range.second < i->end()) {
						continue;
					}

					boost::shared_ptr<AutomationList> the_list = j->line->the_list ();

					/* j is the line that this audio range starts in; fade out of it;
					 * 64 samples length plucked out of thin air.
					 */

					timepos_t b = i->end().earlier (timepos_t (64));
					if (b < half) {
						b = half;
					}

					timepos_t p (j->line->get_origin().distance (b));
					timepos_t q (j->line->get_origin().distance (i->end()));

					/* XXX arguably ControlList::editor_add() should do this */

					p.set_time_domain (the_list->time_domain());
					q.set_time_domain (the_list->time_domain());

					XMLNode &before = the_list->get_state();
					bool const add_p = the_list->editor_add (p, value (the_list, p), false);
					bool const add_q = the_list->editor_add (q, value (the_list, q), false);

					if (add_p || add_q) {
						_editor->session()->add_command (
							new MementoCommand<AutomationList>(*the_list.get (), &before, &the_list->get_state()));
					}
				}
			}

			_nothing_to_drag = true;

			/* Find all the points that should be dragged and put them in the relevant
			 * points lists in the Line structs.
			 */
			for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {

				uint32_t const N = i->line->npoints ();
				for (uint32_t j = 0; j < N; ++j) {

					/* here's a control point on this line */
					ControlPoint* p = i->line->nth (j);

					/* convert point time (which is relative to line
					 * origin) into absolute time
					 */

					timepos_t const w = i->line->get_origin() + (*p->model())->when;

					/* see if it's inside a range */
					list<TimelineRange>::const_iterator k = _ranges.begin ();
					while (k != _ranges.end() && (k->start() >= w || k->end() <= w)) {
						++k;
					}

					if (k != _ranges.end()) {
						/* dragging this point */
						_nothing_to_drag = false;
						i->points.push_back (p);
					}
				}
			}
		}

		for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
			i->line->start_drag_multiple (i->points, y_fraction (current_pointer_y()), i->state);
		}
	}

	for (list<Line>::iterator l = _lines.begin(); l != _lines.end(); ++l) {
		float const f = y_fraction (current_pointer_y());
		/* we are ignoring x position for this drag, so we can just pass in anything */
		pair<float, float> result;
		uint32_t ignored;
		result = l->line->drag_motion (0, f, true, false, ignored);
		show_verbose_cursor_text (l->line->get_verbose_cursor_relative_string (result.first, result.second));
	}
}

void
AutomationRangeDrag::finished (GdkEvent* event, bool motion_occurred)
{
	if (_nothing_to_drag || !motion_occurred) {
		return;
	}

	motion (event, false);
	for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
		i->line->end_drag (false, 0);
	}

	_editor->commit_reversible_command ();
}

void
AutomationRangeDrag::aborted (bool)
{
	for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
		i->line->reset ();
	}
}

DraggingView::DraggingView (RegionView* v, RegionDrag* parent, TimeAxisView* itav)
	: view (v)
	, anchored_fade_length (0)
	, initial_time_axis_view (itav)
{
	TimeAxisView* tav = &v->get_time_axis_view();
	if (tav) {
		time_axis_view = parent->find_time_axis_view (&v->get_time_axis_view ());
	} else {
		time_axis_view = -1;
	}
	layer = v->region()->layer ();
	initial_y = v->get_canvas_group()->position().y;
	initial_playlist = v->region()->playlist ();
	initial_position = v->region()->position ();
	initial_end = v->region()->position () + v->region()->length ();
}

PatchChangeDrag::PatchChangeDrag (Editor* e, PatchChange* i, MidiRegionView* r)
	: Drag (e, i->canvas_item (), Temporal::BeatTime)
	, _region_view (r)
	, _patch_change (i)
	, _cumulative_dx (0)
{
	DEBUG_TRACE (DEBUG::Drags, string_compose ("New PatchChangeDrag, patch @ %1, grab @ %2\n",
	                                           _region_view->region()->source_beats_to_absolute_time (_patch_change->patch()->time()),
						   grab_time()));
}

void
PatchChangeDrag::motion (GdkEvent* ev, bool)
{
	timepos_t f = adjusted_current_time (ev);
	boost::shared_ptr<Region> r = _region_view->region ();
	f = max (f, r->position ());
	f = min (f, r->nt_last ());

	timecnt_t const dxf = grab_time().distance (f); // permitted dx
	double const dxu = _editor->duration_to_pixels (dxf); // permitted fx in units
	_patch_change->move (ArdourCanvas::Duple (dxu - _cumulative_dx, 0));
	_cumulative_dx = dxu;
}

void
PatchChangeDrag::finished (GdkEvent* ev, bool movement_occurred)
{
	if (!movement_occurred) {
		if (was_double_click()) {
			_region_view->edit_patch_change (_patch_change);
		}
		return;
	}

	boost::shared_ptr<Region> r (_region_view->region ());
	timepos_t f = adjusted_current_time (ev);
	f = max (f, r->position ());
	f = min (f, r->nt_last ());

	_region_view->move_patch_change (*_patch_change, _region_view->region()->absolute_time_to_source_beats (f));
}

void
PatchChangeDrag::aborted (bool)
{
	_patch_change->move (ArdourCanvas::Duple (-_cumulative_dx, 0));
}

void
PatchChangeDrag::setup_pointer_offset ()
{
	boost::shared_ptr<Region> region = _region_view->region ();
	_pointer_offset = region->source_beats_to_absolute_time (_patch_change->patch()->time()).distance (raw_grab_time());
}

MidiRubberbandSelectDrag::MidiRubberbandSelectDrag (Editor* e, MidiRegionView* rv)
	: RubberbandSelectDrag (e, rv->get_canvas_group ())
	, _region_view (rv)
{

}

void
MidiRubberbandSelectDrag::select_things (int button_state, timepos_t const & x1, timepos_t const & x2, double y1, double y2, bool /*drag_in_progress*/)
{
	_region_view->update_drag_selection (
		x1, x2, y1, y2,
		Keyboard::modifier_state_contains (button_state, Keyboard::TertiaryModifier));
}

void
MidiRubberbandSelectDrag::deselect_things ()
{
	/* XXX */
}

MidiVerticalSelectDrag::MidiVerticalSelectDrag (Editor* e, MidiRegionView* rv)
	: RubberbandSelectDrag (e, rv->get_canvas_group ())
	, _region_view (rv)
{
	_vertical_only = true;
}

void
MidiVerticalSelectDrag::select_things (int button_state, timepos_t const & /*x1*/, timepos_t const & /*x2*/, double y1, double y2, bool /*drag_in_progress*/)
{
	double const y = _region_view->midi_view()->y_position ();

	y1 = max (0.0, y1 - y);
	y2 = max (0.0, y2 - y);

	_region_view->update_vertical_drag_selection (
		y1,
		y2,
		Keyboard::modifier_state_contains (button_state, Keyboard::TertiaryModifier)
		);
}

void
MidiVerticalSelectDrag::deselect_things ()
{
	/* XXX */
}

EditorRubberbandSelectDrag::EditorRubberbandSelectDrag (Editor* e, ArdourCanvas::Item* i)
	: RubberbandSelectDrag (e, i)
{

}

void
EditorRubberbandSelectDrag::select_things (int button_state, timepos_t const & x1, timepos_t const & x2, double y1, double y2, bool drag_in_progress)
{
	if (drag_in_progress) {
		/* We just want to select things at the end of the drag, not during it */
		return;
	}

	Selection::Operation op = ArdourKeyboard::selection_type (button_state);

	_editor->begin_reversible_selection_op (X_("rubberband selection"));

	_editor->select_all_within (x1, x2.decrement(), y1, y2, _editor->track_views, op, false);

	_editor->commit_reversible_selection_op ();
}

void
EditorRubberbandSelectDrag::deselect_things ()
{
	_editor->begin_reversible_selection_op (X_("Clear Selection (rubberband)"));

	_editor->selection->clear_tracks();
	_editor->selection->clear_regions();
	_editor->selection->clear_points ();
	_editor->selection->clear_lines ();
	_editor->selection->clear_midi_notes ();

	_editor->commit_reversible_selection_op();
}

NoteCreateDrag::NoteCreateDrag (Editor* e, ArdourCanvas::Item* i, MidiRegionView* rv)
	: Drag (e, i, Temporal::BeatTime)
	, _region_view (rv)
	, _drag_rect (0)
{
	_note[0] = _note[1] = timepos_t (Temporal::BeatTime);
}

NoteCreateDrag::~NoteCreateDrag ()
{
	delete _drag_rect;
}

Temporal::Beats
NoteCreateDrag::round_down_to_grid (timepos_t const & pos, GdkEvent const * event) const
{
	return _editor->snap_to_bbt (pos, RoundDownMaybe, SnapToGrid_Unscaled).beats ();
}

void
NoteCreateDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	_drag_rect = new ArdourCanvas::Rectangle (_region_view->get_canvas_group ());

	const timepos_t pos = _drags->current_pointer_time ();
	Temporal::Beats aligned_beats (round_down_to_grid (pos, event));
	const Temporal::Beats grid_beats (_region_view->get_draw_length_beats (pos));

	_note[0] = timepos_t (aligned_beats);
	/* minimum initial length is grid beats */
	_note[1] = _note[0] + timepos_t (grid_beats);

	/* the note positions we've just computed are in absolute beats, but
	 * the drag rect is a member of the region view group, so we need
	 * coordinates relative to the region in order to draw it correctly.
	 */

	const timepos_t rrp1 (_region_view->region()->region_relative_position (_note[0]));
	const timepos_t rrp2 (_region_view->region()->region_relative_position (_note[1]));

	double const x0 = _editor->time_to_pixel (rrp1);
	double const x1 = _editor->time_to_pixel (rrp2);
	double const y = _region_view->note_to_y (_region_view->y_to_note (y_to_region (event->button.y)));

	_drag_rect->set (ArdourCanvas::Rect (x0, y, x1, y + floor (_region_view->midi_stream_view()->note_height ())));
	_drag_rect->set_outline_all ();
	_drag_rect->set_outline_color (0xffffff99);
	_drag_rect->set_fill_color (0xffffff66);
}

void
NoteCreateDrag::motion (GdkEvent* event, bool)
{
	const timepos_t pos = _drags->current_pointer_time ();
	Temporal::Beats aligned_beats = round_down_to_grid (pos, event);

	_note[1] = timepos_t (max (Temporal::Beats(), aligned_beats));

	const timepos_t rrp1 (_region_view->region()->region_relative_position (_note[0]));
	const timepos_t rrp2 (_region_view->region()->region_relative_position (_note[1]));

	double const x0 = _editor->time_to_pixel (rrp1);
	double const x1 = _editor->time_to_pixel (rrp2);
	_drag_rect->set_x0 (std::min(x0, x1));
	_drag_rect->set_x1 (std::max(x0, x1));
}

void
NoteCreateDrag::finished (GdkEvent* ev, bool had_movement)
{
	/* we create a note even if there was no movement */

	/* Compute start within region, rather than absolute time start */

	Beats const start = _region_view->region()->absolute_time_to_region_beats (min (_note[0], _note[1]));
	Beats length = max (Beats (0, 1), (_note[0].distance (_note[1]).abs().beats()));

	GridType grid_to_use = _editor->draw_length() == DRAW_LEN_AUTO ? _editor->grid_type() : _editor->draw_length();
	int32_t div = _editor->get_grid_music_divisions (grid_to_use, ev->button.state);

	if (div > 0) {
		length = length.round_to_subdivision (div, RoundUpMaybe);
	}

#warning NUTEMPO ALERT not snapping correctly

	_editor->begin_reversible_command (_("Create Note"));
	_region_view->create_note_at (timepos_t (start), _drag_rect->y0(), length, ev->button.state, false);
	_editor->commit_reversible_command ();
}

double
NoteCreateDrag::y_to_region (double y) const
{
	double x = 0;
	_region_view->get_canvas_group()->canvas_to_item (x, y);
	return y;
}

void
NoteCreateDrag::aborted (bool)
{

}

HitCreateDrag::HitCreateDrag (Editor* e, ArdourCanvas::Item* i, MidiRegionView* rv)
	: Drag (e, i, Temporal::BeatTime)
	, _region_view (rv)
	, _last_pos (Temporal::Beats())
	, _y (0.0)
{
}

HitCreateDrag::~HitCreateDrag ()
{
}

void
HitCreateDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	_y = _region_view->note_to_y (_region_view->y_to_note (y_to_region (event->button.y)));

	const timepos_t pos = _drags->current_pointer_time ();

	const Beats beats = pos.beats ();

	boost::shared_ptr<MidiRegion> mr = _region_view->midi_region();

	if (beats >= mr->nt_last().beats()) {
		return;
	}

	const Temporal::Beats start = beats - _region_view->region()->position().beats ();
	Temporal::Beats length = _region_view->get_draw_length_beats (pos);

	_editor->begin_reversible_command (_("Create Hit"));
	_region_view->clear_note_selection();
	_region_view->create_note_at (timepos_t (start), _y, length, event->button.state, false);

	_last_pos = timepos_t (start);
}

void
HitCreateDrag::motion (GdkEvent* event, bool)
{
	const timepos_t pos = _drags->current_pointer_time ();

	GridType grid_to_use = _editor->draw_length() == DRAW_LEN_AUTO ? _editor->grid_type() : _editor->draw_length();
	int32_t divisions = _editor->get_grid_music_divisions (grid_to_use, event->button.state);
	if (divisions <= 0) {
		return;
	}

	const Beats beats = pos.beats ();
	const Temporal::Beats start = beats - _region_view->region()->position ().beats();

	if (_last_pos == start) {
		return;
	}

	Temporal::Beats length = _region_view->get_draw_length_beats (pos);

	boost::shared_ptr<MidiRegion> mr = _region_view->midi_region();

	if (beats > mr->nt_last().beats()) {
		return;
	}

#warning NUTEMPO ALERT not snapping correctly

	_region_view->create_note_at (timepos_t (start), _y, length, event->button.state, false);

	_last_pos = timepos_t (start);
}

void
HitCreateDrag::finished (GdkEvent* /* ev */, bool /* had_movement */)
{
	_editor->commit_reversible_command ();

}

double
HitCreateDrag::y_to_region (double y) const
{
	double x = 0;
	_region_view->get_canvas_group()->canvas_to_item (x, y);
	return y;
}

void
HitCreateDrag::aborted (bool)
{
	// umm..
}

CrossfadeEdgeDrag::CrossfadeEdgeDrag (Editor* e, AudioRegionView* rv, ArdourCanvas::Item* i, bool start_yn)
	: Drag (e, i, Temporal::AudioTime)
	, arv (rv)
	, start (start_yn)
{
	std::cout << ("CrossfadeEdgeDrag is DEPRECATED.  See TrimDrag::preserve_fade_anchor") << endl;
}

void
CrossfadeEdgeDrag::start_grab (GdkEvent* event, Gdk::Cursor *cursor)
{
	Drag::start_grab (event, cursor);
}

void
CrossfadeEdgeDrag::motion (GdkEvent*, bool)
{
	double distance;
	timecnt_t new_length;
	timecnt_t len;

	boost::shared_ptr<AudioRegion> ar (arv->audio_region());

	if (start) {
		distance = _drags->current_pointer_x() - grab_x();
		len = timecnt_t (ar->fade_in()->back()->when);
	} else {
		distance = grab_x() - _drags->current_pointer_x();
		len = timecnt_t (ar->fade_out()->back()->when);
	}

	/* how long should it be ? */

	new_length = len + timecnt_t (pixel_to_time (distance));

	/* now check with the region that this is legal */

	new_length = timecnt_t (ar->verify_xfade_bounds (new_length.samples(), start));

	if (start) {
		arv->reset_fade_in_shape_width (ar, new_length.samples());
	} else {
		arv->reset_fade_out_shape_width (ar, new_length.samples());
	}
}

void
CrossfadeEdgeDrag::finished (GdkEvent*, bool)
{
	double distance;
	timecnt_t new_length;
	timecnt_t len;

	boost::shared_ptr<AudioRegion> ar (arv->audio_region());

	if (start) {
		distance = _drags->current_pointer_x() - grab_x();
		len = timecnt_t (ar->fade_in()->back()->when);
	} else {
		distance = grab_x() - _drags->current_pointer_x();
		len = timecnt_t (ar->fade_out()->back()->when);
	}

	timecnt_t tdist = timecnt_t (pixel_to_time (distance));
	timecnt_t newlen = len + tdist;
	new_length = timecnt_t (ar->verify_xfade_bounds (newlen.samples(), start));

	_editor->begin_reversible_command ("xfade trim");
	ar->playlist()->clear_owned_changes ();

	if (start) {
		ar->set_fade_in_length (new_length.samples());
	} else {
		ar->set_fade_out_length (new_length.samples());
	}

	/* Adjusting the xfade may affect other regions in the playlist, so we need
	   to get undo Commands from the whole playlist rather than just the
	   region.
	*/

	vector<Command*> cmds;
	ar->playlist()->rdiff (cmds);
	_editor->session()->add_commands (cmds);
	_editor->commit_reversible_command ();

}

void
CrossfadeEdgeDrag::aborted (bool)
{
	if (start) {
		// arv->redraw_start_xfade ();
	} else {
		// arv->redraw_end_xfade ();
	}
}

RegionCutDrag::RegionCutDrag (Editor* e, ArdourCanvas::Item* item, samplepos_t pos)
	: Drag (e, item, e->default_time_domain(), true)
{
}

RegionCutDrag::~RegionCutDrag ()
{
}

void
RegionCutDrag::start_grab (GdkEvent* event, Gdk::Cursor* c)
{
	Drag::start_grab (event, c);
	motion (event, false);
}

void
RegionCutDrag::motion (GdkEvent* event, bool)
{
}

void
RegionCutDrag::finished (GdkEvent* event, bool)
{
	_editor->get_track_canvas()->canvas()->re_enter();


	timepos_t pos (_drags->current_pointer_time());
	_editor->snap_to_with_modifier (pos, event);

	RegionSelection rs = _editor->get_regions_from_selection_and_mouse (pos);

	if (rs.empty()) {
		return;
	}

	_editor->split_regions_at (pos, rs);
}

void
RegionCutDrag::aborted (bool)
{
}

RegionMarkerDrag::RegionMarkerDrag (Editor* ed, RegionView* r, ArdourCanvas::Item* i)
	: Drag (ed, i, r->region()->position().time_domain())
	, rv (r)
	, view (static_cast<ArdourMarker*> (i->get_data ("marker")))
	, model (rv->find_model_cue_marker (view))
	, dragging_model (model)
{
	assert (view);
}

RegionMarkerDrag::~RegionMarkerDrag ()
{
}

void
RegionMarkerDrag::start_grab (GdkEvent* ev, Gdk::Cursor* c)
{
	Drag::start_grab (ev, c);
	show_verbose_cursor_time (model.position());
	setup_snap_delta (model.position());
}

void
RegionMarkerDrag::motion (GdkEvent* ev, bool first_move)
{
	timepos_t pos = adjusted_current_time (ev);

	if (pos < rv->region()->position() || pos >= (rv->region()->position() + rv->region()->length())) {
		/* out of bounds */
		return;
	}

	timepos_t newpos (rv->region()->position().distance (pos));
	dragging_model.set_position (newpos);
	/* view (ArdourMarker) needs a relative position inside the RegionView */
	view->set_position (newpos);
	show_verbose_cursor_time (newpos);
}

void
RegionMarkerDrag::finished (GdkEvent *, bool did_move)
{
	if (did_move) {
		rv->region()->move_cue_marker (model, dragging_model.position());
	} else if (was_double_click()) {
		/* edit name */

		ArdourDialog d (_("Edit Cue Marker Name"), true, false);
		Gtk::Entry e;
		d.get_vbox()->pack_start (e);
		e.set_text (model.text());
		e.select_region (0, -1);
		e.show ();
		e.set_activates_default ();

		d.add_button (Stock::CANCEL, RESPONSE_CANCEL);
		d.add_button (Stock::OK, RESPONSE_OK);
		d.set_default_response (RESPONSE_OK);
		d.set_position (WIN_POS_MOUSE);

		int result = d.run();
		string str = e.get_text();

		if (result == RESPONSE_OK && !str.empty()) {
			/* explicitly remove the existing (GUI) marker, because
			   we will not find a match when handing the
			   CueMarkersChanged signal.
			*/
			rv->drop_cue_marker (view);
			rv->region()->rename_cue_marker (model, str);
		}
	}
}

void
RegionMarkerDrag::aborted (bool)
{
	view->set_position (model.position());
}

void
RegionMarkerDrag::setup_pointer_sample_offset ()
{
	const timepos_t model_abs_pos = rv->region()->position() + (rv->region()->start().distance (model.position()));
	_pointer_offset = model_abs_pos.distance (raw_grab_time());
}
