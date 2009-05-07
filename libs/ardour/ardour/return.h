/*
    Copyright (C) 2009 Paul Davis 
    Author: Dave Robillard

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

#ifndef __ardour_return_h__
#define __ardour_return_h__

#include <sigc++/signal.h>
#include <string>


#include "pbd/stateful.h" 
#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/io_processor.h"

namespace ARDOUR {

class Return : public IOProcessor 
{
public:	
	Return (Session&);
	Return (Session&, const XMLNode&);
	virtual ~Return ();
	
	uint32_t bit_slot() const { return _bitslot; }

	void run_in_place (BufferSet& bufs, nframes_t start_frame, nframes_t end_frame, nframes_t nframes);
	
	void activate() {}
	void deactivate () {}

	XMLNode& state(bool full);
	XMLNode& get_state(void);
	int      set_state(const XMLNode& node);

	uint32_t pans_required() const { return _configured_input.n_audio(); }

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;
	bool configure_io (ChanCount in, ChanCount out);

	static uint32_t how_many_sends();
	static void make_unique (XMLNode &, Session &);

private:
	/* disallow copy construction */
	Return (const Return&);
	
	uint32_t  _bitslot;
};

} // namespace ARDOUR

#endif /* __ardour_return_h__ */

