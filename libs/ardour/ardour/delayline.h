/*
    Copyright (C) 2006, 2013 Paul Davis
    Copyright (C) 2013, 2014 Robin Gareus <robin@gareus.org>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_delayline_h__
#define __ardour_delayline_h__

#include "ardour/types.h"
#include "ardour/processor.h"

namespace ARDOUR {

class BufferSet;
class ChanCount;
class Session;

/** Meters peaks on the input and stores them for access.
 */
class LIBARDOUR_API DelayLine : public Processor {
public:

  DelayLine (Session& s, const std::string& name);
	~DelayLine ();

	bool display_to_user() const { return false; }

	void run (BufferSet&, framepos_t, framepos_t, pframes_t, bool);
	void set_delay(framecnt_t signal_delay);
	framecnt_t get_delay() { return _pending_delay; }

	bool configure_io (ChanCount in, ChanCount out);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);

	void flush();
	void realtime_handle_transport_stopped () { flush(); }
	void realtime_locate () { flush(); }
	void monitoring_changed() { flush(); }

	XMLNode& state (bool full);

private:
	friend class IO;
	framecnt_t _delay, _pending_delay;
	framecnt_t _bsiz,  _pending_bsiz;
	frameoffset_t _roff, _woff;
	boost::shared_ptr<Sample> _buf;
	boost::shared_ptr<Sample> _pending_buf;
	boost::shared_ptr<MidiBuffer> _midi_buf;
	bool _pending_flush;
};

} // namespace ARDOUR

#endif // __ardour_meter_h__
