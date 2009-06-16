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

#include <sigc++/signal.h>

#include "ardour/ardour.h"
#include "ardour/return.h"
#include "ardour/buffer_set.h"

namespace ARDOUR {

class InternalReturn : public Return
{
  public:	
	InternalReturn (Session&);
	InternalReturn (Session&, const XMLNode&);

	XMLNode& state(bool full);
	XMLNode& get_state(void);
	int set_state(const XMLNode& node);

	void run (BufferSet& bufs, sframes_t start_frame, sframes_t end_frame, nframes_t nframes);
	bool configure_io (ChanCount in, ChanCount out);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;
	void set_block_size (nframes_t);

	BufferSet* get_buffers();
	void release_buffers();

	static sigc::signal<void,nframes_t> CycleStart;

  private:
	BufferSet buffers;
	uint32_t  user_count;
	void allocate_buffers (nframes_t);
	void cycle_start (nframes_t);
};

} // namespace ARDOUR

#endif /* __ardour_internal_return_h__ */
