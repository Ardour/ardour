/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2006 Paul Davis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __ardour_sidechain_h__
#define __ardour_sidechain_h__

#include <string>

#include "pbd/stateful.h"
#include "ardour/ardour.h"
#include "ardour/io_processor.h"

namespace ARDOUR {

class LIBARDOUR_API SideChain : public IOProcessor
{
public:
	SideChain (Session&, const std::string&);
	virtual ~SideChain ();

	void run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, double speed, pframes_t nframes, bool);

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	XMLNode& state(bool full);
	int      set_state(const XMLNode&, int version);

private:
	/* disallow copy construction */
	SideChain (const SideChain&);
};

} // namespace ARDOUR

#endif // __ardour_sidechain_h__
