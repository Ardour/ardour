/*
 * Copyright (C) 2025
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

#pragma once

#include "ardour/libardour_visibility.h"
#include "ardour/midi_model.h"
#include "ardour/midi_operator.h"

namespace ARDOUR {

/** Strum notes (add progressive timing offset to notes).
 *
 * This operator applies a progressive timing offset to selected notes,
 * creating a strumming effect where notes are offset by a specified
 * amount in either forward or backward direction.
 */
class LIBARDOUR_API Strum : public MidiOperator {
public:
	typedef Evoral::Sequence<Temporal::Beats>::NotePtr     NotePtr;
	typedef Evoral::Sequence<Temporal::Beats>::Notes       Notes;

	Strum (bool forward, bool fine);

	PBD::Command* operator() (std::shared_ptr<ARDOUR::MidiModel> model,
	                     Temporal::Beats                      position,
	                     std::vector<Notes>&                  seqs);

	std::string name () const { return std::string ("strum"); }

private:
	bool _forward;
	bool _fine;
};

} /* namespace */
