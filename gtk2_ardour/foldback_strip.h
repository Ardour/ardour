/*
    Copyright (C) 2000-2006 Paul Davis

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
//#include <gtkmm/scrolledwindow.h> // test
#include <gtkmm/sizegroup.h>
#include <gtkmm/table.h>
#include <gtkmm/textview.h>
#include <gtkmm/togglebutton.h>

#include "pbd/stateful.h"

#include "ardour/types.h"
#include "ardour/ardour.h"
#include "ardour/processor.h"

#include "pbd/fastlog.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_knob.h"

#include "axis_view.h"
#include "route_ui.h"
#include "panner_ui.h"
#include "enums.h"
namespace ARDOUR {
	class Route;
	class Send;
	class Processor;
	class Session;
	class PortInsert;
	class Bundle;
	class Plugin;
}
namespace Gtk {
	class Window;
	class Style;
}

class Mixer_UI;
class MotionController;
class RouteGroupMenu;
class ArdourWindow;

class FoldbackStrip : public AxisView, public RouteUI, public Gtk::EventBox
{
public:
	FoldbackStrip (Mixer_UI&, ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);
	~FoldbackStrip ();

	std::string name()  const;
	Gdk::Color color () const;

	boost::shared_ptr<ARDOUR::Stripable> stripable() const { return RouteUI::stripable(); }

	void* width_owner () const { return _width_owner; }

	PannerUI&       panner_ui()       { return panners; }

	void fast_update ();
	void set_embedded (bool);

	void set_route (boost::shared_ptr<ARDOUR::Route>);
	void set_button_names ();
	void revert_to_default_display ();

	/** @return the delivery that is being edited using our fader; it will be the
	 *  last send passed to ::show_send, or our route's main out delivery.
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

	std::string state_id() const;

	void route_active_changed ();

	static FoldbackStrip* entered_foldback_strip() { return _entered_foldback_strip; }

protected:
	friend class Mixer_UI;
	void set_packed (bool yn);
	bool packed () { return _packed; }

	void set_stuff_from_route ();

private:
	Mixer_UI& _mixer;

	void init ();

	bool  _embedded;
	bool  _packed;
	bool  _mixer_owned;
	Width _width;
	void*  _width_owner;

	Gtk::EventBox		spacer;
	Gtk::VBox			send_display;

	Gtk::Frame          global_frame;
	Gtk::VBox           global_vpacker;

	PannerUI     panners;

	Glib::RefPtr<Gtk::SizeGroup> button_size_group;

	Gtk::Table mute_solo_table;
	Gtk::Table bottom_button_table;

	ArdourWidgets::ArdourButton output_button;

	void output_button_resized (Gtk::Allocation&);
	void comment_button_resized (Gtk::Allocation&);

	Gtk::HBox show_sends_box;

	std::string longest_label;

	gint    mark_update_safe ();
	guint32 mode_switch_in_progress;

	ArdourWidgets::ArdourButton name_button;
	ArdourWidgets::ArdourButton _select_button;
	ArdourWidgets::ArdourButton _comment_button;
	ArdourWidgets::ArdourKnob*   fb_level_control;

	void setup_comment_button ();

	gint output_press (GdkEventButton *);
	gint output_release (GdkEventButton *);

	Gtk::Menu output_menu;
	std::list<boost::shared_ptr<ARDOUR::Bundle> > output_menu_bundles;
	void maybe_add_bundle_to_output_menu (boost::shared_ptr<ARDOUR::Bundle>, ARDOUR::BundleList const &,
	                                      ARDOUR::DataType type = ARDOUR::DataType::NIL);

	void bundle_output_chosen (boost::shared_ptr<ARDOUR::Bundle>);

	void io_changed_proxy ();

	Gtk::Menu *send_action_menu;
	void build_send_action_menu ();

	PBD::ScopedConnection panstate_connection;
	PBD::ScopedConnection panstyle_connection;
	void connect_to_pan ();
	void update_panner_choices ();
	void update_fb_level_control ();

	void update_output_display ();

	void set_automated_controls_sensitivity (bool yn);

	Gtk::Menu* route_ops_menu;
	void build_route_ops_menu ();
	gboolean name_button_button_press (GdkEventButton*);
	void list_route_operations ();

	Gtk::Menu* route_select_menu;
	void build_route_select_menu ();
	gboolean select_button_button_press (GdkEventButton*);
	void list_fb_routes ();

	void build_sends_menu ();
	void remove_current_fb ();

	Gtk::Style *passthru_style;

	void show_passthru_color ();

	void route_property_changed (const PBD::PropertyChange&);
	void name_button_resized (Gtk::Allocation&);
	void name_changed ();
	void update_speed_display ();
	void map_frozen ();
	void hide_redirect_editors ();

	bool ignore_speed_adjustment;

	static FoldbackStrip* _entered_foldback_strip;

	void engine_running();
	void engine_stopped();

	void set_current_delivery (boost::shared_ptr<ARDOUR::Delivery>);

	void drop_send ();
	PBD::ScopedConnection send_gone_connection;

	void reset_strip_style ();

	ARDOUR::DataType guess_main_type(bool for_input, bool favor_connected = true) const;

	void update_io_button ();
	void port_connected_or_disconnected (boost::weak_ptr<ARDOUR::Port>, boost::weak_ptr<ARDOUR::Port>);

	bool mixer_strip_enter_event ( GdkEventCrossing * );
	bool mixer_strip_leave_event ( GdkEventCrossing * );

	void add_output_port (ARDOUR::DataType);

	bool _suspend_menu_callbacks;
};

#endif /* __ardour_foldback_strip__ */
