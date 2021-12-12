/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/segment_descriptor.h"

using namespace ARDOUR;
using namespace Temporal;

SegmentDescriptor::SegmentDescriptor ()
	: _time_domain (AudioTime)
	, _position_samples (0)
	, _duration_samples (0)
	, _tempo (120, 4)
	, _meter (4, 4)
{
}

void
SegmentDescriptor::set_position (samplepos_t s)
{
	if (_time_domain != AudioTime) {
		/* XXX error */
		return;
	}
	_position_samples = s;
}

void
SegmentDescriptor::set_position (Temporal::Beats const & b)
{
	if (_time_domain != BeatTime) {
		/* XXX error */
		return;
	}
	_position_beats = b;
}

void
SegmentDescriptor::set_duration (samplepos_t s)
{
	if (_time_domain != AudioTime) {
		/* XXX error */
		return;
	}
	_position_samples = s;
}

void
SegmentDescriptor::set_duration (Temporal::Beats const & b)
{
	if (_time_domain != BeatTime) {
		/* XXX error */
		return;
	}
	_duration_beats = b;
}

void
SegmentDescriptor::set_extent (Temporal::Beats const & p, Temporal::Beats const & d)
{
	_time_domain = BeatTime;
	_position_beats = p;
	_duration_beats = d;
}

void
SegmentDescriptor::set_extent (samplepos_t p, samplecnt_t d)
{
	_time_domain = AudioTime;
	_position_samples = p;
	_duration_samples = d;
}

timecnt_t
SegmentDescriptor::extent() const
{
	if (_time_domain == BeatTime) {
		return timecnt_t (_duration_beats, timepos_t (_position_beats));
	}

	return timecnt_t (_duration_samples, timepos_t (_position_samples));
}

void
SegmentDescriptor::set_tempo (Tempo const & t)
{
	_tempo = t;
}

void
SegmentDescriptor::set_meter (Meter const & m)
{
	_meter = m;
}
