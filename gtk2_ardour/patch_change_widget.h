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

#ifndef __gtkardour_patch_change_widget_h__
#define __gtkardour_patch_change_widget_h__

#include <gtkmm/box.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>

#include "pbd/signals.h"
#include "midi++/midnam_patch.h"

#include "ardour/route.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

#include "ardour_dialog.h"
#include "pianokeyboard.h"

class PatchChangeWidget : public Gtk::VBox
{
public:
	PatchChangeWidget (boost::shared_ptr<ARDOUR::Route>);
	~PatchChangeWidget ();

	void refresh ();

protected:
	int bank (uint8_t) const;
	uint8_t program (uint8_t) const;

	void on_show ();
	void on_hide ();

private:
	boost::shared_ptr<ARDOUR::Route> _route;

	ArdourWidgets::ArdourDropdown _channel_select;
	ArdourWidgets::ArdourDropdown _bank_select;
	Gtk::SpinButton               _bank_msb_spin;
	Gtk::SpinButton               _bank_lsb_spin;
	ArdourWidgets::ArdourButton   _program_btn[128];
	Gtk::Table                    _program_table;

	uint8_t _channel;
	bool    _ignore_spin_btn_signals;
	bool    _no_notifications;

	void select_channel (uint8_t);
	void select_bank (uint32_t);
	void select_bank_spin ();
	void select_program (uint8_t);

	void bank_changed ();
	void program_changed ();
	void bankpatch_changed (uint8_t);

	void refill_banks ();

	void instrument_info_changed ();
	void processors_changed ();

	PBD::ScopedConnection _info_changed_connection;
	PBD::ScopedConnection _route_connection;
	PBD::ScopedConnectionList _ac_connections;

	ARDOUR::InstrumentInfo& _info;
	boost::shared_ptr<MIDI::Name::PatchBank> _current_patch_bank;

	void audition_toggle ();
	void check_note_range (bool);
	void audition ();
	void cancel_audition ();
	bool audition_next ();
	sigc::connection _note_queue_connection;

	ArdourWidgets::ArdourButton _audition_enable;
	Gtk::SpinButton             _audition_start_spin; // Consider a click-box w/note-names
	Gtk::SpinButton             _audition_end_spin;
	Gtk::SpinButton             _audition_velocity;
	uint8_t                     _audition_note_num;
	bool                        _audition_note_on;

	APianoKeyboard _piano;

	void _note_on_event_handler (int, int);
	void note_on_event_handler (int, bool for_audition);
	void note_off_event_handler (int);
};

class PatchChangeGridDialog : public ArdourDialog
{
public:
	PatchChangeGridDialog (boost::shared_ptr<ARDOUR::Route>);
	void on_hide () { w.hide (); ArdourDialog::on_hide (); }
	void on_show () { w.show (); ArdourDialog::on_show (); }
	void refresh () { w.refresh (); }

private:
	void route_property_changed (const PBD::PropertyChange&, boost::weak_ptr<ARDOUR::Route>);
	PBD::ScopedConnection _route_connection;
	PatchChangeWidget w;
};

#endif
