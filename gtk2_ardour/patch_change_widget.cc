/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2011 Paul Davis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <bitset>
#include <gtkmm/frame.h>

#include "pbd/unwind.h"

#include "evoral/midi_events.h"
#include "evoral/PatchChange.hpp"

#include "midi++/midnam_patch.h"

#include "ardour/instrument_info.h"
#include "ardour/midi_track.h"
#include "ardour/plugin_insert.h"

#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"
#include "widgets/tooltips.h"

#include "gui_thread.h"
#include "patch_change_widget.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;

PatchChangeWidget::PatchChangeWidget (boost::shared_ptr<ARDOUR::Route> r)
	: _route (r)
	, _bank_msb_spin (*manage (new Adjustment (0, 0, 127, 1, 16)))
	, _bank_lsb_spin (*manage (new Adjustment (0, 0, 127, 1, 16)))
	, _program_table (/*rows*/ 16, /*cols*/ 8, true)
	, _channel (-1)
	, _ignore_spin_btn_signals (false)
	, _no_notifications (false)
	, _info (r->instrument_info ())
	, _audition_enable (_("Audition on Change"), ArdourWidgets::ArdourButton::led_default_elements)
	, _audition_start_spin (*manage (new Adjustment (48, 0, 127, 1, 16)))
	, _audition_end_spin (*manage (new Adjustment (60, 0, 127, 1, 16)))
	, _audition_velocity (*manage (new Adjustment (100, 1, 127, 1, 16)))
	, _audition_note_on (false)
	, _piano ((PianoKeyboard*)piano_keyboard_new())
	, _pianomm (Glib::wrap((GtkWidget*)_piano))
{
	Box* box;
	box = manage (new HBox ());
	box->set_border_width (2);
	box->set_spacing (4);
	box->pack_start (*manage (new Label (_("Channel:"))), false, false);
	box->pack_start (_channel_select, false, false);
	box->pack_start (*manage (new Label (_("Bank:"))), false, false);
	box->pack_start (_bank_select, true, true);
	box->pack_start (*manage (new Label (_("MSB:"))), false, false);
	box->pack_start (_bank_msb_spin, false, false);
	box->pack_start (*manage (new Label (_("LSB:"))), false, false);
	box->pack_start (_bank_lsb_spin, false, false);

	pack_start (*box, false, false);

	_program_table.set_spacings (1);
	pack_start (_program_table, true, true);

	if (!boost::dynamic_pointer_cast<MidiTrack> (_route)) {
		pack_start ( *manage (new Label (_("Note: Patch Selection is volatile (only Midi-Tracks retain bank/patch selection)."))), false, false);
	}

	box = manage (new HBox ());
	box->set_spacing (4);
	box->pack_start (_audition_enable, false, false);
	box->pack_start (*manage (new Label (_("Start Note:"))), false, false);
	box->pack_start (_audition_start_spin, false, false);
	box->pack_start (*manage (new Label (_("End Note:"))), false, false);
	box->pack_start (_audition_end_spin, false, false);
	box->pack_start (*manage (new Label (_("Velocity:"))), false, false);
	box->pack_start (_audition_velocity, false, false);

	Box* box2 = manage (new HBox ());
	box2->pack_start (*box, true, false);
	box2->set_border_width (2);
	pack_start (*box2, false, false);

	for (uint8_t pgm = 0; pgm < 128; ++pgm) {
		_program_btn[pgm].set_text_ellipsize (Pango::ELLIPSIZE_END);
		_program_btn[pgm].set_layout_ellipsize_width (PANGO_SCALE * 112 * UIConfiguration::instance ().get_ui_scale ());
		int row = pgm % 16;
		int col = pgm / 16;
		_program_table.attach (_program_btn[pgm], col, col + 1, row, row + 1);
		_program_btn[pgm].signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PatchChangeWidget::select_program), pgm));
	}

	for (uint32_t chn = 0; chn < 16; ++chn) {
		using namespace Menu_Helpers;
		using namespace Gtkmm2ext;
		char buf[8];
		snprintf (buf, sizeof(buf), "%d", chn + 1);
		_channel_select.AddMenuElem (MenuElemNoMnemonic (buf, sigc::bind (sigc::mem_fun (*this, &PatchChangeWidget::select_channel), chn)));
	}

	piano_keyboard_set_monophonic (_piano, TRUE);
	g_signal_connect (G_OBJECT (_piano), "note-on", G_CALLBACK (PatchChangeWidget::_note_on_event_handler), this);
	g_signal_connect (G_OBJECT (_piano), "note-off", G_CALLBACK (PatchChangeWidget::_note_off_event_handler), this);
	_pianomm->set_flags(Gtk::CAN_FOCUS);
	pack_start (*_pianomm, false, false);

	_audition_start_spin.set_sensitive (false);
	_audition_end_spin.set_sensitive (false);

	_audition_enable.signal_clicked.connect (sigc::mem_fun (*this, &PatchChangeWidget::audition_toggle));
	_audition_start_spin.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &PatchChangeWidget::check_note_range), false));
	_audition_end_spin.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &PatchChangeWidget::check_note_range), true));
	_bank_msb_spin.signal_changed().connect (sigc::mem_fun (*this, &PatchChangeWidget::select_bank_spin));
	_bank_lsb_spin.signal_changed().connect (sigc::mem_fun (*this, &PatchChangeWidget::select_bank_spin));

	_info.Changed.connect (_info_changed_connection, invalidator (*this),
			boost::bind (&PatchChangeWidget::instrument_info_changed, this), gui_context());

	if (!boost::dynamic_pointer_cast<MidiTrack> (_route)) {
		_route->processors_changed.connect (_route_connection, invalidator (*this),
				boost::bind (&PatchChangeWidget::processors_changed, this), gui_context());
		processors_changed ();
	}

	set_spacing (4);
	show_all ();
}

PatchChangeWidget::~PatchChangeWidget ()
{
	cancel_audition ();
	delete _pianomm;
}

void
PatchChangeWidget::refresh ()
{
	if (is_visible ()) {
		on_show ();
	}
}

void
PatchChangeWidget::on_show ()
{
	Gtk::VBox::on_show ();
	cancel_audition ();
	_channel = -1;
	select_channel (0);
}

void
PatchChangeWidget::on_hide ()
{
	Gtk::VBox::on_hide ();
	_ac_connections.drop_connections ();
	cancel_audition ();
}

void
PatchChangeWidget::select_channel (uint8_t chn)
{
	assert (_route);
	assert (chn < 16);

	if (_channel == chn) {
		return;
	}

	cancel_audition ();

	_channel_select.set_text (string_compose ("%1", (int)(chn + 1)));
	_channel = chn;
	_no_notifications = false;

	_ac_connections.drop_connections ();

	if (boost::dynamic_pointer_cast<MidiTrack> (_route)) {
		boost::shared_ptr<AutomationControl> bank_msb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, chn, MIDI_CTL_MSB_BANK), true);
		boost::shared_ptr<AutomationControl> bank_lsb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, chn, MIDI_CTL_LSB_BANK), true);
		boost::shared_ptr<AutomationControl> program = _route->automation_control(Evoral::Parameter (MidiPgmChangeAutomation, chn), true);

		bank_msb->Changed.connect (_ac_connections, invalidator (*this),
				boost::bind (&PatchChangeWidget::bank_changed, this), gui_context ());
		bank_lsb->Changed.connect (_ac_connections, invalidator (*this),
				boost::bind (&PatchChangeWidget::bank_changed, this), gui_context ());
		program->Changed.connect (_ac_connections, invalidator (*this),
				boost::bind (&PatchChangeWidget::program_changed, this), gui_context ());
	} else if (boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_route->the_instrument())) {
		if (pi->plugin()->knows_bank_patch ()) {
			pi->plugin ()->BankPatchChange.connect (_ac_connections, invalidator (*this),
					boost::bind (&PatchChangeWidget::bankpatch_changed, this, _1), gui_context ());
		} else {
			_no_notifications = true;
			// TODO add note: instrument does not report changes.
		}
	}

	refill_banks ();
}

void
PatchChangeWidget::refill_banks ()
{
	cancel_audition ();
	using namespace Menu_Helpers;
	using namespace Gtkmm2ext;

	_current_patch_bank.reset ();
	_bank_select.clear_items ();

	const int b = bank (_channel);

	{
		PBD::Unwinder<bool> (_ignore_spin_btn_signals, true);
		_bank_msb_spin.set_value (b >> 7);
		_bank_lsb_spin.set_value (b & 127);
	}

	boost::shared_ptr<MIDI::Name::ChannelNameSet> cns = _info.get_patches (_channel);
	if (cns) {
		for (MIDI::Name::ChannelNameSet::PatchBanks::const_iterator i = cns->patch_banks().begin(); i != cns->patch_banks().end(); ++i) {
			std::string n = (*i)->name ();
			if ((*i)->number () == UINT16_MAX) {
				continue;
			}
			_bank_select.AddMenuElem (MenuElemNoMnemonic (n, sigc::bind (sigc::mem_fun (*this, &PatchChangeWidget::select_bank), (*i)->number ())));
			if ((*i)->number () == b) {
				_current_patch_bank = *i;
				_bank_select.set_text (n);
			}
		}
	}

	if (!_current_patch_bank) {
		std::string n = string_compose (_("Bank %1"), b);
		_bank_select.AddMenuElem (MenuElemNoMnemonic (n, sigc::bind (sigc::mem_fun (*this, &PatchChangeWidget::select_bank), b)));
		_bank_select.set_text (n);
	}

	refill_program_list ();
}

void
PatchChangeWidget::refill_program_list ()
{
	std::bitset<128> unset_notes;
	unset_notes.set ();

	if (_current_patch_bank) {
		const MIDI::Name::PatchNameList& patches = _current_patch_bank->patch_name_list ();
		for (MIDI::Name::PatchNameList::const_iterator i = patches.begin(); i != patches.end(); ++i) {
			const std::string n = (*i)->name ();
			MIDI::Name::PatchPrimaryKey const& key = (*i)->patch_primary_key ();

			const uint8_t pgm = key.program();
			_program_btn[pgm].set_text (n);
			set_tooltip (_program_btn[pgm], string_compose (_("%1 (Pgm-%2)"),
						Gtkmm2ext::markup_escape_text (n), (int)(pgm +1)));
			unset_notes.reset (pgm);
		}
	}

	for (uint8_t pgm = 0; pgm < 128; ++pgm) {
		if (!unset_notes.test (pgm)) {
			_program_btn[pgm].set_name (X_("patch change button"));
			continue;
		}
		std::string n = string_compose (_("Pgm-%1"), (int)(pgm +1));
		_program_btn[pgm].set_text (n);
		_program_btn[pgm].set_name (X_("patch change dim button"));
		set_tooltip (_program_btn[pgm], n);
	}

	program_changed ();
}

/* ***** user GUI actions *****/

void
PatchChangeWidget::select_bank_spin ()
{
	if (_ignore_spin_btn_signals) {
		return;
	}
	const uint32_t b = (_bank_msb_spin.get_value_as_int() << 7) + _bank_lsb_spin.get_value_as_int();
	select_bank (b);
}

void
PatchChangeWidget::select_bank (uint32_t bank)
{
	cancel_audition ();

	if (boost::dynamic_pointer_cast<MidiTrack> (_route)) {
		boost::shared_ptr<AutomationControl> bank_msb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, _channel, MIDI_CTL_MSB_BANK), true);
		boost::shared_ptr<AutomationControl> bank_lsb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, _channel, MIDI_CTL_LSB_BANK), true);

		bank_msb->set_value (bank >> 7, PBD::Controllable::NoGroup);
		bank_lsb->set_value (bank & 127, PBD::Controllable::NoGroup);
	} else if (boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_route->the_instrument())) {
		uint8_t event[3];
		event[0] = (MIDI_CMD_CONTROL | _channel);
		event[1] = 0x00;
		event[2] = bank >> 7;
		pi->write_immediate_event (3, event);

		event[1] = 0x20;
		event[2] = bank & 127;
		pi->write_immediate_event (3, event);
	}

	select_program (program (_channel));
}

void
PatchChangeWidget::select_program (uint8_t pgm)
{
	cancel_audition ();
	if (_no_notifications) {
		program_changed ();
	}

	if (pgm > 127) {
		return;
	}

	if (boost::dynamic_pointer_cast<MidiTrack> (_route)) {
		boost::shared_ptr<AutomationControl> program = _route->automation_control(Evoral::Parameter (MidiPgmChangeAutomation, _channel), true);
		program->set_value (pgm, PBD::Controllable::NoGroup);
	} else if (boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_route->the_instrument())) {
		uint8_t event[2];
		event[0] = (MIDI_CMD_PGM_CHANGE | _channel);
		event[1] = pgm;
		pi->write_immediate_event (2, event);
	}

	audition ();
}

/* ***** callbacks, external changes *****/

void
PatchChangeWidget::bank_changed ()
{
	refill_banks ();
}

void
PatchChangeWidget::bankpatch_changed (uint8_t chn)
{
	if (chn == _channel) {
		refill_banks ();
	}
}

void
PatchChangeWidget::program_changed ()
{
	uint8_t p = program (_channel);
	for (uint8_t pgm = 0; pgm < 128; ++pgm) {
		_program_btn[pgm].set_active (pgm == p);
	}
}

void
PatchChangeWidget::instrument_info_changed ()
{
	refill_banks ();
}

void
PatchChangeWidget::processors_changed ()
{
	assert (!boost::dynamic_pointer_cast<MidiTrack> (_route));
	if (_route->the_instrument ()) {
		set_sensitive (true);
	} else {
		set_sensitive (false);
	}
}

/* ***** play notes *****/

void
PatchChangeWidget::audition_toggle ()
{
	_audition_enable.set_active (!_audition_enable.get_active ());
	if (_audition_enable.get_active()) {
		_audition_start_spin.set_sensitive (true);
		_audition_end_spin.set_sensitive (true);
	} else {
		cancel_audition ();
		_audition_start_spin.set_sensitive (false);
		_audition_end_spin.set_sensitive (false);
	}
}

void
PatchChangeWidget::check_note_range (bool upper)
{
	int s = _audition_start_spin.get_value_as_int ();
	int e = _audition_end_spin.get_value_as_int ();
	if (s <= e) {
		return;
	}
	if (upper) {
		_audition_start_spin.set_value (e);
	} else {
		_audition_end_spin.set_value (s);
	}
}

void
PatchChangeWidget::cancel_audition ()
{
	_note_queue_connection.disconnect();

	if (_audition_note_on) {
		note_off_event_handler (_audition_note_num);
		piano_keyboard_set_note_off (_piano, _audition_note_num);
	}
}

void
PatchChangeWidget::audition ()
{
	if (!boost::dynamic_pointer_cast<MidiTrack> (_route) && !boost::dynamic_pointer_cast<PluginInsert> (_route)) {
		return;
	}
	if (_channel > 16) {
		return;
	}

	if (_note_queue_connection.connected ()) {
		cancel_audition ();
	}

	if (!_audition_enable.get_active ()) {
		return;
	}

	assert (!_audition_note_on);
	_audition_note_num = _audition_start_spin.get_value_as_int ();

	_note_queue_connection = Glib::signal_timeout().connect (sigc::bind (sigc::mem_fun (&PatchChangeWidget::audition_next), this), 250);
}

bool
PatchChangeWidget::audition_next ()
{
	if (_audition_note_on) {
		note_off_event_handler (_audition_note_num);
		piano_keyboard_set_note_off (_piano, _audition_note_num);
		return ++_audition_note_num <= _audition_end_spin.get_value_as_int() && _audition_enable.get_active ();
	} else {
		note_on_event_handler (_audition_note_num, true);
		piano_keyboard_set_note_on (_piano, _audition_note_num);
		return true;
	}
}

void
PatchChangeWidget::_note_on_event_handler(GtkWidget*, int note, gpointer arg)
{
	((PatchChangeWidget*)arg)->note_on_event_handler(note, false);
}

void
PatchChangeWidget::_note_off_event_handler(GtkWidget*, int note, gpointer arg)
{
	((PatchChangeWidget*)arg)->note_off_event_handler(note);
}

void
PatchChangeWidget::note_on_event_handler (int note, bool for_audition)
{
	if (!for_audition) {
		cancel_audition ();
		_pianomm->grab_focus ();
	}
	uint8_t event[3];
	event[0] = (MIDI_CMD_NOTE_ON | _channel);
	event[1] = note;
	event[2] = _audition_velocity.get_value_as_int ();
	_audition_note_on = true;
	_audition_note_num = note;

	if (boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (_route)) {
		mt->write_immediate_event (3, event);
	} else if (boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_route->the_instrument())) {
		pi->write_immediate_event (3, event);
	}
}

void
PatchChangeWidget::note_off_event_handler (int note)
{
	uint8_t event[3];
	event[0] = (MIDI_CMD_NOTE_OFF | _channel);
	event[1] = note;
	event[2] = 0;
	_audition_note_on = false;

	if (boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (_route)) {
		mt->write_immediate_event (3, event);
	} else if (boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_route->the_instrument())) {
		pi->write_immediate_event (3, event);
	}
}

/* ***** query info *****/

int
PatchChangeWidget::bank (uint8_t chn) const
{
	if (boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (_route)) {
		boost::shared_ptr<AutomationControl> bank_msb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, chn, MIDI_CTL_MSB_BANK), true);
		boost::shared_ptr<AutomationControl> bank_lsb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, chn, MIDI_CTL_LSB_BANK), true);

		return ((int)bank_msb->get_value () << 7) + (int)bank_lsb->get_value();
	} else if (boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_route->the_instrument())) {
		uint32_t bankpatch = pi->plugin()->bank_patch (chn);
		if (bankpatch != UINT32_MAX) {
			return bankpatch >> 7;
		}
	}
	return 0;
}

uint8_t
PatchChangeWidget::program (uint8_t chn) const
{
	if (boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (_route)) {
		boost::shared_ptr<AutomationControl> program = _route->automation_control(Evoral::Parameter (MidiPgmChangeAutomation, chn), true);
		return program->get_value();
	} else if (boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_route->the_instrument())) {
		uint32_t bankpatch = pi->plugin()->bank_patch (chn);
		if (bankpatch != UINT32_MAX) {
			return bankpatch & 127;
		}
	}
	return 255;
}

/* ***************************************************************************/

PatchChangeGridDialog::PatchChangeGridDialog (boost::shared_ptr<ARDOUR::Route> r)
	: ArdourDialog (string_compose (_("Select Patch for \"%1\""), r->name()), false, false)
	, w (r)
{
	r->PropertyChanged.connect (_route_connection, invalidator (*this), boost::bind (&PatchChangeGridDialog::route_property_changed, this, _1, boost::weak_ptr<Route>(r)), gui_context());
	get_vbox()->add (w);
	w.show ();
}

void
PatchChangeGridDialog::route_property_changed (const PBD::PropertyChange& what_changed, boost::weak_ptr<Route> wr)
{
	boost::shared_ptr<ARDOUR::Route> r = wr.lock ();
	if (r && what_changed.contains (ARDOUR::Properties::name)) {
		set_title (string_compose (_("Select Patch for \"%1\"'"), r->name()));
	}
}
