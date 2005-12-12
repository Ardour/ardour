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
#include <gtkmm2ext/pix.h>
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
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Glib;
using namespace sigc;

int	
ARDOUR_UI::setup_windows ()
{
	using namespace Menu_Helpers;

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

#include "transport_xpms"

void
ARDOUR_UI::transport_stopped ()
{
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
	roll_button.set_active (true);
	play_selection_button.set_active (false);
	auto_loop_button.set_active (false);
}

void
ARDOUR_UI::transport_forwarding ()
{
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

	transport_frame.set_shadow_type (Gtk::SHADOW_OUT);
	transport_frame.set_name ("BaseFrame");
	transport_frame.add (transport_base);

	transport_tearoff->Detach.connect (bind (mem_fun(*this, &ARDOUR_UI::detach_tearoff), static_cast<Gtk::Box*>(&top_packer), 
						 static_cast<Gtk::Widget*>(&transport_frame)));
	transport_tearoff->Attach.connect (bind (mem_fun(*this, &ARDOUR_UI::reattach_tearoff), static_cast<Gtk::Box*> (&top_packer), 
						 static_cast<Gtk::Widget*> (&transport_frame), 1));


	Widget* w;

#ifdef THE_OLD
	w = manage (new Image (Gdk::Pixbuf::create_from_xpm_data(start_xpm)));
	w->show();
	goto_start_button.add (*w);
	w = manage (new Image (Gdk::Pixbuf::create_from_xpm_data(end_xpm)));
	w->show();
	goto_end_button.add (*w);
	w = manage (new Image (Gdk::Pixbuf::create_from_xpm_data(arrow_xpm)));
	w->show();
	roll_button.add (*w);
	w = manage (new Image (Gdk::Pixbuf::create_from_xpm_data(stop_xpm)));
	w->show();
	stop_button.add (*w);
	w = manage (new Image (Gdk::Pixbuf::create_from_xpm_data(play_selection_xpm)));
	w->show();
	play_selection_button.add (*w);
	w = manage (new Image (Gdk::Pixbuf::create_from_xpm_data(rec_xpm)));
	w->show();
	rec_button.add (*w);
	w = manage (new Image (Gdk::Pixbuf::create_from_xpm_data(loop_xpm)));
	w->show();
	auto_loop_button.add (*w);


	stop_button.set_use_stock (false);
	roll_button.set_use_stock (false);
	rec_button.set_use_stock (false);
	goto_start_button.set_use_stock (false);
	goto_end_button.set_use_stock (false);
	auto_loop_button.set_use_stock (false);
#else
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
	w = manage (new Image (Gdk::Pixbuf::create_from_xpm_data(loop_xpm)));
	w->show();
	auto_loop_button.add (*w);

	stop_button.set_use_stock (true);
	roll_button.set_use_stock (true);
	rec_button.set_use_stock (true);
	goto_start_button.set_use_stock (true);
	goto_end_button.set_use_stock (true);
	auto_loop_button.set_use_stock (true);
#endif

	RefPtr<Action> act;

	act = ActionManager::get_action (X_("<Actions>/Common/TransportStop"));
	act->connect_proxy (stop_button);
	act = ActionManager::get_action (X_("<Actions>/Common/TransportRoll"));
	act->connect_proxy (roll_button);
	act = ActionManager::get_action (X_("<Actions>/Common/TransportRecord"));
	act->connect_proxy (rec_button);
	act = ActionManager::get_action (X_("<Actions>/Common/TransportGotoStart"));
	act->connect_proxy (goto_start_button);
	act = ActionManager::get_action (X_("<Actions>/Common/TransportGotoEnd"));
	act->connect_proxy (goto_end_button);
	act = ActionManager::get_action (X_("<Actions>/Common/TransportLoop"));
	act->connect_proxy (auto_loop_button);

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
	ARDOUR_UI::instance()->tooltips().set_tip (follow_button, _("Enable/Disable follow playhead"));
	ARDOUR_UI::instance()->tooltips().set_tip (shuttle_box, _("Shuttle speed control"));
	ARDOUR_UI::instance()->tooltips().set_tip (shuttle_units_button, _("Select semitones or %%-age for speed display"));
	ARDOUR_UI::instance()->tooltips().set_tip (shuttle_style_button, _("Select sprung or wheel behaviour"));
	ARDOUR_UI::instance()->tooltips().set_tip (speed_display_box, _("Current transport speed"));
	
	shuttle_box.set_flags (CAN_FOCUS);
	shuttle_box.set_events (shuttle_box.get_events() | Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::POINTER_MOTION_MASK);
	shuttle_box.set_size_request (100, 15);

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
	follow_button.set_name ("TransportButton");
	
	goto_start_button.unset_flags (Gtk::CAN_FOCUS);
	goto_end_button.unset_flags (Gtk::CAN_FOCUS);
	roll_button.unset_flags (Gtk::CAN_FOCUS);
	stop_button.unset_flags (Gtk::CAN_FOCUS);
	play_selection_button.unset_flags (Gtk::CAN_FOCUS);
	rec_button.unset_flags (Gtk::CAN_FOCUS);
	auto_loop_button.unset_flags (Gtk::CAN_FOCUS);
	auto_return_button.unset_flags (Gtk::CAN_FOCUS);
	auto_play_button.unset_flags (Gtk::CAN_FOCUS);
	auto_input_button.unset_flags (Gtk::CAN_FOCUS);
	punch_out_button.unset_flags (Gtk::CAN_FOCUS);
	punch_in_button.unset_flags (Gtk::CAN_FOCUS);
	click_button.unset_flags (Gtk::CAN_FOCUS);
	follow_button.unset_flags (Gtk::CAN_FOCUS);
	
	goto_start_button.set_events (goto_start_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	goto_end_button.set_events (goto_end_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	roll_button.set_events (roll_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	stop_button.set_events (stop_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	play_selection_button.set_events (play_selection_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	rec_button.set_events (rec_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	auto_loop_button.set_events (auto_loop_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	auto_return_button.set_events (auto_return_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	auto_play_button.set_events (auto_play_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	auto_input_button.set_events (auto_input_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	click_button.set_events (click_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	follow_button.set_events (click_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	punch_in_button.set_events (punch_in_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	punch_out_button.set_events (punch_out_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));

	shuttle_box.signal_button_press_event().connect (mem_fun(*this, &ARDOUR_UI::shuttle_box_button_press));
	shuttle_box.signal_button_release_event().connect (mem_fun(*this, &ARDOUR_UI::shuttle_box_button_release));
	shuttle_box.signal_motion_notify_event().connect (mem_fun(*this, &ARDOUR_UI::shuttle_box_motion));
	shuttle_box.signal_expose_event().connect (mem_fun(*this, &ARDOUR_UI::shuttle_box_expose));

	/* clocks, etc. */

	ARDOUR_UI::Clock.connect (bind (mem_fun (primary_clock, &AudioClock::set), false));
	ARDOUR_UI::Clock.connect (bind (mem_fun (secondary_clock, &AudioClock::set), false));

	primary_clock.set_mode (AudioClock::SMPTE);
	primary_clock.set_name ("TransportClockDisplay");
	secondary_clock.set_mode (AudioClock::BBT);
	secondary_clock.set_name ("TransportClockDisplay");


	primary_clock.ValueChanged.connect (mem_fun(*this, &ARDOUR_UI::primary_clock_value_changed));
	secondary_clock.ValueChanged.connect (mem_fun(*this, &ARDOUR_UI::secondary_clock_value_changed));

	ARDOUR_UI::instance()->tooltips().set_tip (primary_clock, _("Primary clock"));
	ARDOUR_UI::instance()->tooltips().set_tip (secondary_clock, _("secondary clock"));

	/* options */

	auto_return_button.signal_toggled().connect (mem_fun(*this,&ARDOUR_UI::toggle_auto_return));
	auto_play_button.signal_toggled().connect (mem_fun(*this,&ARDOUR_UI::toggle_auto_play));
	auto_input_button.signal_toggled().connect (mem_fun(*this,&ARDOUR_UI::toggle_auto_input));
	click_button.signal_toggled().connect (mem_fun(*this,&ARDOUR_UI::toggle_click));
	follow_button.signal_toggled().connect (mem_fun(*this,&ARDOUR_UI::toggle_follow));
	punch_in_button.signal_toggled().connect (mem_fun(*this,&ARDOUR_UI::toggle_punch_in));
	punch_out_button.signal_toggled().connect (mem_fun(*this,&ARDOUR_UI::toggle_punch_out));

	preroll_button.unset_flags (Gtk::CAN_FOCUS);
	preroll_button.set_events (preroll_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));
	preroll_button.set_name ("TransportButton");

	postroll_button.unset_flags (Gtk::CAN_FOCUS);
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

	alert_box.pack_start (solo_alert_button);
	alert_box.pack_start (auditioning_alert_button);

	transport_tearoff_hbox.set_border_width (5);

	transport_tearoff_hbox.pack_start (goto_start_button, false, false);
	transport_tearoff_hbox.pack_start (goto_end_button, false, false);

	Gtk::Frame* sframe = manage (new Frame);
	Gtk::VBox*  svbox = manage (new VBox);
	Gtk::HBox*  shbox = manage (new HBox);

	sframe->set_shadow_type (Gtk::SHADOW_IN);
	sframe->add (shuttle_box);

	shuttle_box.set_name (X_("ShuttleControl"));

	speed_display_box.add (speed_display_label);
	set_size_request_to_display_given_text (speed_display_box, _("stopped"), 2, 2);
	speed_display_box.set_name (X_("ShuttleDisplay"));

	shuttle_units_button.set_name (X_("ShuttleButton"));
	shuttle_units_button.signal_clicked().connect (mem_fun(*this, &ARDOUR_UI::shuttle_unit_clicked));
	
	shuttle_style_button.set_name (X_("ShuttleButton"));
	shuttle_style_button.signal_clicked().connect (mem_fun(*this, &ARDOUR_UI::shuttle_style_clicked));

	Gtk::Frame* sdframe = manage (new Frame);

	sdframe->set_shadow_type (Gtk::SHADOW_IN);
	sdframe->add (speed_display_box);

	shbox->pack_start (*sdframe, false, false);
	shbox->pack_start (shuttle_units_button, true, true);
	shbox->pack_start (shuttle_style_button, false, false);
	
	svbox->pack_start (*sframe, false, false);
	svbox->pack_start (*shbox, false, false);

	transport_tearoff_hbox.pack_start (*svbox, false, false, 5);

	transport_tearoff_hbox.pack_start (auto_loop_button, false, false);
	transport_tearoff_hbox.pack_start (play_selection_button, false, false);
	transport_tearoff_hbox.pack_start (roll_button, false, false);
	transport_tearoff_hbox.pack_start (stop_button, false, false);
	transport_tearoff_hbox.pack_start (rec_button, false, false, 10);

	transport_tearoff_hbox.pack_start (primary_clock, false, false, 5);
	transport_tearoff_hbox.pack_start (secondary_clock, false, false, 5);

	transport_tearoff_hbox.pack_start (punch_in_button, false, false);
	transport_tearoff_hbox.pack_start (punch_out_button, false, false);
	transport_tearoff_hbox.pack_start (auto_input_button, false, false);
	transport_tearoff_hbox.pack_start (auto_return_button, false, false);
	transport_tearoff_hbox.pack_start (auto_play_button, false, false);
	transport_tearoff_hbox.pack_start (click_button, false, false);
	transport_tearoff_hbox.pack_start (follow_button, false, false);
	
	/* desensitize */

	set_transport_sensitivity (false);

	/* catch up with editor state */

	follow_changed ();

//	transport_tearoff_hbox.pack_start (preroll_button, false, false);
//	transport_tearoff_hbox.pack_start (preroll_clock, false, false);

//	transport_tearoff_hbox.pack_start (postroll_button, false, false);
//	transport_tearoff_hbox.pack_start (postroll_clock, false, false);

	transport_tearoff_hbox.pack_start (alert_box, false, false, 5);
}

void
ARDOUR_UI::setup_clock ()
{
	ARDOUR_UI::Clock.connect (bind (mem_fun (big_clock, &AudioClock::set), false));
	
	big_clock_window = new Gtk::Window (WINDOW_TOPLEVEL);
	
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
ARDOUR_UI::detach_tearoff (Gtk::Box* b, Gtk::Widget* w)
{
	editor->ensure_float (*transport_tearoff->tearoff_window());
	b->remove (*w);
}

void
ARDOUR_UI::reattach_tearoff (Gtk::Box* b, Gtk::Widget* w, int32_t n)
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
			solo_alert_button.set_state (Gtk::STATE_ACTIVE);
		} else {
			solo_alert_button.set_state (Gtk::STATE_NORMAL);
		}
	} else {
		solo_alert_button.set_active (false);
		solo_alert_button.set_state (Gtk::STATE_NORMAL);
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
			auditioning_alert_button.set_state (Gtk::STATE_ACTIVE);
		} else {
			auditioning_alert_button.set_state (Gtk::STATE_NORMAL);
		}
	} else {
		auditioning_alert_button.set_active (false);
		auditioning_alert_button.set_state (Gtk::STATE_NORMAL);
	}
}


gint
ARDOUR_UI::shuttle_box_button_press (GdkEventButton* ev)
{
	if (!session) {
		return TRUE;
	}

	switch (ev->button) {
	case 1:
		shuttle_box.add_modal_grab ();
		shuttle_grabbed = true;
		mouse_shuttle (ev->x, true);
		break;

	case 2:
	case 3:
		return TRUE;
		break;

	case 4:
		break;
	case 5:
		break;
	}

	return TRUE;
}

gint
ARDOUR_UI::shuttle_box_button_release (GdkEventButton* ev)
{
	if (!session) {
		return TRUE;
	}

	switch (ev->button) {
	case 1:
		mouse_shuttle (ev->x, true);
		shuttle_grabbed = false;
		shuttle_box.remove_modal_grab ();
		if (shuttle_behaviour == Sprung) {
			shuttle_fract = SHUTTLE_FRACT_SPEED1;
			session->request_transport_speed (1.0);
			shuttle_box.queue_draw ();
		}
		return TRUE;

	case 2:
		if (session->transport_rolling()) {
			shuttle_fract = SHUTTLE_FRACT_SPEED1;
			session->request_transport_speed (1.0);
		} else {
			shuttle_fract = 0;
		}
		shuttle_box.queue_draw ();
		return TRUE;

	case 3:
		return TRUE;
		
	case 4:
		shuttle_fract += 0.005;
		break;
	case 5:
		shuttle_fract -= 0.005;
		break;
	}

	use_shuttle_fract (true);

	return TRUE;
}

gint
ARDOUR_UI::shuttle_box_motion (GdkEventMotion* ev)
{
	if (!session || !shuttle_grabbed) {
		return TRUE;
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
	return TRUE;
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

	session->request_transport_speed (8.0 * fract); // Formula A2
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
	return TRUE;
}

void
ARDOUR_UI::shuttle_style_clicked ()
{
	if (shuttle_style_menu == 0) {
		shuttle_style_menu = dynamic_cast<Menu*> (ActionManager::get_widget ("/ShuttleStylePopup"));
	}
	shuttle_style_menu->popup (1, 0);
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
		static_cast<Gtk::Label*>(shuttle_units_button.get_child())->set_text ("% ");
		break;
	case Semitones:
		static_cast<Gtk::Label*>(shuttle_units_button.get_child())->set_text (_("st"));
		break;
	}
}

void
ARDOUR_UI::set_shuttle_behaviour (ShuttleBehaviour b)
{
	switch ((shuttle_behaviour = b)) {
	case Sprung:
		static_cast<Gtk::Label*>(shuttle_style_button.get_child())->set_text (_("sprung"));
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
		static_cast<Gtk::Label*>(shuttle_style_button.get_child())->set_text (_("wheel"));
		break;
	}
}

void
ARDOUR_UI::update_speed_display ()
{
	if (!session) {
		speed_display_label.set_text (_("stopped"));
		return;
	}

	char buf[32];
	float x = session->transport_speed ();

	if (x != 0) {
		if (shuttle_units == Percentage) {
			snprintf (buf, sizeof (buf), "%.4f", x);
		} else {
			if (x < 0) {
				snprintf (buf, sizeof (buf), "< %.1f", 12.0 * fast_log2 (-x));
			} else {
				snprintf (buf, sizeof (buf), "> %.1f", 12.0 * fast_log2 (x));
			}
		}
		speed_display_label.set_text (buf);
	} else {
		speed_display_label.set_text (_("stopped"));
	}
}	
	
void
ARDOUR_UI::set_transport_sensitivity (bool yn)
{
	goto_start_button.set_sensitive (yn);
	goto_end_button.set_sensitive (yn);
	roll_button.set_sensitive (yn);
	stop_button.set_sensitive (yn);
	play_selection_button.set_sensitive (yn);
	rec_button.set_sensitive (yn);
	auto_loop_button.set_sensitive (yn);
	shuttle_box.set_sensitive (yn);
}
