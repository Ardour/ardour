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

#include <gtkmm/frame.h>
#include <boost/algorithm/string.hpp>

#include "pbd/unwind.h"
#include "evoral/PatchChange.hpp"
#include "midi++/midnam_patch.h"
#include "ardour/instrument_info.h"
#include "ardour/midi_track.h"

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
	, _info (r->instrument_info ())
{
	assert (boost::dynamic_pointer_cast<MidiTrack> (r));

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

	for (uint8_t pgm = 0; pgm < 128; ++pgm) {
		_program_btn[pgm].set_text_ellipsize (Pango::ELLIPSIZE_END);
		_program_btn[pgm].set_layout_ellipsize_width (PANGO_SCALE * 112 * UIConfiguration::instance ().get_ui_scale ());
		int row = pgm % 16;
		int col = pgm / 16;
		_program_table.attach (_program_btn[pgm], col, col + 1, row, row + 1);
		_program_btn[pgm].signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PatchChangeWidget::select_program), pgm));
	}

	using namespace Menu_Helpers;
	for (uint32_t chn = 0; chn < 16; ++chn) {
		char buf[8];
		snprintf (buf, sizeof(buf), "%d", chn + 1);
		_channel_select.AddMenuElem (MenuElem (buf, sigc::bind (sigc::mem_fun (*this, &PatchChangeWidget::select_channel), chn)));
	}

	_bank_msb_spin.signal_changed().connect (sigc::mem_fun (*this, &PatchChangeWidget::select_bank_spin));
	_bank_lsb_spin.signal_changed().connect (sigc::mem_fun (*this, &PatchChangeWidget::select_bank_spin));

	_info.Changed.connect (_info_changed_connection, invalidator (*this),
			boost::bind (&PatchChangeWidget::instrument_info_changed, this), gui_context());

	set_spacing (4);
	show_all ();
}

PatchChangeWidget::~PatchChangeWidget ()
{
}

void
PatchChangeWidget::on_show ()
{
	Gtk::VBox::on_show ();
	_channel = -1;
	select_channel (0);
}

void
PatchChangeWidget::on_hide ()
{
	Gtk::VBox::on_hide ();
	_ac_connections.drop_connections ();
}


void
PatchChangeWidget::select_channel (uint8_t chn)
{
	assert (_route);

	if (_channel == chn) {
		return;
	}

	_channel_select.set_text (string_compose ("%1", (int)(chn + 1)));
	_channel = chn;

	_ac_connections.drop_connections ();

	boost::shared_ptr<AutomationControl> bank_msb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, chn, MIDI_CTL_MSB_BANK), true);
	boost::shared_ptr<AutomationControl> bank_lsb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, chn, MIDI_CTL_LSB_BANK), true);
	boost::shared_ptr<AutomationControl> program = _route->automation_control(Evoral::Parameter (MidiPgmChangeAutomation, chn), true); 

	bank_msb->Changed.connect (_ac_connections, invalidator (*this),
			boost::bind (&PatchChangeWidget::bank_changed, this), gui_context ());
	bank_lsb->Changed.connect (_ac_connections, invalidator (*this),
			boost::bind (&PatchChangeWidget::bank_changed, this), gui_context ());
	program->Changed.connect (_ac_connections, invalidator (*this),
			boost::bind (&PatchChangeWidget::program_changed, this), gui_context ());

	refill_banks ();
}

void
PatchChangeWidget::refill_banks ()
{
	using namespace Menu_Helpers;

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
			boost::replace_all (n, "_", " ");
			_bank_select.AddMenuElem (MenuElem (n, sigc::bind (sigc::mem_fun (*this, &PatchChangeWidget::select_bank), (*i)->number ())));
			if ((*i)->number () == b) {
				_current_patch_bank = *i;
				_bank_select.set_text (n);
			}
		}
	}

	if (!_current_patch_bank) {
		std::string n = string_compose (_("Bank %1"), b);
		_bank_select.AddMenuElem (MenuElem (n, sigc::bind (sigc::mem_fun (*this, &PatchChangeWidget::select_bank), b)));
		_bank_select.set_text (n);
	}

	refill_program_list ();
}

void
PatchChangeWidget::refill_program_list ()
{
	// TODO if _current_patch_bank!=0, only clear/reset unused patches
	for (uint8_t pgm = 0; pgm < 128; ++pgm) {
		std::string n = string_compose (_("Pgm-%1"), (int)(pgm +1));
		_program_btn[pgm].set_text (n);
		set_tooltip (_program_btn[pgm], n);
	}

	if (_current_patch_bank) {
		const MIDI::Name::PatchNameList& patches = _current_patch_bank->patch_name_list ();
		for (MIDI::Name::PatchNameList::const_iterator i = patches.begin(); i != patches.end(); ++i) {
			std::string n = (*i)->name ();
			boost::replace_all (n, "_", " ");
			MIDI::Name::PatchPrimaryKey const& key = (*i)->patch_primary_key ();

			assert (key.program () < 128);
			assert (key.bank () == bank (_channel));

			const uint8_t pgm = key.program();
			_program_btn[pgm].set_text (n);
			set_tooltip (_program_btn[pgm], string_compose (_("%1 (Pgm-%2)"), n, (int)(pgm +1)));
			}
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
	boost::shared_ptr<AutomationControl> bank_msb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, _channel, MIDI_CTL_MSB_BANK), true);
	boost::shared_ptr<AutomationControl> bank_lsb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, _channel, MIDI_CTL_LSB_BANK), true);

	bank_msb->set_value (bank >> 7, PBD::Controllable::NoGroup);
	bank_lsb->set_value (bank & 127, PBD::Controllable::NoGroup);
}

void
PatchChangeWidget::select_program (uint8_t pgm)
{
	boost::shared_ptr<AutomationControl> program = _route->automation_control(Evoral::Parameter (MidiPgmChangeAutomation, _channel), true);
	program->set_value (pgm, PBD::Controllable::NoGroup);
}

/* ***** callbacks, external changes *****/

void
PatchChangeWidget::bank_changed ()
{
	// TODO optimize, just find the bank, set the text and refill_program_list()
	refill_banks ();
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


/* ***** query info *****/

int
PatchChangeWidget::bank (uint8_t chn) const
{
	boost::shared_ptr<AutomationControl> bank_msb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, chn, MIDI_CTL_MSB_BANK), true);
	boost::shared_ptr<AutomationControl> bank_lsb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, chn, MIDI_CTL_LSB_BANK), true);

	return ((int)bank_msb->get_value () << 7) + (int)bank_lsb->get_value();
}

uint8_t 
PatchChangeWidget::program (uint8_t chn) const
{
	boost::shared_ptr<AutomationControl> program = _route->automation_control(Evoral::Parameter (MidiPgmChangeAutomation, chn), true); 
	return program->get_value();
}

/* ***************************************************************************/

PatchChangeGridDialog::PatchChangeGridDialog (std::string const& title, boost::shared_ptr<ARDOUR::Route> r)
	: ArdourDialog (title, false, false)
	, w (r)
{
	get_vbox()->add (w);
	w.show ();
}
