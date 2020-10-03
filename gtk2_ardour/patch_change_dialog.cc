/*
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/stock.h>
#include <gtkmm/table.h>

#include <boost/algorithm/string.hpp>

#include "gtkmm2ext/utils.h"

#include "midi++/midnam_patch.h"

#include "ardour/instrument_info.h"
#include "ardour/region.h"

#include "patch_change_dialog.h"
#include "gui_thread.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

/** @param tc If non-0, a time converter for this patch change.  If 0, time control will be desensitized */
PatchChangeDialog::PatchChangeDialog (
	const ARDOUR::BeatsSamplesConverter*        tc,
	ARDOUR::Session*                            session,
	Evoral::PatchChange<Temporal::Beats> const& patch,
	ARDOUR::InstrumentInfo&                     info,
	const Gtk::BuiltinStockID&                  ok,
	bool                                        allow_delete,
	bool                                        modal,
	boost::shared_ptr<ARDOUR::Region>           region)
	: ArdourDialog (_("Patch Change"), modal)
	, _region (region)
	, _info (info)
	, _time (X_("patchchangetime"), true, "", true, false)
	, _channel (*manage (new Adjustment (1, 1, 16, 1, 4)))
	, _program (*manage (new Adjustment (1, 1, 128, 1, 16)))
	, _bank_msb (*manage (new Adjustment (0, 0, 127, 1, 16)))
	, _bank_lsb (*manage (new Adjustment (0, 0, 127, 1, 16)))
	, _ignore_signals (false)
	, _keep_open (!modal)
{
	Table* t = manage (new Table (4, 2));
	Label* l;
	t->set_spacings (6);
	int r = 0;

	if (_region) {

		l = manage (left_aligned_label (_("Time")));
		t->attach (*l, 0, 1, r, r + 1);
		t->attach (_time, 1, 2, r, r + 1);
		++r;

		_time.set_session (session);
		_time.set_mode (AudioClock::BBT);
		_time.set (_region->source_beats_to_absolute_time (patch.time ()), true);
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

	l = manage (left_aligned_label (_("Bank MSB")));
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_bank_msb, 1, 2, r, r + 1);
	++r;

	l = manage (left_aligned_label (_("Bank LSB")));
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_bank_lsb, 1, 2, r, r + 1);
	++r;

	assert (patch.bank() != UINT16_MAX);

	_bank_msb.set_value ((patch.bank() >> 7));
	_bank_msb.signal_changed().connect (sigc::mem_fun (*this, &PatchChangeDialog::bank_changed));
	_bank_lsb.set_value ((patch.bank() & 127));
	_bank_lsb.signal_changed().connect (sigc::mem_fun (*this, &PatchChangeDialog::bank_changed));

	get_vbox()->add (*t);

	if (modal) {
		add_button (Stock::CANCEL, RESPONSE_CANCEL);
	}
	add_button (ok, RESPONSE_ACCEPT);
	if (allow_delete) {
		add_button (Gtk::StockID(GTK_STOCK_DELETE), RESPONSE_REJECT);
	}
	set_default_response (RESPONSE_ACCEPT);

	fill_bank_combo ();
	set_active_bank_combo ();
	bank_combo_changed ();

	_info.Changed.connect (_info_changed_connection, invalidator (*this),
			       boost::bind (&PatchChangeDialog::instrument_info_changed, this), gui_context());

	show_all ();
}

void
PatchChangeDialog::on_response (int response_id)
{
	if (_keep_open) {
		Gtk::Dialog::on_response (response_id);
	} else {
		ArdourDialog::on_response (response_id);
	}
}

int
PatchChangeDialog::get_14bit_bank () const
{
	return (_bank_msb.get_value_as_int() << 7) + _bank_lsb.get_value_as_int();
}

void
PatchChangeDialog::instrument_info_changed ()
{
	_bank_combo.clear ();
	_patch_combo.clear ();
	fill_bank_combo ();
	fill_patch_combo ();
}

Evoral::PatchChange<Temporal::Beats>
PatchChangeDialog::patch () const
{
	Temporal::Beats t = Temporal::Beats();

	if (_region) {
		t = _region->region_beats_to_source_beats (_time.current_time ().beats());
	}

	return Evoral::PatchChange<Temporal::Beats> (
		t,
		_channel.get_value_as_int() - 1,
		_program.get_value_as_int() - 1,
		get_14bit_bank ()
		);
}

/** Fill the bank_combo according to the current _channel */
void
PatchChangeDialog::fill_bank_combo ()
{
	_bank_combo.clear ();

	boost::shared_ptr<MIDI::Name::ChannelNameSet> cns = _info.get_patches (_channel.get_value_as_int() - 1);

	if (!cns) {
		return;
	}

	for (MIDI::Name::ChannelNameSet::PatchBanks::const_iterator i = cns->patch_banks().begin(); i != cns->patch_banks().end(); ++i) {
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

	boost::shared_ptr<MIDI::Name::ChannelNameSet> cns = _info.get_patches (_channel.get_value_as_int() - 1);

	if (!cns) {
		return;
	}

	for (MIDI::Name::ChannelNameSet::PatchBanks::const_iterator i = cns->patch_banks().begin(); i != cns->patch_banks().end(); ++i) {

		string n = (*i)->name ();
		boost::replace_all (n, "_", " ");

		if ((*i)->number() == get_14bit_bank()) {
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

	boost::shared_ptr<MIDI::Name::ChannelNameSet> cns = _info.get_patches (_channel.get_value_as_int() - 1);

	if (!cns) {
		return;
	}

	for (MIDI::Name::ChannelNameSet::PatchBanks::const_iterator i = cns->patch_banks().begin(); i != cns->patch_banks().end(); ++i) {
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

	if (_current_patch_bank->number() != UINT16_MAX) {
		_ignore_signals = true;
		_bank_msb.set_value (_current_patch_bank->number() >> 7);
		_bank_lsb.set_value (_current_patch_bank->number() & 127);
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

	const MIDI::Name::PatchNameList& patches = _current_patch_bank->patch_name_list ();
	for (MIDI::Name::PatchNameList::const_iterator j = patches.begin(); j != patches.end(); ++j) {
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

	const MIDI::Name::PatchNameList& patches = _current_patch_bank->patch_name_list ();
	for (MIDI::Name::PatchNameList::const_iterator j = patches.begin(); j != patches.end(); ++j) {
		string n = (*j)->name ();
		boost::replace_all (n, "_", " ");

		MIDI::Name::PatchPrimaryKey const & key = (*j)->patch_primary_key ();
		if (key.program() == _program.get_value() - 1) {
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

	const MIDI::Name::PatchNameList& patches = _current_patch_bank->patch_name_list ();

	for (MIDI::Name::PatchNameList::const_iterator j = patches.begin(); j != patches.end(); ++j) {
		string n = (*j)->name ();
		boost::replace_all (n, "_", " ");

		if (n == _patch_combo.get_active_text ()) {
			_ignore_signals = true;
			_program.set_value ((*j)->program_number() + 1);
			_bank_msb.set_value ((*j)->bank_number() >> 7);
			_bank_lsb.set_value ((*j)->bank_number() & 127);
			_ignore_signals = false;
			break;
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

