/*
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2010-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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

#include <algorithm>
#include <iostream>

#include "ardour/audioengine.h"
#include "ardour/buffer_set.h"
#include "ardour/thread_buffers.h"

using namespace ARDOUR;
using namespace std;

ThreadBuffers::ThreadBuffers ()
	: silent_buffers (new BufferSet)
	, scratch_buffers (new BufferSet)
	, noinplace_buffers (new BufferSet)
	, route_buffers (new BufferSet)
	, mix_buffers (new BufferSet)
	, gain_automation_buffer (0)
	, trim_automation_buffer (0)
	, send_gain_automation_buffer (0)
	, scratch_automation_buffer (0)
	, pan_automation_buffer (0)
	, npan_buffers (0)
{
}

void
ThreadBuffers::ensure_buffers (ChanCount howmany, size_t custom)
{
	 // std::cerr << "ThreadBuffers " << this << " resize buffers with count = " << howmany << " size = " << custom << std::endl;

	/* this is all protected by the process lock in the Session */

	/* we always need at least 1 midi buffer */
	if (howmany.n_midi () < 1) {
		howmany.set_midi (1);
	}

	AudioEngine* _engine = AudioEngine::instance ();

	for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
		size_t count = std::max (scratch_buffers->available ().get (*t), howmany.get (*t));
		size_t size;
		if (custom > 0) {
			size = custom;
		} else {
			size = (*t == DataType::MIDI)
			           ? _engine->raw_buffer_size (*t)
			           : _engine->raw_buffer_size (*t) / sizeof (Sample);
		}

		scratch_buffers->ensure_buffers (*t, count, size);
		noinplace_buffers->ensure_buffers (*t, count, size);
		mix_buffers->ensure_buffers (*t, count, size);
		silent_buffers->ensure_buffers (*t, count, size);
		route_buffers->ensure_buffers (*t, count, size);
	}

	size_t audio_buffer_size = custom > 0 ? custom : _engine->raw_buffer_size (DataType::AUDIO) / sizeof (Sample);

	delete[] gain_automation_buffer;
	gain_automation_buffer = new gain_t[audio_buffer_size];
	delete[] trim_automation_buffer;
	trim_automation_buffer = new gain_t[audio_buffer_size];
	delete[] send_gain_automation_buffer;
	send_gain_automation_buffer = new gain_t[audio_buffer_size];
	delete[] scratch_automation_buffer;
	scratch_automation_buffer = new gain_t[audio_buffer_size];

	allocate_pan_automation_buffers (audio_buffer_size, howmany.n_audio (), false);
}

void
ThreadBuffers::allocate_pan_automation_buffers (samplecnt_t nframes, uint32_t howmany, bool force)
{
	/* we always need at least 2 pan buffers */

	howmany = max (2U, howmany);

	if (!force && howmany <= npan_buffers) {
		return;
	}

	if (pan_automation_buffer) {
		for (uint32_t i = 0; i < npan_buffers; ++i) {
			delete[] pan_automation_buffer[i];
		}

		delete[] pan_automation_buffer;
	}

	pan_automation_buffer = new pan_t*[howmany];

	for (uint32_t i = 0; i < howmany; ++i) {
		pan_automation_buffer[i] = new pan_t[nframes];
	}

	npan_buffers = howmany;
}
