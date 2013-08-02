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
#include "ardour/automation_control.h"

namespace ARDOUR {

class BufferSet;
class IO;

/** Applies a declick operation to all audio inputs, passing the same number of
 * audio outputs, and passing through any other types unchanged.
 */
class Amp : public Processor {
public:
	Amp(Session& s);

	std::string display_name() const;

	bool visible () const;

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	void run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool);

	bool apply_gain () const  { return _apply_gain; }
	void apply_gain (bool yn) { _apply_gain = yn; }

	void set_gain_automation_buffer (gain_t *);

	void setup_gain_automation (framepos_t start_frame, framepos_t end_frame, framecnt_t nframes);

	bool apply_gain_automation() const  { return _apply_gain_automation; }
	void apply_gain_automation(bool yn) { _apply_gain_automation = yn; }

	XMLNode& state (bool full);
	int set_state (const XMLNode&, int version);

	static void apply_gain (BufferSet& bufs, framecnt_t nframes, gain_t initial, gain_t target);
	static void apply_simple_gain(BufferSet& bufs, framecnt_t nframes, gain_t target);

	static void apply_gain (AudioBuffer& buf, framecnt_t nframes, gain_t initial, gain_t target);
	static void apply_simple_gain(AudioBuffer& buf, framecnt_t nframes, gain_t target);

	static void declick (BufferSet& bufs, framecnt_t nframes, int dir);

	gain_t         gain () const { return _gain_control->get_value(); }

	void           set_gain (gain_t g, void *src);
	void           inc_gain (gain_t delta, void *src);

	static void update_meters();

	/* automation */

	struct GainControl : public AutomationControl {
		GainControl (std::string name, Session& session, Amp* a, const Evoral::Parameter &param,
				boost::shared_ptr<AutomationList> al = boost::shared_ptr<AutomationList>() )
			: AutomationControl (session, param, al, name)
			, _amp (a) {
			set_flags (Controllable::Flag (flags() | Controllable::GainLike));
			alist()->reset_default (1.0);
		}

		void set_value (double val);

		double internal_to_interface (double) const;
		double interface_to_internal (double) const;
		double internal_to_user (double) const;

		Amp* _amp;
	};

	boost::shared_ptr<GainControl> gain_control() {
		return _gain_control;
	}

	boost::shared_ptr<const GainControl> gain_control() const {
		return _gain_control;
	}

	std::string value_as_string (boost::shared_ptr<AutomationControl>) const;

        static const float max_gain_coefficient;

private:
	bool   _denormal_protection;
	bool   _apply_gain;
	bool   _apply_gain_automation;
	float  _current_gain;

	boost::shared_ptr<GainControl> _gain_control;

	/** Buffer that we should use for gain automation */
	gain_t* _gain_automation_buffer;
};


} // namespace ARDOUR

#endif // __ardour_amp_h__
