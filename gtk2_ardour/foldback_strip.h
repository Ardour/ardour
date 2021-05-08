/*
 * Copyright (C) 2018-2020 Len Ovens
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

#ifndef _gtkardour_foldback_strip_
#define _gtkardour_foldback_strip_

#include <cmath>
#include <vector>

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/sizegroup.h>
#include <gtkmm/textview.h>
#include <gtkmm/togglebutton.h>

#include "pbd/stateful.h"

#include "ardour/ardour.h"
#include "ardour/meter.h"
#include "ardour/processor.h"
#include "ardour/types.h"

#include "pbd/fastlog.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_knob.h"
#include "widgets/fastmeter.h"

#include "enums.h"
#include "io_button.h"
#include "panner_ui.h"
#include "processor_box.h"
#include "processor_selection.h"
#include "route_ui.h"

namespace ARDOUR {
	class Route;
	class Send;
	class Processor;
	class Session;
	class PortInsert;
	class Bundle;
	class Plugin;
	class PeakMeter;
}

namespace Gtk {
	class Window;
	class Style;
}

class Mixer_UI;
class MotionController;
class RouteGroupMenu;
class ArdourWindow;

class FoldbackSend : public Gtk::VBox
{
public:
	FoldbackSend (boost::shared_ptr<ARDOUR::Send>, boost::shared_ptr<ARDOUR::Route> sr, boost::shared_ptr<ARDOUR::Route> fr, uint32_t wd);
	~FoldbackSend ();

private:
	void led_clicked (GdkEventButton*);
	bool button_press (GdkEventButton*);
	bool button_release (GdkEventButton*);
	void set_gain (float new_gain);
	void set_send_position (bool post);
	void remove_me ();
	void route_property_changed (const PBD::PropertyChange&);
	void name_changed ();
	void send_state_changed ();
	void level_adjusted ();
	void level_changed ();
	void set_tooltip ();

	Gtk::Menu* build_send_menu ();

	ArdourWidgets::ArdourButton          _button;
	boost::shared_ptr<ARDOUR::Send>      _send;
	boost::shared_ptr<ARDOUR::Route>     _send_route;
	boost::shared_ptr<ARDOUR::Route>     _foldback_route;
	boost::shared_ptr<ARDOUR::Processor> _send_proc;
	boost::shared_ptr<ARDOUR::Delivery>  _send_del;
	uint32_t                             _width;
	ArdourWidgets::ArdourKnob            _pan_control;
	Gtk::Adjustment                      _adjustment;
	ArdourWidgets::HSliderController     _slider;
	bool                                 _ignore_ui_adjustment;
	Gtkmm2ext::PersistentTooltip         _slider_persistant_tooltip;
	PBD::ScopedConnectionList            _connections;
};

class FoldbackStrip : public RouteUI, public Gtk::EventBox
{
public:
	FoldbackStrip (Mixer_UI&, ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);
	~FoldbackStrip ();

	std::string name () const;

	PluginSelector* plugin_selector ();

	void fast_update ();
	void set_route (boost::shared_ptr<ARDOUR::Route>);
	void set_button_names ();

	PannerUI& panner_ui ()
	{
		return _panners;
	}

	boost::shared_ptr<ARDOUR::Stripable> stripable () const
	{
		return RouteUI::stripable ();
	}

	/** The delivery that we are handling the level for with our fader has changed */
	PBD::Signal1<void, boost::weak_ptr<ARDOUR::Delivery> > DeliveryChanged;

	static PBD::Signal1<void, FoldbackStrip*> CatchDeletion;

	void route_active_changed ();

	void deselect_all_processors ();

protected:
	void create_sends (ARDOUR::Placement, bool) {}
	void create_selected_sends (ARDOUR::Placement, bool);

private:
	void init ();
	void setup_comment_button ();
	void update_sensitivity ();

	void remove_current_fb ();
	void duplicate_current_fb ();

	void hide_clicked ();
	void cycle_foldbacks (bool next);
	bool name_button_button_press (GdkEventButton*);
	bool number_button_press (GdkEventButton*);
	bool send_scroller_press (GdkEventButton*);
	bool fb_strip_enter_event (GdkEventCrossing*);

	void clear_send_box ();
	void update_send_box ();
	void name_changed ();
	void route_color_changed ();
	void connect_to_pan ();
	void io_changed_proxy ();
	void reset_strip_style ();
	void update_panner_choices ();
	void update_output_display ();

	void spill_change (boost::shared_ptr<ARDOUR::Stripable>);
	void route_property_changed (const PBD::PropertyChange&);
	void presentation_info_changed (PBD::PropertyChange const&);

	Gtk::Menu* build_route_ops_menu ();
	Gtk::Menu* build_route_select_menu ();

	Mixer_UI& _mixer;
	bool      _showing_sends;
	uint32_t  _width;

	Gtk::EventBox       _spacer;
	Gtk::VBox           _send_display;
	Gtk::ScrolledWindow _send_scroller;
	Gtk::Frame          _global_frame;
	Gtk::VBox           _global_vpacker;
	Gtk::HBox           _prev_next_box;
	Gtk::HBox           _level_box;
	ProcessorBox*       _insert_box;
	ProcessorSelection  _pr_selection;
	PannerUI            _panners;
	IOButton            _output_button;

	ArdourWidgets::ArdourButton _number_label;
	ArdourWidgets::ArdourButton _name_button;
	ArdourWidgets::ArdourButton _previous_button;
	ArdourWidgets::ArdourButton _next_button;
	ArdourWidgets::ArdourButton _hide_button;
	ArdourWidgets::ArdourButton _comment_button;
	ArdourWidgets::ArdourKnob   _level_control;
	ArdourWidgets::FastMeter*   _meter;

	PBD::ScopedConnectionList _send_connections;
};

#endif
