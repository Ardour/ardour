/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_idle_o_meter_h__
#define __ardour_idle_o_meter_h__

#include <gtkmm/label.h>
#include "ardour_dialog.h"

class IdleOMeter : public ArdourDialog
{
public:
	IdleOMeter ();
	~IdleOMeter ();

protected:
	virtual void on_show ();
	virtual void on_hide ();

private:
	void reset ();
	bool idle ();

	Gtk::Label _label_cur;
	Gtk::Label _label_min;
	Gtk::Label _label_max;
	Gtk::Label _label_avg;
	Gtk::Label _label_dev;
	Gtk::Label _label_acq;

	int64_t _last_display;

	int64_t _start;
	int64_t _last;
	int64_t _min;
	int64_t _max;

	int64_t _cnt;
	double _total;
	double _var_m, _var_s;
	sigc::connection _idle_connection;
};
#endif

