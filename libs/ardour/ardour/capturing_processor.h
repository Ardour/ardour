/*
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_capturing_processor_h__
#define __ardour_capturing_processor_h__

#include "ardour/fixed_delay.h"
#include "ardour/processor.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API CapturingProcessor : public Processor
{
public:
	CapturingProcessor (Session & session, samplecnt_t latency);
	~CapturingProcessor();

public: // main interface
	BufferSet const & get_capture_buffers() const { return capture_buffers; }

public: // Processor overrides
	bool display_to_user() const { return false; }
	int set_block_size (pframes_t nframes);
	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required);
	bool configure_io (ChanCount in, ChanCount out);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);

protected:
	XMLNode& state ();

private:
	void realloc_buffers();

	samplecnt_t block_size;
	BufferSet capture_buffers;
	FixedDelay _delaybuffers;
	samplecnt_t _latency;
};

} // namespace ARDOUR

#endif // __ardour_capturing_processor_h__
