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

#ifndef __libardour_midi_operator_h__
#define __libardour_midi_operator_h__

#include <vector>
#include <string>

#include "temporal/beats.h"
#include "evoral/Sequence.h"

#include "ardour/libardour_visibility.h"

namespace PBD {
class Command;
}

namespace ARDOUR {

class MidiModel;

class LIBARDOUR_API MidiOperator {
  public:
	MidiOperator () {}
	virtual ~MidiOperator() {}

	virtual PBD::Command* operator() (std::shared_ptr<ARDOUR::MidiModel>,
	                                  Temporal::Beats,
	                                  std::vector<Evoral::Sequence<Temporal::Beats>::Notes>&) = 0;
	virtual std::string name() const = 0;
};

} /* namespace */

#endif /* __libardour_midi_operator_h__ */
