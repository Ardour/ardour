/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2006 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2006 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
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
#endif

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <cmath>

#include <ytkmm/menu.h>
#include <ytkmm/menuitem.h>

#include <sigc++/bind.h>
#include <ytkmm/settings.h>

#include "canvas/canvas.h"

#include "pbd/error.h"
#include "pbd/basename.h"
#include "pbd/fastlog.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"

#include "ardour/audioengine.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "ardour_ui.h"
#include "keyboard.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "actions.h"
#include "main_clock.h"
#include "mixer_ui.h"
#include "recorder_ui.h"
#include "trigger_page.h"
#include "utils.h"
#include "midi_tracer.h"
#include "global_port_matrix.h"
#include "location_ui.h"
#include "rc_option_editor.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace Glib;
using namespace ARDOUR_UI_UTILS;
using namespace Menu_Helpers;

void
ARDOUR_UI::setup_tooltips ()
{
	ArdourCanvas::Canvas::set_tooltip_timeout (Gtk::Settings::get_default()->property_gtk_tooltip_timeout ());

	parameter_changed("click-gain");
	set_tip (error_alert_button, _("Show Error Log and acknowledge warnings"));

	synchronize_sync_source_and_video_pullup ();

	editor->setup_tooltips ();
}

bool
ARDOUR_UI::status_bar_button_press (GdkEventButton* ev)
{
	bool handled = false;

	switch (ev->button) {
	case 1:
		status_bar_label.set_text ("");
		handled = true;
		break;
	default:
		break;
	}

	return handled;
}

void
ARDOUR_UI::display_message (const char* prefix, gint prefix_len, RefPtr<TextBuffer::Tag> ptag, RefPtr<TextBuffer::Tag> mtag, const char* msg)
{
	UI::display_message (prefix, prefix_len, ptag, mtag, msg);

	ArdourLogLevel ll = LogLevelNone;

	if (strcmp (prefix, _("[ERROR]: ")) == 0) {
		ll = LogLevelError;
	} else if (strcmp (prefix, _("[WARNING]: ")) == 0) {
		ll = LogLevelWarning;
	} else if (strcmp (prefix, _("[INFO]: ")) == 0) {
		ll = LogLevelInfo;
	}

	_log_not_acknowledged = std::max(_log_not_acknowledged, ll);
}

XMLNode*
ARDOUR_UI::tearoff_settings (const char* name) const
{
	XMLNode* ui_node = Config->extra_xml(X_("UI"));

	if (ui_node) {
		XMLNode* tearoff_node = ui_node->child (X_("Tearoffs"));
		if (tearoff_node) {
			XMLNode* mnode = tearoff_node->child (name);
			return mnode;
		}
	}

	return 0;
}

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))

static
bool drag_failed (const Glib::RefPtr<Gdk::DragContext>& context, DragResult result, Tabbable* tab)
{
	if (result == Gtk::DRAG_RESULT_NO_TARGET) {
		tab->detach ();
		return true;
	}
	return false;
}

void
ARDOUR_UI::setup_transport ()
{
	RefPtr<Action> act;
	/* setup actions */

	/* CANNOT sigc::bind these to clicked or toggled, must use pressed or released */
	error_alert_button.signal_button_release_event().connect (sigc::mem_fun(*this,&ARDOUR_UI::error_alert_press), false);
	act = ActionManager::get_action (X_("Editor"), X_("toggle-log-window"));
	error_alert_button.set_related_action(act);
	error_alert_button.set_fallthrough_to_parent(true);

	editor_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-editor-visibility")));
	mixer_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-mixer-visibility")));
	prefs_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-preferences-visibility")));
	recorder_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-recorder-visibility")));
	trigger_page_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-trigger-visibility")));

	/* connect signals */
	ARDOUR_UI::Clock.connect (sigc::bind (sigc::mem_fun (primary_clock, &MainClock::set), false, false));
	ARDOUR_UI::Clock.connect (sigc::bind (sigc::mem_fun (secondary_clock, &MainClock::set), false, false));

	editor_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), editor));
	mixer_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), mixer));
	prefs_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), rc_option_editor));
	recorder_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), recorder));
	trigger_page_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), trigger_page));

	/* catch context clicks so that we can show a menu on these buttons */

	editor_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("editor")), false);
	mixer_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("mixer")), false);
	prefs_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("preferences")), false);
	recorder_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("recorder")), false);
	trigger_page_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("trigger")), false);

	/* setup widget style/name */

	editor_visibility_button.set_name (X_("page switch button"));
	mixer_visibility_button.set_name (X_("page switch button"));
	prefs_visibility_button.set_name (X_("page switch button"));
	recorder_visibility_button.set_name (X_("page switch button"));
	trigger_page_visibility_button.set_name (X_("page switch button"));

	/* and widget text */

	/* and tooltips */

	Gtkmm2ext::UI::instance()->set_tip (editor_visibility_button,
	                                    string_compose (_("Left-Click to show the %1 window\n"
	                                                      "Right-click to show more options"), editor->name()));

	Gtkmm2ext::UI::instance()->set_tip (mixer_visibility_button,
	                                    string_compose (_("Left-Click to show the %1 window\n"
	                                                      "Right-click to show more options"), mixer->name()));

	Gtkmm2ext::UI::instance()->set_tip (prefs_visibility_button,
	                                    string_compose (_("Left-Click to show the %1 window\n"
	                                                      "Right-click to show more options"), rc_option_editor->name()));

	Gtkmm2ext::UI::instance()->set_tip (recorder_visibility_button,
	                                    string_compose (_("Left-Click to show the %1 window\n"
	                                                      "Right-click to show more options"), recorder->name()));

	Gtkmm2ext::UI::instance()->set_tip (trigger_page_visibility_button,
	                                    string_compose (_("Left-Click to show the %1 window\n"
	                                                      "Right-click to show more options"), trigger_page->name()));


	/* transport control size-group */

	//tab selections
	button_height_size_group->add_widget (trigger_page_visibility_button);
	button_height_size_group->add_widget (recorder_visibility_button);
	button_height_size_group->add_widget (editor_visibility_button);
	button_height_size_group->add_widget (mixer_visibility_button);
	button_height_size_group->add_widget (prefs_visibility_button);

	/* and the main table layout */
	int vpadding = 3;
	int hpadding = 3;
	int col = 0;
#define TCOL col, col + 1

	tabbables_table.attach (recorder_visibility_button,     TCOL, 0, 1 , FILL, FILL, hpadding, vpadding);
	tabbables_table.attach (trigger_page_visibility_button, TCOL, 1, 2 , FILL, FILL, hpadding, vpadding);
	++col;
	tabbables_table.attach (editor_visibility_button,       TCOL, 0, 1 , FILL, FILL, hpadding, vpadding);
	tabbables_table.attach (mixer_visibility_button,        TCOL, 1, 2 , FILL, FILL, hpadding, vpadding);
	++col;

	tabbables_table.show_all ();
}
#undef PX_SCALE
#undef TCOL

bool
ARDOUR_UI::error_alert_press (GdkEventButton* ev)
{
	bool do_toggle = true;
	if (ev->button == 1) {
		if (_log_not_acknowledged == LogLevelError) {
			// just acknowledge the error, don't hide the log if it's already visible
			RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("toggle-log-window"));
			if (tact->get_active()) {
				do_toggle = false;
			}
		}
		_log_not_acknowledged = LogLevelNone;
		error_blink (false); // immediate acknowledge
	}
	// maybe fall through to to button toggle
	return !do_toggle;
}

void
ARDOUR_UI::error_blink (bool onoff)
{
	switch (_log_not_acknowledged) {
		case LogLevelError:
			// blink
			if (onoff) {
				error_alert_button.set_custom_led_color(0xff0000ff); // bright red
			} else {
				error_alert_button.set_custom_led_color(0x880000ff); // dark red
			}
			break;
		case LogLevelWarning:
			error_alert_button.set_custom_led_color(0xccaa00ff); // yellow
			break;
		case LogLevelInfo:
			error_alert_button.set_custom_led_color(0x88cc00ff); // lime green
			break;
		default:
			error_alert_button.set_custom_led_color(0x333333ff); // gray
			break;
	}
}

void
ARDOUR_UI::set_punch_sensitivity ()
{
	bool can_punch = _session && _session->punch_is_possible() && _session->locations()->auto_punch_location ();
	ActionManager::get_action ("Transport", "TogglePunchIn")->set_sensitive (can_punch);
	ActionManager::get_action ("Transport", "TogglePunchOut")->set_sensitive (can_punch);
}

void
ARDOUR_UI::editor_realized ()
{
	std::function<void (string)> pc (std::bind (&ARDOUR_UI::parameter_changed, this, _1));
	Config->map_parameters (pc);

	UIConfiguration::instance().reset_dpi ();
}

void
ARDOUR_UI::maximise_editing_space ()
{
	if (editor) {
		editor->maximise_editing_space ();
	}
}

void
ARDOUR_UI::restore_editing_space ()
{
	if (editor) {
		editor->restore_editing_space ();
	}
}

void
ARDOUR_UI::show_ui_prefs ()
{
	if (rc_option_editor) {
		show_tabbable (rc_option_editor);
		rc_option_editor->set_current_page (_("Appearance"));
	}
}

void
ARDOUR_UI::show_mixer_prefs ()
{
	if (rc_option_editor) {
		show_tabbable (rc_option_editor);
		rc_option_editor->set_current_page (_("Signal Flow"));
	}
}

void
ARDOUR_UI::show_plugin_prefs ()
{
	if (rc_option_editor) {
		show_tabbable (rc_option_editor);
		rc_option_editor->set_current_page (_("Plugins"));
	}
}

bool
ARDOUR_UI::click_button_clicked (GdkEventButton* ev)
{
	if (ev->button != 3) {
		/* this handler is just for button-3 clicks */
		return false;
	}

	show_tabbable (rc_option_editor);
	rc_option_editor->set_current_page (_("Metronome"));
	return true;
}

void
ARDOUR_UI::toggle_follow_edits ()
{
	RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Transport"), X_("ToggleFollowEdits"));
	UIConfiguration::instance().set_follow_edits (tact->get_active ());
}

void
ARDOUR_UI::update_title ()
{
	stringstream snap_label;
	snap_label << X_("<span weight=\"ultralight\">")
	           << _("Name")
	           << X_("</span>: ");

	if (_session) {
		bool dirty = _session->dirty();

		string session_name;

		if (_session->snap_name() != _session->name()) {
			session_name = _session->snap_name();
		} else {
			session_name = _session->name();
		}

		if (dirty) {
			session_name = "*" + session_name;
		}

		WindowTitle title (session_name);
		title += Glib::get_application_name();
		_main_window.set_title (title.get_string());

		snap_label << Gtkmm2ext::markup_escape_text (session_name);
	} else {
		WindowTitle title (Glib::get_application_name());
		_main_window.set_title (title.get_string());
		snap_label << "-";
	}
	snapshot_name_label.set_markup (snap_label.str());
}

