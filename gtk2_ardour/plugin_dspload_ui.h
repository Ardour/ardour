/*
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
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

#ifndef _gtkardour_plugin_dspload_ui_h_
#define _gtkardour_plugin_dspload_ui_h_

#include <gtkmm/widget.h>
#include <gtkmm/table.h>
#include <gtkmm/label.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/separator.h>

#include "widgets/ardour_button.h"

#include "ardour/plugin_insert.h"

class PluginLoadStatsGui : public Gtk::Table
{
public:
	PluginLoadStatsGui (boost::shared_ptr<ARDOUR::PluginInsert>);

	void start_updating ();
	void stop_updating ();

	double   dsp_avg () const { return _valid ? _avg : -1; }
	uint64_t dsp_max () const { return _valid ? _max : 0; }

private:
	void update_cpu_label ();
	bool draw_bar (GdkEventExpose*);
	void clear_stats () {
		_insert->clear_stats ();
	}

	boost::shared_ptr<ARDOUR::PluginInsert> _insert;
	sigc::connection update_cpu_label_connection;

	Gtk::Label _lbl_min;
	Gtk::Label _lbl_max;
	Gtk::Label _lbl_avg;
	Gtk::Label _lbl_dev;

	ArdourWidgets::ArdourButton _reset_button;
	Gtk::DrawingArea _darea;

	PBD::microseconds_t _min, _max;
	double   _avg, _dev;
	bool     _valid;
};

#endif
