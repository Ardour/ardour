/*
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#include <assert.h>
#include <cmath>

#include "pbd/compose.h"

#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/debug.h"
#include "ardour/delayline.h"
#include "ardour/midi_buffer.h"
#include "ardour/runtime_functions.h"
#include "ardour/rc_configuration.h"

#define MAX_BUFFER_SIZE 8192

using namespace std;
using namespace PBD;
using namespace ARDOUR;

DelayLine::DelayLine (Session& s, const std::string& name)
	: Processor (s, string_compose ("latcomp-%1-%2", name, this), Config->get_default_automation_time_domain())
	, _bsiz (0)
	, _delay (0)
	, _pending_delay (0)
	, _roff (0)
	, _woff (0)
	, _pending_flush (false)
{
}

DelayLine::~DelayLine ()
{
}

bool
DelayLine::set_name (const string& name)
{
	return Processor::set_name (string_compose ("latcomp-%1-%2", name, this));
}

#define FADE_LEN (128)

void
DelayLine::run (BufferSet& bufs, samplepos_t /* start_sample */, samplepos_t /* end_sample */, double /* speed */, pframes_t n_samples, bool)
{
#ifndef NDEBUG
	Glib::Threads::Mutex::Lock lm (_set_delay_mutex, Glib::Threads::TRY_LOCK);
	assert (lm.locked ());
#endif
	assert (n_samples <= MAX_BUFFER_SIZE);

	const sampleoffset_t pending_delay = _pending_delay;
	sampleoffset_t delay_diff = _delay - pending_delay;
	const bool pending_flush = _pending_flush;

	if (delay_diff == 0 && _delay == 0) {
		return;
	}

	_pending_flush = false;

	// TODO handle pending_flush.

	/* Audio buffers */
	if (_buf.size () == bufs.count ().n_audio () && _buf.size () > 0) {

		/* handle delay-changes first */
		if (delay_diff < 0) {
			/* delay increases: fade out, insert silence, fade-in */
			const samplecnt_t fade_in_len = std::min (n_samples, (pframes_t)FADE_LEN);
			samplecnt_t fade_out_len;

			if (_delay < FADE_LEN) {
				/* if old delay was 0 or smaller than new-delay, add some data to fade.
				 * Add at most (FADE_LEN - _delay) samples, but no more than -delay_diff
				 */
				samplecnt_t add = std::min ((samplecnt_t)FADE_LEN - _delay, (samplecnt_t) -delay_diff);
				fade_out_len = std::min (_delay + add, (samplecnt_t)FADE_LEN);

				if (add > 0) {
					AudioDlyBuf::iterator bi = _buf.begin ();
					for (BufferSet::audio_iterator i = bufs.audio_begin (); i != bufs.audio_end (); ++i, ++bi) {
						Sample* rb = (*bi).get ();
						write_to_rb (rb, i->data (), add);
					}
					_woff = (_woff + add) & _bsiz_mask;
					delay_diff += add;
				}
			} else {
				fade_out_len = FADE_LEN;
			}

			/* fade-out, end of previously written data */
			for (AudioDlyBuf::iterator i = _buf.begin(); i != _buf.end (); ++i) {
				Sample* rb = (*i).get ();
				for (uint32_t s = 0; s < fade_out_len; ++s) {
					sampleoffset_t off = (_woff + _bsiz - s) & _bsiz_mask;
					rb[off] *= s / (float) fade_out_len;
				}
				/* clear data in rb */
				// TODO optimize this using memset
				for (uint32_t s = 0; s < -delay_diff; ++s) {
					sampleoffset_t off = (_woff + _bsiz + s) & _bsiz_mask;
					rb[off] = 0.f;
				}
			}

			_woff = (_woff - delay_diff) & _bsiz_mask;

			/* fade-in, directly apply to input buffer */
			for (BufferSet::audio_iterator i = bufs.audio_begin (); i != bufs.audio_end (); ++i) {
				Sample* src = i->data ();
				for (uint32_t s = 0; s < fade_in_len; ++s) {
					src[s] *= s / (float) fade_in_len;
				}
			}
		} else if (delay_diff > 0) {
			/* delay decreases: cross-fade, if possible */
			const samplecnt_t fade_out_len = std::min (_delay, (samplecnt_t)FADE_LEN);
			const samplecnt_t fade_in_len = std::min (n_samples, (pframes_t)FADE_LEN);
			const samplecnt_t xfade_len = std::min (fade_out_len, fade_in_len);

			AudioDlyBuf::iterator bi = _buf.begin ();
			for (BufferSet::audio_iterator i = bufs.audio_begin (); i != bufs.audio_end (); ++i, ++bi) {
				Sample* rb = (*bi).get ();
				Sample* src = i->data ();

				// TODO consider handling fade_out & fade_in separately
				// if fade_out_len < fade_in_len.
				for (uint32_t s = 0; s < xfade_len; ++s) {
					sampleoffset_t off = (_roff + s) & _bsiz_mask;
					const gain_t g = s / (float) xfade_len;
					src[s] *= g;
					src[s] += (1.f - g) * rb[off];
				}
			}

#ifndef NDEBUG
			sampleoffset_t check = (_roff + delay_diff) & _bsiz_mask;
#endif
			_roff = (_woff + _bsiz - pending_delay) & _bsiz_mask;
#ifndef NDEBUG
			assert (_roff == check);
#endif
		}

		/* set new delay */
		_delay = pending_delay;

		if (pending_flush) {
			/* fade out data after read-pointer, clear buffer until write-pointer */
			const samplecnt_t fade_out_len = std::min (_delay, (samplecnt_t)FADE_LEN);

			for (AudioDlyBuf::iterator i = _buf.begin(); i != _buf.end (); ++i) {
				Sample* rb = (*i).get ();
				uint32_t s = 0;
				for (; s < fade_out_len; ++s) {
					sampleoffset_t off = (_roff + s) & _bsiz_mask;
					rb[off] *= 1. - (s / (float) fade_out_len);
				}
				for (; s < _delay; ++s) {
					sampleoffset_t off = (_roff + s) & _bsiz_mask;
					rb[off] = 0;
				}
				assert (_woff == ((_roff + s) & _bsiz_mask));
			}
			// TODO consider adding a fade-in to bufs
		}

		/* delay audio buffers */
		assert (_delay == ((_woff - _roff + _bsiz) & _bsiz_mask));
		AudioDlyBuf::iterator bi = _buf.begin ();
		if (_delay == 0) {
			/* do nothing */
		} else if (n_samples <= _delay) {
			/* write all samples to rb, read all from rb */
			for (BufferSet::audio_iterator i = bufs.audio_begin (); i != bufs.audio_end (); ++i, ++bi) {
				Sample* rb = (*bi).get ();
				write_to_rb (rb, i->data (), n_samples);
				read_from_rb (rb, i->data (), n_samples);
			}
			_roff = (_roff + n_samples) & _bsiz_mask;
			_woff = (_woff + n_samples) & _bsiz_mask;
		} else {
			/* only write _delay samples to ringbuffer, memmove buffer */
			samplecnt_t tail = n_samples - _delay;
			for (BufferSet::audio_iterator i = bufs.audio_begin (); i != bufs.audio_end (); ++i, ++bi) {
				Sample* rb = (*bi).get ();
				Sample* src = i->data ();
				write_to_rb (rb, &src[tail], _delay);
				memmove (&src[_delay], src, tail * sizeof(Sample));
				read_from_rb (rb, src, _delay);
			}
			_roff = (_roff + _delay) & _bsiz_mask;
			_woff = (_woff + _delay) & _bsiz_mask;
		}
	} else {
		/* set new delay for MIDI only */
		_delay = pending_delay;

		/* prepare for the case that an audio-port is added */
		_woff = _delay;
		_roff = 0;
	}

	if (_midi_buf.get ()) {
		for (BufferSet::midi_iterator i = bufs.midi_begin (); i != bufs.midi_end (); ++i) {
			if (i != bufs.midi_begin ()) { break; } // XXX only one buffer for now

			MidiBuffer* dly = _midi_buf.get ();
			MidiBuffer& mb (*i);
			if (pending_flush) {
				dly->silence (n_samples);
			}

			// If the delay time changes, iterate over all events in the dly-buffer
			// and adjust the time in-place. <= 0 becomes 0.
			//
			// iterate over all events in dly-buffer and subtract one cycle
			// (n_samples) from the timestamp, bringing them closer to de-queue.
			for (MidiBuffer::iterator m = dly->begin (); m != dly->end (); ++m) {
				MidiBuffer::TimeType *t = m.timeptr ();
				if (*t > n_samples + delay_diff) {
					*t -= n_samples + delay_diff;
				} else {
					*t = 0;
				}
			}

			if (_delay != 0) {
				// delay events in current-buffer, in place.
				for (MidiBuffer::iterator m = mb.begin (); m != mb.end (); ++m) {
					MidiBuffer::TimeType *t = m.timeptr ();
					*t += _delay;
				}
			}

			// move events from dly-buffer into current-buffer until n_samples
			// and remove them from the dly-buffer
			for (MidiBuffer::iterator m = dly->begin (); m != dly->end ();) {
				const Evoral::Event<MidiBuffer::TimeType> ev (*m, false);
				if (ev.time () >= n_samples) {
					break;
				}
				mb.insert_event (ev);
				m = dly->erase (m);
			}

			/* For now, this is only relevant if there is there's a positive delay.
			 * In the future this could also be used to delay 'too early' events
			 * (ie '_global_port_buffer_offset + _port_buffer_offset' - midi_port.cc)
			 */
			if (_delay != 0) {
				// move events after n_samples from current-buffer into dly-buffer
				// and trim current-buffer after n_samples
				for (MidiBuffer::iterator m = mb.begin (); m != mb.end ();) {
					const Evoral::Event<MidiBuffer::TimeType> ev (*m, false);
					if (ev.time () < n_samples) {
						++m;
						continue;
					}
					dly->insert_event (ev);
					m = mb.erase (m);
				}
			}
		}
	}
}

bool
DelayLine::set_delay (samplecnt_t signal_delay)
{
#ifndef NDEBUG
	Glib::Threads::Mutex::Lock lm (_set_delay_mutex, Glib::Threads::TRY_LOCK);
	assert (lm.locked ());
#endif

	if (signal_delay < 0) {
		signal_delay = 0;
		cerr << "WARNING: latency compensation is not possible.\n";
	}

	if (signal_delay == _pending_delay) {
		DEBUG_TRACE (DEBUG::LatencyDelayLine,
				string_compose ("%1 set_delay - no change: %2 samples for %3 channels\n",
					name (), signal_delay, _configured_output.n_audio ()));
		return false;
	}

	DEBUG_TRACE (DEBUG::LatencyDelayLine,
			string_compose ("%1 set_delay to %2 samples for %3 channels\n",
				name (), signal_delay, _configured_output.n_audio ()));

	if (signal_delay + MAX_BUFFER_SIZE + 1 > _bsiz) {
		allocate_pending_buffers (signal_delay, _configured_output);
	}

	_pending_delay = signal_delay;
	return true;
}

bool
DelayLine::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in;
	return true;
}

void
DelayLine::allocate_pending_buffers (samplecnt_t signal_delay, ChanCount const& cc)
{
	assert (signal_delay >= 0);
#if 1
	/* If no buffers are required, don't allocate any.
	 * This may backfire later, allocating buffers on demand
	 * may take time and cause xruns.
	 *
	 * The default buffersize is 4 * 16kB and - once allocated -
	 * usually sufficies for the lifetime of the delayline instance.
	 */
	if (signal_delay == _pending_delay && signal_delay == 0) {
		return;
	}
#endif
	samplecnt_t rbs = signal_delay + MAX_BUFFER_SIZE + 1;
	rbs = std::max (_bsiz, rbs);

	uint64_t power_of_two;
	for (power_of_two = 1; 1 << power_of_two < rbs; ++power_of_two) {}
	rbs = 1 << power_of_two;

	if (cc.n_audio () == _buf.size () && _bsiz == rbs) {
		return;
	}

	if (cc.n_audio () == 0) {
		return;
	}

	AudioDlyBuf pending_buf;
	for (uint32_t i = 0; i < cc.n_audio (); ++i) {
		boost::shared_array<Sample> b (new Sample[rbs]);
		pending_buf.push_back (b);
		memset (b.get (), 0, rbs * sizeof (Sample));
	}

	AudioDlyBuf::iterator bo = _buf.begin ();
	AudioDlyBuf::iterator bn = pending_buf.begin ();

	sampleoffset_t offset = (_roff <= _woff) ? 0 : rbs - _bsiz;

	for (; bo != _buf.end () && bn != pending_buf.end(); ++bo, ++bn) {
		Sample* rbo = (*bo).get ();
		Sample* rbn = (*bn).get ();
		if (_roff == _woff) {
			continue;
		} else if (_roff < _woff) {
			/* copy data between _roff .. _woff to new buffer */
			copy_vector (&rbn[_roff], &rbo[_roff], _woff - _roff);
		} else {
			/* copy data between _roff .. old_size to end of new buffer, increment _roff
			 * copy data from 0.._woff to beginning of new buffer
			 */
			copy_vector (&rbn[_roff + offset], &rbo[_roff], _bsiz - _roff);
			copy_vector (rbn, rbo, _woff);
		}
	}

	assert (signal_delay >= _pending_delay);
	assert ((_roff <= ((_woff + signal_delay - _pending_delay) & (rbs -1))) || offset > 0);
	_roff += offset;
	assert (_roff < rbs);

	_bsiz = rbs;
	_bsiz_mask = _bsiz - 1;
	_buf.swap (pending_buf);
}

bool
DelayLine::configure_io (ChanCount in, ChanCount out)
{
#ifndef NDEBUG
	Glib::Threads::Mutex::Lock lm (_set_delay_mutex, Glib::Threads::TRY_LOCK);
	assert (lm.locked ());
#endif

	if (out != in) { // always 1:1
		return false;
	}

	if (_configured_output != out) {
		allocate_pending_buffers (_pending_delay, out);
	}

	DEBUG_TRACE (DEBUG::LatencyDelayLine,
			string_compose ("configure IO: %1 Ain: %2 Aout: %3 Min: %4 Mout: %5\n",
				name (), in.n_audio (), out.n_audio (), in.n_midi (), out.n_midi ()));

	// TODO support multiple midi buffers
	if (in.n_midi () > 0 && !_midi_buf) {
		_midi_buf.reset (new MidiBuffer (16384));
	}
#ifndef NDEBUG
	lm.release ();
#endif

	return Processor::configure_io (in, out);
}

void
DelayLine::flush ()
{
	_pending_flush = true;
}

XMLNode&
DelayLine::state ()
{
	XMLNode& node (Processor::state ());
	node.set_property ("type", "delay");
	return node;
}

void
DelayLine::write_to_rb (Sample* rb, Sample* src, samplecnt_t n_samples)
{
	assert (n_samples < _bsiz);
	if (_woff + n_samples < _bsiz) {
		copy_vector (&rb[_woff], src, n_samples);
	} else {
		const samplecnt_t s0 = _bsiz - _woff;
		const samplecnt_t s1 = n_samples - s0;

		copy_vector (&rb[_woff], src, s0);
		copy_vector (rb, &src[s0], s1);
	}
}

void
DelayLine::read_from_rb (Sample* rb, Sample* dst, samplecnt_t n_samples)
{
	assert (n_samples < _bsiz);
	if (_roff + n_samples < _bsiz) {
		copy_vector (dst, &rb[_roff], n_samples);
	} else {
		const samplecnt_t s0 = _bsiz - _roff;
		const samplecnt_t s1 = n_samples - s0;

		copy_vector (dst, &rb[_roff], s0);
		copy_vector (&dst[s0], rb, s1);
	}
}
