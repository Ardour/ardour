/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_polarity_processor_h__
#define _ardour_polarity_processor_h__

#include "ardour/processor.h"
#include "ardour/types.h"

namespace ARDOUR {

class PhaseControl;

class LIBARDOUR_API PolarityProcessor : public Processor
{
public:
	PolarityProcessor (Session&, boost::shared_ptr<PhaseControl>);

	bool display_to_user() const { return false; }
	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required);
	bool configure_io (ChanCount in, ChanCount out);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);

	boost::shared_ptr<PhaseControl> phase_control() {
		return _control;
	}

protected:
	XMLNode& state ();

private:
	boost::shared_ptr<PhaseControl> _control;
	std::vector<gain_t> _current_gain;
};

}

#endif
