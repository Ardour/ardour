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
	std::vector<NotePtr> all_notes;
	for (std::vector<Notes>::iterator s = seqs.begin(); s != seqs.end(); ++s) {
		for (Notes::const_iterator i = (*s).begin(); i != (*s).end(); ++i) {
			all_notes.push_back(*i);
		}
	}

	if (all_notes.size() < 2) {
		return cmd;
	}

	bool forward = _forward;

	// Sort notes
	std::sort(all_notes.begin(), all_notes.end(), [forward](const NotePtr& a, const NotePtr& b) {
		if (a->time() == b->time()) {
			if (forward) {
				return a->note() < b->note();
			} else {
				return a->note() > b->note();
			}
		} else {
			return a->time() < b->time();
		}
	});

	Temporal::Beats total_offset;
	Temporal::Beats offset;

	if (_fine) {
		offset = Temporal::Beats::ticks(Temporal::ticks_per_beat / 128);
	} else {
		offset = Temporal::Beats::ticks(Temporal::ticks_per_beat / 32);
	}

	Temporal::Beats prev_time = all_notes.at(0)->time();

	for (std::vector<NotePtr>::const_iterator i = all_notes.begin(); i != all_notes.end(); ++i) {
		const NotePtr note = *i;
		std::cout << (*i)->note() << std::endl;
		if ((*i)->time() != prev_time) {
			total_offset = 0;
		}

		Temporal::Beats new_start;
		Temporal::Beats new_length = note->length() - total_offset;
		if (new_length <= Temporal::Beats::ticks(0)) {
			new_start = note->end_time() - Temporal::Beats::ticks(1);
			new_length = Temporal::Beats::ticks(1);
		} else {
			new_start = note->time() + total_offset;
		}

		cmd->change(note, MidiModel::NoteDiffCommand::StartTime, new_start);
		cmd->change(note, MidiModel::NoteDiffCommand::Length, new_length);
		total_offset += offset;
		prev_time = (*i)->time();
	}

	return cmd;
}

} /* namespace */
