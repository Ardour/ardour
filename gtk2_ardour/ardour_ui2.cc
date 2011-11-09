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
#include <gtkmm2ext/cairocell.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/tearoff.h>

#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/ardour.h"
#include "ardour/profile.h"
#include "ardour/route.h"

#include "ardour_ui.h"
#include "keyboard.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "actions.h"
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

int
ARDOUR_UI::setup_windows ()
{
	if (create_editor ()) {
		error << _("UI: cannot setup editor") << endmsg;
		return -1;
	}

	if (create_mixer ()) {
		error << _("UI: cannot setup mixer") << endmsg;
		return -1;
	}

	/* all other dialogs are created conditionally */

	we_have_dependents ();

	theme_manager->signal_unmap().connect (sigc::bind (sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleThemeManager")));

#ifdef TOP_MENUBAR
	HBox* status_bar_packer = manage (new HBox);
	EventBox* status_bar_event_box = manage (new EventBox);

	status_bar_event_box->add (status_bar_label);
	status_bar_event_box->add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	status_bar_label.set_size_request (300, -1);
	status_bar_packer->pack_start (*status_bar_event_box, true, true, 6);
	status_bar_packer->pack_start (error_log_button, false, false);

	status_bar_label.show ();
	status_bar_event_box->show ();
	status_bar_packer->show ();
	error_log_button.show ();

	error_log_button.signal_clicked().connect (mem_fun (*this, &UI::toggle_errors));
	status_bar_event_box->signal_button_press_event().connect (mem_fun (*this, &ARDOUR_UI::status_bar_button_press));

	editor->get_status_bar_packer().pack_start (*status_bar_packer, true, true);
	editor->get_status_bar_packer().pack_start (menu_bar_base, false, false, 6);
#else
	top_packer.pack_start (menu_bar_base, false, false);
#endif

	top_packer.pack_start (transport_frame, false, false);

	editor->add_toplevel_controls (top_packer);

	setup_transport();
	build_menu_bar ();

	setup_tooltips ();

	return 0;
}

void
ARDOUR_UI::setup_tooltips ()
{
	set_tip (roll_button, _("Play from playhead"));
	set_tip (stop_button, _("Stop playback"));
	set_tip (rec_button, _("Toggle record"));
	set_tip (play_selection_button, _("Play range/selection"));
	set_tip (join_play_range_button, _("Always play range/selection"));
	set_tip (goto_start_button, _("Go to start of session"));
	set_tip (goto_end_button, _("Go to end of session"));
	set_tip (auto_loop_button, _("Play loop range"));

	set_tip (auto_return_button, _("Return to last playback start when stopped"));
	set_tip (auto_play_button, _("Start playback after any locate"));
	set_tip (auto_input_button, _("Be sensible about input monitoring"));
	set_tip (click_button, _("Enable/Disable audio click"));
	set_tip (time_master_button, string_compose (_("Does %1 control the time?"), PROGRAM_NAME));
	set_tip (solo_alert_button, _("When active, something is soloed.\nClick to de-solo everything"));
	set_tip (auditioning_alert_button, _("When active, auditioning is taking place\nClick to stop the audition"));
	set_tip (primary_clock, _("Primary Clock"));
	set_tip (secondary_clock, _("Secondary Clock"));

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
#ifdef TOP_MENUBAR

	if (strcmp (prefix, _("[ERROR]: ")) == 0) {
		text = "<span color=\"red\" weight=\"bold\">";
	} else if (strcmp (prefix, _("[WARNING]: ")) == 0) {
		text = "<span color=\"yellow\" weight=\"bold\">";
	} else if (strcmp (prefix, _("[INFO]: ")) == 0) {
		text = "<span color=\"green\" weight=\"bold\">";
	} else {
		text = "<span color=\"white\" weight=\"bold\">???";
	}

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

void
ARDOUR_UI::setup_transport ()
{
	RefPtr<Action> act;

	transport_tearoff = manage (new TearOff (transport_tearoff_hbox));
	transport_tearoff->set_name ("TransportBase");
	transport_tearoff->tearoff_window().signal_key_press_event().connect (sigc::bind (sigc::ptr_fun (relay_key_press), &transport_tearoff->tearoff_window()), false);

	if (Profile->get_sae()) {
		transport_tearoff->set_can_be_torn_off (false);
	}

	transport_hbox.pack_start (*transport_tearoff, true, false);

	transport_base.set_name ("TransportBase");
	transport_base.add (transport_hbox);

	transport_frame.set_shadow_type (SHADOW_OUT);
	transport_frame.set_name ("BaseFrame");
	transport_frame.add (transport_base);

	transport_tearoff->Detach.connect (sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::detach_tearoff), static_cast<Box*>(&top_packer),
						 static_cast<Widget*>(&transport_frame)));
	transport_tearoff->Attach.connect (sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::reattach_tearoff), static_cast<Box*> (&top_packer),
						 static_cast<Widget*> (&transport_frame), 1));
	transport_tearoff->Hidden.connect (sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::detach_tearoff), static_cast<Box*>(&top_packer),
						 static_cast<Widget*>(&transport_frame)));
	transport_tearoff->Visible.connect (sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::reattach_tearoff), static_cast<Box*> (&top_packer),
						  static_cast<Widget*> (&transport_frame), 1));

	auto_return_button.set_text(_("Auto Return"));
	auto_play_button.set_text(_("Auto Play"));
	auto_input_button.set_text (_("Auto Input"));

	click_button.set_image (get_icon (X_("metronome")));
	act = ActionManager::get_action ("Transport", "ToggleClick");
	click_button.set_related_action (act);
	click_button.signal_button_press_event().connect (sigc::mem_fun (*this, &ARDOUR_UI::click_button_clicked), false);

	auto_return_button.set_name ("transport option button");
	auto_play_button.set_name ("transport option button");
	auto_input_button.set_name ("transport option button");
	click_button.set_name ("transport option button");

	time_master_button.set_name ("TransportButton");
	sync_button.set_name ("TransportSyncButton");

	stop_button.set_active_state (Active);

	goto_start_button.set_image (get_icon (X_("transport_start")));
	goto_end_button.set_image (get_icon (X_("transport_end")));
	roll_button.set_image (get_icon (X_("transport_play")));
	stop_button.set_image (get_icon (X_("transport_stop")));
	play_selection_button.set_image (get_icon (X_("transport_range")));
	rec_button.set_image (get_icon (X_("transport_record")));
	auto_loop_button.set_image (get_icon (X_("transport_loop")));
	join_play_range_button.set_image (get_icon (X_("tool_object_range")));

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


	act = ActionManager::get_action (X_("Transport"), X_("ToggleTimeMaster"));
	act->connect_proxy (time_master_button);
	act = ActionManager::get_action (X_("Transport"), X_("ToggleExternalSync"));
	act->connect_proxy (sync_button);


	/* clocks, etc. */

	ARDOUR_UI::Clock.connect (sigc::bind (sigc::mem_fun (primary_clock, &AudioClock::set), 'p'));
	ARDOUR_UI::Clock.connect (sigc::bind (sigc::mem_fun (secondary_clock, &AudioClock::set), 's'));

	primary_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::primary_clock_value_changed));
	secondary_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::secondary_clock_value_changed));
	big_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::big_clock_value_changed));

	act = ActionManager::get_action ("Transport", "ToggleAutoReturn");
	auto_return_button.set_related_action (act);
	act = ActionManager::get_action ("Transport", "ToggleAutoPlay");
	auto_play_button.set_related_action (act);
	act = ActionManager::get_action ("Transport", "ToggleAutoInput");
	auto_input_button.set_related_action (act);

	/* alerts */

	/* CANNOT sigc::bind these to clicked or toggled, must use pressed or released */

	solo_alert_button.set_name ("TransportSoloAlert");
	solo_alert_button.signal_button_press_event().connect (sigc::mem_fun(*this,&ARDOUR_UI::solo_alert_press), false);
	auditioning_alert_button.set_name ("TransportAuditioningAlert");
	auditioning_alert_button.signal_button_press_event().connect (sigc::mem_fun(*this,&ARDOUR_UI::audition_alert_press), false);

	alert_box.pack_start (solo_alert_button, false, false);
	alert_box.pack_start (auditioning_alert_button, false, false);

	HBox* tbox = manage (new HBox);
	tbox->set_spacing (2);

	transport_tearoff_hbox.set_border_width (3);

	tbox->pack_start (goto_start_button, false, false);
	tbox->pack_start (goto_end_button, false, false);

	/* translators: Egternal is "External" with a descender character */
	set_size_request_to_display_given_text (sync_button, X_("Egternal"), 4, 10);

	// transport_tearoff_hbox.pack_start (*svbox, false, false, 3);

	Glib::RefPtr<SizeGroup> transport_button_size_group1 = SizeGroup::create (SIZE_GROUP_HORIZONTAL);
	transport_button_size_group1->add_widget (goto_start_button);
	transport_button_size_group1->add_widget (goto_end_button);
	transport_button_size_group1->add_widget (auto_loop_button);
	transport_button_size_group1->add_widget (rec_button);

	if (Profile->get_sae()) {
		tbox->pack_start (auto_loop_button);
		tbox->pack_start (roll_button);
		transport_button_size_group1->add_widget (play_selection_button);
		transport_button_size_group1->add_widget (roll_button);

	} else {

		tbox->pack_start (auto_loop_button, false, false);

		Frame* jpframe = manage (new Frame);
		HBox* jpbox = manage (new HBox);

		jpframe->add (*jpbox);
		jpframe->set_shadow_type (SHADOW_NONE);
		
		jpbox->pack_start (play_selection_button, false, false);
		jpbox->pack_start (join_play_range_button, false, false);
		jpbox->pack_start (roll_button, false, false);

		tbox->pack_start (*jpframe, false, false);

		Glib::RefPtr<SizeGroup> transport_button_size_group2 = SizeGroup::create (SIZE_GROUP_HORIZONTAL);
		transport_button_size_group2->add_widget (play_selection_button);
		transport_button_size_group2->add_widget (join_play_range_button);
		transport_button_size_group2->add_widget (roll_button);
	}

	tbox->pack_start (stop_button, false, false);
	tbox->pack_start (rec_button, false, false, 6);

	HBox* clock_box = manage (new HBox);
	clock_box->set_border_width (2);
	primary_clock->set_border_width (2);
	clock_box->pack_start (*primary_clock, false, false);
	if (!ARDOUR::Profile->get_small_screen()) {
		secondary_clock->set_border_width (2);
		clock_box->pack_start (*secondary_clock, false, false);
	}


	shuttle_box = new ShuttleControl;
	shuttle_box->show ();

	VBox* transport_vbox = manage (new VBox);
	transport_vbox->set_name ("TransportBase");
	transport_vbox->set_border_width (3);
	transport_vbox->set_spacing (3);
	transport_vbox->pack_start (*tbox, true, true, 0);
	transport_vbox->pack_start (*shuttle_box, false, false, 0);

	transport_tearoff_hbox.pack_start (*transport_vbox, false, false, 0);

	/* transport related toggle controls */

	HBox* toggle_box = manage(new HBox);
	VBox* auto_box = manage (new VBox);
	auto_box->set_spacing (2);
	auto_box->set_homogeneous (true);
	toggle_box->set_spacing (2);
	toggle_box->set_homogeneous (true);
	auto_box->pack_start (auto_play_button, true, false);
	auto_box->pack_start (auto_return_button, true, false);
        if (!Profile->get_small_screen()) {
                toggle_box->pack_start (*auto_box, false, false);
        }
	//VBox* io_box = manage (new VBox);
	//io_box->pack_start (auto_input_button, false, false);
	//io_box->pack_start (click_button, false, false);
        //if (!Profile->get_small_screen()) {
	// toggle_box->pack_start (*io_box, false, false);
        //}
	transport_tearoff_hbox.pack_start (*toggle_box, false, false, 4);

	Table* time_controls_table = manage (new Table (2, 2));
	time_controls_table->set_col_spacings (6);
	time_controls_table->attach (sync_button, 0, 1, 0, 1, Gtk::AttachOptions(FILL|EXPAND), Gtk::AttachOptions(0));
	time_controls_table->attach (time_master_button, 0, 1, 1, 2, Gtk::AttachOptions(FILL|EXPAND), Gtk::AttachOptions(0));

	time_controls_table->attach (click_button, 1, 2, 0, 2, Gtk::AttachOptions(FILL|EXPAND), FILL);

	transport_tearoff_hbox.pack_start (*clock_box, false, false);
	transport_tearoff_hbox.pack_start (*time_controls_table, false, false, 4);

	time_info_box = manage (new TimeInfoBox);
	transport_tearoff_hbox.pack_start (*time_info_box, false, false);


        if (Profile->get_small_screen()) {
                transport_tearoff_hbox.pack_start (_editor_transport_box, false, false, 4);
        }
	transport_tearoff_hbox.pack_start (alert_box, false, false);

	if (Profile->get_sae()) {
		Image* img = manage (new Image ((::get_icon (X_("sae")))));
		transport_tearoff_hbox.pack_end (*img, false, false, 6);
	}

	/* desensitize */

	set_transport_sensitivity (false);

	XMLNode* tnode = tearoff_settings ("transport");
	if (tnode) {
		transport_tearoff->set_state (*tnode);
	}
}

void
ARDOUR_UI::manage_window (Window& win)
{
	win.signal_delete_event().connect (sigc::bind (sigc::ptr_fun (just_hide_it), &win));
	win.signal_enter_notify_event().connect (sigc::bind (sigc::mem_fun (Keyboard::the_keyboard(), &Keyboard::enter_window), &win));
	win.signal_leave_notify_event().connect (sigc::bind (sigc::mem_fun (Keyboard::the_keyboard(), &Keyboard::leave_window), &win));
}

void
ARDOUR_UI::detach_tearoff (Box* b, Widget* w)
{
//	editor->ensure_float (transport_tearoff->tearoff_window());
	b->remove (*w);
}

void
ARDOUR_UI::reattach_tearoff (Box* b, Widget* w, int32_t n)
{
	b->pack_start (*w);
	b->reorder_child (*w, n);
}

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
	if (auditioning_alert_button.get_active() != onoff) {
		auditioning_alert_button.set_active (onoff);
		set_transport_sensitivity (!onoff);
	}
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
ARDOUR_UI::solo_alert_press (GdkEventButton*)
{
	if (_session) {
		if (_session->soloing()) {
			_session->set_solo (_session->get_routes(), false);
		} else if (_session->listening()) {
			_session->set_listen (_session->get_routes(), false);
		}
	}
	return true;
}

void
ARDOUR_UI::solo_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->soloing() || _session->listening()) {
		if (onoff) {
			solo_alert_button.set_state (STATE_ACTIVE);
		} else {
			solo_alert_button.set_state (STATE_NORMAL);
		}
	} else {
		solo_alert_button.set_active (false);
		solo_alert_button.set_state (STATE_NORMAL);
	}
}

void
ARDOUR_UI::sync_blink (bool onoff)
{
	if (_session == 0 || !_session->config.get_external_sync()) {
		/* internal sync */
		sync_button.set_visual_state (0);
		return;
	}

	if (!_session->transport_locked()) {
		/* not locked, so blink on and off according to the onoff argument */

		if (onoff) {
			sync_button.set_visual_state (1); // "-active"
		} else {
			sync_button.set_visual_state (0); // normal
		}
	} else {
		/* locked */
		sync_button.set_visual_state (1); // "-active"
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
			auditioning_alert_button.set_state (STATE_ACTIVE);
		} else {
			auditioning_alert_button.set_state (STATE_NORMAL);
		}
	} else {
		auditioning_alert_button.set_active (false);
		auditioning_alert_button.set_state (STATE_NORMAL);
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

	reset_dpi ();
}

void
ARDOUR_UI::maximise_editing_space ()
{
	if (!editor) {
		return;
	}

	transport_tearoff->set_visible (false);
	editor->maximise_editing_space ();
 	if (Config->get_keep_tearoffs()) {
		transport_tearoff->set_visible (true);
	}
}

void
ARDOUR_UI::restore_editing_space ()
{
	if (!editor) {
		return;
	}

	transport_tearoff->set_visible (true);
	editor->restore_editing_space ();
}

bool
ARDOUR_UI::click_button_clicked (GdkEventButton* ev)
{
	if (ev->button != 3) {
		/* this handler is just for button-3 clicks */
		return false;
	}

	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleRCOptionsEditor"));
	assert (act);

	RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic (act);
	tact->set_active ();

	rc_option_editor->set_current_page (_("Misc"));
	return true;
}
