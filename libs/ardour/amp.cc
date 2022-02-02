/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2015 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#include <cstring>
#include <cmath>
#include <algorithm>

#include "evoral/Curve.h"

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

#define GAIN_COEFF_DELTA (1e-5)

Amp::Amp (Session& s, const std::string& name, boost::shared_ptr<GainControl> gc, bool control_midi_also)
	: Processor(s, "Amp", Temporal::AudioTime)
	, _apply_gain_automation(false)
	, _current_gain(GAIN_COEFF_ZERO)
	, _current_automation_sample (INT64_MAX)
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

void
Amp::run (BufferSet& bufs, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/, double /*speed*/, pframes_t nframes, bool)
{
	if (!check_active()) {
		/* disregard potentially prepared gain-automation. */
		_apply_gain_automation = false;
		return;
	}

	if (_apply_gain_automation) {

		gain_t* gab = _gain_automation_buffer;
		assert (gab);

		/* see note in PluginInsert::connect_and_run -- effectively emit Changed signal */
		_gain_control->set_value_unchecked (gab[0]);

		if (_midi_amp) {
			for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {
				MidiBuffer& mb (*i);
				for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ++m) {
					Evoral::Event<MidiBuffer::TimeType> ev = *m;
					if (ev.is_note_on()) {
						assert(ev.time() >= 0 && ev.time() < nframes);
						ev.scale_velocity (fabsf (gab[ev.time()]));
					}
				}
			}
		}

		const gain_t a = 156.825f / (gain_t)_session.nominal_sample_rate(); // 25 Hz LPF; see Amp::apply_gain for details
		gain_t lpf = _current_gain;

		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
			Sample* const sp = i->data();
			lpf = _current_gain;
			for (pframes_t nx = 0; nx < nframes; ++nx) {
				sp[nx] *= lpf;
				lpf += a * (gab[nx] - lpf);
			}
		}

		if (fabsf (lpf) < GAIN_COEFF_SMALL) {
			_current_gain = GAIN_COEFF_ZERO;
		} else {
			_current_gain = lpf;
		}

		/* used it, don't do it again until setup_gain_automation() is
		 * called successfully.
		*/
		_apply_gain_automation = false;

	} else { /* manual (scalar) gain */

		gain_t const target_gain = _gain_control->get_value();

		if (fabsf (_current_gain - target_gain) >= GAIN_COEFF_DELTA) {

			_current_gain = Amp::apply_gain (bufs, _session.nominal_sample_rate(), nframes, _current_gain, target_gain, _midi_amp);

			/* see note in PluginInsert::connect_and_run ()
			 * set_value_unchecked() won't emit a signal since the value is effectively unchanged
			 */
			_gain_control->Changed (false, PBD::Controllable::NoGroup);

		} else if (target_gain != GAIN_COEFF_UNITY) {

			_current_gain = target_gain;
			apply_simple_gain (bufs, nframes, _current_gain, _midi_amp);

		} else {
			/* unity target gain */
			_current_gain = target_gain;
		}
	}
}

gain_t
Amp::apply_gain (BufferSet& bufs, samplecnt_t sample_rate, samplecnt_t nframes, gain_t initial, gain_t target, bool midi_amp)
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

	/* Apply Audio Gain first, calculate target LFP'ed gain coefficient
	 *
	 * Low pass filter coefficient: 1.0 - e^(-2.0 * π * f / 48000) f in Hz.
	 * for f << SR,  approx a ~= 6.2 * f / SR;
	 */
	const gain_t a = 156.825f / (gain_t)sample_rate; // 25 Hz LPF

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

	if (fabsf (rv - target) < GAIN_COEFF_DELTA) {
		rv = target;
	}

	/* MIDI Velocity scale from initial to LPF target */
	if (midi_amp) {
		/* don't Trim midi velocity -- only relevant for Midi on Audio tracks */
		for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {

			gain_t  delta;
			if (rv < initial) {
				/* fade out: remove more and more of delta from initial */
				delta = -(initial - rv);
			} else {
				/* fade in: add more and more of delta from initial */
				delta = rv - initial;
			}

			MidiBuffer& mb (*i);

			for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ) {
				Evoral::Event<MidiBuffer::TimeType> ev = *m;

				if (ev.is_note_on() || ev.is_note_off()) {
					const gain_t scale = fabsf (initial + delta * (ev.time() / (float) nframes));
					if (scale < GAIN_COEFF_SMALL) {
						m = mb.erase (m);
						continue;
					} else if (ev.is_note_on()) {
						ev.scale_velocity (scale);
					}
				}
				++m;
			}

			/* queue MIDI all-note-off when going silent */
			if (initial > GAIN_COEFF_SMALL && rv <= GAIN_COEFF_SMALL) {
				for (uint8_t channel = 0; channel <= 0xF; channel++) {
					uint8_t ev[3] = { ((uint8_t) (MIDI_CMD_CONTROL | channel)), ((uint8_t) MIDI_CTL_SUSTAIN), 0 };
					mb.push_back (nframes - 1, Evoral::MIDI_EVENT, 3, ev);
					ev[1] = MIDI_CTL_ALL_NOTES_OFF;
					mb.push_back (nframes - 1, Evoral::MIDI_EVENT, 3, ev);
				}
			}
		}
	}

	return rv;
}

gain_t
Amp::apply_gain (AudioBuffer& buf, samplecnt_t sample_rate, samplecnt_t nframes, gain_t initial, gain_t target, sampleoffset_t offset)
{
	/* Apply a (potentially) declicked gain to the contents of @a buf
	 * -- used by MonitorProcessor::run()
	 */

	if (nframes == 0) {
		return initial;
	}

	// if we don't need to declick, defer to apply_simple_gain
	if (initial == target) {
		apply_simple_gain (buf, nframes, target, offset);
		return target;
	}

	Sample* const buffer = buf.data (offset);
	const gain_t a = 156.825f / (gain_t)sample_rate; // 25 Hz LPF, see [other] Amp::apply_gain() above for details

	gain_t lpf = initial;
	for (pframes_t nx = 0; nx < nframes; ++nx) {
		buffer[nx] *= lpf;
		lpf += a * (target - lpf);
	}

	if (fabsf (lpf - target) < GAIN_COEFF_DELTA) return target;
	return lpf;
}

void
Amp::apply_simple_gain (BufferSet& bufs, samplecnt_t nframes, gain_t target, bool midi_amp)
{
	if (fabsf (target) < GAIN_COEFF_SMALL) {

		if (midi_amp) {
			for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {
				MidiBuffer& mb (*i);

				for (MidiBuffer::iterator m = mb.begin(); m != mb.end();) {
					Evoral::Event<MidiBuffer::TimeType> ev = *m;
					if (ev.is_note_on() || ev.is_note_off()) {
						m = mb.erase (m);
					} else {
						++m;
					}
				}
			}
		}

		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
			memset (i->data(), 0, sizeof (Sample) * nframes);
		}

	} else if (target != GAIN_COEFF_UNITY) {

		if (midi_amp) {
			for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {
				MidiBuffer& mb (*i);

				for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ++m) {
					Evoral::Event<MidiBuffer::TimeType> ev = *m;
					if (ev.is_note_on()) {
						ev.scale_velocity (fabsf (target));
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
Amp::apply_simple_gain (AudioBuffer& buf, samplecnt_t nframes, gain_t target, sampleoffset_t offset)
{
	if (fabsf (target) < GAIN_COEFF_SMALL) {
		memset (buf.data (offset), 0, sizeof (Sample) * nframes);
	} else if (target != GAIN_COEFF_UNITY) {
		apply_gain_to_buffer (buf.data(offset), nframes, target);
	}
}

XMLNode&
Amp::state ()
{
	XMLNode& node (Processor::state ());
	switch (_gain_control->parameter().type()) {
		case GainAutomation:
			node.set_property("type", "amp");
			break;
		case TrimAutomation:
			node.set_property("type", "trim");
			break;
		case MainOutVolume:
			node.set_property("type", "main-volume");
			break;
		default:
			assert (0);
			break;
	}
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
 *
 *  After calling this, the gain-automation buffer is valid for the next run.
 *  so make sure to call ::run() which invalidates the buffer again.
 */
void
Amp::setup_gain_automation (samplepos_t start_sample, samplepos_t end_sample, samplecnt_t nframes)
{
	Glib::Threads::Mutex::Lock am (control_lock(), Glib::Threads::TRY_LOCK);

	if (am.locked()
	    && (_session.transport_rolling() || _session.bounce_processing())
	    && _gain_control->automation_playback())
	{
		assert (_gain_automation_buffer);

		_apply_gain_automation = _gain_control->get_masters_curve ( start_sample, end_sample, _gain_automation_buffer, nframes);

		if (start_sample != _current_automation_sample && _session.bounce_processing ()) {
			_current_gain = _gain_automation_buffer[0];
		}
		_current_automation_sample = end_sample;
	} else {
		_apply_gain_automation = false;
		_current_automation_sample = INT64_MAX;
	}
}

bool
Amp::visible() const
{
	return true;
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
