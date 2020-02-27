/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/sizegroup.h>

#include "ardour/dB.h"
#include "ardour/profile.h"
#include "widgets/tooltips.h"
#include "gtkmm2ext/gui_thread.h"

#include "actions.h"
#include "ardour_ui.h"
#include "timers.h"
#include "transport_control_ui.h"

#include "pbd/i18n.h"

using namespace Glib;
using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;

TransportControlUI::TransportControlUI ()
{
	Config->ParameterChanged.connect (config_connection, MISSING_INVALIDATOR, boost::bind (&TransportControlUI::parameter_changed, this, _1), gui_context());
}

void
TransportControlUI::map_actions ()
{
	/* setup actions */
	RefPtr<Action> act;

	act = ActionManager::get_action (X_("Transport"), X_("ToggleClick"));
	click_button.set_related_action (act);
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

	/* tooltips depend on actions */
	set_tooltip (roll_button, _("Play from playhead"));
	set_tooltip (stop_button, _("Stop playback"));
	set_tooltip (rec_button, _("Toggle record"));
	set_tooltip (play_selection_button, _("Play range/selection"));
	set_tooltip (goto_start_button, _("Go to start of session"));
	set_tooltip (goto_end_button, _("Go to end of session"));
	set_tooltip (auto_loop_button, _("Play loop range"));
	set_tooltip (midi_panic_button, _("MIDI Panic\nSend note off and reset controller messages on all MIDI channels"));

	/* set click_button tooltip */
	parameter_changed ("click-gain");
}

void
TransportControlUI::setup (TransportControlProvider* ui)
{
	click_button.signal_button_press_event().connect (sigc::mem_fun (*ui, &TransportControlProvider::click_button_clicked), false);
	click_button.signal_scroll_event().connect (sigc::mem_fun (*this, &TransportControlUI::click_button_scroll), false);

	/* setup icons */

	click_button.set_icon (ArdourIcon::TransportMetronom);
	goto_start_button.set_icon (ArdourIcon::TransportStart);
	goto_end_button.set_icon (ArdourIcon::TransportEnd);
	roll_button.set_icon (ArdourIcon::TransportPlay);
	stop_button.set_icon (ArdourIcon::TransportStop);
	play_selection_button.set_icon (ArdourIcon::TransportRange);
	auto_loop_button.set_icon (ArdourIcon::TransportLoop);
	rec_button.set_icon (ArdourIcon::RecButton);
	midi_panic_button.set_icon (ArdourIcon::TransportPanic);

	/* transport control size-group */

	Glib::RefPtr<SizeGroup> transport_button_size_group = SizeGroup::create (SIZE_GROUP_BOTH);
	transport_button_size_group->add_widget (goto_start_button);
	transport_button_size_group->add_widget (goto_end_button);
	transport_button_size_group->add_widget (auto_loop_button);
	transport_button_size_group->add_widget (rec_button);
	if (!ARDOUR::Profile->get_mixbus()) {
		/*note: since we aren't showing this button, it doesn't get allocated
		 * and therefore blows-up the size-group.  so remove it.
		 */
		transport_button_size_group->add_widget (play_selection_button);
	}
	transport_button_size_group->add_widget (roll_button);
	transport_button_size_group->add_widget (stop_button);

	transport_button_size_group->add_widget (midi_panic_button);
	transport_button_size_group->add_widget (click_button);

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))

	click_button.set_size_request (PX_SCALE(20), PX_SCALE(20));
	set_spacing (PX_SCALE(2));

#undef PX_SCALE

	if (!ARDOUR::Profile->get_mixbus()) {
		pack_start (midi_panic_button, true, true, 0);
	} else {
		pack_start (midi_panic_button, true, true, 3);
	}
	pack_start (click_button, true, true, 0);
	pack_start (goto_start_button, true, true);
	pack_start (goto_end_button, true, true);
	pack_start (auto_loop_button, true, true);
	if (!ARDOUR::Profile->get_mixbus()) {
		pack_start (play_selection_button, true, true);
	}
	pack_start (roll_button, true, true);
	pack_start (stop_button, true, true);
	pack_start (rec_button, true, true, 3);

	roll_button.set_name ("transport button");
	stop_button.set_name ("transport button");
	goto_start_button.set_name ("transport button");
	goto_end_button.set_name ("transport button");
	auto_loop_button.set_name ("transport button");
	play_selection_button.set_name ("transport button");
	rec_button.set_name ("transport recenable button");
	midi_panic_button.set_name ("transport button"); // XXX ???
	click_button.set_name ("transport button");

	roll_button.set_controllable (ui->roll_controllable);
	stop_button.set_controllable (ui->stop_controllable);
	goto_start_button.set_controllable (ui->goto_start_controllable);
	goto_end_button.set_controllable (ui->goto_end_controllable);
	auto_loop_button.set_controllable (ui->auto_loop_controllable);
	play_selection_button.set_controllable (ui->play_selection_controllable);
	rec_button.set_controllable (ui->rec_controllable);

	stop_button.set_active (true);

	Timers::blink_connect (sigc::mem_fun (*this, &TransportControlUI::blink_rec_enable));
}

void
TransportControlUI::set_session (ARDOUR::Session *s)
{
	SessionHandlePtr::set_session (s);
	set_loop_sensitivity ();
	map_transport_state ();

	if (!_session) {
		rec_button.set_sensitive (false);
		return;
	}

	_session->config.ParameterChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&TransportControlUI::parameter_changed, this, _1), gui_context());
	_session->StepEditStatusChange.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&TransportControlUI::step_edit_status_change, this, _1), gui_context());
	_session->TransportStateChange.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&TransportControlUI::map_transport_state, this), gui_context());
	_session->auto_loop_location_changed.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&TransportControlUI::set_loop_sensitivity, this), gui_context ());
	_session->PunchLoopConstraintChange.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&TransportControlUI::set_loop_sensitivity, this), gui_context ());

	rec_button.set_sensitive (true);
}

void
TransportControlUI::parameter_changed (std::string p)
{
	if (p == "external-sync") {
		set_loop_sensitivity ();
	} else if (p == "click-record-only") {
		// TODO set a flag, blink or gray-out metronome button while rolling, only
		if (Config->get_click_record_only()) {
			click_button.set_name ("generic button"); // XXX
		} else {
			click_button.set_name ("transport button");
		}
	} else if (p == "click-gain") {
		float gain_db = accurate_coefficient_to_dB (Config->get_click_gain());
		char tmp[32];
		snprintf(tmp, 31, "%+.1f", gain_db);
		set_tooltip (click_button, string_compose (_("Enable/Disable metronome\n\nRight-click to access preferences\nMouse-wheel to modify level\nSignal Level: %1 dBFS"), tmp));
	}
}

void
TransportControlUI::map_transport_state ()
{
	if (!_session) {
		auto_loop_button.unset_active_state ();
		play_selection_button.unset_active_state ();
		roll_button.unset_active_state ();
		stop_button.set_active_state (Gtkmm2ext::ExplicitActive);
		return;
	}

	float sp = _session->transport_speed();

	if (sp != 0.0f) {

		/* we're rolling */

		if (_session->get_play_range()) {

			play_selection_button.set_active_state (Gtkmm2ext::ExplicitActive);
			roll_button.unset_active_state ();
			auto_loop_button.unset_active_state ();

		} else if (_session->get_play_loop ()) {

			auto_loop_button.set_active (true);
			play_selection_button.set_active (false);

			if (Config->get_loop_is_mode()) {
				roll_button.set_active (true);
			} else {
				roll_button.set_active (false);
			}

		} else {

			roll_button.set_active (true);
			play_selection_button.set_active (false);
			auto_loop_button.set_active (false);

		}

		if (UIConfiguration::instance().get_follow_edits() && !_session->config.get_external_sync()) {
			/* light up both roll and play-selection if they are joined */
			roll_button.set_active (true);
			play_selection_button.set_active (true);
		}

		stop_button.set_active (false);

	} else {

		stop_button.set_active (true);
		roll_button.set_active (false);
		play_selection_button.set_active (false);
		if (Config->get_loop_is_mode ()) {
			auto_loop_button.set_active (_session->get_play_loop());
		} else {
			auto_loop_button.set_active (false);
		}
	}
}

void
TransportControlUI::step_edit_status_change (bool yn)
{
	// XXX should really store pre-step edit status of things
	// we make insensitive

	if (yn) {
		rec_button.set_active_state (Gtkmm2ext::ImplicitActive);
		rec_button.set_sensitive (false);
	} else {
		rec_button.unset_active_state ();;
		rec_button.set_sensitive (true);
	}
}

void
TransportControlUI::set_loop_sensitivity ()
{
	if (!_session || _session->config.get_external_sync()) {
		auto_loop_button.set_sensitive (false);
	} else {
		auto_loop_button.set_sensitive (_session && _session->loop_is_possible() && _session->locations()->auto_loop_location());
	}
}

void
TransportControlUI::blink_rec_enable (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->step_editing()) {
		return;
	}

	Session::RecordState const r = _session->record_status ();
	bool const h = _session->have_rec_enabled_track ();

	if (r == Session::Enabled || (r == Session::Recording && !h)) {
		if (onoff) {
			rec_button.set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			rec_button.set_active_state (Gtkmm2ext::Off);
		}
	} else if (r == Session::Recording && h) {
		rec_button.set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		rec_button.unset_active_state ();
	}
}

bool
TransportControlUI::click_button_scroll (GdkEventScroll* ev)
{
	gain_t gain = Config->get_click_gain();
	float gain_db = accurate_coefficient_to_dB (gain);

	switch (ev->direction) {
		case GDK_SCROLL_UP:
		case GDK_SCROLL_LEFT:
			gain_db += 1;
			break;
		case GDK_SCROLL_DOWN:
		case GDK_SCROLL_RIGHT:
			gain_db -= 1;
			break;
	}
	gain_db = std::max (-60.f, gain_db);
	gain = dB_to_coefficient (gain_db);
	gain = std::min (gain, Config->get_max_gain());
	Config->set_click_gain (gain);
	return true;
}
