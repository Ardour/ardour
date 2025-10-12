/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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

#include "pbd/compose.h"
#include <algorithm>

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "canvas/canvas.h"
#include "canvas/debug.h"
#include "canvas/utils.h"

#include "waveview/wave_view.h"

#include "ardour/audioengine.h"
#include "ardour/audioregion.h"
#include "ardour/location.h"
#include "ardour/profile.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/source.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_icon.h"

#include "ardour_ui.h"
#include "audio_clip_editor.h"
#include "audio_clock.h"
#include "boundary.h"
#include "editor_automation_line.h"
#include "editor_cursors.h"
#include "editor_drag.h"
#include "control_point.h"
#include "editor.h"
#include "keyboard.h"
#include "region_view.h"
#include "verbose_cursor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace ArdourWaveView;
using namespace ArdourWidgets;
using std::max;
using std::min;

void
AudioClipEditor::ClipMetric::get_marks (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, int maxchars) const
{
	ace.metric_get_minsec (marks, lower, upper, maxchars);
}

AudioClipEditor::AudioClipEditor (std::string const & name, bool with_transport)
	: CueEditor (name, with_transport)
	, clip_metric (nullptr)
	, scroll_fraction (0)
{
	load_bindings ();
	register_actions ();

	build_grid_type_menu ();
	build_upper_toolbar ();
	build_canvas ();
	build_lower_toolbar ();

	set_action_defaults ();
}


AudioClipEditor::~AudioClipEditor ()
{
	EC_LOCAL_TEMPO_SCOPE;

	drop_grid ();
	drop_waves ();
	delete clip_metric;
}

void
AudioClipEditor::set_action_defaults ()
{
	EC_LOCAL_TEMPO_SCOPE;

	CueEditor::set_action_defaults ();

	if (grid_actions[Editing::GridTypeMinSec]) {
		grid_actions[Editing::GridTypeMinSec]->set_active (false);
		grid_actions[Editing::GridTypeMinSec]->set_active (true);
	}
}

void
AudioClipEditor::load_shared_bindings ()
{
	EC_LOCAL_TEMPO_SCOPE;

	/* Full shared binding loading must have preceded this in some other EditingContext */
	assert (!need_shared_actions);

	Bindings* b = Bindings::get_bindings (X_("Editing"));

	/* Copy each  shared bindings but give them a new name, which will make them refer to actions
	 * named after this EditingContext (ie. unique to this EC)
	 */

	Bindings* shared_bindings = new Bindings (editor_name(), *b);
	register_common_actions (shared_bindings, editor_name());
	shared_bindings->associate ();

	/* Attach bindings to the canvas for this editing context */

	bindings.push_back (shared_bindings);
}

void
AudioClipEditor::pack_inner (Gtk::Box& box)
{
	EC_LOCAL_TEMPO_SCOPE;

	/* No snap, no grid selections until elastic audio */
	// box.pack_start (snap_box, false, false);
	// box.pack_start (grid_box, false, false);
}

void
AudioClipEditor::pack_outer (Gtk::Box& box)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (with_transport_controls) {
		box.pack_start (play_box, false, false);
	}

	box.pack_start (rec_box, false, false);
	box.pack_start (follow_playhead_button, false, false);
}

void
AudioClipEditor::build_lower_toolbar ()
{
	EC_LOCAL_TEMPO_SCOPE;

	_toolbox.pack_start (*_canvas_hscrollbar, false, false);
}

void
AudioClipEditor::build_canvas ()
{
	EC_LOCAL_TEMPO_SCOPE;

	_canvas.set_background_color (UIConfiguration::instance().color ("arrange base"));
	_canvas.signal_event().connect (sigc::mem_fun (*this, &CueEditor::canvas_pre_event), false);
	_canvas.use_nsglview (UIConfiguration::instance().get_nsgl_view_mode () == NSGLHiRes);

	_canvas.PreRender.connect (sigc::mem_fun(*this, &EditingContext::pre_render));

	/* scroll group for items that should not automatically scroll
	 *  (e.g verbose cursor). It shares the canvas coordinate space.
	*/
	no_scroll_group = new ArdourCanvas::Container (_canvas.root());

	h_scroll_group = new ArdourCanvas::ScrollGroup (_canvas.root(), ArdourCanvas::ScrollGroup::ScrollsHorizontally);
	CANVAS_DEBUG_NAME (h_scroll_group, "audioclip h scroll");
	_canvas.add_scroller (*h_scroll_group);


	v_scroll_group = new ArdourCanvas::ScrollGroup (_canvas.root(), ArdourCanvas::ScrollGroup::ScrollsVertically);
	CANVAS_DEBUG_NAME (v_scroll_group, "audioclip v scroll");
	_canvas.add_scroller (*v_scroll_group);

	hv_scroll_group = new ArdourCanvas::ScrollGroup (_canvas.root(),
	                                                 ArdourCanvas::ScrollGroup::ScrollSensitivity (ArdourCanvas::ScrollGroup::ScrollsVertically|
		                ArdourCanvas::ScrollGroup::ScrollsHorizontally));
	CANVAS_DEBUG_NAME (hv_scroll_group, "audioclip hv scroll");
	_canvas.add_scroller (*hv_scroll_group);

	cursor_scroll_group = new ArdourCanvas::ScrollGroup (_canvas.root(), ArdourCanvas::ScrollGroup::ScrollsHorizontally);
	CANVAS_DEBUG_NAME (cursor_scroll_group, "audioclip cursor scroll");
	_canvas.add_scroller (*cursor_scroll_group);

	/*a group to hold global rects like punch/loop indicators */
	global_rect_group = new ArdourCanvas::Container (hv_scroll_group);
	CANVAS_DEBUG_NAME (global_rect_group, "audioclip global rect group");

        transport_loop_range_rect = new ArdourCanvas::Rectangle (global_rect_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, ArdourCanvas::COORD_MAX));
	CANVAS_DEBUG_NAME (transport_loop_range_rect, "audioclip loop rect");
	transport_loop_range_rect->hide();

	/*a group to hold time (measure) lines */
	time_line_group = new ArdourCanvas::Container (h_scroll_group);
	CANVAS_DEBUG_NAME (time_line_group, "audioclip time line group");

	n_timebars = 0;

	clip_metric = new ClipMetric (*this);
	main_ruler = new ArdourCanvas::Ruler (time_line_group, clip_metric, ArdourCanvas::Rect (0, 0, ArdourCanvas::COORD_MAX, timebar_height));
	main_ruler->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
	main_ruler->set_fill_color (UIConfiguration::instance().color (X_("ruler base")));
	main_ruler->set_outline_color (UIConfiguration::instance().color (X_("ruler text")));
	n_timebars++;

	main_ruler->Event.connect (sigc::mem_fun (*this, &CueEditor::ruler_event));

	data_group = new ArdourCanvas::Container (hv_scroll_group);
	CANVAS_DEBUG_NAME (data_group, "cue data group");

	data_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebar_height * n_timebars));
	no_scroll_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebar_height * n_timebars));
	cursor_scroll_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebar_height * n_timebars));
	h_scroll_group->set_position (Duple (_timeline_origin, 0.));

	// _playhead_cursor = new EditorCursor (*this, &Editor::canvas_playhead_cursor_event, X_("playhead"));
	_playhead_cursor = new EditorCursor (*this, X_("playhead"));
	_playhead_cursor->set_sensitive (UIConfiguration::instance().get_sensitize_playhead());
	_playhead_cursor->set_color (UIConfiguration::instance().color ("play head"));
	_playhead_cursor->canvas_item().raise_to_top();
	h_scroll_group->raise_to_top ();

	_canvas.set_name ("AudioClipCanvas");
	_canvas.add_events (Gdk::POINTER_MOTION_HINT_MASK | Gdk::SCROLL_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	_canvas.set_can_focus ();
	_canvas_viewport.signal_size_allocate().connect (sigc::mem_fun(*this, &AudioClipEditor::canvas_allocate), false);
	_canvas_viewport.set_size_request (1, 1);

	_toolbox.pack_start (_canvas_viewport, true, true);

	/* the lines */

	line_container = new ArdourCanvas::Container (data_group);
	CANVAS_DEBUG_NAME (line_container, "audio clip line container");

	start_line = new StartBoundaryRect (line_container);
	start_line->set_outline_what (ArdourCanvas::Rectangle::RIGHT);
	CANVAS_DEBUG_NAME (start_line, "start boundary rect");

	end_line = new EndBoundaryRect (line_container);
	end_line->set_outline_what (ArdourCanvas::Rectangle::LEFT);
	CANVAS_DEBUG_NAME (end_line, "end boundary rect");

	// loop_line = new Line (line_container);
	// loop_line->set_outline_width (line_width * scale);

	start_line->Event.connect (sigc::bind (sigc::mem_fun (*this, &AudioClipEditor::start_line_event_handler), start_line));
	end_line->Event.connect (sigc::bind (sigc::mem_fun (*this, &AudioClipEditor::end_line_event_handler), end_line));
	// loop_line->Event.connect (sigc::bind (sigc::mem_fun (*this, &AudioClipEditor::line_event_handler), loop_line));

	/* hide lines until there is a region */

	// line_container->hide ();

	_verbose_cursor.reset (new VerboseCursor (*this));

	set_colors ();
}
bool
AudioClipEditor::button_press_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (event->type != GDK_BUTTON_PRESS) {
		return false;
	}

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
AudioClipEditor::button_press_handler_1 (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	EC_LOCAL_TEMPO_SCOPE;

	switch (item_type) {
	case ClipStartItem: {
		ArdourCanvas::Rectangle* r = dynamic_cast<ArdourCanvas::Rectangle*> (item);
		if (r) {
			_drags->set (new ClipStartDrag (*this, *r), event);
		}
		return true;
		break;
	}

	case ClipEndItem: {
		ArdourCanvas::Rectangle* r = dynamic_cast<ArdourCanvas::Rectangle*> (item);
		if (r) {
			_drags->set (new ClipEndDrag (*this, *r), event);
		}
		return true;
		break;
	}

	default:
		break;
	}

	return false;
}

bool
AudioClipEditor::button_press_handler_2 (ArdourCanvas::Item*, GdkEvent*, ItemType)
{
	EC_LOCAL_TEMPO_SCOPE;

	return true;
}

bool
AudioClipEditor::button_release_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!Keyboard::is_context_menu_event (&event->button)) {

		/* see if we're finishing a drag */

		if (_drags->active ()) {
			bool const r = _drags->end_grab (event);
			if (r) {
				/* grab dragged, so do nothing else */
				return true;
			}
		}
	}

	return false;
}

bool
AudioClipEditor::motion_handler (ArdourCanvas::Item*, GdkEvent* event, bool from_autoscroll)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_drags->active ()) {
		//drags change the snapped_cursor location, because we are snapping the thing being dragged, not the actual mouse cursor
		return _drags->motion_handler (event, from_autoscroll);
	}

	return true;
}

bool
AudioClipEditor::enter_handler (ArdourCanvas::Item* item, GdkEvent* ev, ItemType item_type)
{
	EC_LOCAL_TEMPO_SCOPE;

	choose_canvas_cursor_on_entry (item_type);

	return true;
}

bool
AudioClipEditor::leave_handler (ArdourCanvas::Item* item, GdkEvent* ev, ItemType item_type)
{
	EC_LOCAL_TEMPO_SCOPE;

	set_canvas_cursor (which_mode_cursor());

	return true;
}

bool
AudioClipEditor::start_line_event_handler (GdkEvent* ev, StartBoundaryRect* l)
{
	EC_LOCAL_TEMPO_SCOPE;

	return typed_event (start_line, ev, ClipStartItem);
}

bool
AudioClipEditor::end_line_event_handler (GdkEvent* ev, EndBoundaryRect* l)
{
	EC_LOCAL_TEMPO_SCOPE;

	return typed_event (end_line, ev, ClipEndItem);
}

bool
AudioClipEditor::key_press (GdkEventKey* ev)
{
	EC_LOCAL_TEMPO_SCOPE;

	return false;
}

void
AudioClipEditor::position_lines ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!_region) {
		return;
	}

	double width = sample_to_pixel (_region->start().samples());
	start_line->set (ArdourCanvas::Rect (0., 0., width, _visible_canvas_height));

	double offset = sample_to_pixel ((_region->start() + _region->length()).samples());
	end_line->set_position (ArdourCanvas::Duple (offset, 0.));
	end_line->set (ArdourCanvas::Rect (0., 0., ArdourCanvas::COORD_MAX, _visible_canvas_height));
}

void
AudioClipEditor::set_colors ()
{
	EC_LOCAL_TEMPO_SCOPE;

	_canvas.set_background_color (UIConfiguration::instance ().color (X_("theme:bg")));

	start_line->set_fill_color (UIConfiguration::instance().color_mod ("cue editor start rect fill", "cue boundary alpha"));
	start_line->set_outline_color (UIConfiguration::instance().color ("cue editor start rect outline"));

	end_line->set_fill_color (UIConfiguration::instance().color_mod ("cue editor end rect fill", "cue boundary alpha"));
	end_line->set_outline_color (UIConfiguration::instance().color ("cue editor end rect outline"));

	// loop_line->set_outline_color (UIConfiguration::instance ().color (X_("theme:contrasting selection")));

	set_waveform_colors ();
}

void
AudioClipEditor::drop_waves ()
{
	EC_LOCAL_TEMPO_SCOPE;

	for (auto& wave : waves) {
		delete wave;
	}

	waves.clear ();
}

void
AudioClipEditor::set_trigger (TriggerReference& tr)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (tr == ref) {
		return;
	}

	CueEditor::set_trigger (tr);
	rec_box.show ();

	main_ruler->show ();
	main_ruler->set_range (0, pixel_to_sample (_visible_canvas_width - 2.));
}

void
AudioClipEditor::set_region (std::shared_ptr<Region> region)
{
	EC_LOCAL_TEMPO_SCOPE;

	CueEditor::set_region (region);

	if (_visible_pending_region) {
		return;
	}

	drop_waves ();

	if (!region) {
		return;
	}

	std::shared_ptr<AudioRegion> r (std::dynamic_pointer_cast<AudioRegion> (region));

	if (!r) {
		return;
	}

	uint32_t    n_chans = r->n_channels ();
	samplecnt_t len;

	len = r->source (0)->length ().samples ();

	for (uint32_t n = 0; n < n_chans; ++n) {
		std::shared_ptr<Region> wr = RegionFactory::get_whole_region_for_source (r->source (n));

		if (!wr) {
			continue;
		}

		std::shared_ptr<AudioRegion> war = std::dynamic_pointer_cast<AudioRegion> (wr);
		if (!war) {
			continue;
		}

		WaveView* wv = new WaveView (data_group, war);
		wv->set_channel (0);
		wv->set_show_zero_line (false);
		wv->set_clip_level (1.0);
		wv->lower_to_bottom ();

		waves.push_back (wv);
	}

	set_spp_from_length (len);
	set_wave_heights ();
	set_waveform_colors ();

	// line_container->show ();
	line_container->raise_to_top ();

	set_session (&r->session ());
	state_connection.disconnect ();

	PBD::PropertyChange interesting_stuff;
	region_changed (interesting_stuff);

	region->PropertyChanged.connect (state_connection, invalidator (*this), std::bind (&AudioClipEditor::region_changed, this, _1), gui_context ());

	maybe_set_from_rsu ();
}

void
AudioClipEditor::canvas_allocate (Gtk::Allocation& alloc)
{
	EC_LOCAL_TEMPO_SCOPE;

	_canvas.size_allocate (alloc);

	_visible_canvas_width = alloc.get_width();
	_visible_canvas_height = alloc.get_height();

	/* no track header here, "track width" is the whole canvas */
	_track_canvas_width = _visible_canvas_width;

	main_ruler->set (ArdourCanvas::Rect (2, 2, alloc.get_width() - 4, timebar_height));

	position_lines ();
	update_fixed_rulers ();

	start_line->set_y1 (_visible_canvas_height - 2.);
	end_line->set_y1 (_visible_canvas_height - 2.);
	// loop_line->set_y1 (_visible_canvas_height - 2.);

	set_wave_heights ();

	catch_pending_show_region ();

	update_grid ();
}

void
AudioClipEditor::set_spp_from_length (samplecnt_t len)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_visible_canvas_width) {
		set_samples_per_pixel (floor (len / _visible_canvas_width));
	}
}

void
AudioClipEditor::set_wave_heights ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (waves.empty ()) {
		return;
	}

	uint32_t       n  = 0;
	const Distance w  = _visible_canvas_height - (n_timebars * timebar_height);
	Distance       ht = w / waves.size ();

	for (auto& wave : waves) {
		wave->set_height (ht);
		wave->set_y_position ((n_timebars * timebar_height) + (n * ht));
		++n;
	}
}

void
AudioClipEditor::set_waveform_colors ()
{
	EC_LOCAL_TEMPO_SCOPE;

	Gtkmm2ext::Color clip    = UIConfiguration::instance ().color ("clipped waveform");
	Gtkmm2ext::Color zero    = UIConfiguration::instance ().color ("zero line");
	Gtkmm2ext::Color fill    = UIConfiguration::instance ().color ("waveform fill");
	Gtkmm2ext::Color outline = UIConfiguration::instance ().color ("waveform outline");

	for (auto& wave : waves) {
		wave->set_fill_color (fill);
		wave->set_outline_color (outline);
		wave->set_clip_color (clip);
		wave->set_zero_color (zero);
	}
}

Gtk::Widget&
AudioClipEditor::contents ()
{
	EC_LOCAL_TEMPO_SCOPE;

	return _contents;
}

void
AudioClipEditor::region_changed (const PBD::PropertyChange& what_changed)
{
	EC_LOCAL_TEMPO_SCOPE;

}

void
AudioClipEditor::set_samples_per_pixel (samplecnt_t spp)
{
	EC_LOCAL_TEMPO_SCOPE;

	CueEditor::set_samples_per_pixel (spp);

	clip_metric->units_per_pixel = samples_per_pixel;

	position_lines ();

	for (auto& wave : waves) {
		wave->set_samples_per_pixel (samples_per_pixel);
	}

	horizontal_adjustment.set_upper (max_zoom_extent().second.samples() / samples_per_pixel);
	horizontal_adjustment.set_page_size (current_page_samples()/ samples_per_pixel / 10);
	horizontal_adjustment.set_page_increment (current_page_samples()/ samples_per_pixel / 20);
	horizontal_adjustment.set_step_increment (current_page_samples() / samples_per_pixel / 100);
}

samplecnt_t
AudioClipEditor::current_page_samples() const
{
	EC_LOCAL_TEMPO_SCOPE;

	return (samplecnt_t) _track_canvas_width * samples_per_pixel;
}

bool
AudioClipEditor::canvas_enter_leave (GdkEventCrossing* ev)
{
	EC_LOCAL_TEMPO_SCOPE;

	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
		if (ev->detail != GDK_NOTIFY_INFERIOR) {
			_canvas.grab_focus ();
			// ActionManager::set_sensitive (_midi_actions, true);
			within_track_canvas = true;
		}
		break;
	case GDK_LEAVE_NOTIFY:
		if (ev->detail != GDK_NOTIFY_INFERIOR) {
			// ActionManager::set_sensitive (_midi_actions, false);
			within_track_canvas = false;
			ARDOUR_UI::instance()->reset_focus (&_canvas_viewport);
			gdk_window_set_cursor (_canvas_viewport.get_window()->gobj(), nullptr);
		}
	default:
		break;
	}
	return false;
}

void
AudioClipEditor::begin_write ()
{
	EC_LOCAL_TEMPO_SCOPE;

}

void
AudioClipEditor::end_write ()
{
	EC_LOCAL_TEMPO_SCOPE;

}

void
AudioClipEditor::show_count_in (std::string const &)
{
	EC_LOCAL_TEMPO_SCOPE;

}

void
AudioClipEditor::hide_count_in ()
{
	EC_LOCAL_TEMPO_SCOPE;

}

void
AudioClipEditor::maybe_update ()
{
	EC_LOCAL_TEMPO_SCOPE;

	ARDOUR::TriggerPtr playing_trigger;

	if (ref.trigger()) {

		/* Trigger editor */

		playing_trigger = ref.box()->currently_playing ();

		if (!playing_trigger) {

			if (_drags->active() || !_region || !_track || !_track->triggerbox()) {
				return;
			}

			if (_track->triggerbox()->record_enabled() == Recording) {

				_playhead_cursor->set_position (data_capture_duration);
			}

		} else {
			if (playing_trigger->active ()) {
				if (playing_trigger->the_region()) {
					_playhead_cursor->set_position (playing_trigger->current_pos().samples() + playing_trigger->the_region()->start().samples());
				}
			} else {
				_playhead_cursor->set_position (0);
			}
		}
#if 0
	} else if (view->midi_region()) {

		/* Timeline region editor */

		if (!_session) {
			return;
		}

		samplepos_t pos = _session->transport_sample();
		samplepos_t spos = view->midi_region()->source_position().samples();
		if (pos < spos) {
			_playhead_cursor->set_position (0);
		} else {
			_playhead_cursor->set_position (pos - spos);
		}
#endif
	} else {
		_playhead_cursor->set_position (0);
	}

	if (_session->transport_rolling() && follow_playhead() && !_scroll_drag) {
		reset_x_origin_to_follow_playhead ();
	}
}

void
AudioClipEditor::unset (bool trigger_too)
{
	EC_LOCAL_TEMPO_SCOPE;

	drop_waves ();
	CueEditor::unset (trigger_too);
}

Gdk::Cursor*
AudioClipEditor::which_canvas_cursor (ItemType type) const
{
	EC_LOCAL_TEMPO_SCOPE;

	Gdk::Cursor* cursor = which_mode_cursor ();

	switch (type) {
	case ClipEndItem:
	case ClipStartItem:
		cursor = _cursors->expand_left_right;
		break;

	default:
		break;
	}

	return cursor;
}

void
AudioClipEditor::compute_fixed_ruler_scale ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_session == 0) {
		return;
	}

	set_minsec_ruler_scale (_leftmost_sample, _leftmost_sample + current_page_samples());
}

void
AudioClipEditor::update_fixed_rulers ()
{
	EC_LOCAL_TEMPO_SCOPE;
	compute_fixed_ruler_scale ();
}

void
AudioClipEditor::snap_mode_chosen (Editing::SnapMode)
{
}

void
AudioClipEditor::grid_type_chosen (Editing::GridType gt)
{
	if (gt != Editing::GridTypeMinSec && grid_actions[gt] && grid_actions[gt]->get_active()) {
		assert (grid_actions[Editing::GridTypeMinSec]);
		grid_actions[Editing::GridTypeMinSec]->set_active (false);
		grid_actions[Editing::GridTypeMinSec]->set_active (true);
	}
}

