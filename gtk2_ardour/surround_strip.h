/*
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
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

#ifndef _gtkardour_surround_strip_
#define _gtkardour_surround_strip_

#include <gtkmm/drawingarea.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/frame.h>
#include <gtkmm/menu.h>

#include "ardour/types.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_knob.h"
#include "widgets/fastmeter.h"

#include "io_button.h"
#include "route_ui.h"

namespace ARDOUR
{
class Route;
class Session;
}

class Mixer_UI;

class SurroundStrip : public RouteUI, public Gtk::EventBox
{
public:
	SurroundStrip (Mixer_UI&, ARDOUR::Session*, std::shared_ptr<ARDOUR::Route>);
	~SurroundStrip ();

	void fast_update ();
	void hide_spacer (bool);

	static PBD::Signal1<void, SurroundStrip*> CatchDeletion;

private:
	void init ();
	void set_route (std::shared_ptr<ARDOUR::Route>);
	void set_button_names ();
	void setup_comment_button ();
	void name_changed ();
	void update_spacers ();
	bool name_button_button_press (GdkEventButton*);
	void route_property_changed (const PBD::PropertyChange&);
	void parameter_changed (std::string const&);

	gint meter_metrics_expose (GdkEventExpose*);
  gint meter_ticks1_expose (GdkEventExpose*);
  gint meter_ticks2_expose (GdkEventExpose*);

	Gtk::Menu* build_route_ops_menu ();

	uint32_t      _width;
	Gtk::EventBox _spacer;
	Gtk::EventBox _spacer_ctrl;
	Gtk::EventBox _spacer_peak;
	Gtk::Frame    _global_frame;
	Gtk::VBox     _global_vpacker;
	Gtk::VBox     _surround_meter_box;
	Gtk::HBox     _binaural_meter_box;
	Gtk::HBox     _binaural_meter_hbox;
	Gtk::HBox     _level_box;
	Gtk::HBox     _top_box;
	IOButton      _output_button;

	Gtk::Label     _lufs_cap;
	Gtk::Label     _lufs_label;
	Gtk::Label     _dbtp_cap;
	Gtk::Label     _dbtp_label;

	ArdourWidgets::ArdourButton _name_button;
	ArdourWidgets::ArdourButton _comment_button;
	ArdourWidgets::ArdourKnob   _level_control;
	ArdourWidgets::FastMeter*   _meter[14];
	Gtk::DrawingArea            _meter_metric_area;
	Gtk::DrawingArea            _meter_ticks1_area;
	Gtk::DrawingArea            _meter_ticks2_area;

	std::vector<ARDOUR::DataType> _types;
};

#endif
