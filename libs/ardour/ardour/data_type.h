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

#include <jack/jack.h>

namespace ARDOUR {

class DataType
{
public:
	enum Symbol {
		NIL = 0,
		AUDIO,
		MIDI
	};

	DataType(const Symbol& symbol)
	: _symbol(symbol)
	{}

	/** Construct from a string (Used for loading from XML) */
	DataType(const string& str) {
		if (str == "audio")
			_symbol = AUDIO;
		//else if (str == "midi")
		//	_symbol = MIDI;
		else
			_symbol = NIL;
	}

	bool operator==(const Symbol symbol) { return _symbol == symbol; }
	bool operator!=(const Symbol symbol) { return _symbol != symbol; }

	/** Get the Jack type this DataType corresponds to */
	const char* to_jack_type() {
		switch (_symbol) {
			case AUDIO: return JACK_DEFAULT_AUDIO_TYPE;
			//case MIDI:  return JACK_DEFAULT_MIDI_TYPE;
			default:    return "";
		}
	}
	
	/** Inverse of the from-string constructor */
	const char* to_string() {
		switch (_symbol) {
			case AUDIO: return "audio";
			//case MIDI:  return "midi";
			default:    return "unknown"; // reeeally shouldn't ever happen
		}
	}

private:
	Symbol _symbol;
};



} // namespace ARDOUR

#endif // __ardour_data_type_h__

