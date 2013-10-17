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

#ifndef __libardour_midi_operator_h__
#define __libardour_midi_operator_h__

#include <vector>
#include <string>

#include "evoral/types.hpp"
#include "evoral/Sequence.hpp"

class Command;

namespace ARDOUR {

class MidiModel;

class LIBARDOUR_API MidiOperator {
  public:
	MidiOperator () {}
	virtual ~MidiOperator() {}

	virtual Command* operator() (boost::shared_ptr<ARDOUR::MidiModel>,
	                             double,
	                             std::vector<Evoral::Sequence<Evoral::MusicalTime>::Notes>&) = 0;
	virtual std::string name() const = 0;
};

} /* namespace */

#endif /* __libardour_midi_operator_h__ */
