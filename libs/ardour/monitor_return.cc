/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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
#include <glibmm/threads.h>

#include "ardour/audio_port.h"
#include "ardour/audioengine.h"
#include "ardour/monitor_return.h"
#include "ardour/session.h"

using namespace ARDOUR;

MonitorReturn::MonitorReturn (Session& s, Temporal::TimeDomain td)
	: InternalReturn (s, td, "Monitor Return")
	, _nch (0)
	, _gain (1.f)
{
#if 0
	MonitorPort& mp (AudioEngine::instance()->monitor_port ());
	mp->enable ();
#endif
}

MonitorReturn::~MonitorReturn ()
{
	MonitorPort& mp (AudioEngine::instance()->monitor_port ());
	mp.clear_ports (true);
}

void
MonitorReturn::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required)
{
	InternalReturn::run (bufs, start_sample, end_sample, speed, nframes, result_required);

	MonitorPort& mp (AudioEngine::instance()->monitor_port ());

	if (mp.silent ()) {
		return;
	}

	uint32_t nch = bufs.count().n_audio ();
	if (_nch != nch) {
		_nch = nch;
		_gain = nch > 0 ? (1.f / sqrtf (nch)) : 1.f;
	}

	const AudioBuffer& bb (mp.get_audio_buffer (nframes));
	for (BufferSet::iterator b = bufs.begin (DataType::AUDIO); b != bufs.end (DataType::AUDIO); ++b) {
		AudioBuffer* ab = dynamic_cast<AudioBuffer*> (&(*b));
		ab->accumulate_with_gain_from (bb, nframes, _gain);
	}
}

XMLNode&
MonitorReturn::state ()
{
	XMLNode& node (InternalReturn::state ());
	node.set_property ("type", "monreturn");
	return node;
}
