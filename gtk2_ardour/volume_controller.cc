/*
    Copyright (C) 1998-2007 Paul Davis
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

    $Id: volume_controller.cc,v 1.4 2000/05/03 15:54:21 pbd Exp $
*/

#include <string.h>
#include <limits.h>

#include "volume_controller.h"

using namespace Gtk;

VolumeController::VolumeController (Glib::RefPtr<Gdk::Pixbuf> p,
				    Gtk::Adjustment *adj,
				    bool with_numeric,
                                    int subw, int subh)

	: MotionFeedback (p, MotionFeedback::Rotary, "", adj, with_numeric, subw, subh)
{			  
	get_adjustment()->signal_value_changed().connect (mem_fun (*this,&VolumeController::adjustment_value_changed));
}

void
VolumeController::adjustment_value_changed ()
{
        
}


