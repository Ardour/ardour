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

#include "ardour/types.h"
#include "ardour/quantize.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"
#include "ardour/midi_region.h"
#include "ardour/tempo.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

/** Quantize notes
 *
 * grid parameters are the quantize value in beats, ie 1.0 = quantize to beats,
 * 0.25 = quantize to beats/4, etc.
 */

Quantize::Quantize (Session& s, QuantizeType /* type */, 
		    bool snap_start, bool snap_end,
		    double start_grid, double end_grid, 
		    float strength, float swing, float threshold)
	: session (s)
	, _snap_start (snap_start)
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

int
Quantize::operator () (std::vector<Evoral::Sequence<Evoral::MusicalTime>::Notes>& seqs)
{
	bool even;

	for (std::vector<Evoral::Sequence<Evoral::MusicalTime>::Notes>::iterator s = seqs.begin();
	     s != seqs.end(); ++s) {

		even = false;

		for (Evoral::Sequence<MidiModel::TimeType>::Notes::iterator i = (*s).begin();
		     i != (*s).end(); ++i) {
			
			double new_start = round ((*i)->time() / _start_grid) * _start_grid; 
			double new_end = round ((*i)->end_time() / _end_grid) * _end_grid;
			double delta;
			
			if (_swing > 0.0 && !even) {
				
				double next_grid = new_start + _start_grid;
				
				/* find a spot 2/3 (* swing factor) of the way between the grid point
				   we would put this note at, and the nominal position of the next note.
				*/
				
				new_start = new_start + (2.0/3.0 * _swing * (next_grid - new_start));
				
			} else if (_swing < 0.0 && !even) {
				
				double prev_grid = new_start - _start_grid;
				
				/* find a spot 2/3 (* swing factor) of the way between the grid point
				   we would put this note at, and the nominal position of the previous note.
				*/
				
				new_start = new_start - (2.0/3.0 * _swing * (new_start - prev_grid));
				
			}
			
			delta = new_start - (*i)->time();
			
			if (fabs (delta) >= _threshold) {
				if (_snap_start) {
					delta *= _strength;
					(*i)->set_time ((*i)->time() + delta);
				}
			}
			
			if (_snap_end) {
				delta = new_end - (*i)->end_time();
				
				if (fabs (delta) >= _threshold) {
					double new_dur = new_end - new_start;
					
					if (new_dur == 0.0) {
						new_dur = _end_grid;
					}
					
					(*i)->set_length (new_dur);
				}
			}
			
			even = !even;
		}
	}

	return 0;
}
