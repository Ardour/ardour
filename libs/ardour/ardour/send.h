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

#ifndef __ardour_send_h__
#define __ardour_send_h__

#include <sigc++/signal.h>
#include <string>


#include <pbd/stateful.h> 
#include <ardour/ardour.h>
#include <ardour/audioengine.h>
#include <ardour/io.h>
#include <ardour/io_processor.h>

namespace ARDOUR {

class Send : public IOProcessor 
{
  public:	
	Send (Session&, Placement);
	Send (Session&, const XMLNode&);
	Send (const Send&);
	virtual ~Send ();

	uint32_t bit_slot() const { return bitslot; }
	
	ChanCount output_streams() const;
	ChanCount input_streams () const;
	
	void run_in_place (BufferSet& bufs, nframes_t start_frame, nframes_t end_frame, nframes_t nframes, nframes_t offset);
	
	void activate() {}
	void deactivate () {}

	void set_metering (bool yn);

	XMLNode& state(bool full);
	XMLNode& get_state(void);
	int set_state(const XMLNode& node);

	uint32_t pans_required() const { return _configured_input.n_audio(); }
	void expect_inputs (const ChanCount&);

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;
	bool configure_io (ChanCount in, ChanCount out);

	static uint32_t how_many_sends();

  private:
	bool      _metering;
	ChanCount expected_inputs;
	uint32_t  bitslot;
};

} // namespace ARDOUR

#endif /* __ardour_send_h__ */
