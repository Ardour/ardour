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

#include "ardour/dB.h"
#include "ardour/logmeter.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/port_manager.h"

#include "gtkmm2ext/utils.h"

#include "widgets/fastmeter.h"
#include "widgets/tooltips.h"

#include "input_port_monitor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;

#define PX_SCALE(px) std::max ((float)px, rintf ((float)px* UIConfiguration::instance ().get_ui_scale ()))

InputPortMonitor::InputPortMonitor (ARDOUR::DataType dt, samplecnt_t sample_rate, Orientation o)
	: _dt (dt)
	, _audio_meter (0)
	, _audio_scope (0)
	, _midi_meter (0)
	, _midi_monitor (0)
	, _orientation (o)
{
	if (o == Vertical) {
		_box = new Gtk::HBox;
	} else {
		_box = new Gtk::VBox;
	}

	if (_dt == DataType::AUDIO) {
		setup_audio_meter ();

		_audio_scope = new InputScope (sample_rate, PX_SCALE (200), 25, o);

		if (UIConfiguration::instance ().get_input_meter_scopes ()) {
			_audio_scope->show ();
		} else {
			_audio_scope->set_no_show_all ();
		}

		ArdourWidgets::set_tooltip (_audio_scope, _("5 second history waveform"));

		_box->pack_start (_bin, false, false);
		_box->pack_start (*_audio_scope, true, true, 1);

		UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &InputPortMonitor::parameter_changed));
		UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &InputPortMonitor::color_handler));

	} else if (_dt == DataType::MIDI) {
		_midi_meter   = new EventMeter (o);
		_midi_monitor = new EventMonitor (o);
		_midi_meter->show ();

		if (UIConfiguration::instance ().get_input_meter_scopes ()) {
			_midi_monitor->show ();
		} else {
			_midi_monitor->set_no_show_all ();
		}

		ArdourWidgets::set_tooltip (_midi_meter, _("Highlight incoming MIDI data per MIDI channel"));
		ArdourWidgets::set_tooltip (_midi_monitor, _("Display most recently received MIDI messages"));

		_box->pack_start (*_midi_meter, false, false);
		_box->pack_start (*_midi_monitor, true, false, 1);
	}
	add (*_box);
	_box->show ();
}

InputPortMonitor::~InputPortMonitor ()
{
	delete _audio_meter;
	delete _audio_scope;
	delete _midi_meter;
	delete _midi_monitor;
	delete _box;
}

void
InputPortMonitor::parameter_changed (std::string const& p)
{
	if (_audio_scope) {
		_audio_scope->parameter_changed (p);
	}
	if (_audio_meter) {
		if (p == "meter-hold") {
			_audio_meter->set_hold_count ((uint32_t) floor(UIConfiguration::instance().get_meter_hold()));
		} else if (p == "meter-style-led") {
			setup_audio_meter ();
		} else if (p == "meter-line-up-level") {
			setup_audio_meter ();
		}
	}
}

void
InputPortMonitor::color_handler ()
{
	if (_audio_meter) {
		setup_audio_meter ();
	}
}

void
InputPortMonitor::clear ()
{
	if (_audio_meter) {
		_audio_meter->clear ();
	}
	if (_audio_scope) {
		_audio_scope->clear ();
	}
	if (_midi_meter) {
		_midi_meter->clear ();
	}
	if (_midi_monitor) {
		_midi_monitor->clear ();
	}
}

void
InputPortMonitor::setup_audio_meter ()
{
	_bin.remove ();
	delete _audio_meter;

	float stp;
	switch (UIConfiguration::instance().get_meter_line_up_level()) {
		case MeteringLineUp24:
			stp = 115.0 * log_meter0dB(-24);
			break;
		case MeteringLineUp20:
			stp = 115.0 * log_meter0dB(-20);
			break;
		default:
		case MeteringLineUp18:
			stp = 115.0 * log_meter0dB(-18);
			break;
		case MeteringLineUp15:
			stp = 115.0 * log_meter0dB(-15);
	}

	_audio_meter = new FastMeter (
			(uint32_t)floor (UIConfiguration::instance ().get_meter_hold ()),
			18,
			_orientation == Vertical ? FastMeter::Vertical : FastMeter::Horizontal,
			PX_SCALE (200),
			UIConfiguration::instance ().color ("meter color0"),
			UIConfiguration::instance ().color ("meter color1"),
			UIConfiguration::instance ().color ("meter color2"),
			UIConfiguration::instance ().color ("meter color3"),
			UIConfiguration::instance ().color ("meter color4"),
			UIConfiguration::instance ().color ("meter color5"),
			UIConfiguration::instance ().color ("meter color6"),
			UIConfiguration::instance ().color ("meter color7"),
			UIConfiguration::instance ().color ("meter color8"),
			UIConfiguration::instance ().color ("meter color9"),
			UIConfiguration::instance ().color ("meter background bottom"),
			UIConfiguration::instance ().color ("meter background top"),
			0x991122ff, // red highlight gradient Bot
			0x551111ff, // red highlight gradient Top
			stp,
			89.125,  // 115.0 * log_meter0dB(-9);
			106.375, // 115.0 * log_meter0dB(-3);
			115.0,   // 115.0 * log_meter0dB(0);
			(UIConfiguration::instance ().get_meter_style_led () ? 3 : 1));

	_bin.add (*_audio_meter);
	_bin.show ();
	_audio_meter->show ();
}


void
InputPortMonitor::update (float l, float p)
{
	assert (_dt == DataType::AUDIO && _audio_meter);
	_audio_meter->set (log_meter0dB (l), log_meter0dB (p));
}

void
InputPortMonitor::update (ARDOUR::CircularSampleBuffer& csb)
{
	assert (_dt == DataType::AUDIO && _audio_scope);
	_audio_scope->update (csb);
}

void
InputPortMonitor::update (float const* v)
{
	assert (_dt == DataType::MIDI && _midi_meter);
	_midi_meter->update (v);
}

void
InputPortMonitor::update (ARDOUR::CircularEventBuffer& ceb)
{
	assert (_dt == DataType::MIDI && _midi_monitor);
	_midi_monitor->update (ceb);
}

/* ****************************************************************************/

InputPortMonitor::InputScope::InputScope (samplecnt_t rate, int l, int g, Orientation o)
	: _pos (0)
	, _rate (rate)
	, _min_length (l)
	, _min_gauge (g)
	, _orientation (o)
{
	_surface = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, l, g);
	use_image_surface (false); /* we already use a surface */

	parameter_changed ("waveform-clip-level");
	parameter_changed ("show-waveform-clipping");
	parameter_changed ("waveform-scale");
	UIConfiguration::instance().DPIReset.connect (sigc::mem_fun (*this, &InputPortMonitor::InputScope::dpi_reset));
}

void
InputPortMonitor::InputScope::dpi_reset ()
{
	if (is_realized ()) {
		queue_resize ();
	}
}

void
InputPortMonitor::InputScope::parameter_changed (std::string const& p)
{
	if (p == "waveform-clip-level") {
		_clip_level = dB_to_coefficient (UIConfiguration::instance ().get_waveform_clip_level ());
	} else if (p == "show-waveform-clipping") {
		_show_clip = UIConfiguration::instance ().get_show_waveform_clipping ();
	} else if (p == "waveform-scale") {
		_logscale = UIConfiguration::instance ().get_waveform_scale () == Logarithmic;
	}
}

void
InputPortMonitor::InputScope::on_size_request (Gtk::Requisition* req)
{
	if (_orientation == Horizontal) {
		req->width  = 2 + _min_length;
		req->height = 2 + _min_gauge;
	} else {
		req->width  = 2 + _min_gauge;
		req->height = 2 + _min_length;
	}
}

void
InputPortMonitor::InputScope::on_size_allocate (Gtk::Allocation& a)
{
	CairoWidget::on_size_allocate (a);

	int w, h;
	if (_orientation == Horizontal) {
		w = 2 + _surface->get_width ();
		h = 2 + _surface->get_height ();
	} else {
		w = 2 + _surface->get_height ();
		h = 2 + _surface->get_width ();
	}

	if (a.get_width () != w || a.get_height () != h) {
		_surface = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, std::max (1, a.get_width () - 2), std::max (1, a.get_height () - 2));
	}
}

void
InputPortMonitor::InputScope::clear ()
{
	int w = _surface->get_width ();
	int h = _surface->get_height ();

	Cairo::RefPtr<Cairo::Context> cr= Cairo::Context::create (_surface);
	cr->rectangle (0, 0, w, h);
	cr->set_operator (Cairo::OPERATOR_SOURCE);
	cr->set_source_rgba (0, 0, 0, 0);
	cr->fill ();
	_pos = 0;
	set_dirty ();
}

void
InputPortMonitor::InputScope::update (CircularSampleBuffer& csb)
{
	int l = _orientation == Horizontal ? _surface->get_width () : _surface->get_height ();
	int g = _orientation == Horizontal ? _surface->get_height () : _surface->get_width ();

	double g2 = g / 2.0;

	int spp = 5.0 /*sec*/ * _rate / l; // samples / pixel
	Cairo::RefPtr<Cairo::Context> cr;

	bool  have_data = false;
	float minf, maxf;

	while (csb.read (minf, maxf, spp)) {
		if (!have_data) {
			have_data = true;
			cr        = Cairo::Context::create (_surface);
		}

		/* see also ExportReport::draw_waveform */
		if (_orientation == Horizontal) {
			cr->rectangle (_pos, 0, 1, g);
		} else {
			if (_pos-- == 0) {
				_pos = l - 1;
			}
			cr->rectangle (0, _pos, g, 1);
		}
		cr->set_operator (Cairo::OPERATOR_SOURCE);
		cr->set_source_rgba (0, 0, 0, 0);
		cr->fill ();

		cr->set_operator (Cairo::OPERATOR_OVER);
		cr->set_line_width (1.0);

		if (_show_clip && (maxf >= _clip_level || -minf >= _clip_level)) {
			Gtkmm2ext::set_source_rgba (cr, UIConfiguration::instance ().color ("clipped waveform"));
		} else {
			Gtkmm2ext::set_source_rgba (cr, UIConfiguration::instance ().color ("waveform fill"));
		}

		if (_logscale) {
			if (maxf > 0) {
				maxf =  alt_log_meter (fast_coefficient_to_dB (maxf));
			} else {
				maxf = -alt_log_meter (fast_coefficient_to_dB (-maxf));
			}
			if (minf > 0) {
				minf =  alt_log_meter (fast_coefficient_to_dB (minf));
			} else {
				minf = -alt_log_meter (fast_coefficient_to_dB (-minf));
			}
		}

		if (_orientation == Horizontal) {
			cr->move_to (_pos + .5, g2 - g2 * maxf);
			cr->line_to (_pos + .5, g2 - g2 * minf);
			cr->stroke ();
			if (++_pos >= l) {
				_pos = 0;
			}
		} else {
			cr->move_to (g2 + g2 * minf, _pos + .5);
			cr->line_to (g2 + g2 * maxf, _pos + .5);
			cr->stroke ();
		}
	}

	if (have_data) {
		_surface->flush ();
		set_dirty ();
	}
}

void
InputPortMonitor::InputScope::render (Cairo::RefPtr<Cairo::Context> const& cr, cairo_rectangle_t* r)
{
	cr->rectangle (r->x, r->y, r->width, r->height);
	cr->clip ();
	cr->set_operator (Cairo::OPERATOR_OVER);

	int w = _surface->get_width ();
	int h = _surface->get_height ();

	cr->save ();
	cr->translate (1, 1);
	cr->rectangle (0, 0, w, h);
	cr->clip ();

	if (_orientation == Vertical) {
		cr->set_source (_surface, 0, 0.0 - _pos);
		cr->paint ();
		cr->set_source (_surface, 0, h - _pos);
		cr->paint ();

		double g2 = .5 * w;
		cr->move_to (g2, 0);
		cr->line_to (g2, h);

	} else {
		cr->set_source (_surface, 0.0 - _pos, 0);
		cr->paint ();
		cr->set_source (_surface, w - _pos, 0);
		cr->paint ();

		double g2 = .5 * h;
		cr->move_to (0, g2);
		cr->line_to (w, g2);
	}

	/* zero line */
	cr->set_line_width (1.0);
	Gtkmm2ext::set_source_rgb_a (cr, UIConfiguration::instance ().color ("zero line"), .7);
	cr->stroke ();

	/* black border - compare to FastMeter::horizontal_expose */
	cr->set_line_width (2.0);
	Gtkmm2ext::rounded_rectangle (cr, 0, 0, get_width (), get_height (), boxy_buttons () ? 0 : 2);
	cr->set_source_rgb (0, 0, 0); // black
	cr->stroke ();
}

/* ****************************************************************************/

InputPortMonitor::EventMeter::EventMeter (Orientation o)
	: _orientation (o)
{
	_layout = Pango::Layout::create (get_pango_context ());
	memset (_chn, 0, sizeof (_chn));

	UIConfiguration::instance().DPIReset.connect (sigc::mem_fun (*this, &EventMeter::dpi_reset));
	dpi_reset ();
}

void
InputPortMonitor::EventMeter::dpi_reset ()
{
	_layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
	_layout->set_text ("Cy5");
	_layout->get_pixel_size (_length, _extent);
	_extent += 2;
	_length += 2;
	if (is_realized ()) {
		queue_resize ();
	}
}

void
InputPortMonitor::EventMeter::on_size_request (Gtk::Requisition* req)
{
	if (_orientation == Horizontal) {
		/* 90 deg CCW */
		req->width  = _extent * 17 + 4;
		req->height = _length + 2;
	} else {
		req->width  = _length + 2;
		req->height = _extent * 17 + 4;
	}
}

void
InputPortMonitor::EventMeter::clear ()
{
	memset (_chn, 0, sizeof (_chn));
	set_dirty ();
}

void
InputPortMonitor::EventMeter::update (float const* v)
{
	if (memcmp (_chn, v, sizeof (_chn))) {
		memcpy (_chn, v, sizeof (_chn));
		set_dirty ();
	}
}

void
InputPortMonitor::EventMeter::render (Cairo::RefPtr<Cairo::Context> const& cr, cairo_rectangle_t* r)
{
	cr->rectangle (r->x, r->y, r->width, r->height);
	cr->clip ();

	double bg_r, bg_g, bg_b, bg_a;
	double fg_r, fg_g, fg_b, fg_a;

	Gtkmm2ext::color_to_rgba (UIConfiguration::instance ().color ("meter bar"), bg_r, bg_g, bg_b, bg_a);
	Gtkmm2ext::color_to_rgba (UIConfiguration::instance ().color ("midi meter 56"), fg_r, fg_g, fg_b, fg_a);

	fg_r -= bg_r;
	fg_g -= bg_g;
	fg_b -= bg_b;

	cr->set_operator (Cairo::OPERATOR_OVER);
	cr->set_line_width (1.0);

	for (uint32_t i = 0; i < 17; ++i) {
		float off = 1.5 + _extent * i;

		if (_orientation == Horizontal) {
			Gtkmm2ext::rounded_rectangle (cr, off, 0.5, _extent, _length, boxy_buttons () ? 0 : 2);
		} else {
			Gtkmm2ext::rounded_rectangle (cr, 0.5, off, _length, _extent, boxy_buttons () ? 0 : 2);
		}

		cr->set_source_rgba (bg_r + _chn[i] * fg_r, bg_g + _chn[i] * fg_g, bg_b + _chn[i] * fg_b, .9);
		cr->fill_preserve ();
		Gtkmm2ext::set_source_rgba (cr, UIConfiguration::instance ().color ("border color"));
		cr->stroke ();

		if (i < 16) {
			_layout->set_text (PBD::to_string (i + 1));
		} else {
			_layout->set_text ("SyS");
		}

		int l, x;
		_layout->get_pixel_size (l, x);
		Gtkmm2ext::set_source_rgba (cr, UIConfiguration::instance ().color ("neutral:foreground2"));

		if (_orientation == Horizontal) {
			cr->save ();
			cr->move_to (off + .5 * (_extent - x), .5 + .5 * (_length + l));
			cr->rotate (M_PI / -2.0);
			_layout->show_in_cairo_context (cr);
			cr->restore ();
		} else {
			cr->move_to (0.5 + .5 * (_length - l), off + .5 * (_extent - x - 2));
			_layout->show_in_cairo_context (cr);
		}
	}
}

/* ****************************************************************************/

InputPortMonitor::EventMonitor::EventMonitor (Orientation o)
	: _orientation (o)
{
	_layout = Pango::Layout::create (get_pango_context ());

	UIConfiguration::instance().DPIReset.connect (sigc::mem_fun (*this, &EventMonitor::dpi_reset));
	dpi_reset ();
}

void
InputPortMonitor::EventMonitor::dpi_reset ()
{
	_layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
	_layout->set_text ("OffC#-1"); // 7 chars
	_layout->get_pixel_size (_width, _height);
	_width += 2;
	_height += 2;
	if (is_realized ()) {
		queue_resize ();
	}
}

void
InputPortMonitor::EventMonitor::on_size_request (Gtk::Requisition* req)
{
	if (_orientation == Horizontal) {
		req->width  = PX_SCALE (200);
		req->height = _height;
	} else {
		req->width  = _width;
		req->height = 8 * _height;
	}
}

void
InputPortMonitor::EventMonitor::clear ()
{
	_l.clear ();
	set_dirty ();
}

void
InputPortMonitor::EventMonitor::update (CircularEventBuffer& ceb)
{
	if (ceb.read (_l)) {
		set_dirty ();
	}
}

void
InputPortMonitor::EventMonitor::render (Cairo::RefPtr<Cairo::Context> const& cr, cairo_rectangle_t* r)
{
	int ww = get_width () - 12;
	int hh = 2;

	for (CircularEventBuffer::EventList::const_iterator i = _l.begin (); i != _l.end (); ++i) {
		if (i->data[0] == 0) {
			break;
		}
		char tmp[32];
		switch (i->data[0] & 0xf0) {
			case MIDI_CMD_NOTE_OFF:
				sprintf (tmp, "Off%4s", ParameterDescriptor::midi_note_name (i->data[1]).c_str ());
				break;
			case MIDI_CMD_NOTE_ON:
				sprintf (tmp, "On %4s", ParameterDescriptor::midi_note_name (i->data[1]).c_str ());
				break;
			case MIDI_CMD_NOTE_PRESSURE:
				sprintf (tmp, "KP %4s", ParameterDescriptor::midi_note_name (i->data[1]).c_str ());
				break;
			case MIDI_CMD_CONTROL:
				sprintf (tmp, "CC%02x %02x", i->data[1], i->data[2]);
				break;
			case MIDI_CMD_PGM_CHANGE:
				sprintf (tmp, "PC %3d ", i->data[1]);
				break;
			case MIDI_CMD_CHANNEL_PRESSURE:
				sprintf (tmp, "CP %02x  ", i->data[1]);
				break;
			case MIDI_CMD_BENDER:
				sprintf (tmp, "PB %04x", i->data[1] | i->data[2] << 7);
				break;
			case MIDI_CMD_COMMON_SYSEX:
				// TODO sub-type ?
				sprintf (tmp, " SysEx ");
				break;
		}

		int w, h;
		_layout->set_text (tmp);
		_layout->get_pixel_size (w, h);

		Gtkmm2ext::set_source_rgb_a (cr, UIConfiguration::instance ().color ("widget:bg"), .7);

		if (_orientation == Horizontal) {
			Gtkmm2ext::rounded_rectangle (cr, ww - w - 1, 1, 2 + w, _height - 3, _height / 4.0);
			cr->fill ();

			Gtkmm2ext::set_source_rgba (cr, UIConfiguration::instance ().color ("neutral:foreground2"));
			cr->move_to (ww - w, .5 * (_height - h));
			_layout->show_in_cairo_context (cr);

			ww -= w + 12;

			if (ww < w) {
				break;
			}
		} else {
			Gtkmm2ext::rounded_rectangle (cr, 1, hh + 1, _width, _height - 3, _height / 4.0);
			cr->fill ();

			Gtkmm2ext::set_source_rgba (cr, UIConfiguration::instance ().color ("neutral:foreground2"));
			cr->move_to (.5 * (_width - w), hh);
			_layout->show_in_cairo_context (cr);

			hh += _height;

			if (hh + h >= get_height ()) {
				break;
			}
		}
	}
}
