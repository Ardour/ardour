/*
    Copyright (C) 2006, 2013 Paul Davis
    Copyright (C) 2013, 2014 Robin Gareus <robin@gareus.org>

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

#include <assert.h>
#include <cmath>

#include "pbd/compose.h"

#include "ardour/debug.h"
#include "ardour/audio_buffer.h"
#include "ardour/midi_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/delayline.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

DelayLine::DelayLine (Session& s, const std::string& name)
    : Processor (s, string_compose ("latency-compensation-%1", name))
		, _delay(0)
		, _pending_delay(0)
		, _bsiz(0)
		, _pending_bsiz(0)
		, _roff(0)
		, _woff(0)
		, _pending_flush(false)
{
}

DelayLine::~DelayLine ()
{
}

#define FADE_LEN (16)
void
DelayLine::run (BufferSet& bufs, framepos_t /* start_frame */, framepos_t /* end_frame */, double /* speed */, pframes_t nsamples, bool)
{
	const uint32_t chn = _configured_output.n_audio();
	pframes_t p0 = 0;
	uint32_t c;

	const frameoffset_t pending_delay = _pending_delay;
	const frameoffset_t delay_diff = _delay - pending_delay;
	const bool pending_flush = _pending_flush;
	_pending_flush = false;

	/* run() and set_delay() may be called in parallel by
	 * different threads.
	 * if a larger buffer is needed, it is allocated in
	 * set_delay(), here it is just swap'ed in place
	 */
	if (_pending_bsiz) {
		assert(_pending_bsiz >= _bsiz);

		const size_t boff = _pending_bsiz - _bsiz;
		if (_bsiz > 0) {
			/* write offset is retained. copy existing data to new buffer */
			frameoffset_t wl = _bsiz - _woff;
			memcpy(_pending_buf.get(), _buf.get(), sizeof(Sample) * _woff * chn);
			memcpy(_pending_buf.get() + (_pending_bsiz - wl) * chn, _buf.get() + _woff * chn, sizeof(Sample) * wl * chn);

			/* new buffer is all zero by default, fade into the existing data copied above */
			frameoffset_t wo = _pending_bsiz - wl;
			for (pframes_t pos = 0; pos < FADE_LEN; ++pos) {
				const gain_t gain = (gain_t)pos / (gain_t)FADE_LEN;
				for (c = 0; c < _configured_input.n_audio(); ++c) {
					_pending_buf.get()[ wo * chn + c ] *= gain;
					wo = (wo + 1) % (_pending_bsiz + 1);
				}
			}

			/* read-pointer will be moved and may up anywhere..
			 * copy current data for smooth fade-out below
			 */
			frameoffset_t roold = _roff;
			frameoffset_t ro = _roff;
			if (ro > _woff) {
				ro += boff;
			}
			ro += delay_diff;
			if (ro < 0) {
				ro -= (_pending_bsiz +1) * floor(ro / (float)(_pending_bsiz +1));
			}
			ro = ro % (_pending_bsiz + 1);
			for (pframes_t pos = 0; pos < FADE_LEN; ++pos) {
				for (c = 0; c < _configured_input.n_audio(); ++c) {
					_pending_buf.get()[ ro * chn + c ] = _buf.get()[ roold * chn + c ];
					ro = (ro + 1) % (_pending_bsiz + 1);
					roold = (roold + 1) % (_bsiz + 1);
				}
			}
		}

		if (_roff > _woff) {
			_roff += boff;
		}

		// use shared_array::swap() ??
		_buf = _pending_buf;
		_bsiz = _pending_bsiz;
		_pending_bsiz = 0;
		_pending_buf.reset();
	}

	/* there may be no buffer when delay == 0.
	 * we also need to check audio-channels in case all audio-channels
	 * were removed in which case no new buffer was allocated. */
	Sample *buf = _buf.get();
	if (buf && _configured_output.n_audio() > 0) {

		assert (_bsiz >= pending_delay);
		const framecnt_t rbs = _bsiz + 1;

		if (pending_delay != _delay || pending_flush) {
			const pframes_t fade_len = (nsamples >= FADE_LEN) ? FADE_LEN : nsamples / 2;

			DEBUG_TRACE (DEBUG::LatencyCompensation,
					string_compose ("Old %1 delay: %2 bufsiz: %3 offset-diff: %4 write-offset: %5 read-offset: %6\n",
						name(), _delay, _bsiz, ((_woff - _roff + rbs) % rbs), _woff, _roff));

			// fade out at old position
			c = 0;
			for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i, ++c) {
				Sample * const data = i->data();
				for (pframes_t pos = 0; pos < fade_len; ++pos) {
					const gain_t gain = (gain_t)(fade_len - pos) / (gain_t)fade_len;
					buf[ _woff * chn + c ] = data[ pos ];
					data[ pos ] = buf[ _roff * chn + c ] * gain;
					_roff = (_roff + 1) % rbs;
					_woff = (_woff + 1) % rbs;
				}
			}

			if (pending_flush) {
				DEBUG_TRACE (DEBUG::LatencyCompensation,
						string_compose ("Flush buffer: %1\n", name()));
				memset(buf, 0, _configured_output.n_audio() * rbs * sizeof (Sample));
			}

			// adjust read pointer
			_roff += _delay - pending_delay;

			if (_roff < 0) {
				_roff -= rbs * floor(_roff / (float)rbs);
			}
			_roff = _roff % rbs;

			// fade in at new position
			c = 0;
			for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i, ++c) {
				Sample * const data = i->data();
				for (pframes_t pos = fade_len; pos < 2 * fade_len; ++pos) {
					const gain_t gain = (gain_t)(pos - fade_len) / (gain_t)fade_len;
					buf[ _woff * chn + c ] = data[ pos ];
					data[ pos ] = buf[ _roff * chn + c ] * gain;
					_roff = (_roff + 1) % rbs;
					_woff = (_woff + 1) % rbs;
				}
			}
			p0  = 2 * fade_len;

			_delay = pending_delay;

			DEBUG_TRACE (DEBUG::LatencyCompensation,
					string_compose ("New %1 delay: %2 bufsiz: %3 offset-diff: %4 write-offset: %5 read-offset: %6\n",
						name(), _delay, _bsiz, ((_woff - _roff + rbs) % rbs), _woff, _roff));
		}

		assert(_delay == ((_woff - _roff + rbs) % rbs));

		c = 0;
		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i, ++c) {
			Sample * const data = i->data();
			for (pframes_t pos = p0; pos < nsamples; ++pos) {
				buf[ _woff * chn + c ] = data[ pos ];
				data[ pos ] = buf[ _roff * chn + c ];
				_roff = (_roff + 1) % rbs;
				_woff = (_woff + 1) % rbs;
			}
		}
	}

	if (_midi_buf.get()) {
		_delay = pending_delay;

		for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {
			if (i != bufs.midi_begin()) { break; } // XXX only one buffer for now

			MidiBuffer* dly = _midi_buf.get();
			MidiBuffer& mb (*i);
			if (pending_flush) {
				dly->silence(nsamples);
			}

			// If the delay time changes, iterate over all events in the dly-buffer
			// and adjust the time in-place. <= 0 becomes 0.
			//
			// iterate over all events in dly-buffer and subtract one cycle
			// (nsamples) from the timestamp, bringing them closer to de-queue.
			for (MidiBuffer::iterator m = dly->begin(); m != dly->end(); ++m) {
				MidiBuffer::TimeType *t = m.timeptr();
				if (*t > nsamples + delay_diff) {
					*t -= nsamples + delay_diff;
				} else {
					*t = 0;
				}
			}

			if (_delay != 0) {
				// delay events in current-buffer, in place.
				for (MidiBuffer::iterator m = mb.begin(); m != mb.end(); ++m) {
					MidiBuffer::TimeType *t = m.timeptr();
					*t += _delay;
				}
			}

			// move events from dly-buffer into current-buffer until nsamples
			// and remove them from the dly-buffer
			for (MidiBuffer::iterator m = dly->begin(); m != dly->end();) {
				const Evoral::Event<MidiBuffer::TimeType> ev (*m, false);
				if (ev.time() >= nsamples) {
					break;
				}
				mb.insert_event(ev);
				m = dly->erase(m);
			}

			/* For now, this is only relevant if there is there's a positive delay.
			 * In the future this could also be used to delay 'too early' events
			 * (ie '_global_port_buffer_offset + _port_buffer_offset' - midi_port.cc)
			 */
			if (_delay != 0) {
				// move events after nsamples from current-buffer into dly-buffer
				// and trim current-buffer after nsamples
				for (MidiBuffer::iterator m = mb.begin(); m != mb.end();) {
					const Evoral::Event<MidiBuffer::TimeType> ev (*m, false);
					if (ev.time() < nsamples) {
						++m;
						continue;
					}
					dly->insert_event(ev);
					m = mb.erase(m);
				}
			}
		}
	}

	_delay = pending_delay;
}

void
DelayLine::set_delay(framecnt_t signal_delay)
{
	if (signal_delay < 0) {
		signal_delay = 0;
		cerr << "WARNING: latency compensation is not possible.\n";
	}

	const framecnt_t rbs = signal_delay + 1;

	DEBUG_TRACE (DEBUG::LatencyCompensation,
			string_compose ("%1 set_delay to %2 samples for %3 channels\n",
				name(), signal_delay, _configured_output.n_audio()));

	if (signal_delay <= _bsiz) {
		_pending_delay = signal_delay;
		return;
	}

	if (_pending_bsiz) {
		if (_pending_bsiz < signal_delay) {
			cerr << "LatComp: buffer resize in progress. "<< name() << "pending: "<< _pending_bsiz <<" want: " << signal_delay <<"\n"; // XXX
		} else {
			_pending_delay = signal_delay;
		}
		return;
	}

	if (_configured_output.n_audio() > 0 ) {
		_pending_buf.reset(new Sample[_configured_output.n_audio() * rbs]);
		memset(_pending_buf.get(), 0, _configured_output.n_audio() * rbs * sizeof (Sample));
		_pending_bsiz = signal_delay;
	} else {
		_pending_buf.reset();
		_pending_bsiz = 0;
	}

	_pending_delay = signal_delay;

	DEBUG_TRACE (DEBUG::LatencyCompensation,
			string_compose ("allocated buffer for %1 of size %2\n",
				name(), signal_delay));
}

bool
DelayLine::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in;
	return true;
}

bool
DelayLine::configure_io (ChanCount in, ChanCount out)
{
	if (out != in) { // always 1:1
		return false;
	}

	// TODO realloc buffers if channel count changes..
	// TODO support multiple midi buffers

	DEBUG_TRACE (DEBUG::LatencyCompensation,
			string_compose ("configure IO: %1 Ain: %2 Aout: %3 Min: %4 Mout: %5\n",
				name(), in.n_audio(), out.n_audio(), in.n_midi(), out.n_midi()));

	if (in.n_midi() > 0 && !_midi_buf) {
		_midi_buf.reset(new MidiBuffer(16384));
	}

	return Processor::configure_io (in, out);
}

void
DelayLine::flush()
{
	_pending_flush = true;
}

XMLNode&
DelayLine::state (bool full_state)
{
	XMLNode& node (Processor::state (full_state));
	node.set_property("type", "delay");
	return node;
}
