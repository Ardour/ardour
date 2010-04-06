/*
    Copyright (C) 2004 Paul Davis
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

#include <string>
#include <iostream>

#include "pbd/controllable.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/bindable_button.h"
#include "gtkmm2ext/gui_thread.h"

#include "i18n.h"

using namespace Gtkmm2ext;
using namespace std;
using namespace PBD;

void
BindableToggleButton::set_controllable (boost::shared_ptr<PBD::Controllable> c)
{
        watch_connection.disconnect ();
        binding_proxy.set_controllable (c);
}

void
BindableToggleButton::watch ()
{
        boost::shared_ptr<Controllable> c (binding_proxy.get_controllable ());

        if (!c) {
                warning << _("button cannot watch state of non-existing Controllable\n") << endl;
                return;
        }

        c->Changed.connect (watch_connection, invalidator(*this), boost::bind (&BindableToggleButton::controllable_changed, this), gui_context());
}

void
BindableToggleButton::controllable_changed ()
{
        float val = binding_proxy.get_controllable()->get_value();
        set_active (fabs (val) >= 0.5f);
}
