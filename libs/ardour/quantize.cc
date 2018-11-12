/*
    Copyright (C) 2004 Paul Davis

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
#include <cmath>

#include "pbd/basename.h"

#include "ardour/quantize.h"
#include "ardour/midi_model.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

/** Quantize notes
 *
 * grid parameters are the quantize value in beats, ie 1.0 = quantize to beats,
 * 0.25 = quantize to beats/4, etc.
 */

Quantize::Quantize (bool snap_start, bool snap_end,
		    double start_grid, double end_grid,
		    float strength, float swing, float threshold)
	: _snap_start (snap_start)
	, _snap_end (snap_end)
	, _start_grid(start_grid)
	, _end_grid(end_grid)
	, _strength (strength/100.0)
	, _swing (swing/100.0)
	, _threshold (threshold)
{
}

Quantize::~Quantize ()
{
}

static double
swing_position (double pos, double grid, double swing, double offset)
{
	/* beats start out numbered at zero.
	 *
	 * every other position on the start-quantize-grid is
	 * optionally swung, meaning that its position is moved
	 * somewhere between its natural position and 2/3 of
	 * the way to the next start-quantize-grid position.
	 *
	 * so, if the _start grid is 0.5, the beat at 0 isn't
	 * swung, but something at 0.5 is, the beat at 1 isn't
	 * swung, but something at 1.5 is.
	 *
	 * if the start grid is 1.0, the beat at 0 isn't swung,
	 * but the beat at 1.0 is. the beat at 2.0 isn't swung,
	 * but the beat at 3.0 is. and so on.
	 *
	 * so the criterion for a position being swung is
	 * whether or not ((possible_grid_position / grid) % 2) != 0
	 */

	const bool swing_quantize_grid_position = pos > 0.0 && fmod ((pos/grid), 2.0) != 0;
	const bool swing_previous_grid_position = pos > grid && fmod ((pos-grid)/grid, 2.0) != 0;

	/* one of these will not be subject to swing */

	double swung_pos = pos;
	double swung_previous_grid_position;

	if (pos > grid) {
		swung_previous_grid_position = pos - grid;
	} else {
		swung_previous_grid_position = 0.0;
	}

	if (swing_previous_grid_position) {
		swung_previous_grid_position = swung_previous_grid_position + (2.0/3.0 * swing * grid);
	}

	if (swing_quantize_grid_position) {
		swung_pos = swung_pos + (2.0/3.0 * swing * grid);
	}

	/* now correct for start-of-model offset */

	pos += offset;

	if (fabs (pos - swung_pos) > fabs (pos - swung_previous_grid_position)) {
		pos = swung_previous_grid_position;
	} else {
		pos = swung_pos;
	}

	return pos;
}

Command*
Quantize::operator () (boost::shared_ptr<MidiModel> model,
                       Temporal::Beats position,
                       std::vector<Evoral::Sequence<Temporal::Beats>::Notes>& seqs)
{
	/* TODO: Rewrite this to be precise with fixed point? */

	/* Calculate offset from start of model to next closest quantize step,
	   to quantize relative to actual session beats (etc.) rather than from the
	   start of the model.
	*/
	const double round_pos = round(position.to_double() / _start_grid) * _start_grid;
	const double offset    = round_pos - position.to_double();

	MidiModel::NoteDiffCommand* cmd = new MidiModel::NoteDiffCommand (model, "quantize");

	for (std::vector<Evoral::Sequence<Temporal::Beats>::Notes>::iterator s = seqs.begin(); s != seqs.end(); ++s) {

		for (Evoral::Sequence<MidiModel::TimeType>::Notes::iterator i = (*s).begin(); i != (*s).end(); ++i) {

			/* compute new start + end points WITHOUT the offset
			 * caused by the start of the model (see above).
			 *
			 * these versions of new_start and new_end are
			 * guaranteed to precisely align with the quantize grid(s).
			 */

			double new_start = round (((*i)->time().to_double() - offset) / _start_grid) * _start_grid;
			double new_end = round (((*i)->end_time().to_double() - offset) / _end_grid) * _end_grid;

			if (_swing) {

				new_start = swing_position (new_start, _start_grid, _swing, offset);
				new_end = swing_position (new_end, _end_grid, _swing, offset);

			} else {

				/* now correct for start-of-model offset */

				new_start += offset;
				new_end += offset;
			}

			double delta = new_start - (*i)->time().to_double();


			if (fabs (delta) >= _threshold) {
				if (_snap_start) {
					delta *= _strength;
					cmd->change ((*i), MidiModel::NoteDiffCommand::StartTime,
					             (*i)->time() + delta);
				}
			}

			if (_snap_end) {
				delta = new_end - (*i)->end_time().to_double();

				if (fabs (delta) >= _threshold) {
					Temporal::Beats new_dur(new_end - new_start);

					if (!new_dur) {
						new_dur = Temporal::Beats(_end_grid);
					}

					cmd->change ((*i), MidiModel::NoteDiffCommand::Length, new_dur);
				}
			}
		}
	}

	return cmd;
}
