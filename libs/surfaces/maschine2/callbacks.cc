/*
 * Copyright (C) 2018 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/session.h"
#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/gui_thread.h"
#include "pbd/i18n.h"

#include "maschine2.h"
#include "m2controls.h"

#include "midi++/port.h"

#define COLOR_WHITE 0xffffffff
#define COLOR_GRAY 0x606060ff
#define COLOR_BLACK 0x000000ff

using namespace ARDOUR;
using namespace ArdourSurface;

void
Maschine2::connect_signals ()
{
	// TODO: use some convenience macros here

	/* Signals */
	session->TransportStateChange.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&Maschine2::notify_transport_state_changed, this), this);
	session->TransportLooped.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&Maschine2::notify_loop_state_changed, this), this);
	session->RecordStateChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&Maschine2::notify_record_state_changed, this), this);
	Config->ParameterChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&Maschine2::notify_parameter_changed, this, _1), this);
	session->config.ParameterChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&Maschine2::notify_parameter_changed, this, _1), this);
	session->DirtyChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&Maschine2::notify_session_dirty_changed, this), this);
	session->history().Changed.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&Maschine2::notify_history_changed, this), this);

	/* Actions */
	Glib::RefPtr<Gtk::ToggleAction> tact;
	Glib::RefPtr<Gtk::RadioAction> ract;
#if 0
	tact = ActionManager::find_toggle_action (X_("Editor"), X_("ToggleMeasureVisibility"));
	tact->signal_toggled ().connect (sigc::mem_fun (*this, &Maschine2::notify_grid_change));
#endif
	ract = ActionManager::get_radio_action (X_("Editor"), X_("snap-off"));
	ract->signal_toggled ().connect (sigc::mem_fun (*this, &Maschine2::notify_snap_change));

	ract = ActionManager::get_radio_action (X_("Editor"), X_("snap-magnetic"));
	ract->signal_toggled ().connect (sigc::mem_fun (*this, &Maschine2::notify_snap_change));

	ract = ActionManager::get_radio_action (X_("Editor"), X_("snap-normal"));
	ract->signal_toggled ().connect (sigc::mem_fun (*this, &Maschine2::notify_snap_change));

	/* Surface events */
	_ctrl->button (M2Contols::Play)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_play, this));
	_ctrl->button (M2Contols::Rec)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_record, this));
	_ctrl->button (M2Contols::Loop)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_loop, this));
	_ctrl->button (M2Contols::Metronom)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_metronom, this));
	_ctrl->button (M2Contols::GotoStart)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_rewind, this));
	_ctrl->button (M2Contols::FastRewind)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_action, this, "Transport", "RewindSlow"));
	_ctrl->button (M2Contols::FastForward)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_action, this, "Transport", "ForwardSlow"));
	_ctrl->button (M2Contols::Panic)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_action, this, "MIDI", "panic"));
	_ctrl->button (M2Contols::JumpForward)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_action, this, "Editor", "jump-forward-to-mark"));
	_ctrl->button (M2Contols::JumpBackward)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_action, this, "Editor", "jump-backward-to-mark"));

	_ctrl->button (M2Contols::Grid)->pressed.connect (button_connections, invalidator (*this), boost::bind (&Maschine2::button_snap_pressed, this), gui_context());
	_ctrl->button (M2Contols::Grid)->released.connect (button_connections, invalidator (*this), boost::bind (&Maschine2::button_snap_released, this), gui_context());
	_ctrl->button (M2Contols::Grid)->changed.connect (button_connections, invalidator (*this), boost::bind (&Maschine2::button_snap_changed, this, _1), gui_context());

	_ctrl->button (M2Contols::Save)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_action, this, "Common", "Save"));
	_ctrl->button (M2Contols::Undo)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_action, this, "Editor", "undo"));
	_ctrl->button (M2Contols::Redo)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_action, this, "Editor", "redo"));

	_ctrl->button (M2Contols::MasterVolume)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::handle_master_change, this, MST_VOLUME));
	_ctrl->button (M2Contols::MasterTempo)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::handle_master_change, this, MST_TEMPO));

	_ctrl->button (M2Contols::EncoderWheel)->released.connect_same_thread (button_connections, boost::bind (&Maschine2::button_encoder, this));
	_ctrl->encoder (0)->changed.connect_same_thread (button_connections, boost::bind (&Maschine2::encoder_master, this, _1));

	for (unsigned int pad = 0; pad < 16; ++pad) {
		_ctrl->pad (pad)->event.connect_same_thread (button_connections, boost::bind (&Maschine2::pad_event, this, pad, _1, _2));
		_ctrl->pad (pad)->changed.connect_same_thread (button_connections, boost::bind (&Maschine2::pad_change, this, pad, _1));
	}

	/* set initial values */
	notify_record_state_changed ();
	notify_transport_state_changed ();
	notify_loop_state_changed ();
	notify_parameter_changed ("clicking");
	notify_snap_change ();
	notify_session_dirty_changed ();
	notify_history_changed ();
}

void
Maschine2::notify_record_state_changed ()
{
	switch (session->record_status ()) {
		case Session::Disabled:
			_ctrl->button (M2Contols::Rec)->set_color (0);
			_ctrl->button (M2Contols::Rec)->set_blinking (false);
			break;
		case Session::Enabled:
			_ctrl->button (M2Contols::Rec)->set_color (COLOR_WHITE);
			_ctrl->button (M2Contols::Rec)->set_blinking (true);
			break;
		case Session::Recording:
			_ctrl->button (M2Contols::Rec)->set_color (COLOR_WHITE);
			_ctrl->button (M2Contols::Rec)->set_blinking (false);
			break;
	}
}

void
Maschine2::notify_transport_state_changed ()
{
	if (transport_rolling ()) {
		_ctrl->button (M2Contols::Play)->set_color (COLOR_WHITE);
	} else {
		_ctrl->button (M2Contols::Play)->set_color (0);
	}
	notify_loop_state_changed ();
}

void
Maschine2::notify_loop_state_changed ()
{
	bool looping = false;
	Location* looploc = session->locations ()->auto_loop_location ();
	if (looploc && session->get_play_loop ()) {
		looping = true;
	}
	_ctrl->button (M2Contols::Loop)->set_color (looping ? COLOR_GRAY : 0);
}

void
Maschine2::notify_parameter_changed (std::string param)
{
	if (param == "clicking") {
		_ctrl->button (M2Contols::Metronom)->set_color (Config->get_clicking () ? COLOR_GRAY : 0);
	}
}

#if 0
void
Maschine2::notify_grid_change ()
{
	Glib::RefPtr<Gtk::ToggleAction> tact = ActionManager::find_toggle_action (X_("Editor"), X_("ToggleMeasureVisibility"));
	_ctrl->button (M2Contols::Grid)->set_color (tact->get_active () ? COLOR_WHITE : 0);
}
#endif

void
Maschine2::notify_snap_change ()
{
	uint32_t rgba = 0;
	if (_ctrl->button (M2Contols::Grid)->is_pressed ()) {
		return;
	}

	Glib::RefPtr<Gtk::RadioAction> ract = ActionManager::get_radio_action (X_("Editor"), X_("snap-magnetic"));
	if (ract->get_active ()) { rgba = COLOR_GRAY; }
	ract = ActionManager::get_radio_action (X_("Editor"), X_("snap-normal"));
	if (ract->get_active ()) { rgba = COLOR_WHITE; }

	_ctrl->button (M2Contols::Grid)->set_color (rgba);
}

void
Maschine2::notify_session_dirty_changed ()
{
	bool is_dirty = session->dirty ();
	_ctrl->button (M2Contols::Save)->set_color (is_dirty ? COLOR_WHITE : COLOR_BLACK);
	_ctrl->button (M2Contols::Save)->set_blinking (is_dirty);
}

void
Maschine2::notify_history_changed ()
{
	_ctrl->button (M2Contols::Redo)->set_color (session->redo_depth() > 0 ? COLOR_WHITE : COLOR_BLACK);
	_ctrl->button (M2Contols::Undo)->set_color (session->undo_depth() > 0 ? COLOR_WHITE : COLOR_BLACK);
}


void
Maschine2::button_play ()
{
	if (transport_rolling ()) {
		transport_stop ();
	} else {
		transport_play ();
	}
}

void
Maschine2::button_record ()
{
	set_record_enable (!get_record_enabled ());
}

void
Maschine2::button_loop ()
{
	loop_toggle ();
}

void
Maschine2::button_metronom ()
{
	Config->set_clicking (!Config->get_clicking ());
}

void
Maschine2::button_rewind ()
{
	goto_start (transport_rolling ());
}

void
Maschine2::button_action (const std::string& group, const std::string& item)
{
	AccessAction (group, item);
}

#if 0
void
Maschine2::button_grid ()
{
	Glib::RefPtr<Gtk::ToggleAction> tact = ActionManager::find_toggle_action (X_("Editor"), X_("ToggleMeasureVisibility"));
	tact->set_active (!tact->get_active ());
}
#endif

void
Maschine2::button_snap_pressed ()
{
	_ctrl->button (M2Contols::Grid)->set_color (COLOR_WHITE);
	_ctrl->button (M2Contols::Grid)->set_blinking (true);
}

void
Maschine2::button_snap_changed (bool pressed)
{
	if (!pressed) {
		_ctrl->button (M2Contols::Grid)->set_blinking (false);
		notify_snap_change ();
	}
	notify_master_change ();
}

void
Maschine2::button_snap_released ()
{
	_ctrl->button (M2Contols::Grid)->set_blinking (false);

	const char* action = 0;

	Glib::RefPtr<Gtk::RadioAction> ract = ActionManager::get_radio_action (X_("Editor"), X_("snap-off"));
	if (ract->get_active ()) { action = "snap-normal"; }

	ract = ActionManager::get_radio_action (X_("Editor"), X_("snap-normal"));
	if (ract->get_active ()) { action = "snap-magnetic"; }

	ract = ActionManager::get_radio_action (X_("Editor"), X_("snap-magnetic"));
	if (ract->get_active ()) { action = "snap-off"; }

	ract = ActionManager::get_radio_action (X_("Editor"), action);
	ract->set_active (true);
}

/* Master mode + state -- main encoder fn */

void
Maschine2::handle_master_change (enum MasterMode id)
{
	switch (id) {
		case MST_VOLUME:
			if (_master_state == MST_VOLUME) { _master_state = MST_NONE; } else { _master_state = MST_VOLUME; }
			break;
		case MST_TEMPO:
			if (_master_state == MST_TEMPO) { _master_state = MST_NONE; } else { _master_state = MST_TEMPO; }
			break;
		default:
			return;
			break;
	}
	notify_master_change ();
}

void
Maschine2::notify_master_change ()
{
	if (_ctrl->button (M2Contols::Grid)->is_pressed ()) {
		_ctrl->button (M2Contols::MasterVolume)->set_color (COLOR_BLACK);
		_ctrl->button (M2Contols::MasterTempo)->set_color (COLOR_BLACK);
		return;
	}
	switch (_master_state) {
		case MST_NONE:
			_ctrl->button (M2Contols::MasterVolume)->set_color (COLOR_BLACK);
			_ctrl->button (M2Contols::MasterTempo)->set_color (COLOR_BLACK);
			break;
		case MST_VOLUME:
			_ctrl->button (M2Contols::MasterVolume)->set_color (COLOR_WHITE);
			_ctrl->button (M2Contols::MasterTempo)->set_color (COLOR_BLACK);
			break;
		case MST_TEMPO:
			_ctrl->button (M2Contols::MasterVolume)->set_color (COLOR_BLACK);
			_ctrl->button (M2Contols::MasterTempo)->set_color (COLOR_WHITE);
			break;
	}
}

static void apply_ac_delta (boost::shared_ptr<AutomationControl> ac, double d) {
	if (!ac) {
		return;
	}
	ac->set_value (ac->interface_to_internal (std::min (ac->upper(), std::max (ac->lower(), ac->internal_to_interface (ac->get_value()) + d))),
			PBD::Controllable::UseGroup);
}

void
Maschine2::encoder_master (int delta)
{
	if (_ctrl->button (M2Contols::Grid)->is_pressed ()) {
		_ctrl->button (M2Contols::Grid)->ignore_release ();
		if (delta > 0) {
			AccessAction ("Editor", "next-snap-choice");
		} else {
			AccessAction ("Editor", "prev-snap-choice");
		}
		return;
	}
	switch (_master_state) {
		case MST_NONE:
			if (_ctrl->button (M2Contols::BtnShift, M2Contols::ModNone)->active ()) {
				if (delta > 0) {
					AccessAction ("Editor", "temporal-zoom-in");
				} else {
					AccessAction ("Editor", "temporal-zoom-out");
				}
			} else {
				if (delta > 0) {
					AccessAction ("Editor", "playhead-forward-to-grid");
				} else {
					AccessAction ("Editor", "playhead-backward-to-grid");
				}
			}
			break;
		case MST_VOLUME:
			{
				boost::shared_ptr<Route> master = session->master_out ();
				if (master) {
					// TODO consider _ctrl->button (M2Contols::EncoderWheel)->is_pressed() for fine grained
					const double factor = _ctrl->button (M2Contols::BtnShift, M2Contols::ModNone)->active () ? 256. : 32.;
					apply_ac_delta (master->gain_control(), delta / factor);
				}
			}
			break;
		case MST_TEMPO:
			// set new tempo..  apply with "enter"
			break;
	}
}

void
Maschine2::button_encoder ()
{
	switch (_master_state) {
		case MST_NONE:
			// OR: add marker ??
			if (_ctrl->button (M2Contols::BtnShift, M2Contols::ModNone)->active ()) {
				AccessAction ("Editor", "zoom-to-session");
			}
			break;
		case MST_VOLUME:
			// ignore -> fine gained?
			break;
		case MST_TEMPO:
			// add new tempo.. ?
			break;
	}
}

void
Maschine2::pad_change (unsigned int pad, float v)
{
	float lvl = v; // _ctrl->pad (pad)->value () / 4095.f;
	Gtkmm2ext::Color c = Gtkmm2ext::hsva_to_color (270 - 270.f * lvl, 1.0, lvl * lvl, 1.0);
	_ctrl->pad (pad)->set_color (c);
}

void
Maschine2::pad_event (unsigned int pad, float v, bool ev)
{
	if (ev) {
		uint8_t msg[3];
		msg[0] = v > 0 ? 0x90 : 0x80;
		msg[1] = 36 + pad; // TODO map note to scale
		msg[2] = ((uint8_t)floor (v * 127)) & 0x7f;
		_output_port->write (msg, 3, 0);
	} else {
		uint8_t msg[3];
		msg[0] = 0xa0;
		msg[1] = 36 + pad; // TODO map note to scale
		msg[2] = ((uint8_t)floor (v * 127)) & 0x7f;
		_output_port->write (msg, 3, 0);
	}
	//printf ("[%2d] %s %.1f\n", pad, ev ? "On/Off" : "Aftertouch" , v * 127);
}
