/*
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2010-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2011-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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

#include <glibmm/objectbase.h>

#include <gtkmm/messagedialog.h>
#include <gtkmm/stock.h>

#include "gtkmm2ext/keyboard.h"

#include "ardour/audioengine.h"
#include "ardour/mtdm.h"
#include "ardour/port_insert.h"
#include "ardour/session.h"

#include "widgets/binding_proxy.h"

#include "context_menu_helper.h"
#include "gui_thread.h"
#include "latency_gui.h"
#include "port_insert_ui.h"
#include "timers.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

PortInsertUI::PortInsertUI (Gtk::Window* parent, ARDOUR::Session* sess, boost::shared_ptr<ARDOUR::PortInsert> pi)
	: _pi (pi)
	, _measure_latency_button (_("Measure Latency"))
	, _invert_button (X_("Ø"))
	, _input_selector (parent, sess, pi->input ())
	, _output_selector (parent, sess, pi->output ())
	, _input_gpm (sess, 250)
	, _output_gpm (sess, 250)
	, _parent (parent)
	, _latency_gui (0)
	, _latency_dialog (0)
{
	_latency_hbox.pack_start (_measure_latency_button, false, false);
	_latency_hbox.pack_start (_edit_latency_button, false, false);
	_latency_hbox.pack_start (_latency_display, false, false);
	_latency_hbox.set_spacing (4);

	_output_selector.set_min_height_divisor (2);
	_input_selector.set_min_height_divisor (2);

	_input_vbox.pack_start (_input_gpm, false, false);
	_input_vbox.set_spacing (4);
	_input_vbox.set_border_width (4);

	_input_hbox.pack_start (_input_vbox, false, false);
	_input_hbox.pack_start (_input_selector, true, true);

	_output_vbox.pack_start (_output_gpm, false, false);
	_output_vbox.pack_start (_invert_button, false, false);
	_output_vbox.set_spacing (4);
	_output_vbox.set_border_width (4);

	_output_hbox.pack_start (_output_vbox, false, false);
	_output_hbox.pack_start (_output_selector, true, true);

	_notebook.append_page (_output_hbox, _("Send/Output"));
	_notebook.append_page (_input_hbox, _("Return/Input"));

	_notebook.set_current_page (0);

	set_spacing (12);
	pack_start (_notebook, true, true);
	pack_start (_latency_hbox, false, false);

	_invert_button.set_controllable (_pi->send_polarity_control ());
	_invert_button.watch ();
	_invert_button.set_name (X_("invert button"));
	_invert_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &PortInsertUI::invert_press), false);
	_invert_button.signal_button_release_event ().connect (sigc::mem_fun (*this, &PortInsertUI::invert_release), false);

	_edit_latency_button.set_icon (ArdourWidgets::ArdourIcon::LatencyClock);
	_edit_latency_button.add_elements (ArdourWidgets::ArdourButton::Text);
	_edit_latency_button.signal_clicked.connect (sigc::mem_fun (*this, &PortInsertUI::edit_latency_button_clicked));

	_measure_latency_button.set_name (X_("MeasureLatencyButton"));
	_measure_latency_button.signal_toggled ().connect (mem_fun (*this, &PortInsertUI::latency_button_toggled));
	_measure_latency_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &PortInsertUI::measure_latency_press), false);

	_input_gpm.setup_meters ();
	_input_gpm.set_fader_name (X_("SendUIFader"));
	_input_gpm.set_controls (boost::shared_ptr<Route> (), _pi->return_meter (), _pi->return_amp (), _pi->return_gain_control ());

	_output_gpm.setup_meters ();
	_output_gpm.set_fader_name (X_("SendUIFader"));
	_output_gpm.set_controls (boost::shared_ptr<Route> (), _pi->send_meter (), _pi->send_amp (), _pi->send_gain_control ());

	Gtkmm2ext::UI::instance ()->set_tip (_invert_button, _("Click to invert polarity of all send channels"));
	Gtkmm2ext::UI::instance ()->set_tip (_edit_latency_button, _("Edit Latency, manually override measured or I/O reported latency"));
	Gtkmm2ext::UI::instance ()->set_tip (_measure_latency_button, _("Measure Latency using the first port of each direction\n(note that gain is not applied during measurement).\nRight-click to forget previous meaurements,\nand revert to use default port latency."));

	_pi->set_metering (true);
	_pi->input ()->changed.connect (_connections, invalidator (*this), boost::bind (&PortInsertUI::return_changed, this, _1, _2), gui_context ());
	_pi->output ()->changed.connect (_connections, invalidator (*this), boost::bind (&PortInsertUI::send_changed, this, _1, _2), gui_context ());
	_pi->LatencyChanged.connect (_connections, invalidator (*this), boost::bind (&PortInsertUI::set_latency_label, this), gui_context ());

	_fast_screen_update_connection = Timers::super_rapid_connect (sigc::mem_fun (*this, &PortInsertUI::fast_update));

	set_latency_label ();
	set_measured_status (NULL);
	show_all ();
}

PortInsertUI::~PortInsertUI ()
{
	_pi->set_metering (false);
	_fast_screen_update_connection.disconnect ();
	delete _latency_gui;
	delete _latency_dialog;
}

void
PortInsertUI::send_changed (IOChange change, void* /*ignored*/)
{
	ENSURE_GUI_THREAD (*this, &PortInsertUI::outs_changed, change, ignored);
	if (change.type & IOChange::ConfigurationChanged) {
		_output_gpm.setup_meters ();
	}
}

void
PortInsertUI::return_changed (IOChange change, void* /*ignored*/)
{
	ENSURE_GUI_THREAD (*this, &PortInsertUI::outs_changed, change, ignored);
	if (change.type & IOChange::ConfigurationChanged) {
		_input_gpm.setup_meters ();
	}
}

void
PortInsertUI::fast_update ()
{
	if (!get_mapped ()) {
		return;
	}

	if (Config->get_meter_falloff () > 0.0f) {
		_input_gpm.update_meters ();
		_output_gpm.update_meters ();
	}
}

bool
PortInsertUI::invert_press (GdkEventButton* ev)
{
	if (ArdourWidgets::BindingProxy::is_bind_action (ev)) {
		return false;
	}

	if (ev->button != 1 || ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS) {
		return true;
	}

	boost::shared_ptr<AutomationControl> ac = _pi->send_polarity_control ();
	ac->start_touch (timepos_t (ac->session ().audible_sample ()));
	return true;
}

bool
PortInsertUI::invert_release (GdkEventButton* ev)
{
	if (ev->button != 1 || ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS) {
		return true;
	}

	boost::shared_ptr<AutomationControl> ac = _pi->send_polarity_control ();
	ac->set_value (_invert_button.get_active () ? 0 : 1, PBD::Controllable::NoGroup);
	ac->stop_touch (timepos_t (ac->session ().audible_sample ()));
	return true;
}

bool
PortInsertUI::check_latency_measurement ()
{
	MTDM* mtdm = _pi->mtdm ();

	if (mtdm->resolve () < 0) {
		_latency_display.set_text (_("No signal detected"));
		return true;
	}

	if (mtdm->err () > 0.3) {
		mtdm->invert ();
		mtdm->resolve ();
	}

	samplecnt_t const sample_rate = AudioEngine::instance ()->sample_rate ();

	if (sample_rate == 0) {
		_latency_display.set_text (_("Disconnected from audio engine"));
		_pi->stop_latency_detection ();
		return false;
	}

	if (!(mtdm->err () > 0.2 || mtdm->inv ())) {
		_pi->unset_user_latency ();
		_pi->set_measured_latency (rint (mtdm->del ()));
		_measure_latency_button.set_active (false);
	}

	set_measured_status (mtdm);
	return true;
}

void
PortInsertUI::forget_measuremed_latency ()
{
	_measure_latency_button.set_active (false);
	_pi->stop_latency_detection ();
	_pi->set_measured_latency (0);
	set_measured_status (NULL);
}

void
PortInsertUI::set_latency_label ()
{
	samplecnt_t const l  = _pi->effective_latency ();
	float const       sr = _pi->session ().sample_rate ();

	_edit_latency_button.set_text (ARDOUR_UI_UTILS::samples_as_time_string (l, sr, true));

	if (_latency_gui) {
		_latency_gui->refresh ();
	}
}

void
PortInsertUI::set_measured_status (MTDM* mtdm)
{
	samplecnt_t const ml = _pi->measured_latency ();
	float const       sr = _pi->session ().sample_rate ();
	if (sr <= 0 || ml <= 0) {
		_latency_display.set_text ("");
		return;
	}

	bool set = false;
	if (mtdm && ! (mtdm->err () > 0.2 || mtdm->inv ())) {
		set = true;
	}

	char buf[256];
	snprintf (buf, sizeof (buf), "%s %ld spl = %.2f ms%s%s%s",
			mtdm ? _("Measured:") : _("Previously measured:"),
			ml,
			ml * 1000.0f / sr,
			mtdm && mtdm->err () > 0.2 ? _(" (err)") : "",
			mtdm && mtdm->inv ()       ? _(" (inv)") : "",
			set                        ? _(" (set)") : ""
			);

	_latency_display.set_text (buf);
}

void
PortInsertUI::edit_latency_button_clicked ()
{
	assert (_pi);
	if (!_latency_gui) {
		_latency_gui    = new LatencyGUI (*(_pi.get ()), _pi->session ().sample_rate (), _pi->session ().get_block_size ());
		_latency_dialog = new ArdourWindow (_("Edit Latency"));
		/* use both keep-above and transient for to try cover as many
		   different WM's as possible.
		*/
		_latency_dialog->set_keep_above (true);
		_latency_dialog->set_transient_for (*_parent);
		_latency_dialog->add (*_latency_gui);
	}

	_latency_gui->refresh ();
	_latency_dialog->show_all ();
}

bool
PortInsertUI::measure_latency_press (GdkEventButton* ev)
{
	if (Gtkmm2ext::Keyboard::is_context_menu_event (ev)) {
		using namespace Gtk::Menu_Helpers;
		Gtk::Menu* menu  = ARDOUR_UI_UTILS::shared_popup_menu ();
		MenuList&  items = menu->items ();
		items.push_back (MenuElem (_("Forget previous measurement"), sigc::mem_fun (*this, &PortInsertUI::forget_measuremed_latency)));
		menu->popup (ev->button, ev->time);
		return true;
	}
	return false;
}

void
PortInsertUI::latency_button_toggled ()
{
	if (_measure_latency_button.get_active ()) {
		_pi->start_latency_detection ();
		_latency_display.set_text (_("Detecting ..."));
		_latency_timeout = Glib::signal_timeout ().connect (mem_fun (*this, &PortInsertUI::check_latency_measurement), 250);
	} else {
		_pi->stop_latency_detection ();
		_latency_timeout.disconnect ();
		set_measured_status (NULL);
	}
}

void
PortInsertUI::finished (IOSelector::Result r)
{
	_input_selector.Finished (r);
	_output_selector.Finished (r);
}

PortInsertWindow::PortInsertWindow (Gtk::Window& parent, ARDOUR::Session* s, boost::shared_ptr<ARDOUR::PortInsert> pi)
	: ArdourWindow (parent, string_compose (_("Port Insert: %1"), pi->name ()))
	, _portinsertui (this, s, pi)
{
	set_name ("IOSelectorWindow");
	add (_portinsertui);
}
