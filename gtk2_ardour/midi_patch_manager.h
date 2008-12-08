/*
    Copyright (C) 2008 Hans Baier 

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

#ifndef MIDI_PATCH_MANAGER_H_
#define MIDI_PATCH_MANAGER_H_

#include "midi++/midnam_patch.h"

namespace ARDOUR {
	class Session;
}

namespace MIDI
{

namespace Name
{

class MidiPatchManager
{
	/// Singleton
private:
	MidiPatchManager() {};
	MidiPatchManager( const MidiPatchManager& );
	MidiPatchManager& operator= (const MidiPatchManager&);
	
	static MidiPatchManager* _manager; 
	
public:
	typedef std::list<boost::shared_ptr<MIDINameDocument> > MidiNameDocuments;
	
	virtual ~MidiPatchManager() { _manager = 0; }
	
	static MidiPatchManager& instance() { 
		if (_manager == 0) {
			_manager = new MidiPatchManager();
		}
		return *_manager; 
	}
	
	void set_session (ARDOUR::Session&);
	
private:
	void drop_session();
	void refresh();
	
	ARDOUR::Session*  _session;
	MidiNameDocuments _documents;
};

} // namespace Name

} // namespace MIDI
#endif /* MIDI_PATCH_MANAGER_H_ */
