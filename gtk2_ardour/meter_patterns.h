/*
    Copyright (C) 2013 Paul Davis
    Author: Robin Gareus

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

#ifndef __ardour_meter_patterns__
#define __ardour_meter_patterns__

#include <list>
#include <vector>

#include "ardour/types.h"
#include "gtkmm2ext/cairo_widget.h"

#include <sigc++/signal.h>

namespace ArdourMeter {

extern sigc::signal<void> ResetAllPeakDisplays;
extern sigc::signal<void,ARDOUR::Route*> ResetRoutePeakDisplays;
extern sigc::signal<void,ARDOUR::RouteGroup*> ResetGroupPeakDisplays;
extern sigc::signal<void> RedrawMetrics;

extern sigc::signal<void, int, ARDOUR::RouteGroup*, ARDOUR::MeterType> SetMeterTypeMulti;

gint meter_expose_ticks (GdkEventExpose *ev, std::vector<ARDOUR::DataType> types, Gtk::DrawingArea *mta);
gint meter_expose_metrics (GdkEventExpose *ev, std::vector<ARDOUR::DataType> types, Gtk::DrawingArea *mma);

void meter_clear_pattern_cache(int which=7);

const std::string meter_type_string (ARDOUR::MeterType);

}

#endif

