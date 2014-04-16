/*
    Copyright (C) 2014 Valeriy Kamyshniy

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

//#include "pbd/failed_constructor.h"
//#include "pbd/file_utils.h"
//#include "pbd/replace_all.h"
//#include "pbd/whitespace.h"
//#include "pbd/stacktrace.h"
//#include "pbd/openuri.h"
//
//#include "ardour/audioengine.h"
//#include "ardour/filesystem_paths.h"
//#include "ardour/recent_sessions.h"
//#include "ardour/session.h"
//#include "ardour/session_state_utils.h"
//#include "ardour/template_utils.h"
//#include "ardour/filename_extensions.h"
//
//#include "ardour_ui.h"
#include "session_lock_dialog.h"
//#include "opts.h"
//VKPRefs:#include "engine_dialog.h"
#include "i18n.h"
//#include "utils.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;

#define dbg_msg(a) MessageDialog (a, PROGRAM_NAME).run();


SessionLockDialog::SessionLockDialog ()
	: WavesDialog (_("session_lock_dialog.xml"), true, false)
	, ok_button (get_waves_button ("ok_button"))
{
	set_keep_above (true);
	set_position (WIN_POS_CENTER);

	ok_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionLockDialog::on_ok));
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


