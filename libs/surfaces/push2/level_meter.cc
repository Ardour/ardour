/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#include <limits.h>

#include "ardour/meter.h"
#include "ardour/logmeter.h"
#include "ardour/rc_configuration.h"

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/gui_thread.h>

#include "pbd/fastlog.h"
#include "pbd/i18n.h"

#include "canvas/box.h"
#include "canvas/meter.h"

#include "level_meter.h"
#include "push2.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace ArdourSurface;
using namespace ArdourCanvas;

LevelMeter::LevelMeter (Push2& p, Item* parent, int len, Meter::Orientation o)
	: Container (parent)
	, _p2 (p)
	, _meter (0)
	, _meter_orientation(o)
	, _regular_meter_width (6)
	, _meter_length (len)
	, _thin_meter_width(2)
	, _max_peak (minus_infinity())
	, _visible_meter_type (MeterType(0))
	, _midi_count (0)
	, _meter_count (0)
	, _max_visible_meters (0)
{
	Config->ParameterChanged.connect (_parameter_connection, invalidator(*this), std::bind (&LevelMeter::parameter_changed, this, _1), &_p2);

	if (_meter_orientation == Meter::Vertical) {
		_meter_packer = new HBox (this);
	} else {
		_meter_packer = new VBox (this);
	}

	_meter_packer->set_collapse_on_hide (true);
}

LevelMeter::~LevelMeter ()
{
	_configuration_connection.disconnect();
	_meter_type_connection.disconnect();
	_parameter_connection.disconnect();
	for (std::vector<MeterInfo>::iterator i = _meters.begin(); i != _meters.end(); ++i) {
		delete (*i).meter;
	}
	_meters.clear();
}

void
LevelMeter::set_meter (PeakMeter* meter)
{
	_configuration_connection.disconnect();
	_meter_type_connection.disconnect();

	_meter = meter;

	if (_meter) {
		_meter->ConfigurationChanged.connect (_configuration_connection, invalidator(*this), std::bind (&LevelMeter::configuration_changed, this, _1, _2), &_p2);
		_meter->MeterTypeChanged.connect (_meter_type_connection, invalidator (*this), std::bind (&LevelMeter::meter_type_changed, this, _1), &_p2);
	}

	setup_meters (_meter_length, _regular_meter_width, _thin_meter_width);
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
	return meter_lineup_cfg (MeteringLineUp24, offset);
	//return meter_lineup_cfg (UIConfiguration::instance().get_meter_line_up_level(), offset);
}

static float vu_standard() {
	return 0;
#if 0
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
#endif
}

float
LevelMeter::update_meters ()
{
	std::vector<MeterInfo>::iterator i;
	uint32_t n;

	if (!_meter) {
		return 0.0f;
	}

	uint32_t nmidi = _meter->input_streams().n_midi();

	for (n = 0, i = _meters.begin(); i != _meters.end(); ++i, ++n) {
		if ((*i).packed) {
			const float mpeak = _meter->meter_level(n, MeterMaxPeak);
			if (mpeak > (*i).max_peak) {
				(*i).max_peak = mpeak;
				//(*i).meter->set_highlight(mpeak >= UIConfiguration::instance().get_meter_peak());
				(*i).meter->set_highlight (mpeak >= 2.0);
			}
			if (mpeak > _max_peak) {
				_max_peak = mpeak;
			}

			if (n < nmidi) {
				(*i).meter->set (_meter->meter_level (n, MeterPeak));
			} else {
				MeterType meter_type = _meter->meter_type ();
				const float peak = _meter->meter_level (n, meter_type);
				if (meter_type == MeterPeak) {
					(*i).meter->set (log_meter (peak));
				} else if (meter_type == MeterPeak0dB) {
					(*i).meter->set (log_meter0dB (peak));
				} else if (meter_type == MeterIEC1NOR) {
					(*i).meter->set (meter_deflect_nordic (peak + meter_lineup(0)));
				} else if (meter_type == MeterIEC1DIN) {
					// (*i).meter->set (meter_deflect_din (peak + meter_lineup_cfg(UIConfiguration::instance().get_meter_line_up_din(), 3.0)));
				} else if (meter_type == MeterIEC2BBC || meter_type == MeterIEC2EBU) {
					(*i).meter->set (meter_deflect_ppm (peak + meter_lineup(0)));
				} else if (meter_type == MeterVU) {
					(*i).meter->set (meter_deflect_vu (peak + vu_standard() + meter_lineup(0)));
				} else if (meter_type == MeterK12) {
					(*i).meter->set (meter_deflect_k (peak, 12), meter_deflect_k(_meter->meter_level(n, MeterPeak), 12));
				} else if (meter_type == MeterK14) {
					(*i).meter->set (meter_deflect_k (peak, 14), meter_deflect_k(_meter->meter_level(n, MeterPeak), 14));
				} else if (meter_type == MeterK20) {
					(*i).meter->set (meter_deflect_k (peak, 20), meter_deflect_k(_meter->meter_level(n, MeterPeak), 20));
				} else { // RMS
					(*i).meter->set (log_meter (peak), log_meter(_meter->meter_level(n, MeterPeak)));
				}
			}
		}
	}

	return _max_peak;
}

void
LevelMeter::parameter_changed (std::string p)
{
	if (p == "meter-hold") {
		std::vector<MeterInfo>::iterator i;
		uint32_t n;

		for (n = 0, i = _meters.begin(); i != _meters.end(); ++i, ++n) {
			//(*i).meter->set_hold_count ((uint32_t) floor(UIConfiguration::instance().get_meter_hold()));
			(*i).meter->set_hold_count (20);
		}
	}
	else if (p == "meter-line-up-level") {
		setup_meters (_meter_length, _regular_meter_width, _thin_meter_width);
	}
	else if (p == "meter-style-led") {
		setup_meters (_meter_length, _regular_meter_width, _thin_meter_width);
	}
	else if (p == "meter-peak") {
		std::vector<MeterInfo>::iterator i;
		uint32_t n;

		for (n = 0, i = _meters.begin(); i != _meters.end(); ++i, ++n) {
			(*i).max_peak = minus_infinity();
		}
	}
}

void
LevelMeter::configuration_changed (ChanCount /*in*/, ChanCount /*out*/)
{
	setup_meters (_meter_length, _regular_meter_width, _thin_meter_width);
}

void
LevelMeter::meter_type_changed (MeterType t)
{
	setup_meters (_meter_length, _regular_meter_width, _thin_meter_width);
}

void
LevelMeter::hide_all_meters ()
{
	for (std::vector<MeterInfo>::iterator i = _meters.begin(); i != _meters.end(); ++i) {
		if ((*i).packed) {
			_meter_packer->remove ((*i).meter);
			(*i).packed = false;
		}
	}
	_meter_count = 0;
}

void
LevelMeter::set_max_audio_meter_count (uint32_t cnt)
{
	if (cnt == _max_visible_meters) {
		return;
	}

	_max_visible_meters = cnt;
	setup_meters (_meter_length, _regular_meter_width, _thin_meter_width);
}

void
LevelMeter::setup_meters (int len, int initial_width, int thin_width)
{
	if (!_meter) {
		hide_all_meters ();
		return; /* do it later or never */
	}

	MeterType meter_type = _meter->meter_type ();
	uint32_t nmidi = _meter->input_streams().n_midi();
	uint32_t nmeters = _meter->input_streams().n_total();
	_regular_meter_width = initial_width;
	_thin_meter_width = thin_width;
	_meter_length = len;

	guint16 width;

	if (nmeters == 0) {
		hide_all_meters ();
		return;
	}

	if (nmeters <= 2) {
		width = _regular_meter_width;
	} else {
		width = _thin_meter_width;
	}

	if (_meters.size() > 0
	    && nmidi == _midi_count
	    && nmeters == _meter_count
	    && _meters[0].width == width
	    && _meters[0].length == len
	    && meter_type == _visible_meter_type) {
		return;
	}

	double total_width = 0.;
	double total_height = 0.;

#if 0
	printf("Meter redraw: %s %s %s %s %s %s\n",
			(meters.size() > 0) ? "yes" : "no",
			(meters.size() > 0 &&  meters[0].width == width) ? "yes" : "no",
			(meters.size() > 0 &&  meters[0].length == len) ? "yes" : "no",
			(nmeters == meter_count) ? "yes" : "no",
			(meter_type == visible_meter_type) ? "yes" : "no",
			!color_changed ? "yes" : "no"
			);
#endif

	hide_all_meters ();
	while (_meters.size() < nmeters) {
		_meters.push_back (MeterInfo());
	}

	for (int32_t n = nmeters-1; nmeters && n >= 0 ; --n) {
#if 0
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

			switch (meter_type) {
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

#endif
		if (_meters[n].width != width || _meters[n].length != len || meter_type != _visible_meter_type || nmidi != _midi_count) {
			bool hl = _meters[n].meter ? _meters[n].meter->get_highlight() : false;
			_meters[n].packed = false;
			delete _meters[n].meter;
			_meters[n].meter = new Meter (this->canvas(), 32, width, _meter_orientation, len);
			_meters[n].meter->set_highlight(hl);
			_meters[n].width = width;
			_meters[n].length = len;
		}

		_meter_packer->add (_meters[n].meter);
		_meters[n].packed = true;

		total_width += _meters[n].width;
		total_height += _meters[n].length;

		if (_max_visible_meters == 0 || (uint32_t) n < _max_visible_meters + nmidi) {
			_meters[n].meter->show ();
		} else {
			_meters[n].meter->hide ();
		}
	}

	_meter_packer->set (ArdourCanvas::Rect (0., 0., total_width, total_height));
	_meter_packer->size_allocate (_meter_packer->get());

	_visible_meter_type = meter_type;
	_midi_count = nmidi;
	_meter_count = nmeters;
}

void LevelMeter::clear_meters (bool reset_highlight)
{
	for (std::vector<MeterInfo>::iterator i = _meters.begin(); i < _meters.end(); ++i) {
		(*i).meter->clear();
		(*i).max_peak = minus_infinity();
		if (reset_highlight)
			(*i).meter->set_highlight(false);
	}
	_max_peak = minus_infinity();
}

void LevelMeter::hide_meters ()
{
	hide_all_meters();
}
