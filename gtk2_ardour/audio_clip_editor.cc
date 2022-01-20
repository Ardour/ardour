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

#include "audio_clip_editor.h"
#include "audio_clock.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor.h"
#include "region_view.h"
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

Glib::RefPtr<Gtk::ActionGroup> ClipEditorBox::clip_editor_actions;

void
ClipEditorBox::init ()
{
	Bindings* bindings = Bindings::get_bindings (X_("Clip Editing"));

	register_clip_editor_actions (bindings);

	//_track_canvas_viewport->canvas()->set_data ("ardour-bindings", midi_bindings);
}

void
ClipEditorBox::register_clip_editor_actions (Bindings* clip_editor_bindings)
{
	clip_editor_actions = ActionManager::create_action_group (clip_editor_bindings, X_("ClipEditing"));

	/* two versions to allow same action for Delete and Backspace */

	// ActionManager::register_action (clip_editor_actions, X_("zoom-in"), _("Zoom In"), sigc::mem_fun (*this, &ClipEditorBox::zoom_in));
	// ActionManager::register_action (clip_editor_actions, X_("zoom-in"), _("Zoom In"), sigc::mem_fun (*this, &ClipEditorBox::zoom_out));
}

void
AudioClipEditor::ClipBBTMetric::get_marks (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, int maxchars) const
{
	TriggerPtr trigger (tref.trigger());
	if (!trigger) {
		std::cerr << "No trigger\n";
		return;
	}

	boost::shared_ptr<AudioTrigger> at = boost::dynamic_pointer_cast<AudioTrigger> (trigger);
	if (!at) {
		return;
	}

	ArdourCanvas::Ruler::Mark mark;

	assert (at->segment_tempo() > 0.);

	Temporal::Tempo tempo (at->segment_tempo(), at->meter().divisions_per_bar());

	std::cerr << "get marks between " << lower << " .. " << upper << " with tempo " << tempo << " upp = " << units_per_pixel << std::endl;

	samplecnt_t samples_per_beat = tempo.samples_per_note_type (AudioEngine::instance()->sample_rate());
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

AudioClipEditor::AudioClipEditor ()
	: clip_metric (0)
	, _spp (0)
	, scroll_fraction (0)
	, current_line_drag (0)
	, current_scroll_drag (0)
{
	const double scale = UIConfiguration::instance ().get_ui_scale ();

	frame       = new ArdourCanvas::Rectangle (root ());
	frame->name = "audio clip editor frame";
	frame->set_fill (false);
	frame->Event.connect (sigc::mem_fun (*this, &AudioClipEditor::event_handler));

	/* Scroll bar does not scroll and it outside the frame */

	scroll_bar_trough = new ArdourCanvas::Rectangle (root ());
	scroll_bar_handle = new ArdourCanvas::Rectangle (scroll_bar_trough);
	scroll_bar_handle->set_outline (false);
	scroll_bar_handle->set_corner_radius (5.);
	scroll_bar_handle->Event.connect (sigc::mem_fun (*this, &AudioClipEditor::scroll_event_handler));
	scroll_bar_handle->disable_scroll_translation ();

	/* A scrolling container for our waves and lines etc. */

	waves_container = new ArdourCanvas::ScrollGroup (frame, ScrollGroup::ScrollsHorizontally);
	add_scroller (*waves_container);

	ruler_container = new ArdourCanvas::Container (waves_container);
	ruler           = new ArdourCanvas::Ruler (ruler_container, 0);
	ruler->name     = "Clip Editor";
	ruler->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
	ruler->set_fill_color (UIConfiguration::instance().color (X_("theme:bg1")));
	ruler->set_outline_color (UIConfiguration::instance().color (X_("theme:contrasting less")));

	/* the lines */

	line_container = new ArdourCanvas::Container (waves_container);

	const double line_width = 3.;

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
AudioClipEditor::scroll_event_handler (GdkEvent* ev)
{
	switch (ev->type) {
		case GDK_BUTTON_PRESS:
			current_scroll_drag = new ScrollDrag (*this);
			current_scroll_drag->begin (&ev->button);
			return true;

		case GDK_BUTTON_RELEASE:
			if (current_scroll_drag) {
				current_scroll_drag->end (&ev->button);
				delete current_scroll_drag;
				current_scroll_drag = 0;
				return true;
			}
			break;

		case GDK_MOTION_NOTIFY:
			if (current_scroll_drag) {
				current_scroll_drag->motion (&ev->motion);
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

double
AudioClipEditor::sample_to_pixel (samplepos_t s)
{
	return round (s / _spp);
}

samplepos_t
AudioClipEditor::pixel_to_sample (double p)
{
	return round (p * _spp);
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
	set_background_color (UIConfiguration::instance ().color (X_("theme:bg")));

	frame->set_outline_color (UIConfiguration::instance ().color (X_("neutral:midground")));

	start_line->set_outline_color (UIConfiguration::instance ().color (X_("theme:contrasting clock")));
	end_line->set_outline_color (UIConfiguration::instance ().color (X_("theme:contrasting alt")));
	loop_line->set_outline_color (UIConfiguration::instance ().color (X_("theme:contrasting selection")));

	scroll_bar_trough->set_fill_color (UIConfiguration::instance ().color (X_("theme:bg")));
	scroll_bar_trough->set_outline_color (UIConfiguration::instance ().color (X_("theme:contrasting less")));
	scroll_bar_handle->set_fill_color (UIConfiguration::instance ().color (X_("theme:contrasting clock")));

	set_waveform_colors ();
}

void
AudioClipEditor::scroll_changed ()
{
	if (!audio_region) {
		return;
	}

	const double right_edge  = scroll_bar_handle->get ().x0;
	const double avail_width = scroll_bar_trough->get ().width () - scroll_bar_handle->get ().width ();
	scroll_fraction          = right_edge / avail_width;
	scroll_fraction          = std::min (1., std::max (0., scroll_fraction));
	const samplepos_t s      = llrintf (audio_region->source (0)->length ().samples () * scroll_fraction);

	ruler->set_range (s, s + pixel_to_sample (frame->get().width() - 2.));

	scroll_to (sample_to_pixel (s), 0);

	queue_draw ();
}

AudioClipEditor::ScrollDrag::ScrollDrag (AudioClipEditor& e)
    : editor (e)
{
	e.scroll_bar_handle->grab ();
}

void
AudioClipEditor::ScrollDrag::begin (GdkEventButton* ev)
{
	last_x = ev->x;
}

void
AudioClipEditor::ScrollDrag::end (GdkEventButton* ev)
{
	editor.scroll_bar_handle->ungrab ();
	editor.scroll_changed ();
}

void
AudioClipEditor::ScrollDrag::motion (GdkEventMotion* ev)
{
	ArdourCanvas::Rectangle& r (*editor.scroll_bar_handle);
	const double             xdelta = ev->x - last_x;
	ArdourCanvas::Rect       n (r.get ());
	const double             handle_width = n.width ();
	const double             avail_width  = editor.scroll_bar_trough->get ().width () - handle_width;

	n.x0 = std::max (0., std::min (avail_width, n.x0 + xdelta));
	n.x1 = n.x0 + handle_width;

	r.set (n);
	last_x = ev->x;

	editor.scroll_changed ();
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
AudioClipEditor::set_region (boost::shared_ptr<AudioRegion> r, TriggerReference tr)
{
	drop_waves ();

	audio_region = r;

	/* Ruler has to reflect tempo of the region, so we have to recreate it
	 * every time. Note that we retain ownership of the metric, and that
	 * because the GUI is single-threaded, we can set it and delete it
	 * safely here (there will be no calls to use it from within the
	 * ruler).
	 */

	delete clip_metric;
	clip_metric = new ClipBBTMetric (tr);
	ruler->set_metric (clip_metric);

	uint32_t    n_chans = r->n_channels ();
	samplecnt_t len;

	len = r->source (0)->length ().samples ();

	for (uint32_t n = 0; n < n_chans; ++n) {
		boost::shared_ptr<Region> wr = RegionFactory::get_whole_region_for_source (r->source (n));
		if (!wr) {
			continue;
		}

		boost::shared_ptr<AudioRegion> war = boost::dynamic_pointer_cast<AudioRegion> (wr);
		if (!war) {
			continue;
		}

		WaveView* wv = new WaveView (waves_container, war);
		wv->set_channel (n);
		wv->set_show_zero_line (false);
		wv->set_clip_level (1.0);
		wv->lower_to_bottom ();

		waves.push_back (wv);
	}

	TriggerPtr trigger (tr.trigger());
	boost::shared_ptr<AudioTrigger> at = boost::dynamic_pointer_cast<AudioTrigger> (trigger);
	if (at) {
		if (at->segment_tempo() == 0.) {
			/* tempo unknown, hide ruler */
			ruler->hide ();
		} else {
			ruler->show ();
			ruler->set_range (0, pixel_to_sample (frame->get().width() - 2.));
		}
	} else {
		ruler->hide ();
	}
	set_spp_from_length (len);
	set_wave_heights ();
	set_waveform_colors ();

	line_container->show ();
	line_container->raise_to_top ();
}

void
AudioClipEditor::on_size_allocate (Gtk::Allocation& alloc)
{
	GtkCanvas::on_size_allocate (alloc);

	ArdourCanvas::Rect r (1, 1, alloc.get_width () - 2, alloc.get_height () - 2);
	frame->set (r);

	const double ruler_height = 25.;
	ruler->set (ArdourCanvas::Rect (2, 2, alloc.get_width() - 4, ruler_height));

	const double scroll_bar_height      = 10.;
	const double scroll_bar_width       = alloc.get_width () - 2;
	const double scroll_bar_handle_left = scroll_bar_width * scroll_fraction;

	scroll_bar_trough->set (ArdourCanvas::Rect (1, alloc.get_height () - scroll_bar_height, scroll_bar_width, alloc.get_height ()));
	scroll_bar_handle->set (ArdourCanvas::Rect (scroll_bar_handle_left, scroll_bar_trough->get ().y0 + 1, scroll_bar_handle_left + 30., scroll_bar_trough->get ().y1 - 1));

	position_lines ();

	start_line->set_y1 (frame->get ().height () - 2. - scroll_bar_height);
	end_line->set_y1 (frame->get ().height () - 2. - scroll_bar_height);
	loop_line->set_y1 (frame->get ().height () - 2. - scroll_bar_height);

	set_wave_heights ();
}

void
AudioClipEditor::set_spp (double samples_per_pixel)
{
	_spp = samples_per_pixel;

	clip_metric->units_per_pixel = _spp;

	position_lines ();

	for (auto& wave : waves) {
		wave->set_samples_per_pixel (_spp);
	}
}

void
AudioClipEditor::set_spp_from_length (samplecnt_t len)
{
	double available_width = frame->get ().width ();
	double s               = floor (len / available_width);

	set_spp (s);
}

void
AudioClipEditor::set_wave_heights ()
{
	if (waves.empty ()) {
		return;
	}

	uint32_t       n  = 0;
	const Distance w  = frame->get ().height () - scroll_bar_trough->get ().height () - 2. - ruler->get().height();
	Distance       ht = w / waves.size ();

	for (auto& wave : waves) {
		wave->set_height (ht);
		wave->set_y_position (ruler->get ().height () + (n * ht));
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

bool
AudioClipEditor::event_handler (GdkEvent* ev)
{
	switch (ev->type) {
		case GDK_BUTTON_PRESS:
			break;
		case GDK_ENTER_NOTIFY:
			break;
		case GDK_LEAVE_NOTIFY:
			break;
		default:
			break;
	}
	return false;
}

AudioClipEditorBox::AudioClipEditorBox ()
{
	_header_label.set_text (_("AUDIO Region Trimmer:"));
	_header_label.set_alignment (0.0, 0.5);

	zoom_in_button.set_icon (ArdourIcon::ZoomIn);
	zoom_out_button.set_icon (ArdourIcon::ZoomOut);

	zoom_in_button.signal_clicked.connect (sigc::mem_fun (*this, &AudioClipEditorBox::zoom_in_click));
	zoom_out_button.signal_clicked.connect (sigc::mem_fun (*this, &AudioClipEditorBox::zoom_out_click));

	header_box.pack_start (_header_label, false, false);
	header_box.pack_start (zoom_in_button, false, false);
	header_box.pack_start (zoom_out_button, false, false);

	pack_start (header_box, false, false, 6);

	editor = manage (new AudioClipEditor);
	editor->set_size_request (600, 120);

	pack_start (*editor, true, true);
	editor->show ();
}

AudioClipEditorBox::~AudioClipEditorBox ()
{
	delete editor;
}

void
AudioClipEditorBox::zoom_in_click ()
{
	editor->set_spp (editor->spp () / 2.);
}

void
AudioClipEditorBox::zoom_out_click ()
{
	editor->set_spp (editor->spp () * 2.);
}

void
AudioClipEditorBox::set_region (boost::shared_ptr<Region> r, TriggerReference tref)
{
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);

	if (!ar) {
		return;
	}

	set_session (&r->session ());

	state_connection.disconnect ();

	_region = r;
	editor->set_region (ar, tref);

	PBD::PropertyChange interesting_stuff;
	region_changed (interesting_stuff);

	_region->PropertyChanged.connect (state_connection, invalidator (*this), boost::bind (&AudioClipEditorBox::region_changed, this, _1), gui_context ());
}

void
AudioClipEditorBox::region_changed (const PBD::PropertyChange& what_changed)
{
}
