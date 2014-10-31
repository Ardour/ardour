/*
    Copyright (C) 2014 Paul Davis

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

#include "floating_text_entry.h"
#include "gtkmm2ext/doi.h"

#include "i18n.h"

FloatingTextEntry::FloatingTextEntry ()
	: ArdourWindow ("")
{
        set_name (X_("FloatingTextEntry"));
	set_position (Gtk::WIN_POS_MOUSE);
        set_border_width (0);
        
        entry.show ();
        entry.signal_activate().connect (sigc::mem_fun (*this, &FloatingTextEntry::activated));
        entry.signal_key_press_event().connect (sigc::mem_fun (*this, &FloatingTextEntry::key_press));

        add (entry);
}

void
FloatingTextEntry::on_realize ()
{
        ArdourWindow::on_realize ();
        get_window()->set_decorations (Gdk::WMDecoration (0));
}

void
FloatingTextEntry::activated ()
{
        use_text (entry.get_text()); // EMIT SIGNAL
        delete_when_idle (this);
}

bool
FloatingTextEntry::key_press (GdkEventKey* ev)
{
        switch (ev->keyval) {
        case GDK_Escape:
                delete_when_idle (this);
                return true;
                break;
        default:
                break;
        }
        return false;
}

void
FloatingTextEntry::on_hide ()
{
        /* No hide button is shown (no decoration on the window), 
           so being hidden is equivalent to the Escape key or any other 
           method of cancelling the edit.
        */

        delete_when_idle (this);
        ArdourWindow::on_hide ();
}
