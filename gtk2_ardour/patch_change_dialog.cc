/*
    Copyright (C) 2010 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#include <gtkmm/stock.h>
#include <gtkmm/table.h>
#include <boost/algorithm/string.hpp>
#include "gtkmm2ext/utils.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/beats_frames_converter.h"
#include "patch_change_dialog.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

/** @param tc If non-0, a time converter for this patch change.  If 0, time control will be desensitized */
PatchChangeDialog::PatchChangeDialog (
	const ARDOUR::BeatsFramesConverter* tc,
	ARDOUR::Session* session,
	Evoral::PatchChange<Evoral::MusicalTime> const & patch,
	string const & model_name,
	string const & custom_device_node,
	const Gtk::BuiltinStockID& ok
	)
	: ArdourDialog (_("Patch Change"), true)
	, _time_converter (tc)
	, _model_name (model_name)
	, _custom_device_mode (custom_device_node)
	, _time (X_("patchchangetime"), true, "", true, false)
	, _channel (*manage (new Adjustment (1, 1, 16, 1, 4)))
	, _program (*manage (new Adjustment (1, 1, 128, 1, 16)))
	, _bank (*manage (new Adjustment (1, 1, 16384, 1, 64)))
	, _ignore_signals (false)
{
	Table* t = manage (new Table (4, 2));
	Label* l;
	t->set_spacings (6);
	int r = 0;

	if (_time_converter) {
		
		l = manage (left_aligned_label (_("Time")));
		t->attach (*l, 0, 1, r, r + 1);
		t->attach (_time, 1, 2, r, r + 1);
		++r;

		_time.set_session (session);
		_time.set_mode (AudioClock::BBT);
		_time.set (_time_converter->to (patch.time ()), true);
	}

	l = manage (left_aligned_label (_("Patch Bank")));
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_bank_combo, 1, 2, r, r + 1);
	++r;

	_bank_combo.signal_changed().connect (sigc::mem_fun (*this, &PatchChangeDialog::bank_combo_changed));

	l = manage (left_aligned_label (_("Patch")));
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_patch_combo, 1, 2, r, r + 1);
	++r;
	
	_patch_combo.signal_changed().connect (sigc::mem_fun (*this, &PatchChangeDialog::patch_combo_changed));

	l = manage (left_aligned_label (_("Channel")));
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_channel, 1, 2, r, r + 1);
	++r;

	_channel.set_value (patch.channel() + 1);
	_channel.signal_changed().connect (sigc::mem_fun (*this, &PatchChangeDialog::channel_changed));

	l = manage (left_aligned_label (_("Program")));
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_program, 1, 2, r, r + 1);
	++r;

	_program.set_value (patch.program () + 1);
	_program.signal_changed().connect (sigc::mem_fun (*this, &PatchChangeDialog::program_changed));

	l = manage (left_aligned_label (_("Bank")));
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_bank, 1, 2, r, r + 1);
	++r;

	_bank.set_value (patch.bank() + 1);
	_bank.signal_changed().connect (sigc::mem_fun (*this, &PatchChangeDialog::bank_changed));

	get_vbox()->add (*t);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (ok, RESPONSE_ACCEPT);
	set_default_response (RESPONSE_ACCEPT);

	fill_bank_combo ();
	set_active_bank_combo ();
	bank_combo_changed ();

	show_all ();
}

Evoral::PatchChange<Evoral::MusicalTime>
PatchChangeDialog::patch () const
{
	Evoral::MusicalTime t = 0;

	if (_time_converter) {
		t = _time_converter->from (_time.current_time ());
	}

	return Evoral::PatchChange<Evoral::MusicalTime> (
		t,
		_channel.get_value_as_int() - 1,
		_program.get_value_as_int() - 1,
		_bank.get_value_as_int() - 1
		);
}

/** Fill the bank_combo according to the current _channel */
void
PatchChangeDialog::fill_bank_combo ()
{
	MIDI::Name::ChannelNameSet::PatchBanks const * banks = get_banks ();
	if (banks == 0) {
		return;
	}

	for (MIDI::Name::ChannelNameSet::PatchBanks::const_iterator i = banks->begin(); i != banks->end(); ++i) {
		string n = (*i)->name ();
		boost::replace_all (n, "_", " ");
		_bank_combo.append_text (n);
	}
}

/** Set the active value of the bank_combo, and _current_patch_bank, from the contents of _bank */
void
PatchChangeDialog::set_active_bank_combo ()
{
	_current_patch_bank.reset ();
	
	MIDI::Name::ChannelNameSet::PatchBanks const * banks = get_banks ();
	if (banks == 0) {
		return;
	}

	for (MIDI::Name::ChannelNameSet::PatchBanks::const_iterator i = banks->begin(); i != banks->end(); ++i) {
		string n = (*i)->name ();
		boost::replace_all (n, "_", " ");

		MIDI::Name::PatchPrimaryKey const * key = (*i)->patch_primary_key ();
		if (key && ((key->msb << 7) | key->lsb) == _bank.get_value () - 1) {
			_current_patch_bank = *i;
			_ignore_signals = true;
			_bank_combo.set_active_text (n);
			_ignore_signals = false;
			return;
		}
	}

	_ignore_signals = true;
	_bank_combo.set_active (-1);
	_ignore_signals = false;
}

/** Update _current_patch_bank and reflect the current value of
 *  bank_combo in the rest of the dialog.
 */
void
PatchChangeDialog::bank_combo_changed ()
{
	if (_ignore_signals) {
		return;
	}
	
	_current_patch_bank.reset ();

	MIDI::Name::ChannelNameSet::PatchBanks const * banks = get_banks ();
	if (banks == 0) {
		return;
	}

	for (MIDI::Name::ChannelNameSet::PatchBanks::const_iterator i = banks->begin(); i != banks->end(); ++i) {
		string n = (*i)->name ();
		boost::replace_all (n, "_", " ");
		if (n == _bank_combo.get_active_text()) {
			_current_patch_bank = *i;
		}
	}

	if (_current_patch_bank == 0) {
		return;
	}

	/* Reflect */

	fill_patch_combo ();
	set_active_patch_combo ();

	MIDI::Name::PatchPrimaryKey const * key = _current_patch_bank->patch_primary_key ();
	if (key) {
		_ignore_signals = true;
		_bank.set_value (((key->msb << 7) | key->lsb) + 1);
		_ignore_signals = false;
	}
}

/** Fill the contents of the patch combo */
void
PatchChangeDialog::fill_patch_combo ()
{
	_patch_combo.clear ();

	if (_current_patch_bank == 0) {
		return;
	}

	const MIDI::Name::PatchBank::PatchNameList& patches = _current_patch_bank->patch_name_list ();
	for (MIDI::Name::PatchBank::PatchNameList::const_iterator j = patches.begin(); j != patches.end(); ++j) {
		string n = (*j)->name ();
		boost::replace_all (n, "_", " ");
		_patch_combo.append_text (n);
	}
}

/** Set the active value of the patch combo from the value of the _program entry */
void
PatchChangeDialog::set_active_patch_combo ()
{
	if (_ignore_signals) {
		return;
	}

	if (_current_patch_bank == 0) {
		_ignore_signals = true;
		_patch_combo.set_active (-1);
		_ignore_signals = false;
		return;
	}
	
	const MIDI::Name::PatchBank::PatchNameList& patches = _current_patch_bank->patch_name_list ();
	for (MIDI::Name::PatchBank::PatchNameList::const_iterator j = patches.begin(); j != patches.end(); ++j) {
		string n = (*j)->name ();
		boost::replace_all (n, "_", " ");

		MIDI::Name::PatchPrimaryKey const & key = (*j)->patch_primary_key ();
		if (key.program_number == _program.get_value() - 1) {
			_ignore_signals = true;
			_patch_combo.set_active_text (n);
			_ignore_signals = false;
			return;
		}
	}

	_ignore_signals = true;
	_patch_combo.set_active (-1);
	_ignore_signals = false;
}	

/** Set _program from the current state of _patch_combo */
void
PatchChangeDialog::patch_combo_changed ()
{
	if (_ignore_signals || _current_patch_bank == 0) {
		return;
	}

	const MIDI::Name::PatchBank::PatchNameList& patches = _current_patch_bank->patch_name_list ();
	for (MIDI::Name::PatchBank::PatchNameList::const_iterator j = patches.begin(); j != patches.end(); ++j) {
		string n = (*j)->name ();
		boost::replace_all (n, "_", " ");
		if (n == _patch_combo.get_active_text ()) {
			MIDI::Name::PatchPrimaryKey const & key = (*j)->patch_primary_key ();
			_ignore_signals = true;
			_program.set_value (key.program_number + 1);
			_ignore_signals = false;
		}
	}
}

void
PatchChangeDialog::channel_changed ()
{
	fill_bank_combo ();
	set_active_bank_combo ();
	fill_patch_combo ();
	set_active_patch_combo ();
}

void
PatchChangeDialog::program_changed ()
{
	if (_ignore_signals) {
		return;
	}

	set_active_patch_combo ();
}

void
PatchChangeDialog::bank_changed ()
{
	if (_ignore_signals) {
		return;
	}

	set_active_bank_combo ();
	fill_patch_combo ();
	set_active_patch_combo ();
}

MIDI::Name::ChannelNameSet::PatchBanks const *
PatchChangeDialog::get_banks ()
{
	MIDI::Name::MidiPatchManager& mpm = MIDI::Name::MidiPatchManager::instance ();
	boost::shared_ptr<MIDI::Name::ChannelNameSet> channel_name_set = mpm.find_channel_name_set (
		_model_name, _custom_device_mode, _channel.get_value_as_int() - 1
		);

	if (!channel_name_set) {
		return 0;
	}

	return &channel_name_set->patch_banks ();
}
