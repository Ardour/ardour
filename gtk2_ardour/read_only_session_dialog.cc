/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include "pbd/file_utils.h"
#include "ardour/filesystem_paths.h"

#include "i18n.h"
#include "about_dialog.h"
#include "read_only_session_dialog.h"

using namespace Gtk;
using namespace Gdk;
using namespace std;
using namespace ARDOUR;
using namespace PBD;

ReadOnlySessionDialog::ReadOnlySessionDialog ()
	: WavesDialog (_("read_only_session_dialog.xml"), true, false)
    , _ok_button ( get_waves_button ("ok_button") )
{
	set_modal (true);
	set_resizable (false);
    
    _ok_button.signal_clicked.connect (sigc::mem_fun (*this, &ReadOnlySessionDialog::ok_button_pressed));
    
	show_all ();
}

void
ReadOnlySessionDialog::on_esc_pressed ()
{
    hide ();
}

void
ReadOnlySessionDialog::on_enter_pressed ()
{
    hide ();
}

void
ReadOnlySessionDialog::ok_button_pressed (WavesButton*)
{
    hide ();
}

ReadOnlySessionDialog::~ReadOnlySessionDialog ()
{
}
