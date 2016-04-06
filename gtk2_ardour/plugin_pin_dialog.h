/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __gtkardour_plugin_pin_dialog_h__
#define __gtkardour_plugin_pin_dialog_h__

#include <gtkmm/drawingarea.h>

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/plugin_insert.h"
#include "ardour/route.h"

#include "ardour_button.h"
#include "ardour_window.h"
#include "io_selector.h"

class PluginPinDialog : public ArdourWindow
{
public:
	PluginPinDialog (boost::shared_ptr<ARDOUR::PluginInsert>);
	~PluginPinDialog ();

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
		CtrlWidget (CtrlType ct, ARDOUR::DataType dt, uint32_t id, uint32_t ip = 0, bool sc = false)
			: x(0), y(0), w (0), h (0), prelight (false)
		{
			e = CtrlElem (new _CtrlElem (ct, dt, id, ip, sc));
		}
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


	Gtk::DrawingArea darea;
	ArdourButton _set_config;
	ArdourButton _tgl_sidechain;
	ArdourButton _add_plugin;
	ArdourButton _del_plugin;
	ArdourButton _add_output_audio;
	ArdourButton _del_output_audio;
	ArdourButton _add_output_midi;
	ArdourButton _del_output_midi;
	ArdourButton _add_sc_audio;
	ArdourButton _add_sc_midi;

	Gtk::Menu reset_menu;
	Gtk::Menu input_menu;
	Gtk::Table* _sidechain_tbl;
	Glib::RefPtr<Gtk::SizeGroup> _pm_size_group;
	Glib::RefPtr<Gtk::SizeGroup> _sc_size_group;

	void plugin_reconfigured ();
	void update_element_pos ();
	void refill_sidechain_table ();

	void darea_size_request (Gtk::Requisition*);
	void darea_size_allocate (Gtk::Allocation&);
	bool darea_expose_event (GdkEventExpose*);
	bool darea_motion_notify_event (GdkEventMotion*);
	bool darea_button_press_event (GdkEventButton*);
	bool darea_button_release_event (GdkEventButton*);

	void draw_io_pin (cairo_t*, const CtrlWidget&);
	void draw_plugin_pin (cairo_t*, const CtrlWidget&);

	void set_color (cairo_t*, bool);
	double pin_x_pos (uint32_t, double, double, uint32_t, uint32_t, bool);
	void draw_connection (cairo_t*, double, double, double, double, bool, bool, bool dashed = false);
	void draw_connection (cairo_t*, const CtrlWidget&, const CtrlWidget&, bool dashed = false);
	const CtrlWidget& get_io_ctrl (CtrlType ct, ARDOUR::DataType dt, uint32_t id, uint32_t ip = 0) const;
	static void edge_coordinates (const CtrlWidget& w, double &x, double &y);

	void reset_mapping ();
	void reset_configuration ();
	void toggle_sidechain ();
	void connect_sidechain ();
	void add_remove_plugin_clicked (bool);
	void add_remove_port_clicked (bool, ARDOUR::DataType);
	void add_sidechain_port (ARDOUR::DataType);
	void handle_input_action (const CtrlElem &, const CtrlElem &);
	void handle_output_action (const CtrlElem &, const CtrlElem &);
	void handle_disconnect (const CtrlElem &);
	void add_port_to_table (boost::shared_ptr<ARDOUR::Port>, uint32_t, bool);
	void remove_port (boost::weak_ptr<ARDOUR::Port>);
	void disconnect_port (boost::weak_ptr<ARDOUR::Port>);
	void connect_port (boost::weak_ptr<ARDOUR::Port>, boost::weak_ptr<ARDOUR::Port>);
	uint32_t maybe_add_route_to_input_menu (boost::shared_ptr<ARDOUR::Route>, ARDOUR::DataType, boost::weak_ptr<ARDOUR::Port>);
	void port_connected_or_disconnected (boost::weak_ptr<ARDOUR::Port>, boost::weak_ptr<ARDOUR::Port>);

	bool sc_input_press (GdkEventButton *, boost::weak_ptr<ARDOUR::Port>);
	bool sc_input_release (GdkEventButton *);

	PBD::ScopedConnectionList _plugin_connections;
	PBD::ScopedConnection _io_connection;
	boost::shared_ptr<ARDOUR::PluginInsert> _pi;

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
};

#endif
