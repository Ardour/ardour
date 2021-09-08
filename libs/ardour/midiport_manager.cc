/*
 * Copyright (C) 1998-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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
	/* this method is idempotent */

	if (_mmc_in) {
		return;
	}

	_mmc_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, X_("MMC in"), true);
	_mmc_out = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("MMC out"), true);

	_scene_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, X_("Scene in"), true);
	_scene_out = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("Scene out"), true);

	_vkbd_out = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("x-virtual-keyboard"), true, IsTerminal);
	boost::dynamic_pointer_cast<AsyncMIDIPort>(_vkbd_out)->set_flush_at_cycle_start (true);

	/* Now register ports used to send positional sync data (MTC and MIDI Clock) */

	boost::shared_ptr<ARDOUR::Port> p;

	p = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("MTC out"));
	_mtc_output_port= boost::dynamic_pointer_cast<MidiPort> (p);

	p = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("MIDI Clock out"), false, TransportGenerator);
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
	ports.insert (make_pair (_mmc_in->name(), _mmc_in));
	ports.insert (make_pair (_mmc_out->name(), _mmc_out));
	ports.insert (make_pair (_vkbd_out->name(), _vkbd_out));
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
	ports.insert (make_pair (_mmc_in->name(), _mmc_in));
	ports.insert (make_pair (_mmc_out->name(), _mmc_out));
	ports.insert (make_pair (_vkbd_out->name(), _vkbd_out));
	ports.insert (make_pair (_scene_out->name(), _scene_out));
	ports.insert (make_pair (_scene_in->name(), _scene_in));

	for (PortMap::const_iterator p = ports.begin(); p != ports.end(); ++p) {
		s.push_back (&p->second->get_state());
	}

	return s;
}

boost::shared_ptr<AsyncMIDIPort>
MidiPortManager::vkbd_output_port () const
{
	return boost::dynamic_pointer_cast<AsyncMIDIPort> (_vkbd_out);
}

void
MidiPortManager::set_public_latency (bool playback)
{
	typedef std::list<boost::shared_ptr<Port> > PortList;
	PortList pl;

	pl.push_back (_mtc_output_port);
	pl.push_back (_midi_clock_output_port);
	pl.push_back (_mmc_in);
	pl.push_back (_mmc_out);
	pl.push_back (_vkbd_out);
	pl.push_back (_scene_out);
	pl.push_back (_scene_in);

	for (PortList::const_iterator p = pl.begin(); p != pl.end(); ++p) {
		LatencyRange range;
		(*p)->get_connected_latency_range (range, playback);
		/* Ports always align to worst-case latency */
		range.min = range.max;
		(*p)->set_private_latency_range (range, playback);
		(*p)->set_public_latency_range (range, playback);
	}
}
