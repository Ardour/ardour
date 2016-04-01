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
		_CtrlElem (CtrlType c, ARDOUR::DataType d, uint32_t i, uint32_t p = -1)
			: ct (c), dt (d), id (i), ip (p) {}
		CtrlType ct;
		ARDOUR::DataType dt;
		uint32_t id; // port/pin ID
		uint32_t ip; // plugin ID (for Sink, Source only)
	};

	typedef boost::shared_ptr<_CtrlElem> CtrlElem;

	struct CtrlWidget {
		CtrlWidget (CtrlType ct, ARDOUR::DataType dt, uint32_t id, uint32_t ip = 0)
			: x(0), y(0), w (0), h (0), prelight (false)
		{
			e = CtrlElem (new _CtrlElem (ct, dt, id, ip));
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
	ArdourButton _strict_io;
	ArdourButton _automatic;
	ArdourButton _add_plugin;
	ArdourButton _del_plugin;
	ArdourButton _add_output_audio;
	ArdourButton _del_output_audio;
	ArdourButton _add_output_midi;
	ArdourButton _del_output_midi;

	void plugin_reconfigured ();
	void update_elements ();
	void update_element_pos ();

	void darea_size_allocate (Gtk::Allocation&);
	bool darea_expose_event (GdkEventExpose*);
	bool darea_motion_notify_event (GdkEventMotion*);
	bool darea_button_press_event (GdkEventButton*);
	bool darea_button_release_event (GdkEventButton*);

	void draw_io_pin (cairo_t*, const CtrlWidget&);
	void draw_plugin_pin (cairo_t*, const CtrlWidget&);

	void set_color (cairo_t*, bool);
	double pin_x_pos (uint32_t, double, double, uint32_t, uint32_t, bool);
	bool is_valid_port (uint32_t, uint32_t, uint32_t, bool);
	void draw_connection (cairo_t*, double, double, double, double, bool, bool dashed = false);

	void automatic_clicked ();
	void add_remove_plugin_clicked (bool);
	void add_remove_port_clicked (bool, ARDOUR::DataType);
	void handle_input_action (const CtrlElem &, const CtrlElem &);
	void handle_output_action (const CtrlElem &, const CtrlElem &);

	PBD::ScopedConnectionList _plugin_connections;
	boost::shared_ptr<ARDOUR::PluginInsert> _pi;

	uint32_t _n_plugins;
	ARDOUR::ChanCount _in, _out;
	ARDOUR::ChanCount _sinks, _sources;

	double _pin_box_size;
	double _width, _height;
	bool _position_valid;
	ARDOUR::Route* _route () { return static_cast<ARDOUR::Route*> (_pi->owner ()); }
};

#endif
