/*
    Copyright (C) 2006 Paul Davis

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

#ifndef __ardour_meter_h__
#define __ardour_meter_h__

#include <vector>
#include "ardour/types.h"
#include "ardour/processor.h"
#include "pbd/fastlog.h"

#include "ardour/kmeterdsp.h"
#include "ardour/iec1ppmdsp.h"
#include "ardour/iec2ppmdsp.h"
#include "ardour/vumeterdsp.h"

namespace ARDOUR {

class BufferSet;
class ChanCount;
class Session;

class Metering {
  public:
	static void               update_meters ();
	static PBD::Signal0<void> Meter;

  private:
	/* this object is not meant to be instantiated */
	Metering();
};

/** Meters peaks on the input and stores them for access.
 */
class PeakMeter : public Processor {
public:
        PeakMeter(Session& s, const std::string& name);
        ~PeakMeter();

	void meter();
	void reset ();
	void reset_max ();

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;
	bool configure_io (ChanCount in, ChanCount out);

	/* special method for meter, to ensure that it can always handle the maximum
	   number of streams in the route, no matter where we put it.
	*/

	void reset_max_channels (const ChanCount&);

	/* tell the meter than no matter how many channels it can handle,
	   `in' is the number it is actually going be handling from
	   now on.
	*/

	void reflect_inputs (const ChanCount& in);

	/** Compute peaks */
	void run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool);

	ChanCount input_streams () const { return current_meters; }
	ChanCount output_streams () const { return current_meters; }

	float peak_power (uint32_t n) {
		if (n < _visible_peak_power.size()) {
			return _visible_peak_power[n];
		} else {
			return minus_infinity();
		}
	}

	float meter_level (uint32_t n, MeterType type);

	void set_type(MeterType t);
	MeterType get_type() { return _meter_type; }

	XMLNode& state (bool full);

	PBD::Signal1<void, MeterType> TypeChanged;

private:
	friend class IO;

	/** The number of meters that we are currently handling;
	 *  may be different to _configured_input and _configured_output
	 *  as it can be altered outside a ::configure_io by ::reflect_inputs.
	 */
	ChanCount current_meters;

	std::vector<float> _peak_signal;
	std::vector<float> _visible_peak_power;
	std::vector<float> _max_peak_signal;
	std::vector<float> _max_peak_power;

	std::vector<Kmeterdsp *> _kmeter;
	std::vector<Iec1ppmdsp *> _iec1meter;
	std::vector<Iec2ppmdsp *> _iec2meter;
	std::vector<Vumeterdsp *> _vumeter;

	MeterType _meter_type;
};

} // namespace ARDOUR

#endif // __ardour_meter_h__
