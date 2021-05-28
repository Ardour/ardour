/*
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __gtkardour_plugin_pin_dialog_h__
#define __gtkardour_plugin_pin_dialog_h__

#include <gtkmm/drawingarea.h>

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/plugin_insert.h"
#include "ardour/route.h"

#include <gtkmm/alignment.h>
#include <gtkmm/box.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/sizegroup.h>

#include "gtkmm2ext/persistent_tooltip.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "widgets/ardour_fader.h"
#include "widgets/slider_controller.h"

#include "ardour_window.h"
#include "io_selector.h"

class PluginPinWidget : public ARDOUR::SessionHandlePtr, public Gtk::VBox
{
public:
	PluginPinWidget (boost::shared_ptr<ARDOUR::PluginInsert>);
	~PluginPinWidget ();
	void set_session (ARDOUR::Session *);
private:
	typedef enum {
		Input,
		Sink,
		Source,
		Output
	} CtrlType;

	struct _CtrlElem {
		_CtrlElem (CtrlType c, ARDOUR::DataType d, uint32_t i, uint32_t p, bool s)
			: ct (c), dt (d), id (i), ip (p), sc (s) {}
		CtrlType ct;
		ARDOUR::DataType dt;
		uint32_t id; // port/pin ID
		uint32_t ip; // plugin ID (for Sink, Source only);
		bool sc; // sidechain
	};

	typedef boost::shared_ptr<_CtrlElem> CtrlElem;

	struct CtrlWidget {
		CtrlWidget (const std::string& n, CtrlType ct, ARDOUR::DataType dt, uint32_t id, uint32_t ip = 0, bool sc = false)
			: name (n), x(0), y(0), w (0), h (0), prelight (false)
		{
			e = CtrlElem (new _CtrlElem (ct, dt, id, ip, sc));
		}
		std::string name;
		double x,y;
		double w,h;
		bool prelight;
		CtrlElem e;
	};

	typedef std::vector<CtrlWidget> CtrlElemList;

	CtrlElem _selection;
	CtrlElem _actor;
	CtrlElem _hover;
	CtrlElemList _elements;
	CtrlElem _drag_dst;

	/* cache settings for expose */
	typedef std::map <uint32_t, ARDOUR::ChanMapping> Mappings;
	Mappings _in_map;
	Mappings _out_map;
	ARDOUR::ChanMapping _thru_map;
	bool _has_midi_bypass;


	Gtk::DrawingArea darea;

	ArdourWidgets::ArdourButton _set_config;
	ArdourWidgets::ArdourButton _tgl_sidechain;
	ArdourWidgets::ArdourButton _add_plugin;
	ArdourWidgets::ArdourButton _del_plugin;
	ArdourWidgets::ArdourButton _add_input_audio;
	ArdourWidgets::ArdourButton _del_input_audio;
	ArdourWidgets::ArdourButton _add_input_midi;
	ArdourWidgets::ArdourButton _del_input_midi;
	ArdourWidgets::ArdourButton _add_output_audio;
	ArdourWidgets::ArdourButton _del_output_audio;
	ArdourWidgets::ArdourButton _add_output_midi;
	ArdourWidgets::ArdourButton _del_output_midi;
	ArdourWidgets::ArdourButton _add_sc_audio;
	ArdourWidgets::ArdourButton _add_sc_midi;

	ArdourWidgets::ArdourDropdown _out_presets;

	Gtk::Menu reset_menu;
	Gtk::Menu input_menu;
	Gtk::Table* _sidechain_tbl;
	Glib::RefPtr<Gtk::SizeGroup> _pm_size_group;

	void plugin_reconfigured ();
	void update_element_pos ();
	void refill_sidechain_table ();
	void refill_output_presets ();

	void darea_size_request (Gtk::Requisition*);
	void darea_size_allocate (Gtk::Allocation&);
	bool darea_expose_event (GdkEventExpose*);
	bool darea_motion_notify_event (GdkEventMotion*);
	bool darea_button_press_event (GdkEventButton*);
	bool darea_button_release_event (GdkEventButton*);
	bool drag_type_matches (const CtrlElem& ct);

	void start_drag (const CtrlElem&, double, double);

	void draw_io_pin (cairo_t*, const CtrlWidget&);
	void draw_plugin_pin (cairo_t*, const CtrlWidget&);

	void set_color (cairo_t*, bool);
	double pin_x_pos (uint32_t, double, double, uint32_t, uint32_t, bool);
	void draw_connection (cairo_t*, double, double, double, double, bool, bool, bool dashed = false);
	void draw_connection (cairo_t*, const CtrlWidget&, const CtrlWidget&, bool dashed = false);
	const CtrlWidget& get_io_ctrl (CtrlType ct, ARDOUR::DataType dt, uint32_t id, uint32_t ip = 0) const;

	static void edge_coordinates (const CtrlWidget& w, double &x, double &y);
	static std::string port_label (const std::string&, bool);

	void reset_mapping ();
	void reset_configuration ();
	void toggle_sidechain ();
	void connect_sidechain ();
	void add_remove_plugin_clicked (bool);
	void add_remove_port_clicked (bool, ARDOUR::DataType);
	void add_remove_inpin_clicked (bool, ARDOUR::DataType);
	void add_sidechain_port (ARDOUR::DataType);
	void select_output_preset (uint32_t n_audio);
	void handle_input_action (const CtrlElem &, const CtrlElem &);
	void handle_output_action (const CtrlElem &, const CtrlElem &);
	void handle_thru_action (const CtrlElem &, const CtrlElem &);
	bool handle_disconnect (const CtrlElem &, bool no_signal = false);
	void disconnect_other_outputs (uint32_t skip_pc, ARDOUR::DataType dt, uint32_t id);
	void disconnect_other_thru (ARDOUR::DataType dt, uint32_t id);
	void remove_port (boost::weak_ptr<ARDOUR::Port>);
	void disconnect_port (boost::weak_ptr<ARDOUR::Port>);
	void connect_port (boost::weak_ptr<ARDOUR::Port>, boost::weak_ptr<ARDOUR::Port>);
	void add_send_from (boost::weak_ptr<ARDOUR::Port>, boost::weak_ptr<ARDOUR::Route>);
	uint32_t add_port_to_table (boost::shared_ptr<ARDOUR::Port>, uint32_t, bool);
	uint32_t maybe_add_route_to_input_menu (boost::shared_ptr<ARDOUR::Route>, ARDOUR::DataType, boost::weak_ptr<ARDOUR::Port>);
	void port_connected_or_disconnected (boost::weak_ptr<ARDOUR::Port>, boost::weak_ptr<ARDOUR::Port>);
	void port_pretty_name_changed (std::string);

	bool sc_input_press (GdkEventButton *, boost::weak_ptr<ARDOUR::Port>);
	bool sc_input_release (GdkEventButton *);

	PBD::ScopedConnectionList _plugin_connections;
	PBD::ScopedConnectionList _io_connection;
	boost::shared_ptr<ARDOUR::PluginInsert> _pi;

	void queue_idle_update ();
	bool idle_update ();

	void error_message_dialog (std::string const&) const;

	uint32_t _n_plugins;
	ARDOUR::ChanCount _in, _ins, _out;
	ARDOUR::ChanCount _sinks, _sources;

	double _bxw2, _bxh2;
	double _pin_box_size;
	double _width, _height;
	double _innerwidth, _margin_x, _margin_y;
	double _min_width;
	double _min_height;
	uint32_t _n_inputs;
	uint32_t _n_sidechains;
	bool _position_valid;
	bool _ignore_updates;
	ARDOUR::Route* _route () { return static_cast<ARDOUR::Route*> (_pi->owner ()); }
	IOSelectorWindow *_sidechain_selector;

	bool   _dragging;
	double _drag_x, _drag_y;

	class Control: public sigc::trackable {
	public:
		Control (boost::shared_ptr<ARDOUR::AutomationControl>, std::string const &);
		~Control ();
		Gtk::Alignment box;
	private:
		void slider_adjusted ();
		void control_changed ();
		void set_tooltip ();

		boost::weak_ptr<ARDOUR::AutomationControl> _control;
		Gtk::Adjustment _adjustment;
		ArdourWidgets::HSliderController _slider;
		Gtkmm2ext::PersistentTooltip _slider_persistant_tooltip;

		bool _ignore_ui_adjustment;
		sigc::connection timer_connection;
		std::string _name;
	};
	std::list<Control*> _controls;
};


class PluginPinDialog : public ArdourWindow
{
public:
	PluginPinDialog (boost::shared_ptr<ARDOUR::PluginInsert>);
	PluginPinDialog (boost::shared_ptr<ARDOUR::Route>);

	void set_session (ARDOUR::Session *);
private:
	Gtk::ScrolledWindow* scroller;
	Gtk::VBox *vbox;
	typedef boost::shared_ptr<PluginPinWidget> PluginPinWidgetPtr;
	typedef std::vector<PluginPinWidgetPtr> PluginPinWidgetList;

	void route_going_away ();
	void route_processors_changed (ARDOUR::RouteProcessorChange);
	void add_processor (boost::weak_ptr<ARDOUR::Processor>);
	void map_height (Gtk::Allocation&);

	boost::shared_ptr<ARDOUR::Route> _route;
	PluginPinWidgetList ppw;
	PBD::ScopedConnectionList _route_connections;
	bool _height_mapped;
};

#endif
