/*
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <vector>
#include <string>
#include <exception>

#include "ardour/ardour.h"
#include "ardour/io_processor.h"
#include "ardour/delivery.h"
#include "ardour/libardour_visibility.h"
#include "ardour/meter.h"
#include "ardour/types.h"

class XMLNode;
class MTDM;

namespace ARDOUR {

class Amp;
class Session;
class IO;
class Delivery;
class PeakMeter;
class MuteMaster;
class Pannable;

/** Port inserts: send output to a Jack port, pick up input at a Jack port
 */
class LIBARDOUR_API PortInsert : public IOProcessor
{
public:
	PortInsert (Session&, std::shared_ptr<Pannable>, std::shared_ptr<MuteMaster> mm);
	~PortInsert ();

	int set_state (const XMLNode&, int version);

	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool);

	void flush_buffers (samplecnt_t nframes);

	samplecnt_t signal_latency () const;

	bool set_name (const std::string& name);

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	void activate ();
	void deactivate ();

	void set_pre_fader (bool);

	void start_latency_detection ();
	void stop_latency_detection ();

	MTDM* mtdm () const { return _mtdm; }
	void set_measured_latency (samplecnt_t);

	samplecnt_t measured_latency () const {
		return _measured_latency;
	}

	static std::string name_and_id_new_insert (Session&, uint32_t&);

	std::shared_ptr<AutomationControl> send_polarity_control () const {
		return _out->polarity_control ();
	}

	std::shared_ptr<GainControl> send_gain_control () const {
		return _out->gain_control ();
	}

	std::shared_ptr<Amp> send_amp() const {
		return _out->amp ();
	}

	std::shared_ptr<Amp> return_amp() const {
		return _amp;
	}

	std::shared_ptr<GainControl> return_gain_control () const {
		return _gain_control;
	}

	std::shared_ptr<PeakMeter> send_meter() const {
		return _send_meter;
	}

	std::shared_ptr<PeakMeter> return_meter() const {
		return _return_meter;
	}

	bool metering() const {
		return _metering;
	}

	void set_metering (bool yn) {
		_metering = yn;
	}

protected:
	XMLNode& state () const;
private:
	/* disallow copy construction */
	PortInsert (const PortInsert&);

	void io_changed (IOChange change, void*);
	void latency_changed ();

	std::shared_ptr<Delivery>    _out;
	std::shared_ptr<Amp>         _amp;
	std::shared_ptr<GainControl> _gain_control;
	std::shared_ptr<PeakMeter>   _send_meter;
	std::shared_ptr<PeakMeter>   _return_meter;
	bool                           _metering;
	uint32_t                       _io_latency;
	uint32_t                       _signal_latency;

	MTDM*       _mtdm;
	bool        _latency_detect;
	samplecnt_t _latency_flush_samples;
	samplecnt_t _measured_latency;
};

} // namespace ARDOUR

