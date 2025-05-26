/*
 * Copyright (C) 2008 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
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

#include <ytkmm/entry.h>
#include <ytkmm/checkbutton.h>

#include "ardour_dialog.h"

class ControlPoint;

class ControlPointDialog : public ArdourDialog
{
public:
	ControlPointDialog (ControlPoint *, bool multi);

	double get_y_fraction () const;

	bool all_selected_points () const;

private:
	ControlPoint* point_;
	Gtk::Entry value_;
	Gtk::CheckButton toggle_all_;
	bool all_selected_points_;
};
