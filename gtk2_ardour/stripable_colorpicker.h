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

#pragma once

#include <memory>

#include <ytkmm/colorbutton.h>
#include <ytkmm/colorselection.h>

#include "ardour_color_dialog.h"

namespace ARDOUR {
	class Stripable;
}

class StripableColorDialog : public ArdourColorDialog
{
public:
	StripableColorDialog (std::shared_ptr<ARDOUR::Stripable>);
	~StripableColorDialog ();
	void popup (Gtk::Window*);

private:
	void finish_color_edit (int response);
	void color_changed ();

	std::shared_ptr<ARDOUR::Stripable> _stripable;

	PBD::ScopedConnectionList _connections;
};

