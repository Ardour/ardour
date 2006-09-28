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

    $Id$
*/

#ifndef __ardour_send_h__
#define __ardour_send_h__

#include <sigc++/signal.h>
#include <string>


#include <pbd/stateful.h> 
#include <ardour/ardour.h>
#include <ardour/audioengine.h>
#include <ardour/io.h>
#include <ardour/redirect.h>

namespace ARDOUR {

class Send : public Redirect {
  public:	
	Send (Session&, Placement);
	Send (Session&, const XMLNode&);
	Send (const Send&);
	~Send ();
	
	void run (vector<Sample *> &bufs, uint32_t nbufs, nframes_t nframes, nframes_t offset);
	void activate() {}
	void deactivate () {}

	void set_metering (bool yn);

	XMLNode& state(bool full);
	XMLNode& get_state(void);
	int set_state(const XMLNode& node);

	uint32_t pans_required() const { return expected_inputs; }
	void expect_inputs (uint32_t);

  private:
	bool _metering;
	uint32_t expected_inputs;
};

} // namespace ARDOUR

#endif /* __ardour_send_h__ */
