/*
    Copyright (C) 2015 Paul Davis
    Author: André Nusser

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

#include "ardour/transpose.h"

namespace ARDOUR {

Transpose::Transpose(int semitones)
	: _semitones(semitones)
{}

Command*
Transpose::operator()(boost::shared_ptr<MidiModel> model,
                      Evoral::Beats                position,
                      std::vector<Notes>&          seqs)
{
	typedef MidiModel::NoteDiffCommand Command;

	Command* cmd = new Command(model, name());

	for (std::vector<Notes>::iterator s = seqs.begin(); s != seqs.end(); ++s) {
		for (Notes::const_iterator i = (*s).begin(); i != (*s).end(); ++i) {

			const NotePtr note = *i;

			model->transpose (cmd, *i, _semitones);
		}
	}

	return cmd;
}

}  // namespace ARDOUR
