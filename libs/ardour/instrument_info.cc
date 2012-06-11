/*
    Copyright (C) 2012 Paul Davis

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

#include "pbd/compose.h"

#include "midi++/midnam_patch.h"

#include "ardour/instrument_info.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/processor.h"
#include "ardour/rc_configuration.h"

#include "i18n.h"

using namespace ARDOUR;
using std::string;

InstrumentInfo::InstrumentInfo ()
	: external_instrument_model (_("Unknown"))
{
}

InstrumentInfo::~InstrumentInfo ()
{
}

void
InstrumentInfo::set_external_instrument (const string& model, const string& mode)
{
	external_instrument_model = model;
	external_instrument_mode = mode;
	internal_instrument.reset ();
	Changed(); /* EMIT SIGNAL */
}

void
InstrumentInfo::set_internal_instrument (boost::shared_ptr<Processor> p)
{
	internal_instrument = p;
	external_instrument_model = (_("Unknown"));
	external_instrument_mode = "";
	Changed(); /* EMIT SIGNAL */
}

string
InstrumentInfo::get_instrument_name () const
{
	boost::shared_ptr<Processor> p = internal_instrument.lock();

	if (p) {
		return p->name();
	}

	if (external_instrument_mode.empty()) {
		return external_instrument_model;
	} else {
		return string_compose ("%1 (%2)", external_instrument_model, external_instrument_mode);
	}
}

string
InstrumentInfo::get_patch_name (uint16_t bank, uint8_t program, uint8_t channel) const
{
	boost::shared_ptr<Processor> p = internal_instrument.lock();

	if (p) {
		return "some plugin program";
	}

	MIDI::Name::PatchPrimaryKey patch_key (bank, program);
	
	boost::shared_ptr<MIDI::Name::Patch> patch =
		MIDI::Name::MidiPatchManager::instance().find_patch (external_instrument_model, 
								     external_instrument_mode, channel, patch_key);

	if (patch) {
		return patch->name();
	} else {
		/* program and bank numbers are zero-based: convert to one-based: MIDI_BP_ZERO */

#define MIDI_BP_ZERO ((Config->get_first_midi_bank_is_zero())?0:1)

		return string_compose ("%1 %2",program + MIDI_BP_ZERO , bank + MIDI_BP_ZERO);
	}
}	
