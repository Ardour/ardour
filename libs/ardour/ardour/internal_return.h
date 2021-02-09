/*
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2021 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_internal_return_h__
#define __ardour_internal_return_h__

#include <list>

#include "ardour/buffer_set.h"
#include "ardour/processor.h"

namespace ARDOUR {

class InternalSend;

class LIBARDOUR_API InternalReturn : public Processor
{
public:
	InternalReturn (Session&, Temporal::TimeDomain, std::string const& name = "Return");

	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool);
	bool configure_io (ChanCount, ChanCount);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);

	void add_send (InternalSend *);
	void remove_send (InternalSend *);

	void set_playback_offset (samplecnt_t cnt);

protected:
	XMLNode& state ();

private:
	/** sends that we are receiving data from */
	std::list<InternalSend*> _sends;
	/** mutex to protect _sends */
	Glib::Threads::Mutex _sends_mutex;
};

} // namespace ARDOUR

#endif /* __ardour_internal_return_h__ */
