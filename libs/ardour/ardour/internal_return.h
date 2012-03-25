/*
    Copyright (C) 2009 Paul Davis

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

#ifndef __ardour_internal_return_h__
#define __ardour_internal_return_h__


#include "ardour/ardour.h"
#include "ardour/return.h"
#include "ardour/buffer_set.h"

namespace ARDOUR {

class InternalSend;

class InternalReturn : public Return
{
  public:
	InternalReturn (Session&);

	XMLNode& state (bool full);
	XMLNode& get_state ();

	void run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool);
	bool configure_io (ChanCount, ChanCount);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;

	void add_send (InternalSend *);
	void remove_send (InternalSend *);

  private:
	/** sends that we are receiving data from */
	std::list<InternalSend*> _sends;
	/** mutex to protect _sends */
	Glib::Mutex _sends_mutex;
};

} // namespace ARDOUR

#endif /* __ardour_internal_return_h__ */
