/*
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtkardour_port_insert_ui_h__
#define __gtkardour_port_insert_ui_h__

#include "widgets/ardour_button.h"
#include "widgets/stateful_button.h"

#include "ardour_window.h"
#include "gain_meter.h"
#include "io_selector.h"

namespace ARDOUR
{
	class PortInsert;
}

class LatencyGUI;
class MTDM;

class PortInsertUI : public Gtk::VBox
{
public:
	PortInsertUI (Gtk::Window*, ARDOUR::Session*, std::shared_ptr<ARDOUR::PortInsert>);
	~PortInsertUI ();

	void redisplay ();
	void finished (IOSelector::Result);

private:
	void fast_update ();
	void send_changed (ARDOUR::IOChange, void*);
	void return_changed (ARDOUR::IOChange, void*);

	bool check_latency_measurement ();
	void set_latency_label ();
	void forget_measuremed_latency ();
	void set_measured_status (MTDM* mtdm = NULL);

	bool invert_press (GdkEventButton* ev);
	bool invert_release (GdkEventButton* ev);
	bool measure_latency_press (GdkEventButton* ev);
	void edit_latency_button_clicked ();
	void latency_button_toggled ();

	std::shared_ptr<ARDOUR::PortInsert> _pi;

	Gtk::Notebook                       _notebook;
	ArdourWidgets::StatefulToggleButton _measure_latency_button;
	ArdourWidgets::ArdourButton         _invert_button;
	ArdourWidgets::ArdourButton         _edit_latency_button;

	IOSelector _input_selector;
	IOSelector _output_selector;
	GainMeter  _input_gpm;
	GainMeter  _output_gpm;
	Gtk::HBox  _input_hbox;
	Gtk::HBox  _output_hbox;
	Gtk::VBox  _input_vbox;
	Gtk::VBox  _output_vbox;
	Gtk::Label _latency_display;
	Gtk::HBox  _latency_hbox;

	Gtk::Window*  _parent;
	LatencyGUI*   _latency_gui;
	ArdourWindow* _latency_dialog;

	sigc::connection _latency_timeout;
	sigc::connection _fast_screen_update_connection;

	PBD::ScopedConnectionList _connections;
};

class PortInsertWindow : public ArdourWindow
{
public:
	PortInsertWindow (Gtk::Window&, ARDOUR::Session*, std::shared_ptr<ARDOUR::PortInsert>);

private:
	PortInsertUI _portinsertui;
};

#endif /* __gtkardour_port_insert_ui_h__ */
