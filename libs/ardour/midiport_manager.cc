/*
    Copyright (C) 1998-99 Paul Barton-Davis 
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

    $Id$
*/

#include "ardour/audioengine.h"
#include "ardour/async_midi_port.h"
#include "ardour/midiport_manager.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace MIDI;
using namespace PBD;


MidiPortManager::MidiPortManager ()
{
}

MidiPortManager::~MidiPortManager ()
{
	if (_midi_in) {
		AudioEngine::instance()->unregister_port (_midi_in);
	}
	if (_midi_in) {
		AudioEngine::instance()->unregister_port (_midi_in);
	}
	if (_mtc_input_port) {
		AudioEngine::instance()->unregister_port (_mtc_input_port);
	}
	if (_mtc_output_port) {
		AudioEngine::instance()->unregister_port (_mtc_output_port);
	}
	if (_midi_clock_input_port) {
		AudioEngine::instance()->unregister_port (_midi_clock_input_port);
	}
	if (_midi_clock_output_port) {
		AudioEngine::instance()->unregister_port (_midi_clock_output_port);
	}

}

MidiPort*
MidiPortManager::port (string const & n)
{
	boost::shared_ptr<MidiPort> mp =  boost::dynamic_pointer_cast<MidiPort> (AudioEngine::instance()->get_port_by_name (n));
	return mp.get();
}

void
MidiPortManager::create_ports ()
{
	/* this method is idempotent
	 */

	if (_midi_in) {
		return;
	}
	      
	_midi_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, _("MIDI control in"), true);
	_midi_out = AudioEngine::instance()->register_output_port (DataType::MIDI, _("MIDI control out"), true);

	_mmc_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, _("MMC in"), true);
	_mmc_out = AudioEngine::instance()->register_output_port (DataType::MIDI, _("MMC out"), true);
	
	/* XXX nasty type conversion needed because of the mixed inheritance
	 * required to integrate MIDI::IPMidiPort and ARDOUR::AsyncMIDIPort.
	 *
	 * At some point, we'll move IPMidiPort into Ardour and make it 
	 * inherit from ARDOUR::MidiPort not MIDI::Port, and then this
	 * mess can go away 
	 */

	_midi_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_midi_in).get();
	_midi_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_midi_out).get();

	_mmc_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_mmc_in).get();
	_mmc_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_mmc_out).get();

	/* Now register ports used for sync (MTC and MIDI Clock)
	 */

	boost::shared_ptr<ARDOUR::Port> p;

	p = AudioEngine::instance()->register_input_port (DataType::MIDI, _("MTC in"));
	_mtc_input_port = boost::dynamic_pointer_cast<MidiPort> (p);
	p = AudioEngine::instance()->register_output_port (DataType::MIDI, _("MTC out"));
	_mtc_output_port= boost::dynamic_pointer_cast<MidiPort> (p);

	p = AudioEngine::instance()->register_input_port (DataType::MIDI, _("MIDI Clock in"));
	_midi_clock_input_port = boost::dynamic_pointer_cast<MidiPort> (p);
	p = AudioEngine::instance()->register_output_port (DataType::MIDI, _("MIDI Clock out"));
	_midi_clock_output_port= boost::dynamic_pointer_cast<MidiPort> (p);
}

void
MidiPortManager::set_port_states (list<XMLNode*> s)
{
	PortManager::PortList pl;

	AudioEngine::instance()->get_ports (DataType::MIDI, pl);
	
	for (list<XMLNode*>::iterator i = s.begin(); i != s.end(); ++i) {
		for (PortManager::PortList::const_iterator j = pl.begin(); j != pl.end(); ++j) {
			// (*j)->set_state (**i);
		}
	}
}

