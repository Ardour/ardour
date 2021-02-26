/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <cassert>

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

void
RecInfoBox::on_size_request (Gtk::Requisition* r)
{
	r->width  = _width;
	r->height = std::max (12, _height);
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

	if (_session) {
		update ();
	}
}

void
RecInfoBox::update ()
{
	set_dirty ();
}

/* ****************************************************************************/

void
DurationInfoBox::set_session (Session* s)
{
	RecInfoBox::set_session (s);

	if (!_session) {
		_rectime_connection.disconnect ();
		return;
	}
	_session->RecordStateChanged.connect (_session_connections, invalidator (*this), boost::bind (&DurationInfoBox::rec_state_changed, this), gui_context());
	_session->UpdateRouteRecordState.connect (_session_connections, invalidator (*this), boost::bind (&DurationInfoBox::update, this), gui_context());
}

void
DurationInfoBox::rec_state_changed ()
{
	if (_session && _session->actively_recording ()) {
		if (!_rectime_connection.connected ()) {
			_rectime_connection = Timers::rapid_connect (sigc::mem_fun (*this, &DurationInfoBox::update));
		}
	} else {
		_rectime_connection.disconnect ();
	}
	update ();
}

void
DurationInfoBox::dpi_reset ()
{
	int wv, hv;
	_layout_value->set_font_description (UIConfiguration::instance ().get_NormalMonospaceFont ());
	_layout_value->set_text ("<00:00:00:0>");
	_layout_value->get_pixel_size (wv, hv);
	_width  = 8 + wv;
	_height = 4 + hv;
	queue_resize ();
}

void
DurationInfoBox::render (Cairo::RefPtr<Cairo::Context> const& cr, cairo_rectangle_t* r)
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

	Gtkmm2ext::rounded_rectangle (cr, 1 , 1, ww - 1, hh - 1, /*_height / 4.0 */ 4);
	cr->fill ();

	if (!_session) {
		return;
	}

	Gtkmm2ext::set_source_rgba (cr, UIConfiguration::instance ().color ("neutral:foreground"));

	samplecnt_t  capture_duration = _session->capture_duration ();
	samplecnt_t  sample_rate      = _session->nominal_sample_rate ();

	int w, h;

	if (capture_duration > 0) {
		char buf[32];
		AudioClock::print_minsec (capture_duration, buf, sizeof (buf), sample_rate, 1);
		if (recording) {
			_layout_value->set_text (string_compose(" %1 ", std::string(buf).substr(1)));
		} else {
			_layout_value->set_text (string_compose("<%1>", std::string(buf).substr(1)));
		}
	} else {
		_layout_value->set_text (" --:--:--:- ");
	}
	_layout_value->get_pixel_size (w, h);
	cr->move_to (.5 * (ww - w), hh/2 - h/2);
	_layout_value->show_in_cairo_context (cr);
}

void
DurationInfoBox::update ()
{
	RecInfoBox::update ();
}

/* ****************************************************************************/

void
XrunInfoBox::set_session (Session* s)
{
	RecInfoBox::set_session (s);

	if (!_session) {
		return;
	}

	_session->Xrun.connect (_session_connections, invalidator (*this), boost::bind (&XrunInfoBox::update, this), gui_context());
	_session->RecordStateChanged.connect (_session_connections, invalidator (*this), boost::bind (&XrunInfoBox::update, this), gui_context());
}

void
XrunInfoBox::dpi_reset ()
{
	int wv, hv;
	_layout_value->set_font_description (UIConfiguration::instance ().get_NormalFont ());
	_layout_value->set_text ("<99+>");
	_layout_value->get_pixel_size (wv, hv);
	_width  = 8 + wv;
	_height = 8 + hv;
	queue_resize ();
}

void
XrunInfoBox::render (Cairo::RefPtr<Cairo::Context> const& cr, cairo_rectangle_t* r)
{
	if (!_session) {
		return;
	}

	int ww = get_width ();
	int hh = get_height ();

	cr->rectangle (r->x, r->y, r->width, r->height);
	cr->clip ();
	cr->set_operator (Cairo::OPERATOR_OVER);

	unsigned int xruns = _session->capture_xruns ();

	if (xruns > 0) {
		Gtkmm2ext::set_source_rgb_a (cr, UIConfiguration::instance ().color ("alert:red"), .7);
	} else {
		Gtkmm2ext::set_source_rgb_a (cr, UIConfiguration::instance ().color ("widget:bg"), .7);
	}

	Gtkmm2ext::rounded_rectangle (cr, 1 , 1, ww - 2, hh - 2, /*_height / 4.0 */ 4);
	cr->fill ();

	if (xruns < 99) {
		if (_session->actively_recording ()) {
			_layout_value->set_text (string_compose ("%1", xruns));
		} else if (_session->capture_duration () > 0) {
			_layout_value->set_text (string_compose ("<%1>", xruns));
		} else {
			_layout_value->set_text ("-");
		}
	} else {
		if (_session->actively_recording ()) {
			_layout_value->set_text ("99+");
		} else if (_session->capture_duration () > 0) {
			_layout_value->set_text ("<99+>");
		} else {
			assert (0);
			return;
		}
	}

	Gtkmm2ext::set_source_rgba (cr, UIConfiguration::instance ().color ("neutral:foreground"));
	int w, h;
	_layout_value->get_pixel_size (w, h);
	cr->move_to (.5 * (ww - w), .5 * (hh - h));
	_layout_value->show_in_cairo_context (cr);
}

void
XrunInfoBox::update ()
{
	RecInfoBox::update ();
}

/* ****************************************************************************/

void
RemainInfoBox::set_session (Session* s)
{
	RecInfoBox::set_session (s);

	if (!_session) {
		_diskspace_connection.disconnect ();
		return;
	}

	_diskspace_connection = Timers::second_connect (sigc::mem_fun (*this, &RemainInfoBox::update));
	_session->UpdateRouteRecordState.connect (_session_connections, invalidator (*this), boost::bind (&RemainInfoBox::update, this), gui_context());
}

void
RemainInfoBox::dpi_reset ()
{
	_layout_label->set_font_description (UIConfiguration::instance ().get_NormalFont ());
	_layout_value->set_font_description (UIConfiguration::instance ().get_NormalMonospaceFont ());

	int wl, hl, wv, hv;

	_layout_label->set_text (_("Disk Space:"));
	_layout_label->get_pixel_size (wl, hl);

	_layout_value->set_text (_(">24h"));
	_layout_value->get_pixel_size (wv, hv);

	_width  = 8 + std::max (wl, wv);
	_height = 2 + hv + 2 + hl + 2;

	queue_resize ();
}

void
RemainInfoBox::count_recenabled_streams (Route& route)
{
  Track* track = dynamic_cast<Track*>(&route);
  if (track && track->rec_enable_control()->get_value()) {
    _rec_enabled_streams += track->n_inputs().n_total();
  }
}

void
RemainInfoBox::render (Cairo::RefPtr<Cairo::Context> const& cr, cairo_rectangle_t* r)
{
	int ww = get_width ();
	int hh = get_height ();

	cr->rectangle (r->x, r->y, r->width, r->height);
	cr->clip ();
	cr->set_operator (Cairo::OPERATOR_OVER);

	if (!_session) {
		return;
	}

	samplecnt_t  sample_rate                 = _session->nominal_sample_rate ();
	boost::optional<samplecnt_t> opt_samples = _session->available_capture_duration ();

	Gtkmm2ext::set_source_rgb_a (cr, UIConfiguration::instance ().color ("widget:bg"), .7);

	if (!opt_samples) {
		/* Available space is unknown */
		_layout_value->set_text (_("Unknown"));
	} else if (opt_samples.value_or (0) == max_samplecnt) {
		_layout_value->set_text (_(">24h"));
	} else {
		_rec_enabled_streams = 0;
		_session->foreach_route (this, &RemainInfoBox::count_recenabled_streams, false);

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
		} else if (remain_sec > 60*3 /* 3 mins */) {
			snprintf (buf, sizeof (buf), "%.0f", remain_sec / 60.f);
			_layout_value->set_text (std::string (buf) + S_("minutes|m"));
		} else {
			Gtkmm2ext::set_source_rgb_a (cr, UIConfiguration::instance ().color ("alert:red"), .7);
			snprintf (buf, sizeof (buf), "%.0f", remain_sec / 60.f);
			_layout_value->set_text (std::string (buf) + S_("minutes|m"));
		}
	}

	/* draw box */
	Gtkmm2ext::rounded_rectangle (cr, 1 , 1, ww - 2, hh - 2, /*_height / 4.0 */ 4);
	cr->fill ();

	/*draw text */
	Gtkmm2ext::set_source_rgba (cr, UIConfiguration::instance ().color ("neutral:foreground"));
	cr->set_line_width (1.0);

	int w, h;
	_layout_label->get_pixel_size (w, h);
	cr->move_to (.5 * (ww - w), 4);
	_layout_label->show_in_cairo_context (cr);

	_layout_value->get_pixel_size (w, h);
	cr->move_to (.5 * (ww - w), hh - 4 - h);
	_layout_value->show_in_cairo_context (cr);
}

void
RemainInfoBox::update ()
{
	RecInfoBox::update ();
}
