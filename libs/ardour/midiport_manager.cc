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
#include "ardour/rc_configuration.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace MIDI;
using namespace PBD;


MidiPortManager::MidiPortManager ()
{
	create_ports ();
}

MidiPortManager::~MidiPortManager ()
{
	Glib::Threads::Mutex::Lock em (AudioEngine::instance()->process_lock());
	if (_midi_in) {
		AudioEngine::instance()->unregister_port (_midi_in);
	}
	if (_midi_out) {
		AudioEngine::instance()->unregister_port (_midi_out);
	}
	if (_scene_in) {
		AudioEngine::instance()->unregister_port (_scene_in);
	}
	if (_scene_out) {
		AudioEngine::instance()->unregister_port (_scene_out);
	}
	if (_mtc_output_port) {
		AudioEngine::instance()->unregister_port (_mtc_output_port);
	}
	if (_midi_clock_output_port) {
		AudioEngine::instance()->unregister_port (_midi_clock_output_port);
	}

}

void
MidiPortManager::create_ports ()
{
	/* this method is idempotent
	 */

	if (_midi_in) {
		return;
	}

	_midi_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, X_("MIDI control in"), true);
	_midi_out = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("MIDI control out"), true);

	_mmc_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, X_("MMC in"), true);
	_mmc_out = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("MMC out"), true);

	_scene_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, X_("Scene in"), true);
	_scene_out = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("Scene out"), true);

	/* Now register ports used to send positional sync data (MTC and MIDI Clock)
	 */

	boost::shared_ptr<ARDOUR::Port> p;

	p = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("MTC out"));
	_mtc_output_port= boost::dynamic_pointer_cast<MidiPort> (p);

	p = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("MIDI Clock out"));
	_midi_clock_output_port= boost::dynamic_pointer_cast<MidiPort> (p);
}

void
MidiPortManager::set_midi_port_states (const XMLNodeList&nodes)
{
	XMLProperty const * prop;
	typedef map<std::string,boost::shared_ptr<Port> > PortMap;
	PortMap ports;
	const int version = 0;

	ports.insert (make_pair (_mtc_output_port->name(), _mtc_output_port));
	ports.insert (make_pair (_midi_clock_output_port->name(), _midi_clock_output_port));
	ports.insert (make_pair (_midi_in->name(), _midi_in));
	ports.insert (make_pair (_midi_out->name(), _midi_out));
	ports.insert (make_pair (_mmc_in->name(), _mmc_in));
	ports.insert (make_pair (_mmc_out->name(), _mmc_out));
	ports.insert (make_pair (_scene_out->name(), _scene_out));
	ports.insert (make_pair (_scene_in->name(), _scene_in));

	for (XMLNodeList::const_iterator n = nodes.begin(); n != nodes.end(); ++n) {
		if ((prop = (*n)->property (X_("name"))) == 0) {
			continue;
		}

		PortMap::iterator p = ports.find (prop->value());
		if (p == ports.end()) {
			continue;
		}

		p->second->set_state (**n, version);
	}
}

list<XMLNode*>
MidiPortManager::get_midi_port_states () const
{
	typedef map<std::string,boost::shared_ptr<Port> > PortMap;
	PortMap ports;
	list<XMLNode*> s;

	ports.insert (make_pair (_mtc_output_port->name(), _mtc_output_port));
	ports.insert (make_pair (_midi_clock_output_port->name(), _midi_clock_output_port));
	ports.insert (make_pair (_midi_in->name(), _midi_in));
	ports.insert (make_pair (_midi_out->name(), _midi_out));
	ports.insert (make_pair (_mmc_in->name(), _mmc_in));
	ports.insert (make_pair (_mmc_out->name(), _mmc_out));
	ports.insert (make_pair (_scene_out->name(), _scene_out));
	ports.insert (make_pair (_scene_in->name(), _scene_in));

	for (PortMap::const_iterator p = ports.begin(); p != ports.end(); ++p) {
		s.push_back (&p->second->get_state());
	}

	return s;
}


