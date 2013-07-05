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

#ifndef __ardour_meter_strip__
#define __ardour_meter_strip__

#include <vector>

#include <cmath>

#include "pbd/stateful.h"

#include "ardour/types.h"
#include "ardour/ardour.h"
#include "route_ui.h"
#include "ardour_button.h"

#include "level_meter.h"

namespace ARDOUR {
	class Route;
	class RouteGroup;
	class Session;
}
namespace Gtk {
	class Window;
	class Style;
}

class MeterStrip : public Gtk::VBox, public RouteUI
{
  public:
	MeterStrip (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);
	MeterStrip (int);
	~MeterStrip ();

	void fast_update ();
	boost::shared_ptr<ARDOUR::Route> route() { return _route; }

	static PBD::Signal1<void,MeterStrip*> CatchDeletion;
	static PBD::Signal0<void> MetricChanged;

	void reset_peak_display ();
	void reset_route_peak_display (ARDOUR::Route*);
	void reset_group_peak_display (ARDOUR::RouteGroup*);

	void set_metric_mode (int);
	bool has_midi() { return _has_midi; }

  protected:
	boost::shared_ptr<ARDOUR::Route> _route;
	PBD::ScopedConnectionList route_connections;
	void self_delete ();

	gint meter_metrics_expose (GdkEventExpose *);
	gint meter_ticks1_expose (GdkEventExpose *);
	gint meter_ticks2_expose (GdkEventExpose *);

	void on_theme_changed ();

	void on_size_allocate (Gtk::Allocation&);
	void on_size_request (Gtk::Requisition*);

	/* route UI */
	void update_rec_display ();
	std::string state_id() const;
	void set_button_names ();

  private:
	Gtk::HBox meterbox;
	Gtk::HBox namebx;
	ArdourButton name_label;
	Gtk::DrawingArea meter_metric_area;
	Gtk::DrawingArea meter_ticks1_area;
	Gtk::DrawingArea meter_ticks2_area;

	Gtk::Alignment meter_align;
	Gtk::Alignment peak_align;
	Gtk::HBox peakbx;
	Gtk::HBox btnbox;
	ArdourButton peak_display;

	std::vector<ARDOUR::DataType> _types;

	float max_peak;
	bool _has_midi;

	LevelMeter   *level_meter;
	void meter_changed ();

	PBD::ScopedConnection _config_connection;
	void strip_property_changed (const PBD::PropertyChange&);
	void meter_configuration_changed (ARDOUR::ChanCount);

	static int max_pattern_metric_size; // == FastMeter::max_pattern_metric_size

	bool peak_button_release (GdkEventButton*);

	void redraw_metrics ();
};

#endif /* __ardour_mixer_strip__ */
