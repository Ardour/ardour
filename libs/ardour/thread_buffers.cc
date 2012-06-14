/*
    Copyright (C) 2010 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <iostream>
#include <algorithm>

#include "ardour/audioengine.h"
#include "ardour/buffer_set.h"
#include "ardour/thread_buffers.h"

using namespace ARDOUR;
using namespace std;

ThreadBuffers::ThreadBuffers ()
	: silent_buffers (new BufferSet)
	, scratch_buffers (new BufferSet)
	, mix_buffers (new BufferSet)
	, gain_automation_buffer (0)
	, send_gain_automation_buffer (0)
	, pan_automation_buffer (0)
	, npan_buffers (0)
{
}

void
ThreadBuffers::ensure_buffers (ChanCount howmany)
{
	// std::cerr << "ThreadBuffers " << this << " resize buffers with count = " << howmany << std::endl;

	/* this is all protected by the process lock in the Session
	 */

	/* we always need at least 1 midi buffer */
	if (howmany.n_midi() < 1) {
		howmany.set_midi(1);
	}

	if (howmany.n_audio() == 0 && howmany.n_midi() == 1) {
		return;
	}

	AudioEngine* _engine = AudioEngine::instance ();

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		size_t count = std::max (scratch_buffers->available().get(*t), howmany.get(*t));
		size_t size = _engine->raw_buffer_size (*t);

		scratch_buffers->ensure_buffers (*t, count, size);
		mix_buffers->ensure_buffers (*t, count, size);
		silent_buffers->ensure_buffers (*t, count, size);
	}

	delete [] gain_automation_buffer;
	gain_automation_buffer = new gain_t[_engine->raw_buffer_size (DataType::AUDIO)];
	delete [] send_gain_automation_buffer;
	send_gain_automation_buffer = new gain_t[_engine->raw_buffer_size (DataType::AUDIO)];

	allocate_pan_automation_buffers (_engine->raw_buffer_size (DataType::AUDIO), howmany.n_audio(), false);
}

void
ThreadBuffers::allocate_pan_automation_buffers (framecnt_t nframes, uint32_t howmany, bool force)
{
	/* we always need at least 2 pan buffers */

	howmany = max (2U, howmany);

	if (!force && howmany <= npan_buffers) {
		return;
	}

	if (pan_automation_buffer) {

		for (uint32_t i = 0; i < npan_buffers; ++i) {
			delete [] pan_automation_buffer[i];
		}

		delete [] pan_automation_buffer;
	}

	pan_automation_buffer = new pan_t*[howmany];

	for (uint32_t i = 0; i < howmany; ++i) {
		pan_automation_buffer[i] = new pan_t[nframes];
	}

	npan_buffers = howmany;
}
