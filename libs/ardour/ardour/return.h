/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
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

	void collect_input (BufferSet& bufs, pframes_t nframes, ChanCount offset = ChanCount::ZERO);
};

} // namespace ARDOUR

#endif /* __ardour_return_h__ */

