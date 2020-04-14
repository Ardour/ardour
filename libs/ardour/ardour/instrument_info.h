/*
 * Copyright (C) 2012-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_instrument_info_h__
#define __ardour_instrument_info_h__

#include <stdint.h>
#include <string>

#include <boost/weak_ptr.hpp>

#include "pbd/signals.h"

#include "evoral/Parameter.h"

#include "ardour/libardour_visibility.h"
#include "midi++/libmidi_visibility.h"

namespace MIDI {
	namespace Name {
		class ChannelNameSet;
		class Patch;
		class ValueNameList;
		class MasterDeviceNames;
		class ControlNameList;
		typedef std::list<boost::shared_ptr<Patch> > PatchNameList;
	}
}

namespace ARDOUR {

class Processor;

class LIBARDOUR_API InstrumentInfo
{
public:
	InstrumentInfo ();
	~InstrumentInfo ();

	std::string model () const;
	std::string mode () const;

	void set_external_instrument (const std::string& model, const std::string& mode);
	void set_internal_instrument (boost::shared_ptr<ARDOUR::Processor>);

	std::string get_note_name (uint16_t bank, uint8_t program, uint8_t channel, uint8_t note) const;

	std::string get_patch_name (uint16_t bank, uint8_t program, uint8_t channel) const;
	std::string get_patch_name_without (uint16_t bank, uint8_t program, uint8_t channel) const;
	std::string get_controller_name (Evoral::Parameter param) const;

	boost::shared_ptr<MIDI::Name::MasterDeviceNames> master_device_names () const;

	boost::shared_ptr<MIDI::Name::ChannelNameSet>  get_patches (uint8_t channel);
	boost::shared_ptr<MIDI::Name::ControlNameList> control_name_list (uint8_t channel);

	boost::shared_ptr<const MIDI::Name::ValueNameList> value_name_list_by_control (uint8_t channel, uint8_t number) const;

	size_t master_controller_count () const;
	uint16_t channels_for_control_list (std::string const& ctrl_name_list) const;

	PBD::Signal0<void> Changed;

	bool have_custom_plugin_info () const;

private:
	std::string get_patch_name (uint16_t bank, uint8_t program, uint8_t channel, bool with_extra) const;

	void invalidate_cached_plugin_model ()
	{
		_plugin_model = "";
		_plugin_mode = "";
	}

	void emit_changed ();

	std::string _external_instrument_model;
	std::string _external_instrument_mode;

	mutable std::string _plugin_model;
	mutable std::string _plugin_mode;

	boost::weak_ptr<ARDOUR::Processor> internal_instrument;

	PBD::ScopedConnection _midnam_changed;
};

}

#endif /* __ardour_instrument_info_h__ */
