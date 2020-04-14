/*
 * Copyright (C) 2012-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include <algorithm>

#include "pbd/compose.h"

#include "midi++/midnam_patch.h"

#include "ardour/instrument_info.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/processor.h"
#include "ardour/rc_configuration.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace MIDI::Name;
using std::string;

InstrumentInfo::InstrumentInfo ()
	: _external_instrument_model (_("Unknown"))
{
}

InstrumentInfo::~InstrumentInfo ()
{
}

void
InstrumentInfo::set_external_instrument (const string& model, const string& mode)
{
	invalidate_cached_plugin_model ();
	if (_external_instrument_model == model && _external_instrument_mode == mode) {
		//std::cerr << "InstrumentInfo::set_external_instrument '" << model << "' '" << mode << "' -- no change\n";
		return;
	}
	//std::cerr << "InstrumentInfo::set_external_instrument '" << model << "' '" << mode << "'\n";
	_external_instrument_model = model;
	_external_instrument_mode  = mode;
	Changed (); /* EMIT SIGNAL */
}

void
InstrumentInfo::set_internal_instrument (boost::shared_ptr<Processor> p)
{
	invalidate_cached_plugin_model ();
	if (p == internal_instrument.lock ()) {
		//std::cerr << "InstrumentInfo::set_internal_instrument -- NO CHANGE\n";
		return;
	}
	_midnam_changed.disconnect ();
	//std::cerr << "InstrumentInfo::set_internal_instrument -> '" << (p ? p->name () : "(NULL)") << "'\n";
	internal_instrument = p;
	if (_external_instrument_model.empty () || _external_instrument_model == _("Unknown")) {
		Changed (); /* EMIT SIGNAL */
	}

	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (p);
	if (pi && pi->plugin ()->has_midnam ()) {
		pi->plugin()->UpdatedMidnam.connect_same_thread (_midnam_changed, boost::bind (&InstrumentInfo::emit_changed, this));
	}
}

void
InstrumentInfo::emit_changed () {
	if (_external_instrument_model.empty ()) {
		Changed ();
	}
}

bool
InstrumentInfo::have_custom_plugin_info () const
{
	boost::shared_ptr<Processor> p = internal_instrument.lock ();

	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (p);
	if (pi && pi->plugin ()->has_midnam ()) {
		std::string                  model        = pi->plugin ()->midnam_model ();
		const std::list<std::string> device_modes = MidiPatchManager::instance ().custom_device_mode_names_by_model (model);
		if (device_modes.size () > 0) {
			return true;
		}
	}
	return false;
}

std::string
InstrumentInfo::model () const
{
	if (!_external_instrument_model.empty ()) {
		return _external_instrument_model;
	}
	if (!_plugin_model.empty ()) {
		return _plugin_model;
	}
	boost::shared_ptr<Processor>    p  = internal_instrument.lock ();
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (p);
	if (pi && pi->plugin ()->has_midnam ()) {
		_plugin_model = pi->plugin ()->midnam_model ();
		return _plugin_model;
	}
	return "";
}

std::string
InstrumentInfo::mode () const
{
	if (!_external_instrument_model.empty ()) {
		return _external_instrument_mode;
	}
	if (!_plugin_mode.empty ()) {
		return _plugin_mode;
	}
	boost::shared_ptr<Processor>    p  = internal_instrument.lock ();
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (p);
	if (pi && pi->plugin ()->has_midnam ()) {
		const std::list<std::string> device_modes = MidiPatchManager::instance ().custom_device_mode_names_by_model (model ());
		if (device_modes.size () > 0) {
			_plugin_mode = device_modes.front ();
			return _plugin_mode;
		}
	}
	return "";
}

string
InstrumentInfo::get_note_name (uint16_t bank, uint8_t program, uint8_t channel, uint8_t note) const
{
	boost::shared_ptr<MasterDeviceNames> const& dev_names (MidiPatchManager::instance ().master_device_by_model (model ()));
	if (dev_names) {
		return dev_names->note_name (mode (), channel, bank, program, note);
	}
	return "";
}

boost::shared_ptr<const ValueNameList>
InstrumentInfo::value_name_list_by_control (uint8_t channel, uint8_t number) const
{
	boost::shared_ptr<MasterDeviceNames> const& dev_names (MidiPatchManager::instance ().master_device_by_model (model ()));
	if (dev_names) {
		return dev_names->value_name_list_by_control (mode (), channel, number);
	}
	return boost::shared_ptr<const ValueNameList> ();
}

boost::shared_ptr<MasterDeviceNames>
InstrumentInfo::master_device_names () const
{
#if 1
	/* this safe if model does not exist */
	boost::shared_ptr<MIDINameDocument> midnam = MidiPatchManager::instance ().document_by_model (model ());
	if (midnam) {
		return midnam->master_device_names (model ());
	}
	return boost::shared_ptr<MasterDeviceNames> ();
#else
	return MidiPatchManager::instance ().master_device_by_model (model ());
#endif
}

/* reverse lookup which "ChannelNameSet" use "UsesControlNameList <name>",
 * then add all channels that the ChannelNameSet is AvailableForChannels.
 */
uint16_t
InstrumentInfo::channels_for_control_list (std::string const& ctrl_name_list) const
{
	boost::shared_ptr<MasterDeviceNames> const& dev_names (MidiPatchManager::instance ().master_device_by_model (model ()));
	uint16_t channels = 0;
	for (int c = 0; c < 16; ++c) {
		boost::shared_ptr<ChannelNameSet> const& chan_names (dev_names->channel_name_set_by_channel (mode (), c));
		if (!chan_names || !chan_names->available_for_channel (c + 1)) {
			continue;
		}
		if (chan_names->control_list_name () == ctrl_name_list) {
			channels |= 0x0001 << c;
		}
	}
	if (channels == 0) {
		channels = 65535;
	}
	return channels;
}

boost::shared_ptr<ControlNameList>
InstrumentInfo::control_name_list (uint8_t channel)
{
	boost::shared_ptr<MasterDeviceNames> const& dev_names (MidiPatchManager::instance ().master_device_by_model (model ()));
	boost::shared_ptr<ChannelNameSet> const&    chan_names (dev_names->channel_name_set_by_channel (mode (), channel));
	if (!chan_names) {
		return boost::shared_ptr<ControlNameList> ();
	}
	return dev_names->control_name_list (chan_names->control_list_name ());
}

size_t
InstrumentInfo::master_controller_count () const
{
	boost::shared_ptr<MasterDeviceNames> const& dev_names (MidiPatchManager::instance ().master_device_by_model (model ()));
	if (!dev_names) {
		return 0;
	}
	MasterDeviceNames::ControlNameLists const& ctllist (dev_names->controls());

	size_t total_ctrls = 0;
	for (MasterDeviceNames::ControlNameLists::const_iterator l = ctllist.begin(); l != ctllist.end(); ++l) {
		boost::shared_ptr<ControlNameList> const& name_list = l->second;
		total_ctrls += name_list->controls().size();
	}
	return total_ctrls;
}

#if 0
MasterDeviceNames::ControlNameLists const&
InstrumentInfo::master_control_names () const
{
	static MasterDeviceNames::ControlNameLists empty_list;

	boost::shared_ptr<MasterDeviceNames> const& dev_names (MidiPatchManager::instance ().master_device_by_model (model ()));
	if (dev_names) {
		return dev_names->controls();
	}
	return empty_list;
}
#endif

string
InstrumentInfo::get_patch_name (uint16_t bank, uint8_t program, uint8_t channel) const
{
	return get_patch_name (bank, program, channel, true);
}

string
InstrumentInfo::get_patch_name_without (uint16_t bank, uint8_t program, uint8_t channel) const
{
	return get_patch_name (bank, program, channel, false);
}

string
InstrumentInfo::get_patch_name (uint16_t bank, uint8_t program, uint8_t channel, bool with_extra) const
{
	PatchPrimaryKey patch_key (program, bank);

	boost::shared_ptr<MIDI::Name::Patch> const& patch (MidiPatchManager::instance ().find_patch (model (), mode (), channel, patch_key));

	if (patch) {
		return patch->name ();
	} else {
		/* program and bank numbers are zero-based: convert to one-based: MIDI_BP_ZERO */

#define MIDI_BP_ZERO ((Config->get_first_midi_bank_is_zero ()) ? 0 : 1)

		if (with_extra) {
			return string_compose ("prg %1 bnk %2", program + MIDI_BP_ZERO, bank + MIDI_BP_ZERO);
		} else {
			return string_compose ("%1", program + MIDI_BP_ZERO);
		}
	}
}

string
InstrumentInfo::get_controller_name (Evoral::Parameter param) const
{
	if (param.type () != MidiCCAutomation) {
		return "";
	}

	boost::shared_ptr<MasterDeviceNames> const& dev_names (MidiPatchManager::instance ().master_device_by_model (model ()));
	if (!dev_names) {
		return "";
	}

	boost::shared_ptr<ChannelNameSet> const& chan_names (dev_names->channel_name_set_by_channel (mode (), param.channel ()));
	if (!chan_names) {
		return "";
	}

	boost::shared_ptr<ControlNameList> const& control_names (dev_names->control_name_list (chan_names->control_list_name ()));
	if (!control_names) {
		return "";
	}
	boost::shared_ptr<const Control> const& c = control_names->control (param.id ());

	if (c) {
		return string_compose (c->name () + " [%1]", int(param.channel ()) + 1);
	}

	return "";
}

boost::shared_ptr<ChannelNameSet>
InstrumentInfo::get_patches (uint8_t channel)
{
	return MidiPatchManager::instance ().find_channel_name_set (model (), mode (), channel);
}
