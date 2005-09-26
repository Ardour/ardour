/*
    Copyright (C) 2002 Paul Davis 

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

    $Id$
*/

#include "prompter.h"
#include "ardour_ui.h"

using namespace sigc;

ArdourPrompter::ArdourPrompter (bool modal)
	: Gtkmm2ext::Prompter (modal)
{
	the_entry().signal_signal_focus_in_event().connect (ptr_fun (ARDOUR_UI::generic_focus_in_event));
	the_entry().signal_signal_focus_out_event().connect (ptr_fun (ARDOUR_UI::generic_focus_out_event));
}

