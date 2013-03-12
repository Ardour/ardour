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

#include <algorithm>

#include "pbd/compose.h"

#include "midi++/midnam_patch.h"

#include "ardour/instrument_info.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/processor.h"
#include "ardour/plugin.h"
#include "ardour/rc_configuration.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace MIDI::Name;
using std::string;

MIDI::Name::PatchNameList InstrumentInfo::_gm_patches;

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
		return get_plugin_patch_name (p, bank, program, channel);
	}

	MIDI::Name::PatchPrimaryKey patch_key (program, bank);
	
	boost::shared_ptr<MIDI::Name::Patch> patch =
		MIDI::Name::MidiPatchManager::instance().find_patch (external_instrument_model, 
								     external_instrument_mode, channel, patch_key);

	if (patch) {
		return patch->name();
	} else {
		/* program and bank numbers are zero-based: convert to one-based: MIDI_BP_ZERO */

#define MIDI_BP_ZERO ((Config->get_first_midi_bank_is_zero())?0:1)

		return string_compose ("prg %1 bnk %2",program + MIDI_BP_ZERO , bank + MIDI_BP_ZERO);
	}
}	

string
InstrumentInfo::get_controller_name (Evoral::Parameter param) const
{
	boost::shared_ptr<Processor> p = internal_instrument.lock();
	if (p || param.type() != MidiCCAutomation) {
		return "";
	}

	boost::shared_ptr<MIDI::Name::MasterDeviceNames> dev_names(
		MIDI::Name::MidiPatchManager::instance().master_device_by_model(
			external_instrument_model));
	if (!dev_names) {
		return "";
	}
	
	boost::shared_ptr<ChannelNameSet> chan_names(
		dev_names->channel_name_set_by_device_mode_and_channel(
			external_instrument_mode, param.channel()));
	if (!chan_names) {
		return "";
	}

	boost::shared_ptr<ControlNameList> control_names(
		dev_names->control_name_list(chan_names->control_list_name()));
	if (!control_names) {
		return "";
	}

	return control_names->control(param.id())->name();
}	

boost::shared_ptr<MIDI::Name::ChannelNameSet>
InstrumentInfo::get_patches (uint8_t channel)
{
	boost::shared_ptr<Processor> p = internal_instrument.lock();
	if (p) {
		return plugin_programs_to_channel_name_set (p);
	}

	boost::shared_ptr<MIDI::Name::ChannelNameSet> channel_name_set =
		MidiPatchManager::instance().find_channel_name_set (external_instrument_model,
														    external_instrument_mode,
														    channel);

	//std::cerr << "got channel name set with name '" << channel_name_set->name() << std::endl;

	return channel_name_set;
}

boost::shared_ptr<MIDI::Name::ChannelNameSet>
InstrumentInfo::plugin_programs_to_channel_name_set (boost::shared_ptr<Processor> p)
{
	PatchNameList patch_list;

	boost::shared_ptr<PluginInsert> insert = boost::dynamic_pointer_cast<PluginInsert> (p);
	if (!insert) {
		return boost::shared_ptr<ChannelNameSet>();
	}

	boost::shared_ptr<Plugin> pp = insert->plugin();
	
	if (pp->current_preset_uses_general_midi()) {

		patch_list = InstrumentInfo::general_midi_patches ();

	} else if (pp->presets_are_MIDI_programs()) {

		std::vector<Plugin::PresetRecord> presets = pp->get_presets ();
		std::vector<Plugin::PresetRecord>::iterator i;
		int n;
		
		/* XXX note the assumption that plugin presets start their numbering at
		 * zero
		 */
		
		for (n = 0, i = presets.begin(); i != presets.end(); ++i, ++n) {
			if ((*i).number >= 0) {
				patch_list.push_back (boost::shared_ptr<Patch> (new Patch ((*i).label, n)));
			} else {
				patch_list.push_back (boost::shared_ptr<Patch> (new Patch (string_compose ("program %1", n), n)));
			}
		}
	} else {
		for (int n = 0; n < 127; ++n) {
			patch_list.push_back (boost::shared_ptr<Patch> (new Patch (string_compose ("program %1", n), n)));
		}
	}

	boost::shared_ptr<PatchBank> pb (new PatchBank (0, p->name()));
	pb->set_patch_name_list (patch_list);

	ChannelNameSet::PatchBanks patch_banks;
	patch_banks.push_back (pb);

	boost::shared_ptr<MIDI::Name::ChannelNameSet> cns (new ChannelNameSet);
	cns->set_patch_banks (patch_banks);

	return cns;
}	

const MIDI::Name::PatchNameList&
InstrumentInfo::general_midi_patches()
{
	if (_gm_patches.empty()) {
		for (int n = 0; n < 128; n++) {
			_gm_patches.push_back (boost::shared_ptr<Patch> (new Patch (general_midi_program_names[n], n))); 
		}
	}

	return _gm_patches;
}

string
InstrumentInfo::get_plugin_patch_name (boost::shared_ptr<Processor> p, uint16_t bank, uint8_t program, uint8_t /*channel*/) const
{
	boost::shared_ptr<PluginInsert> insert = boost::dynamic_pointer_cast<PluginInsert> (p);
	if (insert) {
		boost::shared_ptr<Plugin> pp = insert->plugin();
		
		if (pp->current_preset_uses_general_midi()) {
			return MIDI::Name::general_midi_program_names[std::min((uint8_t) 127,program)];
		}
	}

	return string_compose (_("preset %1 (bank %2)"), (int) program, (int) bank);
}
