/*
    Copyright (C) 2006 Paul Davis 
    
    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
    
    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.
    
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "ardour/port_set.h"
#include "ardour/midi_port.h"
#include "ardour/audio_port.h"

namespace ARDOUR {

PortSet::PortSet()
{
	for (size_t i=0; i < DataType::num_types; ++i)
		_ports.push_back( PortVec() );
}

static bool sort_ports_by_name (Port* a, Port* b)
{
	return (a->name() < b->name());
}

void
PortSet::add(Port* port)
{
	PortVec& v = _ports[port->type()];
	
	v.push_back(port);
	sort(v.begin(), v.end(), sort_ports_by_name);

	_count.set(port->type(), _count.get(port->type()) + 1);

	assert(_count.get(port->type()) == _ports[port->type()].size());
}

bool
PortSet::remove(Port* port)
{
	for (std::vector<PortVec>::iterator l = _ports.begin(); l != _ports.end(); ++l) {
		PortVec::iterator i = find(l->begin(), l->end(), port);
		if (i != l->end()) {
			l->erase(i);
			_count.set(port->type(), _count.get(port->type()) - 1);
			return true;
		}
	}

	return false;
}

/** Get the total number of ports (of all types) in the PortSet
 */
size_t
PortSet::num_ports() const
{
	size_t ret = 0;

	for (std::vector<PortVec>::const_iterator l = _ports.begin(); l != _ports.end(); ++l)
		ret += (*l).size();

	return ret;
}

bool
PortSet::contains(const Port* port) const
{
	for (std::vector<PortVec>::const_iterator l = _ports.begin(); l != _ports.end(); ++l)
		if (find((*l).begin(), (*l).end(), port) != (*l).end())
			return true;

	return false;
}

Port*
PortSet::port(size_t n) const
{
	// This is awesome.  Awesomely slow.
	
	size_t size_so_far = 0;

	for (std::vector<PortVec>::const_iterator l = _ports.begin(); l != _ports.end(); ++l) {
		if (n < size_so_far + (*l).size())
			return (*l)[n - size_so_far];
		else
			size_so_far += (*l).size();
	}

	return NULL; // n out of range
}

Port*
PortSet::port(DataType type, size_t n) const
{
	if (type == DataType::NIL) {
		return port(n);
	} else {
		const PortVec& v = _ports[type];
		assert(n < v.size());
		return v[n];
	}
}

AudioPort*
PortSet::nth_audio_port(size_t n) const
{
	return dynamic_cast<AudioPort*>(port(DataType::AUDIO, n));
}

MidiPort*
PortSet::nth_midi_port(size_t n) const
{
	return dynamic_cast<MidiPort*>(port(DataType::MIDI, n));
}

} // namepace ARDOUR
