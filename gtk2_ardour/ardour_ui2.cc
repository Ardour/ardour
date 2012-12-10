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
	set_tip (goto_start_button, _("Go to start of session"));
	set_tip (goto_end_button, _("Go to end of session"));
	set_tip (auto_loop_button, _("Play loop range"));
	set_tip (midi_panic_button, _("MIDI Panic\nSend note off and reset controller messages on all MIDI channels"));
	set_tip (auto_return_button, _("Return to last playback start when stopped"));
	set_tip (follow_edits_button, _("Playhead follows Range Selections and Edits"));
	set_tip (auto_input_button, _("Be sensible about input monitoring"));
	set_tip (click_button, _("Enable/Disable audio click"));
	set_tip (solo_alert_button, _("When active, something is soloed.\nClick to de-solo everything"));
	set_tip (auditioning_alert_button, _("When active, auditioning is taking place\nClick to stop the audition"));
	set_tip (feedback_alert_button, _("When active, there is a feedback loop."));
	set_tip (primary_clock, _("<b>Primary Clock</b> right-click to set display mode. Click to edit, click+drag a digit or mouse-over+scroll wheel to modify.\nText edits: right-to-left overwrite <tt>Esc</tt>: cancel; <tt>Enter</tt>: confirm; postfix the edit with '+' or '-' to enter delta times.\nSee <span color=\"blue\">http://ardour.org/a3_features_clocks</span> for details."));
	set_tip (secondary_clock, _("<b>Secondary Clock</b> right-click to set display mode. Click to edit, click+drag a digit or mouse-over+scroll wheel to modify.\nText edits: right-to-left overwrite <tt>Esc</tt>: cancel; <tt>Enter</tt>: confirm; postfix the edit with '+' or '-' to enter delta times.\nSee <span color=\"blue\">http://ardour.org/a3_features_clocks</span> for details."));

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

	transport_tearoff_hbox.set_border_width (3);
	transport_tearoff_hbox.set_spacing (3);

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

	follow_edits_button.set_text(_("Follow Edits"));

//	auto_input_button.set_text (_("Auto Input"));

	click_button.set_image (get_icon (X_("metronome")));
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

	goto_start_button.set_image (get_icon (X_("transport_start")));
	goto_end_button.set_image (get_icon (X_("transport_end")));
	roll_button.set_image (get_icon (X_("transport_play")));
	stop_button.set_image (get_icon (X_("transport_stop")));
	play_selection_button.set_image (get_icon (X_("transport_range")));
	rec_button.set_image (get_icon (X_("transport_record")));
	auto_loop_button.set_image (get_icon (X_("transport_loop")));

	midi_panic_button.set_image (get_icon (X_("midi_panic")));
	/* the icon for this has an odd aspect ratio, so fatten up the button */
	midi_panic_button.set_size_request (25, -1);
	
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
	solo_alert_button.signal_button_press_event().connect (sigc::mem_fun(*this,&ARDOUR_UI::solo_alert_press), false);
	auditioning_alert_button.set_name ("rude audition");
	auditioning_alert_button.signal_button_press_event().connect (sigc::mem_fun(*this,&ARDOUR_UI::audition_alert_press), false);
	feedback_alert_button.set_name ("feedback alert");
	feedback_alert_button.signal_button_press_event().connect (sigc::mem_fun (*this, &ARDOUR_UI::feedback_alert_press), false);

	alert_box.pack_start (solo_alert_button, true, false);
	alert_box.pack_start (auditioning_alert_button, true, false);
	alert_box.pack_start (feedback_alert_button, true, false);

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

	goto_start_button.set_size_request (-1, 40);

	HBox* tbox1 = manage (new HBox);
	HBox* tbox2 = manage (new HBox);
	HBox* tbox = manage (new HBox);

	VBox* vbox1 = manage (new VBox);
	VBox* vbox2 = manage (new VBox);

	Alignment* a1 = manage (new Alignment);
	Alignment* a2 = manage (new Alignment);

	tbox1->set_spacing (2);
	tbox2->set_spacing (2);
	tbox->set_spacing (2);

	tbox1->pack_start (midi_panic_button, false, false, 5);
	tbox1->pack_start (click_button, false, false, 5);
	tbox1->pack_start (goto_start_button, false, false);
	tbox1->pack_start (goto_end_button, false, false);
	tbox1->pack_start (auto_loop_button, false, false);

	tbox2->pack_start (play_selection_button, false, false);
	tbox2->pack_start (roll_button, false, false);
	tbox2->pack_start (stop_button, false, false);
	tbox2->pack_start (rec_button, false, false, 5);

	vbox1->pack_start (*tbox1, false, false);
	vbox2->pack_start (*tbox2, false, false);

	a1->add (*vbox1);
	a1->set (0.5, 1.0, 0.0, 0.0);
	a2->add (*vbox2);
	a2->set (0.5, 1.0, 0.0, 0.0);

	tbox->pack_start (*a1, false, false);
	tbox->pack_start (*a2, false, false);

	HBox* clock_box = manage (new HBox);

	clock_box->pack_start (*primary_clock, false, false);
	if (!ARDOUR::Profile->get_small_screen()) {
		clock_box->pack_start (*secondary_clock, false, false);
	}
	clock_box->set_spacing (3);

	shuttle_box = new ShuttleControl;
	shuttle_box->show ();

	VBox* transport_vbox = manage (new VBox);
	transport_vbox->set_name ("TransportBase");
	transport_vbox->set_border_width (0);
	transport_vbox->set_spacing (3);
	transport_vbox->pack_start (*tbox, true, true, 0);
	transport_vbox->pack_start (*shuttle_box, false, false, 0);

	transport_tearoff_hbox.pack_start (*transport_vbox, false, false);

	/* transport related toggle controls */

	VBox* auto_box = manage (new VBox);
	auto_box->set_homogeneous (true);
	auto_box->set_spacing (2);
	auto_box->pack_start (sync_button, false, false);
	auto_box->pack_start (follow_edits_button, false, false);
	auto_box->pack_start (auto_return_button, false, false);

	transport_tearoff_hbox.pack_start (*auto_box, false, false);
	transport_tearoff_hbox.pack_start (*clock_box, true, true);

	time_info_box = manage (new TimeInfoBox);
	transport_tearoff_hbox.pack_start (*time_info_box, false, false);

        if (Profile->get_small_screen()) {
                transport_tearoff_hbox.pack_start (_editor_transport_box, false, false);
        }
	transport_tearoff_hbox.pack_start (alert_box, false, false);

	if (Profile->get_sae()) {
		Image* img = manage (new Image ((::get_icon (X_("sae")))));
		transport_tearoff_hbox.pack_end (*img, false, false);
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

bool
ARDOUR_UI::feedback_alert_press (GdkEventButton *)
{
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
ARDOUR_UI::update_tearoff_visibility ()
{
	if (editor) {
		editor->update_tearoff_visibility ();
	}
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

void
ARDOUR_UI::toggle_always_play_range ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Transport"), X_("ToggleFollowEdits"));
	assert (act);

	RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic (act);
	assert (tact);

	Config->set_always_play_range (tact->get_active ());
}

	
