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
#include "editor_automation_line.h"
#include "editor_cursors.h"
#include "control_point.h"
#include "editor.h"
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
AudioClipEditor::ClipBBTMetric::get_marks (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, int maxchars) const
{
	TriggerPtr trigger (tref.trigger());
	if (!trigger) {
		std::cerr << "No trigger\n";
		return;
	}

	std::shared_ptr<AudioTrigger> at = std::dynamic_pointer_cast<AudioTrigger> (trigger);
	if (!at) {
		return;
	}

	ArdourCanvas::Ruler::Mark mark;

	assert (at->segment_tempo() > 0.);

	Temporal::Tempo tempo (at->segment_tempo(), at->meter().divisions_per_bar());

	std::cerr << "get marks between " << lower << " .. " << upper << " with tempo " << tempo << " upp = " << units_per_pixel << std::endl;

	samplecnt_t samples_per_beat = tempo.samples_per_note_type (TEMPORAL_SAMPLE_RATE);
	int64_t beat_number = (lower + (samples_per_beat/2)) / samples_per_beat;
	int64_t last = INT64_MIN;
	const double scale = UIConfiguration::instance ().get_ui_scale ();

	for (int64_t n = beat_number * samples_per_beat; n < upper; n += samples_per_beat) {
		/* ensure at least a 15 pixel (scaled) gap between marks */
		if (marks.empty() || (((n - last) / units_per_pixel) > (15. * scale))) {
			mark.style    = ArdourCanvas::Ruler::Mark::Major;
			mark.label    = string_compose ("%1", beat_number);
			mark.position = n;
			marks.push_back (mark);
			beat_number++;
			last = n;
		}
	}
}

AudioClipEditor::AudioClipEditor (std::string const & name, bool with_transport)
	: CueEditor (name, with_transport)
	, _viewport (horizontal_adjustment, vertical_adjustment)
	, canvas (*_viewport.canvas())
	, clip_metric (nullptr)
	, scroll_fraction (0)
	, current_line_drag (0)
{
	build_upper_toolbar ();
	build_canvas ();
}

void
AudioClipEditor::pack_inner (Gtk::Box& box)
{
	box.pack_start (snap_box, false, false);
	box.pack_start (grid_box, false, false);
	box.pack_start (draw_box, false, false);
}

void
AudioClipEditor::pack_outer (Gtk::Box& box)
{
	if (with_transport_controls) {
		box.pack_start (play_box, false, false);
	}

	box.pack_start (rec_box, false, false);
	box.pack_start (follow_playhead_button, false, false);
}

void
AudioClipEditor::build_lower_toolbar ()
{
	horizontal_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &CueEditor::scrolled));
	_canvas_hscrollbar = manage (new Gtk::HScrollbar (horizontal_adjustment));

	_toolbox.pack_start (*_canvas_hscrollbar, false, false);
	_toolbox.pack_start (button_bar, false, false);
}

void
AudioClipEditor::build_canvas ()
{
	canvas.set_background_color (UIConfiguration::instance().color ("arrange base"));
	canvas.signal_event().connect (sigc::mem_fun (*this, &CueEditor::canvas_pre_event), false);
	canvas.use_nsglview (UIConfiguration::instance().get_nsgl_view_mode () == NSGLHiRes);

	canvas.PreRender.connect (sigc::mem_fun(*this, &EditingContext::pre_render));

	/* scroll group for items that should not automatically scroll
	 *  (e.g verbose cursor). It shares the canvas coordinate space.
	*/
	no_scroll_group = new ArdourCanvas::Container (canvas.root());

	h_scroll_group = new ArdourCanvas::ScrollGroup (canvas.root(), ArdourCanvas::ScrollGroup::ScrollsHorizontally);
	CANVAS_DEBUG_NAME (h_scroll_group, "audioclip h scroll");
	canvas.add_scroller (*h_scroll_group);


	v_scroll_group = new ArdourCanvas::ScrollGroup (canvas.root(), ArdourCanvas::ScrollGroup::ScrollsVertically);
	CANVAS_DEBUG_NAME (v_scroll_group, "audioclip v scroll");
	canvas.add_scroller (*v_scroll_group);

	hv_scroll_group = new ArdourCanvas::ScrollGroup (canvas.root(),
	                                                 ArdourCanvas::ScrollGroup::ScrollSensitivity (ArdourCanvas::ScrollGroup::ScrollsVertically|
		                ArdourCanvas::ScrollGroup::ScrollsHorizontally));
	CANVAS_DEBUG_NAME (hv_scroll_group, "audioclip hv scroll");
	canvas.add_scroller (*hv_scroll_group);

	cursor_scroll_group = new ArdourCanvas::ScrollGroup (canvas.root(), ArdourCanvas::ScrollGroup::ScrollsHorizontally);
	CANVAS_DEBUG_NAME (cursor_scroll_group, "audioclip cursor scroll");
	canvas.add_scroller (*cursor_scroll_group);

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
	minsec_ruler = new ArdourCanvas::Ruler (time_line_group, clip_metric, ArdourCanvas::Rect (0, 0, ArdourCanvas::COORD_MAX, timebar_height));
	// minsec_ruler->set_name ("audio clip editor ruler");
	minsec_ruler->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
	minsec_ruler->set_fill_color (UIConfiguration::instance().color (X_("theme:bg1")));
	minsec_ruler->set_outline_color (UIConfiguration::instance().color (X_("theme:contrasting less")));
	n_timebars++;

	minsec_ruler->Event.connect (sigc::mem_fun (*this, &AudioClipEditor::minsec_ruler_event));

	data_group = new ArdourCanvas::Container (hv_scroll_group);
	CANVAS_DEBUG_NAME (data_group, "cue data group");

	data_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebar_height * n_timebars));
	no_scroll_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebar_height * n_timebars));
	cursor_scroll_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebar_height * n_timebars));
	h_scroll_group->set_position (Duple (_timeline_origin, 0.));

	_verbose_cursor = new VerboseCursor (*this);

	// _playhead_cursor = new EditorCursor (*this, &Editor::canvas_playhead_cursor_event, X_("playhead"));
	_playhead_cursor = new EditorCursor (*this, X_("playhead"));
	_playhead_cursor->set_sensitive (UIConfiguration::instance().get_sensitize_playhead());
	_playhead_cursor->set_color (UIConfiguration::instance().color ("play head"));
	_playhead_cursor->canvas_item().raise_to_top();
	h_scroll_group->raise_to_top ();

	canvas.set_name ("AudioClipCanvas");
	canvas.add_events (Gdk::POINTER_MOTION_HINT_MASK | Gdk::SCROLL_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	canvas.set_can_focus ();
	canvas.signal_show().connect (sigc::mem_fun (*this, &CueEditor::catch_pending_show_region));
	_viewport.signal_size_allocate().connect (sigc::mem_fun(*this, &AudioClipEditor::canvas_allocate), false);

	_toolbox.pack_start (_viewport, true, true);

	/* the lines */

	line_container = new ArdourCanvas::Container (data_group);

	const double line_width = 3.;
	double scale = UIConfiguration::instance().get_ui_scale();

	start_line = new Line (line_container);
	start_line->set_outline_width (line_width * scale);
	end_line = new Line (line_container);
	end_line->set_outline_width (line_width * scale);
	loop_line = new Line (line_container);
	loop_line->set_outline_width (line_width * scale);

	start_line->Event.connect (sigc::bind (sigc::mem_fun (*this, &AudioClipEditor::line_event_handler), start_line));
	end_line->Event.connect (sigc::bind (sigc::mem_fun (*this, &AudioClipEditor::line_event_handler), end_line));
	loop_line->Event.connect (sigc::bind (sigc::mem_fun (*this, &AudioClipEditor::line_event_handler), loop_line));

	/* hide lines until there is a region */

	line_container->hide ();

	set_colors ();
}

AudioClipEditor::~AudioClipEditor ()
{
	drop_waves ();
	delete clip_metric;
}

bool
AudioClipEditor::line_event_handler (GdkEvent* ev, ArdourCanvas::Line* l)
{
	std::cerr << "event type " << Gtkmm2ext::event_type_string (ev->type) << " on line " << std::endl;

	switch (ev->type) {
		case GDK_BUTTON_PRESS:
			current_line_drag = new LineDrag (*this, *l);
			return true;

		case GDK_BUTTON_RELEASE:
			if (current_line_drag) {
				current_line_drag->end (&ev->button);
				delete current_line_drag;
				current_line_drag = 0;
				return true;
			}
			break;

		case GDK_MOTION_NOTIFY:
			if (current_line_drag) {
				current_line_drag->motion (&ev->motion);
				return true;
			}
			break;

		case GDK_KEY_PRESS:
			return key_press (&ev->key);

		default:
			break;
	}

	return false;
}

bool
AudioClipEditor::key_press (GdkEventKey* ev)
{
	return false;
}

void
AudioClipEditor::position_lines ()
{
	if (!audio_region) {
		return;
	}

	start_line->set_x0 (sample_to_pixel (audio_region->start ().samples ()));
	start_line->set_x1 (sample_to_pixel (audio_region->start ().samples ()));

	end_line->set_x0 (sample_to_pixel (audio_region->end ().samples ()));
	end_line->set_x1 (sample_to_pixel (audio_region->end ().samples ()));
}

AudioClipEditor::LineDrag::LineDrag (AudioClipEditor& ed, ArdourCanvas::Line& l)
    : editor (ed)
    , line (l)
{
	line.grab ();
}

void
AudioClipEditor::LineDrag::begin (GdkEventButton* ev)
{
}

void
AudioClipEditor::LineDrag::end (GdkEventButton* ev)
{
	line.ungrab ();
}

void
AudioClipEditor::LineDrag::motion (GdkEventMotion* ev)
{
	line.set_x0 (ev->x);
	line.set_x1 (ev->x);
}

void
AudioClipEditor::set_colors ()
{
	canvas.set_background_color (UIConfiguration::instance ().color (X_("theme:bg")));

	start_line->set_outline_color (UIConfiguration::instance ().color (X_("theme:contrasting clock")));
	end_line->set_outline_color (UIConfiguration::instance ().color (X_("theme:contrasting alt")));
	loop_line->set_outline_color (UIConfiguration::instance ().color (X_("theme:contrasting selection")));

	set_waveform_colors ();
}

void
AudioClipEditor::drop_waves ()
{
	for (auto& wave : waves) {
		delete wave;
	}

	waves.clear ();
}

void
AudioClipEditor::set_region (std::shared_ptr<ARDOUR::Region> r)
{
	set_region (std::dynamic_pointer_cast<ARDOUR::AudioRegion> (r));
}

void
AudioClipEditor::set_trigger (TriggerReference& tr)
{
	TriggerPtr trigger (tr.trigger());

	if (trigger == ref.trigger()) {
		return;
	}

	ref = tr;

	std::shared_ptr<AudioTrigger> at = std::dynamic_pointer_cast<AudioTrigger> (trigger);
	if (at) {
		minsec_ruler->show ();
		minsec_ruler->set_range (0, pixel_to_sample (_visible_canvas_width - 2.));
	} else {
		minsec_ruler->hide ();
	}

	if (ref.trigger()->the_region()) {
		std::shared_ptr<AudioRegion> ar = std::dynamic_pointer_cast<AudioRegion> (ref.trigger()->the_region());
		if (ar) {
			set_region (ar);
		}
	}

}

void
AudioClipEditor::set_region (std::shared_ptr<AudioRegion> r)
{
	drop_waves ();

	audio_region = r;

	if (!audio_region) {
		return;
	}

	/* Ruler has to reflect tempo of the region, so we have to recreate it
	 * every time. Note that we retain ownership of the metric, and that
	 * because the GUI is single-threaded, we can set it and delete it
	 * safely here (there will be no calls to use it from within the
	 * ruler).
	 */

	delete clip_metric;
	clip_metric = new ClipBBTMetric (ref);
	minsec_ruler->set_metric (clip_metric);

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
		wv->set_channel (n);
		wv->set_show_zero_line (false);
		wv->set_clip_level (1.0);
		wv->lower_to_bottom ();

		waves.push_back (wv);
	}

	set_spp_from_length (len);
	set_wave_heights ();
	set_waveform_colors ();

	line_container->show ();
	line_container->raise_to_top ();

	set_session (&r->session ());
	state_connection.disconnect ();

	PBD::PropertyChange interesting_stuff;
	region_changed (interesting_stuff);
	audio_region->PropertyChanged.connect (state_connection, invalidator (*this), std::bind (&AudioClipEditor::region_changed, this, _1), gui_context ());
}

void
AudioClipEditor::canvas_allocate (Gtk::Allocation& alloc)
{
	canvas.size_allocate (alloc);

	_visible_canvas_width = alloc.get_width();
	_visible_canvas_height = alloc.get_height();

	minsec_ruler->set (ArdourCanvas::Rect (2, 2, alloc.get_width() - 4, timebar_height));

	position_lines ();

	start_line->set_y1 (_visible_canvas_height - 2.);
	end_line->set_y1 (_visible_canvas_height - 2.);
	loop_line->set_y1 (_visible_canvas_height - 2.);

	set_wave_heights ();
}

void
AudioClipEditor::set_spp_from_length (samplecnt_t len)
{
	set_samples_per_pixel (floor (len / _visible_canvas_width));
}

void
AudioClipEditor::set_wave_heights ()
{
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
AudioClipEditor::viewport()
{
	return _viewport;
}

Gtk::Widget&
AudioClipEditor::contents ()
{
	return _contents;
}

void
AudioClipEditor::region_changed (const PBD::PropertyChange& what_changed)
{
}

void
AudioClipEditor::set_samples_per_pixel (samplecnt_t spp)
{
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
	return (samplecnt_t) _track_canvas_width * samples_per_pixel;
}

bool
AudioClipEditor::canvas_enter_leave (GdkEventCrossing* ev)
{
	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
		if (ev->detail != GDK_NOTIFY_INFERIOR) {
			canvas.grab_focus ();
			// ActionManager::set_sensitive (_midi_actions, true);
			within_track_canvas = true;
		}
		break;
	case GDK_LEAVE_NOTIFY:
		if (ev->detail != GDK_NOTIFY_INFERIOR) {
			// ActionManager::set_sensitive (_midi_actions, false);
			within_track_canvas = false;
			ARDOUR_UI::instance()->reset_focus (&_viewport);
			gdk_window_set_cursor (_viewport.get_window()->gobj(), nullptr);
		}
	default:
		break;
	}
	return false;
}

