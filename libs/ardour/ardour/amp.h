/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_amp_h__
#define __ardour_amp_h__

#include "ardour/dB.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/chan_count.h"
#include "ardour/processor.h"
#include "ardour/automation_control.h"

namespace ARDOUR {

class BufferSet;
class GainControl;
class IO;

/** Gain Stage (Fader, Trim).  */
class LIBARDOUR_API Amp : public Processor {
public:
	Amp(Session& s, const std::string& display_name, boost::shared_ptr<GainControl> control, bool control_midi_also);

	std::string display_name () const { return _display_name; }
	void set_display_name (const std::string& name) { _display_name = name; }

	bool visible () const;

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool);

	void set_gain_automation_buffer (gain_t *);

	void setup_gain_automation (samplepos_t start_sample, samplepos_t end_sample, samplecnt_t nframes);

	XMLNode& state ();
	int set_state (const XMLNode&, int version);

	static gain_t apply_gain (BufferSet& bufs, samplecnt_t sample_rate, samplecnt_t nframes, gain_t initial, gain_t target, bool midi_amp = true);
	static void apply_simple_gain(BufferSet& bufs, samplecnt_t nframes, gain_t target, bool midi_amp = true);

	static gain_t apply_gain (AudioBuffer& buf, samplecnt_t sample_rate, samplecnt_t nframes, gain_t initial, gain_t target, sampleoffset_t offset = 0);
	static void apply_simple_gain (AudioBuffer& buf, samplecnt_t nframes, gain_t target, sampleoffset_t offset = 0);

	boost::shared_ptr<GainControl> gain_control() {
		return _gain_control;
	}

	boost::shared_ptr<const GainControl> gain_control() const {
		return _gain_control;
	}

private:
	bool   _apply_gain_automation;
	float  _current_gain;
	samplepos_t _current_automation_sample;

	std::string _display_name;

	boost::shared_ptr<GainControl> _gain_control;

	/** Buffer that we should use for gain automation */
	gain_t* _gain_automation_buffer;
	bool _midi_amp;
};


} // namespace ARDOUR

#endif // __ardour_amp_h__
