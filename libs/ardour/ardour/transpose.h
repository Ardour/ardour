/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_transpose_h__
#define __ardour_transpose_h__

#include "ardour/libardour_visibility.h"
#include "ardour/midi_model.h"
#include "ardour/midi_operator.h"

namespace ARDOUR {

class LIBARDOUR_API Transpose : public MidiOperator {
public:
	typedef Evoral::Sequence<Temporal::Beats>::NotePtr     NotePtr;
	typedef Evoral::Sequence<Temporal::Beats>::Notes       Notes;

	Transpose (int semitones);

	Command* operator() (boost::shared_ptr<ARDOUR::MidiModel> model,
	                     Temporal::Beats                      position,
	                     std::vector<Notes>&                  seqs);

	std::string name () const { return std::string ("transpose"); }

private:
	int _semitones;
};

} /* namespace */

#endif /* __ardour_transpose_h__ */
