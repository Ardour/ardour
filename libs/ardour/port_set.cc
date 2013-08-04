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

#include <string>

#include "ardour/audio_port.h"
#include "ardour/midi_port.h"
#include "ardour/port.h"
#include "ardour/port_set.h"

using std::string;

namespace ARDOUR {

PortSet::PortSet()
{
	for (size_t i=0; i < DataType::num_types; ++i)
		_ports.push_back( PortVec() );
}

static bool sort_ports_by_name (boost::shared_ptr<Port> a, boost::shared_ptr<Port> b)
{
	string aname (a->name());
	string bname (b->name());

	string::size_type last_digit_position_a = aname.size();
	string::reverse_iterator r_iterator = aname.rbegin();

	while (r_iterator!= aname.rend() && Glib::Unicode::isdigit(*r_iterator)) {
		r_iterator++;
		last_digit_position_a--;
	}

	string::size_type last_digit_position_b = bname.size();
	r_iterator = bname.rbegin();

	while (r_iterator != bname.rend() && Glib::Unicode::isdigit(*r_iterator)) {
		r_iterator++;
		last_digit_position_b--;
	}

	// if some of the names don't have a number as posfix, compare as strings

	if (last_digit_position_a == aname.size() || last_digit_position_b == bname.size()) {
		return aname < bname;
	}

	const std::string       prefix_a = aname.substr(0, last_digit_position_a - 1);
	const unsigned int      posfix_a = std::atoi(aname.substr(last_digit_position_a, aname.size() - last_digit_position_a).c_str());
	const std::string       prefix_b = bname.substr(0, last_digit_position_b - 1);
	const unsigned int      posfix_b = std::atoi(bname.substr(last_digit_position_b, bname.size() - last_digit_position_b).c_str());

	if (prefix_a != prefix_b) {
		return aname < bname;
	} else {
		return posfix_a < posfix_b;
	}
}


static bool sort_ports_by_type_and_name (boost::shared_ptr<Port> a, boost::shared_ptr<Port> b)
{
	if (a->type() != b->type()) {
		return a->type() < b->type();
	}

	return sort_ports_by_name (a, b);
}

void
PortSet::add (boost::shared_ptr<Port> port)
{
	PortVec& v = _ports[port->type()];

	v.push_back(port);
	_all_ports.push_back(port);

	sort(v.begin(), v.end(), sort_ports_by_name);
	sort(_all_ports.begin(), _all_ports.end(), sort_ports_by_type_and_name);
	
	_count.set(port->type(), _count.get(port->type()) + 1);
	assert(_count.get(port->type()) == _ports[port->type()].size());
}

bool
PortSet::remove (boost::shared_ptr<Port> port)
{
	PortVec::iterator i = find(_all_ports.begin(), _all_ports.end(), port);
	if (i != _all_ports.end()) {
		_all_ports.erase(i);
	}
	
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
	return _all_ports.size();
}

bool
PortSet::contains (boost::shared_ptr<const Port> port) const
{
	return find(_all_ports.begin(), _all_ports.end(), port) != _all_ports.end();
}

boost::shared_ptr<Port>
PortSet::port(size_t n) const
{
	assert(n < _all_ports.size());
	return _all_ports[n];
}

boost::shared_ptr<Port>
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

boost::shared_ptr<AudioPort>
PortSet::nth_audio_port(size_t n) const
{
	return boost::dynamic_pointer_cast<AudioPort> (port (DataType::AUDIO, n));
}

boost::shared_ptr<MidiPort>
PortSet::nth_midi_port(size_t n) const
{
	return boost::dynamic_pointer_cast<MidiPort> (port (DataType::MIDI, n));
}

void
PortSet::clear()
{
	_ports.clear();
	_all_ports.clear();
}

} // namepace ARDOUR
