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

#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

#include "evoral/Curve.hpp"

#include "ardour/amp.h"
#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/midi_buffer.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using std::min;

/* gain range of -inf to +6dB, default 0dB */
const float Amp::max_gain_coefficient = 1.99526231f;

Amp::Amp (Session& s)
	: Processor(s, "Amp")
	, _apply_gain(true)
	, _apply_gain_automation(false)
	, _current_gain(1.0)
	, _gain_automation_buffer(0)
{
	Evoral::Parameter p (GainAutomation);
	p.set_range (0, max_gain_coefficient, 1, false);
	boost::shared_ptr<AutomationList> gl (new AutomationList (p));
	_gain_control = boost::shared_ptr<GainControl> (new GainControl (X_("gaincontrol"), s, this, p, gl));
	_gain_control->set_flags (Controllable::GainLike);

	add_control(_gain_control);
}

std::string
Amp::display_name() const
{
	return _("Fader");
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
Amp::run (BufferSet& bufs, framepos_t /*start_frame*/, framepos_t /*end_frame*/, pframes_t nframes, bool)
{
	if (!_active && !_pending_active) {
		return;
	}

	if (_apply_gain) {

		if (_apply_gain_automation) {

			gain_t* gab = _gain_automation_buffer;
			assert (gab);

			for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
				Sample* const sp = i->data();
				for (pframes_t nx = 0; nx < nframes; ++nx) {
					sp[nx] *= gab[nx];
				}
			}

			_current_gain = gab[nframes-1];

		} else { /* manual (scalar) gain */

			gain_t const dg = _gain_control->user_double();

			if (_current_gain != dg) {

				Amp::apply_gain (bufs, nframes, _current_gain, dg);
				_current_gain = dg;

			} else if (_current_gain != 1.0f) {

				/* gain has not changed, but its non-unity
				*/

				for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {

					MidiBuffer& mb (*i);
					const float midi_velocity_factor = gain_coefficient_to_midi_velocity_factor (_current_gain);
					
					for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ++m) {
						Evoral::MIDIEvent<MidiBuffer::TimeType> ev = *m;
						if (ev.is_note_on()) {
							ev.scale_velocity (midi_velocity_factor);
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

void
Amp::apply_gain (BufferSet& bufs, framecnt_t nframes, gain_t initial, gain_t target)
{
        /** Apply a (potentially) declicked gain to the buffers of @a bufs
	 */

	if (nframes == 0 || bufs.count().n_total() == 0) {
		return;
	}

	// if we don't need to declick, defer to apply_simple_gain
	if (initial == target) {
		apply_simple_gain (bufs, nframes, target);
		return;
	}

	const framecnt_t declick = std::min ((framecnt_t) 128, nframes);
	gain_t         delta;
	double         fractional_shift = -1.0/declick;
	double         fractional_pos;

	if (target < initial) {
		/* fade out: remove more and more of delta from initial */
		delta = -(initial - target);
	} else {
		/* fade in: add more and more of delta from initial */
		delta = target - initial;
	}

	/* MIDI Gain */

	for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {

		MidiBuffer& mb (*i);

		for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ++m) {
			Evoral::MIDIEvent<MidiBuffer::TimeType> ev = *m;

			if (ev.is_note_on()) {
				const gain_t scale = delta * (ev.time()/(double) nframes);
				ev.scale_velocity (gain_coefficient_to_midi_velocity_factor (initial+scale));
			}
		}
	}

	/* Audio Gain */

	for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
		Sample* const buffer = i->data();

		fractional_pos = 1.0;

		for (pframes_t nx = 0; nx < declick; ++nx) {
			buffer[nx] *= (initial + (delta * (0.5 + 0.5 * cos (M_PI * fractional_pos))));
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
Amp::declick (BufferSet& bufs, framecnt_t nframes, int dir)
{
        /* Almost exactly like ::apply_gain() but skips MIDI buffers and has fixed initial+target
           values.
         */

	if (nframes == 0 || bufs.count().n_total() == 0) {
		return;
	}

	const framecnt_t declick = std::min ((framecnt_t) 128, nframes);
	gain_t         delta, initial, target;
	double         fractional_shift = -1.0/(declick-1);
	double         fractional_pos;

	if (dir < 0) {
		/* fade out: remove more and more of delta from initial */
		delta = -1.0;
                initial = 1.0;
                target = 0.0;
	} else {
		/* fade in: add more and more of delta from initial */
		delta = 1.0;
                initial = 0.0;
                target = 1.0;
	}

	/* Audio Gain */

	for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
		Sample* const buffer = i->data();

		fractional_pos = 1.0;

		for (pframes_t nx = 0; nx < declick; ++nx) {
			buffer[nx] *= (initial + (delta * (0.5 + 0.5 * cos (M_PI * fractional_pos))));
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
Amp::apply_gain (AudioBuffer& buf, framecnt_t nframes, gain_t initial, gain_t target)
{
        /** Apply a (potentially) declicked gain to the contents of @a buf
	 */

	if (nframes == 0) {
		return;
	}

	// if we don't need to declick, defer to apply_simple_gain
	if (initial == target) {
		apply_simple_gain (buf, nframes, target);
		return;
	}

	const framecnt_t declick = std::min ((framecnt_t) 128, nframes);
	gain_t         delta;
	double         fractional_shift = -1.0/declick;
	double         fractional_pos;

	if (target < initial) {
		/* fade out: remove more and more of delta from initial */
		delta = -(initial - target);
	} else {
		/* fade in: add more and more of delta from initial */
		delta = target - initial;
	}


        Sample* const buffer = buf.data();

        fractional_pos = 1.0;

        for (pframes_t nx = 0; nx < declick; ++nx) {
                buffer[nx] *= (initial + (delta * (0.5 + 0.5 * cos (M_PI * fractional_pos))));
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

void
Amp::apply_simple_gain (BufferSet& bufs, framecnt_t nframes, gain_t target)
{
	if (target == 0.0) {

		for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {
			MidiBuffer& mb (*i);

			for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ++m) {
				Evoral::MIDIEvent<MidiBuffer::TimeType> ev = *m;
				if (ev.is_note_on()) {
					ev.set_velocity (0);
				}
			}
		}

		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
			memset (i->data(), 0, sizeof (Sample) * nframes);
		}

	} else if (target != 1.0) {

		for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {
			MidiBuffer& mb (*i);
			const float midi_velocity_factor = gain_coefficient_to_midi_velocity_factor (target);

			for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ++m) {
				Evoral::MIDIEvent<MidiBuffer::TimeType> ev = *m;
				if (ev.is_note_on()) {
					ev.scale_velocity (midi_velocity_factor);
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
	if (target == 0.0) {
                memset (buf.data(), 0, sizeof (Sample) * nframes);
	} else if (target != 1.0) {
                apply_gain_to_buffer (buf.data(), nframes, target);
	}
}

void
Amp::inc_gain (gain_t factor, void *src)
{
	float desired_gain = _gain_control->user_double();

	if (desired_gain == 0.0f) {
		set_gain (0.000001f + (0.000001f * factor), src);
	} else {
		set_gain (desired_gain + (desired_gain * factor), src);
	}
}

void
Amp::set_gain (gain_t val, void *src)
{
	val = min (val, max_gain_coefficient);

	if (src != _gain_control.get()) {
		_gain_control->set_value (val);
		// bit twisty, this will come back and call us again
		// (this keeps control in sync with reality)
		return;
	}

	_gain_control->set_double (val);
	_session.set_dirty();
}

XMLNode&
Amp::state (bool full_state)
{
	XMLNode& node (Processor::state (full_state));
	node.add_property("type", "amp");
        node.add_child_nocopy (_gain_control->get_state());

	return node;
}

int
Amp::set_state (const XMLNode& node, int version)
{
        XMLNode* gain_node;

	Processor::set_state (node, version);

        if ((gain_node = node.child (Controllable::xml_node_name.c_str())) != 0) {
                _gain_control->set_state (*gain_node, version);
        }

	return 0;
}

void
Amp::GainControl::set_value (double val)
{
	// max gain at about +6dB (10.0 ^ ( 6 dB * 0.05))
	if (val > 1.99526231) {
		val = 1.99526231;
	}

	_amp->set_gain (val, this);

	AutomationControl::set_value(val);
}

double
Amp::GainControl::internal_to_interface (double v) const
{
	return gain_to_slider_position (v);
}

double
Amp::GainControl::interface_to_internal (double v) const
{
	return slider_position_to_gain (v);
}

double
Amp::GainControl::internal_to_user (double v) const
{
	return accurate_coefficient_to_dB (v);
}

/** Write gain automation for this cycle into the buffer previously passed in to
 *  set_gain_automation_buffer (if we are in automation playback mode and the
 *  transport is rolling).
 */
void
Amp::setup_gain_automation (framepos_t start_frame, framepos_t end_frame, framecnt_t nframes)
{
	Glib::Threads::Mutex::Lock am (control_lock(), Glib::Threads::TRY_LOCK);

	if (am.locked() && _session.transport_rolling() && _gain_control->automation_playback()) {
		assert (_gain_automation_buffer);
		_apply_gain_automation = _gain_control->list()->curve().rt_safe_get_vector (
			start_frame, end_frame, _gain_automation_buffer, nframes);
	} else {
		_apply_gain_automation = false;
	}
}

bool
Amp::visible() const
{
	return true;
}

std::string
Amp::value_as_string (boost::shared_ptr<AutomationControl> ac) const
{
	if (ac == _gain_control) {
		char buffer[32];
		snprintf (buffer, sizeof (buffer), "%.2fdB", ac->internal_to_user (ac->get_value ()));
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
