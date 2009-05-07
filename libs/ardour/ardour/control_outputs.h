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

#ifndef __ardour_control_outputs_h__
#define __ardour_control_outputs_h__

#include <string>
#include "ardour/types.h"
#include "ardour/chan_count.h"
#include "ardour/io_processor.h"

namespace ARDOUR {

class BufferSet;
class IO;

class ControlOutputs : public IOProcessor {
public:
	ControlOutputs(Session& s, IO* io);

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;
	bool configure_io (ChanCount in, ChanCount out);

	void run_in_place (BufferSet& bufs, nframes_t start_frame, nframes_t end_frame, nframes_t nframes);
	
	bool deliver() const  { return _deliver; }
	void deliver(bool yn) { _deliver = yn; }

	XMLNode& state (bool full);
	XMLNode& get_state();

private:
	bool _deliver;
};


} // namespace ARDOUR

#endif // __ardour_control_outputs_h__

