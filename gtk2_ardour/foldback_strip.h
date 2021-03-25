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

#ifndef __ardour_foldback_strip__
#define __ardour_foldback_strip__

#include <vector>

#include <cmath>

#include <gtkmm/adjustment.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/sizegroup.h>
#include <gtkmm/textview.h>
#include <gtkmm/togglebutton.h>

#include "pbd/stateful.h"

#include "ardour/types.h"
#include "ardour/ardour.h"
#include "ardour/processor.h"
#include "ardour/meter.h"

#include "pbd/fastlog.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_knob.h"
#include "widgets/fastmeter.h"

#include "route_ui.h"
#include "panner_ui.h"
#include "enums.h"
#include "processor_box.h"
#include "processor_selection.h"

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
	FoldbackSend (boost::shared_ptr<ARDOUR::Send>, \
		boost::shared_ptr<ARDOUR::Route> sr, boost::shared_ptr<ARDOUR::Route> fr, uint32_t wd);
	~FoldbackSend ();

private:
	ArdourWidgets::ArdourButton _button;
	boost::shared_ptr<ARDOUR::Send> _send;
	boost::shared_ptr<ARDOUR::Route> _send_route;
	boost::shared_ptr<ARDOUR::Route> _foldback_route;
	boost::shared_ptr<ARDOUR::Processor> _send_proc;
	boost::shared_ptr<ARDOUR::Delivery> _send_del;
	uint32_t _width;

	void led_clicked(GdkEventButton *);
	gboolean button_press (GdkEventButton*);
	Gtk::Menu* build_send_menu ();
	void set_gain (float new_gain);
	void set_send_position (bool post);
	void remove_me ();

	void route_property_changed (const PBD::PropertyChange&);
	void name_changed ();
	void send_state_changed ();
	void level_adjusted ();
	void level_changed ();
	void set_tooltip ();
	ArdourWidgets::ArdourKnob   pan_control;
	Gtk::Adjustment _adjustment;
	ArdourWidgets::HSliderController _slider;
	bool _ignore_ui_adjustment;
	Gtkmm2ext::PersistentTooltip _slider_persistant_tooltip;

	PBD::ScopedConnectionList _connections;



};

class FoldbackStrip : public RouteUI, public Gtk::EventBox
{
public:
	FoldbackStrip (Mixer_UI&, ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);
	~FoldbackStrip ();

	std::string name()  const;

	PannerUI&       panner_ui() { return panners; }
	PluginSelector* plugin_selector();

	void set_embedded (bool);
	void fast_update ();
	void set_route (boost::shared_ptr<ARDOUR::Route>);
	void set_button_names ();
	void revert_to_default_display ();

	boost::shared_ptr<ARDOUR::Stripable> stripable() const {
		return RouteUI::stripable();
	}

	/** @return the delivery that is being edited using our fader; it will be the
	 *  last send passed to show_send(), or our route's main out delivery.
	 */
	boost::shared_ptr<ARDOUR::Delivery> current_delivery () const {
		return _current_delivery;
	}

	bool mixer_owned () const {
		return _mixer_owned;
	}

	/** The delivery that we are handling the level for with our fader has changed */
	PBD::Signal1<void, boost::weak_ptr<ARDOUR::Delivery> > DeliveryChanged;

	static PBD::Signal1<void,FoldbackStrip*> CatchDeletion;

	void route_active_changed ();

	void copy_processors ();
	void cut_processors ();
	void paste_processors ();
	void select_all_processors ();
	void deselect_all_processors ();
	bool delete_processors ();  //note: returns false if nothing was deleted
	void toggle_processors ();
	void ab_plugins ();
	void duplicate_current_fb ();
	void set_selected (bool yn);

	static FoldbackStrip* entered_foldback_strip() { return _entered_foldback_strip; }

protected:
	friend class Mixer_UI;
	void set_packed (bool yn);
	bool packed () { return _packed; }

private:
	Mixer_UI& _mixer;
	void init ();

	bool  _embedded;
	bool  _packed;
	bool  _mixer_owned;
	ARDOUR::Session* _session;
	bool _showing_sends;
	uint32_t _width;
	ARDOUR::PeakMeter* _peak_meter;

	Gtk::EventBox		spacer;
	Gtk::VBox			send_display;
	Gtk::ScrolledWindow	send_scroller;

	Gtk::Frame          global_frame;
	Gtk::VBox           global_vpacker;

	ProcessorBox* insert_box;
	ProcessorSelection _pr_selection;
	PannerUI     panners;

	Gtk::HBox master_box;

	ArdourWidgets::ArdourButton output_button;

	Gtk::HBox prev_next_box;

	void help_count_plugins (boost::weak_ptr<ARDOUR::Processor>);
	uint32_t _plugin_insert_cnt;

	ArdourWidgets::ArdourButton name_button;
	ArdourWidgets::ArdourButton _show_sends_button;
	ArdourWidgets::ArdourButton _previous_button;
	ArdourWidgets::ArdourButton _next_button;
	ArdourWidgets::ArdourButton _hide_button;
	ArdourWidgets::ArdourButton _comment_button;
	ArdourWidgets::ArdourKnob*   fb_level_control;
	ArdourWidgets::FastMeter*   _meter;

	void setup_comment_button ();
	void hide_clicked();

	gint output_press (GdkEventButton *);
	gint output_release (GdkEventButton *);

	Gtk::Menu output_menu;
	std::list<boost::shared_ptr<ARDOUR::Bundle> > output_menu_bundles;
	void maybe_add_bundle_to_output_menu (boost::shared_ptr<ARDOUR::Bundle>, ARDOUR::BundleList const &, ARDOUR::DataType type = ARDOUR::DataType::NIL);

	void bundle_output_chosen (boost::shared_ptr<ARDOUR::Bundle>);

	void io_changed_proxy ();

	PBD::ScopedConnection panstate_connection;
	PBD::ScopedConnection panstyle_connection;
	void connect_to_pan ();
	void update_panner_choices ();
	void update_fb_level_control ();

	void update_output_display ();
	void update_send_box ();
	void processors_changed (ARDOUR::RouteProcessorChange);
	void clear_send_box ();

	gboolean name_button_button_press (GdkEventButton*);
	Gtk::Menu* build_route_ops_menu ();
	Gtk::Menu* build_route_select_menu ();

	void cycle_foldbacks (bool next);
	void update_sensitivity ();
	void spill_change (boost::shared_ptr<ARDOUR::Stripable>);
	void show_sends_clicked ();
	void send_blink (bool);

	bool send_button_press_event (GdkEventButton *ev);
	Gtk::Menu* build_sends_menu ();

	void create_selected_sends (bool include_buses);
	void remove_current_fb ();

	void route_property_changed (const PBD::PropertyChange&);
	void name_changed ();
	void map_frozen ();
	void hide_processor_editor (boost::weak_ptr<ARDOUR::Processor> processor);
	void hide_redirect_editors ();

	static FoldbackStrip* _entered_foldback_strip;

	void engine_running();
	void engine_stopped();

	void set_current_delivery (boost::shared_ptr<ARDOUR::Delivery>);

	void drop_send ();
	PBD::ScopedConnection send_gone_connection;

	void reset_strip_style ();

	void update_io_button ();
	void port_connected_or_disconnected (boost::weak_ptr<ARDOUR::Port>, boost::weak_ptr<ARDOUR::Port>);
	void port_pretty_name_changed (std::string);

	bool mixer_strip_enter_event ( GdkEventCrossing * );
	bool mixer_strip_leave_event ( GdkEventCrossing * );

	PBD::ScopedConnectionList _connections;

};

#endif /* __ardour_foldback_strip__ */
