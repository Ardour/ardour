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

#ifndef __ardour_trigger_strip__
#define __ardour_trigger_strip__

#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/frame.h>

#include "pbd/stateful.h"

#include "ardour/ardour.h"
#include "ardour/types.h"

#include "widgets/ardour_button.h"

#include "automation_controller.h"
#include "axis_view.h"
#include "fitted_canvas_widget.h"
#include "level_meter.h"
#include "panner_ui.h"
#include "processor_box.h"
#include "processor_selection.h"
#include "route_ui.h"
#include "triggerbox_ui.h"

class PluginSelector;
class TriggerMaster;

class TriggerStrip : public AxisView, public RouteUI, public Gtk::EventBox
{
public:
	TriggerStrip (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);
	~TriggerStrip ();

	/* AxisView */
	std::string name () const;
	Gdk::Color  color () const;

	boost::shared_ptr<ARDOUR::Stripable> stripable () const
	{
		return RouteUI::stripable ();
	}

	void set_session (ARDOUR::Session* s);
	void set_selected (bool yn);

	void fast_update ();

	static PBD::Signal1<void, TriggerStrip*> CatchDeletion;

protected:
	void self_delete ();

	//void on_size_allocate (Gtk::Allocation&);
	//void on_size_request (Gtk::Requisition*);

	/* AxisView */
	std::string state_id () const;

	/* route UI */
	void set_button_names ();
#if 0
	void route_rec_enable_changed ();
	void blink_rec_display (bool onoff);
#endif

private:
	void init ();

	/* RouteUI */
	void set_route (boost::shared_ptr<ARDOUR::Route>);
	void route_property_changed (const PBD::PropertyChange&);
	void route_color_changed ();
	void update_sensitivity ();
	void parameter_changed (std::string);
	void route_active_changed ();
	void map_frozen ();

	/* Callbacks */
	void io_changed ();
	void name_changed ();
	void name_button_resized (Gtk::Allocation&);
	bool name_button_press (GdkEventButton*);
	void build_route_ops_menu ();
	void reset_peak_display ();
	void reset_route_peak_display (ARDOUR::Route*);
	void reset_group_peak_display (ARDOUR::RouteGroup*);

	/* Plugin related */
	PluginSelector* plugin_selector ();
	void            hide_processor_editor (boost::weak_ptr<ARDOUR::Processor>);

	/* Panner */
	void connect_to_pan ();
	void update_panner_choices ();

	bool                  _clear_meters;
	ProcessorSelection    _pb_selection;
	PBD::ScopedConnection _panstate_connection;

	/* Layout */
	Gtk::Frame global_frame;
	Gtk::VBox  global_vpacker;
	Gtk::Table mute_solo_table;
	Gtk::Table volume_table;

	/* Widgets */
	FittedCanvasWidget _tmaster_widget;
	TriggerMaster*     _tmaster;

	ArdourWidgets::ArdourButton             _name_button;
	ProcessorBox                            _processor_box;
	TriggerBoxWidget                        _trigger_display;
	PannerUI                                _panners;
	LevelMeterVBox                          _level_meter;
	boost::shared_ptr<AutomationController> _gain_control;

	Gtk::Menu* _route_ops_menu;
};

#endif /* __ardour_trigger_strip__ */
