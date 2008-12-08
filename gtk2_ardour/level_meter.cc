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

#include <ardour/io.h>
#include <ardour/route.h>
#include <ardour/route_group.h>
#include <ardour/session.h>
#include <ardour/session_route.h>
#include <ardour/dB.h>
#include <ardour/meter.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/barcontroller.h>
#include <midi++/manager.h>
#include <pbd/fastlog.h>

#include "ardour_ui.h"
#include "level_meter.h"
#include "utils.h"
#include "logmeter.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "public_editor.h"

#include <ardour/session.h>
#include <ardour/route.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace sigc;
using namespace std;

//sigc::signal<void> LevelMeter::ResetAllPeakDisplays;
//sigc::signal<void,RouteGroup*> LevelMeter::ResetGroupPeakDisplays;


LevelMeter::LevelMeter (Session& s)
	: _session (s)
	
{
	set_spacing (1);
	Config->ParameterChanged.connect (mem_fun (*this, &LevelMeter::parameter_changed));
	UI::instance()->theme_changed.connect (mem_fun(*this, &LevelMeter::on_theme_changed));
	ColorsChanged.connect (mem_fun (*this, &LevelMeter::color_handler));
	max_peak = minus_infinity();
}

void
LevelMeter::on_theme_changed()
{
	style_changed = true;
}

LevelMeter::~LevelMeter ()
{
	for (vector<MeterInfo>::iterator i = meters.begin(); i != meters.end(); i++) {
		if ((*i).meter) {
			delete (*i).meter;
		}
	}
}

void
LevelMeter::set_io (boost::shared_ptr<IO> io)
{
	_io = io;
}

float
LevelMeter::update_meters ()
{
	vector<MeterInfo>::iterator i;
	uint32_t n;
	float peak, mpeak;
	
	for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
		if ((*i).packed) {
			peak = _io->peak_meter().peak_power (n);
			(*i).meter->set (log_meter (peak));
			mpeak = _io->peak_meter().max_peak_power(n);
			if (mpeak > max_peak) {
				max_peak = mpeak;
			}
			if (mpeak > max_peak) {
				max_peak = mpeak;
			}
		}
	}
	return max_peak;
}

void
LevelMeter::parameter_changed(const char* parameter_name)
{
#define PARAM_IS(x) (!strcmp (parameter_name, (x)))

	ENSURE_GUI_THREAD (bind (mem_fun(*this, &LevelMeter::parameter_changed), parameter_name));

	if (PARAM_IS ("meter-hold")) {
	
		vector<MeterInfo>::iterator i;
		uint32_t n;
		
		for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
			
			(*i).meter->set_hold_count ((uint32_t) floor(Config->get_meter_hold()));
		}
	}

#undef PARAM_IS
}

void
LevelMeter::hide_all_meters ()
{

	for (vector<MeterInfo>::iterator i = meters.begin(); i != meters.end(); ++i) {
		if ((*i).packed) {
			remove (*((*i).meter));
			(*i).packed = false;
		}
	}
}

void
LevelMeter::setup_meters (int len, int initial_width)
{
 	if (!_io) {
 		return; /* do it later */
 	}
 
	uint32_t nmeters = _io->n_outputs().n_total();
	regular_meter_width = initial_width;

	guint16 width;

	hide_all_meters ();

	Route* r;

	if ((r = dynamic_cast<Route*> (_io.get())) != 0) {

		switch (r->meter_point()) {
		case MeterPreFader:
		case MeterInput:
			nmeters = r->n_inputs().n_total();
			break;
		case MeterPostFader:
			nmeters = r->n_outputs().n_total();
			break;
		}

	} else {

		nmeters = _io->n_outputs().n_total();

	}

	if (nmeters == 0) {
		return;
	}

	if (nmeters <= 2) {
		width = regular_meter_width;
	} else {
		width = thin_meter_width;
	}

	while (meters.size() < nmeters) {
		meters.push_back (MeterInfo());
	}


	int b = ARDOUR_UI::config()->canvasvar_MeterColorBase.get();
	int m = ARDOUR_UI::config()->canvasvar_MeterColorMid.get();
	int t = ARDOUR_UI::config()->canvasvar_MeterColorTop.get();
	int c = ARDOUR_UI::config()->canvasvar_MeterColorClip.get();

	//cerr << "LevelMeter::setup_meters() called color_changed = " << color_changed << " colors: " << endl;//DEBUG

	for (int32_t n = nmeters-1; nmeters && n >= 0 ; --n) {
		if (meters[n].width != width || meters[n].length != len || color_changed) {
			delete meters[n].meter;
			meters[n].meter = new FastMeter ((uint32_t) floor (Config->get_meter_hold()), width, FastMeter::Vertical, len, b, m, t, c);
			//cerr << "LevelMeter::setup_meters() w:l = " << width << ":" << len << endl;//DEBUG
			meters[n].width = width;
			meters[n].length = len;
			meters[n].meter->add_events (Gdk::BUTTON_RELEASE_MASK);
			meters[n].meter->signal_button_release_event().connect (bind (mem_fun(*this, &LevelMeter::meter_button_release), n));
		}

		pack_end (*meters[n].meter, false, false);
		meters[n].meter->show_all ();
		meters[n].packed = true;
	}
	show();
	color_changed = false;
}

gint
LevelMeter::meter_button_release (GdkEventButton* ev, uint32_t which)
{
	if (ev->button == 1) {
		clear_meters();
	}
	return true;
}
	

void LevelMeter::clear_meters ()
{
	for (vector<MeterInfo>::iterator i = meters.begin(); i < meters.end(); i++) {
		(*i).meter->clear();
	}
	max_peak = minus_infinity();
}

void LevelMeter::hide_meters ()
{
	hide_all_meters();
}

void
LevelMeter::color_handler ()
{
	color_changed = true;
}

