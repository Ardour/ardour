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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <fstream>
#include <algorithm>

#include "waves_button.h"

#include <gtkmm/filechooser.h>

#include "session_lock_dialog.h"
#include "i18n.h"
#include "dbg_msg.h"
#include "ardour_ui.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;

SessionLockDialog::SessionLockDialog ()
	: WavesDialog (_("session_lock_dialog.xml"), true, false)
	, _ok_button (get_waves_button ("ok_button"))
{
	set_keep_above (true);
	set_position (WIN_POS_CENTER);

	_ok_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionLockDialog::on_ok));
}

SessionLockDialog::~SessionLockDialog()
{
}

//app logic
void
SessionLockDialog::on_ok (WavesButton*)
{
	hide();
	response (Gtk::RESPONSE_OK);
}

void
SessionLockDialog::on_show ()
{
    WavesDialog::on_show ();
    ARDOUR_UI::instance()->on_lock_session ();
}

void
SessionLockDialog::on_hide ()
{
    ARDOUR_UI::instance()->on_unlock_session ();
    WavesDialog::on_hide ();
}

bool
SessionLockDialog::on_key_press_event (GdkEventKey*)
{
    return true;    
}
