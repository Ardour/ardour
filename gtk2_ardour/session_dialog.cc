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

//#include <fstream>
//#include <algorithm>
//
//#include "waves_button.h"
//
//#include <gtkmm/filechooser.h>
//
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
#include "session_dialog.h"
//#include "opts.h"
#include "i18n.h"
//#include "utils.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;

#define dbg_msg(a) MessageDialog (a, PROGRAM_NAME).run();


SessionDialog::SessionDialog (WM::Proxy<TracksControlPanel>& system_configuration_dialog, 
							  bool require_new,
							  const std::string& session_name,
							  const std::string& session_path,
							  const std::string& template_name,
							  bool cancel_not_quit)
	: WavesDialog (_("session_dialog.xml"), true, false)
	, quit_button (get_waves_button ("quit_button"))
	, system_configuration_button (get_waves_button ("system_configuration_button")) 
	, new_session_button (get_waves_button ("new_session_button"))
	, open_selected_button (get_waves_button ("open_selected_button"))
	, open_saved_session_button (get_waves_button ("open_saved_session_button"))
	, session_details_label(get_label("session_details_label"))
	, new_only (require_new)
	, _provided_session_name (session_name)
	, _provided_session_path 	(session_path)
	, _existing_session_chooser_used (false)
	, _system_configuration_dialog(system_configuration_dialog)
{
	recent_session_button[0] = &get_waves_button ("recent_session_button_0");
	recent_session_button[1] = &get_waves_button ("recent_session_button_1");
	recent_session_button[2] = &get_waves_button ("recent_session_button_2");
	recent_session_button[3] = &get_waves_button ("recent_session_button_3");
	recent_session_button[4] = &get_waves_button ("recent_session_button_4");
	recent_session_button[5] = &get_waves_button ("recent_session_button_5");
	recent_session_button[6] = &get_waves_button ("recent_session_button_6");
	recent_session_button[7] = &get_waves_button ("recent_session_button_7");
	recent_session_button[8] = &get_waves_button ("recent_session_button_8");
	recent_session_button[9] = &get_waves_button ("recent_session_button_9");
	init();
}

SessionDialog::~SessionDialog()
{
}

