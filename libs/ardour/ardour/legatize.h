/*
    Copyright (C) 2014 Paul Davis
    Author: David Robillard

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

#ifndef __ardour_legatize_h__
#define __ardour_legatize_h__

#include <string>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/midi_operator.h"

namespace ARDOUR {

/** "Legatize" nodes (extend note ends to force legato).
 *
 * This can also do "remove overlap" by setting shrink_only to true, in which
 * case note lengths will only be changed if they are long enough to overlap
 * the following note.
 */
class LIBARDOUR_API Legatize : public MidiOperator {
public:
	Legatize(bool shrink_only);
	~Legatize();

	typedef Evoral::Sequence<Evoral::Beats>::Notes Notes;

	Command* operator()(boost::shared_ptr<ARDOUR::MidiModel> model,
	                    Evoral::Beats                        position,
	                    std::vector<Notes>&                  seqs);

	std::string name() const { return std::string ("legatize"); }

private:
	bool _shrink_only;
};

} /* namespace */

#endif /* __ardour_legatize_h__ */
