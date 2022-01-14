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

#include "pbd/enumwriter.h"
#include "pbd/failed_constructor.h"
#include "pbd/i18n.h"
#include "pbd/xml++.h"

#include "temporal/tempo.h"

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


SegmentDescriptor::SegmentDescriptor (XMLNode const & node, int version)
	: _time_domain (AudioTime)
	, _position_samples (0)
	, _duration_samples (0)
	, _tempo (120, 4)
	, _meter (4, 4)
{
	if (!set_state (node, version)) {
		throw failed_constructor ();
	}
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

XMLNode&
SegmentDescriptor::get_state (void) const
{
	XMLNode* root = new XMLNode (X_("SegmentDescriptor"));

	root->set_property (X_("time-domain"), _time_domain);

	if (_time_domain == Temporal::AudioTime) {
		root->set_property (X_("position"), _position_samples);
		root->set_property (X_("duration"), _duration_samples);
	} else {
		root->set_property (X_("position"), _position_beats);
		root->set_property (X_("duration"), _duration_beats);
	}

	root->add_child_nocopy (_tempo.get_state());
	root->add_child_nocopy (_meter.get_state());

	return *root;
}

int
SegmentDescriptor::set_state (XMLNode const & node, int version)
{
	if (node.name() != X_("SegmentDescriptor")) {
		return -1;
	}

	if (node.get_property (X_("time-domain"), _time_domain)) {
		return -1;
	}

	if (_time_domain == Temporal::AudioTime) {
		if (node.get_property (X_("position"), _position_samples)) {
			return -1;
		}
		if (node.get_property (X_("duration"), _duration_samples)) {
			return -1;
		}
	} else {
		if (node.get_property (X_("position"), _position_beats)) {
			return -1;
		}
		if (node.get_property (X_("duration"), _duration_beats)) {
			return -1;
		}
	}

	XMLNode* child;

	child = node.child (Tempo::xml_node_name.c_str());

	if (!child) {
		return -1;
	}

	if (_tempo.set_state (*child, version)) {
		return -1;
	}

	child = node.child (Meter::xml_node_name.c_str());

	if (!child) {
		return -1;
	}

	if (_meter.set_state (*child, version)) {
		return -1;
	}

	return 0;
}
