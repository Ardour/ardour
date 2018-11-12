/*
  Copyright (C) 2002 Paul Davis

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

#include <limits.h>

#include "ardour/meter.h"
#include "ardour/logmeter.h"

#include <gtkmm2ext/utils.h>
#include "pbd/fastlog.h"

#include "level_meter.h"
#include "utils.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "public_editor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace std;

LevelMeterBase::LevelMeterBase (Session* s, PBD::EventLoop::InvalidationRecord* ir, FastMeter::Orientation o)
	: parent_invalidator(ir)
	, _meter (0)
	, _meter_orientation(o)
	, regular_meter_width (6)
	, meter_length (0)
	, thin_meter_width(2)
	, max_peak (minus_infinity())
	, _meter_type (MeterPeak)
	, visible_meter_type (MeterType(0))
	, midi_count (0)
	, meter_count (0)
	, max_visible_meters (0)
	, color_changed (false)
{
	set_session (s);

	Config->ParameterChanged.connect (_parameter_connection, parent_invalidator, boost::bind (&LevelMeterBase::parameter_changed, this, _1), gui_context());
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun(*this, &LevelMeterBase::parameter_changed));
	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &LevelMeterBase::color_handler));
}

LevelMeterBase::~LevelMeterBase ()
{
	_configuration_connection.disconnect();
	_meter_type_connection.disconnect();
	_parameter_connection.disconnect();
	for (vector<MeterInfo>::iterator i = meters.begin(); i != meters.end(); i++) {
		delete (*i).meter;
	}
	meters.clear();
}

void
LevelMeterBase::set_meter (PeakMeter* meter)
{
	_configuration_connection.disconnect();
	_meter_type_connection.disconnect();

	_meter = meter;
	color_changed = true; // force update

	if (_meter) {
		_meter->ConfigurationChanged.connect (_configuration_connection, parent_invalidator, boost::bind (&LevelMeterBase::configuration_changed, this, _1, _2), gui_context());
		_meter->TypeChanged.connect (_meter_type_connection, parent_invalidator, boost::bind (&LevelMeterBase::meter_type_changed, this, _1), gui_context());
	}
}

static float meter_lineup_cfg(MeterLineUp lul, float offset) {
	switch (lul) {
		case MeteringLineUp24:
			return offset + 6.0;
		case MeteringLineUp20:
			return offset + 2.0;
		case MeteringLineUp18:
			return offset;
		case MeteringLineUp15:
			return offset - 3.0;
		default:
			break;
	}
	return offset;
}

static float meter_lineup(float offset) {
	return meter_lineup_cfg (UIConfiguration::instance().get_meter_line_up_level(), offset);
}

static float vu_standard() {
	// note - default meter config is +2dB (france)
	switch (UIConfiguration::instance().get_meter_vu_standard()) {
		default:
		case MeteringVUfrench:   // 0VU = -2dBu
			return 0;
		case MeteringVUamerican: // 0VU =  0dBu
			return -2;
		case MeteringVUstandard: // 0VU = +4dBu
			return -6;
		case MeteringVUeight:    // 0VU = +8dBu
			return -10;
	}
}

float
LevelMeterBase::update_meters ()
{
	vector<MeterInfo>::iterator i;
	uint32_t n;

	if (!_meter) {
		return 0.0f;
	}

	uint32_t nmidi = _meter->input_streams().n_midi();

	for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
		if ((*i).packed) {
			const float mpeak = _meter->meter_level(n, MeterMaxPeak);
			if (mpeak > (*i).max_peak) {
				(*i).max_peak = mpeak;
				(*i).meter->set_highlight(mpeak >= UIConfiguration::instance().get_meter_peak());
			}
			if (mpeak > max_peak) {
				max_peak = mpeak;
			}

			if (n < nmidi) {
				(*i).meter->set (_meter->meter_level (n, MeterPeak));
			} else {
				const float peak = _meter->meter_level (n, _meter_type);
				if (_meter_type == MeterPeak) {
					(*i).meter->set (log_meter (peak));
				} else if (_meter_type == MeterPeak0dB) {
					(*i).meter->set (log_meter0dB (peak));
				} else if (_meter_type == MeterIEC1NOR) {
					(*i).meter->set (meter_deflect_nordic (peak + meter_lineup(0)));
				} else if (_meter_type == MeterIEC1DIN) {
					(*i).meter->set (meter_deflect_din (peak + meter_lineup_cfg(UIConfiguration::instance().get_meter_line_up_din(), 3.0)));
				} else if (_meter_type == MeterIEC2BBC || _meter_type == MeterIEC2EBU) {
					(*i).meter->set (meter_deflect_ppm (peak + meter_lineup(0)));
				} else if (_meter_type == MeterVU) {
					(*i).meter->set (meter_deflect_vu (peak + vu_standard() + meter_lineup(0)));
				} else if (_meter_type == MeterK12) {
					(*i).meter->set (meter_deflect_k (peak, 12), meter_deflect_k(_meter->meter_level(n, MeterPeak), 12));
				} else if (_meter_type == MeterK14) {
					(*i).meter->set (meter_deflect_k (peak, 14), meter_deflect_k(_meter->meter_level(n, MeterPeak), 14));
				} else if (_meter_type == MeterK20) {
					(*i).meter->set (meter_deflect_k (peak, 20), meter_deflect_k(_meter->meter_level(n, MeterPeak), 20));
				} else { // RMS
					(*i).meter->set (log_meter (peak), log_meter(_meter->meter_level(n, MeterPeak)));
				}
			}
		}
	}
	return max_peak;
}

void
LevelMeterBase::parameter_changed (string p)
{
	ENSURE_GUI_THREAD (*this, &LevelMeterBase::parameter_changed, p)

	if (p == "meter-hold") {
		vector<MeterInfo>::iterator i;
		uint32_t n;

		for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
			(*i).meter->set_hold_count ((uint32_t) floor(UIConfiguration::instance().get_meter_hold()));
		}
	}
	else if (p == "meter-line-up-level") {
		color_changed = true;
		setup_meters (meter_length, regular_meter_width, thin_meter_width);
	}
	else if (p == "meter-style-led") {
		color_changed = true;
		setup_meters (meter_length, regular_meter_width, thin_meter_width);
	}
	else if (p == "meter-peak") {
		vector<MeterInfo>::iterator i;
		uint32_t n;

		for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
			(*i).max_peak = minus_infinity();
		}
	}
}

void
LevelMeterBase::configuration_changed (ChanCount /*in*/, ChanCount /*out*/)
{
	setup_meters (meter_length, regular_meter_width, thin_meter_width);
}

void
LevelMeterBase::meter_type_changed (MeterType t)
{
	_meter_type = t;
	setup_meters (meter_length, regular_meter_width, thin_meter_width);
	MeterTypeChanged(t);
}

void
LevelMeterBase::hide_all_meters ()
{
	for (vector<MeterInfo>::iterator i = meters.begin(); i != meters.end(); ++i) {
		if ((*i).packed) {
			mtr_remove (*((*i).meter));
			(*i).packed = false;
		}
	}
	meter_count = 0;
}

void
LevelMeterBase::set_max_audio_meter_count (uint32_t cnt)
{
	if (cnt == max_visible_meters) {
		return;
	}
	color_changed = true; // force re-setup
	max_visible_meters = cnt;
	setup_meters (meter_length, regular_meter_width, thin_meter_width);
}

void
LevelMeterBase::setup_meters (int len, int initial_width, int thin_width)
{

 	if (!_meter) {
		hide_all_meters ();
 		return; /* do it later or never */
 	}

	uint32_t nmidi = _meter->input_streams().n_midi();
	uint32_t nmeters = _meter->input_streams().n_total();
	regular_meter_width = initial_width;
	thin_meter_width = thin_width;
	meter_length = len;

	guint16 width;

	if (nmeters == 0) {
		hide_all_meters ();
		return;
	}

	if (nmeters <= 2) {
		width = regular_meter_width;
	} else {
		width = thin_meter_width;
	}

	width = rint (width * UIConfiguration::instance().get_ui_scale());

	if (   meters.size() > 0
	    && nmidi == midi_count
	    && nmeters == meter_count
	    && meters[0].width == width
	    && meters[0].length == len
	    && !color_changed
	    && _meter_type == visible_meter_type) {
		return;
	}

#if 0
	printf("Meter redraw: %s %s %s %s %s %s\n",
			(meters.size() > 0) ? "yes" : "no",
			(meters.size() > 0 &&  meters[0].width == width) ? "yes" : "no",
			(meters.size() > 0 &&  meters[0].length == len) ? "yes" : "no",
			(nmeters == meter_count) ? "yes" : "no",
			(_meter_type == visible_meter_type) ? "yes" : "no",
			!color_changed ? "yes" : "no"
			);
#endif

	hide_all_meters ();
	while (meters.size() < nmeters) {
		meters.push_back (MeterInfo());
	}

	//cerr << "LevelMeterBase::setup_meters() called color_changed = " << color_changed << " colors: " << endl;//DEBUG

	for (int32_t n = nmeters-1; nmeters && n >= 0 ; --n) {
		uint32_t c[10];
		uint32_t b[4];
		float stp[4];
		int styleflags = UIConfiguration::instance().get_meter_style_led() ? 3 : 1;
		b[0] = UIConfiguration::instance().color ("meter background bottom");
		b[1] = UIConfiguration::instance().color ("meter background top");
		b[2] = 0x991122ff; // red highlight gradient Bot
		b[3] = 0x551111ff; // red highlight gradient Top
		if ((uint32_t) n < nmidi) {
			c[0] = UIConfiguration::instance().color ("midi meter color0");
			c[1] = UIConfiguration::instance().color ("midi meter color1");
			c[2] = UIConfiguration::instance().color ("midi meter color2");
			c[3] = UIConfiguration::instance().color ("midi meter color3");
			c[4] = UIConfiguration::instance().color ("midi meter color4");
			c[5] = UIConfiguration::instance().color ("midi meter color5");
			c[6] = UIConfiguration::instance().color ("midi meter color6");
			c[7] = UIConfiguration::instance().color ("midi meter color7");
			c[8] = UIConfiguration::instance().color ("midi meter color8");
			c[9] = UIConfiguration::instance().color ("midi meter color9");
			stp[0] = 115.0 *  32.0 / 128.0;
			stp[1] = 115.0 *  64.0 / 128.0;
			stp[2] = 115.0 * 100.0 / 128.0;
			stp[3] = 115.0 * 112.0 / 128.0;
		} else {
			c[0] = UIConfiguration::instance().color ("meter color0");
			c[1] = UIConfiguration::instance().color ("meter color1");
			c[2] = UIConfiguration::instance().color ("meter color2");
			c[3] = UIConfiguration::instance().color ("meter color3");
			c[4] = UIConfiguration::instance().color ("meter color4");
			c[5] = UIConfiguration::instance().color ("meter color5");
			c[6] = UIConfiguration::instance().color ("meter color6");
			c[7] = UIConfiguration::instance().color ("meter color7");
			c[8] = UIConfiguration::instance().color ("meter color8");
			c[9] = UIConfiguration::instance().color ("meter color9");

			switch (_meter_type) {
				case MeterK20:
					stp[0] = 115.0 * meter_deflect_k(-40, 20);  //-20
					stp[1] = 115.0 * meter_deflect_k(-20, 20);  //  0
					stp[2] = 115.0 * meter_deflect_k(-18, 20);  // +2
					stp[3] = 115.0 * meter_deflect_k(-16, 20);  // +4
					c[0] = c[1] = 0x008800ff;
					c[2] = c[3] = 0x00ff00ff;
					c[4] = c[5] = 0xffff00ff;
					c[6] = c[7] = 0xffff00ff;
					c[8] = c[9] = 0xff0000ff;
					break;
				case MeterK14:
					stp[0] = 115.0 * meter_deflect_k(-34, 14);  //-20
					stp[1] = 115.0 * meter_deflect_k(-14, 14);  //  0
					stp[2] = 115.0 * meter_deflect_k(-12, 14);  // +2
					stp[3] = 115.0 * meter_deflect_k(-10, 14);  // +4
					c[0] = c[1] = 0x008800ff;
					c[2] = c[3] = 0x00ff00ff;
					c[4] = c[5] = 0xffff00ff;
					c[6] = c[7] = 0xffff00ff;
					c[8] = c[9] = 0xff0000ff;
					break;
				case MeterK12:
					stp[0] = 115.0 * meter_deflect_k(-32, 12);  //-20
					stp[1] = 115.0 * meter_deflect_k(-12, 12);  //  0
					stp[2] = 115.0 * meter_deflect_k(-10, 12);  // +2
					stp[3] = 115.0 * meter_deflect_k( -8, 12);  // +4
					c[0] = c[1] = 0x008800ff;
					c[2] = c[3] = 0x00ff00ff;
					c[4] = c[5] = 0xffff00ff;
					c[6] = c[7] = 0xffff00ff;
					c[8] = c[9] = 0xff0000ff;
					break;
				case MeterIEC2BBC:
					c[0] = c[1] = c[2] = c[3] = c[4] = c[5] = c[6] = c[7] = c[8] = c[9] =
						UIConfiguration::instance().color ("meter color BBC");
					stp[0] = stp[1] = stp[2] = stp[3] = 115.0;
					break;
				case MeterIEC2EBU:
					stp[0] = 115.0 * meter_deflect_ppm(-24); // ignored
					stp[1] = 115.0 * meter_deflect_ppm(-18);
					stp[2] = 115.0 * meter_deflect_ppm( -9);
					stp[3] = 115.0 * meter_deflect_ppm(  0); // ignored
					c[3] = c[2] = c[1];
					c[6] = c[7] = c[8] = c[9];
					break;
				case MeterIEC1NOR:
					stp[0] = 115.0 * meter_deflect_nordic(-30); // ignored
					stp[1] = 115.0 * meter_deflect_nordic(-18);
					stp[2] = 115.0 * meter_deflect_nordic(-12);
					stp[3] = 115.0 * meter_deflect_nordic( -9); // ignored
					c[0] = c[1] = c[2]; // bright-green
					c[6] = c[7] = c[8] = c[9];
					break;
				case MeterIEC1DIN:
					stp[0] = 115.0 * meter_deflect_din(-29); // ignored
					stp[1] = 115.0 * meter_deflect_din(-18);
					stp[2] = 115.0 * meter_deflect_din(-15); // ignored
					stp[3] = 115.0 * meter_deflect_din( -9);
					c[0] = c[2] = c[3] = c[1];
					c[4] = c[6];
					c[5] = c[7];
					break;
				case MeterVU:
					stp[0] = 115.0 * meter_deflect_vu(-26); // -6
					stp[1] = 115.0 * meter_deflect_vu(-23); // -3
					stp[2] = 115.0 * meter_deflect_vu(-20); // 0
					stp[3] = 115.0 * meter_deflect_vu(-18); // +2
					c[0] = c[2] = c[3] = c[4] = c[5] = c[1];
					c[7] = c[8] = c[9] = c[6];
					break;
				case MeterPeak0dB:
					 stp[1] =  89.125; // 115.0 * log_meter0dB(-9);
					 stp[2] = 106.375; // 115.0 * log_meter0dB(-3);
					 stp[3] = 115.0;   // 115.0 * log_meter0dB(0);
					switch (UIConfiguration::instance().get_meter_line_up_level()) {
					case MeteringLineUp24:
						stp[0] = 115.0 * log_meter0dB(-24);
						break;
					case MeteringLineUp20:
						stp[0] = 115.0 * log_meter0dB(-20);
						break;
					default:
					case MeteringLineUp18:
						stp[0] = 115.0 * log_meter0dB(-18);
						break;
					case MeteringLineUp15:
						stp[0] = 115.0 * log_meter0dB(-15);
					}
					break;
				default: // PEAK, RMS
					stp[1] = 77.5;  // 115 * log_meter(-9)
					stp[2] = 92.5;  // 115 * log_meter(-3)
					stp[3] = 100.0; // 115 * log_meter(0)
					switch (UIConfiguration::instance().get_meter_line_up_level()) {
					case MeteringLineUp24:
						stp[0] = 42.0;
						break;
					case MeteringLineUp20:
						stp[0] = 50.0;
						break;
					default:
					case MeteringLineUp18:
						stp[0] = 55.0;
						break;
					case MeteringLineUp15:
						stp[0] = 62.5;
						break;
				}
			}
		}
		if (meters[n].width != width || meters[n].length != len || color_changed || _meter_type != visible_meter_type || nmidi != midi_count) {
			bool hl = meters[n].meter ? meters[n].meter->get_highlight() : false;
			meters[n].packed = false;
			delete meters[n].meter;
			meters[n].meter = new FastMeter ((uint32_t) floor (UIConfiguration::instance().get_meter_hold()), width, _meter_orientation, len,
					c[0], c[1], c[2], c[3], c[4],
					c[5], c[6], c[7], c[8], c[9],
					b[0], b[1], b[2], b[3],
					stp[0], stp[1], stp[2], stp[3],
					styleflags
					);
			meters[n].meter->set_highlight(hl);
			meters[n].width = width;
			meters[n].length = len;
			meters[n].meter->add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
			meters[n].meter->signal_button_press_event().connect (sigc::mem_fun (*this, &LevelMeterBase::meter_button_press), false);
			meters[n].meter->signal_button_release_event().connect (sigc::mem_fun (*this, &LevelMeterBase::meter_button_release), false);
		}

		//pack_end (*meters[n].meter, false, false);
		mtr_pack (*meters[n].meter);
		meters[n].packed = true;
		if (max_visible_meters == 0 || (uint32_t) n < max_visible_meters + nmidi) {
			meters[n].meter->show_all ();
		} else {
			meters[n].meter->hide ();
		}
	}
	//show();
	color_changed = false;
	visible_meter_type = _meter_type;
	midi_count = nmidi;
	meter_count = nmeters;
}

void
LevelMeterBase::set_meter_type(MeterType t)
{
	_meter_type = t;
	_meter->set_type(t);
}

bool
LevelMeterBase::meter_button_press (GdkEventButton* ev)
{
	return static_cast<bool>(ButtonPress (ev)); /* EMIT SIGNAL */
}

bool
LevelMeterBase::meter_button_release (GdkEventButton* ev)
{
	if (ev->button == 1) {
		clear_meters (false);
	}
	ButtonRelease(ev);

	return true;
}


void LevelMeterBase::clear_meters (bool reset_highlight)
{
	for (vector<MeterInfo>::iterator i = meters.begin(); i < meters.end(); i++) {
		(*i).meter->clear();
		(*i).max_peak = minus_infinity();
		if (reset_highlight)
			(*i).meter->set_highlight(false);
	}
	max_peak = minus_infinity();
}

void LevelMeterBase::hide_meters ()
{
	hide_all_meters();
}

void
LevelMeterBase::color_handler ()
{
	color_changed = true;
	setup_meters (meter_length, regular_meter_width, thin_meter_width);
}

LevelMeterHBox::LevelMeterHBox(Session* s)
	: LevelMeterBase(s, invalidator(*this))
{
	set_spacing(1);
	show();
}


LevelMeterHBox::~LevelMeterHBox() {}

void
LevelMeterHBox::mtr_pack(Gtk::Widget &w) {
	pack_end (w, false, false);
}

void
LevelMeterHBox::mtr_remove(Gtk::Widget &w) {
	remove (w);
}


LevelMeterVBox::LevelMeterVBox(Session* s)
	: LevelMeterBase(s, invalidator(*this), FastMeter::Horizontal)
{
	set_spacing(1);
	show();
}
LevelMeterVBox::~LevelMeterVBox() {}

void
LevelMeterVBox::mtr_pack(Gtk::Widget &w) {
	pack_end (w, false, false);
}

void
LevelMeterVBox::mtr_remove(Gtk::Widget &w) {
	remove (w);
}
