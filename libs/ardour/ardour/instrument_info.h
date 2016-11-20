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

#ifndef __ardour_instrument_info_h__
#define __ardour_instrument_info_h__

#include <string>
#include <stdint.h>

#include <boost/weak_ptr.hpp>

#include "pbd/signals.h"

#include "evoral/Parameter.hpp"

#include "midi++/libmidi_visibility.h"
#include "ardour/libardour_visibility.h"

namespace MIDI {
	namespace Name {
		class ChannelNameSet;
		class Patch;
		typedef std::list<boost::shared_ptr<Patch> > PatchNameList;
	}
}

namespace ARDOUR {

class Processor;

class LIBARDOUR_API InstrumentInfo {
  public:
    InstrumentInfo();
    ~InstrumentInfo ();

    void set_external_instrument (const std::string& model, const std::string& mode);
    void set_internal_instrument (boost::shared_ptr<ARDOUR::Processor>);

    std::string get_patch_name (uint16_t bank, uint8_t program, uint8_t channel) const;
    std::string get_patch_name_without (uint16_t bank, uint8_t program, uint8_t channel) const;
    std::string get_controller_name (Evoral::Parameter param) const;
    std::string get_instrument_name () const;

    boost::shared_ptr<MIDI::Name::ChannelNameSet> get_patches (uint8_t channel);

    PBD::Signal0<void> Changed;

    static const MIDI::Name::PatchNameList& general_midi_patches();

  private:
    std::string external_instrument_model;
    std::string external_instrument_mode;

    boost::weak_ptr<ARDOUR::Processor> internal_instrument;

    boost::shared_ptr<MIDI::Name::ChannelNameSet> plugin_programs_to_channel_name_set (boost::shared_ptr<Processor> p);
    std::string get_plugin_patch_name (boost::shared_ptr<ARDOUR::Processor>, uint16_t bank, uint8_t program, uint8_t channel) const;
    std::string get_plugin_controller_name (boost::shared_ptr<ARDOUR::Processor>, Evoral::Parameter) const;

    std::string get_patch_name (uint16_t bank, uint8_t program, uint8_t channel, bool with_extra) const;
    static MIDI::Name::PatchNameList _gm_patches;
};

} /* namespace ARDOUR */


#endif /* __ardour_instrument_info_h__ */
