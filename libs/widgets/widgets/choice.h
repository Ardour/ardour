/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef _WIDGETS_CHOICE_H_
#define _WIDGETS_CHOICE_H_

#include <string>
#include <vector>

#include <gtkmm/dialog.h>
#include <gtkmm/image.h>
#include <gtkmm/stock.h>
#include <gtkmm/box.h>

#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API Choice : public Gtk::Dialog
{
public:
	Choice (std::string title, std::string prompt, std::vector<std::string> choices, bool center = true);
	virtual ~Choice ();

protected:
	void on_realize ();
};

} /* namespace */

#endif
