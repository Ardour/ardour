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

#include <cstring>
#include <cmath>
#include <algorithm>

#include "evoral/Curve.hpp"

#include "ardour/amp.h"
#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/configuration.h"
#include "ardour/io.h"
#include "ardour/mute_master.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace ARDOUR;

Amp::Amp(Session& s, boost::shared_ptr<MuteMaster> mm)
	: Processor(s, "Amp")
	, _apply_gain(true)
	, _apply_gain_automation(false)
	, _current_gain(1.0)
	, _mute_master (mm)
{
	boost::shared_ptr<AutomationList> gl(new AutomationList(Evoral::Parameter(GainAutomation)));
	_gain_control = boost::shared_ptr<GainControl>( new GainControl(X_("gaincontrol"), s, this, Evoral::Parameter(GainAutomation), gl ));
	add_control(_gain_control);
}

bool
Amp::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	out = in;
	return true;
}

bool
Amp::configure_io (ChanCount in, ChanCount out)
{
	if (out != in) { // always 1:1
		return false;
	}
	
	return Processor::configure_io (in, out);
}

void
Amp::run_in_place (BufferSet& bufs, sframes_t start_frame, sframes_t end_frame, nframes_t nframes)
{
	gain_t mute_gain;

	if (_mute_master) {
		mute_gain = _mute_master->mute_gain_at (MuteMaster::PreFader);
	} else {
		mute_gain = 1.0;
	}

	if (_apply_gain) {
		
		if (_apply_gain_automation) {
			
			gain_t* gab = _session.gain_automation_buffer ();

			if (mute_gain == 0.0) {
				
				/* absolute mute */

				if (_current_gain == 0.0) {
					
					/* already silent */

					for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
						i->clear ();
					}
				} else {
					
					/* cut to silence */

					Amp::apply_gain (bufs, nframes, _current_gain, 0.0);
					_current_gain = 0.0;
				}
					

			} else if (mute_gain != 1.0) {

				/* mute dimming */

				for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
					Sample* const sp = i->data();
					for (nframes_t nx = 0; nx < nframes; ++nx) {
						sp[nx] *= gab[nx] * mute_gain;
					}
				}

				_current_gain = gab[nframes-1] * mute_gain;

			} else {

				/* no mute */

				for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
					Sample* const sp = i->data();
					for (nframes_t nx = 0; nx < nframes; ++nx) {
						sp[nx] *= gab[nx];
					}
				}

				_current_gain = gab[nframes-1];
			}
				
			
		} else { /* manual (scalar) gain */

			gain_t dg = _gain_control->user_float() * mute_gain;
			
			if (_current_gain != dg) {
				
				Amp::apply_gain (bufs, nframes, _current_gain, dg);
				_current_gain = dg;
				
			} else if ((_current_gain != 0.0f) && (_current_gain != 1.0f)) {
				
				/* gain has not changed, but its non-unity, so apply it unless
				   its zero.
				*/

				for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
					Sample* const sp = i->data();
					apply_gain_to_buffer(sp, nframes, _current_gain);
				}

			} else if (_current_gain == 0.0f) {
				
				/* silence! */

				for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
					i->clear();
				}
			}
		}
	}
}

void
Amp::apply_gain (BufferSet& bufs, nframes_t nframes, gain_t initial, gain_t target)
{
        /** Apply a (potentially) declicked gain to the audio buffers of @a bufs 
	 */
	
	if (nframes == 0 || bufs.count().n_audio() == 0) {
		return;
	}

	// if we don't need to declick, defer to apply_simple_gain
	if (initial == target) {
		apply_simple_gain (bufs, nframes, target);
		return;
	}

	const nframes_t declick = std::min ((nframes_t)128, nframes);
	gain_t         delta;
	double         fractional_shift = -1.0/declick;
	double         fractional_pos;
	gain_t         polscale = 1.0f;

	if (target < initial) {
		/* fade out: remove more and more of delta from initial */
		delta = -(initial - target);
	} else {
		/* fade in: add more and more of delta from initial */
		delta = target - initial;
	}

	for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
		Sample* const buffer = i->data();

		fractional_pos = 1.0;

		for (nframes_t nx = 0; nx < declick; ++nx) {
			buffer[nx] *= polscale * (initial + (delta * (0.5 + 0.5 * cos (M_PI * fractional_pos))));
			fractional_pos += fractional_shift;
		}
		
		/* now ensure the rest of the buffer has the target value applied, if necessary. */
		
		if (declick != nframes) {

			if (target == 0.0) {
				memset (&buffer[declick], 0, sizeof (Sample) * (nframes - declick));
			} else if (target != 1.0) {
				apply_gain_to_buffer (&buffer[declick], nframes - declick, target);
			}
		}
	}
}

void
Amp::apply_simple_gain (BufferSet& bufs, nframes_t nframes, gain_t target)
{
	if (target == 0.0) {
		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
			memset (i->data(), 0, sizeof (Sample) * nframes);
		}
	} else if (target != 1.0) {
		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
			apply_gain_to_buffer (i->data(), nframes, target);
		}
	}
}

void
Amp::inc_gain (gain_t factor, void *src)
{
	float desired_gain = _gain_control->user_float();
	if (desired_gain == 0.0f) {
		set_gain (0.000001f + (0.000001f * factor), src);
	} else {
		set_gain (desired_gain + (desired_gain * factor), src);
	}
}

void
Amp::set_gain (gain_t val, void *src)
{
	// max gain at about +6dB (10.0 ^ ( 6 dB * 0.05))
	if (val > 1.99526231f) {
		val = 1.99526231f;
	}

	//cerr << "set desired gain to " << val << " when curgain = " << _gain_control->get_value () << endl;

	if (src != _gain_control.get()) {
		_gain_control->set_value(val);
		// bit twisty, this will come back and call us again
		// (this keeps control in sync with reality)
		return;
	}

	{
		// Glib::Mutex::Lock dm (declick_lock);
		_gain_control->set_float(val, false);
	}

	if (_session.transport_stopped()) {
		// _gain = val;
	}
	
	/*
	if (_session.transport_stopped() && src != 0 && src != this && _gain_control->automation_write()) {
		_gain_control->list()->add (_session.transport_frame(), val);
		
	}
	*/

	_session.set_dirty();
}

XMLNode&
Amp::state (bool full_state)
{
	XMLNode& node (Processor::state (full_state));
	node.add_property("type", "amp");
	return node;
}

void
Amp::GainControl::set_value (float val)
{
	// max gain at about +6dB (10.0 ^ ( 6 dB * 0.05))
	if (val > 1.99526231f)
		val = 1.99526231f;

	_amp->set_gain (val, this);
	
	AutomationControl::set_value(val);
}

float
Amp::GainControl::get_value (void) const
{
	return AutomationControl::get_value();
}

void
Amp::setup_gain_automation (sframes_t start_frame, sframes_t end_frame, nframes_t nframes)
{
	Glib::Mutex::Lock am (data().control_lock(), Glib::TRY_LOCK);

	if (am.locked() && _session.transport_rolling() && _gain_control->automation_playback()) {
		_apply_gain_automation = _gain_control->list()->curve().rt_safe_get_vector (
			start_frame, end_frame, _session.gain_automation_buffer(), nframes);
	} else {
		_apply_gain_automation = false;
	}
}
