/*
 * Copyright (C) 2009-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk_ardour_midi_cut_buffer_h__
#define __gtk_ardour_midi_cut_buffer_h__

#include "temporal/beats.h"

#include "ardour/automatable_sequence.h"

namespace ARDOUR {
	class Session;
}

class MidiCutBuffer : public ARDOUR::AutomatableSequence<Temporal::Beats>
{
public:
	typedef Temporal::Beats TimeType;

	MidiCutBuffer (ARDOUR::Session*);
	~MidiCutBuffer();

	TimeType origin() const { return _origin; }
	void set_origin (TimeType);

	void set (const Evoral::Sequence<TimeType>::Notes&);

private:
	TimeType _origin;
};

#endif /* __gtk_ardour_midi_cut_buffer_h__ */
