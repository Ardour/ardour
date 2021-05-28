/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include <cassert>

#include "pbd/malign.h"

#include "ardour/audio_buffer.h"
#include "ardour/audioengine.h"
#include "ardour/audio_port.h"
#include "ardour/data_type.h"
#include "ardour/port_engine.h"
#include "ardour/rc_configuration.h"

using namespace ARDOUR;
using namespace std;

#define ENGINE AudioEngine::instance()
#define port_engine AudioEngine::instance()->port_engine()

AudioPort::AudioPort (const std::string& name, PortFlags flags)
	: Port (name, DataType::AUDIO, flags)
	, _buffer (new AudioBuffer (0))
	, _data (0)
{
	assert (name.find_first_of (':') == string::npos);
	_src.setup (_resampler_quality);
	_src.set_rrfilt (10);
}

AudioPort::~AudioPort ()
{
	if (_data) cache_aligned_free (_data);
	delete _buffer;
}

void
AudioPort::set_buffer_size (pframes_t nframes)
{
	if (_data) cache_aligned_free (_data);
	cache_aligned_malloc ((void**) &_data, sizeof (Sample) * lrint (floor (nframes * Config->get_max_transport_speed())));
}

void
AudioPort::cycle_start (pframes_t nframes)
{
	/* caller must hold process lock */
	Port::cycle_start (nframes);

	if (sends_output()) {
		_buffer->prepare ();
	} else if (!externally_connected ()) {
		/* ardour internal port, just silence input, don't resample */
		// TODO reset resampler only once
		_src.reset ();
		memset (_data, 0, _cycle_nframes * sizeof (float));
	} else {
		_src.inp_data  = (float*)port_engine.get_buffer (_port_handle, nframes);
		_src.inp_count = nframes;
		_src.out_count = _cycle_nframes;
		_src.set_rratio (_cycle_nframes / (double)nframes);
		_src.out_data  = _data;
		_src.process ();
		while (_src.out_count > 0) {
			*_src.out_data =  _src.out_data[-1];
			++_src.out_data;
			--_src.out_count;
		}
	}
}

void
AudioPort::cycle_end (pframes_t nframes)
{
	if (sends_output() && !_buffer->written() && _port_handle) {
		if (!_buffer->data (0)) {
			get_audio_buffer (nframes);
		}
		if (_buffer->capacity() >= nframes) {
			_buffer->silence (nframes);
		}
	}

	if (sends_output() && _port_handle) {

		if (!externally_connected ()) {
			/* ardour internal port, data goes nowhere, skip resampling */
			// TODO reset resampler only once
			_src.reset ();
			return;
		}

		_src.inp_count = _cycle_nframes;
		_src.out_count = nframes;
		_src.set_rratio (nframes / (double)_cycle_nframes);
		_src.inp_data  = _data;
		_src.out_data  = (float*)port_engine.get_buffer (_port_handle, nframes);
		_src.process ();
		while (_src.out_count > 0) {
			*_src.out_data =  _src.out_data[-1];
			++_src.out_data;
			--_src.out_count;
		}
	}
}

void
AudioPort::cycle_split ()
{
}

AudioBuffer&
AudioPort::get_audio_buffer (pframes_t nframes)
{
	/* caller must hold process lock */
	assert (_port_handle);

	Sample* addr;

	if (!externally_connected () || (0 != (flags() & TransportSyncPort))) {
		addr = (Sample *) port_engine.get_buffer (_port_handle, nframes);
	} else {
		/* _data was read and resampled as necessary in ::cycle_start */
		addr = &_data[_global_port_buffer_offset];
	}

	_buffer->set_data (addr, nframes);

	return *_buffer;
}

Sample*
AudioPort::engine_get_whole_audio_buffer ()
{
	/* caller must hold process lock */
	assert (_port_handle);
	return (Sample *) port_engine.get_buffer (_port_handle, ENGINE->samples_per_cycle());
}
