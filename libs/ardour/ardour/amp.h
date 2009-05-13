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

#ifndef __ardour_amp_h__
#define __ardour_amp_h__

#include "ardour/types.h"
#include "ardour/chan_count.h"
#include "ardour/processor.h"

namespace ARDOUR {

class BufferSet;
class IO;


/** Applies a declick operation to all audio inputs, passing the same number of
 * audio outputs, and passing through any other types unchanged.
 */
class Amp : public Processor {
public:
	Amp(Session& s, IO& io);

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;
	bool configure_io (ChanCount in, ChanCount out);
	
	void run_in_place (BufferSet& bufs, sframes_t start_frame, sframes_t end_frame, nframes_t nframes);

	bool apply_gain() const  { return _apply_gain; }
	void apply_gain(bool yn) { _apply_gain = yn; }

	bool apply_gain_automation() const  { return _apply_gain_automation; }
	void apply_gain_automation(bool yn) { _apply_gain_automation = yn; }

	void muute(bool yn) { _mute = yn; }

	void set_gain(float current, float desired) {
		_current_gain = current;
		_desired_gain = desired;
	}
	
	void apply_mute(bool yn, float current=1.0, float desired=0.0) {
		_mute = yn;
		_current_mute_gain = current;
		_desired_mute_gain = desired;
	}

	XMLNode& state (bool full);

	static void apply_gain (BufferSet& bufs, nframes_t nframes,
			gain_t initial, gain_t target, bool invert_polarity);

	static void apply_simple_gain(BufferSet& bufs, nframes_t nframes, gain_t target);

private:
	IO&   _io;
	bool  _mute;
	bool  _apply_gain;
	bool  _apply_gain_automation;
	float _current_gain;
	float _desired_gain;
	float _current_mute_gain;
	float _desired_mute_gain;
};


} // namespace ARDOUR

#endif // __ardour_amp_h__
