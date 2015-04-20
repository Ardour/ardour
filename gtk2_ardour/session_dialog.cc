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

#include "session_dialog.h"
#include "utils.h"
#include "i18n.h"
#include "dbg_msg.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

SessionDialog::SessionDialog (WM::Proxy<TracksControlPanel>& system_configuration_dialog, 
							  bool require_new,
							  const std::string& session_name,
							  const std::string& session_path,
							  const std::string& template_name,
							  bool cancel_not_quit)
	: WavesDialog (_("session_dialog.xml"), true, false)
	, _quit_button (get_waves_button ("quit_button"))
	, _new_session_button (get_waves_button ("new_session_button"))
	, _new_session_with_template_button (get_waves_button ("new_session_with_template_button"))
	, _open_selected_button (get_waves_button ("open_selected_button"))
	, _open_saved_session_button (get_waves_button ("open_saved_session_button"))
	, _system_configuration_button (get_waves_button ("system_configuration_button")) 
	, _session_details_label_1 (get_label("session_details_label_1"))
	, _session_details_label_2 (get_label("session_details_label_2"))
	, _session_details_label_3 (get_label("session_details_label_3"))
	, _session_details_label_4 (get_label("session_details_label_4"))
	, _new_only (require_new)
	, _provided_session_name (session_name)
	, _provided_session_path (session_path)
	, _existing_session_chooser_used (false)
	, _system_configuration_dialog(system_configuration_dialog)
{
    _open_selected_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_open_selected));
    _open_saved_session_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_open_saved_session));
    _quit_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_quit));
    _new_session_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_new_session));
    _new_session_with_template_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_new_session_with_template));
    _system_configuration_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_system_configuration));

    _recent_session_button[0] = &get_waves_button ("recent_session_button_0");
    _recent_session_button[1] = &get_waves_button ("recent_session_button_1");
    _recent_session_button[2] = &get_waves_button ("recent_session_button_2");
    _recent_session_button[3] = &get_waves_button ("recent_session_button_3");
    _recent_session_button[4] = &get_waves_button ("recent_session_button_4");
    _recent_session_button[5] = &get_waves_button ("recent_session_button_5");
    _recent_session_button[6] = &get_waves_button ("recent_session_button_6");
    _recent_session_button[7] = &get_waves_button ("recent_session_button_7");
    _recent_session_button[8] = &get_waves_button ("recent_session_button_8");
    _recent_session_button[9] = &get_waves_button ("recent_session_button_9");
    
    _recent_template_button[0] = &get_waves_button ("recent_template_button_0");
    _recent_template_button[1] = &get_waves_button ("recent_template_button_1");
    _recent_template_button[2] = &get_waves_button ("recent_template_button_2");
    _recent_template_button[3] = &get_waves_button ("recent_template_button_3");
    _recent_template_button[4] = &get_waves_button ("recent_template_button_4");
    _recent_template_button[5] = &get_waves_button ("recent_template_button_5");
    _recent_template_button[6] = &get_waves_button ("recent_template_button_6");
    _recent_template_button[7] = &get_waves_button ("recent_template_button_7");
    _recent_template_button[8] = &get_waves_button ("recent_template_button_8");
    _recent_template_button[9] = &get_waves_button ("recent_template_button_9");

    for (size_t i = 0; i < MAX_RECENT_SESSION_COUNT; i++) {
        _recent_session_button[i]->signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_recent_object ));
        _recent_session_button[i]->signal_double_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_recent_session_double_click ));
    }

    for (size_t i = 0; i < MAX_RECENT_TEMPLATE_COUNT; i++) {
        _recent_template_button[i]->signal_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_recent_object ));
        _recent_template_button[i]->signal_double_clicked.connect (sigc::mem_fun (*this, &SessionDialog::on_recent_template_double_click ));
    }
}

SessionDialog::~SessionDialog()
{
}

