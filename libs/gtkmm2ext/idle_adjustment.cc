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

#define _BSD_SOURCE
#include <gtkmm2ext/idle_adjustment.h>
#include <gtkmm/main.h>
#include <iostream>

#include "pbd/timersub.h"

using namespace Gtk;
using namespace sigc;
using namespace Gtkmm2ext;

IdleAdjustment::IdleAdjustment (Gtk::Adjustment& adj)
{
	adj.signal_value_changed().connect (mem_fun (*this, &IdleAdjustment::underlying_adjustment_value_changed));
	timeout_queued = 0;
	gettimeofday (&last_vc, 0);
}

IdleAdjustment::~IdleAdjustment ()
{
}

void
IdleAdjustment::underlying_adjustment_value_changed ()
{
	gettimeofday (&last_vc, 0);
	
	if (timeout_queued) {
		return;
	}

	Glib::signal_timeout().connect(mem_fun(*this, &IdleAdjustment::timeout_handler), 250);
	timeout_queued = true;
}	

gint
IdleAdjustment::timeout_handler ()
{
	struct timeval now;
	struct timeval tdiff;

	gettimeofday (&now, 0);

	timersub (&now, &last_vc, &tdiff);

	std::cerr << "timer elapsed, diff = " << tdiff.tv_sec << " + " << tdiff.tv_usec << std::endl;

	if (tdiff.tv_sec > 0 || tdiff.tv_usec > 250000) {
		std::cerr << "send signal\n";
		value_changed ();
		timeout_queued = false;
		return FALSE;
	} else {
		return TRUE;
	}
}
