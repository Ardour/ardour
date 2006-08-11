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

#ifndef __ardour_data_type_h__
#define __ardour_data_type_h__

#include <string>
#include <ardour/data_type.h>
#include <jack/jack.h>

namespace ARDOUR {


/** A type of Data Ardour is capable of processing.
 *
 * The majority of this class is dedicated to conversion to and from various
 * other type representations, simple comparison between then, etc.  This code
 * is deliberately 'ugly' so other code doesn't have to be.
 */
class DataType
{
public:
	/// WARNING: make REALLY sure you don't mess up indexes if you change this
	enum Symbol {
		NIL = 0,
		AUDIO,
		MIDI
	};
	
	/** Number of types (not including NIL).
	 * WARNING: make sure this matches Symbol!
	 */
	static const size_t num_types = 2;


	/** Helper for collections that store typed things by index (BufferSet, PortList).
	 * Guaranteed to be a valid index from 0 to (the number of available types - 1),
	 * because NIL is not included.  No, this isn't pretty - purely for speed.
	 * See DataType::to_index().
	 */
	inline static size_t symbol_index(const Symbol symbol)
		{ return (size_t)symbol - 1; }

	DataType(const Symbol& symbol)
	: _symbol(symbol)
	{}

	/** Construct from a string (Used for loading from XML and Ports)
	 * The string can be as in an XML file (eg "audio" or "midi"), or a
	 * Jack type string (from jack_port_type) */
	DataType(const std::string& str) {
		if (str == "audio")
			_symbol = AUDIO;
		else if (str == JACK_DEFAULT_AUDIO_TYPE)
			_symbol = AUDIO;
		else if (str == "midi")
			_symbol = MIDI;
		else if (str == JACK_DEFAULT_MIDI_TYPE)
			_symbol = MIDI;
		else
			_symbol = NIL;
	}

	/** Get the Jack type this DataType corresponds to */
	const char* to_jack_type() const {
		switch (_symbol) {
			case AUDIO: return JACK_DEFAULT_AUDIO_TYPE;
			case MIDI:  return JACK_DEFAULT_MIDI_TYPE;
			default:    return "";
		}
	}
	
	/** Inverse of the from-string constructor */
	const char* to_string() const {
		switch (_symbol) {
			case AUDIO: return "audio";
			case MIDI:  return "midi";
			default:    return "unknown"; // reeeally shouldn't ever happen
		}
	}

	Symbol        to_symbol() const { return _symbol; }
	inline size_t to_index() const  { return symbol_index(_symbol); }

	/** DataType iterator, for writing generic loops that iterate over all
	 * available types.
	 */
	class iterator {
	public:

		iterator(size_t index) : _index(index) {}

		DataType  operator*()  { return DataType((Symbol)_index); }
		iterator& operator++() { ++_index; return *this; } // yes, prefix only
		bool operator==(const iterator& other) { return (_index == other._index); }
		bool operator!=(const iterator& other) { return (_index != other._index); }

	private:
		friend class DataType;

		size_t _index;
	};

	static iterator begin() { return iterator(1); }
	static iterator end()   { return iterator(num_types+1); }
	
	bool operator==(const Symbol symbol) { return (_symbol == symbol); }
	bool operator!=(const Symbol symbol) { return (_symbol != symbol); }
	
	bool operator==(const DataType other) { return (_symbol == other._symbol); }
	bool operator!=(const DataType other) { return (_symbol != other._symbol); }

private:
	Symbol _symbol;
};



} // namespace ARDOUR

#endif // __ardour_data_type_h__

