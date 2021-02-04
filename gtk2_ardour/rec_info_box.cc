/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include "ardour/session.h"
#include "ardour/session_route.h"
#include "ardour/track.h"

#include "gtkmm2ext/utils.h"
#include "temporal/time.h"

#include "audio_clock.h"
#include "gui_thread.h"
#include "rec_info_box.h"
#include "timers.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

RecInfoBox::RecInfoBox ()
{
	set_name (X_("RecInfoBox"));
	_layout_label = Pango::Layout::create (get_pango_context ());
	_layout_value = Pango::Layout::create (get_pango_context ());

	UIConfiguration::instance().DPIReset.connect (sigc::mem_fun (*this, &RecInfoBox::dpi_reset));
	dpi_reset ();
}

RecInfoBox::~RecInfoBox ()
{
}

void
RecInfoBox::dpi_reset ()
{
	_layout_label->set_font_description (UIConfiguration::instance ().get_NormalFont ());
	_layout_value->set_font_description (UIConfiguration::instance ().get_LargeMonospaceFont ());

	int wl, hl, wv, hv;

	_layout_label->set_text (_("Last Capture Duration:"));
	_layout_label->get_pixel_size (wl, hl);

	_layout_value->set_text ("00:00:00:00");
	_layout_value->get_pixel_size (wv, hv);

	_width  = 8 + std::max (wl, wv);
	_height = 4 + 3 * (hl + hv + 8);

	queue_resize ();
}

void
RecInfoBox::on_size_request (Gtk::Requisition* r)
{
	r->width  = _width;
	r->height = std::max (150, _height);
}

void
RecInfoBox::on_size_allocate (Gtk::Allocation& a)
{
  CairoWidget::on_size_allocate (a);
}

void
RecInfoBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		return;
	}

	_session->RecordStateChanged.connect (_session_connections, invalidator (*this), boost::bind (&RecInfoBox::rec_state_changed, this), gui_context());
	_session->UpdateRouteRecordState.connect (_session_connections, invalidator (*this), boost::bind (&RecInfoBox::update, this), gui_context());
	update ();
}

void
RecInfoBox::rec_state_changed ()
{
	if (_session && _session->actively_recording ()) {
		if (!_rectime_connection.connected ()) {
			_rectime_connection = Timers::rapid_connect (sigc::mem_fun (*this, &RecInfoBox::update));
		}
	} else {
		_rectime_connection.disconnect ();
	}
	update ();
}

void
RecInfoBox::update ()
{
	set_dirty ();
}

void
RecInfoBox::count_recenabled_streams (Route& route)
{
  Track* track = dynamic_cast<Track*>(&route);
  if (track && track->rec_enable_control()->get_value()) {
    _rec_enabled_streams += track->n_inputs().n_total();
  }
}

void
RecInfoBox::render (Cairo::RefPtr<Cairo::Context> const& cr, cairo_rectangle_t* r)
{
	int ww = get_width ();
	int hh = get_height ();

	cr->rectangle (r->x, r->y, r->width, r->height);
	cr->clip ();
	cr->set_operator (Cairo::OPERATOR_OVER);

	bool recording;
	if (_session && _session->actively_recording ()) {
		Gtkmm2ext::set_source_rgb_a (cr, UIConfiguration::instance ().color ("alert:red"), .7);
		recording = true;
	} else {
		Gtkmm2ext::set_source_rgb_a (cr, UIConfiguration::instance ().color ("widget:bg"), .7);
		recording = false;
	}

	Gtkmm2ext::rounded_rectangle (cr, 1 , 1, ww - 2, hh - 2, /*_height / 4.0 */ 4);
	cr->fill ();

	if (!_session) {
		return;
	}

	Gtkmm2ext::set_source_rgba (cr, UIConfiguration::instance ().color ("neutral:foreground"));

	unsigned int xruns            = _session->capture_xruns ();
	samplecnt_t  capture_duration = _session->capture_duration ();
	samplecnt_t  sample_rate      = _session->nominal_sample_rate ();

	/* TODO: cache, calling this at rapid timer intervals when recording is not great */
	boost::optional<samplecnt_t> opt_samples = _session->available_capture_duration ();

	int top = 2;
	int w, h;

	if (recording || capture_duration == 0) {
		_layout_label->set_text (_("Capture Duration:"));
	} else {
		_layout_label->set_text (_("Last Capture Duration:"));
	}
	_layout_label->get_pixel_size (w, h);
	cr->move_to (.5 * (ww - w), top);
	_layout_label->show_in_cairo_context (cr);
	top += h + 2;

	if (capture_duration > 0) {
		char buf[32];
		AudioClock::print_minsec (capture_duration, buf, sizeof (buf), sample_rate, 1);
		_layout_value->set_text (std::string(buf).substr(1));
	} else {
		_layout_value->set_text ("--:--:--:-");
	}
	_layout_value->get_pixel_size (w, h);
	cr->move_to (.5 * (ww - w), top);
	_layout_value->show_in_cairo_context (cr);
	top += h + 6;

	_layout_label->set_text (_("Capture x-runs:"));
	_layout_label->get_pixel_size (w, h);
	cr->move_to (.5 * (ww - w), top);
	_layout_label->show_in_cairo_context (cr);
	top += h + 2;

	_layout_value->set_text (string_compose ("%1", xruns));
	_layout_value->get_pixel_size (w, h);
	cr->move_to (.5 * (ww - w), top);
	_layout_value->show_in_cairo_context (cr);
	top += h + 6;

	/* disk space */
	_layout_label->set_text (_("Available record time:"));
	_layout_label->get_pixel_size (w, h);
	cr->move_to (.5 * (ww - w), top);
	_layout_label->show_in_cairo_context (cr);
	top += h + 2;

	if (!opt_samples) {
		/* Available space is unknown */
		_layout_value->set_text (_("Unknown"));
	} else if (opt_samples.value_or (0) == max_samplecnt) {
		_layout_value->set_text (_(">24h"));
	} else {
		_rec_enabled_streams = 0;
		_session->foreach_route (this, &RecInfoBox::count_recenabled_streams, false);

		samplecnt_t samples = opt_samples.value_or (0);

		if (_rec_enabled_streams > 0) {
			samples /= _rec_enabled_streams;
		}

		float remain_sec = samples / (float)sample_rate;
		char buf[32];

		if (remain_sec > 86400) {
			_layout_value->set_text (_(">24h"));
		} else if (remain_sec > 32400 /* 9 hours */) {
			snprintf (buf, sizeof (buf), "%.0f", remain_sec / 3600.f);
			_layout_value->set_text (std::string (buf) + S_("hours|h"));
		} else if (remain_sec > 5940 /* 99 mins */) {
			snprintf (buf, sizeof (buf), "%.1f", remain_sec / 3600.f);
			_layout_value->set_text (std::string (buf) + S_("hours|h"));
		} else {
			snprintf (buf, sizeof (buf), "%.0f", remain_sec / 60.f);
			_layout_value->set_text (std::string (buf) + S_("minutes|m"));
		}
	}

	_layout_value->get_pixel_size (w, h);
	cr->move_to (.5 * (ww - w), top);
	_layout_value->show_in_cairo_context (cr);
	top += h + 6;
}
