/*
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_meter_strip__
#define __ardour_meter_strip__

#include <vector>
#include <cmath>

#include <gtkmm/alignment.h>
#include <gtkmm/box.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/separator.h>

#include "pbd/stateful.h"

#include "ardour/types.h"
#include "ardour/ardour.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_knob.h"

#include "level_meter.h"
#include "route_ui.h"

namespace ARDOUR {
	class Route;
	class RouteGroup;
	class Session;
}

class MeterStrip : public Gtk::VBox, public AxisView, public RouteUI
{
public:
	MeterStrip (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);
	MeterStrip (int, ARDOUR::MeterType);
	~MeterStrip ();

	std::string name() const;
	Gdk::Color color () const;

	boost::shared_ptr<ARDOUR::Stripable> stripable() const { return RouteUI::stripable(); }

	void set_session (ARDOUR::Session* s);
	void fast_update ();
	boost::shared_ptr<ARDOUR::Route> route() { return _route; }

	static PBD::Signal1<void,MeterStrip*> CatchDeletion;
	static PBD::Signal0<void> MetricChanged;
	static PBD::Signal0<void> ConfigurationChanged;

	void reset_peak_display ();
	void reset_route_peak_display (ARDOUR::Route*);
	void reset_group_peak_display (ARDOUR::RouteGroup*);

	void set_meter_type_multi (int, ARDOUR::RouteGroup*, ARDOUR::MeterType);

	void set_metric_mode (int, ARDOUR::MeterType);
	int  get_metric_mode() { return _metricmode; }
	void set_tick_bar (int);
	int  get_tick_bar() { return _tick_bar; }
	bool has_midi() { return _has_midi; }
	bool is_metric_display() { return _strip_type == 0; }
	ARDOUR::MeterType meter_type();

	bool selected() const { return false; }

protected:
	boost::shared_ptr<ARDOUR::Route> _route;
	PBD::ScopedConnectionList meter_route_connections;
	PBD::ScopedConnectionList level_meter_connection;
	void self_delete ();

	gint meter_metrics_expose (GdkEventExpose *);
	gint meter_ticks1_expose (GdkEventExpose *);
	gint meter_ticks2_expose (GdkEventExpose *);

	void on_theme_changed ();

	void on_size_allocate (Gtk::Allocation&);
	void on_size_request (Gtk::Requisition*);

	/* route UI */
	void blink_rec_display (bool onoff);
	std::string state_id() const;
	void set_button_names ();

private:
	Gtk::VBox mtr_vbox;
	Gtk::VBox nfo_vbox;
	Gtk::EventBox mtr_container;
	Gtk::HSeparator mtr_hsep;
	Gtk::HBox meterbox;
	Gtk::HBox spacer;
	Gtk::HBox namebx;
	Gtk::VBox namenumberbx;
	ArdourWidgets::ArdourButton name_label;
	ArdourWidgets::ArdourButton number_label;
	Gtk::DrawingArea meter_metric_area;
	Gtk::DrawingArea meter_ticks1_area;
	Gtk::DrawingArea meter_ticks2_area;

	Gtk::HBox mutebox;
	Gtk::HBox solobox;
	Gtk::HBox recbox;
	Gtk::HBox mon_in_box;
	Gtk::HBox mon_disk_box;
	Gtk::HBox gain_box;

	Gtk::Alignment meter_align;
	Gtk::Alignment peak_align;
	Gtk::HBox peakbx;
	Gtk::VBox btnbox;
	ArdourWidgets::ArdourButton peak_display;

	std::vector<ARDOUR::DataType> _types;
	ARDOUR::MeterType metric_type;

	bool _clear_meters;
	bool _meter_peaked;
	bool _has_midi;
	int _tick_bar;
	int _strip_type;
	int _metricmode;

	LevelMeterHBox *level_meter;

	ArdourWidgets::ArdourKnob gain_control;

	void route_property_changed (const PBD::PropertyChange&);
	void meter_configuration_changed (ARDOUR::ChanCount);
	void meter_type_changed (ARDOUR::MeterType);
	void update_background (ARDOUR::MeterType);

	bool peak_button_release (GdkEventButton*);

	void gain_start_touch ();
	void gain_end_touch ();

	void parameter_changed (std::string const & p);
	void redraw_metrics ();
	void update_button_box ();
	void update_name_box ();
	void name_changed ();

	void route_color_changed ();

	bool _suspend_menu_callbacks;
	bool level_meter_button_press (GdkEventButton* ev);
	void popup_level_meter_menu (GdkEventButton* ev);
	void add_level_meter_type_item (Gtk::Menu_Helpers::MenuList&, Gtk::RadioMenuItem::Group&, std::string const &, ARDOUR::MeterType);

	bool name_label_button_release (GdkEventButton* ev);
	void popup_name_label_menu (GdkEventButton* ev);
	void add_label_height_item (Gtk::Menu_Helpers::MenuList&, Gtk::RadioMenuItem::Group&, std::string const &, uint32_t);

	void set_meter_type (ARDOUR::MeterType mode);
	void set_label_height (uint32_t);
};

#endif /* __ardour_mixer_strip__ */
