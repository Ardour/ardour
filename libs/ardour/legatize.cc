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

#include "ardour/legatize.h"
#include "ardour/midi_model.h"

namespace ARDOUR {

Legatize::Legatize(bool shrink_only)
	: _shrink_only(shrink_only)
{}

Legatize::~Legatize ()
{}

Command*
Legatize::operator()(boost::shared_ptr<ARDOUR::MidiModel> model,
                     Evoral::Beats                        position,
                     std::vector<Legatize::Notes>&        seqs)
{
	MidiModel::NoteDiffCommand* cmd = new MidiModel::NoteDiffCommand(model, "legatize");

	for (std::vector<Legatize::Notes>::iterator s = seqs.begin(); s != seqs.end(); ++s) {
		for (Legatize::Notes::iterator i = (*s).begin(); i != (*s).end();) {
			Legatize::Notes::iterator next = i;
			if (++next == (*s).end()) {
				break;
			}

			const Evoral::Beats new_end = (*next)->time() - Evoral::Beats::tick();
			if ((*i)->end_time() > new_end ||
			    (!_shrink_only && (*i)->end_time() < new_end)) {
				const Evoral::Beats new_length(new_end - (*i)->time());
				cmd->change((*i), MidiModel::NoteDiffCommand::Length, new_length);
			}

			i = next;
		}
	}

	return cmd;
}

}  // namespace ARDOUR
