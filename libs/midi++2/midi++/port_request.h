/*
    Copyright (C) 1999 Paul Barton-Davis 

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

#ifndef __midi_port_request_h__
#define __midi_port_request_h__

#include <string>

namespace MIDI {

struct PortRequest {
    enum Status {
	    Unknown,
	    OK,
	    Busy,
	    NoSuchFile,
	    TypeUnsupported,
	    NotAllowed
    };
    const char *devname;
    const char *tagname;
    int mode;
    Port::Type type;
    Status status;
    
    PortRequest () {
	    devname = 0;
	    tagname = 0;
	    mode = 0;
	    type = Port::Unknown;
	    status = Unknown;
    }

    PortRequest (const std::string &xdev, 
		 const std::string &xtag, 
		 const std::string &xmode,
		 const std::string &xtype);
};

}; /* namespace MIDI */

#endif // __midi_port_request_h__

