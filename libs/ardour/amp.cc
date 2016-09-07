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
#include "ardour/gain_control.h"
#include "ardour/midi_buffer.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

// used for low-pass filter denormal protection
#define GAIN_COEFF_TINY (1e-10) // -200dB

Amp::Amp (Session& s, const std::string& name, boost::shared_ptr<GainControl> gc, bool control_midi_also)
	: Processor(s, "Amp")
	, _apply_gain(true)
	, _apply_gain_automation(false)
	, _current_gain(GAIN_COEFF_ZERO)
	, _current_automation_frame (INT64_MAX)
	, _gain_control (gc)
	, _gain_automation_buffer(0)
	, _midi_amp (control_midi_also)
{
	set_display_name (name);
	add_control (_gain_control);
}

bool
Amp::can_support_io_configuration (const ChanCount& in, ChanCount& out)
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

static void
scale_midi_velocity(Evoral::Event<MidiBuffer::TimeType>& ev, float factor)
{
	factor = std::max(factor, 0.0f);
	ev.set_velocity(std::min(127L, lrintf(ev.velocity() * factor)));
}

void
Amp::run (BufferSet& bufs, framepos_t /*start_frame*/, framepos_t /*end_frame*/, double /*speed*/, pframes_t nframes, bool)
{
	if (!_active && !_pending_active) {
		return;
	}

	if (_apply_gain) {

		if (_apply_gain_automation) {

			gain_t* gab = _gain_automation_buffer;
			assert (gab);

			if (_midi_amp) {
				for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {
					MidiBuffer& mb (*i);
					for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ++m) {
						Evoral::Event<MidiBuffer::TimeType> ev = *m;
						if (ev.is_note_on()) {
							assert(ev.time() >= 0 && ev.time() < nframes);
							scale_midi_velocity (ev, fabsf (gab[ev.time()]));
						}
					}
				}
			}


			const double a = 156.825 / _session.nominal_frame_rate(); // 25 Hz LPF; see Amp::apply_gain for details
			double lpf = _current_gain;

			for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
				Sample* const sp = i->data();
				lpf = _current_gain;
				for (pframes_t nx = 0; nx < nframes; ++nx) {
					sp[nx] *= lpf;
					lpf += a * (gab[nx] - lpf);
				}
			}

			if (fabs (lpf) < GAIN_COEFF_TINY) {
				_current_gain = GAIN_COEFF_ZERO;
			} else {
				_current_gain = lpf;
			}

		} else { /* manual (scalar) gain */

			gain_t const dg = _gain_control->get_value();

			if (_current_gain != dg) {

				_current_gain = Amp::apply_gain (bufs, _session.nominal_frame_rate(), nframes, _current_gain, dg, _midi_amp);

			} else if (_current_gain != GAIN_COEFF_UNITY) {

				/* gain has not changed, but its non-unity */

				if (_midi_amp) {
					/* don't Trim midi velocity -- only relevant for Midi on Audio tracks */
					for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {

						MidiBuffer& mb (*i);

						for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ++m) {
							Evoral::Event<MidiBuffer::TimeType> ev = *m;
							if (ev.is_note_on()) {
								scale_midi_velocity (ev, fabsf (_current_gain));
							}
						}
					}
				}

				for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
					apply_gain_to_buffer (i->data(), nframes, _current_gain);
				}
			}
		}
	}

	_active = _pending_active;
}

gain_t
Amp::apply_gain (BufferSet& bufs, framecnt_t sample_rate, framecnt_t nframes, gain_t initial, gain_t target, bool midi_amp)
{
        /** Apply a (potentially) declicked gain to the buffers of @a bufs */
	gain_t rv = target;

	if (nframes == 0 || bufs.count().n_total() == 0) {
		return initial;
	}

	// if we don't need to declick, defer to apply_simple_gain
	if (initial == target) {
		apply_simple_gain (bufs, nframes, target);
		return target;
	}

	/* MIDI Gain */
	if (midi_amp) {
		/* don't Trim midi velocity -- only relevant for Midi on Audio tracks */
		for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {

			gain_t  delta;
			if (target < initial) {
				/* fade out: remove more and more of delta from initial */
				delta = -(initial - target);
			} else {
				/* fade in: add more and more of delta from initial */
				delta = target - initial;
			}

			MidiBuffer& mb (*i);

			for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ++m) {
				Evoral::Event<MidiBuffer::TimeType> ev = *m;

				if (ev.is_note_on()) {
					const gain_t scale = delta * (ev.time()/(double) nframes);
					scale_midi_velocity (ev, fabsf (initial + scale));
				}
			}
		}
	}

	/* Audio Gain */

	/* Low pass filter coefficient: 1.0 - e^(-2.0 * π * f / 48000) f in Hz.
	 * for f << SR,  approx a ~= 6.2 * f / SR;
	 */
	const double a = 156.825 / sample_rate; // 25 Hz LPF

	for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
		Sample* const buffer = i->data();
		double lpf = initial;

		for (pframes_t nx = 0; nx < nframes; ++nx) {
			buffer[nx] *= lpf;
			lpf += a * (target - lpf);
		}
		if (i == bufs.audio_begin()) {
			rv = lpf;
		}
	}
	if (fabsf (rv - target) < GAIN_COEFF_TINY) return target;
	if (fabsf (rv) < GAIN_COEFF_TINY) return GAIN_COEFF_ZERO;
	return rv;
}

void
Amp::declick (BufferSet& bufs, framecnt_t nframes, int dir)
{
	if (nframes == 0 || bufs.count().n_total() == 0) {
		return;
	}

	const framecnt_t declick = std::min ((framecnt_t) 512, nframes);
	const double     fractional_shift = 1.0 / declick ;
	gain_t           delta, initial;

	if (dir < 0) {
		/* fade out: remove more and more of delta from initial */
		delta = -1.0;
                initial = GAIN_COEFF_UNITY;
	} else {
		/* fade in: add more and more of delta from initial */
		delta = 1.0;
                initial = GAIN_COEFF_ZERO;
	}

	/* Audio Gain */
	for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
		Sample* const buffer = i->data();

		double fractional_pos = 0.0;

		for (pframes_t nx = 0; nx < declick; ++nx) {
			buffer[nx] *= initial + (delta * fractional_pos);
			fractional_pos += fractional_shift;
		}

		/* now ensure the rest of the buffer has the target value applied, if necessary. */
		if (declick != nframes) {
			if (dir < 0) {
                                memset (&buffer[declick], 0, sizeof (Sample) * (nframes - declick));
			}
		}
	}
}


gain_t
Amp::apply_gain (AudioBuffer& buf, framecnt_t sample_rate, framecnt_t nframes, gain_t initial, gain_t target)
{
        /* Apply a (potentially) declicked gain to the contents of @a buf
	 * -- used by MonitorProcessor::run()
	 */

	if (nframes == 0) {
		return initial;
	}

	// if we don't need to declick, defer to apply_simple_gain
	if (initial == target) {
		apply_simple_gain (buf, nframes, target);
		return target;
	}

        Sample* const buffer = buf.data();
	const double a = 156.825 / sample_rate; // 25 Hz LPF, see [other] Amp::apply_gain() above for details

	double lpf = initial;
        for (pframes_t nx = 0; nx < nframes; ++nx) {
                buffer[nx] *= lpf;
		lpf += a * (target - lpf);
        }

	if (fabs (lpf - target) < GAIN_COEFF_TINY) return target;
	if (fabs (lpf) < GAIN_COEFF_TINY) return GAIN_COEFF_ZERO;
	return lpf;
}

void
Amp::apply_simple_gain (BufferSet& bufs, framecnt_t nframes, gain_t target, bool midi_amp)
{
	if (fabsf (target) < GAIN_COEFF_SMALL) {

		if (midi_amp) {
			/* don't Trim midi velocity -- only relevant for Midi on Audio tracks */
			for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {
				MidiBuffer& mb (*i);

				for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ++m) {
					Evoral::Event<MidiBuffer::TimeType> ev = *m;
					if (ev.is_note_on()) {
						ev.set_velocity (0);
					}
				}
			}
		}

		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
			memset (i->data(), 0, sizeof (Sample) * nframes);
		}

	} else if (target != GAIN_COEFF_UNITY) {

		if (midi_amp) {
			/* don't Trim midi velocity -- only relevant for Midi on Audio tracks */
			for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {
				MidiBuffer& mb (*i);

				for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ++m) {
					Evoral::Event<MidiBuffer::TimeType> ev = *m;
					if (ev.is_note_on()) {
						scale_midi_velocity(ev, fabsf (target));
					}
				}
			}
		}

		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
			apply_gain_to_buffer (i->data(), nframes, target);
		}
	}
}

void
Amp::apply_simple_gain (AudioBuffer& buf, framecnt_t nframes, gain_t target)
{
	if (fabsf (target) < GAIN_COEFF_SMALL) {
                memset (buf.data(), 0, sizeof (Sample) * nframes);
	} else if (target != GAIN_COEFF_UNITY) {
                apply_gain_to_buffer (buf.data(), nframes, target);
	}
}

XMLNode&
Amp::state (bool full_state)
{
	XMLNode& node (Processor::state (full_state));
	node.set_property("type", _gain_control->parameter().type() == GainAutomation ? "amp" : "trim");
	node.add_child_nocopy (_gain_control->get_state());

	return node;
}

int
Amp::set_state (const XMLNode& node, int version)
{
	XMLNode* gain_node;

	Processor::set_state (node, version);

	if ((gain_node = node.child (Controllable::xml_node_name.c_str ())) != 0) {
		_gain_control->set_state (*gain_node, version);
	}

	return 0;
}

/** Write gain automation for this cycle into the buffer previously passed in to
 *  set_gain_automation_buffer (if we are in automation playback mode and the
 *  transport is rolling).
 */
void
Amp::setup_gain_automation (framepos_t start_frame, framepos_t end_frame, framecnt_t nframes)
{
	Glib::Threads::Mutex::Lock am (control_lock(), Glib::Threads::TRY_LOCK);

	if (am.locked()
	    && (_session.transport_rolling() || _session.bounce_processing())
	    && _gain_control->automation_playback())
	{
		assert (_gain_automation_buffer);
		_apply_gain_automation = _gain_control->list()->curve().rt_safe_get_vector (
			start_frame, end_frame, _gain_automation_buffer, nframes);

		/* XXX the future requires a way to automate the control master
		 * and merge its own automation vector/curve with this one. We
		 * don't have a way to do that just yet, so for now, just get
		 * the master's current gain and scale our own automation
		 * vector/curve by this value.
		 */

		if (_gain_control->slaved()) {
			const double master_gain = _gain_control->get_masters_value ();
			if (master_gain != 1.0) {
				apply_gain_to_buffer (_gain_automation_buffer, nframes, master_gain);
			}
		}

		if (start_frame != _current_automation_frame && _session.bounce_processing ()) {
			_current_gain = _gain_automation_buffer[0];
		}
		_current_automation_frame = end_frame;
	} else {
		_apply_gain_automation = false;
		_current_automation_frame = INT64_MAX;
	}
}

bool
Amp::visible() const
{
	return true;
}

std::string
Amp::value_as_string (boost::shared_ptr<const AutomationControl> ac) const
{
	if (ac == _gain_control) {
		char buffer[32];
		snprintf (buffer, sizeof (buffer), _("%.2fdB"), ac->internal_to_user (ac->get_value ()));
		return buffer;
	}

	return Automatable::value_as_string (ac);
}

/** Sets up the buffer that setup_gain_automation and ::run will use for
 *  gain automationc curves.  Must be called before setup_gain_automation,
 *  and must be called with process lock held.
 */

void
Amp::set_gain_automation_buffer (gain_t* g)
{
	_gain_automation_buffer = g;
}
