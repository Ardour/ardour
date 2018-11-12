/*
    Copyright (C) 2009 Paul Davis
    Author: David Robillard

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

#include <string>


#include "pbd/stateful.h"
#include "ardour/ardour.h"
#include "ardour/io_processor.h"

namespace ARDOUR {

class Amp;
class PeakMeter;
class GainControl;

class LIBARDOUR_API Return : public IOProcessor
{
public:
	Return (Session&, bool internal = false);
	virtual ~Return ();

	uint32_t bit_slot() const { return _bitslot; }

	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool);

	boost::shared_ptr<Amp> amp() const { return _amp; }
	boost::shared_ptr<PeakMeter> meter() const { return _meter; }
	boost::shared_ptr<GainControl> gain_control() const { return _gain_control; }

	bool metering() const { return _metering; }
	void set_metering (bool yn) { _metering = yn; }

	int      set_state(const XMLNode&, int version);

	uint32_t pans_required() const { return _configured_input.n_audio(); }

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	static uint32_t how_many_returns();
	static std::string name_and_id_new_return (Session&, uint32_t&);

protected:
	XMLNode& state();

	bool _metering;
	boost::shared_ptr<GainControl> _gain_control;
	boost::shared_ptr<Amp> _amp;
	boost::shared_ptr<PeakMeter> _meter;

private:
	/* disallow copy construction */
	Return (const Return&);

	uint32_t _bitslot;

	void collect_input (BufferSet& bufs, pframes_t nframes, ChanCount offset = ChanCount::ZERO);
};

} // namespace ARDOUR

#endif /* __ardour_return_h__ */

