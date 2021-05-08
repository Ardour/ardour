/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_monitor_return_h__
#define __ardour_monitor_return_h__

#include "ardour/internal_return.h"

namespace ARDOUR {

class AudioPort;

class LIBARDOUR_API MonitorReturn : public InternalReturn
{
public:
	MonitorReturn (Session&);
	~MonitorReturn ();

	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool);

protected:
	XMLNode& state ();

	uint32_t _nch;
	gain_t   _gain;
};

} // namespace ARDOUR

#endif
