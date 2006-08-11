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

#include <ardour/port_set.h>

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
PortSet::add_port(Port* port)
{
	const size_t list_index = port->type().to_index();
	assert(list_index < _ports.size());
	
	PortVec& v = _ports[list_index];
	
	v.push_back(port);
	sort(v.begin(), v.end(), sort_ports_by_name);

	_chan_count.set_count(port->type(), _chan_count.get_count(port->type()) + 1);

	assert(_chan_count.get_count(port->type()) == _ports[port->type().to_index()].size());
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
	// This is awesome
	
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
PortSet::nth_port_of_type(DataType type, size_t n) const
{
	const PortVec& v = _ports[type.to_index()];
	assert(n < v.size());
	return v[n];
}

AudioPort*
PortSet::nth_audio_port(size_t n) const
{
	return dynamic_cast<AudioPort*>(nth_port_of_type(DataType::AUDIO, n));
}

MidiPort*
PortSet::nth_midi_port(size_t n) const
{
	return dynamic_cast<MidiPort*>(nth_port_of_type(DataType::MIDI, n));
}

} // namepace ARDOUR
