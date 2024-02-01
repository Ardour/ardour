/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/midi_region.h"
#include "ardour/midi_source.h"

#include "canvas/canvas.h"
#include "canvas/container.h"
#include "canvas/debug.h"
#include "canvas/scroll_group.h"
#include "canvas/rectangle.h"

#include "gtkmm2ext/actions.h"

#include "ardour_ui.h"
#include "editor_cursors.h"
#include "editor_drag.h"
#include "keyboard.h"
#include "midi_cue_background.h"
#include "midi_cue_editor.h"
#include "midi_cue_view.h"
#include "note_base.h"
#include "ui_config.h"
#include "verbose_cursor.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace Gtkmm2ext;
using namespace Temporal;

MidiCueEditor::MidiCueEditor()
	: vertical_adjustment (0.0, 0.0, 10.0, 400.0)
	, horizontal_adjustment (0.0, 0.0, 1e16)
	, view (nullptr)
	, mouse_mode (Editing::MouseDraw)
{
	build_canvas ();

	_verbose_cursor = new VerboseCursor (*this);

	// _playhead_cursor = new EditorCursor (*this, &Editor::canvas_playhead_cursor_event, X_("playhead"));
	_playhead_cursor = new EditorCursor (*this, X_("playhead"));
	_playhead_cursor->set_sensitive (UIConfiguration::instance().get_sensitize_playhead());

	_snapped_cursor = new EditorCursor (*this, X_("snapped"));
}

MidiCueEditor::~MidiCueEditor ()
{
}

void
MidiCueEditor::build_canvas ()
{
	_canvas_viewport = new ArdourCanvas::GtkCanvasViewport (horizontal_adjustment, vertical_adjustment);

	_canvas = _canvas_viewport->canvas ();
	_canvas->set_background_color (UIConfiguration::instance().color ("arrange base"));
	dynamic_cast<ArdourCanvas::GtkCanvas*>(_canvas)->use_nsglview (UIConfiguration::instance().get_nsgl_view_mode () == NSGLHiRes);

	/* scroll group for items that should not automatically scroll
	 *  (e.g verbose cursor). It shares the canvas coordinate space.
	*/
	no_scroll_group = new ArdourCanvas::Container (_canvas->root());

	ArdourCanvas::ScrollGroup* hsg;
	ArdourCanvas::ScrollGroup* hg;
	ArdourCanvas::ScrollGroup* cg;

	h_scroll_group = hg = new ArdourCanvas::ScrollGroup (_canvas->root(), ArdourCanvas::ScrollGroup::ScrollsHorizontally);
	CANVAS_DEBUG_NAME (h_scroll_group, "canvas h scroll");
	_canvas->add_scroller (*hg);

	hv_scroll_group = hsg = new ArdourCanvas::ScrollGroup (_canvas->root(),
							       ArdourCanvas::ScrollGroup::ScrollSensitivity (ArdourCanvas::ScrollGroup::ScrollsVertically|
													     ArdourCanvas::ScrollGroup::ScrollsHorizontally));
	CANVAS_DEBUG_NAME (hv_scroll_group, "cue canvas hv scroll");
	_canvas->add_scroller (*hsg);

	cursor_scroll_group = cg = new ArdourCanvas::ScrollGroup (_canvas->root(), ArdourCanvas::ScrollGroup::ScrollsHorizontally);
	CANVAS_DEBUG_NAME (cursor_scroll_group, "cue canvas cursor scroll");
	_canvas->add_scroller (*cg);

	/*a group to hold global rects like punch/loop indicators */
	global_rect_group = new ArdourCanvas::Container (hv_scroll_group);
	CANVAS_DEBUG_NAME (global_rect_group, "cue global rect group");

        transport_loop_range_rect = new ArdourCanvas::Rectangle (global_rect_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, ArdourCanvas::COORD_MAX));
	CANVAS_DEBUG_NAME (transport_loop_range_rect, "cue loop rect");
	transport_loop_range_rect->hide();

	/*a group to hold time (measure) lines */
	time_line_group = new ArdourCanvas::Container (h_scroll_group);
	CANVAS_DEBUG_NAME (time_line_group, "cue  time line group");

	// used as rubberband rect
	rubberband_rect = new ArdourCanvas::Rectangle (hv_scroll_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, 0.0));
	rubberband_rect->hide();
	rubberband_rect->set_outline_color (UIConfiguration::instance().color ("rubber band rect"));
	rubberband_rect->set_fill_color (UIConfiguration::instance().color_mod ("rubber band rect", "selection rect"));
	CANVAS_DEBUG_NAME (rubberband_rect, X_("cue rubberband rect"));

	bg = new CueMidiBackground (hv_scroll_group);
	_canvas_viewport->signal_size_allocate().connect (sigc::mem_fun(*this, &MidiCueEditor::canvas_allocate));

	_canvas->set_name ("MidiCueCanvas");
	_canvas->add_events (Gdk::POINTER_MOTION_HINT_MASK | Gdk::SCROLL_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	_canvas->signal_enter_notify_event().connect (sigc::mem_fun(*this, &MidiCueEditor::canvas_enter_leave), false);
	_canvas->signal_leave_notify_event().connect (sigc::mem_fun(*this, &MidiCueEditor::canvas_enter_leave), false);
	_canvas->set_can_focus ();

	Bindings* midi_bindings = Bindings::get_bindings (X_("MIDI"));
	_canvas->set_data (X_("ardour-bindings"), midi_bindings);
}

bool
MidiCueEditor::canvas_enter_leave (GdkEventCrossing* ev)
{
	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
		if (ev->detail != GDK_NOTIFY_INFERIOR) {
			_canvas_viewport->canvas()->grab_focus ();
			ActionManager::set_sensitive (_midi_actions, true);
			EditingContext::push_editing_context (this);
			std::cerr << "GRAB FOCUS\n";
		}
		break;
	case GDK_LEAVE_NOTIFY:
		if (ev->detail != GDK_NOTIFY_INFERIOR) {
			ActionManager::set_sensitive (_midi_actions, false);
			ARDOUR_UI::instance()->reset_focus (_canvas_viewport);
			EditingContext::pop_editing_context ();
			std::cerr << "DROP FOCUS\n";
		}
	default:
		break;
	}
	return false;
}

void
MidiCueEditor::canvas_allocate (Gtk::Allocation alloc)
{
	bg->set_size (alloc.get_width(), alloc.get_height());
	if (view) {
		view->set_size (alloc.get_width(), alloc.get_height());
	}
}

timepos_t
MidiCueEditor::snap_to_grid (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref) const
{
	/* BBT time only */
	return snap_to_bbt (presnap, direction, gpref);
}

void
MidiCueEditor::snap_to_internal (timepos_t& start, Temporal::RoundMode direction, SnapPref pref, bool ensure_snap) const
{
	UIConfiguration const& uic (UIConfiguration::instance ());
	const timepos_t presnap = start;


	timepos_t dist = timepos_t::max (start.time_domain()); // this records the distance of the best snap result we've found so far
	timepos_t best = timepos_t::max (start.time_domain()); // this records the best snap-result we've found so far

	timepos_t pre (presnap);
	timepos_t post (snap_to_grid (pre, direction, pref));

	check_best_snap (presnap, post, dist, best);

	if (timepos_t::max (start.time_domain()) == best) {
		return;
	}

	/* now check "magnetic" state: is the grid within reasonable on-screen distance to trigger a snap?
	 * this also helps to avoid snapping to somewhere the user can't see.  (i.e.: I clicked on a region and it disappeared!!)
	 * ToDo: Perhaps this should only occur if EditPointMouse?
	 */
	samplecnt_t snap_threshold_s = pixel_to_sample (uic.get_snap_threshold ());

	if (!ensure_snap && ::llabs (best.distance (presnap).samples()) > snap_threshold_s) {
		return;
	}

	start = best;
}

void
MidiCueEditor::reset_zoom (samplecnt_t spp)
{
	CueEditor::reset_zoom (spp);

	if (view) {
		view->set_samples_per_pixel (spp);
	}
}

samplecnt_t
MidiCueEditor::current_page_samples() const
{
	return (samplecnt_t) _visible_canvas_width* samples_per_pixel;
}

void
MidiCueEditor::apply_midi_note_edit_op (ARDOUR::MidiOperator& op, const RegionSelection& rs)
{
}

PBD::Command*
MidiCueEditor::apply_midi_note_edit_op_to_region (ARDOUR::MidiOperator& op, MidiRegionView& mrv)
{
#if 0
	Evoral::Sequence<Temporal::Beats>::Notes selected;
	mrv.selection_as_notelist (selected, true);

	if (selected.empty()) {
		return 0;
	}

	vector<Evoral::Sequence<Temporal::Beats>::Notes> v;
	v.push_back (selected);

	timepos_t pos = mrv.midi_region()->source_position();

	return op (mrv.midi_region()->model(), pos.beats(), v);
#endif

	return nullptr;
}

bool
MidiCueEditor::canvas_note_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, NoteItem);
}

Gtk::Widget&
MidiCueEditor::viewport()
{
	return *_canvas_viewport;
}

void
MidiCueEditor::set_region (std::shared_ptr<ARDOUR::MidiTrack> t, std::shared_ptr<ARDOUR::MidiRegion> r)
{
	delete view;
	view = nullptr;

	if (!t || !r) {
		return;
	}

	view = new MidiCueView (t, *hv_scroll_group, *this, *bg, 0xff0000ff);
	view->set_region (r);

	bg->set_view (view);

	/* Compute zoom level to show entire source plus some margin if possible */

	std::shared_ptr<Temporal::TempoMap> map;

	Temporal::timecnt_t duration = Temporal::timecnt_t (r->midi_source()->length().beats());
	/* XXX build tempo map from source */
	map.reset (new Temporal::TempoMap (Temporal::Tempo (120, 4), Temporal::Meter (4, 4)));

	{
		EditingContext::TempoMapScope tms (*this, map);
		double width = bg->width();
		samplecnt_t samples = duration.samples();

		samplecnt_t spp = floor (samples / width);
		reset_zoom (spp);
	}
}

bool
MidiCueEditor::button_press_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	switch (event->button.button) {
	case 1:
		return button_press_handler_1 (item, event, item_type);
		break;

	case 2:
		return button_press_handler_2 (item, event, item_type);
		break;

	case 3:
		break;

	default:
		return button_press_dispatch (&event->button);
		break;

	}

	return false;
}

bool
MidiCueEditor::button_press_handler_1 (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	NoteBase* note = nullptr;

	if (mouse_mode == Editing::MouseContent) {
		switch (item_type) {
		case NoteItem:
			/* Existing note: allow trimming/motion */
			if ((note = reinterpret_cast<NoteBase*> (item->get_data ("notebase")))) {
				if (note->big_enough_to_trim() && note->mouse_near_ends()) {
					_drags->set (new NoteResizeDrag (*this, item), event, get_canvas_cursor());
				} else {
					_drags->set (new NoteDrag (*this, item), event);
				}
			}
			return true;
		default:
			break;
		}
	}

	return false;
}

bool
MidiCueEditor::button_press_handler_2 (ArdourCanvas::Item*, GdkEvent*, ItemType)
{
	return true;
}

bool
MidiCueEditor::button_release_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	std::cerr << "hey\n";

	if (Keyboard::is_context_menu_event (&event->button)) {
		switch (item_type) {
		case NoteItem:
			if (internal_editing()) {
				popup_note_context_menu (item, event);
				return true;
			}
			break;
		default:
			break;
		}
	}

	return false;
}

bool
MidiCueEditor::button_press_dispatch (GdkEventButton* ev)
{
	/* this function is intended only for buttons 4 and above. */

	Gtkmm2ext::MouseButton b (ev->state, ev->button);
	return button_bindings->activate (b, Gtkmm2ext::Bindings::Press);
}

bool
MidiCueEditor::button_release_dispatch (GdkEventButton* ev)
{
	/* this function is intended only for buttons 4 and above. */

	Gtkmm2ext::MouseButton b (ev->state, ev->button);
	return button_bindings->activate (b, Gtkmm2ext::Bindings::Release);
}

bool
MidiCueEditor::motion_handler (ArdourCanvas::Item*, GdkEvent*, bool from_autoscroll)
{
	return true;
}

bool
MidiCueEditor::enter_handler (ArdourCanvas::Item*, GdkEvent*, ItemType)
{
	return true;
}

bool
MidiCueEditor::leave_handler (ArdourCanvas::Item*, GdkEvent*, ItemType)
{
	return true;
}

bool
MidiCueEditor::key_press_handler (ArdourCanvas::Item*, GdkEvent* ev, ItemType)
{

	switch (ev->key.keyval) {
	case GDK_d:
		set_mouse_mode (Editing::MouseDraw);
		std::cerr << "draw\n";
		break;
	case GDK_e:
		set_mouse_mode (Editing::MouseContent);
		std::cerr << "content/edit\n";
		break;
	}

	return true;
}

bool
MidiCueEditor::key_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType)
{
	return true;
}

void
MidiCueEditor::set_mouse_mode (Editing::MouseMode m, bool force)
{
	if (m != Editing::MouseDraw && m != Editing::MouseContent) {
		return;
	}

	mouse_mode = m;
}

void
MidiCueEditor::step_mouse_mode (bool next)
{
}

Editing::MouseMode
MidiCueEditor::current_mouse_mode () const
{
	return mouse_mode;
}

bool
MidiCueEditor::internal_editing() const
{
	return true;
}

RegionSelection
MidiCueEditor::region_selection()
{
	RegionSelection rs;
	return rs;
}
