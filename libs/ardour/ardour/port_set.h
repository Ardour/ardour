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

#ifndef __ardour_port_set_h__
#define __ardour_port_set_h__

#include <vector>
#include <ardour/port.h>
#include <ardour/audio_port.h>
#include <ardour/midi_port.h>
#include <ardour/chan_count.h>

namespace ARDOUR {


/** An ordered list of Ports, possibly of various types.
 *
 * This allows access to all the ports as a list, ignoring type, or accessing
 * the nth port of a given type.  Note that port(n) and nth_audio_port(n) may
 * NOT return the same port.
 */
class PortSet {
public:
	PortSet();

	size_t num_ports() const;
	size_t num_ports(DataType type) const { return _ports[type.to_index()].size(); }

	void add_port(Port* port);

	Port* port(size_t index) const;

	Port* nth_port_of_type(DataType type, size_t n) const;

	AudioPort* nth_audio_port(size_t n) const;
	
	MidiPort* nth_midi_port(size_t n) const;

	bool contains(const Port* port) const;
	
	/** Remove all ports from the PortSet.  Ports are not deregistered with
	 * the engine, it's the caller's responsibility to not leak here!
	 */
	void clear() { _ports.clear(); }

	const ChanCount& chan_count() const { return _chan_count; }

	bool empty() const { return (_chan_count.get_total_count() == 0); }

	// ITERATORS
	
	// obviously these iterators will need to get more clever
	// experimental phase, it's the interface that counts right now
	
	class iterator {
	public:

		Port* operator*()  { return _list.port(_index); }
		iterator& operator++() { ++_index; return *this; } // yes, prefix only
		bool operator==(const iterator& other) { return (_index == other._index); }
		bool operator!=(const iterator& other) { return (_index != other._index); }

	private:
		friend class PortSet;

		iterator(PortSet& list, size_t index) : _list(list), _index(index) {}

		PortSet& _list;
		size_t    _index;
	};

	iterator begin() { return iterator(*this, 0); }
	iterator end()   { return iterator(*this, _chan_count.get_total_count()); }
	
	class const_iterator {
	public:

		const Port* operator*()  { return _list.port(_index); }
		const_iterator& operator++() { ++_index; return *this; } // yes, prefix only
		bool operator==(const const_iterator& other) { return (_index == other._index); }
		bool operator!=(const const_iterator& other) { return (_index != other._index); }

	private:
		friend class PortSet;

		const_iterator(const PortSet& list, size_t index) : _list(list), _index(index) {}

		const PortSet& _list;
		size_t          _index;
	};

	const_iterator begin() const { return const_iterator(*this, 0); }
	const_iterator end()   const { return const_iterator(*this, _chan_count.get_total_count()); }

	

	class audio_iterator {
	public:

		AudioPort* operator*()  { return _list.nth_audio_port(_index); }
		audio_iterator& operator++() { ++_index; return *this; } // yes, prefix only
		bool operator==(const audio_iterator& other) { return (_index == other._index); }
		bool operator!=(const audio_iterator& other) { return (_index != other._index); }

	private:
		friend class PortSet;

		audio_iterator(PortSet& list, size_t index) : _list(list), _index(index) {}

		PortSet& _list;
		size_t    _index;
	};

	audio_iterator audio_begin() { return audio_iterator(*this, 0); }
	audio_iterator audio_end()   { return audio_iterator(*this, _chan_count.get_count(DataType::AUDIO)); }




private:	
	// Prevent copies (undefined)
	PortSet(const PortSet& copy);
	void operator=(const PortSet& other);

	typedef std::vector<Port*> PortVec;
	
	// Vector of vectors, indexed by DataType::to_index()
	std::vector<PortVec> _ports;

	ChanCount _chan_count;
};


} // namespace ARDOUR

#endif // __ardour_port_set_h__
