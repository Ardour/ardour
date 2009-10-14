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

#ifndef __ardour_gtk_gui_thread_h__
#define __ardour_gtk_gui_thread_h__

#include <gtkmm2ext/gtk_ui.h>
#include "pbd/crossthread.h"

#define ENSURE_GUI_THREAD(slot) \
     if (!Gtkmm2ext::UI::instance()->caller_is_ui_thread()) {\
	Gtkmm2ext::UI::instance()->call_slot ((slot));\
        return;\
     }

#define GTK_SAFE(theSlot) crossthread_safe (Gtkmm2ext::UI::instance()->thread_id(),\
					    *Gtkmm2ext::UI::instance(), \
					    (theSlot))

#endif /* __ardour_gtk_gui_thread_h__ */
