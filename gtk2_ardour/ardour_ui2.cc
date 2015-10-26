/*
    Copyright (C) 1999 Paul Davis

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

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <cmath>

#include <sigc++/bind.h>
#include "pbd/error.h"
#include "pbd/basename.h"
#include "pbd/fastlog.h"

#include "gtkmm2ext/cairocell.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/click_box.h"
#include "gtkmm2ext/window_title.h"

#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "ardour_ui.h"
#include "keyboard.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "actions.h"
#include "main_clock.h"
#include "utils.h"
#include "theme_manager.h"
#include "midi_tracer.h"
#include "shuttle_control.h"
#include "global_port_matrix.h"
#include "location_ui.h"
#include "rc_option_editor.h"
#include "time_info_box.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Glib;
using namespace ARDOUR_UI_UTILS;


bool
ARDOUR_UI::tabs_button_event (GdkEventButton* ev)
{
	std::vector<Widget*> children = _tabs.get_children();

	for (std::vector<Widget*>::iterator w = children.begin(); w != children.end(); ++w) {

		Gtk::Widget* close_button = reinterpret_cast<Gtk::Widget*> ((*w)->get_data ("close-button"));

		if (close_button) {

			Gtk::Allocation alloc (close_button->get_allocation());
			int dx, dy;

			/* Allocation origin uses toplevel window coordinates;
			 * event origin uses _tabs-centric coordinate space, so
			 * translate before computing if event is inside the
			 * close button.
			 */

			close_button->get_toplevel()->translate_coordinates (_tabs, alloc.get_x(), alloc.get_y(), dx, dy);

			if (ev->x >= dx &&
			    ev->y >= dy &&
			    ev->x < dx + alloc.get_width() &&
			    ev->y < dy + alloc.get_height()) {
				if (close_button->event ((GdkEvent*) ev)) {
					return true;
				}
			}
		}
	}

	return false;
}

void
ARDOUR_UI::tabs_page_removed (Gtk::Widget*, guint)
{
	if (_tabs.get_n_pages() == 1) {
		_tabs.set_show_tabs (false);
	} else {
		_tabs.set_show_tabs (true);
	}
}

void
ARDOUR_UI::tabs_page_added (Gtk::Widget*, guint)
{
	if (_tabs.get_n_pages() == 1) {
		_tabs.set_show_tabs (false);
	} else {
		_tabs.set_show_tabs (true);
	}
}

void
ARDOUR_UI::tabs_switch (GtkNotebookPage*, guint page_number)
{
}

void
ARDOUR_UI::setup_tooltips ()
{
	set_tip (roll_button, _("Play from playhead"));
	set_tip (stop_button, _("Stop playback"));
	set_tip (rec_button, _("Toggle record"));
	set_tip (play_selection_button, _("Play range/selection"));
	set_tip (goto_start_button, _("Go to start of session"));
	set_tip (goto_end_button, _("Go to end of session"));
	set_tip (auto_loop_button, _("Play loop range"));
	set_tip (midi_panic_button, _("MIDI Panic\nSend note off and reset controller messages on all MIDI channels"));
	set_tip (auto_return_button, _("Return to last playback start when stopped"));
	set_tip (follow_edits_button, _("Playhead follows range selections and edits"));
	set_tip (auto_input_button, _("Be sensible about input monitoring"));
	set_tip (click_button, _("Enable/Disable audio click"));
	set_tip (solo_alert_button, _("When active, something is soloed.\nClick to de-solo everything"));
	set_tip (auditioning_alert_button, _("When active, auditioning is taking place.\nClick to stop the audition"));
	set_tip (feedback_alert_button, _("When active, there is a feedback loop."));
	set_tip (primary_clock, _("<b>Primary Clock</b> right-click to set display mode. Click to edit, click+drag a digit or mouse-over+scroll wheel to modify.\nText edits: right-to-left overwrite <tt>Esc</tt>: cancel; <tt>Enter</tt>: confirm; postfix the edit with '+' or '-' to enter delta times.\n"));
	set_tip (secondary_clock, _("<b>Secondary Clock</b> right-click to set display mode. Click to edit, click+drag a digit or mouse-over+scroll wheel to modify.\nText edits: right-to-left overwrite <tt>Esc</tt>: cancel; <tt>Enter</tt>: confirm; postfix the edit with '+' or '-' to enter delta times.\n"));
	set_tip (editor_meter_peak_display, _("Reset All Peak Indicators"));
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
ARDOUR_UI::display_message (const char *prefix, gint prefix_len, RefPtr<TextBuffer::Tag> ptag, RefPtr<TextBuffer::Tag> mtag, const char *msg)
{
	string text;

	UI::display_message (prefix, prefix_len, ptag, mtag, msg);

	ArdourLogLevel ll = LogLevelNone;

	if (strcmp (prefix, _("[ERROR]: ")) == 0) {
		text = "<span color=\"red\" weight=\"bold\">";
		ll = LogLevelError;
	} else if (strcmp (prefix, _("[WARNING]: ")) == 0) {
		text = "<span color=\"yellow\" weight=\"bold\">";
		ll = LogLevelWarning;
	} else if (strcmp (prefix, _("[INFO]: ")) == 0) {
		text = "<span color=\"green\" weight=\"bold\">";
		ll = LogLevelInfo;
	} else {
		text = "<span color=\"white\" weight=\"bold\">???";
	}

	_log_not_acknowledged = std::max(_log_not_acknowledged, ll);

#ifdef TOP_MENUBAR
	text += prefix;
	text += "</span>";
	text += msg;

	status_bar_label.set_markup (text);
#endif
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

void
ARDOUR_UI::setup_transport ()
{
	RefPtr<Action> act;

	transport_hbox.set_border_width (PX_SCALE(3));
	transport_hbox.set_spacing (PX_SCALE(3));

	transport_base.set_name ("TransportBase");
	transport_base.add (transport_hbox);

	transport_frame.set_shadow_type (SHADOW_OUT);
	transport_frame.set_name ("BaseFrame");
	transport_frame.add (transport_base);

	auto_return_button.set_text(_("Auto Return"));

	follow_edits_button.set_text(_("Follow Edits"));

//	auto_input_button.set_text (_("Auto Input"));

	click_button.set_icon (ArdourIcon::TransportMetronom);

	act = ActionManager::get_action ("Transport", "ToggleClick");
	click_button.set_related_action (act);
	click_button.signal_button_press_event().connect (sigc::mem_fun (*this, &ARDOUR_UI::click_button_clicked), false);

	auto_return_button.set_name ("transport option button");
	follow_edits_button.set_name ("transport option button");
	auto_input_button.set_name ("transport option button");

	/* these have to provide a clear indication of active state */

	click_button.set_name ("transport button");
	sync_button.set_name ("transport active option button");

	stop_button.set_active (true);

	goto_start_button.set_icon (ArdourIcon::TransportStart);
	goto_end_button.set_icon (ArdourIcon::TransportEnd);
	roll_button.set_icon (ArdourIcon::TransportPlay);
	stop_button.set_icon (ArdourIcon::TransportStop);
	play_selection_button.set_icon (ArdourIcon::TransportRange);
	auto_loop_button.set_icon (ArdourIcon::TransportLoop);
	rec_button.set_icon (ArdourIcon::RecButton);
	midi_panic_button.set_icon (ArdourIcon::TransportPanic);

	act = ActionManager::get_action (X_("Transport"), X_("Stop"));
	stop_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("Roll"));
	roll_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("Record"));
	rec_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("GotoStart"));
	goto_start_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("GotoEnd"));
	goto_end_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("Loop"));
	auto_loop_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("PlaySelection"));
	play_selection_button.set_related_action (act);
	act = ActionManager::get_action (X_("MIDI"), X_("panic"));
	midi_panic_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("ToggleExternalSync"));
	sync_button.set_related_action (act);

	/* clocks, etc. */

	ARDOUR_UI::Clock.connect (sigc::mem_fun (primary_clock, &AudioClock::set));
	ARDOUR_UI::Clock.connect (sigc::mem_fun (secondary_clock, &AudioClock::set));

	primary_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::primary_clock_value_changed));
	secondary_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::secondary_clock_value_changed));
	big_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::big_clock_value_changed));

	act = ActionManager::get_action ("Transport", "ToggleAutoReturn");
	auto_return_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("ToggleFollowEdits"));
	follow_edits_button.set_related_action (act);
	act = ActionManager::get_action ("Transport", "ToggleAutoInput");
	auto_input_button.set_related_action (act);

	/* alerts */

	/* CANNOT sigc::bind these to clicked or toggled, must use pressed or released */

	solo_alert_button.set_name ("rude solo");
	act = ActionManager::get_action (X_("Main"), X_("cancel-solo"));
	solo_alert_button.set_related_action (act);
	auditioning_alert_button.set_name ("rude audition");
	auditioning_alert_button.signal_button_press_event().connect (sigc::mem_fun(*this,&ARDOUR_UI::audition_alert_press), false);
	feedback_alert_button.set_name ("feedback alert");
	feedback_alert_button.signal_button_press_event().connect (sigc::mem_fun (*this, &ARDOUR_UI::feedback_alert_press), false);
	error_alert_button.set_name ("error alert");
	error_alert_button.signal_button_release_event().connect (sigc::mem_fun(*this,&ARDOUR_UI::error_alert_press), false);
	act = ActionManager::get_action (X_("Editor"), X_("toggle-log-window"));
	error_alert_button.set_related_action(act);
	error_alert_button.set_fallthrough_to_parent(true);

	alert_box.set_homogeneous (true);
	alert_box.set_spacing (PX_SCALE(2));
	alert_box.pack_start (solo_alert_button, true, true);
	alert_box.pack_start (auditioning_alert_button, true, true);
	alert_box.pack_start (feedback_alert_button, true, true);

	/* all transport buttons should be the same size vertically and
	 * horizontally
	 */

	Glib::RefPtr<SizeGroup> transport_button_size_group = SizeGroup::create (SIZE_GROUP_BOTH);
	transport_button_size_group->add_widget (goto_start_button);
	transport_button_size_group->add_widget (goto_end_button);
	transport_button_size_group->add_widget (auto_loop_button);
	transport_button_size_group->add_widget (rec_button);
	transport_button_size_group->add_widget (play_selection_button);
	transport_button_size_group->add_widget (roll_button);
	transport_button_size_group->add_widget (stop_button);

	/* the icon for this has an odd aspect ratio, so fatten up the button */
	midi_panic_button.set_size_request (PX_SCALE(25), -1);
	goto_start_button.set_size_request (PX_SCALE(28), PX_SCALE(44));
	click_button.set_size_request (PX_SCALE(32), PX_SCALE(44));


	HBox* tbox1 = manage (new HBox);
	HBox* tbox2 = manage (new HBox);
	HBox* tbox = manage (new HBox);

	VBox* vbox1 = manage (new VBox);
	VBox* vbox2 = manage (new VBox);

	Alignment* a1 = manage (new Alignment);
	Alignment* a2 = manage (new Alignment);

	tbox1->set_spacing (PX_SCALE(2));
	tbox2->set_spacing (PX_SCALE(2));
	tbox->set_spacing (PX_SCALE(2));

	if (!Profile->get_trx()) {
		tbox1->pack_start (midi_panic_button, true, true, 5);
		tbox1->pack_start (click_button, true, true, 5);
	}

	tbox1->pack_start (goto_start_button, true, true);
	tbox1->pack_start (goto_end_button, true, true);
	tbox1->pack_start (auto_loop_button, true, true);

	if (!Profile->get_trx()) {
		tbox2->pack_start (play_selection_button, true, true);
	}
	tbox2->pack_start (roll_button, true, true);
	tbox2->pack_start (stop_button, true, true);
	tbox2->pack_start (rec_button, true, true, 5);

	vbox1->pack_start (*tbox1, true, true);
	vbox2->pack_start (*tbox2, true, true);

	a1->add (*vbox1);
	a1->set (0.5, 0.5, 0.0, 1.0);
	a2->add (*vbox2);
	a2->set (0.5, 0.5, 0.0, 1.0);

	tbox->pack_start (*a1, false, false);
	tbox->pack_start (*a2, false, false);

	HBox* clock_box = manage (new HBox);

	clock_box->pack_start (*primary_clock, false, false);
	if (!ARDOUR::Profile->get_small_screen() && !ARDOUR::Profile->get_trx()) {
		clock_box->pack_start (*secondary_clock, false, false);
	}
	clock_box->set_spacing (PX_SCALE(3));

	shuttle_box = manage (new ShuttleControl);
	shuttle_box->show ();

	VBox* transport_vbox = manage (new VBox);
	transport_vbox->set_name ("TransportBase");
	transport_vbox->set_border_width (0);
	transport_vbox->set_spacing (PX_SCALE(3));
	transport_vbox->pack_start (*tbox, true, true, 0);

	if (!Profile->get_trx()) {
		transport_vbox->pack_start (*shuttle_box, false, false, 0);
	}

	time_info_box = manage (new TimeInfoBox);

	transport_hbox.pack_start (*transport_vbox, false, true);

	/* transport related toggle controls */

	VBox* auto_box = manage (new VBox);
	auto_box->set_homogeneous (true);
	auto_box->set_spacing (PX_SCALE(2));
	auto_box->pack_start (sync_button, true, true);
	if (!ARDOUR::Profile->get_trx()) {
		auto_box->pack_start (follow_edits_button, true, true);
		auto_box->pack_start (auto_return_button, true, true);
	}

	if (!ARDOUR::Profile->get_trx()) {
		transport_hbox.pack_start (*auto_box, false, false);
	}
	transport_hbox.pack_start (*clock_box, true, true);

	if (ARDOUR::Profile->get_trx()) {
		transport_hbox.pack_start (*auto_box, false, false);
	}

	if (!ARDOUR::Profile->get_trx()) {
		transport_hbox.pack_start (*time_info_box, false, false);
	}

	if (!ARDOUR::Profile->get_trx()) {
		transport_hbox.pack_start (alert_box, false, false);
		transport_hbox.pack_start (meter_box, false, false);
		transport_hbox.pack_start (editor_meter_peak_display, false, false);
	}

	/* desensitize */

	set_transport_sensitivity (false);
}
#undef PX_SCALE

void
ARDOUR_UI::soloing_changed (bool onoff)
{
	if (solo_alert_button.get_active() != onoff) {
		solo_alert_button.set_active (onoff);
	}
}

void
ARDOUR_UI::_auditioning_changed (bool onoff)
{
	auditioning_alert_button.set_active (onoff);
	set_transport_sensitivity (!onoff);
}

void
ARDOUR_UI::auditioning_changed (bool onoff)
{
	UI::instance()->call_slot (MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::_auditioning_changed, this, onoff));
}

bool
ARDOUR_UI::audition_alert_press (GdkEventButton*)
{
	if (_session) {
		_session->cancel_audition();
	}
	return true;
}

bool
ARDOUR_UI::feedback_alert_press (GdkEventButton *)
{
	return true;
}

bool
ARDOUR_UI::error_alert_press (GdkEventButton* ev)
{
	bool do_toggle = true;
	if (ev->button == 1) {
		if (_log_not_acknowledged == LogLevelError) {
			// just acknowledge the error, don't hide the log if it's already visible
			RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-log-window"));
			Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
			if (tact && tact->get_active()) {
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
ARDOUR_UI::solo_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->soloing() || _session->listening()) {
		if (onoff) {
			solo_alert_button.set_active (true);
		} else {
			solo_alert_button.set_active (false);
		}
	} else {
		solo_alert_button.set_active (false);
	}
}

void
ARDOUR_UI::sync_blink (bool onoff)
{
	if (_session == 0 || !_session->config.get_external_sync()) {
		/* internal sync */
		sync_button.set_active (false);
		return;
	}

	if (!_session->transport_locked()) {
		/* not locked, so blink on and off according to the onoff argument */

		if (onoff) {
			sync_button.set_active (true);
		} else {
			sync_button.set_active (false);
		}
	} else {
		/* locked */
		sync_button.set_active (true);
	}
}

void
ARDOUR_UI::audition_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->is_auditioning()) {
		if (onoff) {
			auditioning_alert_button.set_active (true);
		} else {
			auditioning_alert_button.set_active (false);
		}
	} else {
		auditioning_alert_button.set_active (false);
	}
}

void
ARDOUR_UI::feedback_blink (bool onoff)
{
	if (_feedback_exists) {
		if (onoff) {
			feedback_alert_button.set_active (true);
		} else {
			feedback_alert_button.set_active (false);
		}
	} else {
		feedback_alert_button.set_active (false);
	}
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
ARDOUR_UI::set_transport_sensitivity (bool yn)
{
	ActionManager::set_sensitive (ActionManager::transport_sensitive_actions, yn);
	shuttle_box->set_sensitive (yn);
}

void
ARDOUR_UI::editor_realized ()
{
	boost::function<void (string)> pc (boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
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
		rc_option_editor->set_current_page (_("GUI"));
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
	rc_option_editor->set_current_page (_("Misc"));
	return true;
}

void
ARDOUR_UI::toggle_follow_edits ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Transport"), X_("ToggleFollowEdits"));
	assert (act);

	RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic (act);
	assert (tact);

	UIConfiguration::instance().set_follow_edits (tact->get_active ());
}

void
ARDOUR_UI::update_title ()
{
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
	} else {
		WindowTitle title (Glib::get_application_name());
		_main_window.set_title (title.get_string());
	}

}

