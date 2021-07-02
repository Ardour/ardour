/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2020 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_meter_h__
#define __ardour_meter_h__

#include <vector>

#include "pbd/fastlog.h"
#include "pbd/g_atomic_compat.h"

#include "ardour/libardour_visibility.h"
#include "ardour/processor.h"
#include "ardour/types.h"

#include "ardour/iec1ppmdsp.h"
#include "ardour/iec2ppmdsp.h"
#include "ardour/kmeterdsp.h"
#include "ardour/vumeterdsp.h"

namespace ARDOUR {

class BufferSet;
class ChanCount;
class Session;

/** Meters peaks on the input and stores them for access.
 */
class LIBARDOUR_API PeakMeter : public Processor
{
public:
	PeakMeter (Session& s, const std::string& name);
	~PeakMeter ();

	void reset ();
	void reset_max ();

	std::string display_name () const;

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	/* special method for meter, to ensure that it can always handle the maximum
	   number of streams in the route, no matter where we put it.
	*/

	void set_max_channels (const ChanCount&);

	/* tell the meter than no matter how many channels it can handle,
	   `in' is the number it is actually going be handling from
	   now on.
	*/

	void reflect_inputs (const ChanCount& in);
	void emit_configuration_changed ();

	/** Compute peaks */
	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool);

	void activate () {}
	void deactivate () {}

	ChanCount input_streams ()  const { return current_meters; }
	ChanCount output_streams () const { return current_meters; }

	float meter_level (uint32_t n, MeterType type);

	void      set_meter_type (MeterType t);
	MeterType meter_type () const { return _meter_type; }

	PBD::Signal1<void, MeterType> MeterTypeChanged;

protected:
	XMLNode& state ();

private:
	friend class IO;

	/** The number of meters that we are currently handling;
	 *  may be different to _configured_input and _configured_output
	 *  as it can be altered outside a \ref configure_io by \ref reflect_inputs .
	 */
	ChanCount current_meters;

	GATOMIC_QUAL gint _reset_dpm;
	GATOMIC_QUAL gint _reset_max;

	uint32_t           _bufcnt;
	std::vector<float> _peak_buffer;     // internal, integrate
	std::vector<float> _peak_power;      // includes accurate falloff, hence dB
	std::vector<float> _max_peak_signal; // dB calculation is done on demand

	std::vector<Kmeterdsp*>  _kmeter;
	std::vector<Iec1ppmdsp*> _iec1meter;
	std::vector<Iec2ppmdsp*> _iec2meter;
	std::vector<Vumeterdsp*> _vumeter;

	MeterType _meter_type;
};

} // namespace ARDOUR

#endif // __ardour_meter_h__
