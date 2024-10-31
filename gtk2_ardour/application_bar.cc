/*
 * Copyright (C) 2005-2024 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2024 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2024 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#include "gtk2ardour-version.h"
#endif

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <gtkmm/accelmap.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/stock.h>
#include <gtkmm/uimanager.h>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/failed_constructor.h"
#include "pbd/memento_command.h"
#include "pbd/openuri.h"
#include "pbd/types_convert.h"
#include "pbd/file_utils.h"
#include <pbd/localtime_r.h>
#include "pbd/pthread_utils.h"
#include "pbd/replace_all.h"
#include "pbd/scoped_file_descriptor.h"
#include "pbd/xml++.h"

#include "gtkmm2ext/application.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"

#include "widgets/tooltips.h"

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/profile.h"
#include "ardour/revision.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"
#include "ardour/triggerbox.h"
#include "ardour/track.h"
#include "ardour/vca_manager.h"
#include "ardour/utils.h"

#include "control_protocol/basic_ui.h"

#include "actions.h"
#include "application_bar.h"
#include "ardour_ui.h"
#include "debug.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "keyeditor.h"
#include "luainstance.h"
#include "main_clock.h"
#include "meter_patterns.h"
#include "mixer_ui.h"
#include "public_editor.h"
#include "rc_option_editor.h"
#include "recorder_ui.h"
#include "session_dialog.h"
#include "session_option_editor.h"
#include "splash.h"
#include "time_info_box.h"
#include "timers.h"
#include "trigger_page.h"
#include "triggerbox_ui.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace std;

static const gchar *_record_mode_strings[] = {
	N_("Layered"),
	N_("Non-Layered"),
	N_("Snd on Snd"),
	0
};

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))

ApplicationBar::ApplicationBar ()
	: _have_layout (false)
	, _basic_ui (0)
{
}

ApplicationBar::~ApplicationBar ()
{
}

void
ApplicationBar::on_parent_changed (Gtk::Widget*)
{
	assert (!_have_layout);
	_have_layout = true;

	_transport_ctrl.setup (ARDOUR_UI::instance ());
	_transport_ctrl.map_actions ();

	/* sync_button */
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Transport"), X_("ToggleExternalSync"));
	_sync_button.set_related_action (act);
	_sync_button.signal_button_press_event().connect (sigc::mem_fun (*this, &ApplicationBar::sync_button_clicked), false);
	_sync_button.set_sizing_text (S_("LogestSync|M-Clk"));

	/* sub-layout for Sync | Shuttle (grow) */
	HBox* ssbox = manage (new HBox);
	ssbox->set_spacing (PX_SCALE(2));
	ssbox->pack_start (_sync_button, false, false, 0);
	ssbox->pack_start (_shuttle_box, true, true, 0);
	ssbox->pack_start (*_shuttle_box.vari_button(), false, false, 0);
	ssbox->pack_start (*_shuttle_box.info_button(), false, false, 0);

	int vpadding = 1;
	int hpadding = 2;
	int col = 0;
#define TCOL col, col + 1

	_table.attach (_transport_ctrl, TCOL, 0, 1 , SHRINK, SHRINK, 0, 0);
	_table.attach (*ssbox,         TCOL, 1, 2 , FILL,   SHRINK, 0, 0);
	++col;



	_table.set_spacings (0);
	_table.set_row_spacings (4);
	_table.set_border_width (1);
	_table.show_all();  //TODO: update visibility somewhere else
	pack_start(_table, false, false);

	/*sizing */
	Glib::RefPtr<SizeGroup> button_height_size_group = ARDOUR_UI::instance()->button_height_size_group;
	button_height_size_group->add_widget (_transport_ctrl.size_button ());
	button_height_size_group->add_widget (_sync_button);

	/* theming */
	_sync_button.set_name ("transport active option button");

	set_transport_sensitivity (false);
}
#undef PX_SCALE
#undef TCOL

void
ApplicationBar::set_session (Session *s)
{
	SessionHandlePtr::set_session (s);

	_transport_ctrl.set_session (s);
	_shuttle_box.set_session (s);

	if (_basic_ui) {
		delete _basic_ui;
	}

	map_transport_state ();

	if (!_session) {
		_blink_connection.disconnect ();

		return;
	}

	_basic_ui = new BasicUI (*s);

	_session->AuditionActive.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::auditioning_changed, this, _1), gui_context());
	_session->TransportStateChange.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::map_transport_state, this), gui_context());

	_blink_connection = Timers::blink_connect (sigc::mem_fun(*this, &ApplicationBar::blink_handler));
}

void
ApplicationBar::set_transport_sensitivity (bool yn)
{
	ActionManager::set_sensitive (ActionManager::transport_sensitive_actions, yn);
	_shuttle_box.set_sensitive (yn);
}


void
ApplicationBar::_auditioning_changed (bool onoff)
{
//	auditioning_alert_button.set_active (onoff);
//	auditioning_alert_button.set_sensitive (onoff);
//	if (!onoff) {
//		auditioning_alert_button.set_visual_state (Gtkmm2ext::NoVisualState);
//	}
	set_transport_sensitivity (!onoff);
}

void
ApplicationBar::auditioning_changed (bool onoff)
{
	UI::instance()->call_slot (MISSING_INVALIDATOR, std::bind (&ApplicationBar::_auditioning_changed, this, onoff));
}


void
ApplicationBar::parameter_changed (std::string p)
{
	if (p == "external-sync") {

		if (!_session->config.get_external_sync()) {
			_sync_button.set_text (S_("SyncSource|Int."));
		} else {
		}

	} else if (p == "sync-source") {
		if (_session) {
			if (!_session->config.get_external_sync()) {
				_sync_button.set_text (S_("SyncSource|Int."));
			} else {
				_sync_button.set_text (TransportMasterManager::instance().current()->display_name());
			}
		} else {
			/* changing sync source without a session is unlikely/impossible , except during startup */
			_sync_button.set_text (TransportMasterManager::instance().current()->display_name());
		}
		if (_session->config.get_video_pullup() == 0.0f || TransportMasterManager::instance().current()->type() != Engine) {
			UI::instance()->set_tip (_sync_button, _("Enable/Disable external positional sync"));
		} else {
			UI::instance()->set_tip (_sync_button, _("External sync is not possible: video pull up/down is set"));
		}
	}
}


bool
ApplicationBar::sync_button_clicked (GdkEventButton* ev)
{
	if (ev->button != 3) {
		/* this handler is just for button-3 clicks */
		return false;
	}

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("Window", "toggle-transport-masters");
	tact->set_active();
	return true;
}

void
ApplicationBar::sync_blink (bool onoff)
{
	if (_session == 0 || !_session->config.get_external_sync()) {
		/* internal sync */
		_sync_button.set_active (false);
		return;
	}

	if (!_session->transport_locked()) {
		/* not locked, so blink on and off according to the onoff argument */

		if (onoff) {
			_sync_button.set_active (true);
		} else {
			_sync_button.set_active (false);
		}
	} else {
		/* locked */
		_sync_button.set_active (true);
	}
}

void
ApplicationBar::blink_handler (bool blink_on)
{
	sync_blink (blink_on);

#if 0
	if (UIConfiguration::instance().get_no_strobe() || !UIConfiguration::instance().get_blink_alert_indicators()) {
		blink_on = true;
	}
	error_blink (blink_on);
	solo_blink (blink_on);
	audition_blink (blink_on);
	feedback_blink (blink_on);
#endif
}

void
ApplicationBar::map_transport_state ()
{
	_shuttle_box.map_transport_state ();

/*	if (!_session) {
		record_mode_selector.set_sensitive (false);
		return;
	}

	float sp = _session->transport_speed();

	if (sp != 0.0f) {
		record_mode_selector.set_sensitive (!_session->actively_recording ());
	} else {
		record_mode_selector.set_sensitive (true);
		update_disk_space ();
	}

*/
}
