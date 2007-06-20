/*
    Copyright (C) 2000 Paul Davis 

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

*/

#ifndef __ardour_insert_h__
#define __ardour_insert_h__

#include <vector>
#include <string>
#include <exception>

#include <sigc++/signal.h>
#include <ardour/ardour.h>
#include <ardour/redirect.h>
#include <ardour/plugin_state.h>
#include <ardour/types.h>

class XMLNode;

namespace ARDOUR {

class Session;

class Insert : public Redirect
{
  public:
	Insert(Session& s, std::string name, Placement p);
	Insert(Session& s, std::string name, Placement p, int imin, int imax, int omin, int omax);
	
	virtual ~Insert() { }

	virtual void run (BufferSet& bufs, nframes_t start_frame, nframes_t end_frame, nframes_t nframes, nframes_t offset) = 0;
	
	virtual void activate () {}
	virtual void deactivate () {}

	virtual bool      can_support_input_configuration (ChanCount in) const = 0;
	virtual ChanCount output_for_input_configuration (ChanCount in) const = 0;
	virtual bool      configure_io (ChanCount in, ChanCount out) = 0;

protected:
	bool      _configured;
	ChanCount _configured_input;
};

} // namespace ARDOUR

#endif /* __ardour_insert_h__ */
