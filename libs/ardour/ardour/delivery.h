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

#ifndef __ardour_delivery_h__
#define __ardour_delivery_h__

#include <string>
#include "ardour/types.h"
#include "ardour/chan_count.h"
#include "ardour/io_processor.h"

namespace ARDOUR {

class BufferSet;
class IO;

class Delivery : public IOProcessor {
public:
	enum Role {
		Send   = 0x1,
		Solo   = 0x2,
		Listen = 0x4,
		Main   = 0x8
	};

	Delivery (Session& s, IO* io, const std::string& name, Role);
	Delivery (Session& s, const std::string& name, Role);
	Delivery (Session&, const XMLNode&);

	bool visible() const;

	Role role() const { return _role; }

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;
	bool configure_io (ChanCount in, ChanCount out);

	void run_in_place (BufferSet& bufs, sframes_t start_frame, sframes_t end_frame, nframes_t nframes);

	void set_metering (bool yn);
	
	bool muted_by_self() const { return _muted_by_self; }
	bool muted_by_others() const { return _muted_by_others; }

	void set_self_mute (bool);
	void set_nonself_mute (bool);
	
	sigc::signal<void> SelfMuteChange;
	sigc::signal<void> OtherMuteChange;

	XMLNode& state (bool full);
	int set_state (const XMLNode&);

private:
	Role _role;
	bool _metering;
	bool _muted_by_self;
	bool _muted_by_others;
};


} // namespace ARDOUR

#endif // __ardour__h__

