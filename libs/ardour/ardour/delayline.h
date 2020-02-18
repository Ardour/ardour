/*
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_delayline_h__
#define __ardour_delayline_h__

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

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

	bool set_name (const std::string& str);
	bool set_delay (samplecnt_t signal_delay);
	samplecnt_t delay () { return _pending_delay; }

	/* processor interface */
	bool display_to_user () const { return false; }
	void run (BufferSet&, samplepos_t, samplepos_t, double, pframes_t, bool);
	bool configure_io (ChanCount in, ChanCount out);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	void flush ();

protected:
	XMLNode& state ();

private:
	void allocate_pending_buffers (samplecnt_t, ChanCount const&);

	void write_to_rb (Sample* rb, Sample* src, samplecnt_t); // honor _woff, _bsiz.
	void read_from_rb (Sample* rb, Sample* dst, samplecnt_t); // honor _roff, _bsiz

	friend class IO;

	samplecnt_t    _bsiz;
	samplecnt_t    _bsiz_mask;
	samplecnt_t    _delay, _pending_delay;
	sampleoffset_t _roff, _woff;
	bool           _pending_flush;

	typedef std::vector<boost::shared_array<Sample> > AudioDlyBuf;
	typedef std::vector<boost::shared_array<MidiBuffer> > MidiDlyBuf;

	AudioDlyBuf _buf;
	boost::shared_ptr<MidiBuffer> _midi_buf;

#ifndef NDEBUG
	Glib::Threads::Mutex _set_delay_mutex;
#endif
};

} // namespace ARDOUR

#endif // __ardour_meter_h__
