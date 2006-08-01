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

    $Id$
*/

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <cmath>

#include <sigc++/bind.h>
#include <pbd/error.h>
#include <pbd/basename.h>
#include <pbd/fastlog.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/tearoff.h>

#include <ardour/audioengine.h>
#include <ardour/ardour.h>
#include <ardour/route.h>

#include "ardour_ui.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "actions.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Glib;
using namespace sigc;

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

	setup_clock ();
	setup_transport();
	setup_adjustables ();
	build_menu_bar ();

	top_packer.pack_start (menu_bar_base, false, false);
	top_packer.pack_start (transport_frame, false, false);

	editor->add_toplevel_controls (top_packer);

	return 0;
}

void
ARDOUR_UI::setup_adjustables ()
{
	adjuster_table.set_homogeneous (true);

	online_control_strings.push_back (_("MMC + Local"));
	online_control_strings.push_back (_("MMC"));
	online_control_strings.push_back (_("Local"));

	online_control_button = new GlobalClickBox ("CONTROL",
						    online_control_strings);

	online_control_button->adjustment.signal_value_changed().connect(mem_fun(*this,&ARDOUR_UI::control_methods_adjusted));

	mmc_id_strings.push_back ("1");
	mmc_id_strings.push_back ("2");
	mmc_id_strings.push_back ("3");
	mmc_id_strings.push_back ("4");
	mmc_id_strings.push_back ("5");
	mmc_id_strings.push_back ("6");
	mmc_id_strings.push_back ("7");
	mmc_id_strings.push_back ("8");
	mmc_id_strings.push_back ("9");

	mmc_id_button = new GlobalClickBox (_("MMC ID"), mmc_id_strings);

	mmc_id_button->adjustment.signal_value_changed().connect (mem_fun(*this,&ARDOUR_UI::mmc_device_id_adjusted));

	adjuster_table.attach (*online_control_button, 0, 2, 1, 2, FILL|EXPAND, FILL, 5, 5);
	adjuster_table.attach (*mmc_id_button, 2, 3, 1, 2, FILL, FILL, 5, 5);
}

void
ARDOUR_UI::transport_stopped ()
{
	stop_button.set_active (true);
	
	roll_button.set_active (false);
	play_selection_button.set_active (false);
	auto_loop_button.set_active (false);

	shuttle_fract = 0;
	shuttle_box.queue_draw ();

	update_disk_space ();
}

static const double SHUTTLE_FRACT_SPEED1=0.48412291827; /* derived from A1,A2 */

void
ARDOUR_UI::transport_rolling ()
{
	stop_button.set_active (false);
	if (session->get_play_range()) {

		play_selection_button.set_active (true);
		roll_button.set_active (false);
		auto_loop_button.set_active (false);

	} else if (session->get_auto_loop ()) {

		auto_loop_button.set_active (true);
		play_selection_button.set_active (false);
		roll_button.set_active (false);

	} else {

		roll_button.set_active (true);
		play_selection_button.set_active (false);
		auto_loop_button.set_active (false);
	}

	/* reset shuttle controller */

	shuttle_fract = SHUTTLE_FRACT_SPEED1;  /* speed = 1.0, believe it or not */
	shuttle_box.queue_draw ();
}

void
ARDOUR_UI::transport_rewinding ()
{
	stop_button.set_active(false);
	roll_button.set_active (true);
	play_selection_button.set_active (false);
	auto_loop_button.set_active (false);
}

void
ARDOUR_UI::transport_forwarding ()
{
	stop_button.set_active (false);
	roll_button.set_active (true);
	play_selection_button.set_active (false);
	auto_loop_button.set_active (false);
}

void
ARDOUR_UI::setup_transport ()
{
	transport_tearoff = manage (new TearOff (transport_tearoff_hbox));
	transport_tearoff->set_name ("TransportBase");

	transport_hbox.pack_start (*transport_tearoff, true, false);

	transport_base.set_name ("TransportBase");
	transport_base.add (transport_hbox);

	transport_frame.set_shadow_type (SHADOW_OUT);
	transport_frame.set_name ("BaseFrame");
	transport_frame.add (transport_base);

	transport_tearoff->Detach.connect (bind (mem_fun(*this, &ARDOUR_UI::detach_tearoff), static_cast<Box*>(&top_packer), 
						 static_cast<Widget*>(&transport_frame)));
	transport_tearoff->Attach.connect (bind (mem_fun(*this, &ARDOUR_UI::reattach_tearoff), static_cast<Box*> (&top_packer), 
						 static_cast<Widget*> (&transport_frame), 1));
	transport_tearoff->Hidden.connect (bind (mem_fun(*this, &ARDOUR_UI::detach_tearoff), static_cast<Box*>(&top_packer), 
						 static_cast<Widget*>(&transport_frame)));
	transport_tearoff->Visible.connect (bind (mem_fun(*this, &ARDOUR_UI::reattach_tearoff), static_cast<Box*> (&top_packer), 
						  static_cast<Widget*> (&transport_frame), 1));
	
	shuttle_box.set_name ("TransportButton");
	goto_start_button.set_name ("TransportButton");
	goto_end_button.set_name ("TransportButton");
	roll_button.set_name ("TransportButton");
	stop_button.set_name ("TransportButton");
	play_selection_button.set_name ("TransportButton");
	rec_button.set_name ("TransportRecButton");
	auto_loop_button.set_name ("TransportButton");
	auto_return_button.set_name ("TransportButton");
	auto_play_button.set_name ("TransportButton");
	auto_input_button.set_name ("TransportButton");
	punch_in_button.set_name ("TransportButton");
	punch_out_button.set_name ("TransportButton");
	click_button.set_name ("TransportButton");
	time_master_button.set_name ("TransportButton");

	vector<Gdk::Color> colors;
	Gdk::Color c;

	/* record button has 3 color states, so we set 2 extra here */
	set_color(c, rgba_from_style ("TransportRecButton", 0xff, 0, 0, 0, "bg", Gtk::STATE_PRELIGHT, false ));
	colors.push_back (c);
	
	set_color(c, rgba_from_style ("TransportRecButton", 0xff, 0, 0, 0, "bg", Gtk::STATE_ACTIVE, false ));
	colors.push_back (c);
	
	rec_button.set_colors (colors);
	colors.clear ();
	
	/* other buttons get 2 color states, so add one here */
	set_color(c, rgba_from_style ("TransportButton", 0x7f, 0xff, 0x7f, 0, "bg", Gtk::STATE_ACTIVE, false ));
	colors.push_back (c);

	stop_button.set_colors (colors);
	roll_button.set_colors (colors);
	auto_loop_button.set_colors (colors);
	play_selection_button.set_colors (colors);
	goto_start_button.set_colors (colors);
	goto_end_button.set_colors (colors);
	
	Widget* w;

	stop_button.set_active (true);

	w = manage (new Image (Stock::MEDIA_PREVIOUS, ICON_SIZE_BUTTON));
	w->show();
	goto_start_button.add (*w);
	w = manage (new Image (Stock::MEDIA_NEXT, ICON_SIZE_BUTTON));
	w->show();
	goto_end_button.add (*w);
	w = manage (new Image (Stock::MEDIA_PLAY, ICON_SIZE_BUTTON));
	w->show();
	roll_button.add (*w);
	w = manage (new Image (Stock::MEDIA_STOP, ICON_SIZE_BUTTON));
	w->show();
	stop_button.add (*w);
	w = manage (new Image (Stock::MEDIA_PLAY, ICON_SIZE_BUTTON));
	w->show();
	play_selection_button.add (*w);
	w = manage (new Image (Stock::MEDIA_RECORD, ICON_SIZE_BUTTON));
	w->show();
	rec_button.add (*w);
	w = manage (new Image (get_xpm("loop.xpm")));
	w->show();
	auto_loop_button.add (*w);

	RefPtr<Action> act;

	act = ActionManager::get_action (X_("Transport"), X_("Stop"));
	act->connect_proxy (stop_button);
	act = ActionManager::get_action (X_("Transport"), X_("Roll"));
	act->connect_proxy (roll_button);
	act = ActionManager::get_action (X_("Transport"), X_("Record"));
	act->connect_proxy (rec_button);
	act = ActionManager::get_action (X_("Transport"), X_("GotoStart"));
	act->connect_proxy (goto_start_button);
	act = ActionManager::get_action (X_("Transport"), X_("GotoEnd"));
	act->connect_proxy (goto_end_button);
	act = ActionManager::get_action (X_("Transport"), X_("Loop"));
	act->connect_proxy (auto_loop_button);
	act = ActionManager::get_action (X_("Transport"), X_("PlaySelection"));
	act->connect_proxy (play_selection_button);
	act = ActionManager::get_action (X_("Transport"), X_("ToggleTimeMaster"));
	act->connect_proxy (time_master_button);

	ARDOUR_UI::instance()->tooltips().set_tip (roll_button, _("Play from playhead"));
	ARDOUR_UI::instance()->tooltips().set_tip (stop_button, _("Stop playback"));
	ARDOUR_UI::instance()->tooltips().set_tip (play_selection_button, _("Play range/selection"));
	ARDOUR_UI::instance()->tooltips().set_tip (goto_start_button, _("Go to start of session"));
	ARDOUR_UI::instance()->tooltips().set_tip (goto_end_button, _("Go to end of session"));
	ARDOUR_UI::instance()->tooltips().set_tip (auto_loop_button, _("Play loop range"));
	ARDOUR_UI::instance()->tooltips().set_tip (auto_return_button, _("Return to last playback start when stopped"));
	ARDOUR_UI::instance()->tooltips().set_tip (auto_play_button, _("Start playback after any locate"));
	ARDOUR_UI::instance()->tooltips().set_tip (auto_input_button, _("Be sensible about input monitoring"));
	ARDOUR_UI::instance()->tooltips().set_tip (punch_in_button, _("Start recording at auto-punch start"));
	ARDOUR_UI::instance()->tooltips().set_tip (punch_out_button, _("Stop recording at auto-punch end"));
	ARDOUR_UI::instance()->tooltips().set_tip (click_button, _("Enable/Disable audio click"));
	ARDOUR_UI::instance()->tooltips().set_tip (sync_option_combo, _("Positional sync source"));
	ARDOUR_UI::instance()->tooltips().set_tip (time_master_button, _("Does Ardour control the time?"));
	ARDOUR_UI::instance()->tooltips().set_tip (shuttle_box, _("Shuttle speed control"));
	ARDOUR_UI::instance()->tooltips().set_tip (shuttle_units_button, _("Select semitones or %%-age for speed display"));
	ARDOUR_UI::instance()->tooltips().set_tip (speed_display_box, _("Current transport speed"));
	
	shuttle_box.set_flags (CAN_FOCUS);
	shuttle_box.add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK);
	shuttle_box.set_size_request (100, 15);

	shuttle_box.signal_button_press_event().connect (mem_fun(*this, &ARDOUR_UI::shuttle_box_button_press));
	shuttle_box.signal_button_release_event().connect (mem_fun(*this, &ARDOUR_UI::shuttle_box_button_release));
	shuttle_box.signal_scroll_event().connect (mem_fun(*this, &ARDOUR_UI::shuttle_box_scroll));
	shuttle_box.signal_motion_notify_event().connect (mem_fun(*this, &ARDOUR_UI::shuttle_box_motion));
	shuttle_box.signal_expose_event().connect (mem_fun(*this, &ARDOUR_UI::shuttle_box_expose));

	/* clocks, etc. */

	ARDOUR_UI::Clock.connect (bind (mem_fun (primary_clock, &AudioClock::set), false));
	ARDOUR_UI::Clock.connect (bind (mem_fun (secondary_clock, &AudioClock::set), false));

	primary_clock.set_mode (AudioClock::SMPTE);
	secondary_clock.set_mode (AudioClock::BBT);

	primary_clock.ValueChanged.connect (mem_fun(*this, &ARDOUR_UI::primary_clock_value_changed));
	secondary_clock.ValueChanged.connect (mem_fun(*this, &ARDOUR_UI::secondary_clock_value_changed));

	ARDOUR_UI::instance()->tooltips().set_tip (primary_clock, _("Primary clock"));
	ARDOUR_UI::instance()->tooltips().set_tip (secondary_clock, _("secondary clock"));

	ActionManager::get_action ("Transport", "ToggleAutoReturn")->connect_proxy (auto_return_button);
	ActionManager::get_action ("Transport", "ToggleAutoPlay")->connect_proxy (auto_play_button);
	ActionManager::get_action ("Transport", "ToggleAutoInput")->connect_proxy (auto_input_button);
	ActionManager::get_action ("Transport", "ToggleClick")->connect_proxy (click_button);
	ActionManager::get_action ("Transport", "TogglePunchIn")->connect_proxy (punch_in_button);
	ActionManager::get_action ("Transport", "TogglePunchOut")->connect_proxy (punch_out_button);

	preroll_button.unset_flags (CAN_FOCUS);
	preroll_button.set_events (preroll_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	preroll_button.set_name ("TransportButton");

	postroll_button.unset_flags (CAN_FOCUS);
	postroll_button.set_events (postroll_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	postroll_button.set_name ("TransportButton");

	preroll_clock.set_mode (AudioClock::MinSec);
	preroll_clock.set_name ("TransportClockDisplay");
	postroll_clock.set_mode (AudioClock::MinSec);
	postroll_clock.set_name ("TransportClockDisplay");

	/* alerts */

	/* CANNOT bind these to clicked or toggled, must use pressed or released */

	solo_alert_button.set_name ("TransportSoloAlert");
	solo_alert_button.signal_pressed().connect (mem_fun(*this,&ARDOUR_UI::solo_alert_toggle));
	auditioning_alert_button.set_name ("TransportAuditioningAlert");
	auditioning_alert_button.signal_pressed().connect (mem_fun(*this,&ARDOUR_UI::audition_alert_toggle));

	alert_box.pack_start (solo_alert_button, false, false);
	alert_box.pack_start (auditioning_alert_button, false, false);

	transport_tearoff_hbox.set_border_width (3);

	transport_tearoff_hbox.pack_start (goto_start_button, false, false);
	transport_tearoff_hbox.pack_start (goto_end_button, false, false);

	Frame* sframe = manage (new Frame);
	VBox*  svbox = manage (new VBox);
	HBox*  shbox = manage (new HBox);

	sframe->set_shadow_type (SHADOW_IN);
	sframe->add (shuttle_box);

	shuttle_box.set_name (X_("ShuttleControl"));

	speed_display_box.add (speed_display_label);
	speed_display_box.set_name (X_("ShuttleDisplay"));

	shuttle_units_button.set_name (X_("ShuttleButton"));
	shuttle_units_button.signal_clicked().connect (mem_fun(*this, &ARDOUR_UI::shuttle_unit_clicked));
	
	shuttle_style_button.set_name (X_("ShuttleButton"));

	vector<string> shuttle_strings;
	shuttle_strings.push_back (_("sprung"));
	shuttle_strings.push_back (_("wheel"));
	set_popdown_strings (shuttle_style_button, shuttle_strings);
	shuttle_style_button.signal_changed().connect (mem_fun (*this, &ARDOUR_UI::shuttle_style_changed));

	Frame* sdframe = manage (new Frame);

	sdframe->set_shadow_type (SHADOW_IN);
	sdframe->add (speed_display_box);

	mtc_port_changed ();
	sync_option_combo.set_active_text (positional_sync_strings.front());
	sync_option_combo.signal_changed().connect (mem_fun (*this, &ARDOUR_UI::sync_option_changed));
	Gtkmm2ext::set_size_request_to_display_given_text (sync_option_combo, "Internal", 22, 10);

	shbox->pack_start (*sdframe, false, false);
	shbox->pack_start (shuttle_units_button, true, true);
	shbox->pack_start (shuttle_style_button, false, false);
	
	svbox->pack_start (*sframe, false, false);
	svbox->pack_start (*shbox, false, false);

	transport_tearoff_hbox.pack_start (*svbox, false, false, 3);

	transport_tearoff_hbox.pack_start (auto_loop_button, false, false);
	transport_tearoff_hbox.pack_start (play_selection_button, false, false);
	transport_tearoff_hbox.pack_start (roll_button, false, false);
	transport_tearoff_hbox.pack_start (stop_button, false, false);
	transport_tearoff_hbox.pack_start (rec_button, false, false, 6);

	HBox* clock_box = manage (new HBox);
	clock_box->pack_start (primary_clock, false, false);
	clock_box->pack_start (secondary_clock, false, false);
	VBox* time_controls_box = manage (new VBox);
	time_controls_box->pack_start (sync_option_combo, false, false);
	time_controls_box->pack_start (time_master_button, false, false);
	clock_box->pack_start (*time_controls_box, false, false, 1);
	transport_tearoff_hbox.pack_start (*clock_box, false, false, 0);
	
	HBox* toggle_box = manage(new HBox);
	
	VBox* punch_box = manage (new VBox);
	punch_box->pack_start (punch_in_button, false, false);
	punch_box->pack_start (punch_out_button, false, false);
	toggle_box->pack_start (*punch_box, false, false);

	VBox* auto_box = manage (new VBox);
	auto_box->pack_start (auto_play_button, false, false);
	auto_box->pack_start (auto_return_button, false, false);
	toggle_box->pack_start (*auto_box, false, false);
	
	VBox* io_box = manage (new VBox);
	io_box->pack_start (auto_input_button, false, false);
	io_box->pack_start (click_button, false, false);
	toggle_box->pack_start (*io_box, false, false);
	
	/* desensitize */

	set_transport_sensitivity (false);

//	toggle_box->pack_start (preroll_button, false, false);
//	toggle_box->pack_start (preroll_clock, false, false);

//	toggle_box->pack_start (postroll_button, false, false);
//	toggle_box->pack_start (postroll_clock, false, false);

	transport_tearoff_hbox.pack_start (*toggle_box, false, false, 4);
	transport_tearoff_hbox.pack_start (alert_box, false, false);
}

void
ARDOUR_UI::setup_clock ()
{
	ARDOUR_UI::Clock.connect (bind (mem_fun (big_clock, &AudioClock::set), false));
	
	big_clock_window = new Window (WINDOW_TOPLEVEL);
	
	big_clock_window->set_border_width (0);
	big_clock_window->add  (big_clock);
	big_clock_window->set_title (_("ardour: clock"));
	big_clock_window->set_type_hint (Gdk::WINDOW_TYPE_HINT_MENU);
	big_clock_window->signal_realize().connect (bind (sigc::ptr_fun (set_decoration), big_clock_window,  (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH)));
	big_clock_window->signal_unmap().connect (bind (sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleBigClock")));

	manage_window (*big_clock_window);
}

void
ARDOUR_UI::manage_window (Window& win)
{
	win.signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), &win));
	win.signal_enter_notify_event().connect (bind (mem_fun (Keyboard::the_keyboard(), &Keyboard::enter_window), &win));
	win.signal_leave_notify_event().connect (bind (mem_fun (Keyboard::the_keyboard(), &Keyboard::leave_window), &win));
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
	Gtkmm2ext::UI::instance()->call_slot(bind (mem_fun(*this, &ARDOUR_UI::_auditioning_changed), onoff));
}

void
ARDOUR_UI::audition_alert_toggle ()
{
	if (session) {
		session->cancel_audition();
	}
}

void
ARDOUR_UI::solo_alert_toggle ()
{
	if (session) {
		session->set_all_solo (!session->soloing());
	}
}

void
ARDOUR_UI::solo_blink (bool onoff)
{
	if (session == 0) {
		return;
	}
	
	if (session->soloing()) {
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
ARDOUR_UI::audition_blink (bool onoff)
{
	if (session == 0) {
		return;
	}
	
	if (session->is_auditioning()) {
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
ARDOUR_UI::build_shuttle_context_menu ()
{
	using namespace Menu_Helpers;

	shuttle_context_menu = new Menu();
	MenuList& items = shuttle_context_menu->items();

	Menu* speed_menu = manage (new Menu());
	MenuList& speed_items = speed_menu->items();

	RadioMenuItem::Group group;

	speed_items.push_back (RadioMenuElem (group, "8", bind (mem_fun (*this, &ARDOUR_UI::set_shuttle_max_speed), 8.0f)));
	if (shuttle_max_speed == 8.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();	
	}
	speed_items.push_back (RadioMenuElem (group, "6", bind (mem_fun (*this, &ARDOUR_UI::set_shuttle_max_speed), 6.0f)));
	if (shuttle_max_speed == 6.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();	
	}
	speed_items.push_back (RadioMenuElem (group, "4", bind (mem_fun (*this, &ARDOUR_UI::set_shuttle_max_speed), 4.0f)));
	if (shuttle_max_speed == 4.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();	
	}
	speed_items.push_back (RadioMenuElem (group, "3", bind (mem_fun (*this, &ARDOUR_UI::set_shuttle_max_speed), 3.0f)));
	if (shuttle_max_speed == 3.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();	
	}
	speed_items.push_back (RadioMenuElem (group, "2", bind (mem_fun (*this, &ARDOUR_UI::set_shuttle_max_speed), 2.0f)));
	if (shuttle_max_speed == 2.0) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();	
	}
	speed_items.push_back (RadioMenuElem (group, "1.5", bind (mem_fun (*this, &ARDOUR_UI::set_shuttle_max_speed), 1.5f)));
	if (shuttle_max_speed == 1.5) {
		static_cast<RadioMenuItem*>(&speed_items.back())->set_active ();	
	}

	items.push_back (MenuElem (_("Maximum speed"), *speed_menu));
}

void
ARDOUR_UI::show_shuttle_context_menu ()
{
	if (shuttle_context_menu == 0) {
		build_shuttle_context_menu ();
	}

	shuttle_context_menu->popup (1, 0);
}

void
ARDOUR_UI::set_shuttle_max_speed (float speed)
{
	shuttle_max_speed = speed;
}

gint
ARDOUR_UI::shuttle_box_button_press (GdkEventButton* ev)
{
	if (!session) {
		return true;
	}

	if (Keyboard::is_context_menu_event (ev)) {
		show_shuttle_context_menu ();
		return true;
	}

	switch (ev->button) {
	case 1:
		shuttle_box.add_modal_grab ();
		shuttle_grabbed = true;
		mouse_shuttle (ev->x, true);
		break;

	case 2:
	case 3:
		return true;
		break;
	}

	return true;
}

gint
ARDOUR_UI::shuttle_box_button_release (GdkEventButton* ev)
{
	if (!session) {
		return true;
	}
	
	switch (ev->button) {
	case 1:
		mouse_shuttle (ev->x, true);
		shuttle_grabbed = false;
		shuttle_box.remove_modal_grab ();
		if (shuttle_behaviour == Sprung) {
			if (session->get_auto_play() || roll_button.get_state()) {
				shuttle_fract = SHUTTLE_FRACT_SPEED1;				
				session->request_transport_speed (1.0);
				stop_button.set_active (false);
				roll_button.set_active (true);
			} else {
				shuttle_fract = 0;
				session->request_transport_speed (0.0);
			}
			shuttle_box.queue_draw ();
		}
		return true;

	case 2:
		if (session->transport_rolling()) {
			shuttle_fract = SHUTTLE_FRACT_SPEED1;
			session->request_transport_speed (1.0);
			stop_button.set_active (false);
			roll_button.set_active (true);
		} else {
			shuttle_fract = 0;
		}
		shuttle_box.queue_draw ();
		return true;

	case 3:
		return true;
		
	case 4:
		shuttle_fract += 0.005;
		break;
	case 5:
		shuttle_fract -= 0.005;
		break;
	}

	use_shuttle_fract (true);

	return true;
}

gint
ARDOUR_UI::shuttle_box_scroll (GdkEventScroll* ev)
{
	if (!session) {
		return true;
	}
	
	switch (ev->direction) {
		
	case GDK_SCROLL_UP:
		shuttle_fract += 0.005;
		break;
	case GDK_SCROLL_DOWN:
		shuttle_fract -= 0.005;
		break;
	default:
		/* scroll left/right */
		return false;
	}

	use_shuttle_fract (true);

	return true;
}

gint
ARDOUR_UI::shuttle_box_motion (GdkEventMotion* ev)
{
	if (!session || !shuttle_grabbed) {
		return true;
	}

	return mouse_shuttle (ev->x, false);
}

gint
ARDOUR_UI::mouse_shuttle (double x, bool force)
{
	double half_width = shuttle_box.get_width() / 2.0;
	double distance = x - half_width;

	if (distance > 0) {
		distance = min (distance, half_width);
	} else {
		distance = max (distance, -half_width);
	}

	shuttle_fract = distance / half_width;
	use_shuttle_fract (force);
	return true;
}

void
ARDOUR_UI::use_shuttle_fract (bool force)
{
	struct timeval now;
	struct timeval diff;
	
	/* do not attempt to submit a motion-driven transport speed request
	   more than once per process cycle.
	 */

	gettimeofday (&now, 0);
	timersub (&now, &last_shuttle_request, &diff);

	if (!force && (diff.tv_usec + (diff.tv_sec * 1000000)) < engine->usecs_per_cycle()) {
		return;
	}
	
	last_shuttle_request = now;

	bool neg = (shuttle_fract < 0.0);

	double fract = 1 - sqrt (1 - (shuttle_fract * shuttle_fract)); // Formula A1

	if (neg) {
		fract = -fract;
	}

	session->request_transport_speed (shuttle_max_speed * fract); // Formula A2
	shuttle_box.queue_draw ();
}

gint
ARDOUR_UI::shuttle_box_expose (GdkEventExpose* event)
{
	gint x;
	Glib::RefPtr<Gdk::Window> win (shuttle_box.get_window());

	/* redraw the background */

	win->draw_rectangle (shuttle_box.get_style()->get_bg_gc (shuttle_box.get_state()),
			     true,
			     event->area.x, event->area.y,
			     event->area.width, event->area.height);


	x = (gint) floor ((shuttle_box.get_width() / 2.0) + (0.5 * (shuttle_box.get_width() * shuttle_fract)));

	/* draw line */

	win->draw_line (shuttle_box.get_style()->get_fg_gc (shuttle_box.get_state()),
			x,
			0,
			x,
			shuttle_box.get_height());
	return true;
}

void
ARDOUR_UI::shuttle_unit_clicked ()
{
	if (shuttle_unit_menu == 0) {
		shuttle_unit_menu = dynamic_cast<Menu*> (ActionManager::get_widget ("/ShuttleUnitPopup"));
	}
	shuttle_unit_menu->popup (1, 0);
}

void
ARDOUR_UI::set_shuttle_units (ShuttleUnits u)
{
	switch ((shuttle_units = u)) {
	case Percentage:
		shuttle_units_button.set_label("% ");
		break;
	case Semitones:
		shuttle_units_button.set_label(_("ST"));
		break;
	}
}

void
ARDOUR_UI::shuttle_style_changed ()
{
	ustring str = shuttle_style_button.get_active_text ();

	if (str == _("sprung")) {
		set_shuttle_behaviour (Sprung);
	} else if (str == _("wheel")) {
		set_shuttle_behaviour (Wheel);
	}
}


void
ARDOUR_UI::set_shuttle_behaviour (ShuttleBehaviour b)
{
	switch ((shuttle_behaviour = b)) {
	case Sprung:
		shuttle_style_button.set_active_text (_("sprung"));
		shuttle_fract = 0.0;
		shuttle_box.queue_draw ();
		if (session) {
			if (session->transport_rolling()) {
			        shuttle_fract = SHUTTLE_FRACT_SPEED1;
				session->request_transport_speed (1.0);
			}
		}
		break;
	case Wheel:
		shuttle_style_button.set_active_text (_("wheel"));
		break;
	}
}

void
ARDOUR_UI::update_speed_display ()
{
	if (!session) {
		if (last_speed_displayed != 0) {
			speed_display_label.set_text (_("stop"));
			last_speed_displayed = 0;
		}
		return;
	}

	char buf[32];
	float x = session->transport_speed ();

	if (x != last_speed_displayed) {

		if (x != 0) {
			if (shuttle_units == Percentage) {
				snprintf (buf, sizeof (buf), "%.2f", x);
			} else {
				if (x < 0) {
					snprintf (buf, sizeof (buf), "< %.1f", 12.0 * fast_log2 (-x));
				} else {
					snprintf (buf, sizeof (buf), "> %.1f", 12.0 * fast_log2 (x));
				}
			}
			speed_display_label.set_text (buf);
		} else {
			speed_display_label.set_text (_("stop"));
		}

		last_speed_displayed = x;
	}
}	
	
void
ARDOUR_UI::set_transport_sensitivity (bool yn)
{
	ActionManager::set_sensitive (ActionManager::transport_sensitive_actions, yn);
	shuttle_box.set_sensitive (yn);
}

void
ARDOUR_UI::editor_realized ()
{
	set_size_request_to_display_given_text (speed_display_box, _("-0.55"), 2, 2);
	/* XXX: this should really be saved in instant.xml or something similar and restored from there */
	shuttle_style_button.set_active_text (_("sprung"));
	const guint32 FUDGE = 20; // Combo's are stupid - they steal space from the entry for the button
	set_size_request_to_display_given_text (shuttle_style_button, _("sprung"), 2+FUDGE, 10);
}

void
ARDOUR_UI::sync_option_changed ()
{
	string which;

	if (session == 0) {
		return;
	}

	which = sync_option_combo.get_active_text();

	if (which == positional_sync_strings[Session::None]) {
		session->request_slave_source (Session::None);
	} else if (which == positional_sync_strings[Session::MTC]) {
		session->request_slave_source (Session::MTC);
	} else if (which == positional_sync_strings[Session::JACK]) {
		session->request_slave_source (Session::JACK);
	} 
}

void
ARDOUR_UI::maximise_editing_space ()
{
	if (!editor) {
		return;
	}

	transport_tearoff->set_visible (false);
	editor->maximise_editing_space ();
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
