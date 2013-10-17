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

#ifndef __gtkmm2ext_idle_adjustment_h__
#define __gtkmm2ext_idle_adjustment_h__

#include <sys/time.h>
#include <gtkmm/adjustment.h>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API IdleAdjustment : public sigc::trackable
{
  public:
	IdleAdjustment (Gtk::Adjustment& adj);
	~IdleAdjustment ();

	sigc::signal<void> value_changed;

  private:
	void underlying_adjustment_value_changed();
	struct timeval last_vc;
	gint timeout_handler();
	bool timeout_queued;
};

}

#endif /* __gtkmm2ext_idle_adjustment_h__ */
