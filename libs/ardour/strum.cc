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

#include "ardour/strum.h"
#include "ardour/midi_model.h"

namespace ARDOUR {

Strum::Strum(bool forward, bool fine)
	: _forward(forward)
	, _fine(fine)
{}

PBD::Command*
Strum::operator()(std::shared_ptr<ARDOUR::MidiModel> model,
                  Temporal::Beats                      position,
                  std::vector<Strum::Notes>&           seqs)
{
	typedef MidiModel::NoteDiffCommand Command;

	Command* cmd = new Command(model, name());

	if (seqs.empty()) {
		return cmd;
	}

	// Get all notes from all sequences and sort them by start time
	Notes all_notes;
	for (std::vector<Notes>::iterator s = seqs.begin(); s != seqs.end(); ++s) {
		all_notes.insert(all_notes.end(), (*s).begin(), (*s).end());
	}

	if (all_notes.size() < 2) {
		return cmd;
	}

	// Sort notes by start time
	std::sort(all_notes.begin(), all_notes.end(),
	          [](const NotePtr& a, const NotePtr& b) {
	              return a->time() < b->time();
	          });

	Temporal::Beats total_offset;
	Temporal::Beats offset;

	if (_fine) {
		offset = Temporal::Beats::ticks(Temporal::ticks_per_beat / 128);
	} else {
		offset = Temporal::Beats::ticks(Temporal::ticks_per_beat / 32);
	}

	if (_forward) {
		for (Notes::const_iterator i = all_notes.begin(); i != all_notes.end(); ++i) {
			const NotePtr note = *i;
			Temporal::Beats new_start = note->time() + total_offset;
			cmd->change(note, MidiModel::NoteDiffCommand::StartTime, new_start);
			total_offset += offset;
		}
	} else { // backward
		for (Notes::const_reverse_iterator i = all_notes.rbegin(); i != all_notes.rend(); ++i) {
			const NotePtr note = *i;
			Temporal::Beats new_start = note->time() + total_offset;
			cmd->change(note, MidiModel::NoteDiffCommand::StartTime, new_start);
			total_offset += offset;
		}
	}

	return cmd;
}

} /* namespace */
