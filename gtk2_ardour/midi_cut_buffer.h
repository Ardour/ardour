/*
    Copyright (C) 2009 Paul Davis

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

#ifndef __gtk_ardour_midi_cut_buffer_h__
#define __gtk_ardour_midi_cut_buffer_h__

#include "ardour/midi_model.h"

namespace ARDOUR {
	class Session;
}

class MidiCutBuffer : public ARDOUR::AutomatableSequence<ARDOUR::MidiModel::TimeType>
{
  public:
	typedef ARDOUR::MidiModel::TimeType TimeType;

	MidiCutBuffer (ARDOUR::Session&);
	~MidiCutBuffer();

	TimeType origin() const { return _origin; }
	void set_origin (TimeType);

	void set (const Evoral::Sequence<TimeType>::Notes&);

  private:
	TimeType _origin;
};

#endif /* __gtk_ardour_midi_cut_buffer_h__ */
